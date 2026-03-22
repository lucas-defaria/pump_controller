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
    constexpr uint8_t PIN_CAN_CS  = 9;  // D9 - MCP2515 chip select
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
    // CURRENT SENSING - ACS772LCB-050B (UNIDIRECTIONAL MODE)
    // =========================================================================
    
    // ACS772LCB-050B Hall-effect current sensor specifications
    // Configured for UNIDIRECTIONAL operation (0 to +50A only)
    // Measured characteristics:
    //   - 0A input = 0.6V output (confirmed)
    //   - 2.65A input = ~0.706V expected (0.6 + 2.65*0.04)
    // Note: PWM load causes noise - using multi-sample averaging
    constexpr float ACS772_SENSITIVITY = 0.06f;        // V/A (40mV/A for 050B)
    constexpr float ACS772_ZERO_CURRENT_V = 0.6f;      // Volts (measured: 0.6V @ 0A - confirmed)
    constexpr float ACS772_MAX_CURRENT = 50.0f;         // Amperes (sensor absolute max)
    
    // ADC configuration
    constexpr float ADC_REFERENCE_VOLTAGE = 4.9f;       // Volts (Arduino Uno default)
    
    // Multi-sample averaging to filter PWM noise
    // PWM at 977Hz creates ~1.02ms period
    // Taking 10 samples with small delays helps average out PWM cycles
    constexpr uint8_t CURRENT_ADC_SAMPLES = 10;         // Number of ADC samples to average
    constexpr uint8_t CURRENT_ADC_DELAY_US = 50;        // Delay between samples (microseconds)
    
    // Current reading filter coefficient (EMA)
    // Higher alpha = faster response, more noise
    // Lower alpha = slower response, smoother
    // Reduced alpha due to PWM interference
    constexpr float CURRENT_FILTER_ALPHA = 0.15f;       // 0<alpha<=1 (reduced for PWM)
    
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
    // ACS772LCB-050B saturates at ~50A - readings near this indicate:
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
    // When D7 receives HIGH signal, immediately shutdown outputs (same as EMERGENCY protection)
    // This allows external systems (ECU, safety relay, etc.) to force pump shutdown
    // WARNING: Unlike internal protection, external shutdown is IMMEDIATE (no rate limiting)
    constexpr bool ENABLE_EXTERNAL_SAFETY = true;  // Enable external safety shutdown via D7
    
    // Signal polarity: true = HIGH triggers shutdown, false = LOW triggers shutdown
    // Set to true for active-high safety signal (HIGH = shutdown, LOW = normal)
    // Set to false for active-low safety signal (LOW = shutdown, HIGH = normal)
    constexpr bool EXTERNAL_SAFETY_ACTIVE_HIGH = true;  // HIGH = emergency shutdown
    
    // Rate limiting for voltage changes (per update cycle)
    // Prevents sudden jumps, reduces stress on pump/electrical system
    // At 20Hz update (50ms), this allows 0.05 per cycle = 1.0s for full range
    constexpr float VOLTAGE_LIMIT_RATE_MAX = 0.05f;      // Max change per cycle (normal)
    
    // EMERGENCY rate limiting (bypass normal rate limiting in critical situations)
    // When entering EMERGENCY level, apply immediate reduction without rate limiting
    constexpr float VOLTAGE_LIMIT_RATE_EMERGENCY = 1.0f; // Instant shutdown (no rate limiting)
    
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
    
    // Main outputs (both on Timer 0 - no SPI conflict)
    constexpr uint8_t PIN_PWM_OUT_1    = 6;  // D6 - OC0A (Timer 0)
    constexpr uint8_t PIN_PWM_OUT_2    = 5;  // D5 - OC0B (Timer 0)
    
    // Status LED NeoPixel
    // DEBUG: Set to false to disable NeoPixel updates (test SPI/Timer2 conflict isolation)
    constexpr bool ENABLE_STATUS_LED = false;
    constexpr uint8_t PIN_STATUS_LED   = 2;  // D2 - NeoPixel RGB LED
    constexpr uint16_t STATUS_LED_COUNT = 1; // Número de LEDs NeoPixel
    constexpr uint8_t LED_BRIGHTNESS   = 200; // Brilho do LED (0-255)

    // NTC temperature sensor (future - analog input, curve TBD)
    constexpr uint8_t PIN_NTC_SENSOR   = A4; // Reserved for future NTC sensor
    
    // Current sensors (ACS772LCB-100U)
    constexpr uint8_t PIN_CURRENT_1    = A2; // Current sensor channel 1
    constexpr uint8_t PIN_CURRENT_2    = A3; // Current sensor channel 2
    
    // Extra/future pins (stubs)
    constexpr uint8_t PIN_EXTRA_PWM_1  = 1;  // PD1 (TX - careful with Serial)
    constexpr uint8_t PIN_EXTRA_PWM_2  = 0;  // PD0 (RX - careful with Serial)
    constexpr uint8_t PIN_ANALOG_OUT_1 = 6;  // D6 (0-10V via external circuit)
    constexpr uint8_t PIN_ANALOG_OUT_2 = 9;  // D9 (0-10V via external circuit)
    constexpr uint8_t PIN_DIG_IN_1     = 7;  // D7 (active low)
    constexpr uint8_t PIN_DIG_IN_2     = 8;  // D8 (active low)
    constexpr uint8_t PIN_AUX_IN_1     = A0; // Extra analog input
    constexpr uint8_t PIN_AUX_IN_2     = A1; // Extra analog input
    constexpr uint8_t PIN_VCC_SENSE    = A5; // Supply voltage sense (voltage divider)
    
    // =========================================================================
    // PWM INPUT (SLAVE MODE)
    // =========================================================================
    
    // External PWM input configuration for slave mode
    // When valid PWM signal is detected on PIN_DIG_IN_1 (D7), system enters slave mode
    // and replicates the input PWM on both outputs (ignoring temperature control)
    // When signal is lost/idle, system returns to normal temperature-based control
    constexpr uint8_t PIN_PWM_INPUT = PIN_DIG_IN_1;  // D7 - PWM input for slave mode
    
    // Expected PWM frequency range (Hz) - typically 25Hz �10Hz tolerance
    constexpr float PWM_INPUT_FREQ_MIN = 15.0f;      // Minimum valid frequency (Hz)
    constexpr float PWM_INPUT_FREQ_MAX = 35.0f;      // Maximum valid frequency (Hz)
    
    // Signal timeout (milliseconds) - if no valid pulse received in this time, signal is considered lost
    constexpr unsigned long PWM_INPUT_TIMEOUT_MS = 200; // 200ms timeout (5x period @ 25Hz)
    
    // Slave mode enable flag
    constexpr bool  ENABLE_PWM_SLAVE_MODE = true;     // Enable external PWM slave mode
    
    // =========================================================================
    // TIMING
    // =========================================================================
    
    // Main control loop update interval
    constexpr unsigned long MAIN_LOOP_INTERVAL_MS = 50; // 20Hz
    
    // Status report interval (verbose logging)
    constexpr unsigned long STATUS_REPORT_INTERVAL_MS = 1000; // 1Hz
}
