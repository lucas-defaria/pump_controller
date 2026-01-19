#pragma once
#include <Arduino.h>
#include "Config.h"

// -----------------------------------------------------------------------------
// VoltageSensor - Measures supply voltage via resistive divider
// -----------------------------------------------------------------------------
// Hardware Configuration:
//   - Voltage divider: R1=10k? (to GND), R2=1k? (to VCC)
//   - Divider ratio: 1/11 = 0.0909
//   - ADC input range: 0-5V (Arduino Uno 10-bit ADC)
//   - Supply voltage range: 8V to 14.5V (automotive)
//
// Voltage calculation:
//   V_ADC = V_SUPPLY × (R2 / (R1 + R2))
//   V_SUPPLY = V_ADC / VOLTAGE_DIVIDER_RATIO
//
// At 12V supply: ADC sees 12V × 0.0909 = 1.09V (safe)
// At 14.5V max:  ADC sees 14.5V × 0.0909 = 1.32V (safe)
// At 8V min:     ADC sees 8V × 0.0909 = 0.73V (safe)
// -----------------------------------------------------------------------------

class VoltageSensor {
public:
    explicit VoltageSensor(uint8_t pin) 
        : _pin(pin)
        , _filteredVoltage(12.0f)  // Initialize to nominal 12V
        , _initialized(false)
    {}

    void begin() {
        pinMode(_pin, INPUT);
        
        // Initialize filter with first reading to avoid startup transient
        int adc = analogRead(_pin);
        _filteredVoltage = adcToSupplyVoltage(adc);
        _initialized = true;
    }

    // Returns supply voltage in Volts (with EMA filtering)
    float readVoltage() {
        int adc = analogRead(_pin);
        float voltage = adcToSupplyVoltage(adc);
        
        // Apply Exponential Moving Average filter for noise reduction
        if (_initialized) {
            _filteredVoltage = (Config::VOLTAGE_FILTER_ALPHA * voltage) + 
                              ((1.0f - Config::VOLTAGE_FILTER_ALPHA) * _filteredVoltage);
        } else {
            _filteredVoltage = voltage;
            _initialized = true;
        }
        
        return _filteredVoltage;
    }

    // Get filtered voltage without triggering new ADC read
    float getFilteredVoltage() const {
        return _filteredVoltage;
    }

    // Check if sensor reading is valid (within expected automotive range)
    bool isValid() const {
        return _filteredVoltage >= Config::VOLTAGE_MINIMUM_VALID &&
               _filteredVoltage <= 16.0f;  // Reasonable upper limit
    }

private:
    uint8_t _pin;
    float _filteredVoltage;
    bool _initialized;

    // Convert ADC reading to supply voltage accounting for divider
    float adcToSupplyVoltage(int adc) const {
        // ADC to voltage at divider output
        float adcVoltage = (adc / 1023.0f) * Config::ADC_REFERENCE_VOLTAGE;
        
        // Calculate actual supply voltage from divider ratio
        // V_supply = V_adc / divider_ratio
        float supplyVoltage = adcVoltage / Config::VOLTAGE_DIVIDER_RATIO;
        
        return supplyVoltage;
    }
};
