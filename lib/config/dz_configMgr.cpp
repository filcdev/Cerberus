#include "dz_configMgr.h"
#include <SPI.h>
#include "dz_config.h"
#include "dz_state.h"
#include <SPIFFS.h>

#if defined(DZ_USE_SDMMC)
  #include <SD_MMC.h>
  #define DZ_SD_FS SD_MMC
#else
  #include <SD.h>
  #define DZ_SD_FS SD
#endif

DeviceConfig cfg;

DZConfigManager::DZConfigManager() : logger("CFG") {}

void DZConfigManager::begin() {
  logger.info("Initializing Config Manager");
  if (!SPIFFS.begin(true)) {
    logger.error("SPIFFS Mount Failed");
    stateControl.setError(ErrorSource::CFG, true, "SPIFFS Init");
    return;
  }

#if defined(DZ_USE_SDMMC)
  DZ_SD_FS.setPins(SD_MMC_CLK, SD_MMC_CMD, SD_MMC_D0);
  const bool sdOk = DZ_SD_FS.begin("/sdcard", true);
#else
  const bool sdOk = DZ_SD_FS.begin(SD_CS_PIN);
#endif

  if (sdOk) {
    logger.info("SD Card detected, checking for updates");
    checkSDUpdates();
    DZ_SD_FS.end();
  } else {
    logger.info("No SD Card detected");
  }
  
  parseConfigFile();
}

void DZConfigManager::checkSDUpdates() {
  // First-time setup: copy config.json from SD if not on SPIFFS
  if (!SPIFFS.exists("/config.json") && DZ_SD_FS.exists("/config.json")) {
    logger.info("Copying config.json from SD card (first-time setup)");
    copyFile("/config.json", "/config.json");
  }

  if (DZ_SD_FS.exists("/config.new.json")) {
    logger.info("Found config.new.json, updating config");
    if (copyFile("/config.new.json", "/config.json")) {
      DZ_SD_FS.remove("/config.new.json");
    }
  }
  
  if (DZ_SD_FS.exists("/uids.json")) {
    logger.info("Found uids.json, updating uids");
    if (copyFile("/uids.json", "/uids.json")) {
      DZ_SD_FS.remove("/uids.json");
    }
  }
}

bool DZConfigManager::copyFile(const char* srcPath, const char* dstPath) {
  File src = DZ_SD_FS.open(srcPath, FILE_READ);
  if (!src) return false;

  File dst = SPIFFS.open(dstPath, FILE_WRITE);
  if (!dst) {
    src.close();
    return false;
  }

  uint8_t buf[512];
  while (src.available()) {
    size_t len = src.read(buf, sizeof(buf));
    dst.write(buf, len);
  }

  src.close();
  dst.close();
  return true;
}

void DZConfigManager::parseConfigFile() {
  logger.info("Parsing config file");
  if (!SPIFFS.exists("/config.json")) {
    logger.error("Config file missing");
    stateControl.setError(ErrorSource::CFG, true, "Cfg Miss");
    return;
  }

  File f = SPIFFS.open("/config.json", FILE_READ);
  if (!f) {
    logger.error("Failed to open config file");
    stateControl.setError(ErrorSource::CFG, true, "Cfg Open");
    return;
  }

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, f);
  f.close();

  if (err) {
    logger.error("Config Parse Error: %s", err.c_str());
    stateControl.setError(ErrorSource::CFG, true, "Cfg Parse");
    return;
  }
  
  if (doc["hostname"]) cfg.hostname = doc["hostname"].as<std::string>();
  if (doc["api_key"]) cfg.api_key = doc["api_key"].as<std::string>();
  if (doc["wifi_ssid"]) cfg.wifi_ssid = doc["wifi_ssid"].as<std::string>();
  if (doc["wifi_psk"]) cfg.wifi_psk = doc["wifi_psk"].as<std::string>();
  if (doc["ws_addr"]) cfg.ws_addr = doc["ws_addr"].as<std::string>();
  if (doc["ws_port"]) cfg.ws_port = doc["ws_port"].as<uint16_t>();
  if (doc["ws_path"]) cfg.ws_path = doc["ws_path"].as<std::string>();
  if (doc["ws_secure"]) cfg.ws_secure = doc["ws_secure"].as<bool>();
  if (doc["ws_allow_insecure"]) cfg.ws_allow_insecure = doc["ws_allow_insecure"].as<bool>();
  if (doc["ota_url"]) cfg.ota_url = doc["ota_url"].as<std::string>();

  // ── DESFire AES‑128 key (hex string in config.json) ─────
  if (doc["desfire_key"].is<const char*>()) {
    const char *hex = doc["desfire_key"].as<const char*>();
    size_t hexLen = strlen(hex);
    if (hexLen == 32) {                     // 32 hex chars = 16 bytes
      for (size_t i = 0; i < 16; ++i) {
        char byteStr[3] = {hex[i*2], hex[i*2+1], '\0'};
        cfg.desfireKey[i] = (uint8_t)strtoul(byteStr, nullptr, 16);
      }
      cfg.desfireKeySet = true;
      logger.info("DESFire key loaded from config");
    } else {
      logger.info("DESFire key in config has invalid length (%d)", hexLen);
    }
  }

  logger.info("Config loaded successfully");
  stateControl.setError(ErrorSource::CFG, false, "");
}


