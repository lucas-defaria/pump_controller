# Manual do Usuário - PumpControl

## Especificações Técnicas

### Alimentação
- **Tensão de entrada:** 8-16V DC (sistema automotivo)
- **Consumo:** Variável conforme carga conectada

### Sensor de Pressão (MAP)
- **Range de operação:** 15-700 kPa absoluto (0.15 - 7bar) (gauge/relativo)
- **Tipo:** MPX5700AP (pressão absoluta, convertida para gauge)

### Saídas PWM
- **Canais:** 2 saídas independentes
- **Controle:** Percentual da tensão de alimentação
  - Pressão baixa (0.4 bar): 70% da tensão de entrada
  - Pressão alta (0.6 bar): 100% da tensão de entrada
  - Valores intermediários: interpolação linear

### Monitoramento de Corrente
- **Canais:** 2 sensores independentes (ACS772LCB-050B)
- **Range:** 0-50A Maxima medida pelo sensor por canal (unidirecional)

---

## Conector de Interface

![image](https://hackmd.io/_uploads/HkmSSi0LZl.png)



### Pinagem do Conector - vista traseira

| Pino | Função |
|------|--------|
| 1 | VCC (Alimentação +8-16V) |
| 2 | GND (Terra) |
| 3 |             |
| 4 |             |
| 5 |             |
| 6 |             |
| 7 |             |
| 8 |             |
| 9 |             |
| 10 |             |
| 11 |             |
| 12 |             |
| 13 |             |
| 14 |             |
| 15 | Desliga saída (Safety) aciona por negativo |
| 16 |  |

![saidas](https://hackmd.io/_uploads/rkDF_sA8bg.png)

#### +12 = +Bateria
#### S1 S2 = Saída para bomba 1 e 2 - acionamento por negativo
#### GND = Negativo bateria
---

## Sistema de Proteção por Corrente

O sistema possui proteção progressiva que **nunca desliga completamente** a bomba (crítico para motor em funcionamento).

### Níveis de Proteção

| Corrente | Nível | Ação | Redução de Potência |
|----------|-------|------|---------------------|
| 0-25A | **NORMAL** | Operação normal | 0% (potência plena) |
| 25-30A | **WARNING** | Alerta inicial | 30% de redução |
| 30-35A | **HIGH** | Corrente alta | 40% de redução |
| 35-40A | **CRITICAL** | Corrente crítica | 50% de redução |
| >40A | **FAULT** | Falha detectada | 50% mantido (mínimo seguro) |
| >45A | **EMERGENCY** | Emergência | Desligamento completo* |

> *EMERGENCY: Indica curto-circuito ou sobrecarga severa (sensor próximo ao limite de 50A)

---

## Indicação Visual - LED RGB

O LED indica o estado do sistema em tempo real pela cor e comportamento:

### Cores Sólidas (operação normal)
| Cor | Significado |
|-----|-------------|
| ?? **Verde** | Corrente baixa (0-25A) - operação normal |
| ?? **Verde?Amarelo** | Corrente moderada (25-30A) - entrando em warning |
| ?? **Amarelo?Laranja** | Corrente elevada (30-35A) - proteção ativa |
| ?? **Laranja?Vermelho** | Corrente alta (35-40A) - proteção crítica |

### Piscando (situações críticas)
| Comportamento | Significado | Ação Necessária |
|---------------|-------------|-----------------|
| ?? **Pisca lento** (1Hz) | FAULT - Corrente >40A | Verificar carga/fiação |
| ?? **Pisca rápido** (5Hz) | EMERGENCY - Corrente >45A | Verificar curto-circuito |
| ?? **Pisca azul** (2Hz) | Desligamento por segurança externa | Verificar entrada D8 |

---

## Entrada de Segurança Externa Pino 15

### Função
Permite que sistemas externos (ECU, relé de segurança, etc.) desliguem imediatamente as saídas.

### Configuração
- **Pino:** 15 (ativo em LOW - pull-up interno)
- **Ação:** Quando ativada, força saídas para 0V imediatamente
- **Indicação:** LED pisca em azul enquanto ativa

### Comportamento
- Quando a entrada de segurança é acionada:
  1. Saídas PWM vão para 0% instantaneamente (sem rampa)
  2. LED pisca em azul
  3. Mensagem no serial: "EXTERNAL SAFETY ACTIVE"
  4. Sistema retoma operação normal quando entrada é liberada

---

---

## Observações Importantes

### ?? Proteção Crítica
- O sistema **NUNCA** desliga completamente a bomba em níveis NORMAL-FAULT
- Apenas em EMERGENCY (>45A) ocorre desligamento completo
- Proteção progressiva reduz corrente gradualmente

### ?? Operação Normal
- LED verde = tudo OK
- Transição de cores = sistema adaptando à carga
- Corrente típica: 15-25A em operação normal

### ?? Atenção Necessária
- LED piscando vermelho = verificar carga e fiação
- LED piscando azul = entrada de segurança ativa
- Corrente >30A = investigar causa

### ?? Adaptação Automática
- Sistema ajusta saída baseado em:
  1. Pressão MAP (0.4-0.6 bar)
  2. Tensão de alimentação (8-16V)
  3. Corrente monitorada (proteção)
  
- Saída é sempre um **percentual da tensão de entrada**
- Exemplo: 80% em 12V = 9.6V / 80% em 14V = 11.2V

---

## Resumo - O que Observar

? **Normal:** LED verde sólido, corrente <25A  
?? **Atenção:** LED amarelo/laranja, corrente 25-35A  
?? **Crítico:** LED vermelho ou piscando, corrente >35A  
?? **Segurança:** LED azul piscando, entrada D8 ativa

---

*Versão do firmware: Baseada em Config.h com proteção progressiva*
