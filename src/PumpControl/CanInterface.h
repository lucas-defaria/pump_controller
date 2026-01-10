#pragma once
#include <Arduino.h>
// Futuro: Implementar interface MCP2515 (SPI) para leitura CAN.
// Placeholder class.
class CanInterface {
public:
    void begin() {
        // TODO: iniciar SPI e MCP2515 (usar biblioteca adequada, ex: mcp_can)
    }
    void poll() {
        // TODO: leitura de frames CAN
    }
};
