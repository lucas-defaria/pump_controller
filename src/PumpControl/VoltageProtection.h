#pragma once
#include <Arduino.h>
#include "Config.h"
#include "VoltageSensor.h"

// -----------------------------------------------------------------------------
// VoltageProtection - Simplified voltage sensor fault detection
// -----------------------------------------------------------------------------
// DESIGN PHILOSOPHY:
//   With percentage-based control, the system automatically adapts to voltage
//   variations (8-14.5V automotive range). No need for voltage drop thresholds.
//   This class now only detects sensor faults (invalid readings).
//
// Protection Strategy:
//   - NORMAL: Sensor reading is within valid automotive range (7-16V)
//   - FAULT:  Sensor reading outside valid range or sensor malfunction
//
// Features:
//   - Simple binary state (NORMAL or FAULT)
//   - Event logging to Serial
//   - Fault counting for diagnostics
// -----------------------------------------------------------------------------

class VoltageProtection 
{
public:
    // Protection level enumeration (simplified to 2 states)
    enum class ProtectionLevel : uint8_t {
        NORMAL = 0,    // Voltage sensor OK
        FAULT          // Sensor fault or invalid reading
    };

    explicit VoltageProtection(VoltageSensor& sensor)
        : _sensor(sensor)
        , _currentLevel(ProtectionLevel::NORMAL)
        , _lastLevelChangeMs(0)
        , _faultCount(0)
    {}

    void begin() {
        _currentLevel = ProtectionLevel::NORMAL;
        _lastLevelChangeMs = millis();
        _faultCount = 0;
        
        Serial.println(F("[VOLTAGE_PROTECTION] System initialized (fault detection only)"));
    }

    // Update protection state based on voltage sensor validity
    // Returns the current protection level
    ProtectionLevel update() {
        // Check sensor validity (within automotive range 7-16V)
        bool sensorValid = _sensor.isValid();
        
        // Determine protection level
        ProtectionLevel newLevel = sensorValid ? ProtectionLevel::NORMAL : ProtectionLevel::FAULT;
        
        // Check if level changed
        if (newLevel != _currentLevel) {
            handleLevelChange(newLevel);
            _currentLevel = newLevel;
            _lastLevelChangeMs = millis();
        }
        
        return _currentLevel;
    }

    // Get current protection level
    ProtectionLevel getLevel() const {
        return _currentLevel;
    }

    // Check if voltage sensor is working properly
    bool isSensorOk() const {
        return _currentLevel == ProtectionLevel::NORMAL;
    }

    // Get total fault count
    // Using uint32_t to prevent overflow (max ~4.3 billion events)
    uint32_t getFaultCount() const {
        return _faultCount;
    }

    // Get time since last level change in milliseconds (compensated for Timer 0)
    // COMPENSATED: millis() runs 64x faster, divide by prescaler factor for real time
    unsigned long getTimeSinceLastChange() const {
        return (unsigned long)(millis() - _lastLevelChangeMs) / Config::TIMER0_PRESCALER_FACTOR;
    }

    // Convert protection level to string (for logging)
    const char* getLevelString() const {
        return getLevelString(_currentLevel);
    }

    static const char* getLevelString(ProtectionLevel level) {
        switch (level) {
            case ProtectionLevel::NORMAL:   return "NORMAL";
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
    unsigned long _lastLevelChangeMs;
    uint32_t _faultCount;         // uint32_t prevents overflow

    // Handle protection level changes (logging and fault counting)
    void handleLevelChange(ProtectionLevel newLevel) {
        // Safe millis() rollover: subtraction is always valid for unsigned types
        // COMPENSATED: millis() runs 64x faster due to Timer 0 prescaler change
        // Divide by TIMER0_PRESCALER_FACTOR to get actual elapsed time
        unsigned long timeSinceLast = (unsigned long)(millis() - _lastLevelChangeMs) / Config::TIMER0_PRESCALER_FACTOR;
        float voltage = _sensor.readVoltage();
        
        // Log level change
        Serial.print(F("[VOLTAGE_PROTECTION] Sensor status: "));
        Serial.print(getLevelString(_currentLevel));
        Serial.print(F(" -> "));
        Serial.print(getLevelString(newLevel));
        Serial.print(F(" | Voltage: "));
        Serial.print(voltage, 2);
        Serial.print(F("V | Time: "));
        Serial.print(timeSinceLast);
        Serial.println(F("ms"));
        
        // Increment fault counter if entering FAULT
        if (newLevel == ProtectionLevel::FAULT) {
            _faultCount++;
            Serial.print(F("[VOLTAGE_PROTECTION] *** SENSOR FAULT *** Count: "));
            Serial.println(_faultCount);
            Serial.print(F("[VOLTAGE_PROTECTION] Valid range: "));
            Serial.print(Config::VOLTAGE_MINIMUM_VALID, 1);
            Serial.print(F("-"));
            Serial.print(Config::VOLTAGE_MAXIMUM_VALID, 1);
            Serial.println(F("V"));
            
            // TODO: Could trigger:
            // - Use fallback voltage value (e.g., 12V nominal)
            // - External alarm/indicator LED
            // - CAN bus error message
            // - Safe mode operation
        }
        
        // Log recovery from FAULT
        if (_currentLevel == ProtectionLevel::FAULT && 
            newLevel == ProtectionLevel::NORMAL) {
            Serial.println(F("[VOLTAGE_PROTECTION] Sensor recovered from FAULT"));
        }
    }
};
