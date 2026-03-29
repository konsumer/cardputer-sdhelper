#pragma once
// CardputerMotion: IMU abstraction (accelerometer + gyroscope + temperature)
// Hardware: BMI270 via M5Unified's M5.Imu — Cardputer-Adv only.
// SDL/web: stub values, injectable from JS via Module.motionSet(ax,ay,az,gx,gy,gz).

#include <M5GFX.h>

#ifndef SDL_h_
#include "M5Cardputer.h"
#endif

#ifdef SDL_h_
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
// Must be at file scope — EM_JS cannot appear inside a function body
EM_JS(void, _cardputer_motion_js_init, (), {
  Module.motionSet = function(ax, ay, az, gx, gy, gz, temp) {
    Module._cardputerMotionSet(ax, ay, az, gx, gy, gz,
                               temp === undefined ? 25.0 : temp);
  };
});
#endif
#endif

// ---------------------------------------------------------------------------
// CardputerMotion class
// ---------------------------------------------------------------------------
class CardputerMotion {
public:
  void setup() {
#ifdef SDL_h_
    _instance = this;
#ifdef __EMSCRIPTEN__
    _cardputer_motion_js_init();
#endif
#endif
    // Hardware: M5.Imu is already initialised by M5Cardputer.begin() in
    // Cardputer::setup(). Nothing extra to do here.
  }

  void loop() {
#ifndef SDL_h_
    M5.Imu.update();
#endif
  }

  // Returns true if IMU hardware is present and functional.
  bool isEnabled() {
#ifdef SDL_h_
    return true; // stub always "enabled" so SDL apps can test IMU code paths
#else
    return M5.Imu.isEnabled();
#endif
  }

  // Accelerometer in G. Returns false if no data available.
  bool getAccel(float *x, float *y, float *z) {
#ifdef SDL_h_
    *x = _ax; *y = _ay; *z = _az;
    return true;
#else
    return M5.Imu.getAccel(x, y, z);
#endif
  }

  // Gyroscope in degrees/second. Returns false if no data available.
  bool getGyro(float *x, float *y, float *z) {
#ifdef SDL_h_
    *x = _gx; *y = _gy; *z = _gz;
    return true;
#else
    return M5.Imu.getGyro(x, y, z);
#endif
  }

  // Die temperature in °C. Returns false if not available.
  bool getTemp(float *t) {
#ifdef SDL_h_
    *t = _temp;
    return true;
#else
    return M5.Imu.getTemp(t);
#endif
  }

  // Inject simulated IMU data (SDL / test use only)
  static void setData(float ax, float ay, float az,
                      float gx, float gy, float gz,
                      float temp = 25.0f) {
#ifdef SDL_h_
    if (!_instance) return;
    _instance->_ax   = ax;  _instance->_ay   = ay;  _instance->_az   = az;
    _instance->_gx   = gx;  _instance->_gy   = gy;  _instance->_gz   = gz;
    _instance->_temp = temp;
#endif
  }

private:
#ifdef SDL_h_
  float _ax = 0.f, _ay = 0.f, _az = 1.f; // default: 1G down
  float _gx = 0.f, _gy = 0.f, _gz = 0.f;
  float _temp = 25.f;
  static CardputerMotion* _instance;
#endif
};

#ifdef SDL_h_
CardputerMotion* CardputerMotion::_instance = nullptr;

extern "C" void cardputerMotionSet(float ax, float ay, float az,
                                   float gx, float gy, float gz, float temp) {
  CardputerMotion::setData(ax, ay, az, gx, gy, gz, temp);
}
#endif
