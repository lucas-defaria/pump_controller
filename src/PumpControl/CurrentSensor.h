#pragma once
#include <Arduino.h>
#include "Config.h"

// -----------------------------------------------------------------------------
// CurrentSensor (Stub) - Futuro: medir corrente usando ACS772LCB (pinos A2, A3)
// ACS772: Saída centrada em Vcc/2 com sensibilidade dependente do modelo.
// Aqui apenas estrutura para futuro.
// -----------------------------------------------------------------------------
class CurrentSensor {
public:
    explicit CurrentSensor(uint8_t pin) : _pin(pin) {}
    void begin() { pinMode(_pin, INPUT); }

    // Retorna corrente estimada em Ampere (placeholder)
    float readCurrentA() {
        int adc = analogRead(_pin);
        float v = (adc / 1023.0f) * 5.0f; // assume 5V ref
        // TODO: aplicar fórmula real: I = (Vout - Vcc/2)/Sensibilidade
        return v; // placeholder
    }
private:
    uint8_t _pin;
};
