#pragma once
#include <Arduino.h>
#include "Config.h"

// -----------------------------------------------------------------------------
// CurrentSensor - Measures current using ACS772LCB-050B Hall-effect sensor
// -----------------------------------------------------------------------------
// ACS772LCB-050B Specifications:
//   - Unidirectional: 0 to +50A measurement range
//   - Sensitivity: 40mV/A (0.040 V/A)
//   - Zero current output: 0.6V (measured, confirms unidirectional variant)
//   - Supply voltage: 5V
//   - Output voltage range: 0.6V (0A) to 2.6V (50A)
//
// Output voltage formula: Vout = Vzero + (I × Sensitivity)
// Current calculation:    I = (Vout - Vzero) / Sensitivity
//
// PWM interference mitigation:
//   - Multi-sample averaging (configurable via Config::CURRENT_ADC_SAMPLES)
//   - EMA filtering for additional smoothing
//   - Samples taken with microsecond delays to average PWM cycles
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

    // Returns instantaneous current in Amperes (with multi-sample averaging and EMA filtering)
    float readCurrentA() {
        // Multi-sample averaging to filter PWM noise
        // Takes multiple ADC readings with small delays to average out PWM cycles
        float voltage = readVoltageAveraged();
        
        // Apply Exponential Moving Average filter for additional noise reduction
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
        
        // Clamp to valid range (0 to max) for unidirectional sensor
        if (current < 0.0f) current = 0.0f;
        if (current > Config::ACS772_MAX_CURRENT) current = Config::ACS772_MAX_CURRENT;
        
        return current;
    }

    // Returns raw unfiltered current (single sample, for diagnostics)
    float readCurrentRawA() {
        int adc = analogRead(_pin);
        float voltage = adcToVoltage(adc);
        float current = (voltage - Config::ACS772_ZERO_CURRENT_V) / 
                        Config::ACS772_SENSITIVITY;
        if (current < 0.0f) current = 0.0f;
        if (current > Config::ACS772_MAX_CURRENT) current = Config::ACS772_MAX_CURRENT;
        return current;
    }
    
    // Returns averaged voltage reading (for diagnostics)
    float readVoltageRaw() {
        return readVoltageAveraged();
    }

    // Returns filtered output voltage (for diagnostics)
    float getFilteredVoltage() const {
        return _filteredVoltage;
    }

    // Reset filter (useful after power cycling or fault recovery)
    void resetFilter() {
        _filteredVoltage = readVoltageAveraged();
    }

private:
    uint8_t _pin;
    float _filteredVoltage;  // EMA filtered voltage reading
    bool _initialized;

    // Read voltage with multi-sample averaging to filter PWM interference
    // Takes multiple ADC samples with small delays to average PWM cycles
    // PWM at 977Hz = ~1.02ms period, so 10 samples @ 50µs = 500µs covers ~half cycle
    // SAFETY: uint32_t can hold up to 4,194,303 samples (each ADC read max 1023)
    // Current CURRENT_ADC_SAMPLES=10 is safe. Max safe value = 4194 samples.
    float readVoltageAveraged() {
        uint32_t adcSum = 0;
        
        for (uint8_t i = 0; i < Config::CURRENT_ADC_SAMPLES; i++) {
            adcSum += analogRead(_pin);
            if (i < Config::CURRENT_ADC_SAMPLES - 1) {
                delayMicroseconds(Config::CURRENT_ADC_DELAY_US);
            }
        }
        
        float avgAdc = adcSum / (float)Config::CURRENT_ADC_SAMPLES;
        return adcToVoltage(avgAdc);
    }

    // Convert ADC reading to voltage
    // Arduino ADC: 10-bit (0-1023), reference voltage configurable
    inline float adcToVoltage(float adc) const {
        return (adc / 1023.0f) * Config::ADC_REFERENCE_VOLTAGE;
    }
};
