#pragma once
// CardputerGps: GPS abstraction
// Abstracts native (SDL2), web (Emscripten+SDL2), and cardputer (ESP32-S3/M5)

#include <M5GFX.h>

#ifdef SDL_h_
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
// Must be at file scope — EM_JS cannot appear inside a function body
EM_JS(void, _cardputer_gps_js_init, (), {
  Module.satelliteCount = 0;
  Module.gpsSet = function(lat, lng) {
    Module._cardputerGpsSet(lat, lng);
  };
});
#endif
#else
#include <MultipleSatellite.h>
#endif

// ---------------------------------------------------------------------------
// CardputerGps class
// ---------------------------------------------------------------------------
class CardputerGps {
public:
  void setup() {
#ifdef SDL_h_
#ifdef __EMSCRIPTEN__
    _cardputer_gps_js_init();
#endif
    _instance = this;
#else
    _gps.begin();
    _gps.setSystemBootMode(BOOT_HOST_START);
    _started = true;
#endif
  }

  void loop() {
#ifndef SDL_h_
    if (_started) _gps.updateGPS();
#endif
  }

  // Returns true and fills lat/lng if a valid fix is available
  bool getLocation(double *lat, double *lng) {
#ifdef SDL_h_
    if (!_valid) return false;
    *lat = _lat;
    *lng = _lng;
    return true;
#else
    if (!_started || !_gps.location.isValid()) return false;
    *lat = _gps.location.lat();
    *lng = _gps.location.lng();
    return true;
#endif
  }

  int satellites() {
#ifdef SDL_h_
#ifdef __EMSCRIPTEN__
    return EM_ASM_INT({ return Module.satelliteCount || 0; });
#else
    return 0;
#endif
#else
    return _started ? (int)_gps.satellites.value() : 0;
#endif
  }

private:
#ifdef SDL_h_
  double _lat   = 0.0;
  double _lng   = 0.0;
  bool   _valid = false;
public:
  // Called from JS / test harness to inject a GPS fix
  static void setLocation(double lat, double lng) {
    if (!_instance) return;
    _instance->_lat   = lat;
    _instance->_lng   = lng;
    _instance->_valid = true;
  }
private:
#else
  MultipleSatellite _gps{Serial1, 115200, SERIAL_8N1, 15, 13};
  bool _started = false;
#endif

  static CardputerGps* _instance;
};

CardputerGps* CardputerGps::_instance = nullptr;

#ifdef SDL_h_
// C entry point for Emscripten / JS injection
extern "C" void cardputerGpsSet(double lat, double lng) {
  CardputerGps::setLocation(lat, lng);
}
#endif
