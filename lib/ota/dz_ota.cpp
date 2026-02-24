#include "dz_ota.h"
#include "dz_state.h"

DZOTAControl otaControl;

DZOTAControl::DZOTAControl() : logger("OTA") {}

void DZOTAControl::begin()
{
  logger.info("OTA initialized");
}

void DZOTAControl::otaTask(void* param)
{
  DZOTAControl* self = static_cast<DZOTAControl*>(param);

  WiFiClientSecure client;
  client.setInsecure();

  httpUpdate.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  httpUpdate.onProgress([](int current, int total) {
    if (total > 0) {
      int pct = (current * 100) / total;
      stateControl.setOtaProgress(pct);
    }
  });
  t_httpUpdate_return ret = httpUpdate.update(client, self->otaUrl);

  if (ret == HTTP_UPDATE_OK) {
    self->updateStatus = OTA_SUCCESS;
  } else {
    self->updateStatus = OTA_FAILED;
  }

  vTaskDelete(NULL);
}

void DZOTAControl::handle()
{
  if (stateControl.getDeviceState() != DEVICE_STATE_UPDATING) return;

  switch (updateStatus) {
    case OTA_SUCCESS:
      logger.info("OTA Update Successful, rebooting...");
      updateStatus = OTA_IDLE;
      stateControl.setDeviceState(DEVICE_STATE_IDLE);
      stateControl.setMessage("Rebooting...");
      delay(1000);
      ESP.restart();
      break;

    case OTA_FAILED:
      logger.error("OTA Update Failed");
      updateStatus = OTA_IDLE;
      stateControl.setDeviceState(DEVICE_STATE_IDLE);
      stateControl.setError(ErrorSource::OTA, true, "OTA Failed");
      break;

    default:
      break;
  }
}

void DZOTAControl::startUpdate(const char* url)
{
  logger.info("Starting OTA update from %s", url);
  if (stateControl.hasError(ErrorSource::WIFI)) {
    logger.error("Cannot start OTA: WiFi error");
    return;
  }

  otaUrl = url;
  updateStatus = OTA_IN_PROGRESS;
  stateControl.setError(ErrorSource::OTA, false);
  stateControl.setDeviceState(DEVICE_STATE_UPDATING);
  stateControl.setMessage("Updating...");
  xTaskCreate(otaTask, "ota_task", 8192, this, 5, NULL);
}
