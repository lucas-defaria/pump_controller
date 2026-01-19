#pragma once
#include <Arduino.h>
#include "Config.h"
#include "VoltageSensor.h"

// -----------------------------------------------------------------------------
// VoltageProtection - Percentage-based low voltage protection
// -----------------------------------------------------------------------------
// DESIGN PHILOSOPHY:
//   Adaptive protection based on measured supply voltage, not fixed thresholds
//   Automotive voltage varies 8-14.5V, so we use percentage drop instead
//
// Protection Strategy:
//   - Measures baseline supply voltage (running average)
//   - Calculates dynamic thresholds: baseline × (1 - drop_percentage)
//   - WARNING:  15% drop from baseline (e.g., 12V ? 10.2V)
//   - CRITICAL: 30% drop from baseline (e.g., 12V ? 8.4V)
//
// This adapts automatically:
//   - At 14V supply: WARNING=11.9V, CRITICAL=9.8V
//   - At 12V supply: WARNING=10.2V, CRITICAL=8.4V
//   - At 10V supply: WARNING=8.5V,  CRITICAL=7.0V
//
// Features:
//   - Hysteresis to prevent oscillation
//   - Event logging to Serial
//   - Fault counting for diagnostics
// -----------------------------------------------------------------------------

class VoltageProtection 
{
public:
    // Protection level enumeration
    enum class ProtectionLevel : uint8_t {
        NORMAL = 0,    // Voltage OK
        WARNING,       // Voltage dropping (15% below baseline)
        CRITICAL,      // Critical low voltage (30% below baseline)
        FAULT          // Sensor fault or extreme condition
    };

    explicit VoltageProtection(VoltageSensor& sensor)
        : _sensor(sensor)
        , _currentLevel(ProtectionLevel::NORMAL)
        , _baselineVoltage(12.0f)  // Initialize to nominal 12V
        , _warningThreshold(10.2f)
        , _criticalThreshold(8.4f)
        , _lastLevelChangeMs(0)
        , _faultCount(0)
        , _baselineInitialized(false)
    {}

    void begin() {
        _currentLevel = ProtectionLevel::NORMAL;
        _lastLevelChangeMs = millis();
        _faultCount = 0;
        _baselineInitialized = false;
        
        Serial.println(F("[VOLTAGE_PROTECTION] System initialized"));
    }

    // Update protection state based on voltage reading
    // Returns the current protection level
    ProtectionLevel update() {
        // Read current supply voltage
        float voltage = _sensor.readVoltage();
        
        // Check for sensor validity
        if (!_sensor.isValid()) {
            if (_currentLevel != ProtectionLevel::FAULT) {
                handleLevelChange(ProtectionLevel::FAULT, voltage);
                _currentLevel = ProtectionLevel::FAULT;
            }
            return _currentLevel;
        }
        
        // Initialize or update baseline voltage (slow adaptation)
        updateBaseline(voltage);
        
        // Calculate dynamic thresholds based on baseline
        _warningThreshold = _baselineVoltage * (1.0f - Config::VOLTAGE_DROP_WARNING_PERCENT);
        _criticalThreshold = _baselineVoltage * (1.0f - Config::VOLTAGE_DROP_CRITICAL_PERCENT);
        
        // Determine new protection level with hysteresis
        ProtectionLevel newLevel = calculateProtectionLevel(voltage);
        
        // Check if level changed
        if (newLevel != _currentLevel) {
            handleLevelChange(newLevel, voltage);
            _currentLevel = newLevel;
            _lastLevelChangeMs = millis();
        }
        
        return _currentLevel;
    }

    // Get current protection level
    ProtectionLevel getLevel() const {
        return _currentLevel;
    }

    // Get baseline voltage (reference for percentage calculation)
    float getBaselineVoltage() const {
        return _baselineVoltage;
    }

    // Get calculated warning threshold
    float getWarningThreshold() const {
        return _warningThreshold;
    }

    // Get calculated critical threshold
    float getCriticalThreshold() const {
        return _criticalThreshold;
    }

    // Get total fault count
    uint16_t getFaultCount() const {
        return _faultCount;
    }

    // Convert protection level to string (for logging)
    const char* getLevelString() const {
        return getLevelString(_currentLevel);
    }

    static const char* getLevelString(ProtectionLevel level) {
        switch (level) {
            case ProtectionLevel::NORMAL:   return "NORMAL";
            case ProtectionLevel::WARNING:  return "WARNING";
            case ProtectionLevel::CRITICAL: return "CRITICAL";
            case ProtectionLevel::FAULT:    return "FAULT";
            default: return "UNKNOWN";
        }
    }

    // Reset fault counter (for maintenance/diagnostics)
    void resetFaultCount() {
        _faultCount = 0;
        Serial.println(F("[VOLTAGE_PROTECTION] Fault count reset"));
    }

private:
    VoltageSensor& _sensor;
    ProtectionLevel _currentLevel;
    float _baselineVoltage;       // Reference voltage for percentage calculation
    float _warningThreshold;      // Calculated warning threshold (baseline - 15%)
    float _criticalThreshold;     // Calculated critical threshold (baseline - 30%)
    unsigned long _lastLevelChangeMs;
    uint16_t _faultCount;
    bool _baselineInitialized;

    // Update baseline voltage with slow adaptation
    void updateBaseline(float voltage) {
        if (!_baselineInitialized) {
            // First reading: initialize baseline directly
            _baselineVoltage = voltage;
            _baselineInitialized = true;
        } else {
            // Slow adaptation: only track upward changes (voltage recovery)
            // This prevents the baseline from chasing downward during voltage drops
            // Alpha = 0.01 means very slow tracking (1% per update, ~5s @ 20Hz)
            const float BASELINE_ALPHA = 0.01f;
            
            // Only update baseline if voltage is higher (recovery scenario)
            if (voltage > _baselineVoltage) {
                _baselineVoltage = (BASELINE_ALPHA * voltage) + 
                                  ((1.0f - BASELINE_ALPHA) * _baselineVoltage);
            }
        }
        
        // Clamp baseline to reasonable automotive range
        if (_baselineVoltage < 10.0f) _baselineVoltage = 10.0f;
        if (_baselineVoltage > 15.0f) _baselineVoltage = 15.0f;
    }

    // Calculate protection level with hysteresis
    ProtectionLevel calculateProtectionLevel(float voltage) {
        // Hysteresis prevents rapid oscillation between levels
        // Use voltage hysteresis band from Config.h
        
        switch (_currentLevel) {
            case ProtectionLevel::NORMAL:
                if (voltage < _warningThreshold) {
                    return ProtectionLevel::WARNING;
                }
                return ProtectionLevel::NORMAL;
                
            case ProtectionLevel::WARNING:
                if (voltage < _criticalThreshold) {
                    return ProtectionLevel::CRITICAL;
                }
                // Recover to NORMAL with hysteresis
                if (voltage > _warningThreshold + Config::VOLTAGE_HYSTERESIS) {
                    return ProtectionLevel::NORMAL;
                }
                return ProtectionLevel::WARNING;
                
            case ProtectionLevel::CRITICAL:
                // Recover to WARNING with hysteresis
                if (voltage > _criticalThreshold + Config::VOLTAGE_HYSTERESIS) {
                    return ProtectionLevel::WARNING;
                }
                return ProtectionLevel::CRITICAL;
                
            case ProtectionLevel::FAULT:
                // Recover from fault if sensor becomes valid again
                if (_sensor.isValid() && voltage > Config::VOLTAGE_MINIMUM_VALID) {
                    // Re-enter at appropriate level based on voltage
                    if (voltage < _criticalThreshold) {
                        return ProtectionLevel::CRITICAL;
                    } else if (voltage < _warningThreshold) {
                        return ProtectionLevel::WARNING;
                    } else {
                        return ProtectionLevel::NORMAL;
                    }
                }
                return ProtectionLevel::FAULT;
                
            default:
                return ProtectionLevel::NORMAL;
        }
    }

    // Handle protection level changes (logging and fault counting)
    void handleLevelChange(ProtectionLevel newLevel, float voltage) {
        unsigned long timeSinceLast = millis() - _lastLevelChangeMs;
        
        // Log level change
        Serial.print(F("[VOLTAGE_PROTECTION] Level change: "));
        Serial.print(getLevelString(_currentLevel));
        Serial.print(F(" -> "));
        Serial.print(getLevelString(newLevel));
        Serial.print(F(" | Voltage: "));
        Serial.print(voltage, 2);
        Serial.print(F("V | Baseline: "));
        Serial.print(_baselineVoltage, 2);
        Serial.print(F("V | Time: "));
        Serial.print(timeSinceLast);
        Serial.println(F("ms"));
        
        // Log thresholds on level change
        Serial.print(F("[VOLTAGE_PROTECTION] Thresholds: WARNING="));
        Serial.print(_warningThreshold, 2);
        Serial.print(F("V (-"));
        Serial.print(Config::VOLTAGE_DROP_WARNING_PERCENT * 100.0f, 0);
        Serial.print(F("%), CRITICAL="));
        Serial.print(_criticalThreshold, 2);
        Serial.print(F("V (-"));
        Serial.print(Config::VOLTAGE_DROP_CRITICAL_PERCENT * 100.0f, 0);
        Serial.println(F("%)"));
        
        // Increment fault counter if entering FAULT or CRITICAL level
        if (newLevel == ProtectionLevel::FAULT || newLevel == ProtectionLevel::CRITICAL) {
            _faultCount++;
            Serial.print(F("[VOLTAGE_PROTECTION] *** LOW VOLTAGE EVENT *** Count: "));
            Serial.println(_faultCount);
            
            // TODO: Could trigger:
            // - Reduce pump power further (coordinate with PowerProtection)
            // - External alarm/indicator LED
            // - CAN bus warning message
            // - Emergency shutdown if voltage too low for too long
        }
        
        // Log recovery
        if (_currentLevel == ProtectionLevel::CRITICAL && 
            newLevel == ProtectionLevel::WARNING) {
            Serial.println(F("[VOLTAGE_PROTECTION] Recovered from CRITICAL"));
        }
        if (_currentLevel == ProtectionLevel::FAULT && 
            newLevel != ProtectionLevel::FAULT) {
            Serial.println(F("[VOLTAGE_PROTECTION] Recovered from FAULT"));
        }
    }
};
