#pragma once
#include <Arduino.h>
#include "Config.h"
#include "CurrentSensor.h"
#include "PowerProtection.h"

// -----------------------------------------------------------------------------
// PowerProtection - Current protection with fault limiting
// -----------------------------------------------------------------------------
// CRITICAL DESIGN REQUIREMENT:
//   Cannot fully shut down pump - would damage engine under load!
//   Protection strategy: Voltage reduction to limit power on fault
//
// Protection Zones:
//   NORMAL:    0-40A  - No action, full voltage allowed
//   FAULT:     >40A   - Reduce to minimum safe voltage (50%), log fault
//   EMERGENCY: >45A   - Immediate shutdown if enabled (short circuit)
//
// Features:
//   - Hysteresis: 2.5A band to prevent oscillation/chattering
//   - Dual channel monitoring (triggers on EITHER channel exceeding limit)
//   - Rate-limited voltage changes for gradual response
//   - Event logging to Serial
//   - Never fully disables output (unless EMERGENCY shutdown enabled)
// -----------------------------------------------------------------------------

class PowerProtection 
{
public:
    // Protection level enumeration
    enum class ProtectionLevel : uint8_t {
        NORMAL = 0,    // No protection active
        FAULT,         // Fault condition - reduce to minimum safe voltage
        EMERGENCY      // EMERGENCY - Short circuit / sensor saturation (immediate shutdown if enabled)
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
    // Using uint32_t to prevent overflow (max ~4.3 billion events)
    uint32_t getFaultCount() const {
        return _faultCount;
    }

    // Convert protection level to string (for logging)
    const char* getLevelString() const {
        return getLevelString(_currentLevel);
    }

    static const char* getLevelString(ProtectionLevel level) {
        switch (level) {
            case ProtectionLevel::NORMAL:    return "NORMAL";
            case ProtectionLevel::FAULT:     return "FAULT";
            case ProtectionLevel::EMERGENCY: return "*** EMERGENCY ***";
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
    uint32_t _faultCount;         // Cumulative fault events (uint32_t prevents overflow)

    // Calculate protection level with hysteresis
    ProtectionLevel calculateProtectionLevel(float current) {
        // EMERGENCY CHECK FIRST - Immediate response to dangerous current levels
        // Sensor saturation (~50A) indicates short circuit or severe overload
        // Check regardless of current level to enable immediate shutdown
        if (current >= Config::CURRENT_THRESHOLD_EMERGENCY) {
            return ProtectionLevel::EMERGENCY;
        }

        // Hysteresis logic: different thresholds for rising vs falling
        // This prevents rapid oscillation between levels
        switch (_currentLevel) {
            case ProtectionLevel::NORMAL:
                if (current >= Config::CURRENT_THRESHOLD_FAULT) {
                    return ProtectionLevel::FAULT;
                }
                return ProtectionLevel::NORMAL;

            case ProtectionLevel::FAULT:
                if (current < Config::CURRENT_THRESHOLD_FAULT - Config::CURRENT_HYSTERESIS) {
                    return ProtectionLevel::NORMAL;
                }
                return ProtectionLevel::FAULT;

            case ProtectionLevel::EMERGENCY:
                // Emergency requires current to drop below FAULT threshold to recover
                if (current < Config::CURRENT_THRESHOLD_FAULT - Config::CURRENT_HYSTERESIS) {
                    return ProtectionLevel::NORMAL;
                }
                return ProtectionLevel::EMERGENCY;

            default:
                return ProtectionLevel::NORMAL;
        }
    }

    // Get voltage limit factor for a given protection level
    float getVoltageLimitForLevel(ProtectionLevel level) const {
        switch (level) {
            case ProtectionLevel::NORMAL:
                return Config::PROTECTION_PERCENT_NORMAL;    // 100% - no limiting

            case ProtectionLevel::FAULT:
                return Config::PROTECTION_PERCENT_FAULT;     // 50% - minimum safe level

            case ProtectionLevel::EMERGENCY:
                // Emergency shutdown: 0% if enabled, otherwise 50% (fail-safe)
                return Config::ENABLE_EMERGENCY_SHUTDOWN ?
                       Config::PROTECTION_PERCENT_EMERGENCY :  // 0% - complete shutdown
                       Config::PROTECTION_PERCENT_FAULT;       // 50% - minimum power

            default:
                return 1.0f;
        }
    }

    // Handle protection level changes (logging and fault counting)
    void handleLevelChange(ProtectionLevel newLevel, float current) {
        // Safe millis() rollover: subtraction is always valid for unsigned types
        unsigned long timeSinceLast = (unsigned long)(millis() - _lastLevelChangeMs);
        
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
        
        // EMERGENCY level - critical alert
        if (newLevel == ProtectionLevel::EMERGENCY) {
            Serial.println(F(""));
            Serial.println(F("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!"));
            Serial.println(F("!!!   EMERGENCY SHUTDOWN TRIGGERED    !!!"));
            Serial.println(F("!!!   SHORT CIRCUIT OR OVERLOAD       !!!"));
            Serial.println(F("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!"));
            Serial.print(F("Current: "));
            Serial.print(current, 2);
            Serial.print(F("A (Threshold: "));
            Serial.print(Config::CURRENT_THRESHOLD_EMERGENCY, 1);
            Serial.println(F("A)"));
            Serial.print(F("Sensor near saturation limit ("));
            Serial.print(Config::ACS772_MAX_CURRENT, 0);
            Serial.println(F("A)"));
            
            if (Config::ENABLE_EMERGENCY_SHUTDOWN) {
                Serial.println(F("ACTION: Complete shutdown (0% power)"));
            } else {
                Serial.println(F("ACTION: Minimum power (50%) - SHUTDOWN DISABLED"));
                Serial.println(F("WARNING: Hardware may be at risk!"));
            }
            Serial.println(F("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!"));
            Serial.println(F(""));
        }
        
        // Increment fault counter if entering FAULT or EMERGENCY level
        if (newLevel == ProtectionLevel::FAULT || newLevel == ProtectionLevel::EMERGENCY) {
            _faultCount++;
            Serial.print(F("[PROTECTION] *** FAULT EVENT *** Count: "));
            Serial.println(_faultCount);
            
            // TODO: Could trigger external alarm, LED indicator, CAN message, etc.
        }
        
        // Log recovery from FAULT or EMERGENCY
        if ((_currentLevel == ProtectionLevel::FAULT || _currentLevel == ProtectionLevel::EMERGENCY) && 
            (newLevel != ProtectionLevel::FAULT && newLevel != ProtectionLevel::EMERGENCY)) {
            Serial.println(F("[PROTECTION] Recovered from FAULT/EMERGENCY"));
        }
    }

    // Apply rate limiting to voltage limit changes
    void applyRateLimiting(float targetLimit) {
        // Rate limiting prevents sudden voltage jumps
        // Gradual changes reduce electrical/mechanical stress
        
        // EMERGENCY OVERRIDE: Skip rate limiting for immediate shutdown
        if (_currentLevel == ProtectionLevel::EMERGENCY) {
            _voltageLimit = targetLimit;  // Apply immediately
            return;
        }
        
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
        float minLimit = Config::ENABLE_EMERGENCY_SHUTDOWN ? 
                        0.0f : Config::PROTECTION_PERCENT_FAULT;
        if (_voltageLimit < minLimit) {
            _voltageLimit = minLimit;
        }
        if (_voltageLimit > 1.0f) {
            _voltageLimit = 1.0f;
        }
    }
};
