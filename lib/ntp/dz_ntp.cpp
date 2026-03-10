#include "dz_ntp.h"
#include "dz_state.h"
#include <Arduino.h>

DZNTPControl::DZNTPControl() : lastSyncAttempt(0), logger("NTP") {}

bool isTimeSet = false;
static const int TIME_RETRY_INTERVAL = 5000;

void DZNTPControl::setup() {
    logger.info("Configuring NTP");
    configTzTime(TIMEZONE, NTP_SERVER);
}

void DZNTPControl::handle() {
    unsigned long currentMillis = millis();
    unsigned long interval = isTimeSet ? TIME_SYNC_INTERVAL : TIME_RETRY_INTERVAL;
    if (currentMillis - lastSyncAttempt >= interval) {
        lastSyncAttempt = currentMillis;
        struct tm timeinfo;
        if (!getLocalTime(&timeinfo, 100)) {
            logger.error("Failed to obtain time");
            return;
        } else {
            if (!isTimeSet) {
                logger.info("Time synchronized");
                isTimeSet = true;
            }
        }
    }

    static unsigned long lastTimeUpdate = 0;
    if (currentMillis - lastTimeUpdate >= 1000) {
        lastTimeUpdate = currentMillis;
        std::string formatted = getFormattedTime();
        if (stateControl.getTime() != formatted) {
            stateControl.setTime(formatted);
        }
    }
}

std::string DZNTPControl::getFormattedTime() {
    struct tm timeinfo;
    if (!isTimeSet || !getLocalTime(&timeinfo, 100)) {
        return "--:--";
    }
    char timeString[6];
    std::snprintf(timeString, sizeof(timeString), "%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min);
    return std::string(timeString);
}
