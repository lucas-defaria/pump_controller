/* ----------------------------------------------------------------------------
   PumpControl.ino - Advanced fuel pump control with current protection
   
   Features:
   - MAP sensor-based pressure control (MPX5700ASX on A4)
   - Dual ACS772LCB-100U current sensors (A2, A3)
   - Multi-level progressive current protection (never fully shuts down)
   - Two PWM outputs (D3, D5) for SSR control
   - NeoPixel RGB LED indicating current level and protection state
   - Serial logging of all parameters
   
   LED Status Indication:
   - NORMAL (0-25A):           Green solid
   - WARNING-CRITICAL (25-40A): Gradual transition green?yellow?orange?red
   - FAULT (>40A):             Blinking red (1Hz)
   - EMERGENCY (>45A):         Fast blinking red (5Hz)
   
   Protection Strategy:
   - NORMAL (0-25A):     Full voltage available
   - WARNING (25-30A):   10% voltage reduction
   - HIGH (30-35A):      25% voltage reduction
   - CRITICAL (35-40A):  40% voltage reduction
   - FAULT (>40A):       50% voltage reduction (minimum safe level)
   
   CRITICAL SAFETY REQUIREMENT:
   Pump cannot be fully shut down - would damage engine under load.
   Protection system progressively reduces power to limit current.
   
   Configuration in Config.h
---------------------------------------------------------------------------- */
#include <Arduino.h>
#include "Config.h"
#include "MapSensor.h"
#include "PowerOutputs.h"
#include "CurrentSensor.h"
#include "PowerProtection.h"
#include "VoltageSensor.h"
#include "VoltageProtection.h"
#include "CanInterface.h"
#include "StatusLed.h"
#include "PwmInput.h"

// ============================================================================
// Global instances
// ============================================================================

MapSensor      g_map(Config::PIN_MAP_SENSOR);
PowerOutputs   g_power(Config::PIN_PWM_OUT_1, Config::PIN_PWM_OUT_2);
CurrentSensor  g_curr1(Config::PIN_CURRENT_1);
CurrentSensor  g_curr2(Config::PIN_CURRENT_2);
PowerProtection g_protection(g_curr1, g_curr2);
VoltageSensor  g_voltage(Config::PIN_VCC_SENSE);
VoltageProtection g_voltageProtection(g_voltage);
CanInterface   g_can;  // stub for future CAN bus integration
StatusLed      g_statusLed(Config::PIN_STATUS_LED, Config::STATUS_LED_COUNT);
PwmInput       g_pwmInput(Config::PIN_PWM_INPUT);  // External PWM input for slave mode

unsigned long g_lastUpdateMs = 0;
unsigned long g_lastStatusMs = 0;

// ============================================================================
// Pressure to output percentage conversion
// ============================================================================

// Converts pressure (bar) to target output percentage (0.0 to 1.0)
// Linear interpolation between low and high setpoints
// Low pressure (?0.2bar): 70% of supply voltage
// High pressure (?0.4bar): 100% of supply voltage
static float pressureToTargetPercent(float bar) {
    const float pLow  = Config::MAP_BAR_LOW_SETPOINT;
    const float pHigh = Config::MAP_BAR_HIGH_SETPOINT;
    const float percentLow  = Config::OUTPUT_PERCENT_MIN;  // 0.70 (70%)
    const float percentHigh = Config::OUTPUT_PERCENT_MAX;  // 1.00 (100%)

    if (bar <= pLow)  return percentLow;
    if (bar >= pHigh) return percentHigh;

    float spanP = pHigh - pLow;
    float ratio = (bar - pLow) / spanP; // 0..1
    return percentLow + ratio * (percentHigh - percentLow);
}

// ============================================================================
// Setup
// ============================================================================

void setup() {
    Serial.begin(115200);
    // COMPENSATED: millis() runs 64x faster due to Timer 0 prescaler change for PWM
    // 2000ms actual time = 2000 * 64 ticks with modified Timer 0
    while(!Serial && millis() < MILLIS_COMPENSATED(2000)) { 
        // Wait for serial port (optional, for debugging)
    }

    Serial.println(F("========================================"));
    Serial.println(F("  PumpControl with Current Protection  "));
    Serial.println(F("========================================"));
    Serial.println();

    // Initialize all subsystems
    // CRITICAL: Power outputs initialized FIRST to ensure motor starts OFF
    Serial.println(F("Initializing power outputs (motor OFF)..."));
    g_power.begin();  // This sets motor to OFF state with safety delays
    
    Serial.println(F("Initializing sensors..."));
    g_map.begin();
    g_curr1.begin();
    g_curr2.begin();
    g_protection.begin();
    g_voltage.begin();
    g_voltageProtection.begin();
    g_can.begin(); // stub
    g_pwmInput.begin(); // External PWM input for slave mode - configures PIN_DIG_IN_1 as INPUT (no pullup)
    
    // Enable PWM debug for first 10 seconds (for troubleshooting)
    // Comment out after confirming PWM detection works
    g_pwmInput.setDebug(true);
    
    Serial.println(F("Initializing status LED..."));
    g_statusLed.begin();

    // Configure digital inputs (active low)
    // NOTE: PIN_DIG_IN_1 (D7) is used for PWM input and configured by g_pwmInput.begin()
    // Do NOT reconfigure it here as INPUT_PULLUP or it will interfere with PWM signal
    // pinMode(Config::PIN_DIG_IN_1, INPUT_PULLUP);  // REMOVED - conflicts with PWM input
    pinMode(Config::PIN_DIG_IN_2, INPUT_PULLUP);     // D8 for external safety (active low)

    // Print configuration summary
    Serial.println(F("Configuration:"));
    Serial.print(F("  Pressure: ")); 
    Serial.print(Config::MAP_BAR_LOW_SETPOINT, 2);
    Serial.print(F("-"));
    Serial.print(Config::MAP_BAR_HIGH_SETPOINT, 2);
    Serial.println(F(" bar"));
    
    Serial.print(F("  Voltage:  "));
    Serial.print(Config::OUTPUT_PERCENT_MIN * 100.0f, 0);
    Serial.print(F("-"));
    Serial.print(Config::OUTPUT_PERCENT_MAX * 100.0f, 0);
    Serial.println(F("% of Vsupply"));
    
    Serial.println();
    Serial.println(F("Protection thresholds (A):"));
    Serial.print(F("  WARNING:  ")); Serial.println(Config::CURRENT_THRESHOLD_WARNING, 1);
    Serial.print(F("  HIGH:     ")); Serial.println(Config::CURRENT_THRESHOLD_HIGH, 1);
    Serial.print(F("  CRITICAL: ")); Serial.println(Config::CURRENT_THRESHOLD_CRITICAL, 1);
    Serial.print(F("  FAULT:    ")); Serial.println(Config::CURRENT_THRESHOLD_FAULT, 1);
    Serial.println();
    
    Serial.println(F("System ready"));
    Serial.println(F("========================================"));
    Serial.println();
    
    // Additional safety: Keep motor OFF for 2 seconds after full initialization
    // Allows all sensors to stabilize before motor operation begins
    Serial.println(F("Safety delay: Motor OFF for 2 seconds..."));
    g_power.setDuty(0.0f);  // Ensure motor is OFF
    // Using delayMicroseconds because it's NOT affected by Timer 0 prescaler change
    // delay() would require dividing by 64, not multiplying
    delayMicroseconds(2000000UL);  // 2 seconds = 2,000,000 microseconds
    Serial.println(F("Starting normal operation"));
    Serial.println();
}

// ============================================================================
// Main loop
// ============================================================================

void loop() {
    unsigned long now = millis();
    
    // Main control loop - runs at MAIN_LOOP_INTERVAL_MS (20Hz default)
    // Safe millis() rollover handling: subtraction is always safe for unsigned types
    // COMPENSATED: millis() runs 64x faster, so interval must be 64x larger
    if ((unsigned long)(now - g_lastUpdateMs) >= MILLIS_COMPENSATED(Config::MAIN_LOOP_INTERVAL_MS)) {
        g_lastUpdateMs = now;

        // ====================================================================
        // 1. Update PWM input reading (uses pulseIn, may block ~40-100ms)
        // ====================================================================
        if (Config::ENABLE_PWM_SLAVE_MODE) {
            g_pwmInput.update();
        }

        // ====================================================================
        // 2. Check external safety input (D8) - HIGHEST PRIORITY
        // ====================================================================
        bool externalSafetyActive = false;
        if (Config::ENABLE_EXTERNAL_SAFETY) {
            int safetyInput = digitalRead(Config::PIN_DIG_IN_2);  // D8
            if (Config::EXTERNAL_SAFETY_ACTIVE_HIGH) {
                externalSafetyActive = (safetyInput == HIGH);  // HIGH = shutdown
            } else {
                externalSafetyActive = (safetyInput == LOW);   // LOW = shutdown
            }
            
            // If external safety triggered, immediately shutdown and skip normal control
            if (externalSafetyActive) {
                g_power.setDuty(0.0f);  // IMMEDIATE shutdown (no rate limiting)
                g_statusLed.updateExternalSafetyBlink();  // Blue blinking LED
                Serial.println(F("*** EXTERNAL SAFETY ACTIVE - OUTPUT FORCED OFF ***"));
                // Skip rest of control loop - safety has priority
                return;
            }
        }
        
        // ====================================================================
        // 3. Check for PWM slave mode (SECOND PRIORITY)
        // ====================================================================
        bool slaveMode = (Config::ENABLE_PWM_SLAVE_MODE && g_pwmInput.isSignalValid());
        
        if (slaveMode) {
            // ================================================================
            // SLAVE MODE: Replicate input PWM on outputs
            // ================================================================
            // Still monitor current and voltage for protection
            float inputDuty = g_pwmInput.getDutyCycle();
            
            // Read sensors for protection
            float supplyVoltage = g_voltage.readVoltage();
            float voltageLimit = g_protection.update();
            
            // Apply input duty cycle with protection limits
            // CRITICAL: Use setOutputPercent() to respect voltage limit from protection
            g_power.setSupplyVoltage(supplyVoltage);
            g_power.setVoltageLimit(voltageLimit);
            g_power.setOutputPercent(inputDuty);  // This applies voltageLimit internally
            
            // Read current sensors and update status LED
            float current1 = g_curr1.readCurrentA();
            float current2 = g_curr2.readCurrentA();
            float maxCurrent = max(current1, current2);
            
            PowerProtection::ProtectionLevel protLevel = g_protection.getLevel();
            bool inFault = (protLevel == PowerProtection::ProtectionLevel::FAULT);
            bool inEmergency = (protLevel == PowerProtection::ProtectionLevel::EMERGENCY);
            g_statusLed.updateFromCurrent(maxCurrent, inFault, inEmergency);
            
            // Compact status output for slave mode
            Serial.print(F("*** SLAVE MODE *** | PWM In:"));
            Serial.print(inputDuty * 100.0f, 1);
            Serial.print(F("% @ "));
            Serial.print(g_pwmInput.getFrequency(), 1);
            Serial.print(F("Hz | Vs:"));
            Serial.print(supplyVoltage, 1);
            Serial.print(F("V | Vo:"));
            Serial.print(g_power.getActualOutputVoltage(), 1);
            Serial.print(F("V | I1:"));
            Serial.print(current1, 1);
            Serial.print(F("A | I2:"));
            Serial.print(current2, 1);
            Serial.print(F("A | Lim:"));
            Serial.print(voltageLimit * 100.0f, 0);
            Serial.print(F("% | "));
            Serial.println(g_protection.getLevelString());
            
        } else {
            // ================================================================
            // NORMAL MAP-BASED CONTROL MODE (4)
            // ================================================================
            
            // Read pressure sensor
            float pressureBar = g_map.readPressureBar();
            
            // Read supply voltage (used for all percentage calculations)
            float supplyVoltage = g_voltage.readVoltage();
            VoltageProtection::ProtectionLevel voltageLevel = g_voltageProtection.update();
            
            // Calculate target output as percentage of supply voltage
            float targetPercent = pressureToTargetPercent(pressureBar);
            float targetVoltage = targetPercent * supplyVoltage;  // Convert to actual voltage
            
            // Update current protection system (reads current sensors)
            float voltageLimit = g_protection.update();
            
            // Read current sensors and update status LED
            float current1 = g_curr1.readCurrentA();
            float current2 = g_curr2.readCurrentA();
            float maxCurrent = max(current1, current2);
            
            // Update LED based on current level and protection state
            PowerProtection::ProtectionLevel protLevel = g_protection.getLevel();
            bool inFault = (protLevel == PowerProtection::ProtectionLevel::FAULT);
            bool inEmergency = (protLevel == PowerProtection::ProtectionLevel::EMERGENCY);
            g_statusLed.updateFromCurrent(maxCurrent, inFault, inEmergency);
            
            // Apply voltage limit and set power output
            g_power.setSupplyVoltage(supplyVoltage);  // Update measured supply voltage
            g_power.setVoltageLimit(voltageLimit);
            g_power.setOutputPercent(targetPercent);  // Use percentage-based method
            
            // Read digital inputs (active low) - for logging only
            // Note: PIN_DIG_IN_2 (D8) is now used for external safety (checked at start of loop)
            bool dig1Active = (digitalRead(Config::PIN_DIG_IN_1) == LOW);
            bool dig2Active = (digitalRead(Config::PIN_DIG_IN_2) == LOW);
            
            // Compact status output (every cycle)
            Serial.print(F("P:"));
            Serial.print(pressureBar, 2);
            Serial.print(F("bar | Vs:"));
            Serial.print(supplyVoltage, 1);
            Serial.print(F("V | T%:"));
            Serial.print(targetPercent * 100.0f, 0);
            Serial.print(F("% | Vo:"));
            Serial.print(g_power.getActualOutputVoltage(), 1);
            Serial.print(F("V | I1:"));
            Serial.print(current1, 1);
            Serial.print(F("A | I2:"));
            Serial.print(current2, 1);
            Serial.print(F("A | Lim:"));
            Serial.print(voltageLimit * 100.0f, 0);
            Serial.print(F("% | "));
            Serial.println(g_protection.getLevelString());
        }
    }
    
    // ========================================================================
    // Detailed status report - runs at STATUS_REPORT_INTERVAL_MS (1Hz default)
    // ========================================================================
    // Safe millis() rollover handling: subtraction is always safe for unsigned types
    // COMPENSATED: millis() runs 64x faster, so interval must be 64x larger
    if ((unsigned long)(now - g_lastStatusMs) >= MILLIS_COMPENSATED(Config::STATUS_REPORT_INTERVAL_MS)) {
        g_lastStatusMs = now;
        
        printDetailedStatus();
    }
    
    // ========================================================================
    // CAN bus polling (stub for future implementation)
    // ========================================================================
    g_can.poll();
}

// ============================================================================
// Status reporting
// ============================================================================

void printDetailedStatus() {
    Serial.println(F("----------------------------------------"));
    Serial.println(F("STATUS REPORT"));
    Serial.println(F("----------------------------------------"));
    
    // PWM Slave Mode Status
    if (Config::ENABLE_PWM_SLAVE_MODE) {
        Serial.print(F("PWM Slave Mode:  "));
        if (g_pwmInput.isSignalValid()) {
            Serial.println(F("*** ACTIVE ***"));
            Serial.print(F("  Input Duty:    "));
            Serial.print(g_pwmInput.getDutyCycle() * 100.0f, 1);
            Serial.println(F(" %"));
            Serial.print(F("  Input Freq:    "));
            Serial.print(g_pwmInput.getFrequency(), 2);
            Serial.println(F(" Hz"));
            Serial.print(F("  Period:        "));
            Serial.print(g_pwmInput.getPeriodUs() / 1000.0f, 2);
            Serial.println(F(" ms"));
            Serial.print(F("  High Time:     "));
            Serial.print(g_pwmInput.getHighTimeUs() / 1000.0f, 2);
            Serial.println(F(" ms"));
        } else {
            Serial.println(F("Inactive (no signal) - Normal MAP mode"));
            // Debug information
            Serial.print(F("  Pin D7 State:  "));
            Serial.println(g_pwmInput.getCurrentState() == HIGH ? "HIGH" : "LOW");
            Serial.print(F("  Pulses Det:    "));
            Serial.println(g_pwmInput.getPulsesDetected());
            Serial.print(F("  Last Freq:     "));
            Serial.print(g_pwmInput.getFrequency(), 2);
            Serial.println(F(" Hz"));
            Serial.print(F("  Time Since:    "));
            Serial.print(g_pwmInput.getTimeSinceLastPulseMs());
            Serial.println(F(" ms"));
        }
        Serial.println();
    }
    
    // Pressure
    float pressure = g_map.readPressureBar();
    Serial.print(F("Pressure:        ")); 
    Serial.print(pressure, 3);
    Serial.println(F(" bar"));
    
    // Current readings (filtered)
    float i1 = g_curr1.readCurrentA();
    float i2 = g_curr2.readCurrentA();
    Serial.print(F("Current Ch1:     ")); 
    Serial.print(i1, 2);
    Serial.println(F(" A"));
    Serial.print(F("Current Ch2:     ")); 
    Serial.print(i2, 2);
    Serial.println(F(" A"));
    Serial.print(F("Max Current:     ")); 
    Serial.print(max(i1, i2), 2);
    Serial.println(F(" A"));
    
    // Diagnostic: raw voltage readings
    Serial.print(F("Ch1 Voltage:     ")); 
    Serial.print(g_curr1.getFilteredVoltage(), 3);
    Serial.print(F(" V (filtered), "));
    Serial.print(g_curr1.readVoltageRaw(), 3);
    Serial.println(F(" V (raw avg)"));
    Serial.print(F("Ch2 Voltage:     ")); 
    Serial.print(g_curr2.getFilteredVoltage(), 3);
    Serial.print(F(" V (filtered), "));
    Serial.print(g_curr2.readVoltageRaw(), 3);
    Serial.println(F(" V (raw avg)"));
    
    // Supply voltage status
    float supplyV = g_voltage.readVoltage();
    Serial.print(F("Supply Voltage:  ")); 
    Serial.print(supplyV, 2);
    Serial.println(F(" V"));
    Serial.print(F("Voltage Status:  ")); 
    Serial.println(g_voltageProtection.getLevelString());
    Serial.print(F("Sensor Valid:    ")); 
    Serial.println(g_voltageProtection.isSensorOk() ? "YES" : "NO");
    Serial.print(F("V Fault Count:   ")); 
    Serial.println(g_voltageProtection.getFaultCount());
    Serial.println();
    
    // Current protection status
    Serial.print(F("Protection:      ")); 
    Serial.println(g_protection.getLevelString());
    Serial.print(F("Voltage Limit:   ")); 
    Serial.print(g_protection.getVoltageLimit() * 100.0f, 1);
    Serial.println(F(" %"));
    Serial.print(F("Fault Count:     ")); 
    Serial.println(g_protection.getFaultCount());
    
    // Output status
    float targetPercent = pressureToTargetPercent(pressure);
    float targetV = targetPercent * supplyV;
    Serial.print(F("Target Percent:  ")); 
    Serial.print(targetPercent * 100.0f, 1);
    Serial.println(F(" %"));
    Serial.print(F("Target Voltage:  ")); 
    Serial.print(targetV, 2);
    Serial.println(F(" V"));
    Serial.print(F("Actual Voltage:  ")); 
    Serial.print(g_power.getActualOutputVoltage(), 2);
    Serial.println(F(" V"));
    Serial.print(F("PWM Duty:        ")); 
    Serial.print(g_power.getCurrentDuty() * 100.0f, 1);
    Serial.println(F(" %"));
    
    // External safety status
    if (Config::ENABLE_EXTERNAL_SAFETY) {
        int safetyInput = digitalRead(Config::PIN_DIG_IN_2);
        bool safetyActive = Config::EXTERNAL_SAFETY_ACTIVE_HIGH ? 
                           (safetyInput == HIGH) : (safetyInput == LOW);
        Serial.print(F("External Safety: ")); 
        Serial.println(safetyActive ? "*** ACTIVE (SHUTDOWN) ***" : "OK");
    }
    
    // Digital inputs
    // Note: D7 (PIN_DIG_IN_1) is used for PWM input when ENABLE_PWM_SLAVE_MODE is true
    if (Config::ENABLE_PWM_SLAVE_MODE) {
        Serial.print(F("Digital In 1:    ")); 
        Serial.println(F("(used for PWM input)"));
    } else {
        bool d1 = (digitalRead(Config::PIN_DIG_IN_1) == LOW);
        Serial.print(F("Digital In 1:    ")); 
        Serial.println(d1 ? "ACTIVE" : "inactive");
    }
    
    bool d2 = (digitalRead(Config::PIN_DIG_IN_2) == LOW);
    Serial.print(F("Digital In 2:    ")); 
    Serial.println(d2 ? "ACTIVE" : "inactive");
    
    // Runtime
    // COMPENSATED: millis() runs 64x faster, divide by prescaler factor for real time
    Serial.print(F("Uptime:          ")); 
    Serial.print(millis() / (1000UL * Config::TIMER0_PRESCALER_FACTOR));
    Serial.println(F(" s"));
    
    Serial.println(F("----------------------------------------"));
    Serial.println();
}
