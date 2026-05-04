/* ----------------------------------------------------------------------------
   PwmRampTest.ino - FW test sketch

   Cycles PWM from 50% to 100% in 10% steps every 2 seconds, then wraps back
   to 50%. Step changes are abrupt (no smoothing).

   Mirrors the production PWM setup so the test exercises the real hardware
   path:
     - Outputs on D6 (PWM_OUT_1) and D5 (PWM_OUT_2)
     - Timer 0 / Timer 2 reconfigured for 3.9 kHz Phase-Correct PWM
     - Software compensation for hardware-inverted driver (BC817 + BC807)
     - Timer 0 prescaler change makes millis()/delay() run 8x faster, so the
       step interval is multiplied by TIMER0_PRESCALER_FACTOR
---------------------------------------------------------------------------- */
#include <Arduino.h>

constexpr uint8_t PIN_PWM_OUT_1 = 6;   // D6
constexpr uint8_t PIN_PWM_OUT_2 = 5;   // D5
constexpr bool    PWM_INVERTED_BY_HARDWARE = true;
constexpr uint8_t TIMER0_PRESCALER_FACTOR  = 8;

constexpr uint8_t  DUTY_MIN_PERCENT  = 50;
constexpr uint8_t  DUTY_MAX_PERCENT  = 100;
constexpr uint8_t  DUTY_STEP_PERCENT = 10;
constexpr unsigned long STEP_INTERVAL_MS = 2000;

static uint8_t       g_currentDutyPercent = DUTY_MIN_PERCENT;
static unsigned long g_lastStepMs = 0;

static void writeDutyPercent(uint8_t percent) {
    if (percent > 100) percent = 100;

    int pwmValue = static_cast<int>((percent / 100.0f) * 255.0f + 0.5f);
    if (PWM_INVERTED_BY_HARDWARE) {
        pwmValue = 255 - pwmValue;
    }
    analogWrite(PIN_PWM_OUT_1, pwmValue);
    analogWrite(PIN_PWM_OUT_2, pwmValue);
}

static void configureHighFreqPwm() {
    // Timer 2 (D3, D11): Phase-Correct PWM, prescaler 8 -> 3906 Hz
    TCCR2A = (TCCR2A & 0xFC) | 0x01;   // WGM21=0, WGM20=1
    TCCR2B = (TCCR2B & 0xF7);          // WGM22=0
    TCCR2B = (TCCR2B & 0xF8) | 0x02;   // CS22=0, CS21=1, CS20=0 (prescaler 8)

    // Timer 0 (D5, D6): Phase-Correct PWM, prescaler 8 -> 3906 Hz
    // WARNING: changes millis()/delay() to run 8x faster
    TCCR0A = (TCCR0A & 0xFC) | 0x01;
    TCCR0B = (TCCR0B & 0xF7);
    TCCR0B = (TCCR0B & 0xF8) | 0x02;
}

void setup() {
    Serial.begin(115200);

    // Drive pins to a safe (motor OFF) state before enabling output.
    // With inverted hardware: HIGH = MOSFET OFF.
    if (PWM_INVERTED_BY_HARDWARE) {
        digitalWrite(PIN_PWM_OUT_1, HIGH);
        digitalWrite(PIN_PWM_OUT_2, HIGH);
    } else {
        digitalWrite(PIN_PWM_OUT_1, LOW);
        digitalWrite(PIN_PWM_OUT_2, LOW);
    }
    delayMicroseconds(100);
    pinMode(PIN_PWM_OUT_1, OUTPUT);
    pinMode(PIN_PWM_OUT_2, OUTPUT);

    configureHighFreqPwm();

    // Hold OFF for ~100 ms before starting the ramp.
    // delayMicroseconds is unaffected by the Timer 0 prescaler change.
    writeDutyPercent(0);
    delayMicroseconds(100000UL);

    Serial.println(F("========================================"));
    Serial.println(F("  PWM Ramp Test (50% -> 100%, step 10%) "));
    Serial.println(F("========================================"));
    Serial.println(F("2 seconds per step, abrupt transitions, wraps back to 50%."));

    g_currentDutyPercent = DUTY_MIN_PERCENT;
    writeDutyPercent(g_currentDutyPercent);
    g_lastStepMs = millis();

    Serial.print(F("Duty: "));
    Serial.print(g_currentDutyPercent);
    Serial.println(F(" %"));
}

void loop() {
    const unsigned long now = millis();
    const unsigned long compensatedInterval =
        STEP_INTERVAL_MS * TIMER0_PRESCALER_FACTOR;

    if ((unsigned long)(now - g_lastStepMs) >= compensatedInterval) {
        g_lastStepMs = now;

        if (g_currentDutyPercent >= DUTY_MAX_PERCENT) {
            g_currentDutyPercent = DUTY_MIN_PERCENT;
        } else {
            g_currentDutyPercent += DUTY_STEP_PERCENT;
        }

        writeDutyPercent(g_currentDutyPercent);

        Serial.print(F("Duty: "));
        Serial.print(g_currentDutyPercent);
        Serial.println(F(" %"));
    }
}
