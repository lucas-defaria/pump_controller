/* ----------------------------------------------------------------------------
   PumpControl.ino - Advanced fuel pump control with current protection
   
   Features:
   - MAP sensor-based pressure control (MPX5700ASX on A4)
   - Dual ACS772LCB-100U current sensors (A2, A3)
   - Multi-level progressive current protection (never fully shuts down)
   - Two PWM outputs (D3, D5) for SSR control
   - Serial logging of all parameters
   
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
#include "CanInterface.h"

// ============================================================================
// Global instances
// ============================================================================

MapSensor      g_map(Config::PIN_MAP_SENSOR);
PowerOutputs   g_power(Config::PIN_PWM_OUT_1, Config::PIN_PWM_OUT_2);
CurrentSensor  g_curr1(Config::PIN_CURRENT_1);
CurrentSensor  g_curr2(Config::PIN_CURRENT_2);
PowerProtection g_protection(g_curr1, g_curr2);
CanInterface   g_can;  // stub for future CAN bus integration

unsigned long g_lastUpdateMs = 0;
unsigned long g_lastStatusMs = 0;

// ============================================================================
// Pressure to voltage conversion
// ============================================================================

// Converts pressure (bar) to target voltage (Volts) based on setpoints
// Linear interpolation between low and high setpoints
static float pressureToTargetVoltage(float bar) {
    const float pLow  = Config::MAP_BAR_LOW_SETPOINT;
    const float pHigh = Config::MAP_BAR_HIGH_SETPOINT;
    const float vLow  = Config::OUTPUT_VOLTAGE_MIN;
    const float vHigh = Config::OUTPUT_VOLTAGE_MAX;

    if (bar <= pLow)  return vLow;
    if (bar >= pHigh) return vHigh;

    float spanP = pHigh - pLow;
    float ratio = (bar - pLow) / spanP; // 0..1
    return vLow + ratio * (vHigh - vLow);
}

// ============================================================================
// Setup
// ============================================================================

void setup() {
    Serial.begin(115200);
    while(!Serial && millis() < 2000) { 
        // Wait for serial port (optional, for debugging)
    }

    Serial.println(F("========================================"));
    Serial.println(F("  PumpControl with Current Protection  "));
    Serial.println(F("========================================"));
    Serial.println();

    // Initialize all subsystems
    g_map.begin();
    g_power.begin();
    g_curr1.begin();
    g_curr2.begin();
    g_protection.begin();
    g_can.begin(); // stub

    // Configure digital inputs (active low)
    pinMode(Config::PIN_DIG_IN_1, INPUT_PULLUP);
    pinMode(Config::PIN_DIG_IN_2, INPUT_PULLUP);

    // Print configuration summary
    Serial.println(F("Configuration:"));
    Serial.print(F("  Pressure: ")); 
    Serial.print(Config::MAP_BAR_LOW_SETPOINT, 2);
    Serial.print(F("-"));
    Serial.print(Config::MAP_BAR_HIGH_SETPOINT, 2);
    Serial.println(F(" bar"));
    
    Serial.print(F("  Voltage:  "));
    Serial.print(Config::OUTPUT_VOLTAGE_MIN, 1);
    Serial.print(F("-"));
    Serial.print(Config::OUTPUT_VOLTAGE_MAX, 1);
    Serial.println(F(" V"));
    
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
}

// ============================================================================
// Main loop
// ============================================================================

void loop() {
    unsigned long now = millis();
    
    // Main control loop - runs at MAIN_LOOP_INTERVAL_MS (20Hz default)
    if (now - g_lastUpdateMs >= Config::MAIN_LOOP_INTERVAL_MS) {
        g_lastUpdateMs = now;

        // ====================================================================
        // 1. Read pressure sensor
        // ====================================================================
        float pressureBar = g_map.readPressureBar();
        
        // ====================================================================
        // 2. Calculate target voltage based on pressure
        // ====================================================================
        float targetVoltage = pressureToTargetVoltage(pressureBar);
        
        // ====================================================================
        // 3. Update protection system (reads current sensors)
        // ====================================================================
        float voltageLimit = g_protection.update();
        
        // ====================================================================
        // 4. Apply voltage limit to power outputs
        // ====================================================================
        g_power.setVoltageLimit(voltageLimit);
        g_power.setOutputVoltage(targetVoltage);
        
        // ====================================================================
        // 5. Read current sensors (for logging)
        // ====================================================================
        float current1 = g_curr1.readCurrentA();
        float current2 = g_curr2.readCurrentA();
        float maxCurrent = max(current1, current2);
        
        // ====================================================================
        // 6. Read digital inputs (active low)
        // ====================================================================
        bool dig1Active = (digitalRead(Config::PIN_DIG_IN_1) == LOW);
        bool dig2Active = (digitalRead(Config::PIN_DIG_IN_2) == LOW);
        
        // ====================================================================
        // 7. Compact status output (every cycle)
        // ====================================================================
        Serial.print(F("P:"));
        Serial.print(pressureBar, 2);
        Serial.print(F("bar | Vt:"));
        Serial.print(targetVoltage, 1);
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
    }
    
    // ========================================================================
    // Detailed status report - runs at STATUS_REPORT_INTERVAL_MS (1Hz default)
    // ========================================================================
    if (now - g_lastStatusMs >= Config::STATUS_REPORT_INTERVAL_MS) {
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
    
    // Pressure
    float pressure = g_map.readPressureBar();
    Serial.print(F("Pressure:        ")); 
    Serial.print(pressure, 3);
    Serial.println(F(" bar"));
    
    // Current readings
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
    
    // Protection status
    Serial.print(F("Protection:      ")); 
    Serial.println(g_protection.getLevelString());
    Serial.print(F("Voltage Limit:   ")); 
    Serial.print(g_protection.getVoltageLimit() * 100.0f, 1);
    Serial.println(F(" %"));
    Serial.print(F("Fault Count:     ")); 
    Serial.println(g_protection.getFaultCount());
    
    // Output status
    float targetV = pressureToTargetVoltage(pressure);
    Serial.print(F("Target Voltage:  ")); 
    Serial.print(targetV, 2);
    Serial.println(F(" V"));
    Serial.print(F("Actual Voltage:  ")); 
    Serial.print(g_power.getActualOutputVoltage(), 2);
    Serial.println(F(" V"));
    Serial.print(F("PWM Duty:        ")); 
    Serial.print(g_power.getCurrentDuty() * 100.0f, 1);
    Serial.println(F(" %"));
    
    // Digital inputs
    bool d1 = (digitalRead(Config::PIN_DIG_IN_1) == LOW);
    bool d2 = (digitalRead(Config::PIN_DIG_IN_2) == LOW);
    Serial.print(F("Digital In 1:    ")); 
    Serial.println(d1 ? "ACTIVE" : "inactive");
    Serial.print(F("Digital In 2:    ")); 
    Serial.println(d2 ? "ACTIVE" : "inactive");
    
    // Runtime
    Serial.print(F("Uptime:          ")); 
    Serial.print(millis() / 1000);
    Serial.println(F(" s"));
    
    Serial.println(F("----------------------------------------"));
    Serial.println();
}
