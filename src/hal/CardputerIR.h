#pragma once
// CardputerIR: IR transmitter abstraction
// Hardware: IRremote library, TX-only on GPIO 44 (Cardputer has no built-in RX).
// SDL/web: send calls are no-ops on native SDL; on Emscripten they fire
//          Module.irOut({ protocol, address, command }) if set.

#include <M5GFX.h>

#ifndef SDL_h_
// IRremote setup — must come before the include
#define DISABLE_CODE_FOR_RECEIVER
#define IR_TX_PIN 44
#include <IRremote.hpp>
#endif

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
// Must be at file scope — EM_JS cannot appear inside a function body
EM_JS(void, _cardputer_ir_js_init, (), {
  Module.irOut = null;
});
EM_JS(void, _cardputer_ir_js_out, (int protocol, int address, int command), {
  if (Module.irOut) Module.irOut({ protocol: protocol, address: address, command: command });
});
#endif

// ---------------------------------------------------------------------------
// CardputerIR class
// ---------------------------------------------------------------------------
class CardputerIR {
public:
  void setup() {
#ifndef SDL_h_
    IrSender.begin(DISABLE_LED_FEEDBACK);
    IrSender.setSendPin(IR_TX_PIN);
#endif
#ifdef __EMSCRIPTEN__
    _cardputer_ir_js_init();
#endif
  }

  void loop() {}

  // Generic send via protocol enum (mirrors IRremote's decode_type_t values)
  bool send(uint8_t protocol, uint16_t address, uint16_t command, uint8_t repeats = 0) {
#ifndef SDL_h_
    IrSender.write((decode_type_t)protocol, address, command, repeats);
#endif
    _notify(protocol, address, command);
    return true;
  }

  bool sendNEC(uint16_t address, uint8_t command, uint8_t repeats = 0) {
#ifndef SDL_h_
    IrSender.sendNEC(address, command, repeats);
#endif
    _notify(3 /*NEC*/, address, command);
    return true;
  }

  bool sendSamsung(uint16_t address, uint8_t command, uint8_t repeats = 0) {
#ifndef SDL_h_
    IrSender.sendSamsung(address, command, repeats);
#endif
    _notify(10 /*SAMSUNG*/, address, command);
    return true;
  }

  bool sendSony(uint16_t address, uint8_t command, uint8_t repeats = 0, uint8_t bits = 12) {
#ifndef SDL_h_
    IrSender.sendSony(address, command, repeats, bits);
#endif
    _notify(12 /*SONY*/, address, command);
    return true;
  }

  bool sendRC5(uint8_t address, uint8_t command, uint8_t repeats = 0, bool toggle = false) {
#ifndef SDL_h_
    IrSender.sendRC5(address, command, repeats, toggle);
#endif
    _notify(7 /*RC5*/, address, command);
    return true;
  }

  bool sendRC6(uint8_t address, uint8_t command, uint8_t repeats = 0, bool toggle = false) {
#ifndef SDL_h_
    IrSender.sendRC6(address, command, repeats, toggle);
#endif
    _notify(8 /*RC6*/, address, command);
    return true;
  }

  bool sendPanasonic(uint16_t address, uint8_t command, uint8_t repeats = 0) {
#ifndef SDL_h_
    IrSender.sendPanasonic(address, command, repeats);
#endif
    _notify(6 /*PANASONIC*/, address, command);
    return true;
  }

  bool sendLG(uint8_t address, uint16_t command, uint8_t repeats = 0) {
#ifndef SDL_h_
    IrSender.sendLG(address, command, repeats);
#endif
    _notify(5 /*LG*/, address, command);
    return true;
  }

  bool sendDenon(uint8_t address, uint8_t command, uint8_t repeats = 0) {
#ifndef SDL_h_
    IrSender.sendDenon(address, command, repeats);
#endif
    _notify(2 /*DENON*/, address, command);
    return true;
  }

  bool sendSharp(uint8_t address, uint8_t command, uint8_t repeats = 0) {
#ifndef SDL_h_
    IrSender.sendSharp(address, command, repeats);
#endif
    _notify(13 /*SHARP*/, address, command);
    return true;
  }

  bool sendJVC(uint8_t address, uint8_t command, uint8_t repeats = 0) {
#ifndef SDL_h_
    IrSender.sendJVC(address, command, repeats);
#endif
    _notify(4 /*JVC*/, address, command);
    return true;
  }

  // Raw timing burst (microseconds, mark-first, no leading space).
  // On Emscripten, irOut receives { protocol: 0, address: 0, command: 0 }
  // since raw timings have no protocol structure to report.
  bool sendRaw(const uint16_t *buf, uint16_t len, uint8_t freqKHz = 38) {
#ifndef SDL_h_
    IrSender.sendRaw(buf, len, freqKHz);
#endif
    _notify(0 /*UNKNOWN*/, 0, 0);
    return true;
  }

private:
  static void _notify(uint8_t protocol, uint16_t address, uint16_t command) {
#ifdef __EMSCRIPTEN__
    _cardputer_ir_js_out(protocol, address, command);
#endif
  }
};
