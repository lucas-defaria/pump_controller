#pragma once
#include <Arduino.h>
#include "Config.h"
#include "CurrentSensor.h"

// -----------------------------------------------------------------------------
// PowerProtection - Multi-level current protection with progressive limiting
// -----------------------------------------------------------------------------
// CRITICAL DESIGN REQUIREMENT:
//   Cannot fully shut down pump - would damage engine under load!
//   Protection strategy: Progressive voltage reduction to limit power
//
// Protection Zones:
//   NORMAL:   0-25A   - No action, full voltage allowed
//   WARNING:  25-30A  - Reduce voltage by 10% (warn level)
//   HIGH:     30-35A  - Reduce voltage by 25% (high current)
//   CRITICAL: 35-40A  - Reduce voltage by 40% (approaching max)
//   FAULT:    >40A    - Reduce to minimum safe voltage (50% or 6V), log fault
//
// Features:
//   - Hysteresis: 2A bands to prevent oscillation/chattering
//   - Dual channel monitoring (triggers on EITHER channel exceeding limit)
//   - Rate-limited voltage changes for gradual response
//   - Event logging to Serial
//   - Never fully disables output
// -----------------------------------------------------------------------------

class PowerProtection {
public:
    // Protection level enumeration
    enum class ProtectionLevel : uint8_t {
        NORMAL = 0,    // No protection active
        WARNING,       // Early warning zone
        HIGH,          // High current detected
        CRITICAL,      // Critical current level
        FAULT          // Fault condition - sensor limit exceeded
    };

    PowerProtection(CurrentSensor& sensor1, CurrentSensor& sensor2)
        : _sensor1(sensor1)
        , _sensor2(sensor2)
        , _currentLevel(ProtectionLevel::NORMAL)
        , _voltageLimit(1.0f)
        , _lastLevelChangeMs(0)
        , _faultCount(0)
    {}

    void begin() {
        _currentLevel = ProtectionLevel::NORMAL;
        _voltageLimit = 1.0f;  // Start at 100% (no limiting)
        _lastLevelChangeMs = millis();
        _faultCount = 0;
        
        Serial.println(F("[PROTECTION] System initialized"));
    }

    // Update protection state based on current readings
    // Returns the voltage limit factor (0.0 to 1.0)
    float update() {
        // Read both current sensors
        float current1 = _sensor1.readCurrentA();
        float current2 = _sensor2.readCurrentA();
        
        // Use the maximum of the two channels for protection decision
        float maxCurrent = max(current1, current2);
        
        // Determine new protection level based on thresholds and hysteresis
        ProtectionLevel newLevel = calculateProtectionLevel(maxCurrent);
        
        // Check if level changed
        if (newLevel != _currentLevel) {
            handleLevelChange(newLevel, maxCurrent);
            _currentLevel = newLevel;
            _lastLevelChangeMs = millis();
        }
        
        // Calculate target voltage limit based on current protection level
        float targetLimit = getVoltageLimitForLevel(_currentLevel);
        
        // Apply rate limiting to voltage changes (gradual transition)
        applyRateLimiting(targetLimit);
        
        return _voltageLimit;
    }

    // Get current protection level
    ProtectionLevel getLevel() const {
        return _currentLevel;
    }

    // Get current voltage limit factor (0.0 to 1.0)
    float getVoltageLimit() const {
        return _voltageLimit;
    }

    // Get total fault count (persistent counter)
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
            case ProtectionLevel::HIGH:     return "HIGH";
            case ProtectionLevel::CRITICAL: return "CRITICAL";
            case ProtectionLevel::FAULT:    return "FAULT";
            default: return "UNKNOWN";
        }
    }

    // Reset fault counter (for maintenance/diagnostics)
    void resetFaultCount() {
        _faultCount = 0;
        Serial.println(F("[PROTECTION] Fault count reset"));
    }

private:
    CurrentSensor& _sensor1;
    CurrentSensor& _sensor2;
    ProtectionLevel _currentLevel;
    float _voltageLimit;          // Current voltage limit factor (0.0-1.0)
    unsigned long _lastLevelChangeMs;
    uint16_t _faultCount;         // Cumulative fault events

    // Calculate protection level with hysteresis
    ProtectionLevel calculateProtectionLevel(float current) {
        // Hysteresis logic: different thresholds for rising vs falling
        // This prevents rapid oscillation between levels
        
        switch (_currentLevel) {
            case ProtectionLevel::NORMAL:
                if (current >= Config::CURRENT_THRESHOLD_WARNING) {
                    return ProtectionLevel::WARNING;
                }
                return ProtectionLevel::NORMAL;
                
            case ProtectionLevel::WARNING:
                if (current >= Config::CURRENT_THRESHOLD_HIGH) {
                    return ProtectionLevel::HIGH;
                }
                if (current < Config::CURRENT_THRESHOLD_WARNING - Config::CURRENT_HYSTERESIS) {
                    return ProtectionLevel::NORMAL;
                }
                return ProtectionLevel::WARNING;
                
            case ProtectionLevel::HIGH:
                if (current >= Config::CURRENT_THRESHOLD_CRITICAL) {
                    return ProtectionLevel::CRITICAL;
                }
                if (current < Config::CURRENT_THRESHOLD_HIGH - Config::CURRENT_HYSTERESIS) {
                    return ProtectionLevel::WARNING;
                }
                return ProtectionLevel::HIGH;
                
            case ProtectionLevel::CRITICAL:
                if (current >= Config::CURRENT_THRESHOLD_FAULT) {
                    return ProtectionLevel::FAULT;
                }
                if (current < Config::CURRENT_THRESHOLD_CRITICAL - Config::CURRENT_HYSTERESIS) {
                    return ProtectionLevel::HIGH;
                }
                return ProtectionLevel::CRITICAL;
                
            case ProtectionLevel::FAULT:
                // Once in fault, need to drop below critical to recover
                if (current < Config::CURRENT_THRESHOLD_CRITICAL - Config::CURRENT_HYSTERESIS) {
                    return ProtectionLevel::HIGH;
                }
                return ProtectionLevel::FAULT;
                
            default:
                return ProtectionLevel::NORMAL;
        }
    }

    // Get voltage limit factor for a given protection level
    float getVoltageLimitForLevel(ProtectionLevel level) const {
        switch (level) {
            case ProtectionLevel::NORMAL:
                return 1.00f;  // 100% - no limiting
                
            case ProtectionLevel::WARNING:
                return 0.90f;  // 90% - 10% reduction
                
            case ProtectionLevel::HIGH:
                return 0.75f;  // 75% - 25% reduction
                
            case ProtectionLevel::CRITICAL:
                return 0.60f;  // 60% - 40% reduction
                
            case ProtectionLevel::FAULT:
                // Minimum safe voltage: 50% or 6V, whichever is higher
                // At 12V system: max(0.5, 6/12) = 0.5 = 6V
                return Config::FAULT_MINIMUM_VOLTAGE_FACTOR;
                
            default:
                return 1.0f;
        }
    }

    // Handle protection level changes (logging and fault counting)
    void handleLevelChange(ProtectionLevel newLevel, float current) {
        unsigned long timeSinceLast = millis() - _lastLevelChangeMs;
        
        // Log level change
        Serial.print(F("[PROTECTION] Level change: "));
        Serial.print(getLevelString(_currentLevel));
        Serial.print(F(" -> "));
        Serial.print(getLevelString(newLevel));
        Serial.print(F(" | Current: "));
        Serial.print(current, 2);
        Serial.print(F("A | Time since last: "));
        Serial.print(timeSinceLast);
        Serial.println(F("ms"));
        
        // Increment fault counter if entering FAULT level
        if (newLevel == ProtectionLevel::FAULT) {
            _faultCount++;
            Serial.print(F("[PROTECTION] *** FAULT EVENT *** Count: "));
            Serial.println(_faultCount);
            
            // TODO: Could trigger external alarm, LED indicator, CAN message, etc.
        }
        
        // Log recovery from FAULT
        if (_currentLevel == ProtectionLevel::FAULT && 
            newLevel != ProtectionLevel::FAULT) {
            Serial.println(F("[PROTECTION] Recovered from FAULT"));
        }
    }

    // Apply rate limiting to voltage limit changes
    void applyRateLimiting(float targetLimit) {
        // Rate limiting prevents sudden voltage jumps
        // Gradual changes reduce electrical/mechanical stress
        
        float delta = targetLimit - _voltageLimit;
        
        if (abs(delta) < 0.001f) {
            // Close enough, no change needed
            _voltageLimit = targetLimit;
            return;
        }
        
        // Maximum change per update cycle
        float maxChange = Config::VOLTAGE_LIMIT_RATE_MAX;
        
        if (delta > maxChange) {
            _voltageLimit += maxChange;
        } else if (delta < -maxChange) {
            _voltageLimit -= maxChange;
        } else {
            _voltageLimit = targetLimit;
        }
        
        // Ensure limits stay in valid range
        if (_voltageLimit < Config::FAULT_MINIMUM_VOLTAGE_FACTOR) {
            _voltageLimit = Config::FAULT_MINIMUM_VOLTAGE_FACTOR;
        }
        if (_voltageLimit > 1.0f) {
            _voltageLimit = 1.0f;
        }
    }
};
