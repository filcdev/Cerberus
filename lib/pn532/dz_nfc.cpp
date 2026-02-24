#include "dz_nfc.h"
#include "dz_state.h"
#include "dz_ws.h"
#include <ArduinoJson.h>
#include <string>

DZNFCControl::DZNFCControl() : logger("NFC") {}

void DZNFCControl::begin()
{
  logger.info("Initializing NFC");
  Serial1.begin(PN532_HSU_BAUD, SERIAL_8N1, PN532_HSU_RX_PIN, PN532_HSU_TX_PIN);
  delay(10);

  nfc.begin();
  uint32_t versiondata = 0;
  unsigned long start = millis();
  while (!versiondata && (millis() - start) < 2000) {
    versiondata = nfc.getFirmwareVersion();
    if (!versiondata) delay(200);
  }

  if (!versiondata) {
    logger.error("PN532 not found");
    stateControl.setError(ErrorSource::NFC, true, "PN532 Disconn");
    return;
  }

  logger.info("PN532 Found. Firmware version: %x", versiondata);
  nfc.SAMConfig();
  nfc.setPassiveActivationRetries(0x01);
  stateControl.setError(ErrorSource::NFC, false);
}

void DZNFCControl::handle()
{
  unsigned long now = millis();
  if (now - lastHealthCheck >= 5000) {
    lastHealthCheck = now;
    uint32_t versiondata = nfc.getFirmwareVersion();
    if (!versiondata) {
      if (!stateControl.hasError(ErrorSource::NFC)) logger.error("PN532 Disconnected");
      stateControl.setError(ErrorSource::NFC, true, "PN532 Disconn");
    } else {
      if (stateControl.hasError(ErrorSource::NFC)) logger.info("PN532 Reconnected");
      stateControl.setError(ErrorSource::NFC, false);
    }
  }
  if (stateControl.hasError(ErrorSource::NFC)) return;
  if (now - lastDetectionTime < 1000) return;
  if (now - lastPollTime < 100) return;
  lastPollTime = now;

  uint8_t uid[7];
  uint8_t uidLength;
  if (nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength)) {
    lastDetectionTime = now;
    std::string uidStr;
    char buf[4];
    for (uint8_t i = 0; i < uidLength; i++) {
      if (i > 0) uidStr += ':';
      snprintf(buf, sizeof(buf), "%02X", uid[i]);
      uidStr += buf;
    }
    logger.info("Card detected: %s", uidStr.c_str());
    std::string name;
    bool authorized = dbControl.isAuthorized(uidStr, name);
    if (authorized) {
      logger.info("Access Granted: %s", name.c_str());
      stateControl.setHeader("Welcome >>");
      stateControl.setMessage(name);
      stateControl.openDoor();
    } else {
      logger.info("Access Denied");
      stateControl.setMessage("Access Denied");
      stateControl.denyAccess();
    }
    wsControl.sendCardRead(uidStr, authorized, false);
  }
}