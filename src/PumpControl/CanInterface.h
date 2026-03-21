#pragma once
#include <Arduino.h>
#include <mcp_can.h>
#include <SPI.h>
#include "Config.h"

// ---------------------------------------------------------------------------
// CAN Data Store - extensible struct for all parsed CAN values
// Add new fields here as new CAN messages are integrated
// ---------------------------------------------------------------------------
struct CanData {
    // Transmission oil temperature (from CAN ID 0x418, byte 2)
    float    transTemp_C;           // Converted temperature in Celsius
    uint8_t  transTemp_rawF;        // Raw Fahrenheit value from CAN (1-253)
    bool     transTemp_valid;       // True if at least one valid message received
    unsigned long transTemp_lastRxMs; // millis() timestamp of last valid reception

    // Future: MAP pressure via CAN
    // float    mapBar;
    // bool     mapValid;
    // unsigned long mapLastRxMs;
};

// ---------------------------------------------------------------------------
// CAN Interface - MCP2515 communication via SPI
// ---------------------------------------------------------------------------
class CanInterface {
public:
    CanInterface();

    // Initialize MCP2515. Returns true on success.
    bool begin();

    // Poll for pending CAN messages. Call every loop iteration.
    void poll();

    // Data access
    const CanData& getData() const { return _data; }
    bool isTempFresh() const;
    float getTempC() const;

    // Status
    bool isInitialized() const { return _initialized; }
    uint32_t getMessageCount() const { return _msgCount; }
    uint32_t getErrorCount() const { return _errCount; }

private:
    MCP_CAN  _can;
    CanData  _data;
    bool     _initialized;
    uint32_t _msgCount;
    uint32_t _errCount;

    void processFrame(uint32_t id, uint8_t len, uint8_t* buf);
    void parseGearboxMsg(uint8_t* buf, uint8_t len);

    static float fahrenheitToCelsius(float f);
};
