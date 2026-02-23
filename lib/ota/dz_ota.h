#ifndef DZ_OTA_H
#define DZ_OTA_H

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPUpdate.h>
#include "dz_logger.h"

enum OTAUpdateStatus {
    OTA_IDLE,
    OTA_IN_PROGRESS,
    OTA_SUCCESS,
    OTA_FAILED
};

class DZOTAControl {
public:
    DZOTAControl();
    void begin();
    void handle();
    void startUpdate(const char* url);
    volatile OTAUpdateStatus updateStatus = OTA_IDLE;
private:
    Logger logger;
    String otaUrl;
    static void otaTask(void* param);
};

extern DZOTAControl otaControl;

#endif