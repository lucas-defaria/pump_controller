#pragma once
#include <Arduino.h>
#include "Config.h"

// -----------------------------------------------------------------------------
// CurrentSensor - Measures current using ACS772LCB-100U Hall-effect sensor
// -----------------------------------------------------------------------------
// ACS772LCB-100U Specifications:
//   - Bidirectional: ±100A measurement range
//   - Sensitivity: 20mV/A (0.020 V/A)
//   - Zero current output (Quiescent): Vcc/2 (typ. 2.5V @ 5V supply)
//   - Supply voltage: 5V
//   - Output voltage range: 0.5V to 4.5V
//
// Output voltage formula: Vout = Vcc/2 + (I × Sensitivity)
// Current calculation:    I = (Vout - Vcc/2) / Sensitivity
//
// For positive current flow through IP+ to IP-:
//   Vout > 2.5V (increases with current)
// For negative current (reverse flow):
//   Vout < 2.5V (decreases with current)
// -----------------------------------------------------------------------------
class CurrentSensor {
public:
    explicit CurrentSensor(uint8_t pin) 
        : _pin(pin)
        , _filteredVoltage(Config::ACS772_ZERO_CURRENT_V)
        , _initialized(false)
    {}

    void begin() {
        pinMode(_pin, INPUT);
        
        // Initialize filter with first reading to avoid startup transient
        int adc = analogRead(_pin);
        _filteredVoltage = adcToVoltage(adc);
        _initialized = true;
    }

    // Returns instantaneous current in Amperes (with EMA filtering)
    float readCurrentA() {
        int adc = analogRead(_pin);
        float voltage = adcToVoltage(adc);
        
        // Apply Exponential Moving Average filter for noise reduction
        if (_initialized) {
            _filteredVoltage = (Config::CURRENT_FILTER_ALPHA * voltage) + 
                              ((1.0f - Config::CURRENT_FILTER_ALPHA) * _filteredVoltage);
        } else {
            _filteredVoltage = voltage;
            _initialized = true;
        }
        
        // Calculate current: I = (Vout - Vzero) / Sensitivity
        float current = (_filteredVoltage - Config::ACS772_ZERO_CURRENT_V) / 
                        Config::ACS772_SENSITIVITY;
        
        return current;
    }

    // Returns raw unfiltered current (for diagnostics)
    float readCurrentRawA() {
        int adc = analogRead(_pin);
        float voltage = adcToVoltage(adc);
        float current = (voltage - Config::ACS772_ZERO_CURRENT_V) / 
                        Config::ACS772_SENSITIVITY;
        return current;
    }

    // Returns filtered output voltage (for diagnostics)
    float getFilteredVoltage() const {
        return _filteredVoltage;
    }

    // Reset filter (useful after power cycling or fault recovery)
    void resetFilter() {
        int adc = analogRead(_pin);
        _filteredVoltage = adcToVoltage(adc);
    }

private:
    uint8_t _pin;
    float _filteredVoltage;  // EMA filtered voltage reading
    bool _initialized;

    // Convert ADC reading to voltage
    // Arduino ADC: 10-bit (0-1023), reference voltage configurable
    inline float adcToVoltage(int adc) const {
        return (adc / 1023.0f) * Config::ADC_REFERENCE_VOLTAGE;
    }
};
