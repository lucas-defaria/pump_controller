#pragma once
#include <Arduino.h>
#include "Config.h"

// -----------------------------------------------------------------------------
// PwmInput - Reads external PWM signal for slave mode operation
// -----------------------------------------------------------------------------
// Simplified approach using pulseIn() for reliable low-frequency PWM reading
// Optimized for 25Hz PWM with variable duty cycle (0-100%)
// -----------------------------------------------------------------------------

class PwmInput {
public:
    PwmInput(uint8_t pin) 
        : _pin(pin)
        , _dutyCycle(0.0f)
        , _frequency(0.0f)
        , _signalValid(false)
        , _lastValidSignalMs(0)
        , _pulsesDetected(0)
        , _debugEnabled(false)
    {}

    void begin() {
        pinMode(_pin, INPUT);  // No pullup - external signal
        _lastValidSignalMs = millis();
    }

    // Enable/disable debug output
    void setDebug(bool enable) {
        _debugEnabled = enable;
    }

    // Update PWM reading - measures one complete cycle
    // IMPORTANT: This blocks for up to ~100ms to measure the pulse
    // Call this in the main loop but not every cycle - once per control loop is enough
    void update() {
        unsigned long nowMs = millis();
        
        // Measure HIGH pulse duration (timeout 100ms = minimum 10Hz)
        unsigned long highUs = pulseIn(_pin, HIGH, 100000UL);
        
        if (highUs > 0) {
            // We got a valid HIGH pulse, now measure LOW pulse
            unsigned long lowUs = pulseIn(_pin, LOW, 100000UL);
            
            if (lowUs > 0) {
                // Both HIGH and LOW measured - we have a complete cycle!
                unsigned long periodUs = highUs + lowUs;
                
                // Calculate frequency
                _frequency = 1000000.0f / (float)periodUs;
                
                // Calculate duty cycle
                _dutyCycle = (float)highUs / (float)periodUs;
                
                // Clamp duty cycle
                if (_dutyCycle < 0.0f) _dutyCycle = 0.0f;
                if (_dutyCycle > 1.0f) _dutyCycle = 1.0f;
                
                // Validate frequency range (15-35Hz for 25Hz target)
                if (_frequency >= Config::PWM_INPUT_FREQ_MIN && 
                    _frequency <= Config::PWM_INPUT_FREQ_MAX) {
                    _signalValid = true;
                    _lastValidSignalMs = nowMs;
                    _pulsesDetected++;
                    
                    if (_debugEnabled) {
                        Serial.print(F("[PWM] VALID - Freq: "));
                        Serial.print(_frequency, 2);
                        Serial.print(F("Hz, Duty: "));
                        Serial.print(_dutyCycle * 100.0f, 1);
                        Serial.print(F("%, Period: "));
                        Serial.print(periodUs / 1000.0f, 2);
                        Serial.print(F("ms (H:"));
                        Serial.print(highUs / 1000.0f, 2);
                        Serial.print(F("ms, L:"));
                        Serial.print(lowUs / 1000.0f, 2);
                        Serial.println(F("ms)"));
                    }
                } else {
                    if (_debugEnabled) {
                        Serial.print(F("[PWM] Freq out of range: "));
                        Serial.print(_frequency, 2);
                        Serial.println(F("Hz"));
                    }
                }
            } else {
                // Got HIGH but timeout on LOW
                if (_debugEnabled) {
                    Serial.println(F("[PWM] Timeout waiting for LOW pulse"));
                }
            }
        } else {
            // Timeout on HIGH pulse - no signal or signal is LOW
            if (_debugEnabled && (nowMs % 5000) < 100) {  // Print every 5 seconds
                Serial.println(F("[PWM] No HIGH pulse detected (signal may be stuck LOW or absent)"));
            }
        }
        
        // Check if signal has timed out (no valid pulse recently)
        // COMPENSATED: millis() runs faster due to Timer 0 prescaler
        if ((unsigned long)(nowMs - _lastValidSignalMs) >= 
            MILLIS_COMPENSATED(Config::PWM_INPUT_TIMEOUT_MS)) {
            if (_signalValid && _debugEnabled) {
                Serial.println(F("[PWM] Signal LOST (timeout)"));
            }
            _signalValid = false;
        }
    }

    // Check if valid PWM signal is present (for mode switching)
    bool isSignalValid() const {
        return _signalValid;
    }

    // Get current duty cycle (0.0 to 1.0)
    float getDutyCycle() const {
        return _dutyCycle;
    }

    // Get detected frequency (Hz)
    float getFrequency() const {
        return _frequency;
    }

    // Get pulse period (microseconds) - calculated from frequency
    unsigned long getPeriodUs() const {
        if (_frequency > 0.0f) {
            return (unsigned long)(1000000.0f / _frequency);
        }
        return 0;
    }

    // Get high pulse width (microseconds) - calculated from duty and period
    unsigned long getHighTimeUs() const {
        return (unsigned long)(_dutyCycle * getPeriodUs());
    }

    // Get number of valid pulses detected since begin()
    unsigned long getPulsesDetected() const {
        return _pulsesDetected;
    }

    // Get current pin state (for debugging)
    int getCurrentState() const {
        return digitalRead(_pin);
    }

    // Get time since last valid signal (ms) - for debugging
    unsigned long getTimeSinceLastPulseMs() const {
        return (millis() - _lastValidSignalMs) / Config::TIMER0_PRESCALER_FACTOR;
    }

private:
    uint8_t _pin;
    float _dutyCycle;
    float _frequency;
    bool _signalValid;
    unsigned long _lastValidSignalMs;
    unsigned long _pulsesDetected;
    bool _debugEnabled;
};
