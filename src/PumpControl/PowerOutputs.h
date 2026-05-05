#pragma once
#include <Arduino.h>
#include "Config.h"

// -----------------------------------------------------------------------------
// PowerOutputs - Manages two main PWM outputs for MOSFET driver
// with voltage limiting capability for protection system
// -----------------------------------------------------------------------------
// Both outputs use Timer 0 hardware PWM (D5=OC0B, D6=OC0A).
// Timer 0 has NO pin overlap with SPI, so CAN bus (MCP2515) doesn't interfere.
// Previously D3 (Timer 2 OC2B) was used, but SPI shares D11 (OC2A) with Timer 2,
// causing periodic PWM glitches on D3 when CAN was active.
// -----------------------------------------------------------------------------
class PowerOutputs {
public:
    PowerOutputs(uint8_t pin1, uint8_t pin2)
        : _pin1(pin1)
        , _pin2(pin2)
        , _currentDuty(0.0f)
        , _voltageLimit(1.0f)
        , _supplyVoltage(12.0f)  // Initialize to nominal 12V, will be updated dynamically
        , _isHiZ(false)
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

        // Step 4: Configure high-frequency PWM (3.9 kHz)
        // Both D5 (OC0B) and D6 (OC0A) are on Timer 0 — no SPI conflict.
        // Timer 0 modification affects millis() and delay() - they run 8x faster!
        // All timing code compensated using MILLIS_COMPENSATED() macro.
        if (Config::ENABLE_HIGH_FREQ_PWM) {
            // Configure Timer 0 (pins D5, D6) for 3.9 kHz Phase-Correct PWM
            // Phase-Correct PWM: 16MHz / (2 * 256 * 8) = 3906.25 Hz ≈ 3.9 kHz
            // Set WGM02=0, WGM01=0, WGM00=1 for Phase-Correct PWM, TOP=0xFF
            TCCR0A = (TCCR0A & 0xFC) | 0x01;  // WGM01=0, WGM00=1
            TCCR0B = (TCCR0B & 0xF7);         // WGM02=0
            // Set prescaler to 8 (CS02=0, CS01=1, CS00=0)
            TCCR0B = (TCCR0B & 0xF8) | 0x02;  // Prescaler 8 (8x faster timing!)
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

        // In Hi-Z (slave) mode the µC pins are disconnected from the gate driver
        // so the external PWM (uC_IN1) can drive T4 alone via R30 without fighting
        // R28. Skip analogWrite — it would re-enable OUTPUT and force a battle on
        // the base node. Duty is still cached for diagnostics and for the moment
        // we leave Hi-Z (setHiZ(false) re-applies it).
        if (_isHiZ) {
            return;
        }

        writeDutyToPins(duty);
    }

    // Switch the output stage between active OUTPUT mode and Hi-Z (INPUT).
    // Hi-Z (true): pins go to INPUT (no pullup). Lets external PWM at uC_IN1
    //              control T4 base alone (R30=1K dominates R26=100K pulldown).
    // Hi-Z (false): drive HIGH at hardware level FIRST (= MOSFET OFF via the
    //               inverted driver), then pinMode(OUTPUT), then re-apply the
    //               cached duty cycle. The HIGH-before-OUTPUT order avoids a
    //               brief LOW glitch (= MOSFET full ON) during the transition.
    void setHiZ(bool hiZ) {
        if (hiZ == _isHiZ) return;

        if (hiZ) {
            // Disengage µC from the gate driver base node. analogWrite stays on
            // the OCRn registers (Timer 0 keeps running) but the pin direction
            // change disconnects it from PORT. Use INPUT (no pullup) so R30
            // alone biases T4.
            pinMode(_pin1, INPUT);
            pinMode(_pin2, INPUT);
            _isHiZ = true;
        } else {
            // Drive safe state BEFORE re-engaging OUTPUT, otherwise PORT may hold
            // the last value (potentially LOW = motor full).
            digitalWrite(_pin1, Config::PWM_INVERTED_BY_HARDWARE ? HIGH : LOW);
            digitalWrite(_pin2, Config::PWM_INVERTED_BY_HARDWARE ? HIGH : LOW);
            pinMode(_pin1, OUTPUT);
            pinMode(_pin2, OUTPUT);
            _isHiZ = false;
            // Re-apply cached duty so the timer drives the pins correctly.
            writeDutyToPins(_currentDuty);
        }
    }

    // Whether outputs are currently in Hi-Z (slave passthrough) state.
    bool isHiZ() const { return _isHiZ; }

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
    void writeDutyToPins(float duty) {
        // Convert duty cycle to PWM value (0-255)
        int pwmValue = static_cast<int>(duty * 255.0f + 0.5f);

        // Hardware inversion compensation:
        // If driver circuit inverts signal (NPN+PNP topology), invert PWM in software
        // This ensures: duty=1.0 → MOSFET ON (full power), duty=0.0 → MOSFET OFF
        if (Config::PWM_INVERTED_BY_HARDWARE) {
            pwmValue = 255 - pwmValue;  // Invert: 0→255, 255→0
        }

        // Both outputs on Timer 0 - hardware PWM, no SPI conflict
        analogWrite(_pin1, pwmValue);  // D6 (OC0A)
        analogWrite(_pin2, pwmValue);  // D5 (OC0B)
    }

    uint8_t _pin1;
    uint8_t _pin2;
    float _currentDuty;      // Current requested duty cycle (before limiting)
    float _voltageLimit;     // Voltage limit factor from protection system
    float _supplyVoltage;    // Measured supply voltage (updated dynamically)
    bool  _isHiZ;            // True when outputs are tri-stated (slave passthrough)
};
