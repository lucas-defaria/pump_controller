#pragma once
#include <Arduino.h>
#include <math.h>
#include "Config.h"

// -----------------------------------------------------------------------------
// TempSensor - Heatsink temperature via NTC 10K thermistor
// -----------------------------------------------------------------------------
// Hardware (see Config.h for full schematic):
//   +5V -- R_PULLUP --+-- NTC -- GND
//                     +-- ADC pin (high-impedance via R20)
//
// Beta equation:
//   1/T = 1/T25 + (1/B) * ln(R_NTC / R25)
//   T_celsius = T_kelvin - 273.15
//
// Monitoring only - no protection logic.
// -----------------------------------------------------------------------------

class TempSensor {
public:
    explicit TempSensor(uint8_t pin)
        : _pin(pin)
        , _filteredTempC(25.0f)
        , _initialized(false)
    {}

    void begin() {
        pinMode(_pin, INPUT);
        int adc = analogRead(_pin);
        _filteredTempC = adcToCelsius(adc);
        _initialized = true;
    }

    // Returns heatsink temperature in Celsius (with EMA filtering)
    float readTemperatureC() {
        int adc = analogRead(_pin);
        float tempC = adcToCelsius(adc);

        if (_initialized) {
            _filteredTempC = (Config::TEMP_FILTER_ALPHA * tempC) +
                             ((1.0f - Config::TEMP_FILTER_ALPHA) * _filteredTempC);
        } else {
            _filteredTempC = tempC;
            _initialized = true;
        }
        return _filteredTempC;
    }

    float getFilteredTemperatureC() const {
        return _filteredTempC;
    }

    // Sensor sanity check: ADC near 0 = NTC short to GND, near 1023 = open circuit
    bool isSensorOk() const {
        return _filteredTempC > -40.0f && _filteredTempC < 150.0f;
    }

private:
    uint8_t _pin;
    float _filteredTempC;
    bool _initialized;

    float adcToCelsius(int adc) const {
        // Guard against open/shorted sensor (avoid div-by-zero / log(0))
        if (adc <= 0)    return 150.0f;   // NTC shorted -> very high temp reading
        if (adc >= 1023) return -40.0f;   // NTC open    -> very low  temp reading

        // Divider supply and ADC reference are the same +5V rail -> cancels out
        float rNtc = Config::NTC_R_PULLUP * (float)adc / (float)(1023 - adc);

        float invT = (1.0f / Config::NTC_T25_KELVIN) +
                     (1.0f / Config::NTC_BETA) * log(rNtc / Config::NTC_R25);
        float tKelvin = 1.0f / invT;
        return tKelvin - 273.15f;
    }
};
