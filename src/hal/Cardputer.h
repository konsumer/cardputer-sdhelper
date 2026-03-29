#pragma once
// Cardputer: basic screen and input, extends M5Canvas for direct draw API
// Abstracts native (SDL2), web (Emscripten+SDL2), and cardputer (ESP32-S3/M5)

#include <M5GFX.h>

// Key definitions (match M5Cardputer Keyboard_def.h values)
#ifndef KEY_LEFT_CTRL
#define KEY_LEFT_CTRL  0x80
#define KEY_LEFT_SHIFT 0x81
#define KEY_LEFT_ALT   0x82
#define KEY_FN         0xff
#define KEY_OPT        0x00
#define KEY_BACKSPACE  0x2a
#define KEY_TAB        0x2b
#define KEY_ENTER      0x28
#define KEY_ESC        0x29
// Arrow keys (HID usage codes)
// On Cardputer: Fn+; = up, Fn+, = left, Fn+. = down, Fn+/ = right
#define KEY_RIGHT      0x4f
#define KEY_LEFT       0x50
#define KEY_DOWN       0x51
#define KEY_UP         0x52
#endif

#ifndef SDL_h_
#include "M5Cardputer.h"
#endif

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
// Must be at file scope — EM_JS cannot appear inside a function body
EM_JS(void, _cardputer_power_js_init, (), {
  Module.batteryLevel   = 100;
  Module.batteryVoltage = 4200;
  Module.isCharging     = false;
});
#endif

// ---------------------------------------------------------------------------
// Cardputer — extends M5Canvas so it IS the drawable surface
// ---------------------------------------------------------------------------
class Cardputer : public M5Canvas {
public:
  // The canvas is always constructed with a pointer to _display.
  // On SDL _display is used directly; on hardware _hw_display is set in setup()
  // to point at M5Cardputer.Display and used for all subsequent calls.
  Cardputer() : M5Canvas(&_display) {}

  void setup() {
#ifndef SDL_h_
    auto cfg = M5.config();
    M5Cardputer.begin(cfg, true);
    // Store pointer to the real hardware display.
    _hw_display = &M5Cardputer.Display;
    // Drive GPIO 5 high so SD/LoRa don't conflict on startup
    pinMode(5, OUTPUT);
    digitalWrite(5, HIGH);
#else
    _display.init();
    // Require Ctrl+Shift for Panel_sdl rotate/scale shortcuts so normal
    // key presses (L, R, 1-6) don't accidentally trigger them.
    lgfx::Panel_sdl::setShortcutKeymod((SDL_Keymod)(KMOD_CTRL | KMOD_SHIFT));
#ifdef __EMSCRIPTEN__
    _cardputer_power_js_init();
#endif
#endif
    setColorDepth(16);
    createSprite(_disp().width(), _disp().height());
  }

  void loop() {
#ifndef SDL_h_
    M5Cardputer.update();
#endif
    pushSprite(&_disp(), 0, 0);
  }

  // Returns true while the key is held.
  // c can be a printable char ('a','A','1','!',...) or a KEY_* constant.
  bool isKeyPressed(char c) {
#ifdef SDL_h_
    const uint8_t* keys = SDL_GetKeyboardState(NULL);

    if ((uint8_t)c == KEY_ESC) return keys[SDL_SCANCODE_ESCAPE];

    bool shift_held = keys[SDL_SCANCODE_LSHIFT] || keys[SDL_SCANCODE_RSHIFT];

    switch ((uint8_t)c) {
      case KEY_BACKSPACE:  return keys[SDL_SCANCODE_BACKSPACE];
      case KEY_TAB:        return keys[SDL_SCANCODE_TAB];
      case KEY_ENTER:      return keys[SDL_SCANCODE_RETURN];
      case KEY_LEFT_CTRL:  return keys[SDL_SCANCODE_LCTRL]  || keys[SDL_SCANCODE_RCTRL];
      case KEY_LEFT_SHIFT: return keys[SDL_SCANCODE_LSHIFT] || keys[SDL_SCANCODE_RSHIFT];
      case KEY_LEFT_ALT:   return keys[SDL_SCANCODE_LALT]   || keys[SDL_SCANCODE_RALT];
      case KEY_RIGHT:      return keys[SDL_SCANCODE_RIGHT];
      case KEY_LEFT:       return keys[SDL_SCANCODE_LEFT];
      case KEY_DOWN:       return keys[SDL_SCANCODE_DOWN];
      case KEY_UP:         return keys[SDL_SCANCODE_UP];
      case KEY_OPT:        return false;
      case KEY_FN:         return false;
      default:             break;
    }

    // Shifted symbols
    switch (c) {
      case '!': return keys[SDL_SCANCODE_1]            && shift_held;
      case '@': return keys[SDL_SCANCODE_2]            && shift_held;
      case '#': return keys[SDL_SCANCODE_3]            && shift_held;
      case '$': return keys[SDL_SCANCODE_4]            && shift_held;
      case '%': return keys[SDL_SCANCODE_5]            && shift_held;
      case '^': return keys[SDL_SCANCODE_6]            && shift_held;
      case '&': return keys[SDL_SCANCODE_7]            && shift_held;
      case '*': return keys[SDL_SCANCODE_8]            && shift_held;
      case '(': return keys[SDL_SCANCODE_9]            && shift_held;
      case ')': return keys[SDL_SCANCODE_0]            && shift_held;
      case '_': return keys[SDL_SCANCODE_MINUS]        && shift_held;
      case '+': return keys[SDL_SCANCODE_EQUALS]       && shift_held;
      case '~': return keys[SDL_SCANCODE_GRAVE]        && shift_held;
      case '{': return keys[SDL_SCANCODE_LEFTBRACKET]  && shift_held;
      case '}': return keys[SDL_SCANCODE_RIGHTBRACKET] && shift_held;
      case '|': return keys[SDL_SCANCODE_BACKSLASH]    && shift_held;
      case ':': return keys[SDL_SCANCODE_SEMICOLON]    && shift_held;
      case '"': return keys[SDL_SCANCODE_APOSTROPHE]   && shift_held;
      case '<': return keys[SDL_SCANCODE_COMMA]        && shift_held;
      case '>': return keys[SDL_SCANCODE_PERIOD]       && shift_held;
      case '?': return keys[SDL_SCANCODE_SLASH]        && shift_held;
      default:  break;
    }

    // Printable ASCII
    SDL_Keycode kc;
    if (c >= 'A' && c <= 'Z') {
      kc = (SDL_Keycode)((c - 'A') + 'a');
      return keys[SDL_GetScancodeFromKey(kc)] && shift_held;
    }
    kc = (SDL_Keycode)c;
    SDL_Scancode sc = SDL_GetScancodeFromKey(kc);
    if (sc == SDL_SCANCODE_UNKNOWN) return false;
    return keys[sc];

#else
    // Cardputer hardware path
    bool fn = M5Cardputer.Keyboard.keysState().fn;
    switch ((uint8_t)c) {
      case KEY_ESC:   return fn && M5Cardputer.Keyboard.isKeyPressed('`');
      case KEY_UP:    return fn && M5Cardputer.Keyboard.isKeyPressed(';');
      case KEY_LEFT:  return fn && M5Cardputer.Keyboard.isKeyPressed(',');
      case KEY_RIGHT: return fn && M5Cardputer.Keyboard.isKeyPressed('/');
      case KEY_DOWN:  return fn && M5Cardputer.Keyboard.isKeyPressed('.');
      case '`': return !fn && M5Cardputer.Keyboard.isKeyPressed('`');
      case ';': return !fn && M5Cardputer.Keyboard.isKeyPressed(';');
      case ',': return !fn && M5Cardputer.Keyboard.isKeyPressed(',');
      case '/': return !fn && M5Cardputer.Keyboard.isKeyPressed('/');
      case '.': return !fn && M5Cardputer.Keyboard.isKeyPressed('.');
      default:  return M5Cardputer.Keyboard.isKeyPressed(c);
    }
#endif
  }

  // true only if actively charging (false for unknown / discharging)
  // Note: Cardputer has no charging detection circuit — always returns false on hardware.
  bool isCharging() {
#ifdef SDL_h_
#ifdef __EMSCRIPTEN__
    return (bool)EM_ASM_INT({ return Module.isCharging ? 1 : 0; });
#else
    return false;
#endif
#else
    return M5Cardputer.Power.isCharging() == m5::Power_Class::is_charging;
#endif
  }

  // Battery level 0–100 %, derived from voltage.
  // Uses the same 3300–4150 mV curve as M5Unified.
  int batteryLevel() {
#ifdef SDL_h_
#ifdef __EMSCRIPTEN__
    return EM_ASM_INT({ return Module.batteryLevel | 0; });
#else
    return 100;
#endif
#else
    int mv = M5Cardputer.Power.getBatteryVoltage();
    if (mv <= 0)    return -1;   // ADC error
    int level = (mv - 3300) * 100 / (4150 - 3300);
    if (level < 0)   level = 0;
    if (level > 100) level = 100;
    return level;
#endif
  }

  // Battery voltage in millivolts (raw ADC × 2 on Cardputer).
  int batteryVoltage() {
#ifdef SDL_h_
#ifdef __EMSCRIPTEN__
    return EM_ASM_INT({ return Module.batteryVoltage | 0; });
#else
    return 4200;
#endif
#else
    return M5Cardputer.Power.getBatteryVoltage();
#endif
  }

private:
  // SDL: real display object (canvas constructor takes &_display, address stable)
  // Hardware: unused placeholder; actual display is accessed via _hw_display
  M5GFX    _display;
#ifndef SDL_h_
  M5GFX*   _hw_display;
#endif

  // Returns the correct display reference for either target
  M5GFX& _disp() {
#ifdef SDL_h_
    return _display;
#else
    return *_hw_display;
#endif
  }
};

// ---------------------------------------------------------------------------
// Platform entry points — defined here so any translation unit that includes
// this header gets a main() / app_main() wired up to Arduino-style setup/loop.
// ---------------------------------------------------------------------------
void setup(void);
void loop(void);

#if defined(__EMSCRIPTEN__)
#include <emscripten.h>
static void _em_loop(void) {
  loop();
  lgfx::Panel_sdl::loop();
}
int main(int, char**) {
  lgfx::Panel_sdl::setup();
  setup();
  emscripten_set_main_loop(_em_loop, 0, 1);
  return 0;
}

#elif defined(SDL_h_)
__attribute__((weak))
int user_func(bool* running) {
  setup();
  while (*running) loop();
  return 0;
}
int main(int, char**) {
  return lgfx::Panel_sdl::main(user_func, 128);
}

#elif defined(ESP_PLATFORM) && !defined(ARDUINO)
extern "C" {
  int app_main(int, char**) {
    setup();
    while (true) loop();
    return 0;
  }
}
#endif
