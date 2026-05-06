#pragma once

// -----------------------------------------------------------------------------
// Config.h - Compile-time configuration constants for Fan Control project
// -----------------------------------------------------------------------------
// Adjust setpoints, thresholds, and operational parameters here.
// Nothing is user-configurable at runtime.
// -----------------------------------------------------------------------------

namespace Config {
    // =========================================================================
    // TEMPERATURE CONTROL - Transmission Oil Temperature
    // =========================================================================

    // Fan control based on transmission oil temperature (from CAN bus)
    // Below TEMP_FAN_OFF_C:  fan OFF (0% PWM)
    // Between OFF and FULL:  linear ramp
    // Above TEMP_FAN_FULL_C: fan FULL (100% PWM)

    constexpr float TEMP_FAN_OFF_C   = 80.0f;  // °C - below this = fan OFF
    constexpr float TEMP_FAN_FULL_C  = 100.0f; // °C - above this = fan 100%

    // Output percent range for temperature control
    constexpr float OUTPUT_PERCENT_MIN = 0.00f; // 0% at/below TEMP_FAN_OFF_C
    constexpr float OUTPUT_PERCENT_MAX = 1.00f; // 100% at/above TEMP_FAN_FULL_C

    // =========================================================================
    // CAN BUS - MCP2515 via SPI
    // =========================================================================

    // MCP2515 SPI pins (SPI: MOSI=D11, MISO=D12, SCK=D13)
    constexpr uint8_t PIN_CAN_CS  = 10; // D10 - MCP2515 chip select (hardware SS pin on ATmega328P)
    constexpr uint8_t PIN_CAN_INT = 4;  // D4  - MCP2515 interrupt

    // CAN message IDs and byte mapping (extensible - add new IDs here)
    constexpr uint32_t CAN_ID_GEARBOX      = 0x418; // Gearbox message (temp, gear, sport)
    constexpr uint8_t  CAN_BYTE_TRANS_TEMP  = 2;    // Byte 2 = transmission temp (°F, 1-253)

    // Timeout: no CAN temp message received -> fan forced to 100% (safety)
    constexpr unsigned long CAN_TEMP_TIMEOUT_MS = 3000; // 3 seconds

    // Valid raw temperature range (Fahrenheit as received from CAN)
    constexpr uint8_t CAN_TEMP_RAW_MIN = 1;
    constexpr uint8_t CAN_TEMP_RAW_MAX = 253;

    // =========================================================================
    // CURRENT SENSING - ACS758LCB-050B (BIDIRECTIONAL)
    // =========================================================================

    // ACS758LCB-050B Hall-effect current sensor specifications
    // Bidirectional variant: ±50A range, nominal zero = Vcc/2, sensitivity = 40mV/A
    // Calibration on this board — two loaded points on scope (after RC filter):
    //   - 1.32A input = 2.54V output
    //   - 2.30A input = 2.58V output
    // Slope = (2.58-2.54)/(2.30-1.32) = 40.8 mV/A ≈ nominal 40 mV/A ✓
    // Zero derived from slope (not measured — Voe offset made direct 0A reading ~22mV high):
    //   Vzero = 2.54 - 1.32*0.04 = 2.488V (matches 2.58 - 2.30*0.04 = 2.488V)
    // This offset is within the ±60mV Voe spec of the datasheet.
    // Negative currents (reverse flow) are clamped to 0 in CurrentSensor.
    constexpr float ACS758_SENSITIVITY = 0.04f;        // V/A (40mV/A for 050B bidirectional)
    constexpr float ACS758_ZERO_CURRENT_V = 2.49f;    // Volts (derived @ 0A - calibrate per board)
    constexpr float ACS758_MAX_CURRENT = 50.0f;        // Amperes (sensor absolute max)
    
    // ADC configuration
    constexpr float ADC_REFERENCE_VOLTAGE = 5.02f;      // Volts (measured at module 5V pin, fed by DCDC)
    
    // Multi-sample averaging to filter PWM noise
    // PWM at 3.9kHz creates ~256us period
    // 32 samples with 50us delay ≈ 4.9ms window = ~19 PWM cycles (good averaging)
    constexpr uint8_t CURRENT_ADC_SAMPLES = 32;         // Number of ADC samples to average
    constexpr uint8_t CURRENT_ADC_DELAY_US = 50;        // Delay between samples (microseconds)

    // Current reading filter coefficient (EMA)
    // Higher alpha = faster response, more noise
    // Lower alpha = slower response, smoother
    // 0.05 @ 20Hz loop => time constant ~1s (slow but clean for protection use)
    constexpr float CURRENT_FILTER_ALPHA = 0.05f;       // 0<alpha<=1 (heavy smoothing for PWM ripple)
    
    // =========================================================================
    // VOLTAGE MONITORING - Supply voltage measurement with percentage-based protection
    // =========================================================================
    
    // Voltage divider configuration: R1=10K (to GND), R2=1K (to VCC)
    // Divider ratio: R2/(R1+R2) = 1K/(10K+1K) = 1/11 = 0.0909
    // At 12V input: ADC sees 12V � 0.0909 = 1.09V ? (safe)
    // At 14.5V input: ADC sees 14.5V � 0.0909 = 1.32V ? (safe)
    // At 8V input: ADC sees 8V � 0.0909 = 0.73V ? (safe)
    constexpr float VOLTAGE_DIVIDER_RATIO = 0.0909f;    // 1/(10+1) = 1/11
    
    // Voltage reading filter coefficient (EMA)
    constexpr float VOLTAGE_FILTER_ALPHA = 1.0f;        // 0<alpha<=1 (smoother than current)
    
    // Percentage-based voltage protection (adaptive to actual supply voltage)
    // Instead of fixed thresholds (9V, 12V), use percentage drop from measured voltage
    // This adapts automatically to automotive voltage variations (8-14.5V)
    constexpr float VOLTAGE_DROP_WARNING_PERCENT = 0.30f;   // 15% drop triggers warning
    constexpr float VOLTAGE_DROP_CRITICAL_PERCENT = 0.50f;  // 30% drop triggers critical protection
    
    // Hysteresis for voltage protection (Volts)
    constexpr float VOLTAGE_HYSTERESIS = 0.5f;               // 0.5V band to prevent oscillation
    
    // Voltage sensor validation (for fault detection only)
    // System now uses percentage-based control, so no voltage drop thresholds needed
    // Only detect sensor faults (reading outside valid automotive range)
    constexpr float VOLTAGE_MINIMUM_VALID = 7.0f;    // Volts (below this = sensor fault)
    constexpr float VOLTAGE_MAXIMUM_VALID = 16.0f;   // Volts (above this = sensor fault)
    
    // =========================================================================
    // CURRENT PROTECTION SYSTEM
    // =========================================================================
    
    // Protection threshold levels (Amperes)
    constexpr float CURRENT_THRESHOLD_FAULT    = 40.0f;  // FAULT zone starts
    
    // EMERGENCY: Absolute maximum current (sensor hardware limit)
    // ACS758LCB-050B saturates at ~50A - readings near this indicate:
    // 1. Actual current >= 50A (short circuit, severe overload)
    // 2. Sensor at maximum capability
    // Action: IMMEDIATE shutdown to prevent component damage
    constexpr float CURRENT_THRESHOLD_EMERGENCY = 45.0f;  // Emergency shutdown (90% of sensor max)
    
    // Emergency shutdown enable flag
    // WARNING: Set to true for safety, false only for testing/pump-critical applications
    // When true: System shuts down completely at EMERGENCY threshold
    // When false: System reduces to minimum (50%) but never fully shuts down
    constexpr bool ENABLE_EMERGENCY_SHUTDOWN = true;  // Set true for production with hardware protection
    
    // Hysteresis band (Amperes)
    // Prevents chattering/oscillation between protection levels
    // Level changes require crossing threshold � hysteresis
    constexpr float CURRENT_HYSTERESIS = 2.5f;           // Amperes
    
    // Percentage-based voltage limiting for protection levels
    // These percentages apply to the measured supply voltage (adaptive)
    // NORMAL:    100% (1.00) - no limiting (full power)
    // FAULT:      50% (0.50) - 50% reduction (minimum safe)
    // EMERGENCY:   0% (0.00) - COMPLETE SHUTDOWN (short circuit protection)
    constexpr float PROTECTION_PERCENT_NORMAL   = 1.00f;  // 100% - full power
    constexpr float PROTECTION_PERCENT_FAULT    = 0.50f;  // 50% - minimum safe level
    constexpr float PROTECTION_PERCENT_EMERGENCY = 0.00f; // 0% - EMERGENCY SHUTDOWN (if enabled)
    
    // =========================================================================
    // EXTERNAL SAFETY INPUT (EMERGENCY SHUTDOWN VIA D7)
    // =========================================================================

    // External safety input configuration
    // When D7 (uC_IN1) is pulled LOW by the external system, immediately shutdown outputs.
    // D7 is held HIGH by the OPTO output when the safety signal is inactive (normal operation).
    // WARNING: Unlike internal protection, external shutdown is IMMEDIATE (no rate limiting)
    constexpr bool ENABLE_EXTERNAL_SAFETY = true;  // Enable external safety shutdown via D7

    // Signal polarity: true = HIGH triggers shutdown, false = LOW triggers shutdown
    // Set to true for active-high safety signal (HIGH = shutdown, LOW = normal)
    // Set to false for active-low safety signal (LOW = shutdown, HIGH = normal)
    constexpr bool EXTERNAL_SAFETY_ACTIVE_HIGH = false;  // LOW = emergency shutdown (HIGH = OK)
    
    // Rate limiting for voltage changes (per update cycle)
    // Prevents sudden jumps, reduces stress on pump/electrical system
    // At 20Hz update (50ms), this allows 0.05 per cycle = 1.0s for full range
    constexpr float VOLTAGE_LIMIT_RATE_MAX = 0.05f;      // Max change per cycle (normal)
    
    // EMERGENCY rate limiting (bypass normal rate limiting in critical situations)
    // When entering EMERGENCY level, apply immediate reduction without rate limiting
    constexpr float VOLTAGE_LIMIT_RATE_EMERGENCY = 1.0f; // Instant shutdown (no rate limiting)
    
    // =========================================================================
    // TEMPERATURE SENSOR - NTC 10K (HEATSINK MONITORING)
    // =========================================================================

    // Hardware configuration (see schematic):
    //   +5V -- R19(10K/1%) --+-- NTC(GND) ---- GND
    //                        |
    //                        +-- R20(10K/1%) -- ADC (PIN_NTC_TEMP)
    //                        |
    //                        +-- C16(4.7uF) --- GND
    //
    // R20 is high-impedance isolation; ADC reads the divider junction voltage.
    // Since the divider supply (+5V) and ADC reference are the same rail,
    // the reference voltage cancels out: R_NTC = R_PULLUP * adc / (1023 - adc).
    //
    // NOTE: Schematic also shows NTC2 in parallel with NTC1. If both are
    // populated, set NTC_R25 to the effective parallel value (e.g. 5000 for
    // two 10K). Assumption here: only NTC1 populated (10K).
    constexpr float NTC_R_PULLUP   = 10000.0f;  // R19 pull-up to +5V (Ohms)
    constexpr float NTC_R25        = 10000.0f;  // NTC nominal resistance @ 25°C (Ohms)
    constexpr float NTC_BETA       = 3950.0f;   // Beta coefficient (typical 10K NTC)
    constexpr float NTC_T25_KELVIN = 298.15f;   // 25°C reference in Kelvin

    // Temperature reading filter coefficient (EMA)
    // Heatsink thermal mass is large -> slow filter is fine and reduces noise
    constexpr float TEMP_FILTER_ALPHA = 0.10f;  // 0<alpha<=1 (smaller = smoother)

    // =========================================================================
    // PWM CONFIGURATION
    // =========================================================================
    
    // Hardware PWM inversion compensation
    // The driver circuit (BC817 NPN + BC807 PNP) inverts the PWM signal:
    //   Arduino PWM HIGH ? BC817 ON ? BC807 OFF ? Gate LOW ? MOSFET OFF
    //   Arduino PWM LOW  ? BC817 OFF ? BC807 ON ? Gate HIGH ? MOSFET ON
    // Set to true to compensate for this inversion in software
    constexpr bool PWM_INVERTED_BY_HARDWARE = true;
    
    // =========================================================================
    // PWM FREQUENCY CONFIGURATION
    // =========================================================================
    
    // High-frequency PWM configuration for motor control
    // Default Arduino PWM: ~490 Hz (audible whine, EMI issues, poor current sensing)
    // Target: 3.9 kHz (reduced from 31.25 kHz due to heating issues)
    //
    // CRITICAL: Timer 0 prescaler modification affects system timing functions!
    // When ENABLE_HIGH_FREQ_PWM = true:
    // - millis() runs 8x faster (prescaler changed from 64 to 8)
    // - delay() runs 8x faster (prescaler changed from 64 to 8)
    // - delayMicroseconds() is NOT affected (uses different mechanism)
    //
    // Timer configuration for 3.9 kHz PWM:
    // - Timer 0 (D5, D6): Phase-Correct PWM, prescaler 64?8, affects millis()/delay()
    // - Timer 2 (D3, D11): Phase-Correct PWM, prescaler 64?8, independent
    // - Formula: 16MHz / 512 / 8 = 3906.25 Hz ? 3.9 kHz (Phase-Correct counts up and down)
    //
    // ALL timing code must be compensated using TIMER0_PRESCALER_FACTOR
    // This is done automatically using MILLIS_COMPENSATED() macro
    //
    constexpr bool ENABLE_HIGH_FREQ_PWM = true;  // Enable 3.9 kHz PWM
    
    // Timer 0 prescaler compensation factor (64?8 = 8x faster)
    // All millis() comparisons and delay() calls must multiply by this factor
    // Example: delay(1000) becomes delay(1000*8) when Timer 0 prescaler changes
    constexpr uint8_t TIMER0_PRESCALER_FACTOR = 8;
    
    // Timing compensation macro for millis() comparisons
    // Use this macro for all time intervals to ensure accurate timing
    // Example: if (millis() < MILLIS_COMPENSATED(2000)) // 2 seconds actual time
    #define MILLIS_COMPENSATED(ms) ((ms) * Config::TIMER0_PRESCALER_FACTOR)
    
    // =========================================================================
    // PIN ASSIGNMENTS
    // =========================================================================
    
    // Main sensors and outputs
    constexpr uint8_t PIN_NTC_SENSOR   = A4; // Reserved for future NTC sensor (was MAP MPX5700AP on pump variant)
    constexpr uint8_t PIN_PWM_OUT_1    = 6;  // D6 (SSR channel 1) // Modificado para D6 para evitar conflito com Timer 0
    constexpr uint8_t PIN_PWM_OUT_2    = 5;  // D5 (SSR channel 2)
    
    // Status LED NeoPixel
    constexpr uint8_t PIN_STATUS_LED   = 2;  // D2 - NeoPixel RGB LED
    constexpr uint16_t STATUS_LED_COUNT = 1; // Número de LEDs NeoPixel
    constexpr uint8_t LED_BRIGHTNESS   = 200; // Brilho do LED (0-255, 50 = ~20%)
    
    // Current sensors (ACS758LCB-050B)
    constexpr uint8_t PIN_CURRENT_1    = A2; // Current sensor channel 1
    constexpr uint8_t PIN_CURRENT_2    = A3; // Current sensor channel 2
    
    // Extra/future pins (stubs)
    constexpr uint8_t PIN_EXTRA_PWM_1  = 1;  // PD1 (TX - careful with Serial)
    constexpr uint8_t PIN_EXTRA_PWM_2  = 0;  // PD0 (RX - careful with Serial)
    constexpr uint8_t PIN_ANALOG_OUT_1 = 6;  // D6 (0-10V via external circuit) - não tem mais no Hardware, mas mantido como referência
    constexpr uint8_t PIN_ANALOG_OUT_2 = 9;  // D9 (0-10V via external circuit) - não tem mais no Hardware, mas mantido como referência
    constexpr uint8_t PIN_DIG_IN_1     = 7;  // D7 (active low)
    constexpr uint8_t PIN_DIG_IN_2     = 8;  // D8 (active low)
    constexpr uint8_t PIN_AUX_IN_1     = A0; // Extra analog input
    constexpr uint8_t PIN_NTC_TEMP     = A1; // NTC 10K thermistor (heatsink temperature)
    constexpr uint8_t PIN_VCC_SENSE    = A5; // Supply voltage sense (voltage divider)
    
    // =========================================================================
    // PWM INPUT (EXTERNAL SOURCE)
    // =========================================================================
    // External PWM input configuration
    // When a valid PWM signal is detected on PIN_DIG_IN_2 (D8), the system uses
    // the input duty cycle as the target for the outputs (instead of CAN/temperature).
    // When signal is lost/idle, system falls back to CAN-based temperature control.
    //
    // HARDWARE NOTE: on this board R30 (in series with uC_IN1/D7) and R45 (in
    // series with uC_IN2/D8) are NOT populated. So D7 and D8 are connected
    // ONLY to the OPTO outputs — there is no electrical path from the input
    // pins back to the base of T4/T8. The Arduino is the SOLE driver of the
    // gate via uC_PWM1 (D6) and uC_PWM2 (D5). The external PWM at D8 is read
    // by software (pulseIn) and replicated on both PWM outputs at 3.9 kHz.
    constexpr uint8_t PIN_PWM_INPUT = PIN_DIG_IN_2;  // D8 - external PWM input (uC_IN2)

    // Expected PWM frequency range (Hz) - typical external command is ~300 Hz
    constexpr float PWM_INPUT_FREQ_MIN = 200.0f;     // Minimum valid frequency (Hz)
    constexpr float PWM_INPUT_FREQ_MAX = 400.0f;     // Maximum valid frequency (Hz)

    // Signal timeout (milliseconds) - if no valid pulse received in this time, signal is considered lost
    constexpr unsigned long PWM_INPUT_TIMEOUT_MS = 200; // 200ms timeout (~60 periods @ 300Hz)

    // External PWM mode enable flag
    constexpr bool  ENABLE_EXTERNAL_PWM_MODE = true;  // Allow external PWM at D8 to override CAN/temperature control
    
    // =========================================================================
    // TIMING
    // =========================================================================
    
    // Main control loop update interval
    constexpr unsigned long MAIN_LOOP_INTERVAL_MS = 50; // 20Hz
    
    // Status report interval (verbose logging)
    constexpr unsigned long STATUS_REPORT_INTERVAL_MS = 1000; // 1Hz
}
