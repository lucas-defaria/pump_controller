#pragma once
#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include "Config.h"

// -----------------------------------------------------------------------------
// StatusLed - Controle de LED NeoPixel para indica��o visual de corrente
// 
// NORMAL (0-40A):   Verde sólido com gradiente verde->vermelho conforme corrente sobe
// FAULT (>40A):     Pisca vermelho (1Hz)
// EMERGENCY (>45A): Pisca vermelho rápido (5Hz)
// 
// A cor do LED permite identificar visualmente o n�vel de corrente e 
// comportamento da prote��o sem precisar do monitor serial
// -----------------------------------------------------------------------------
class StatusLed {
public:
    explicit StatusLed(uint8_t pin, uint16_t numPixels = 1)
        : _strip(numPixels, pin, NEO_GRB + NEO_KHZ800)
        , _blinkState(false)
        , _lastBlinkMs(0) {}

    void begin() {
        _strip.begin();
        _strip.setBrightness(Config::LED_BRIGHTNESS);
        _strip.show(); // Initialize all pixels to 'off'
        _lastBlinkMs = millis();
    }

    // Atualiza LED para indicar desligamento por external safety
    // Pisca azul em 2Hz (250ms ON/OFF)
    void updateExternalSafetyBlink() {
        unsigned long now = millis();
        unsigned long blinkInterval = MILLIS_COMPENSATED(250); // 250ms com compensa��o (2Hz)
        
        if ((unsigned long)(now - _lastBlinkMs) >= blinkInterval) {
            _lastBlinkMs = now;
            _blinkState = !_blinkState;
            
            if (_blinkState) {
                setColor(0, 0, 255); // Azul brilhante
            } else {
                setColor(0, 0, 0);   // Apagado
            }
        }
    }
    
    // Atualiza a cor do LED baseado na corrente
    // current: corrente atual em Amperes
    // inFault: true se estiver em n�vel FAULT
    // inEmergency: true se estiver em n�vel EMERGENCY
    void updateFromCurrent(float current, bool inFault, bool inEmergency) {
        unsigned long now = millis();
        
        // EMERGENCY: Pisca vermelho r�pido (5Hz = 200ms per�odo = 100ms ON/OFF)
        if (inEmergency) {
            unsigned long blinkInterval = MILLIS_COMPENSATED(100); // 100ms com compensa��o
            if ((unsigned long)(now - _lastBlinkMs) >= blinkInterval) {
                _lastBlinkMs = now;
                _blinkState = !_blinkState;
                
                if (_blinkState) {
                    setColor(255, 0, 0); // Vermelho brilhante
                } else {
                    setColor(0, 0, 0);   // Apagado
                }
            }
            return;
        }
        
        // FAULT: Pisca vermelho lento (1Hz = 1000ms per�odo = 500ms ON/OFF)
        if (inFault) {
            unsigned long blinkInterval = MILLIS_COMPENSATED(500); // 500ms com compensa��o
            if ((unsigned long)(now - _lastBlinkMs) >= blinkInterval) {
                _lastBlinkMs = now;
                _blinkState = !_blinkState;
                
                if (_blinkState) {
                    setColor(255, 0, 0); // Vermelho
                } else {
                    setColor(0, 0, 0);   // Apagado
                }
            }
            return;
        }
        
        // NORMAL: gradiente verde->vermelho de 0A até FAULT
        uint8_t red, green, blue;

        float ratio = current / Config::CURRENT_THRESHOLD_FAULT; // 0..1
        if (ratio < 0.0f) ratio = 0.0f;
        if (ratio > 1.0f) ratio = 1.0f;

        // Interpolação: 0A = Verde (0,255,0) -> FAULT = Vermelho (255,0,0)
        // Passa por amarelo no meio
        if (ratio <= 0.5f) {
            float subRatio = ratio * 2.0f;
            red = (uint8_t)(subRatio * 255.0f);
            green = 255;
            blue = 0;
        } else {
            float subRatio = (ratio - 0.5f) * 2.0f;
            red = 255;
            green = (uint8_t)((1.0f - subRatio) * 255.0f);
            blue = 0;
        }
        
        setColor(red, green, blue);
    }

    // Define cor espec�fica RGB (0-255)
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
    bool _blinkState;              // Estado atual do pisca (ON/OFF)
    unsigned long _lastBlinkMs;    // �ltimo momento de mudan�a de estado do pisca
};
