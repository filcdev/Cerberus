#include "dz_wifi.h"
#include "dz_state.h"
#include "dz_configMgr.h"

DZWIFIControl::DZWIFIControl() : logger("WIFI") {}

void DZWIFIControl::begin()
{
  logger.info("Connecting to WiFi SSID: %s", cfg.wifi_ssid.c_str());
  WiFi.begin(cfg.wifi_ssid.c_str(), cfg.wifi_psk.c_str());
  _hostname += cfg.hostname;
  WiFi.setHostname(_hostname.c_str());
}

void DZWIFIControl::handle()
{
  if (WiFi.status() != WL_CONNECTED) {
    if(!stateControl.hasError(ErrorSource::WIFI)) {
      logger.error("WiFi Disconnected");
      stateControl.setError(ErrorSource::WIFI, true, "WiFi Disconn");
    }
  } else {
    if(stateControl.hasError(ErrorSource::WIFI)) {
      logger.info("WiFi Reconnected");
      stateControl.setError(ErrorSource::WIFI, false);
    }
  }
}
