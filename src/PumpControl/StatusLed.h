#pragma once
#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include "Config.h"

// -----------------------------------------------------------------------------
// StatusLed - Controle de LED NeoPixel para indicação visual de estado
// 
// Verde: Baixa carga (MAP <= setpoint baixo)
// Vermelho: Alta carga (MAP >= setpoint alto)
// Interpolação gradual entre verde e vermelho para valores intermediários
// -----------------------------------------------------------------------------
class StatusLed {
public:
    explicit StatusLed(uint8_t pin, uint16_t numPixels = 1)
        : _strip(numPixels, pin, NEO_GRB + NEO_KHZ800) {}

    void begin() {
        _strip.begin();
        _strip.setBrightness(Config::LED_BRIGHTNESS);
        _strip.show(); // Initialize all pixels to 'off'
    }

    // Atualiza a cor do LED baseado na pressão MAP
    // pressureBar: pressão atual em bar (gauge)
    void updateFromPressure(float pressureBar) {
        const float pLow  = Config::MAP_BAR_LOW_SETPOINT;
        const float pHigh = Config::MAP_BAR_HIGH_SETPOINT;

        uint8_t red, green, blue;

        if (pressureBar <= pLow) {
            // Baixa carga - VERDE
            red = 0;
            green = 255;
            blue = 0;
        } 
        else if (pressureBar >= pHigh) {
            // Alta carga - VERMELHO
            red = 255;
            green = 0;
            blue = 0;
        } 
        else {
            // Interpolação linear entre verde e vermelho
            float ratio = (pressureBar - pLow) / (pHigh - pLow); // 0..1
            red = (uint8_t)(ratio * 255.0f);
            green = (uint8_t)((1.0f - ratio) * 255.0f);
            blue = 0;
        }

        setColor(red, green, blue);
    }

    // Define cor específica RGB (0-255)
    void setColor(uint8_t red, uint8_t green, uint8_t blue) {
        uint32_t color = _strip.Color(red, green, blue);
        for(uint16_t i = 0; i < _strip.numPixels(); i++) {
            _strip.setPixelColor(i, color);
        }
        _strip.show();
    }

    // Desliga o LED
    void off() {
        setColor(0, 0, 0);
    }

private:
    Adafruit_NeoPixel _strip;
};
