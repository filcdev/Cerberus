#ifndef DZ_NFC_H
#define DZ_NFC_H
#include "dz_config.h"
#include <PN532_HSU.h>
#include <PN532.h>
#include "dz_logger.h"
#include "dz_desfire.h"


class DZNFCControl {
public:
    DZNFCControl();
    void begin();
    void handle();
private:
    Logger logger;
    PN532_HSU pn532hsu {Serial1};
    PN532 nfc {pn532hsu};
    DZDESFire desfire {nfc};
    unsigned long lastDetectionTime = 0;
    unsigned long lastHealthCheck = 0;
    unsigned long lastPollTime = 0;
    bool tryDESFireAuth(std::string &nameOut);
    std::string uidToStr(const uint8_t* uid, uint8_t len);
};

#endif