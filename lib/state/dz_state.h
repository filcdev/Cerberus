#ifndef DZ_STATE_H
#define DZ_STATE_H

#include <string>
#include <array>
#include "dz_logger.h"
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
#include "freertos/semphr.h"

enum DeviceState
{
  DEVICE_STATE_BOOTING = 0,
  DEVICE_STATE_IDLE = 1,
  DEVICE_STATE_ERROR = 2,
  DEVICE_STATE_UPDATING = 3
};

enum class ErrorSource {
  NFC,
  CFG,
  WIFI,
  WEBSOCKET,
  DB,
  OTA,
  COUNT
};

struct ErrorEntry
{
  bool hasError = false;
  std::string message = "";
};

struct Errors
{
  ErrorEntry nfc;
  ErrorEntry sd;
  ErrorEntry wifi;
  ErrorEntry webSocket;
  ErrorEntry db;
  ErrorEntry ota;

  std::array<std::string, 6> messages() const
  {
    return {
      nfc.message,
      sd.message,
      wifi.message,
      webSocket.message,
      db.message,
      ota.message
    };
  }
};

struct GlobalState
{
  DeviceState deviceState = DEVICE_STATE_BOOTING;
  Errors error;
  std::string header = "";
  std::string message = "";
  bool doorOpen = false;
  unsigned long doorOpenTmr = 0;
  bool accessDenied = false;
  unsigned long accessDeniedTmr = 0;
  std::string time = "--:--";
  int otaProgress = 0;
};

class DZStateControl
{
public:
  DZStateControl();
  void begin();
  void handle();
  void openDoor();
  void denyAccess();
  bool isAccessDenied();

  void setError(ErrorSource source, bool hasError, const std::string& message = "");
  bool hasError(ErrorSource source);
  
  void setDeviceState(DeviceState newState);
  DeviceState getDeviceState();
  
  void setMessage(const std::string& message);
  std::string getMessage();
  
  void setHeader(const std::string& header);
  std::string getHeader();
  
  void setTime(const std::string& time);
  std::string getTime();
  
  void setOtaProgress(int progress);
  int getOtaProgress();
  
  bool isDoorOpen();
  
  Errors getErrors();
  GlobalState getSnapshot();

private:
  Logger logger;
  GlobalState _state;
  SemaphoreHandle_t _mutex;

  unsigned long messageTmr = 0;
  std::string lastMessage = "";
  TimerHandle_t doorTimer = NULL;
  static void doorTimerCallback(TimerHandle_t xTimer);

  TimerHandle_t deniedTimer = NULL;
  static void deniedTimerCallback(TimerHandle_t xTimer);
};

extern DZStateControl stateControl;

#endif
