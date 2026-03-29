#pragma once
// CardputerSd: SD card / filesystem abstraction
// Abstracts native (SDL2), web (Emscripten+SDL2), and cardputer (ESP32-S3/M5)

#include <M5GFX.h>

#ifdef SDL_h_
#include <stdio.h>
#include <dirent.h>
#include <sys/stat.h>
#include <string.h>
#include <unistd.h>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
// Must be at file scope — EM_JS cannot appear inside a function body
EM_JS(void, _cardputer_sd_idbfs_sync, (), {
  FS.syncfs(false, function(err) {
    if (err) console.warn('IDBFS sync error:', err);
  });
});
#endif
#else
#include "M5Cardputer.h"
#include <SPI.h>
#include <SD.h>
#define SD_SPI_SCK_PIN  40
#define SD_SPI_MISO_PIN 39
#define SD_SPI_MOSI_PIN 14
#define SD_SPI_CS_PIN   12
#endif

// ---------------------------------------------------------------------------
// CardputerSd class
// ---------------------------------------------------------------------------
class CardputerSd {
public:
  bool ok = false;

  void setup() {
#ifndef SDL_h_
    SPI.begin(SD_SPI_SCK_PIN, SD_SPI_MISO_PIN, SD_SPI_MOSI_PIN, SD_SPI_CS_PIN);
    ok = SD.begin(SD_SPI_CS_PIN, SPI, 25000000);
#else
    // SDL builds use a local "sdcard/" directory as the virtual root
    mkdir("sdcard", 0755);
    ok = true;
#endif
  }

  void loop() {}

  // --- Directory ops ---

  void listDir(const char *dirname, uint8_t levels) {
#ifdef SDL_h_
    char full[512]; _path(full, sizeof(full), dirname);
    DIR *dir = opendir(full);
    if (!dir) { printf("Failed to open directory: %s\n", dirname); return; }
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
      if (entry->d_name[0] == '.') continue;
      char child[512];
      snprintf(child, sizeof(child), "%s/%s", dirname, entry->d_name);
      if (entry->d_type == DT_DIR) {
        printf("  DIR : %s\n", entry->d_name);
        if (levels) listDir(child, levels - 1);
      } else {
        char fullchild[512]; _path(fullchild, sizeof(fullchild), child);
        struct stat st; stat(fullchild, &st);
        printf("  FILE: %s  SIZE: %ld\n", entry->d_name, (long)st.st_size);
      }
    }
    closedir(dir);
#else
    File root = SD.open(dirname);
    if (!root || !root.isDirectory()) return;
    File file = root.openNextFile();
    while (file) {
      if (file.isDirectory() && levels) listDir(file.path(), levels - 1);
      file = root.openNextFile();
    }
#endif
  }

  bool createDir(const char *path) {
#ifdef SDL_h_
    char full[512]; _path(full, sizeof(full), path);
    return mkdir(full, 0755) == 0;
#else
    return SD.mkdir(path);
#endif
  }

  bool removeDir(const char *path) {
#ifdef SDL_h_
    char full[512]; _path(full, sizeof(full), path);
    return rmdir(full) == 0;
#else
    return SD.rmdir(path);
#endif
  }

  // --- File ops ---

  bool exists(const char *path) {
#ifdef SDL_h_
    char full[512]; _path(full, sizeof(full), path);
    FILE *f = fopen(full, "r");
    if (!f) return false;
    fclose(f);
    return true;
#else
    File file = SD.open(path);
    if (!file) return false;
    file.close();
    return true;
#endif
  }

  // Read up to bufsz bytes from file into buf. Returns bytes read, or -1 on error.
  int read(const char *path, uint8_t *buf, size_t bufsz) {
#ifdef SDL_h_
    char full[512]; _path(full, sizeof(full), path);
    FILE *f = fopen(full, "rb");
    if (!f) return -1;
    int n = (int)fread(buf, 1, bufsz, f);
    fclose(f);
    return n;
#else
    File file = SD.open(path);
    if (!file) return -1;
    int n = 0;
    while (file.available() && (size_t)n < bufsz) buf[n++] = file.read();
    file.close();
    return n;
#endif
  }

  // Write bufsz bytes from buf to file (overwrites). Returns false on error.
  bool write(const char *path, const uint8_t *buf, size_t bufsz) {
#ifdef SDL_h_
    char full[512]; _path(full, sizeof(full), path);
    FILE *f = fopen(full, "wb");
    if (!f) return false;
    bool ok = fwrite(buf, 1, bufsz, f) == bufsz;
    fclose(f);
    _sync();
    return ok;
#else
    File file = SD.open(path, FILE_WRITE);
    if (!file) return false;
    bool ok = file.write(buf, bufsz) == bufsz;
    file.close();
    return ok;
#endif
  }

  // Append bufsz bytes from buf to file. Returns false on error.
  bool append(const char *path, const uint8_t *buf, size_t bufsz) {
#ifdef SDL_h_
    char full[512]; _path(full, sizeof(full), path);
    FILE *f = fopen(full, "ab");
    if (!f) return false;
    bool ok = fwrite(buf, 1, bufsz, f) == bufsz;
    fclose(f);
    _sync();
    return ok;
#else
    File file = SD.open(path, FILE_APPEND);
    if (!file) return false;
    bool ok = file.write(buf, bufsz) == bufsz;
    file.close();
    return ok;
#endif
  }

  bool rename(const char *path1, const char *path2) {
#ifdef SDL_h_
    char full1[512], full2[512];
    _path(full1, sizeof(full1), path1);
    _path(full2, sizeof(full2), path2);
    return ::rename(full1, full2) == 0;
#else
    return SD.rename(path1, path2);
#endif
  }

  bool remove(const char *path) {
#ifdef SDL_h_
    char full[512]; _path(full, sizeof(full), path);
    return ::remove(full) == 0;
#else
    return SD.remove(path);
#endif
  }

private:
#ifdef SDL_h_
  static void _path(char *out, size_t outsz, const char *path) {
    snprintf(out, outsz, "sdcard%s", path);
  }

  static void _sync() {
#ifdef __EMSCRIPTEN__
    _cardputer_sd_idbfs_sync();
#endif
  }
#endif
};
