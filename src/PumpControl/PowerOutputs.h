#pragma once
#include <Arduino.h>
#include "Config.h"

// -----------------------------------------------------------------------------
// PowerOutputs - Gerencia duas sa�das PWM principais (SSR simulados)
// e potencialmente replica para sa�das extras (stubs futuros)
// -----------------------------------------------------------------------------
class PowerOutputs {
public:
    PowerOutputs(uint8_t pin1, uint8_t pin2) : _pin1(pin1), _pin2(pin2) {}

    void begin() {
        pinMode(_pin1, OUTPUT);
        pinMode(_pin2, OUTPUT);
        // Extras (podem conflitar com Serial): cuidado ao habilitar futuramente
        // pinMode(Config::PIN_EXTRA_PWM_1, OUTPUT);
        // pinMode(Config::PIN_EXTRA_PWM_2, OUTPUT);
        setDuty(0.0f);
    }

    // dutyVoltage em Volts -> converte para duty cycle
    void setOutputVoltage(float voltage) {
        if (voltage < 0) voltage = 0;
        if (voltage > Config::SUPPLY_VOLTAGE) voltage = Config::SUPPLY_VOLTAGE;
        float duty = voltage / Config::SUPPLY_VOLTAGE; // 0..1
        setDuty(duty);
    }

    void setDuty(float duty) {
        if (duty < 0) duty = 0;
        if (duty > 1) duty = 1;
        int pwmValue = static_cast<int>(duty * 255.0f + 0.5f);
        analogWrite(_pin1, pwmValue);
        analogWrite(_pin2, pwmValue);

        Serial.print(F(" | PWM: ")); Serial.println(pwmValue);
        // Replica para extras (se habilitado futuramente)
        // analogWrite(Config::PIN_EXTRA_PWM_1, pwmValue);
        // analogWrite(Config::PIN_EXTRA_PWM_2, pwmValue);
    }

private:
    uint8_t _pin1;
    uint8_t _pin2;
};
