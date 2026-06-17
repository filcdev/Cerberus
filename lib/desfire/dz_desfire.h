#ifndef DZ_DESFIRE_H
#define DZ_DESFIRE_H

#include <PN532.h>
#include "dz_logger.h"

class DZDESFire
{
public:
  DZDESFire(PN532 &nfc);

  // ── Card detection ──────────────────────────────────────────
  /// Returns true when the currently selected card is a DESFire.
  bool detectDESFire();

  /// Retrieve the hardware / software version block.
  bool getVersion(uint8_t *version, uint8_t *versionLen);

  /// DESFire EV1(+) native GetCardUID (7‑byte).
  bool getCardUID(uint8_t *uid, uint8_t *uidLen);

  // ── Application ─────────────────────────────────────────────
  bool selectApplication(const uint8_t *aid, uint8_t aidLen);

  // ── Authentication ──────────────────────────────────────────
  /// AES‑128 authenticati​on (DESFire EV1+).
  bool authenticateAES(uint8_t keyNum, const uint8_t *keyData);

  // ── File operations ─────────────────────────────────────────
  bool readData(uint8_t fileNum, uint16_t offset, uint16_t length,
                uint8_t *data, uint16_t *dataLen);
  bool getFileSettings(uint8_t fileNum, uint8_t *settings,
                       uint8_t *settingsLen);

  // ── Helpers ─────────────────────────────────────────────────
  /// Format a 7‑byte UID as "AA:BB:CC:DD:EE:FF:GG".
  static void uidToString(const uint8_t *uid, uint8_t len,
                          char *out, size_t outSize);

private:
  PN532 &_nfc;
  Logger logger;

  // ── Low‑level APDU ──────────────────────────────────────────
  bool sendAPDU(uint8_t cla, uint8_t ins, uint8_t p1, uint8_t p2,
                const uint8_t *data, uint8_t dataLen, uint8_t le,
                uint8_t *recv, uint8_t *recvLen);
  bool recvResponse(uint8_t *recv, uint8_t *recvLen, uint16_t timeoutMs = 200);

  // ── Internal helpers ────────────────────────────────────────
  bool selectCard();

  void computeSessionKeyAES(const uint8_t *key,
                            const uint8_t *rndA,
                            const uint8_t *rndB,
                            uint8_t *sessionKey);
  bool aesCmac(const uint8_t *key, const uint8_t *data, uint16_t dataLen,
               uint8_t *cmac);

  // ── DESFire native command codes ────────────────────────────
  static const uint8_t CMD_GET_VERSION       = 0x60;
  static const uint8_t CMD_GET_CARD_UID      = 0x51;
  static const uint8_t CMD_SELECT_APP        = 0x5A;
  static const uint8_t CMD_AUTHENTICATE_AES  = 0xAA;
  static const uint8_t CMD_READ_DATA         = 0xBD;
  static const uint8_t CMD_GET_FILE_SETTINGS = 0xF5;
  static const uint8_t CMD_ADDITIONAL_FRAME  = 0xAF;

  // ── Status codes ────────────────────────────────────────────
  static const uint8_t ST_SUCCESS          = 0x00;
  static const uint8_t ST_ADDITIONAL_FRAME = 0xAF;
  static const uint8_t ST_APP_NOT_FOUND    = 0xA0;
  static const uint8_t ST_AUTH_ERROR       = 0xAE;
  static const uint8_t ST_PERM_DENIED      = 0x9D;

  static const uint8_t DESFIRE_CLA = 0x90;

  // ── Session cache ───────────────────────────────────────────
  bool    _authenticated = false;
  uint8_t _sessionKey[16];
};

#endif
