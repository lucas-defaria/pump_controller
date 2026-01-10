#pragma once
#include <Arduino.h>
#include "Config.h"

// -----------------------------------------------------------------------------
// MapSensor - Leitura e conversão do sensor MPX5700ASX para pressão (bar)
// Fórmula típica (datasheet): Vout = Vs * (0.00125 * P(kPa) + 0.04)
// => P(kPa) = (Vout/Vs - 0.04) / 0.00125
// => P(bar) = P(kPa) / 100
// Assumimos Vs = 5V (alimentação Arduino). Ajuste se diferente.
// -----------------------------------------------------------------------------
class MapSensor {
public:
    explicit MapSensor(uint8_t pin, float filterAlpha = Config::MAP_FILTER_ALPHA)
        : _pin(pin), _alpha(filterAlpha) {}

    void begin() {
        pinMode(_pin, INPUT);
        _filteredVoltage = sampleVoltage();
    }

    // Atualiza leitura filtrada e retorna pressão em bar
    float readPressureBar() {
        float v = sampleVoltage();
        _filteredVoltage = _alpha * v + (1.0f - _alpha) * _filteredVoltage;
        return voltageToBar(_filteredVoltage);
    }

    float rawVoltage() const { return _filteredVoltage; }

private:
    uint8_t _pin;
    float _alpha;
    float _filteredVoltage = 0.0f;

    static constexpr float VS = 5.0f; // tensão de referência sensor

    float sampleVoltage() const {
        int adc = analogRead(_pin); // 0..1023
        return (adc / 1023.0f) * VS;
    }

    float voltageToBar(float v) const {
        float ratio = v / VS; // Vout/Vs
        float p_kPa = (ratio - 0.04f) / 0.00125f; // inversão
        if (p_kPa < 0) p_kPa = 0; // proteção contra valores negativos
        return p_kPa / 100.0f;    // kPa -> bar
    }
};
