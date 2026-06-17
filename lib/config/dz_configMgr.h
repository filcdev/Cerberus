#ifndef DZ_CONFIGMGR_H
#define DZ_CONFIGMGR_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include "dz_logger.h"

struct DeviceConfig {
  std::string hostname = "DokZar-ESP32";
  std::string api_key = "";
  std::string wifi_ssid = "";
  std::string wifi_psk = "";
  std::string ws_addr = "";
  uint16_t ws_port = 443;
  std::string ws_path = "/";
  bool ws_secure = false;
  bool ws_allow_insecure = false;
  std::string ota_url = "";

  // ── DESFire ─────────────────────────────────────────────────
  uint8_t desfireKey[16]  = {0};
  bool    desfireKeySet   = false;
};

class DZConfigManager {
public:
  DZConfigManager();
  void begin();

private:
  Logger logger;
  void parseConfigFile();
  bool copyFile(const char* srcPath, const char* dstPath);
  void checkSDUpdates();
};

extern DeviceConfig cfg;

#endif
