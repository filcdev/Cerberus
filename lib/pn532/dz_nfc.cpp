#include "dz_nfc.h"
#include "dz_state.h"
#include "dz_ws.h"
#include "dz_configMgr.h"
#include <ArduinoJson.h>
#include <string>

// ── DESFire PICC AID (3‑byte application identifier) ──────
static const uint8_t DESFIRE_AID[]  = DESFIRE_DEFAULT_AID;
static const uint8_t DESFIRE_AID_LEN = DESFIRE_DEFAULT_AID_LEN;

DZNFCControl::DZNFCControl() : logger("NFC"), desfire(nfc) {}

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

    std::string uidStr = uidToStr(uid, uidLength);

    // ── DESFire‑only authentication ─────────────────────────
    std::string name;
    bool authorized = tryDESFireAuth(name);

    if (authorized) {
      logger.info("Access Granted (DESFire): %s", name.c_str());
      stateControl.setHeader("Welcome >>");
      stateControl.setMessage(name);
      stateControl.openDoor();
    } else {
      logger.info("Access Denied: %s", uidStr.c_str());
      stateControl.setMessage("Access Denied");
      stateControl.denyAccess();
    }

    wsControl.sendCardRead(uidStr, authorized, false);
  }
}

// ────────────────────────────────────────────────────────────────
//  uidToStr  – format UID as "AA:BB:CC:DD:..."
// ────────────────────────────────────────────────────────────────
std::string DZNFCControl::uidToStr(const uint8_t *uid, uint8_t len)
{
  std::string out;
  char buf[4];
  for (uint8_t i = 0; i < len; i++) {
    if (i > 0) out += ':';
    snprintf(buf, sizeof(buf), "%02X", uid[i]);
    out += buf;
  }
  return out;
}

// ────────────────────────────────────────────────────────────────
//  tryDESFireAuth  – authenticate + read file → access decision
// ────────────────────────────────────────────────────────────────
bool DZNFCControl::tryDESFireAuth(std::string &nameOut)
{
  if (!cfg.desfireKeySet) {
    logger.debug("DESFire key not configured");
    return false;
  }

  // ── Step 1 – Detect DESFire ───────────────────────────────
  if (!desfire.detectDESFire()) {
    logger.debug("Not a DESFire card");
    return false;
  }

  logger.info("DESFire card detected");

  // ── Step 2 – Select application ───────────────────────────
  if (!desfire.selectApplication(DESFIRE_AID, DESFIRE_AID_LEN)) {
    logger.info("DESFire app select failed – wrong AID?");
    return false;
  }

  // ── Step 3 – AES‑128 authenticate ─────────────────────────
  if (!desfire.authenticateAES(DESFIRE_DEFAULT_KEY_NUM, cfg.desfireKey)) {
    logger.info("DESFire auth failed – wrong key?");
    return false;
  }

  logger.info("DESFire authenticated");

  // ── Step 4 – Read credential file ─────────────────────────
  uint8_t fileData[64];
  uint16_t dataLen = 0;
  if (!desfire.readData(DESFIRE_DEFAULT_FILE_ID, 0, 32,
                        fileData, &dataLen)) {
    logger.info("DESFire file read failed");
    return false;
  }

  // ── Step 5 – Extract name from file data (ASCII) ──────────
  nameOut.clear();
  for (uint16_t i = 0; i < dataLen && fileData[i] != 0x00; i++) {
    if (fileData[i] >= 0x20 && fileData[i] <= 0x7E) {
      nameOut += (char)fileData[i];
    }
  }

  if (nameOut.empty()) {
    // Build hex string as fallback identifier
    char hex[4];
    for (uint16_t i = 0; i < dataLen && i < 32; i++) {
      snprintf(hex, sizeof(hex), "%02X", fileData[i]);
      nameOut += hex;
    }
  }

  logger.info("DESFire credential: %s", nameOut.c_str());
  return true;
}