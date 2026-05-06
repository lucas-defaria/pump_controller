# PumpControl — Controle de Bomba de Combustível

Firmware para controle de bomba de combustível baseado em Arduino Nano, com dois modos de operação (MAP-based e slave por PWM externo) e proteção multinível por corrente, tensão e safety externa.

Este README descreve o estado atual do firmware no branch `main`.

## Visão geral

- **Modo MAP (default)**: pressão do MAP modulando duty entre 50% e 100% de Vsupply.
- **Modo slave (override)**: quando há PWM externo válido em D8, o duty externo é replicado nos outputs (anula o controle por MAP).
- **Proteção em 3 níveis** por corrente: NORMAL → FAULT → EMERGENCY.
- **Boot hold-off** de 2 s com motor OFF e inicialização defensiva (pinos em estado seguro antes de virar OUTPUT).
- **PWM de potência a 3.9 kHz** em Timer 0 (D5/D6), com compensação de timing em todo o código.

## Hardware

- **MCU**: Arduino Nano (ATmega328P @ 16 MHz)
- **Alimentação do módulo**: +5 V vindo da placa host entra no pino **5V (VOUT)** do Nano. **VIN desconectado** para evitar dropout do regulador interno (o DCDC da host já entrega 5 V regulado).
- **Power stage**: IRFB3077 como switch low-side, MBR30100 como freewheeling. Carga ligada entre VCC2 e PWM_OUT — o sinal medido no dreno é invertido em relação ao gate e só aparece quando há carga.
- **Driver de gate**: BC817 (NPN) + BC807 (PNP) — topologia inverte o sinal PWM, compensada em SW (`PWM_INVERTED_BY_HARDWARE = true`).
- **Sensores de corrente**: 2× **ACS758LCB-050B** (bidirecional ±50 A, 40 mV/A nominal). Correntes negativas (fluxo reverso) são clampadas a 0.
- **Sensor MAP**: MPX5700AP (pressão absoluta 15–700 kPa). Conversão para gauge subtraindo `ATMOSPHERIC_PRESSURE_BAR` (1.013 bar).
- **NTC 10 K** no dissipador, dividor com R19 (10 K) e R20 em série (alta impedância) para o ADC. C16 (4.7 μF) filtra.
- **Sense de Vsupply**: divisor 1:11 (R1=10 K, R2=1 K).
- **LED de status**: NeoPixel (1 LED) em D2.

## Pinout (Config.h)

| Pino | Função | Observações |
|------|--------|-------------|
| A1 | NTC heatsink | Beta 3950, R25 = 10 K |
| A2 | Current Ch1 | ACS758LCB-050B |
| A3 | Current Ch2 | ACS758LCB-050B |
| A4 | MAP | MPX5700AP |
| A5 | Vsupply | Divisor 1/11 |
| D2 | NeoPixel | Status visual |
| D5 | PWM_OUT_2 | Timer 0 / OC0B |
| D6 | PWM_OUT_1 | Timer 0 / OC0A — movido de D3 (ver nota abaixo) |
| D7 | Safety input | OPTO output, ativo LOW (HIGH = OK) |
| D8 | PWM input externo | Slave mode (200–400 Hz) |
| A0 | AUX in | Reservado |

> **Por que D6 e D5 (Timer 0)** — o pino D11 (OC2A do Timer 2) é compartilhado com SPI/MOSI; quando o CAN/MCP2515 está ativo, ocorrem glitches periódicos no PWM em D3 (OC2B do mesmo Timer 2). Timer 0 não tem overlap com SPI, então mover ambas as saídas para D5/D6 elimina o problema.

> **R30 e R45 não populados** — D7 e D8 são apenas saídas dos OPTOs; não há caminho elétrico de retorno para a base de T4/T8. O Arduino é o **único driver** dos gates dos MOSFETs via D5/D6.

## PWM de potência

- **3.9 kHz** Phase-Correct PWM em Timer 0 (`16 MHz / (2 × 256 × 8) ≈ 3906 Hz`).
- Prescaler do Timer 0 alterado de 64 → 8 para atingir essa frequência.
- **Efeito colateral**: `millis()` e `delay()` rodam **8× mais rápido**. Todo código de timing usa o macro `MILLIS_COMPENSATED(ms)` (multiplica por `TIMER0_PRESCALER_FACTOR = 8`). `delayMicroseconds()` **não** é afetado.
- `PWM_INVERTED_BY_HARDWARE = true`: SW inverte o byte (`pwmValue = 255 - pwmValue`) antes do `analogWrite`, de forma que `duty = 1.0` corresponde a MOSFET ON (potência total).

## Modos de operação

### MAP mode (default)

Mapeamento linear entre pressão (gauge) e percentual da Vsupply:

| Pressão (bar gauge) | Saída |
|---------------------|-------|
| ≤ 0.4 (`MAP_BAR_LOW_SETPOINT`) | 50% Vsupply (`OUTPUT_PERCENT_MIN`) |
| 0.4 → 0.6 | interpolação linear |
| ≥ 0.6 (`MAP_BAR_HIGH_SETPOINT`) | 100% Vsupply (`OUTPUT_PERCENT_MAX`) |

Filtro EMA no MAP: `MAP_FILTER_ALPHA = 0.15`.

### Slave mode (PWM externo em D8)

Quando `ENABLE_EXTERNAL_PWM_MODE = true` e há sinal válido em D8, o controle por MAP é **sobrescrito**: o duty cycle medido na entrada vira o target dos outputs.

- Frequência válida: **200–400 Hz** (típico ~300 Hz)
- Timeout: **200 ms** sem pulso → volta para MAP
- Leitura via `pulseIn()` (pode bloquear até ~100 ms por amostra), uma medida por iteração do loop principal
- Linha de status no Serial muda para `*** EXTERNAL PWM MODE ***` com freq e duty medidos

## Proteção por corrente (3 níveis)

Estratégia: **nunca desligar totalmente em condição de fault** (motor sob carga seria danificado), exceto em EMERGENCY (curto/saturação).

| Nível | Faixa | Limite de tensão | Ação |
|-------|-------|------------------|------|
| **NORMAL** | < 40 A | 100% | Sem limitação |
| **FAULT** | ≥ 40 A | 50% | Reduz para mínimo seguro com rate limiting |
| **EMERGENCY** | ≥ 45 A | 0%* | Shutdown imediato, **bypassa rate limiting** e força `setDuty(0)` no main loop |

\* Se `ENABLE_EMERGENCY_SHUTDOWN = false`, EMERGENCY cai para 50% como fail-safe.

- Histerese: **2.5 A** para retornar ao nível anterior
- Decisão usa `max(I_ch1, I_ch2)` (qualquer canal acima do threshold dispara)
- Rate limiting normal: 0.05 por ciclo de 50 ms (≈ 1 s para varredura completa)
- Override de EMERGENCY no `loop()`: mesmo se source for slave, EMERGENCY força duty 0

## Proteção por tensão de alimentação

Adaptativa por queda percentual da Vsupply medida (lida em A5):

- **WARNING**: queda de 30%
- **CRITICAL**: queda de 50%
- Histerese: 0.5 V para evitar oscilação
- Faixa válida do sensor: 7.0–16.0 V (fora disso = sensor fault)

## Safety externa (D7)

- `ENABLE_EXTERNAL_SAFETY = true`
- `EXTERNAL_SAFETY_ACTIVE_HIGH = false` → **LOW = shutdown**, HIGH = OK (OPTO mantém HIGH em operação normal)
- **Bypassa rate limiting**: ação instantânea
- LED pisca azul (`updateExternalSafetyBlink`) enquanto ativo
- Skipa o restante do loop de controle (prioridade máxima)

## Sensor de corrente — calibração

ACS758LCB-050B (bidirecional, sensitivity nominal 40 mV/A):

- Calibração de slope feita em bancada (após filtro RC):
  - 1.32 A → 2.54 V
  - 2.30 A → 2.58 V
  - Slope = `(2.58 − 2.54) / (2.30 − 1.32) = 40.8 mV/A` ≈ nominal
- Vzero derivado do slope: **2.488 V @ 0 A** (offset Voe dentro do spec ±60 mV)
- Multi-amostragem: **32 samples × 50 μs ≈ 4.9 ms** (~19 ciclos de PWM a 3.9 kHz)
- EMA `CURRENT_FILTER_ALPHA = 0.05` (constante de tempo ~1 s a 20 Hz — agressivo para rejeitar ripple, lento o bastante para proteção)

## Temperatura (NTC 10K) — monitoramento

- Equação Beta: `1/T = 1/T25 + (1/β) · ln(R / R25)`, β = 3950
- Mesmo rail (+5 V) para divisor e ADC → `Vref` cancela: `R_NTC = R_PULLUP × adc / (1023 − adc)`
- Detecção de falha: temp fora de [-40, 150] °C indica sensor aberto/curto
- **Sem ação de proteção** — apenas leitura e log no status report

## LED de status (NeoPixel em D2)

| Cor | Significado |
|-----|-------------|
| Verde sólido | Normal (gradiente verde→vermelho conforme corrente sobe) |
| Vermelho 1 Hz | FAULT |
| Vermelho 5 Hz | EMERGENCY |
| Azul piscando | Safety externa ativa |

## Comunicação

- **Serial @ 115200 bps**:
  - Linha compacta a 20 Hz com modo (MAP/EXTERNAL PWM), pressão ou duty externo, target, Vsupply, I1, I2, voltage limit, nível de proteção
  - Relatório detalhado a 1 Hz com todas as métricas, fault counts e estado dos inputs digitais
- **CAN bus (MCP2515)**: stub presente (`g_can.poll()`), infra mínima — sem tráfego ativo nesta versão do `main`. Versão com CAN funcional segue em `develop-TempControl`.

## Sequência de boot

1. `PowerOutputs::begin()` força os pinos em estado seguro (HIGH = MOSFET OFF na topologia invertida) ainda como INPUT
2. Configura como OUTPUT após estabilização (100 μs)
3. Configura Timer 0 (Phase-Correct, prescaler 8) para 3.9 kHz
4. `setDuty(0)` + 100 ms de grace period
5. Inicialização dos sensores e da safety externa
6. **Hold-off de 2 s** com motor OFF (`delayMicroseconds(2_000_000)` — não afetado pelo prescaler)
7. Entra no loop normal

## Configuração — flags principais (Config.h)

| Flag | Default | Função |
|------|---------|--------|
| `ENABLE_HIGH_FREQ_PWM` | `true` | 3.9 kHz no Timer 0 |
| `PWM_INVERTED_BY_HARDWARE` | `true` | Compensa BC817+BC807 |
| `ENABLE_EMERGENCY_SHUTDOWN` | `true` | 0% em EMERGENCY (false = 50%) |
| `ENABLE_EXTERNAL_SAFETY` | `true` | D7 LOW desliga |
| `EXTERNAL_SAFETY_ACTIVE_HIGH` | `false` | Polaridade da safety |
| `ENABLE_EXTERNAL_PWM_MODE` | `true` | Slave mode em D8 |

Ajustes finos: setpoints de pressão (`MAP_BAR_*_SETPOINT`), thresholds de corrente (`CURRENT_THRESHOLD_*`), faixa válida do sensor (`VOLTAGE_*_VALID`), filtros EMA.

## Build

- Arduino IDE 1.8+ ou PlatformIO
- Lib: `Adafruit_NeoPixel`
- Board: Arduino Nano (ATmega328P, Old Bootloader)
- Sketch: `src/PumpControl/PumpControl.ino`

## Notas da PCB v1.0

- 5 V da host alimenta o Nano via pino 5V (VOUT); VIN deixado desconectado para evitar dropout do regulador interno
- Conectores CN4 e CN5 podem estar com pinagem invertida
- Sensor MAP: Vout é o **pino 1** (não pino 3)
- Pode ser necessário remover componentes do pino RESET durante gravação
- Trilha de 5 V do USB do Arduino deve ser cortada para evitar alimentação reversa
- PWM_OUT_1 movido de D3 → **D6** para resolver glitches por compartilhamento de Timer 2 com SPI

## Estrutura do firmware

```
src/PumpControl/
├── PumpControl.ino       — main loop, source select, override de EMERGENCY
├── Config.h              — todos os parâmetros de compile-time
├── MapSensor.{h,cpp}     — MPX5700AP, conversão absoluta → gauge, EMA
├── PowerOutputs.{h,cpp}  — Timer 0 PWM, inversão por HW, voltage limiting
├── CurrentSensor.{h,cpp} — ACS758LCB-050B, multi-sampling, EMA
├── PowerProtection.h     — máquina de estados NORMAL/FAULT/EMERGENCY
├── VoltageSensor.{h,cpp} — divisor 1:11, leitura de Vsupply
├── VoltageProtection.h   — proteção por queda percentual
├── TempSensor.h          — NTC 10K, equação Beta (monitoramento)
├── PwmInput.h            — pulseIn-based, slave mode em D8
├── StatusLed.h           — NeoPixel state machine
└── CanInterface.{h,cpp}  — stub MCP2515
```
