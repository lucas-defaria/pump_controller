# PumpControl - Sistema Avançado de Controle de Bomba de Combustível

Sistema inteligente de controle de bomba de combustível baseado em Arduino Nano, com controle proporcional por pressão MAP e proteção multinível contra sobrecorrente.

## ?? Visão Geral

O PumpControl é um sistema eletrônico embarcado projetado para aplicações automotivas de alta performance, controlando a alimentação de bombas de combustível de forma proporcional à demanda do motor (medida via sensor MAP). O sistema garante operação segura através de múltiplas camadas de proteção elétrica e térmica.

### Características Principais

- **Controle Proporcional**: Ajusta automaticamente a tensão da bomba (70-100% da alimentação) baseado na pressão MAP (0.4-0.6 bar gauge)
- **Proteção Inteligente**: Sistema multinível de proteção contra sobrecorrente com ação progressiva (nunca desliga completamente - crítico para motores sob carga)
- **Monitoramento em Tempo Real**: Leitura contínua de pressão, corrente (2 canais), tensão de alimentação
- **PWM Alta Frequência**: 3.9 kHz para operação silenciosa e eficiente
- **Status Visual**: LED NeoPixel RGB indicando estado operacional
- **Entrada de Segurança Externa**: Desligamento remoto via sinal digital (D7)
- **Expansível**: Preparado para comunicação CAN bus (MCP2515)

## ?? Especificações Técnicas

### Hardware
- **MCU**: Arduino Nano (ATmega328P @ 16MHz)
- **Alimentação**: 8-16V DC (típico 12V automotivo)
- **Saídas PWM**: 2x canais @ 3.9 kHz (D3, D5)
- **Corrente Máxima**: 50A por canal (ACS772LCB-050B)
- **Sensor de Pressão**: MPX5700AP (15-700 kPa absoluto)
- **Proteção**: Circuito de inversão com BC817/BC807 para acionamento de MOSFETs

### Arquitetura de Software

```
PumpControl.ino
??? MapSensor (MPX5700AP)           - Leitura e conversão de pressão
??? PowerOutputs (PWM)              - Controle de saídas com rate limiting
??? CurrentSensor (ACS772LCB-050B)  - Medição de corrente com multi-amostragem
??? PowerProtection                 - Sistema de proteção multinível
??? VoltageSensor                   - Monitoramento de tensão de alimentação
??? VoltageProtection               - Proteção adaptativa por queda de tensão
??? StatusLed (NeoPixel)            - Indicação visual de estado
??? CanInterface (MCP2515)          - Comunicação CAN (preparado)
??? Config.h                        - Parâmetros de configuração
```

## ?? Mapeamento de Pinos

| Pino | Função | Descrição |
|------|--------|-----------|
| A4 | MAP Sensor | Entrada analógica do sensor MPX5700AP |
| A2 | Current Ch1 | Sensor de corrente canal 1 (ACS772LCB-050B) |
| A3 | Current Ch2 | Sensor de corrente canal 2 (ACS772LCB-050B) |
| A5 | Supply Voltage | Medição de tensão de alimentação (divisor 1:11) |
| D2 | Status LED | NeoPixel RGB para indicação visual |
| D3 | PWM Out 1 | Saída PWM canal 1 (3.9 kHz) |
| D5 | PWM Out 2 | Saída PWM canal 2 (3.9 kHz) |
| D7 | Safety Input | Entrada de desligamento externo (ativo HIGH) |
| D8 | Digital In 2 | Entrada digital reservada (ativo LOW) |

## ??? Sistema de Proteção Multinível

O sistema implementa proteção progressiva contra sobrecorrente, **nunca desligando completamente** a bomba (crítico para motores sob carga):

| Nível | Faixa de Corrente | Ação | Limite de Tensão |
|-------|-------------------|------|------------------|
| **NORMAL** | 0 - 25A | Operação normal | 100% (sem limite) |
| **WARNING** | 25 - 30A | Redução leve | 70% da alimentação |
| **HIGH** | 30 - 35A | Redução moderada | 60% da alimentação |
| **CRITICAL** | 35 - 40A | Redução agressiva | 50% da alimentação |
| **FAULT** | 40 - 45A | Limite mínimo seguro | 50% da alimentação |
| **EMERGENCY** | > 45A | Desligamento total* | 0% (opcional) |

\* *EMERGENCY shutdown configurável via `ENABLE_EMERGENCY_SHUTDOWN` em Config.h*

### Proteção por Tensão de Alimentação

Sistema adaptativo que detecta quedas de tensão e ajusta limites automaticamente:
- **WARNING**: Queda de 30% na tensão de alimentação
- **CRITICAL**: Queda de 50% na tensão de alimentação
- Histerese de 0.5V para evitar oscilações

### Entrada de Segurança Externa

Sinal digital em D7 permite desligamento remoto:
- **Ativo HIGH** (configurável): Desliga imediatamente todas as saídas
- Bypass de rate limiting (ação instantânea)
- Ideal para integração com ECU ou sistemas de segurança

## ?? Conversão de Pressão

**Sensor MPX5700AP** (pressão absoluta):
```
Vout = Vs × (0.00125 × P[kPa] + 0.04)

Pressão gauge = Pressão absoluta - Pressão atmosférica
P[bar gauge] = (P[kPa] - 101.3) / 100
```

**Mapeamento Pressão ? Potência**:
```
0.4 bar gauge ? 70% da tensão de alimentação
0.6 bar gauge ? 100% da tensão de alimentação
Interpolação linear entre pontos
```

## ?? Como Usar

### Requisitos
- Arduino IDE 1.8.x ou superior / PlatformIO
- Biblioteca Adafruit_NeoPixel (para LED RGB)
- Placa: Arduino Nano / Arduino Uno compatible

### Compilação e Upload

1. **Abrir o projeto**
   ```
   FW/src/PumpControl/PumpControl.ino
   ```

2. **Configurar parâmetros** (opcional)
   Editar `Config.h` para ajustar:
   - Setpoints de pressão (`MAP_BAR_LOW_SETPOINT`, `MAP_BAR_HIGH_SETPOINT`)
   - Thresholds de corrente (`CURRENT_THRESHOLD_*`)
   - Habilitação de proteções (`ENABLE_EMERGENCY_SHUTDOWN`, `ENABLE_EXTERNAL_SAFETY`)

3. **Selecionar placa**
   - Tools ? Board ? Arduino Nano
   - Tools ? Processor ? ATmega328P (Old Bootloader)

4. **Upload**
   - Conectar Arduino via USB
   - Sketch ? Upload

5. **Monitorar operação**
   - Tools ? Serial Monitor
   - Baud Rate: 115200

### Primeira Inicialização

O sistema executa sequência de inicialização segura:
1. Todas as saídas desligadas (0% duty)
2. Delay de 2 segundos para estabilização de sensores
3. Início do controle proporcional

**IMPORTANTE**: Durante gravação, pode ser necessário remover componentes do pino RESET (ver `Problemas placa.txt`).

## ?? Monitoramento Serial

### Saída Compacta (20Hz)
```
P:0.52bar | Vs:12.3V | T%:80% | Vo:9.8V | I1:18.5A | I2:17.2A | Lim:100% | NORMAL
```

### Relatório Detalhado (1Hz)
```
----------------------------------------
STATUS REPORT
----------------------------------------
Pressure:        0.523 bar
Current Ch1:     18.47 A
Current Ch2:     17.28 A
Max Current:     18.47 A
Supply Voltage:  12.34 V
Protection:      NORMAL
Voltage Limit:   100.0 %
Target Percent:  80.5 %
Target Voltage:  9.94 V
Actual Voltage:  9.92 V
PWM Duty:        80.4 %
Uptime:          127 s
----------------------------------------
```

## ?? Diagnóstico e Troubleshooting

### LED de Status (NeoPixel - D2)

| Cor | Estado | Significado |
|-----|--------|-------------|
| ?? Azul | Inicialização | Sistema inicializando |
| ?? Verde | Normal | Operação normal |
| ?? Amarelo | Advertência | Corrente elevada (WARNING) |
| ?? Laranja | Alto | Corrente alta (HIGH) |
| ?? Vermelho | Crítico | Proteção ativa (CRITICAL/FAULT) |
| ? Apagado | Desligado | Segurança externa ativa |

### Problemas Comuns

**Leitura de corrente instável/ruidosa**
- Causa: Interferência de PWM no sensor
- Solução: Sistema já implementa multi-amostragem (10 samples + filtro EMA)

**Sensor MAP lê 0V**
- Verificar: Pino correto do sensor (pino 1 = Vout)
- Verificar: Alimentação 5V do sensor
- Nota: Conectores CN4/CN5 podem estar invertidos na PCB v1.0

**Gravação falha**
- Remover componentes conectados ao pino RESET durante upload
- Usar "Old Bootloader" nas configurações da IDE

**Bomba não responde**
- Verificar: Tensão de alimentação (8-16V)
- Verificar: Conexão do gate dos MOSFETs
- Verificar: Inversão de PWM (`PWM_INVERTED_BY_HARDWARE = true`)

## ?? Notas Importantes da PCB v1.0

Baseado em `Problemas placa.txt`:

1. **Conectores invertidos**: CN4 e CN5 podem estar com pinagem invertida
2. **Sensor MAP**: Pino correto de Vout é o pino 1 (não o 3)
3. **Sensor de corrente**: Modelo real montado é **ACS772LCB-050B** (50A unidirecional)
4. **Gravação**: Necessário remover componentes do pino RESET
5. **Alimentação USB**: Trilha de 5V do Arduino deve ser cortada para evitar alimentação reversa

## ?? Funcionalidades Futuras

- [ ] Comunicação CAN bus (MCP2515) - infraestrutura já implementada
- [ ] Saídas analógicas 0-10V (D6, D9) - requer circuito externo DAC
- [ ] Watchdog para detecção de travamento
- [ ] Calibração de sensores via serial
- [ ] Datalog em SD card
- [ ] Integração com display OLED

## ?? Documentação Adicional

- **Manual de Instalação**: Ver `Installation_Manual.html` (gerado automaticamente)
- **Esquemático**: `pcb/EvoProject 1.0.pdf`
- **Configuração de LED NeoPixel**: `FW/NEOPIXEL_SETUP.md`

## ?? Licença

Uso interno/educacional. Para uso comercial, consulte os autores.

---

**Versão**: 2.0 (Janeiro 2026)  
**Hardware**: PCB EvoProject v1.0  
**Firmware**: PumpControl Advanced Protection System
