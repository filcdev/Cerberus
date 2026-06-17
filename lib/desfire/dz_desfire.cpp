#include "dz_desfire.h"
#include <mbedtls/aes.h>
#include <mbedtls/cmac.h>
#include <mbedtls/cipher.h>
#include <esp_system.h>
#include <cstring>

// ────────────────────────────────────────────────────────────────
//  Constructor
// ────────────────────────────────────────────────────────────────
DZDESFire::DZDESFire(PN532 &nfc)
    : _nfc(nfc), logger("DESFire")
{
  memset(_sessionKey, 0, sizeof(_sessionKey));
}

// ════════════════════════════════════════════════════════════════
//  UID formatting helper
// ════════════════════════════════════════════════════════════════
void DZDESFire::uidToString(const uint8_t *uid, uint8_t len,
                            char *out, size_t outSize)
{
  size_t pos = 0;
  for (uint8_t i = 0; i < len && pos + 3 <= outSize; ++i) {
    if (i > 0) out[pos++] = ':';
    snprintf(out + pos, outSize - pos, "%02X", uid[i]);
    pos += 2;
  }
  if (pos < outSize) out[pos] = '\0';
}

// ════════════════════════════════════════════════════════════════
//  Card selection – bring card to ACTIVE state
// ════════════════════════════════════════════════════════════════
bool DZDESFire::selectCard()
{
  // inListPassiveTarget wakes up and selects the card for T=CL
  return _nfc.inListPassiveTarget();
}

// ════════════════════════════════════════════════════════════════
//  Low‑level APDU exchange
// ════════════════════════════════════════════════════════════════
bool DZDESFire::sendAPDU(uint8_t cla, uint8_t ins, uint8_t p1,
                         uint8_t p2, const uint8_t *data,
                         uint8_t dataLen, uint8_t le,
                         uint8_t *recv, uint8_t *recvLen)
{
  uint8_t sendBuf[64];
  uint8_t sendLen = 0;

  sendBuf[sendLen++] = cla;
  sendBuf[sendLen++] = ins;
  sendBuf[sendLen++] = p1;
  sendBuf[sendLen++] = p2;

  if (data && dataLen > 0) {
    sendBuf[sendLen++] = dataLen;            // Lc
    memcpy(sendBuf + sendLen, data, dataLen);
    sendLen += dataLen;
  }

  if (le > 0) {
    sendBuf[sendLen++] = le;
  }

  return _nfc.inDataExchange(sendBuf, sendLen, recv, recvLen);
}

// ════════════════════════════════════════════════════════════════
//  Receive response with timeout (handles PN532 framing)
// ════════════════════════════════════════════════════════════════
bool DZDESFire::recvResponse(uint8_t *recv, uint8_t *recvLen,
                             uint16_t timeoutMs)
{
  // After sendAPDU the response is already in recv / recvLen.
  // This helper is called when we need to fetch Additional Frame
  // responses separately.
  uint8_t cmd = CMD_ADDITIONAL_FRAME;
  return _nfc.inDataExchange(&cmd, 1, recv, recvLen);
}

// ════════════════════════════════════════════════════════════════
//  DESFire detection – try GetVersion
// ════════════════════════════════════════════════════════════════
bool DZDESFire::detectDESFire()
{
  uint8_t ver[32] = {0};
  uint8_t verLen = sizeof(ver);
  if (getVersion(ver, &verLen)) {
    // A DESFire GetVersion response is at least 7 bytes
    return (verLen >= 7);
  }
  return false;
}

bool DZDESFire::getVersion(uint8_t *version, uint8_t *versionLen)
{
  if (!selectCard()) {
    logger.debug("selectCard failed");
    return false;
  }

  uint8_t recv[64];
  uint8_t recvLen = sizeof(recv);
  memset(recv, 0, sizeof(recv));

  if (!sendAPDU(DESFIRE_CLA, CMD_GET_VERSION, 0x00, 0x00,
                nullptr, 0, 0x00, recv, &recvLen)) {
    return false;
  }

  // Response: [version bytes...] SW1 SW2
  if (recvLen < 2) return false;

  uint8_t sw1 = recv[recvLen - 2];
  uint8_t sw2 = recv[recvLen - 1];

  if (sw1 != 0x91 || sw2 != ST_SUCCESS) {
    // Check for Additional Frame (multi‑frame response)
    if (sw1 == 0x91 && sw2 == ST_ADDITIONAL_FRAME) {
      // Copy data portion (excluding SW)
      uint8_t dataLen = recvLen - 2;
      memcpy(version, recv, dataLen);
      uint8_t total = dataLen;

      // Fetch remaining frames
      while (sw2 == ST_ADDITIONAL_FRAME) {
        uint8_t more[64];
        uint8_t moreLen = sizeof(more);
        if (!recvResponse(more, &moreLen)) return false;
        if (moreLen < 2) return false;
        sw1 = more[moreLen - 2];
        sw2 = more[moreLen - 1];
        uint8_t chunk = moreLen - 2;
        memcpy(version + total, more, chunk);
        total += chunk;
      }
      *versionLen = total;
      return (sw1 == 0x91 && sw2 == ST_SUCCESS);
    }
    return false;
  }

  uint8_t dataLen = recvLen - 2;
  memcpy(version, recv, dataLen);
  *versionLen = dataLen;
  return true;
}

// ════════════════════════════════════════════════════════════════
//  GetCardUID  (DESFire EV1+)
// ════════════════════════════════════════════════════════════════
bool DZDESFire::getCardUID(uint8_t *uid, uint8_t *uidLen)
{
  if (!selectCard()) return false;

  uint8_t recv[32];
  uint8_t recvLen = sizeof(recv);
  memset(recv, 0, sizeof(recv));

  if (!sendAPDU(DESFIRE_CLA, CMD_GET_CARD_UID, 0x00, 0x00,
                nullptr, 0, 0x00, recv, &recvLen)) {
    return false;
  }

  // Response: UID (7 bytes) + SW1 SW2
  if (recvLen < 9) return false;          // 7‑byte UID + 2‑byte SW

  uint8_t sw1 = recv[recvLen - 2];
  uint8_t sw2 = recv[recvLen - 1];

  if (sw1 != 0x91 || sw2 != ST_SUCCESS) return false;

  *uidLen = recvLen - 2;
  memcpy(uid, recv, *uidLen);
  return true;
}

// ════════════════════════════════════════════════════════════════
//  SelectApplication
// ════════════════════════════════════════════════════════════════
bool DZDESFire::selectApplication(const uint8_t *aid, uint8_t aidLen)
{
  if (!selectCard()) return false;

  uint8_t recv[8];
  uint8_t recvLen = sizeof(recv);
  memset(recv, 0, sizeof(recv));

  // DESFire SelectApplication: 90 5A 00 00 Lc AID 00
  if (!sendAPDU(DESFIRE_CLA, CMD_SELECT_APP, 0x00, 0x00,
                aid, aidLen, 0x00, recv, &recvLen)) {
    return false;
  }

  if (recvLen != 2) return false;

  uint8_t sw1 = recv[0];
  uint8_t sw2 = recv[1];

  _authenticated = false;                 // reset auth on app change

  return (sw1 == 0x91 && sw2 == ST_SUCCESS);
}

// ════════════════════════════════════════════════════════════════
//  AES session‑key derivation
// ════════════════════════════════════════════════════════════════
void DZDESFire::computeSessionKeyAES(const uint8_t *key,
                                     const uint8_t *rndA,
                                     const uint8_t *rndB,
                                     uint8_t *sessionKey)
{
  // Session key = AES‑128‑CBC Encrypt (RndA[0..3] + RndB[0..3]) |
  //                AES‑128‑CBC Encrypt (RndA[12..15] + RndB[12..15])
  // … using key as the AES key and an all‑zero IV.
  uint8_t iv[16]  = {0};
  uint8_t plain[16];
  uint8_t crypt[32];

  mbedtls_aes_context ctx;
  mbedtls_aes_init(&ctx);
  mbedtls_aes_setkey_enc(&ctx, key, 128);

  // First half: RndA[0..3] + RndB[0..3]  (zero‑padded to 16)
  memset(plain, 0, 16);
  memcpy(plain,      rndA,     4);
  memcpy(plain + 4,  rndB,     4);
  mbedtls_aes_crypt_cbc(&ctx, MBEDTLS_AES_ENCRYPT, 16,
                        iv, plain, crypt);

  // Second half: RndA[12..15] + RndB[12..15]
  memset(iv, 0, 16);
  memset(plain, 0, 16);
  memcpy(plain,      rndA + 12, 4);
  memcpy(plain + 4,  rndB + 12, 4);
  mbedtls_aes_crypt_cbc(&ctx, MBEDTLS_AES_ENCRYPT, 16,
                        iv, plain, crypt + 16);

  mbedtls_aes_free(&ctx);

  memcpy(sessionKey, crypt, 16);
}

// ════════════════════════════════════════════════════════════════
//  AES‑CMAC helper (for CMAC verification, if needed)
// ════════════════════════════════════════════════════════════════
bool DZDESFire::aesCmac(const uint8_t *key, const uint8_t *data,
                        uint16_t dataLen, uint8_t *cmac)
{
  const mbedtls_cipher_info_t *cipher =
      mbedtls_cipher_info_from_type(MBEDTLS_CIPHER_AES_128_ECB);

  if (!cipher) return false;

  int ret = mbedtls_cipher_cmac(cipher, key, 128,
                                data, dataLen, cmac);
  return (ret == 0);
}

// ════════════════════════════════════════════════════════════════
//  AES‑128 Authenticate  (DESFire EV1 / EV2)
// ════════════════════════════════════════════════════════════════
bool DZDESFire::authenticateAES(uint8_t keyNum, const uint8_t *keyData)
{
  if (!selectCard()) return false;

  // ── Step 1 – Send Authenticate command ──────────────────────
  uint8_t recv[32];
  uint8_t recvLen = sizeof(recv);
  memset(recv, 0, sizeof(recv));

  {
    uint8_t authCmd = keyNum & 0x0F;          // P1 = key number
    if (!sendAPDU(DESFIRE_CLA, CMD_AUTHENTICATE_AES,
                  authCmd, 0x00, nullptr, 0, 0x00, recv, &recvLen))
      return false;
  }

  // Response should be AF + RndB (16 bytes) + SW
  if (recvLen != 19) return false;            // 1 (AF) + 16 + 2

  if (recv[0] != ST_ADDITIONAL_FRAME) return false;
  if (recv[recvLen - 2] != 0x91 || recv[recvLen - 1] != ST_ADDITIONAL_FRAME)
    return false;

  uint8_t rndB[16];
  memcpy(rndB, recv + 1, 16);

  // ── Step 2 – Generate RndA, derive session key ──────────────
  uint8_t rndA[16];
  for (int i = 0; i < 16; ++i) {
    rndA[i] = (uint8_t)esp_random();          // ESP‑IDF TRNG
  }

  uint8_t combined[32];
  memcpy(combined,      rndA, 16);
  memcpy(combined + 16, rndB, 16);

  // Rotate RndB left by one byte
  uint8_t rndB_rot[16];
  memcpy(rndB_rot, rndB + 1, 15);
  rndB_rot[15] = rndB[0];

  // Encrypt RndA || RndB' with master key
  uint8_t cryptogram[32];
  {
    mbedtls_aes_context ctx;
    mbedtls_aes_init(&ctx);
    mbedtls_aes_setkey_enc(&ctx, keyData, 128);

    uint8_t plain[16];
    uint8_t iv[16] = {0};

    // Encrypt RndA (16 bytes)
    mbedtls_aes_crypt_cbc(&ctx, MBEDTLS_AES_ENCRYPT, 16,
                          iv, rndA, cryptogram);
    // Encrypt RndB' (16 bytes)
    mbedtls_aes_crypt_cbc(&ctx, MBEDTLS_AES_ENCRYPT, 16,
                          iv, rndB_rot, cryptogram + 16);

    mbedtls_aes_free(&ctx);
  }

  // ── Step 3 – Send cryptogram ────────────────────────────────
  {
    uint8_t authData[32];
    memcpy(authData, cryptogram, 32);

    memset(recv, 0, sizeof(recv));
    recvLen = sizeof(recv);

    if (!sendAPDU(DESFIRE_CLA, CMD_ADDITIONAL_FRAME,
                  0x00, 0x00, authData, 32, 0x00, recv, &recvLen))
      return false;
  }

  // Response: AF + RndA' (16 bytes encrypted) + SW
  if (recvLen != 19) return false;
  if (recv[0] != ST_ADDITIONAL_FRAME) return false;

  uint8_t sw1 = recv[recvLen - 2];
  uint8_t sw2 = recv[recvLen - 1];
  if (sw1 != 0x91 || sw2 != ST_SUCCESS) return false;

  // ── Step 4 – Verify RndA' (decrypt & check rotation) ────────
  uint8_t rndA_enc[16];
  memcpy(rndA_enc, recv + 1, 16);

  uint8_t rndA_dec[16];
  {
    mbedtls_aes_context ctx;
    mbedtls_aes_init(&ctx);
    mbedtls_aes_setkey_enc(&ctx, keyData, 128);

    uint8_t iv[16] = {0};
    mbedtls_aes_crypt_cbc(&ctx, MBEDTLS_AES_DECRYPT, 16,
                          iv, rndA_enc, rndA_dec);
    mbedtls_aes_free(&ctx);
  }

  // Expected: RndA rotated left by one byte
  uint8_t expected[16];
  memcpy(expected, rndA + 1, 15);
  expected[15] = rndA[0];

  if (memcmp(rndA_dec, expected, 16) != 0) {
    logger.debug("AES auth failed – RndA' mismatch");
    return false;
  }

  // ── Derive session key for subsequent operations ────────────
  computeSessionKeyAES(keyData, rndA, rndB, _sessionKey);
  _authenticated = true;

  logger.debug("AES auth OK");
  return true;
}

// ════════════════════════════════════════════════════════════════
//  ReadData  (requires prior authentication if file is protected)
// ════════════════════════════════════════════════════════════════
bool DZDESFire::readData(uint8_t fileNum, uint16_t offset,
                         uint16_t length, uint8_t *data,
                         uint16_t *dataLen)
{
  if (!selectCard()) return false;

  uint8_t cmdData[7];
  cmdData[0] = fileNum;
  cmdData[1] = (uint8_t)(offset & 0xFF);
  cmdData[2] = (uint8_t)((offset >> 8) & 0xFF);
  cmdData[3] = (uint8_t)((offset >> 16) & 0xFF);	  // 3‑byte offset
  cmdData[4] = (uint8_t)(length & 0xFF);
  cmdData[5] = (uint8_t)((length >> 8) & 0xFF);
  cmdData[6] = (uint8_t)((length >> 16) & 0xFF);	  // 3‑byte length

  uint8_t recv[64];
  uint8_t recvLen = sizeof(recv);
  memset(recv, 0, sizeof(recv));

  if (!sendAPDU(DESFIRE_CLA, CMD_READ_DATA, 0x00, 0x00,
                cmdData, 7, 0x00, recv, &recvLen)) {
    return false;
  }

  if (recvLen < 2) return false;

  uint8_t sw1 = recv[recvLen - 2];
  uint8_t sw2 = recv[recvLen - 1];

  // Handle multi‑frame response
  if (sw1 == 0x91 && sw2 == ST_ADDITIONAL_FRAME) {
    uint16_t total = recvLen - 2;
    memcpy(data, recv, total);

    while (sw2 == ST_ADDITIONAL_FRAME) {
      uint8_t more[64];
      uint8_t moreLen = sizeof(more);
      if (!recvResponse(more, &moreLen)) return false;
      if (moreLen < 2) return false;
      sw1 = more[moreLen - 2];
      sw2 = more[moreLen - 1];
      uint8_t chunk = moreLen - 2;
      memcpy(data + total, more, chunk);
      total += chunk;
    }
    *dataLen = total;
    return (sw1 == 0x91 && sw2 == ST_SUCCESS);
  }

  if (sw1 != 0x91 || sw2 != ST_SUCCESS) return false;

  *dataLen = recvLen - 2;
  memcpy(data, recv, *dataLen);
  return true;
}

// ════════════════════════════════════════════════════════════════
//  GetFileSettings
// ════════════════════════════════════════════════════════════════
bool DZDESFire::getFileSettings(uint8_t fileNum, uint8_t *settings,
                                uint8_t *settingsLen)
{
  if (!selectCard()) return false;

  uint8_t cmdData = fileNum;
  uint8_t recv[32];
  uint8_t recvLen = sizeof(recv);
  memset(recv, 0, sizeof(recv));

  if (!sendAPDU(DESFIRE_CLA, CMD_GET_FILE_SETTINGS, 0x00, 0x00,
                &cmdData, 1, 0x00, recv, &recvLen)) {
    return false;
  }

  if (recvLen < 2) return false;

  uint8_t sw1 = recv[recvLen - 2];
  uint8_t sw2 = recv[recvLen - 1];

  if (sw1 != 0x91 || sw2 != ST_SUCCESS) return false;

  *settingsLen = recvLen - 2;
  memcpy(settings, recv, *settingsLen);
  return true;
}
