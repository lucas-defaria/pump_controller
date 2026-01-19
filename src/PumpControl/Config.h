#pragma once

// -----------------------------------------------------------------------------
// Config.h - Compile-time configuration constants for Pump Control project
// -----------------------------------------------------------------------------
// Adjust setpoints, thresholds, and operational parameters here.
// Nothing is user-configurable at runtime.
// -----------------------------------------------------------------------------

namespace Config {
    // =========================================================================
    // MAP SENSOR & PRESSURE CONTROL
    // =========================================================================
    
    // MPX5700AP - Absolute pressure sensor (15-700 kPa absolute)
    // Sensor reads absolute pressure, converted to gauge (relative to atmosphere)
    // Gauge pressure = Absolute pressure - Atmospheric pressure
    // Verified: 833mV @ atmospheric conditions = 1.013 bar absolute = 0 bar gauge
    
    // Atmospheric pressure at sea level (for absolute?gauge conversion)
    constexpr float ATMOSPHERIC_PRESSURE_BAR = 1.013f; // bar (101.3 kPa)
    
    // Pressure setpoints (bar gauge - relative to atmospheric)
    // Negative values = vacuum (intake manifold below atmospheric)
    // Positive values = boost (turbo pressure above atmospheric)
    // Pressure (bar gauge) where output should be at OUTPUT_VOLTAGE_MIN
    constexpr float MAP_BAR_LOW_SETPOINT  = -0.3f; // bar gauge (high vacuum)
    // Pressure (bar gauge) where output should be at OUTPUT_VOLTAGE_MAX
    constexpr float MAP_BAR_HIGH_SETPOINT = 1.2f;  // bar gauge (moderate boost)

    // Target voltages corresponding to pressure setpoints
    constexpr float OUTPUT_VOLTAGE_MIN = 4.0f;//9.0f;   // Volts
    constexpr float OUTPUT_VOLTAGE_MAX = 12.0f;//12.0f;  // Volts

    // Supply voltage to power stage (assumed)
    constexpr float SUPPLY_VOLTAGE = 12.0f; // Volts

    // MAP sensor filter coefficient (EMA)
    constexpr float MAP_FILTER_ALPHA   = 0.15f; // 0<alpha<=1 (smaller = smoother)

    // =========================================================================
    // CURRENT SENSING - ACS772LCB-050 (UNIDIRECTIONAL MODE)
    // =========================================================================
    
    // ACS772LCB-050 Hall-effect current sensor specifications
    // Configured for UNIDIRECTIONAL operation (0 to +50A only)
    // Physical chip marked as 050B, but behaving as unidirectional variant
    constexpr float ACS772_SENSITIVITY = 0.040f;        // V/A (40mV/A)
    constexpr float ACS772_ZERO_CURRENT_V = 0.6f;      // Volts (measured: 0.6V @ 0A - confirmed)
    constexpr float ACS772_MAX_CURRENT = 50.0f;         // Amperes (sensor absolute max)
    
    // Note: Standard bidirectional 050B should have zero at 2.5V
    // This configuration uses measured zero point for unidirectional operation
    
    // ADC configuration
    constexpr float ADC_REFERENCE_VOLTAGE = 5.0f;       // Volts (Arduino Uno default)
    
    // Current reading filter coefficient (EMA)
    // Higher alpha = faster response, more noise
    // Lower alpha = slower response, smoother
    constexpr float CURRENT_FILTER_ALPHA = 0.25f;       // 0<alpha<=1
    
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
    constexpr float VOLTAGE_FILTER_ALPHA = 0.20f;        // 0<alpha<=1 (smoother than current)
    
    // Percentage-based voltage protection (adaptive to actual supply voltage)
    // Instead of fixed thresholds (9V, 12V), use percentage drop from measured voltage
    // This adapts automatically to automotive voltage variations (8-14.5V)
    constexpr float VOLTAGE_DROP_WARNING_PERCENT = 0.30f;   // 15% drop triggers warning
    constexpr float VOLTAGE_DROP_CRITICAL_PERCENT = 0.50f;  // 30% drop triggers critical protection
    
    // Hysteresis for voltage protection (Volts)
    constexpr float VOLTAGE_HYSTERESIS = 0.5f;               // 0.5V band to prevent oscillation
    
    // Minimum valid supply voltage (below this = sensor fault)
    constexpr float VOLTAGE_MINIMUM_VALID = 7.0f;            // Volts (below normal automotive range)
    
    // =========================================================================
    // CURRENT PROTECTION SYSTEM
    // =========================================================================
    
    // Protection threshold levels (Amperes)
    // Normal operation: 0-25A (typical fuel pump at full load)
    constexpr float CURRENT_THRESHOLD_WARNING  = 25.0f;  // WARNING zone starts
    constexpr float CURRENT_THRESHOLD_HIGH     = 30.0f;  // HIGH zone starts
    constexpr float CURRENT_THRESHOLD_CRITICAL = 35.0f;  // CRITICAL zone starts
    constexpr float CURRENT_THRESHOLD_FAULT    = 40.0f;  // FAULT zone starts
    
    // Hysteresis band (Amperes)
    // Prevents chattering/oscillation between protection levels
    // Level changes require crossing threshold � hysteresis
    constexpr float CURRENT_HYSTERESIS = 2.0f;           // Amperes
    
    // Voltage limiting for protection levels
    // NORMAL:   100% (1.00) - no limiting
    // WARNING:   90% (0.90) - 10% reduction
    // HIGH:      75% (0.75) - 25% reduction
    // CRITICAL:  60% (0.60) - 40% reduction
    // FAULT:     50% (0.50) - 50% reduction (minimum safe voltage)
    
    // Minimum voltage factor during FAULT (prevents complete shutdown)
    // Should ensure at least 6V output @ 12V system
    constexpr float FAULT_MINIMUM_VOLTAGE_FACTOR = 0.50f; // 50% = 6V @ 12V system
    
    // Rate limiting for voltage changes (per update cycle)
    // Prevents sudden jumps, reduces stress on pump/electrical system
    // At 20Hz update (50ms), this allows 0.05 per cycle = 1.0s for full range
    constexpr float VOLTAGE_LIMIT_RATE_MAX = 0.05f;      // Max change per cycle
    
    // =========================================================================
    // PIN ASSIGNMENTS
    // =========================================================================
    
    // Main sensors and outputs
    constexpr uint8_t PIN_MAP_SENSOR   = A4; // MPX5700AP pressure sensor (absolute)
    constexpr uint8_t PIN_PWM_OUT_1    = 3;  // D3 (SSR channel 1)
    constexpr uint8_t PIN_PWM_OUT_2    = 5;  // D5 (SSR channel 2)

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
    // TIMING
    // =========================================================================
    
    // Main control loop update interval
    constexpr unsigned long MAIN_LOOP_INTERVAL_MS = 50; // 20Hz
    
    // Status report interval (verbose logging)
    constexpr unsigned long STATUS_REPORT_INTERVAL_MS = 1000; // 1Hz
}
