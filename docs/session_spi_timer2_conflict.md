# Sessão: Conflito SPI/Timer 2 - PWM D3 resetando periodicamente

## Problema
Com o CAN bus ativo (MCP2515 via SPI), a saída PWM no pino D3 (OC2B, Timer 2) reseta para 0% duty periodicamente. D5 (OC0B, Timer 0) funciona perfeitamente com o mesmo valor de PWM.

## Causa raiz identificada
**SPI e Timer 2 compartilham o pino D11 (PB3 = MOSI = OC2A).** Transações SPI do MCP2515 interferem com o Timer 2, corrompendo a saída hardware OC2B no D3. No PumpController original, o CanInterface era um stub vazio sem comunicação SPI real - por isso nunca houve problema.

### Evidências
- Comentando `g_can.poll()`: D3 funciona perfeitamente (sem SPI = sem interferência)
- D5 (Timer 0) nunca apresenta o problema (Timer 0 não compartilha pinos com SPI)
- Com dados CAN reais a 50Hz: problema menos perceptível mas ainda presente no osciloscópio
- Com SavvyCAN a 500ms: problema visível com ciclo bem definido

## Tentativas de solução

### 1. Mover PWM para Timer 0 (D6) - FUNCIONA mas hardware é fixo
- Alterar `PIN_PWM_OUT_1 = 6` (OC0A, Timer 0) no Config.h
- Ambas saídas ficariam no Timer 0, sem conflito com SPI
- **Descartado**: PCB tem trilhas fixas em D3 e D5

### 2. Desabilitar SPI entre transações - PARCIAL
```cpp
// Em CanInterface::poll():
SPCR |= _BV(SPE);   // Habilita SPI antes
// ... checkReceive / readMsgBuf ...
SPCR &= ~_BV(SPE);  // Desabilita SPI depois
```
- Reduziu o problema (dados reais a 50Hz pareciam OK visualmente)
- Ainda apresenta glitches breves durante a transação SPI (~40µs)

### 3. Restaurar COM2B1 após poll - PARCIAL
```cpp
TCCR2A |= _BV(COM2B1);  // Reconecta Timer 2 ao D3 após SPI
```
- Combinado com solução 2, ajudou mas não eliminou completamente

### 4. Software PWM via ISR do Timer 2 - IMPLEMENTADO (ainda com problema de reset)
Abordagem: não usar saída hardware OC2B, controlar D3 via PORTD nas ISRs do Timer 2.

**PowerOutputs.h:**
- `extern volatile bool g_timer2_softpwm;` (declaração)
- Em `begin()`: `TCCR2A &= ~(_BV(COM2B1) | _BV(COM2B0));` desconecta OC2B
- Em `begin()`: `TIMSK2 |= _BV(OCIE2B) | _BV(TOIE2);` habilita interrupções
- Em `setDuty()`: para D3, escreve OCR2B e controla flag ao invés de analogWrite

**PumpControl.ino:**
```cpp
volatile bool g_timer2_softpwm = false;

ISR(TIMER2_OVF_vect) {
    // BOTTOM: set D3 HIGH (início do ciclo PWM)
    if (g_timer2_softpwm) {
        PORTD |= _BV(PD3);
    }
}

ISR(TIMER2_COMPB_vect) {
    // Compare match (up e down): toggle D3
    if (g_timer2_softpwm) {
        PORTD ^= _BV(PD3);
    }
}
```

**Lógica do toggle:**
- BOTTOM → OVF ISR → força HIGH
- Up-count match → COMPB ISR → toggle HIGH→LOW
- Down-count match → COMPB ISR → toggle LOW→HIGH
- Replica comportamento Phase-Correct PWM não-invertido

**Status:** Compila com erro de definição múltipla da variável `g_timer2_softpwm` (resolvido com `extern`), mas o **problema de reset do PWM persiste**. A interferência do SPI pode estar afetando o próprio contador do Timer 2 (TCNT2), não apenas a saída OC2B.

## Arquivos modificados nesta sessão

| Arquivo | Mudanças |
|---------|----------|
| `Config.h` | `TEMP_FAN_OFF_C`: 95→50, `PIN_CAN_CS`: 10→9 |
| `CanInterface.cpp` | Removido workaround SPI enable/disable (substituído por ISR) |
| `PowerOutputs.h` | Software PWM via ISR para D3, `extern volatile bool g_timer2_softpwm` |
| `PumpControl.ino` | Adicionadas ISRs Timer 2 (OVF + COMPB), definição `g_timer2_softpwm` |

## Próximos passos a investigar

1. **Verificar se SPI corrompe TCNT2** (não apenas OC2B): se o contador do Timer 2 é afetado, as ISRs também terão timing errado
2. **Mover D3 para Timer 1**: usar ICR1 como TOP e OC1A/OC1B - Timer 1 não compartilha pinos com SPI. Mas D9 (OC1A) é CAN CS e D10 (OC1B) tem pull-down do módulo
3. **Timer 1 com ISR para D3**: usar Timer 1 compare interrupt para gerar software PWM no D3, eliminando completamente o Timer 2 da equação
4. **Verificar se NeoPixel (D2, PORTD) contribui**: NeoPixel escreve PORTD inteiro durante show(), D3 é PD3. Com COM2B desconectado e ISR controlando via PORTD, o NeoPixel pode sobrescrever o estado de D3
5. **Testar desabilitando NeoPixel**: comentar `g_statusLed.updateFromCurrent()` para isolar se o NeoPixel está sobrescrevendo D3 via PORTD

### Hipótese mais provável: conflito NeoPixel + Software PWM
Com a solução de ISR (software PWM via PORTD), o NeoPixel `show()` pode estar causando o problema:
- NeoPixel D2 está no PORTD (PD2)
- `show()` faz `hi = PORTD | pinMask; lo = PORTD & ~pinMask;` e depois escreve `hi`/`lo` repetidamente para PORTD inteiro via assembly
- Com hardware OC2B, o timer overrides PORTD → NeoPixel não afeta D3
- Com software PWM via PORTD, NeoPixel `show()` pode sobrescrever PD3 durante os ~30µs de transmissão (interrupts desabilitados com `cli()`)
- **Isso causaria glitches no D3 a cada chamada de `updateFromCurrent()`** (a cada 50ms do loop)

## Contexto do hardware
- Arduino Nano (ATmega328P)
- MCP2515 + TJA1050 (CAN bus 500kbps, cristal 8MHz)
- SPI: MOSI=D11, MISO=D12, SCK=D13, CS=D9
- PWM saídas: D3 (Timer 2 OC2B), D5 (Timer 0 OC0B)
- NeoPixel: D2 (PORTD)
- Driver inversor: BC817+BC807 (PWM_INVERTED_BY_HARDWARE = true)
- Timer 0 prescaler: 64→8 (millis() 8x mais rápido, compensado com MILLIS_COMPENSATED)
- PWM frequência: 3.9kHz Phase-Correct
