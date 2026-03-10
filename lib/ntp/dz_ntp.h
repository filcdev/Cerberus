#ifndef DZ_NTP_H
#define DZ_NTP_H

#include <WiFi.h>
#include "time.h"
#include "dz_logger.h"

#define NTP_SERVER "pool.ntp.org"
#define TIMEZONE "CET-1CEST,M3.5.0,M10.5.0/3"
#define TIME_SYNC_INTERVAL 3600000

class DZNTPControl {
public:
    DZNTPControl();
    void setup();
    void handle();
    std::string getFormattedTime();

private:
    Logger logger;
    unsigned long lastSyncAttempt;
};

#endif