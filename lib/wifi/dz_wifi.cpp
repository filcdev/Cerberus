#include "dz_wifi.h"
#include "dz_state.h"
#include "dz_configMgr.h"

DZWIFIControl::DZWIFIControl() : logger("WIFI") {}

void DZWIFIControl::begin()
{
  if (cfg.wifi_ssid.empty()) {
    logger.error("WiFi SSID not configured – upload data/config.json via uploadfs");
    stateControl.setError(ErrorSource::WIFI, true, "No SSID");
    return;
  }
  logger.info("Connecting to WiFi SSID: %s", cfg.wifi_ssid.c_str());
  WiFi.begin(cfg.wifi_ssid.c_str(), cfg.wifi_psk.c_str());
  _hostname += cfg.hostname;
  WiFi.setHostname(_hostname.c_str());
}

void DZWIFIControl::handle()
{
  wl_status_t status = WiFi.status();
  if (status != WL_CONNECTED) {
    if(!stateControl.hasError(ErrorSource::WIFI)) {
      // Log reason for disconnect
      const char* reason;
      switch (status) {
        case WL_NO_SSID_AVAIL:  reason = "SSID not found";          break;
        case WL_CONNECT_FAILED: reason = "Connection failed";        break;
        case WL_CONNECTION_LOST:reason = "Connection lost";          break;
        case WL_DISCONNECTED:   reason = "Disconnected";             break;
        case WL_IDLE_STATUS:    reason = "Idle";                     break;
        case WL_NO_SHIELD:      reason = "No WiFi hardware";         break;
        case WL_SCAN_COMPLETED: reason = "Scan completed";           break;
        default:                reason = "Unknown";                  break;
      }
      logger.error("WiFi Disconnected – status: %d (%s)  RSSI: %d  channel: %d",
                   status, reason, WiFi.RSSI(), WiFi.channel());
      stateControl.setError(ErrorSource::WIFI, true, "WiFi Disconn");
    }
    // Auto‑reconnect every 30 seconds
    if (millis() - lastConnectionAttempt >= 30000) {
      lastConnectionAttempt = millis();
      logger.info("Reconnecting WiFi...");
      WiFi.reconnect();
    }
  } else {
    if(stateControl.hasError(ErrorSource::WIFI)) {
      logger.info("WiFi Reconnected  RSSI: %d  channel: %d  IP: %s",
                  WiFi.RSSI(), WiFi.channel(), WiFi.localIP().toString().c_str());
      stateControl.setError(ErrorSource::WIFI, false);
    }
  }
}
