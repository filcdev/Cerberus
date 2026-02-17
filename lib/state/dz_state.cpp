#include <Arduino.h>
#include "dz_state.h"

DZStateControl stateControl;

DZStateControl::DZStateControl() : logger("STAT") {
  _mutex = xSemaphoreCreateMutex();
}

void DZStateControl::begin() {
  doorTimer = xTimerCreate("DoorTimer", pdMS_TO_TICKS(5000), pdFALSE, (void*)this, doorTimerCallback);
}

void DZStateControl::openDoor() {
  if (xSemaphoreTake(_mutex, portMAX_DELAY)) {
    _state.doorOpen = true;
    xSemaphoreGive(_mutex);
  }
  if (doorTimer != NULL) {
    xTimerReset(doorTimer, 0);
  }
}

void DZStateControl::doorTimerCallback(TimerHandle_t xTimer) {
  DZStateControl* instance = (DZStateControl*) pvTimerGetTimerID(xTimer);
  instance->logger.info("Closing door");
  if (xSemaphoreTake(instance->_mutex, portMAX_DELAY)) {
    instance->_state.doorOpen = false;
    xSemaphoreGive(instance->_mutex);
  }
}

void DZStateControl::setError(ErrorSource source, bool hasError, const std::string& message) {
  if (xSemaphoreTake(_mutex, portMAX_DELAY)) {
    switch (source) {
      case ErrorSource::NFC:
        _state.error.nfc.hasError = hasError;
        _state.error.nfc.message = message;
        break;
      case ErrorSource::CFG:
        _state.error.sd.hasError = hasError;
        _state.error.sd.message = message;
        break;
      case ErrorSource::WIFI:
        _state.error.wifi.hasError = hasError;
        _state.error.wifi.message = message;
        break;
      case ErrorSource::WEBSOCKET:
        _state.error.webSocket.hasError = hasError;
        _state.error.webSocket.message = message;
        break;
      case ErrorSource::DB:
        _state.error.db.hasError = hasError;
        _state.error.db.message = message;
        break;
      case ErrorSource::OTA:
        _state.error.ota.hasError = hasError;
        _state.error.ota.message = message;
        break;
      default: break;
    }
    xSemaphoreGive(_mutex);
  }
}

bool DZStateControl::hasError(ErrorSource source) {
  bool result = false;
  if (xSemaphoreTake(_mutex, portMAX_DELAY)) {
    switch (source) {
      case ErrorSource::NFC: result = _state.error.nfc.hasError; break;
      case ErrorSource::CFG: result = _state.error.sd.hasError; break;
      case ErrorSource::WIFI: result = _state.error.wifi.hasError; break;
      case ErrorSource::WEBSOCKET: result = _state.error.webSocket.hasError; break;
      case ErrorSource::DB: result = _state.error.db.hasError; break;
      case ErrorSource::OTA: result = _state.error.ota.hasError; break;
      default: break;
    }
    xSemaphoreGive(_mutex);
  }
  return result;
}

void DZStateControl::setDeviceState(DeviceState newState) {
  if (xSemaphoreTake(_mutex, portMAX_DELAY)) {
    _state.deviceState = newState;
    xSemaphoreGive(_mutex);
  }
}

DeviceState DZStateControl::getDeviceState() {
  DeviceState ds = DEVICE_STATE_BOOTING;
  if (xSemaphoreTake(_mutex, portMAX_DELAY)) {
    ds = _state.deviceState;
    xSemaphoreGive(_mutex);
  }
  return ds;
}

void DZStateControl::setMessage(const std::string& message) {
  if (xSemaphoreTake(_mutex, portMAX_DELAY)) {
    _state.message = message;
    xSemaphoreGive(_mutex);
  }
}

std::string DZStateControl::getMessage() {
  std::string msg;
  if (xSemaphoreTake(_mutex, portMAX_DELAY)) {
    msg = _state.message;
    xSemaphoreGive(_mutex);
  }
  return msg;
}

void DZStateControl::setHeader(const std::string& header) {
  if (xSemaphoreTake(_mutex, portMAX_DELAY)) {
    _state.header = header;
    xSemaphoreGive(_mutex);
  }
}

std::string DZStateControl::getHeader() {
  std::string hdr;
  if (xSemaphoreTake(_mutex, portMAX_DELAY)) {
    hdr = _state.header;
    xSemaphoreGive(_mutex);
  }
  return hdr;
}

void DZStateControl::setTime(const std::string& time) {
  if (xSemaphoreTake(_mutex, portMAX_DELAY)) {
    _state.time = time;
    xSemaphoreGive(_mutex);
  }
}

std::string DZStateControl::getTime() {
  std::string t;
  if (xSemaphoreTake(_mutex, portMAX_DELAY)) {
    t = _state.time;
    xSemaphoreGive(_mutex);
  }
  return t;
}

bool DZStateControl::isDoorOpen() {
  bool open = false;
  if (xSemaphoreTake(_mutex, portMAX_DELAY)) {
    open = _state.doorOpen;
    xSemaphoreGive(_mutex);
  }
  return open;
}

Errors DZStateControl::getErrors() {
  Errors errs;
  if (xSemaphoreTake(_mutex, portMAX_DELAY)) {
    errs = _state.error;
    xSemaphoreGive(_mutex);
  }
  return errs;
}

GlobalState DZStateControl::getSnapshot() {
  GlobalState s;
  if (xSemaphoreTake(_mutex, portMAX_DELAY)) {
    s = _state;
    xSemaphoreGive(_mutex);
  }
  return s;
}

void DZStateControl::handle()
{
  GlobalState localState;
  if (xSemaphoreTake(_mutex, portMAX_DELAY)) {
    localState = _state;
    xSemaphoreGive(_mutex);
  }

  if(localState.deviceState != DEVICE_STATE_UPDATING){
    bool anyError = 
      localState.error.nfc.hasError ||
      localState.error.sd.hasError ||
      localState.error.wifi.hasError ||
      localState.error.webSocket.hasError ||
      localState.error.db.hasError ||
      localState.error.ota.hasError;

    if(anyError) {
      if (localState.deviceState != DEVICE_STATE_ERROR) {
        logger.error("Entering ERROR state");
        setDeviceState(DEVICE_STATE_ERROR);
      }
    }
    else {
      if (localState.deviceState != DEVICE_STATE_IDLE) {
        if (localState.deviceState == DEVICE_STATE_ERROR) {
          logger.info("Errors cleared, entering IDLE state");
        }
        setDeviceState(DEVICE_STATE_IDLE);
      }
    }
  }
  
  digitalWrite(2, localState.doorOpen ? HIGH : LOW);
  
  if(localState.message != "") {
    if(lastMessage != localState.message) {
      lastMessage = localState.message;
      messageTmr = millis();
    }
    if(messageTmr == 0) messageTmr = millis();
    if(millis() - messageTmr > 5000) {
      if (xSemaphoreTake(_mutex, portMAX_DELAY)) {
        _state.message = "";
        _state.header = "";
        xSemaphoreGive(_mutex);
      }
      lastMessage = "";
      messageTmr = 0;
    }
  }
}