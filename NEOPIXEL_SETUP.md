# Configuração do LED NeoPixel - Status Visual

## Descrição
O sistema agora inclui um LED NeoPixel RGB no pino D2 para indicação visual do estado de carga do motor baseado na pressão MAP.

### Indicação de Cores:
- **?? VERDE**: Baixa carga (MAP ? 0.2 bar)
- **?? AMARELO/LARANJA**: Carga média (MAP entre 0.2 e 0.4 bar) - transição gradual
- **?? VERMELHO**: Alta carga (MAP ? 0.4 bar - setpoint superior)

A cor muda gradualmente entre verde e vermelho conforme a pressão MAP aumenta, proporcionando feedback visual em tempo real da carga do motor.

## Hardware

### Conexões:
- **Pino D2**: Conectado ao DATA IN do LED NeoPixel
- **5V**: Alimentação do NeoPixel
- **GND**: Terra comum

### LED Suportado:
- WS2812B / NeoPixel compatível
- Configurado para 1 LED (pode ser ajustado em Config.h)

## Instalação da Biblioteca

### Método 1: Arduino IDE
1. Abra o Arduino IDE
2. Vá em **Sketch** ? **Include Library** ? **Manage Libraries...**
3. Pesquise por **"Adafruit NeoPixel"**
4. Clique em **Install** na biblioteca "Adafruit NeoPixel" by Adafruit
5. Aguarde a instalação completar

### Método 2: PlatformIO
Adicione ao seu `platformio.ini`:
```ini
lib_deps =
    adafruit/Adafruit NeoPixel@^1.12.0
```

## Arquivos Modificados/Criados

### Novos Arquivos:
- `StatusLed.h` - Classe de controle do LED NeoPixel

### Arquivos Modificados:
- `Config.h` - Adicionadas configurações:
  - `PIN_STATUS_LED` (D2)
  - `STATUS_LED_COUNT` (1)
  - `LED_BRIGHTNESS` (50 = ~20% do brilho máximo)
  
- `PumpControl.ino`:
  - Adicionado include `StatusLed.h`
  - Criada instância global `g_statusLed`
  - Inicialização no `setup()`
  - Atualização no `loop()` a cada ciclo (20Hz)

## Configuração

Todas as configurações estão em `Config.h`:

```cpp
// Pino do LED NeoPixel
constexpr uint8_t PIN_STATUS_LED = 2;  // D2

// Quantidade de LEDs na tira/anel
constexpr uint16_t STATUS_LED_COUNT = 1;

// Brilho (0-255, recomendado 30-100)
constexpr uint8_t LED_BRIGHTNESS = 50;
```

## Como Funciona

A classe `StatusLed` utiliza os mesmos setpoints de pressão MAP definidos em `Config.h`:
- `MAP_BAR_LOW_SETPOINT` (0.2 bar) - Verde
- `MAP_BAR_HIGH_SETPOINT` (0.4 bar) - Vermelho

Entre esses valores, o LED faz uma interpolação linear:
- Componente **RED**: Aumenta de 0 para 255
- Componente **GREEN**: Diminui de 255 para 0
- Componente **BLUE**: Permanece em 0

## Solução de Problemas

### LED não acende:
1. Verifique as conexões (DATA, 5V, GND)
2. Confirme que a biblioteca Adafruit NeoPixel está instalada
3. Verifique se o pino D2 não está sendo usado por outra função
4. Teste aumentar `LED_BRIGHTNESS` para 100 em Config.h

### LED com cor errada:
1. Alguns NeoPixels usam ordem diferente (RGB vs GRB)
2. Modifique em `StatusLed.h` linha 13:
   ```cpp
   // Tente trocar NEO_GRB por NEO_RGB se as cores estiverem trocadas
   : _strip(numPixels, pin, NEO_GRB + NEO_KHZ800) {}
   ```

### LED piscando/instável:
1. Adicione um capacitor 1000µF entre 5V e GND perto do LED
2. Adicione um resistor 470? entre D2 e DATA IN do LED

## Personalização

### Mudar cores:
Edite o método `updateFromPressure()` em `StatusLed.h`:
```cpp
// Exemplo: Azul para baixa carga, Amarelo para alta
if (pressureBar <= pLow) {
    red = 0; green = 0; blue = 255;  // Azul
} 
else if (pressureBar >= pHigh) {
    red = 255; green = 255; blue = 0;  // Amarelo
}
```

### Múltiplos LEDs:
Altere em `Config.h`:
```cpp
constexpr uint16_t STATUS_LED_COUNT = 8;  // Para tira com 8 LEDs
```

## Referências
- [Adafruit NeoPixel Library](https://github.com/adafruit/Adafruit_NeoPixel)
- [NeoPixel Überguide](https://learn.adafruit.com/adafruit-neopixel-uberguide)
