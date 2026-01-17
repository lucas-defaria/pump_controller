#pragma once
#include <Arduino.h>
#include "Config.h"

// -----------------------------------------------------------------------------
// PowerOutputs - Manages two main PWM outputs (simulating SSRs)
// with voltage limiting capability for protection system
// -----------------------------------------------------------------------------
class PowerOutputs {
public:
    PowerOutputs(uint8_t pin1, uint8_t pin2) 
        : _pin1(pin1)
        , _pin2(pin2)
        , _currentDuty(0.0f)
        , _voltageLimit(1.0f)
    {}

    void begin() {
        pinMode(_pin1, OUTPUT);
        pinMode(_pin2, OUTPUT);
        // Future: Extra PWM outputs (careful - may conflict with Serial)
        // pinMode(Config::PIN_EXTRA_PWM_1, OUTPUT);
        // pinMode(Config::PIN_EXTRA_PWM_2, OUTPUT);
        setDuty(0.0f);
    }

    // Set output voltage in Volts (respects current voltage limit)
    void setOutputVoltage(float voltage) {
        if (voltage < 0) voltage = 0;
        if (voltage > Config::SUPPLY_VOLTAGE) voltage = Config::SUPPLY_VOLTAGE;
        
        float duty = voltage / Config::SUPPLY_VOLTAGE; // Convert to 0..1
        
        // Apply voltage limit from protection system
        duty = duty * _voltageLimit;
        
        setDuty(duty);
    }

    // Set duty cycle directly (0.0 to 1.0)
    void setDuty(float duty) {
        if (duty < 0) duty = 0;
        if (duty > 1) duty = 1;
        
        _currentDuty = duty;
        
        int pwmValue = static_cast<int>(duty * 255.0f + 0.5f);
        analogWrite(_pin1, pwmValue);
        analogWrite(_pin2, pwmValue);

        // Future: Replicate to extra PWM outputs if enabled
        // analogWrite(Config::PIN_EXTRA_PWM_1, pwmValue);
        // analogWrite(Config::PIN_EXTRA_PWM_2, pwmValue);
    }

    // Set voltage limit factor (0.0 to 1.0)
    // Called by protection system to progressively reduce power
    void setVoltageLimit(float limit) {
        if (limit < 0) limit = 0;
        if (limit > 1) limit = 1;
        _voltageLimit = limit;
    }

    // Get current voltage limit factor
    float getVoltageLimit() const {
        return _voltageLimit;
    }

    // Get current duty cycle (0.0 to 1.0)
    float getCurrentDuty() const {
        return _currentDuty;
    }

    // Get actual output voltage accounting for limit
    float getActualOutputVoltage() const {
        return _currentDuty * Config::SUPPLY_VOLTAGE * _voltageLimit;
    }

private:
    uint8_t _pin1;
    uint8_t _pin2;
    float _currentDuty;      // Current requested duty cycle (before limiting)
    float _voltageLimit;     // Voltage limit factor from protection system
};
