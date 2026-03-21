#include "CanInterface.h"

CanInterface::CanInterface()
    : _can(Config::PIN_CAN_CS)
    , _data()
    , _initialized(false)
    , _msgCount(0)
    , _errCount(0)
{
    _data.transTemp_C = 0.0f;
    _data.transTemp_rawF = 0;
    _data.transTemp_valid = false;
    _data.transTemp_lastRxMs = 0;
}

bool CanInterface::begin() {
    if (_can.begin(MCP_ANY, CAN_500KBPS, MCP_8MHZ) == CAN_OK) {
        _can.setMode(MCP_NORMAL);
        _initialized = true;
        Serial.println(F("[CAN] MCP2515 initialized OK (500kbps, 8MHz)"));
    } else {
        _initialized = false;
        Serial.println(F("[CAN] *** MCP2515 INIT FAILED ***"));
    }
    return _initialized;
}

void CanInterface::poll() {
    if (!_initialized) return;

    uint32_t rxId;
    uint8_t  len;
    uint8_t  buf[8];

    // Drain all pending messages (same pattern as CAN_Gateway_EVO)
    while (_can.checkReceive() == CAN_MSGAVAIL) {
        if (_can.readMsgBuf(&rxId, &len, buf) == CAN_OK) {
            _msgCount++;
            // Mask out extended frame flag if present
            rxId &= 0x7FF;
            processFrame(rxId, len, buf);
        } else {
            _errCount++;
        }
    }
}

void CanInterface::processFrame(uint32_t id, uint8_t len, uint8_t* buf) {
    switch (id) {
        case Config::CAN_ID_GEARBOX:
            parseGearboxMsg(buf, len);
            break;
        // Future: add new CAN IDs here
        // case Config::CAN_ID_MAP_PRESSURE:
        //     parseMapMsg(buf, len);
        //     break;
        default:
            break;
    }
}

void CanInterface::parseGearboxMsg(uint8_t* buf, uint8_t len) {
    if (len <= Config::CAN_BYTE_TRANS_TEMP) return; // Message too short

    uint8_t rawF = buf[Config::CAN_BYTE_TRANS_TEMP];

    if (rawF >= Config::CAN_TEMP_RAW_MIN && rawF <= Config::CAN_TEMP_RAW_MAX) {
        _data.transTemp_rawF = rawF;
        _data.transTemp_C = fahrenheitToCelsius((float)rawF);
        _data.transTemp_valid = true;
        _data.transTemp_lastRxMs = millis();
    }
}

bool CanInterface::isTempFresh() const {
    if (!_data.transTemp_valid) return false;
    return (unsigned long)(millis() - _data.transTemp_lastRxMs) < MILLIS_COMPENSATED(Config::CAN_TEMP_TIMEOUT_MS);
}

float CanInterface::getTempC() const {
    if (isTempFresh()) {
        return _data.transTemp_C;
    }
    // Safety fallback: return high value to force fan to 100%
    return Config::TEMP_FAN_FULL_C + 10.0f;
}

float CanInterface::fahrenheitToCelsius(float f) {
    return (f - 32.0f) * 5.0f / 9.0f;
}
