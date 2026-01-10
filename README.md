# Projeto Controle de Bomba (PumpControl)

Base inicial para firmware em Arduino (SuperMini ou compatível) para controlar saídas de potência (SSR simulados via PWM) em função da pressão medida por um sensor MAP MPX5700ASX.

## Objetivos Principais
- Ler sensor MAP no pino `A4` (MPX5700ASX).
- Converter leitura em pressão (bar) usando equação do datasheet.
- Mapear pressão para tensão desejada nas saídas (ex: 0.20 bar -> 9V, 0.40 bar -> 12V) com interpolação linear.
- Acionar duas saídas PWM (`D3`, `D5`).

Configurações (setpoints) estão em `include/Config.h` e NÃO são alteradas em runtime pelo usuário.

## Extras Planejados (stubs já criados)
- Medição de corrente via ACS772LCB (`A2`, `A3`).
- Saídas extras PWM (PD1, PD0) espelhando duty (cuidado: podem conflitar com Serial RX/TX).
- Saídas analógicas 0-10V (`D6`, `D9`) – exigem circuito externo (ex: DAC + amplificador). Atualmente não implementadas.
- Entradas digitais (`D7`, `D8`) ativas em nível baixo (pull-up interno).
- Entradas analógicas adicionais (`A0`, `A1`).
- Leitura CAN via MCP2515 (SPI) – placeholder.
- Leitura de VCC via divisor `A5` – a implementar.
- Lógica de proteção de sobrecorrente.

## Arquitetura Simplificada
```
PumpControl.ino
  |-- MapSensor (MPX5700)
  |-- PowerOutputs (PWM SSR)
  |-- CurrentSensor (stub)
  |-- CanInterface (stub)
  |-- Config (constantes)
```

## Conversão de Pressão
Equação aplicada (datasheet MPX5700):
```
Vout = Vs * (0.00125 * P(kPa) + 0.04)
=> P(kPa) = (Vout/Vs - 0.04) / 0.00125
=> P(bar) = P(kPa) / 100
```
Assume alimentação do sensor Vs = 5V.

## Mapeamento Pressão -> Tensão Saída
Linear entre `MAP_BAR_LOW_SETPOINT` e `MAP_BAR_HIGH_SETPOINT`.
Abaixo do low: tensão mínima (`OUTPUT_VOLTAGE_MIN`). Acima do high: tensão máxima (`OUTPUT_VOLTAGE_MAX`).
Tensão convertida para duty: `duty = Vout / SUPPLY_VOLTAGE`.

## Fluxo Principal
1. Lê pressão filtrada (EMA simples).
2. Calcula tensão alvo.
3. Ajusta PWM das duas saídas.
4. Reporta via Serial monitor.

## Como Usar
1. Abrir o projeto na IDE Arduino ou PlatformIO.
2. Ajustar pinos se necessário em `Config.h` (caso placa difira). Evitar uso de RX/TX para PWM se precisar de Serial.
3. Fazer upload.
4. Monitorar Serial a 115200 baud.

## Próximos Passos / Melhorias
- Calibração do sensor (offset/escala) conforme montagem real.
- Implementar medição de corrente real (sensibilidade ACS772).
- Proteção: desligar PWM se corrente exceder limite.
- Implementar leitura e escalonamento da tensão de alimentação (A5).
- CAN: integrar biblioteca MCP2515 e definir protocolo de mensagens.
- Saídas analógicas reais 0-10V (circuito externo + código).
- Watchdog / failsafe se sensor MAP inválido.

## Licença
Uso livre interno. Adicione licença conforme necessidade.
