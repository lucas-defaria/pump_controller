#pragma once
#include <Arduino.h>
#include "Config.h"

// -----------------------------------------------------------------------------
// MapSensor - Leitura e conversão do sensor MPX5700AP para pressão MAP (bar gauge)
// 
// MPX5700AP: Absolute pressure sensor (15-700 kPa absolute)
// Fórmula datasheet: Vout = Vs * (0.00125 * P(kPa) + 0.04)
// => P(kPa) = (Vout/Vs - 0.04) / 0.00125
// => P(bar absolute) = P(kPa) / 100
// => P(bar gauge) = P(bar absolute) - ATMOSPHERIC_PRESSURE_BAR
// 
// Verificado: 833mV @ atmosfera = 1.013 bar abs = 0 bar gauge
// Vs = 5V (alimentação Arduino)
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
        float p_kPa = (ratio - 0.04f) / 0.00125f; // inversão da fórmula do datasheet
        float p_bar_absolute = p_kPa / 100.0f;    // kPa -> bar (pressão absoluta)
        
        // Converter de pressão absoluta para gauge (relativa à atmosfera)
        // Gauge = Absoluto - Atmosférico
        // Valores negativos = vácuo (abaixo da pressão atmosférica)
        // Valores positivos = boost (acima da pressão atmosférica)
        float p_bar_gauge = p_bar_absolute - Config::ATMOSPHERIC_PRESSURE_BAR;
        
        return p_bar_gauge; // Retorna pressão gauge (pode ser negativa)
    }
};
