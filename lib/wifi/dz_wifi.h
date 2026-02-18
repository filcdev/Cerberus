#ifndef DZ_WIFI_H
#define DZ_WIFI_H
#include <WiFi.h>
#include "dz_logger.h"

class DZWIFIControl
{
public:
  DZWIFIControl();
  void begin();
  void handle();
  bool isConnected() const {
    return WiFi.status() == WL_CONNECTED;
  }
private:
  Logger logger;
  std::string _hostname = "SzW-";
  unsigned long previousMillis = 0;
  unsigned long lastConnectionAttempt = 300000; // 5 minutes
};

#endif