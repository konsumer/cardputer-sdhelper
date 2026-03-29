#include "hal/Cardputer.h"
#include "hal/CardputerSd.h"
#include "hal/CardputerLora.h"
#include "hal/CardputerGps.h"
#include "hal/CardputerMotion.h"
#include "hal/CardputerIR.h"

Cardputer      c;
CardputerSd    sd;
CardputerLora  lora;
CardputerGps   gps;
CardputerMotion motion;
CardputerIR    ir;

// wrap-around counter
class Counter {
public:
  int val;
  int size;
  Counter(int size = 1, int val = 0) : size(size), val(val) {}
  int increment(int n = 1) { return val = ((val + n) % size + size) % size; }
  int decrement(int n = 1) { return val = ((val - n) % size + size) % size; }

  operator int() const { return val; }

  Counter& operator++()    { val = ((val + 1) % size + size) % size; return *this; }
  Counter& operator--()    { val = ((val - 1) % size + size) % size; return *this; }
  Counter  operator++(int) { Counter tmp = *this; ++(*this); return tmp; }
  Counter  operator--(int) { Counter tmp = *this; --(*this); return tmp; }
  Counter& operator+=(int n) { val = ((val + n) % size + size) % size; return *this; }
  Counter& operator-=(int n) { val = ((val - n) % size + size) % size; return *this; }
};


class Tab {
public:
  const char* name;
  Tab(const char* name) : name(name) {}

  virtual void setup()  {}
  virtual void update() {}
  virtual ~Tab() {}
};


class TabKeys : public Tab {
public:
  TabKeys() : Tab("KEY") {}

  void setup() override {
    _cx = c.width()  / 2;
    _cy = c.height() / 2 - 15;
  }

  void update() override {
    c.clear(BLUE);
    c.setTextSize(4.0);
    c.setTextColor(WHITE);

    static const struct { uint8_t key; const char* label; } specials[] = {
      { KEY_UP,        "UP"        },
      { KEY_DOWN,      "DOWN"      },
      { KEY_LEFT,      "LEFT"      },
      { KEY_RIGHT,     "RIGHT"     },
      { KEY_LEFT_CTRL, "CTRL"      },
      { KEY_LEFT_ALT,  "ALT"       },
      { KEY_OPT,       "OPT"       },
      { KEY_BACKSPACE, "BACKSPACE" },
      { KEY_TAB,       "TAB"       },
      { KEY_ENTER,     "ENTER"     },
      { KEY_ESC,       "ESC"       },
      { ' ',           "SPACE"     },
    };
    for (auto& s : specials) {
      if (c.isKeyPressed(s.key)) {
        c.drawCenterString(s.label, _cx, _cy);
        return;
      }
    }

    static const char printable[] =
      "abcdefghijklmnopqrstuvwxyz"
      "1234567890"
      "`-=[]\\;',./"
      "~_+{}|:\"<>?!@#$%^&*()";
    for (char ch : printable) {
      if (c.isKeyPressed(ch)) {
        char buf[2] = { ch, '\0' };
        c.drawCenterString(buf, _cx, _cy);
        return;
      }
    }
  }

private:
  int _cx, _cy;
};


class TabGraphics : public Tab {
public:
  TabGraphics() : Tab("GFX") {}

  void setup() override {}

  void update() override {
    int w = c.width();
    int h = c.height() - 16;

    auto rc = []() -> uint32_t {
      return c.color888(rand() % 256, rand() % 256, rand() % 256);
    };

    switch (rand() % 5) {
      case 0: c.fillCircle(rand() % w, rand() % h, rand() % 20 + 4, rc()); break;
      case 1: c.fillRect(rand() % w, rand() % h, rand() % 40 + 4, rand() % 40 + 4, rc()); break;
      case 2: c.fillRoundRect(rand() % w, rand() % h, rand() % 40 + 8, rand() % 30 + 8, 4, rc()); break;
      case 3: c.fillTriangle(rand() % w, rand() % h, rand() % w, rand() % h, rand() % w, rand() % h, rc()); break;
      case 4: c.drawLine(rand() % w, rand() % h, rand() % w, rand() % h, rc()); break;
    }
  }
};


class TabCounter : public Tab {
public:
  TabCounter() : Tab("SD") {}

  void setup() override {
    if (!sd.ok) return;
    char buf[32] = "0";
    int n = sd.read("/counter.txt", (uint8_t*)buf, sizeof(buf) - 1);
    if (n > 0) buf[n] = '\0';
    _count = atoi(buf) + 1;
    char out[32];
    int len = snprintf(out, sizeof(out), "%d", _count);
    sd.write("/counter.txt", (const uint8_t*)out, len);
  }

  void update() override {
    c.clear(BLACK);
    c.setTextColor(WHITE);

    if (!sd.ok) {
      c.setTextSize(1.5);
      c.drawCenterString("No SD card", c.width() / 2, c.height() / 2 - 16);
      return;
    }

    c.setTextSize(1);
    c.drawCenterString("Times you have loaded this tab:", c.width() / 2, c.height() / 2 - 40);

    char buf[32];
    snprintf(buf, sizeof(buf), "%d", _count);
    c.setTextSize(4.0);
    c.drawCenterString(buf, c.width() / 2, c.height() / 2 - 24);
  }

private:
  int _count = 0;
};


class TabRadio : public Tab {
public:
  TabRadio() : Tab("RAD") {}

  void setup() override {
    _logCount = 0;
    lora.onMessage = [this](uint8_t *data, int len) {
      if (_logCount < LOG_LINES) _logCount++;
      for (int i = 0; i < _logCount - 1; i++) memcpy(_log[i], _log[i + 1], LOG_WIDTH);
      int n = len < LOG_WIDTH - 1 ? len : LOG_WIDTH - 1;
      memcpy(_log[_logCount - 1], data, n);
      _log[_logCount - 1][n] = '\0';
    };
  }

  void update() override {
    int cx      = c.width() / 2;
    int h       = c.height() - 16;

    c.clear(BLACK);
    c.setTextSize(1.0);

    // --- GPS ---
    double lat, lng;
    int sats = gps.satellites();
    if (gps.getLocation(&lat, &lng)) {
      char buf[48];
      snprintf(buf, sizeof(buf), "%.5f, %.5f", lat, lng);
      c.setTextColor(TFT_GREEN);
      c.drawCenterString(buf, cx, 2);
    } else {
      char buf[32];
      snprintf(buf, sizeof(buf), "GPS: no fix (sats:%d)", sats);
      c.setTextColor(sats > 0 ? TFT_YELLOW : TFT_DARKGREY);
      c.drawCenterString(buf, cx, 2);
    }

    // --- LoRa log ---
    int lineH = 12;
    int logY  = 16;
    c.setTextColor(WHITE);
    for (int i = 0; i < _logCount; i++) {
      c.drawString(_log[i], 2, logY + i * lineH);
    }

    if (_sentTimer > 0) {
      --_sentTimer;
      c.setTextColor(TFT_GREEN);
      c.drawCenterString("Sent LoRa!", cx, h - 26);
    }

    c.setTextColor(TFT_YELLOW);
    c.drawCenterString("[ENTER] send ping", cx, h - 14);

    bool enter = c.isKeyPressed(KEY_ENTER);
    if (enter && !_prevEnter) {
      lora.sendMessage("ping");
      _sentTimer = 30;
    }
    _prevEnter = enter;
  }

private:
  static const int LOG_LINES = 6;
  static const int LOG_WIDTH = 40;
  char _log[LOG_LINES][LOG_WIDTH];
  int  _logCount  = 0;
  int  _sentTimer = 0;
  bool _prevEnter = false;
};


class TabMotion : public Tab {
public:
  TabMotion() : Tab("ACC") {}

  void setup() override {}

  void update() override {
    int cx = c.width()  / 2;
    int cy = c.height() / 2 - 8;
    int w  = c.width();
    int h  = c.height() - 16;

    c.clear(BLACK);
    c.setTextSize(1.0);

    if (!motion.isEnabled()) {
      c.setTextColor(TFT_DARKGREY);
      c.drawCenterString("No IMU (Cardputer-Adv only)", cx, cy);
      return;
    }

    float ax, ay, az, gx, gy, gz, temp;
    motion.getAccel(&ax, &ay, &az);
    motion.getGyro(&gx, &gy, &gz);
    motion.getTemp(&temp);

    // --- Accel bar graph — horizontal bars for X/Y/Z, range -2G..+2G ---
    const float SCALE = 2.0f;
    const int barW    = w / 2 - 8;
    const int barH    = 10;
    const int col1    = 4;
    const int col2    = w / 2 + 4;
    int y             = 4;

    auto drawBar = [&](int bx, int by, float val, float scale, uint32_t col, const char* label) {
      c.setTextColor(TFT_DARKGREY);
      c.drawString(label, bx, by);
      int bary = by + 11;
      c.drawRect(bx, bary, barW, barH, TFT_DARKGREY);
      int filled = (int)((val / scale + 1.0f) * 0.5f * barW);
      if (filled < 0)    filled = 0;
      if (filled > barW) filled = barW;
      c.fillRect(bx, bary, filled, barH, col);
      // centre marker
      c.drawFastVLine(bx + barW / 2, bary, barH, WHITE);
    };

    c.setTextColor(WHITE);
    c.drawString("Accel (G)", col1, y);
    c.drawString("Gyro (dps)", col2, y);
    y += 10;

    drawBar(col1, y,      ax, SCALE, TFT_RED,   "X");
    drawBar(col2, y,      gx, 250.f, TFT_RED,   "X");
    y += 24;
    drawBar(col1, y,      ay, SCALE, TFT_GREEN, "Y");
    drawBar(col2, y,      gy, 250.f, TFT_GREEN, "Y");
    y += 24;
    drawBar(col1, y,      az, SCALE, TFT_BLUE,  "Z");
    drawBar(col2, y,      gz, 250.f, TFT_BLUE,  "Z");
    y += 24;

    // --- Numeric readout ---
    char buf[48];
    snprintf(buf, sizeof(buf), "%.2f %.2f %.2f G", ax, ay, az);
    c.setTextColor(TFT_LIGHTGREY);
    c.drawCenterString(buf, cx, y);
    y += 12;

    snprintf(buf, sizeof(buf), "%.1f %.1f %.1f dps", gx, gy, gz);
    c.drawCenterString(buf, cx, y);
    y += 12;

    snprintf(buf, sizeof(buf), "Temp: %.1f C", temp);
    c.drawCenterString(buf, cx, y);
  }
};


class TabIR : public Tab {
public:
  TabIR() : Tab("IR") {}

  void setup() override {
    _sent = false;
  }

  void update() override {
    int cx = c.width()  / 2;
    int h  = c.height() - 16;

    c.clear(BLACK);
    c.setTextSize(1.0);

    // --- Protocol selector ---
    static const struct { const char* name; uint8_t proto; } protocols[] = {
      { "NEC",      3  },
      { "Samsung",  10 },
      { "Sony",     12 },
      { "RC5",      7  },
      { "RC6",      8  },
      { "Panasonic",6  },
    };
    static const int NPROTO = sizeof(protocols) / sizeof(protocols[0]);

    // Up/Down selects protocol (edge-triggered)
    bool up   = c.isKeyPressed(KEY_UP);
    bool down = c.isKeyPressed(KEY_DOWN);
    if (up   && !_prevUp)   _proto = (_proto - 1 + NPROTO) % NPROTO;
    if (down && !_prevDown) _proto = (_proto + 1)          % NPROTO;
    _prevUp   = up;
    _prevDown = down;

    // Draw protocol list
    c.setTextColor(TFT_DARKGREY);
    c.drawString("Protocol:", 4, 4);
    for (int i = 0; i < NPROTO; i++) {
      bool sel = i == _proto;
      c.setTextColor(sel ? TFT_YELLOW : TFT_DARKGREY);
      c.drawString(protocols[i].name, 4, 16 + i * 12);
      if (sel) c.drawString("<", 70, 16 + i * 12);
    }

    // Address / command display
    char buf[32];
    snprintf(buf, sizeof(buf), "Addr: 0x%04X", _addr);
    c.setTextColor(WHITE);
    c.drawString(buf, cx + 4, 16);
    snprintf(buf, sizeof(buf), "Cmd:  0x%02X",  _cmd);
    c.drawString(buf, cx + 4, 28);

    // Send on ENTER
    if (c.isKeyPressed(KEY_ENTER) && !_prevEnter) {
      ir.send(protocols[_proto].proto, _addr, _cmd);
      _sent       = true;
      _sentTimer  = 30; // frames to show flash
    }
    _prevEnter = c.isKeyPressed(KEY_ENTER);

    // Sent flash
    if (_sentTimer > 0) {
      --_sentTimer;
      c.setTextColor(TFT_GREEN);
      snprintf(buf, sizeof(buf), "Sent %s!", protocols[_proto].name);
      c.drawCenterString(buf, cx, h - 26);
    }

    c.setTextColor(TFT_YELLOW);
    c.drawCenterString("[UP/DN] proto  [ENTER] send", cx, h - 14);
  }

private:
  int  _proto     = 0;
  uint16_t _addr  = 0x1111;
  uint8_t  _cmd   = 0x34;
  bool _sent      = false;
  int  _sentTimer = 0;
  bool _prevUp    = false;
  bool _prevDown  = false;
  bool _prevEnter = false;
};


class TabBattery : public Tab {
public:
  TabBattery() : Tab("BAT") {}

  void setup() override {}

  void update() override {
    int cx      = c.width()  / 2;
    int cy      = c.height() / 2;
    int w       = c.width();

    bool  charging = c.isCharging();
    int   level    = c.batteryLevel();
    int   voltage  = c.batteryVoltage();

    c.clear(BLACK);
    c.setTextSize(1.0);

    // --- Battery icon ---
    const int bw = 80, bh = 34;
    const int bx = cx - bw / 2, by = cy - bh / 2 - 14;
    const int nub = 5;
    // Outer rect
    c.drawRect(bx, by, bw, bh, WHITE);
    // Nub
    c.fillRect(bx + bw, by + bh / 2 - nub / 2, nub, nub, WHITE);
    // Fill (green > 20%, yellow > 10%, red otherwise)
    int filled = (int)((float)level / 100.f * (bw - 4));
    uint32_t col = level > 20 ? TFT_GREEN : (level > 10 ? TFT_YELLOW : TFT_RED);
    if (filled > 0) c.fillRect(bx + 2, by + 2, filled, bh - 4, col);

    // Percentage centred in icon (or "?" if ADC error)
    char buf[16];
    if (level < 0) {
      c.setTextColor(TFT_DARKGREY);
      c.drawCenterString("?", cx, by + bh / 2 - 4);
    } else {
      snprintf(buf, sizeof(buf), "%d%%", level);
      c.setTextColor(WHITE);
      c.drawCenterString(buf, cx, by + bh / 2 - 4);
    }

    // Voltage
    if (voltage > 0) {
      snprintf(buf, sizeof(buf), "%d mV", voltage);
    } else {
      snprintf(buf, sizeof(buf), "-- mV");
    }
    c.setTextColor(TFT_LIGHTGREY);
    c.drawCenterString(buf, cx, by + bh + 6);

    // Charging indicator (Cardputer has no charge detection — always "unknown")
    if (charging) {
      c.setTextColor(TFT_YELLOW);
      c.drawCenterString("CHARGING", cx, by + bh + 18);
    } else {
      c.setTextColor(TFT_DARKGREY);
      c.drawCenterString("on battery", cx, by + bh + 18);
    }
  }
};


std::vector<Tab*> tabs = {
  new TabKeys(),
  new TabGraphics(),
  new TabCounter(),
  new TabRadio(),
  new TabMotion(),
  new TabIR(),
  new TabBattery(),
};
Counter currentTab(tabs.size());

void setup(void) {
  c.setup();
  sd.setup();
  lora.setup();
  gps.setup();
  motion.setup();
  ir.setup();
  tabs[currentTab]->setup();
}

void loop(void) {
  lora.loop();
  gps.loop();
  motion.loop();

  tabs[currentTab]->update();

  // draw tab bar
  c.setTextSize(1.0);
  int count = tabs.size();
  int w = c.width() / count;
  int h = 16;
  int y = c.height() - h;
  for (int i = 0; i < count; i++) {
    int x = i * w;
    bool active = i == currentTab;
    c.fillRoundRect(x + 1, y, w - 2, h - 2, 3, active ? WHITE : DARKGREY);
    c.setTextColor(active ? BLACK : WHITE);
    c.drawCenterString(tabs[i]->name, x + w / 2, y + 4);
  }

  // left/right switches tabs (edge-triggered)
  bool left  = c.isKeyPressed(KEY_LEFT);
  bool right = c.isKeyPressed(KEY_RIGHT);
  static bool prevLeft  = false;
  static bool prevRight = false;
  if (left && !prevLeft)   { --currentTab; tabs[currentTab]->setup(); }
  if (right && !prevRight) { ++currentTab; tabs[currentTab]->setup(); }
  prevLeft  = left;
  prevRight = right;

  c.loop();  // flush canvas to display — must be last
}
