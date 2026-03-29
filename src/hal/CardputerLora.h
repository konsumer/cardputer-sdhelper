#pragma once
// CardputerLora: LoRa radio abstraction
// Abstracts native (SDL2), web (Emscripten+SDL2), and cardputer (ESP32-S3/M5)

#include <M5GFX.h>
#include <functional>

#ifdef SDL_h_
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
// Must be at file scope — EM_JS cannot appear inside a function body
EM_JS(void, _cardputer_lora_js_init, (), {
  Module.loraInject = function(bytes) {
    var buf = Module._malloc(bytes.length);
    Module.HEAPU8.set(bytes, buf);
    Module._cardputerLoraInject(buf, bytes.length);
    Module._free(buf);
  };
  Module.loraOut = null;
});
#endif
#else
#include <RadioLib.h>
#define LORA_BW           125.0f
#define LORA_SF           12
#define LORA_CR           5
#define LORA_FREQ         868.0
#define LORA_SYNC_WORD    0x34
#define LORA_TX_POWER     22
#define LORA_PREAMBLE_LEN 20
#endif

// ---------------------------------------------------------------------------
// CardputerLora class
// ---------------------------------------------------------------------------
class CardputerLora {
public:
  // Called with received packet data and length
  std::function<void(uint8_t*, int)> onMessage;

  void setup() {
#ifndef SDL_h_
    _module = new Module(GPIO_NUM_5, GPIO_NUM_4, GPIO_NUM_3, GPIO_NUM_6);
    _sx = new SX1262(_module);
    if (_sx->begin(LORA_FREQ, LORA_BW, LORA_SF, LORA_CR, LORA_SYNC_WORD,
                   LORA_TX_POWER, LORA_PREAMBLE_LEN, 3.0, true) == RADIOLIB_ERR_NONE) {
      _sx->setCurrentLimit(140);
      _sx->setPacketReceivedAction(_rx_isr);
      _sx->setPacketSentAction(_tx_isr);
      _sx->startReceive();
      _ok = true;
    }
#endif
#ifdef __EMSCRIPTEN__
    _cardputer_lora_js_init();
#endif
    _instance = this;
  }

  void loop() {
#ifdef SDL_h_
    if (_pending && onMessage) {
      _pending = false;
      onMessage(_buf, _len);
    }
#else
    if (_rx_flag && _sx) {
      _rx_flag = false;
      int len = _sx->getPacketLength();
      if (len > 0 && onMessage) {
        uint8_t buf[256];
        if (len > (int)sizeof(buf)) len = sizeof(buf);
        if (_sx->readData(buf, len) == RADIOLIB_ERR_NONE) onMessage(buf, len);
      }
      _sx->startReceive();
    }
    if (_tx_flag && _sx) {
      _tx_flag = false;
      _sx->startReceive();
    }
#endif
  }

  bool sendMessage(uint8_t *data, int len) {
#ifdef SDL_h_
#ifdef __EMSCRIPTEN__
    EM_ASM({
      if (Module.loraOut) Module.loraOut(Array.from(Module.HEAPU8.subarray($0, $0 + $1)));
    }, data, len);
#endif
    return true;
#else
    if (!_ok || !_sx) return false;
    return _sx->transmit(data, len) == RADIOLIB_ERR_NONE;
#endif
  }

  // Convenience overload for null-terminated strings
  bool sendMessage(const char *str) {
    return sendMessage((uint8_t*)str, strlen(str));
  }

private:
#ifdef SDL_h_
  uint8_t _buf[256] = {};
  int     _len      = 0;
  bool    _pending  = false;
public:
  // Called from JS (Emscripten) or test harness to inject a received packet
  static void inject(uint8_t *data, int len) {
    if (!_instance) return;
    if (len > (int)sizeof(_instance->_buf)) len = sizeof(_instance->_buf);
    memcpy(_instance->_buf, data, len);
    _instance->_len = len;
    _instance->_pending = true;
  }
private:
#else
  // Module and radio are heap-allocated in setup() once GPIO constants are valid
  Module*  _module  = nullptr;
  SX1262*  _sx      = nullptr;
  bool     _ok      = false;
  static volatile bool _rx_flag;
  static volatile bool _tx_flag;

  ICACHE_RAM_ATTR static void _rx_isr() { _rx_flag = true; }
  ICACHE_RAM_ATTR static void _tx_isr() { _tx_flag = true; }
#endif

  static CardputerLora* _instance;
};

#ifndef SDL_h_
volatile bool CardputerLora::_rx_flag = false;
volatile bool CardputerLora::_tx_flag = false;
#endif
CardputerLora* CardputerLora::_instance = nullptr;

#ifdef SDL_h_
// C entry point for Emscripten / JS injection
extern "C" void cardputerLoraInject(uint8_t *data, int len) {
  CardputerLora::inject(data, len);
}
#endif
