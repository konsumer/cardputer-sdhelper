#include "M5Cardputer.h"
#include "SdFat.h"
#include "esp_task_wdt.h"

M5Canvas c(&M5Cardputer.Display);
SdFat sd;
SPIClass sdSPI(FSPI);
cid_t cid;
uint64_t totalBytes = 0;
uint32_t totalSectors = 0;
bool accepted = false;
bool onOK = false;
volatile float progress = 0;
volatile bool formatDone = false;
volatile bool formatOk = false;

// FatFormatter::initFatDir() always prints exactly 32 dots regardless of card size.
// Each dot increments progress by 1/32.
class ProgressPrinter : public Print {
public:
  size_t write(uint8_t b) override {
    if (b == '.') {
      progress += 1.0f / 32.0f;
      if (progress > 0.99f) progress = 0.99f;
    }
    return 1;
  }
};

static const char *humanSize(uint64_t bytes) {
  static const char* suffix[] = {"B", "KB", "MB", "GB", "TB"};
  char length = sizeof(suffix) / sizeof(suffix[0]);

  int i = 0;
  double dblBytes = bytes;

  if (bytes > 1024) {
    for (i = 0; (bytes / 1024) > 0 && i<length-1; i++, bytes /= 1024)
      dblBytes = bytes / 1024.0;
  }

  static char output[200];
  sprintf(output, "%.02lf %s", dblBytes, suffix[i]);
  return output;
}

static uint8_t sectorBuffer[512] __attribute__((aligned(4)));
static ProgressPrinter progressPrinter;

void formatTask(void* arg) {
  // The format task runs a tight SPI busy-wait loop that never yields, which
  // starves the IDLE tasks and triggers the TWDT. Unsubscribe both idle tasks
  // and deinit the TWDT for the duration. We don't reinit after since the
  // device has nothing left to do once formatting completes.
  TaskHandle_t idle0 = xTaskGetIdleTaskHandleForCPU(0);
  TaskHandle_t idle1 = xTaskGetIdleTaskHandleForCPU(1);
  esp_task_wdt_delete(idle0);
  esp_task_wdt_delete(idle1);
  esp_task_wdt_deinit();

  FatFormatter fatFormatter;
  bool ok = fatFormatter.format(sd.card(), sectorBuffer, &progressPrinter);

  formatOk = ok;
  formatDone = true;
  progress = 1.0f;
  vTaskDelete(nullptr);
}

void setup() {
  pinMode(TFT_CS, OUTPUT);
  digitalWrite(TFT_CS, HIGH);

  auto cfg = M5.config();
  M5Cardputer.begin(cfg, true);
  c.createSprite(M5.Display.width(), M5.Display.height());

  sdSPI.begin(SDCARD_SCK, SDCARD_MISO, SDCARD_MOSI, SDCARD_CS);
  pinMode(SDCARD_MISO, INPUT_PULLUP);

  SdSpiConfig cfgsd(SDCARD_CS, SHARED_SPI, SD_SCK_MHZ(SDCARD_MHZ), &sdSPI);
  if (!sd.begin(cfgsd)) {
    c.setTextColor(WHITE);
    c.fillScreen(RED);
    c.drawCenterString("SD Init Failed!", c.width()/2, c.height()/2 - 5);
    c.pushSprite(0, 0);
    while(1);
  }
  sd.card()->readCID(&cid);

  totalSectors = sd.card()->sectorCount();
  totalBytes = (uint64_t)totalSectors * 512;
}


void loop() {
  M5Cardputer.update();

  if (!accepted && M5Cardputer.Keyboard.isChange()) {
    if (M5Cardputer.Keyboard.isKeyPressed(',') || M5Cardputer.Keyboard.isKeyPressed('/')) {
      onOK = !onOK;
    }
    if (M5Cardputer.Keyboard.isKeyPressed(KEY_ENTER) && onOK) {
      accepted = true;
      xTaskCreatePinnedToCore(formatTask, "format", 8192, nullptr, 1, nullptr, 0);
    }
  }

  c.fillScreen(BLACK);

  c.setTextColor(GREEN);
  c.drawCenterString("SD Helper", c.width()/2, 4);

  if (!accepted) {
    c.setTextColor(WHITE);
    c.setCursor(4, 20);
    c.printf("Manufacturer: 0x%02X", cid.mid);
    c.setCursor(4, 28);
    c.printf("Product: %.5s", cid.pnm);

    uint8_t fat = sd.vol()->fatType();
    c.setCursor(4, 36);
    c.printf("Format: %s", fat == 32 ? "FAT32" : fat == 64 ? "exFAT" : "Other");

    c.setCursor(4, 44);
    c.printf("Sectors: %d", totalSectors);

    c.setCursor(4, 52);
    c.printf("Total Size: %s", humanSize(totalBytes));

    if (totalSectors > 67108864) { // 32GB
      c.setCursor(4, c.height() - 24);
      c.print("Your disk is >32GB. FAT32 formatting is not recommended.");
    } else {
      c.drawCenterString("Format the drive with FAT32?", c.width()/2, c.height() - 40);

      c.fillRoundRect( 8, c.height() - 25, (c.width()/2) - 10, 20, 5, onOK ? DARKGREY : LIGHTGREY);
      c.setTextColor(onOK ? LIGHTGREY : BLACK);
      c.setCursor(54, c.height() - 18);
      c.print("NO");

      c.fillRoundRect((c.width()/2) + 12, c.height() - 25, (c.width()/2) - 20, 20, 5, onOK ? LIGHTGREY : DARKGREY);
      c.setTextColor(onOK ? BLACK : LIGHTGREY);
      c.setCursor((c.width()/2) + 50, c.height() - 18);
      c.print("YES");
    }
  } else if (formatDone) {
    c.setTextColor(formatOk ? GREEN : RED);
    c.drawCenterString(formatOk ? "Format OK!" : "Format Failed!", c.width()/2, c.height()/2);
    c.pushSprite(0, 0);
    while(true) {
      M5Cardputer.update();
      if (M5Cardputer.Keyboard.isKeyPressed(KEY_ENTER)) {
        accepted = false;
        progress = 0;
        onOK = false;
        formatDone = false;
        break;
      }
    }
  } else {
    c.setTextColor(WHITE);
    c.drawCenterString("Formatting...", c.width()/2, c.height()/2 - 30);
    c.fillRoundRect(4, c.height()/2, c.width() - 8, 20, 5, DARKGREY);
    c.fillRoundRect(6, c.height()/2 + 2, (c.width() - 12) * progress, 16, 5, LIGHTGREY);
  }

  c.pushSprite(0, 0);
}
