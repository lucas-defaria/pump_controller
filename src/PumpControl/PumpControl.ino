/* ----------------------------------------------------------------------------
   PumpControl.ino - Controle básico de bomba baseado em pressão MAP
   - Lê sensor MPX5700ASX em A4
   - Ajusta duas saídas PWM (D3, D5) simulando relé de estado sólido
   - Mapeia 0.20 bar -> 9V e 0.40 bar -> 12V (interpolação linear)
   - Constantes configuráveis em Config.h
   - Extras (corrente, CAN, etc.) presentes como stubs para expansão futura

   Ciclo principal: a cada MAIN_LOOP_INTERVAL_MS atualiza pressão e saída.
---------------------------------------------------------------------------- */
#include <Arduino.h>
#include "Config.h"
#include "MapSensor.h"
#include "PowerOutputs.h"
#include "CurrentSensor.h"
#include "CanInterface.h"

// Instâncias principais
MapSensor      g_map(Config::PIN_MAP_SENSOR);
PowerOutputs   g_power(Config::PIN_PWM_OUT_1, Config::PIN_PWM_OUT_2);
CurrentSensor  g_curr1(Config::PIN_CURRENT_1); // stub
CurrentSensor  g_curr2(Config::PIN_CURRENT_2); // stub
CanInterface   g_can;                          // stub

unsigned long g_lastUpdateMs = 0;

// Converte pressão (bar) para tensão alvo (Volts) com base nos setpoints.
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

void setup() {
    Serial.begin(115200);
    while(!Serial && millis() < 2000) { /* aguarda porta serial (opcional) */ }

    g_map.begin();
    g_power.begin();
    g_curr1.begin();
    g_curr2.begin();
    g_can.begin(); // stub

    pinMode(Config::PIN_DIG_IN_1, INPUT_PULLUP); // ativo em 0V
    pinMode(Config::PIN_DIG_IN_2, INPUT_PULLUP);

    Serial.println(F("PumpControl iniciado"));
}

void loop() {
    unsigned long now = millis();
    if (now - g_lastUpdateMs >= Config::MAIN_LOOP_INTERVAL_MS) {
        g_lastUpdateMs = now;

        float pressureBar = g_map.readPressureBar();
        float targetVoltage = pressureToTargetVoltage(pressureBar);
        g_power.setOutputVoltage(targetVoltage);

        // Leitura (stub) correntes
        float i1 = g_curr1.readCurrentA();
        float i2 = g_curr2.readCurrentA();

        // Estado entradas digitais (ativo LOW)
        bool dig1Active = (digitalRead(Config::PIN_DIG_IN_1) == LOW);
        bool dig2Active = (digitalRead(Config::PIN_DIG_IN_2) == LOW);

        // Futuro: VCC sense, CAN poll, proteção sobrecorrente, etc.

        Serial.print(F("P(bar): ")); Serial.print(pressureBar, 3);
        Serial.print(F(" | Vout(V): ")); Serial.print(targetVoltage, 2);
        Serial.print(F(" | I1: ")); Serial.print(i1, 2);
        Serial.print(F(" | I2: ")); Serial.print(i2, 2);
        Serial.print(F(" | D1: ")); Serial.print(dig1Active ? "ON" : "OFF");
        Serial.print(F(" | D2: ")); Serial.println(dig2Active ? "ON" : "OFF");
    }

    // Poll de CAN (stub)
    g_can.poll();
}
