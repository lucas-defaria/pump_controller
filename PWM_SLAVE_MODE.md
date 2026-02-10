# Modo Slave PWM - Documentação

## Visão Geral

Foi implementado um modo secundário de operação onde a placa pode funcionar como "slave", reproduzindo um sinal PWM externo nas suas saídas, ao invés de usar o controle baseado no sensor MAP.

## Funcionamento

### Modo Normal (MAP-based)
- **Quando**: Nenhum sinal PWM válido detectado na entrada digital PIN 1 (D7)
- **Comportamento**: Sistema opera normalmente lendo o sensor MAP e controlando as bombas baseado na pressão medida
- **Este é o modo padrão quando não há sinal PWM**

### Modo Slave (PWM Replication)
- **Quando**: Sinal PWM válido detectado na entrada digital PIN 1 (D7)
- **Comportamento**: Sistema replica o duty cycle do PWM de entrada nas duas saídas
- **Transição automática**: Sistema detecta automaticamente quando há sinal PWM válido e muda para modo slave
- **Retorno automático**: Se o sinal PWM desaparecer por mais de 200ms, retorna ao modo normal MAP

## Características Técnicas

### Hardware
- **Pino de entrada**: D7 (PIN_DIG_IN_1 / Config::PIN_PWM_INPUT)
- **Frequência esperada**: 25Hz (aceita 15-35Hz)
- **Duty cycle**: 0-100% (variável)
- **Timeout**: 200ms sem pulso válido = sinal perdido

### Proteções Mantidas
Mesmo em modo slave, todas as proteções continuam ativas:
- ? Proteção de corrente (current protection)
- ? Proteção de tensão (voltage protection)
- ? External safety input (D8)
- ? Rate limiting
- ? Status LED indicando corrente e falhas

### Prioridade de Controle
1. **MAIS ALTA**: External Safety Input (D8) - desliga tudo imediatamente
2. **MÉDIA**: PWM Slave Mode - se sinal válido presente
3. **NORMAL**: MAP-based Control - modo padrão quando não há sinal PWM

## Configuração

Todas as configurações estão em [Config.h](Config.h):

```cpp
// Habilitar/desabilitar modo slave
constexpr bool ENABLE_PWM_SLAVE_MODE = true;

// Pino de entrada PWM
constexpr uint8_t PIN_PWM_INPUT = PIN_DIG_IN_1;  // D7

// Faixa de frequência válida (Hz)
constexpr float PWM_INPUT_FREQ_MIN = 15.0f;
constexpr float PWM_INPUT_FREQ_MAX = 35.0f;

// Timeout para perda de sinal (ms)
constexpr unsigned long PWM_INPUT_TIMEOUT_MS = 200;
```

## Detalhes de Implementação

### Classe PwmInput

Nova classe criada em [PwmInput.h](PwmInput.h) que implementa:
- Leitura precisa de PWM por detecção de bordas (rising/falling edge)
- Cálculo automático de frequência e duty cycle
- Validação de sinal (frequência dentro da faixa esperada)
- Timeout automático quando sinal desaparece
- Usa `micros()` para máxima precisão de timing

### Métodos Principais

```cpp
g_pwmInput.update();              // Atualizar leitura (chamar frequentemente)
g_pwmInput.isSignalValid();       // Verificar se sinal PWM válido presente
g_pwmInput.getDutyCycle();        // Obter duty cycle (0.0 a 1.0)
g_pwmInput.getFrequency();        // Obter frequência detectada (Hz)
```

## Output Serial

### Modo Normal
```
P:0.45bar | Vs:12.3V | T%:85% | Vo:10.5V | I1:12.3A | I2:11.8A | Lim:100% | NORMAL
```

### Modo Slave (novo)
```
*** SLAVE MODE *** | PWM In:75.5% @ 25.2Hz | Vs:12.3V | Vo:9.3V | I1:12.3A | I2:11.8A | Lim:100% | NORMAL
```

### Status Report Detalhado
Quando em modo slave, o status report mostra informações adicionais:
```
PWM Slave Mode:  *** ACTIVE ***
  Input Duty:    75.5 %
  Input Freq:    25.12 Hz
  Period:        39.81 ms
  High Time:     30.06 ms
```

## Testes Recomendados

1. **Teste sem sinal**: Verificar que sistema opera normalmente em modo MAP
2. **Teste com PWM 25Hz**: Aplicar PWM de 25Hz e verificar mudança para modo slave
3. **Teste de duty cycle variável**: Variar duty cycle de 10% a 90% e verificar que saídas acompanham
4. **Teste de timeout**: Remover sinal PWM e verificar retorno ao modo MAP após 200ms
5. **Teste de proteções**: Verificar que proteções de corrente/tensão continuam funcionando em modo slave
6. **Teste de frequência fora da faixa**: Aplicar PWM de 10Hz ou 40Hz e verificar que sistema ignora (permanece em modo MAP)

## Notas Importantes

- ?? **Não altera comportamento padrão**: Sistema continua funcionando normalmente quando não há sinal PWM
- ?? **Detecção automática**: Não precisa configurar manualmente, sistema detecta presença de sinal
- ?? **Proteções sempre ativas**: Mesmo em modo slave, todas as proteções (corrente, tensão) permanecem operantes
- ?? **External safety tem prioridade**: Input de segurança externa (D8) sempre tem prioridade sobre tudo
- ? **Transição suave**: Mudança entre modos é suave graças ao rate limiting existente
- ? **Compatível com timer modificado**: Usa `micros()` que não é afetado pela modificação do Timer 0

## Arquivos Modificados

1. **PwmInput.h** (novo) - Classe para leitura de PWM
2. **Config.h** - Adicionadas configurações de PWM slave
3. **PumpControl.ino** - Integração do modo slave no loop principal

## Histórico

- **2025-02-02**: Implementação inicial do modo slave PWM
