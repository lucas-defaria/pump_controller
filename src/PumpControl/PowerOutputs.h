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
        , _supplyVoltage(12.0f)  // Initialize to nominal 12V, will be updated dynamically
    {}

    void begin() {
        // CRITICAL: Initialize pins to safe state BEFORE configuring as OUTPUT
        // Hardware circuit inverts PWM: HIGH=OFF, LOW=ON
        // We want motor OFF during initialization, so set pins HIGH first
        
        // Step 1: Set pins to safe state while still INPUT (high impedance)
        // With inverted circuit: digitalWrite HIGH = BC817 ON = BC807 OFF = MOSFET OFF
        if (Config::PWM_INVERTED_BY_HARDWARE) {
            digitalWrite(_pin1, HIGH);  // Motor OFF (inverted circuit)
            digitalWrite(_pin2, HIGH);  // Motor OFF (inverted circuit)
        } else {
            digitalWrite(_pin1, LOW);   // Motor OFF (normal circuit)
            digitalWrite(_pin2, LOW);   // Motor OFF (normal circuit)
        }
        
        // Step 2: Small delay to ensure pins stabilize before OUTPUT mode
        delayMicroseconds(100);
        
        // Step 3: Configure as OUTPUT (pins already in safe state)
        pinMode(_pin1, OUTPUT);
        pinMode(_pin2, OUTPUT);
        
        // Step 4: Configure high-frequency PWM (31.25 kHz)
        // CRITICAL: This modifies Timer 0 and Timer 2 to Phase-Correct PWM mode
        // Timer 0 modification affects millis() and delay() - they run 64x faster!
        // All timing code has been compensated using MILLIS_COMPENSATED() macro
        if (Config::ENABLE_HIGH_FREQ_PWM) {
            // Configure Timer 2 (pins D3, D11) for 31.25 kHz Phase-Correct PWM
            // Phase-Correct PWM: 16MHz / 512 / 1 = 31.25 kHz (counts up and down)
            // Set WGM22=0, WGM21=0, WGM20=1 for Phase-Correct PWM, TOP=0xFF
            TCCR2A = (TCCR2A & 0xFC) | 0x01;  // WGM21=0, WGM20=1
            TCCR2B = (TCCR2B & 0xF7);         // WGM22=0
            // Set prescaler to 1 (CS22=0, CS21=0, CS20=1)
            TCCR2B = (TCCR2B & 0xF8) | 0x01;
            
            // Configure Timer 0 (pins D5, D6) for 31.25 kHz Phase-Correct PWM
            // WARNING: This makes millis() and delay() run 64x faster!
            // Phase-Correct PWM: 16MHz / 512 / 1 = 31.25 kHz
            // Set WGM02=0, WGM01=0, WGM00=1 for Phase-Correct PWM, TOP=0xFF
            TCCR0A = (TCCR0A & 0xFC) | 0x01;  // WGM01=0, WGM00=1
            TCCR0B = (TCCR0B & 0xF7);         // WGM02=0
            // Set prescaler to 1 (CS02=0, CS01=0, CS00=1)
            TCCR0B = (TCCR0B & 0xF8) | 0x01;  // Prescaler 1 (64x faster timing!)
        }
        
        // Step 5: Set initial duty cycle to 0% (motor OFF)
        // This will write PWM=255 (HIGH) which keeps motor OFF with inverted circuit
        setDuty(0.0f);
        
        // Step 6: Additional safety delay before normal operation
        // Using delayMicroseconds because it's not affected by Timer 0 prescaler change
        delayMicroseconds(100000);  // 100ms grace period
    }

    // Set output as percentage of supply voltage (0.0 to 1.0)
    // Example: 0.70 = 70% of measured supply voltage
    // Respects current voltage limit from protection system
    void setOutputPercent(float percent) {
        if (percent < 0) percent = 0;
        if (percent > 1) percent = 1;
        
        // Apply voltage limit from protection system
        float duty = percent * _voltageLimit;
        
        setDuty(duty);
    }

    // Legacy method: Set output voltage in Volts (for backward compatibility)
    // Internally converts to percentage using current supply voltage
    void setOutputVoltage(float voltage) {
        if (voltage < 0) voltage = 0;
        if (voltage > _supplyVoltage) voltage = _supplyVoltage;
        
        float percent = voltage / _supplyVoltage;  // Convert to percentage
        setOutputPercent(percent);
    }

    // Update measured supply voltage (call this every cycle with fresh reading)
    void setSupplyVoltage(float voltage) {
        if (voltage < 7.0f) voltage = 7.0f;    // Clamp to reasonable minimum
        if (voltage > 16.0f) voltage = 16.0f;  // Clamp to reasonable maximum
        _supplyVoltage = voltage;
    }

    // Get current supply voltage
    float getSupplyVoltage() const {
        return _supplyVoltage;
    }

    // Set duty cycle directly (0.0 to 1.0)
    // Note: Hardware circuit (BC817+BC807 driver) inverts PWM signal
    // Software compensates for this inversion when PWM_INVERTED_BY_HARDWARE=true
    void setDuty(float duty) {
        if (duty < 0) duty = 0;
        if (duty > 1) duty = 1;
        
        _currentDuty = duty;
        
        // Convert duty cycle to PWM value (0-255)
        int pwmValue = static_cast<int>(duty * 255.0f + 0.5f);
        
        // Hardware inversion compensation:
        // If driver circuit inverts signal (NPN+PNP topology), invert PWM in software
        // This ensures: duty=1.0 ? MOSFET ON (full power), duty=0.0 ? MOSFET OFF
        if (Config::PWM_INVERTED_BY_HARDWARE) {
            pwmValue = 255 - pwmValue;  // Invert: 0?255, 255?0
        }
        
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

    // Get actual output voltage accounting for limit and measured supply
    float getActualOutputVoltage() const {
        return _currentDuty * _voltageLimit * _supplyVoltage;
    }

private:
    uint8_t _pin1;
    uint8_t _pin2;
    float _currentDuty;      // Current requested duty cycle (before limiting)
    float _voltageLimit;     // Voltage limit factor from protection system
    float _supplyVoltage;    // Measured supply voltage (updated dynamically)
};
