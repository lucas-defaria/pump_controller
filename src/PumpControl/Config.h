#pragma once

// -----------------------------------------------------------------------------
// Config.h - Compile-time configuration constants for Pump Control project
// -----------------------------------------------------------------------------
// Ajuste aqui (em c�digo) os setpoints de press�o e tens�es desejadas.
// Nada disso � configur�vel pelo usu�rio final em runtime.
// -----------------------------------------------------------------------------

namespace Config {
    // Press�o (bar) onde a sa�da deve estar em OUTPUT_VOLTAGE_MIN
    constexpr float MAP_BAR_LOW_SETPOINT  = 0.20f; // bar
    // Press�o (bar) onde a sa�da deve estar em OUTPUT_VOLTAGE_MAX
    constexpr float MAP_BAR_HIGH_SETPOINT = 0.40f; // bar

    // Tens�es alvo correspondentes (ex: 9V @ 0.2bar e 12V @ 0.4bar)
    constexpr float OUTPUT_VOLTAGE_MIN = 9.0f;   // Volts
    constexpr float OUTPUT_VOLTAGE_MAX = 12.0f;  // Volts

    // Tens�o de alimenta��o do est�gio de pot�ncia (assumido)
    constexpr float SUPPLY_VOLTAGE = 12.0f; // Volts

    // Pinos principais
    constexpr uint8_t PIN_MAP_SENSOR   = A4; // MPX5700ASX
    constexpr uint8_t PIN_PWM_OUT_1    = 3;  // D3 (SSR 1)
    constexpr uint8_t PIN_PWM_OUT_2    = 5;  // D5 (SSR 2)

    // Extras (stubs / futuros)
    constexpr uint8_t PIN_CURRENT_1    = A2; // ACS772 canal 1
    constexpr uint8_t PIN_CURRENT_2    = A3; // ACS772 canal 2
    constexpr uint8_t PIN_EXTRA_PWM_1  = 1;  // PD1 (ex: TX pin em alguns Arduinos) - cuidado com conflito
    constexpr uint8_t PIN_EXTRA_PWM_2  = 0;  // PD0 (ex: RX pin) - cuidado com conflito
    constexpr uint8_t PIN_ANALOG_OUT_1 = 6;  // D6 (0-10V via circuito externo)
    constexpr uint8_t PIN_ANALOG_OUT_2 = 9;  // D9 (0-10V via circuito externo)
    constexpr uint8_t PIN_DIG_IN_1     = 7;  // D7 (ativo em n�vel baixo)
    constexpr uint8_t PIN_DIG_IN_2     = 8;  // D8 (ativo em n�vel baixo)
    constexpr uint8_t PIN_AUX_IN_1     = A0; // Anal�gica extra
    constexpr uint8_t PIN_AUX_IN_2     = A1; // Anal�gica extra
    constexpr uint8_t PIN_VCC_SENSE    = A5; // Divisor de tens�o entrada alimenta��o

    // Filtro simples (EMA) para leitura do sensor MAP
    constexpr float MAP_FILTER_ALPHA   = 0.15f; // 0<alpha<=1 (menor = mais suave)

    // Intervalo de atualiza��o principal (ms)
    constexpr unsigned long MAIN_LOOP_INTERVAL_MS = 50; // 20Hz
}
