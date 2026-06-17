#include <Arduino.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_task_wdt.h"
#include "dz_state.h"
#include "dz_button.h"
#include "dz_wifi.h"
#include "dz_config.h"
#include "dz_ntp.h"
#include "dz_nfc.h"
#include "dz_db.h"
#include "dz_configMgr.h"
#include "dz_ota.h"
#include "dz_led.h"
#include "dz_ws.h"
#include "dz_logger.h"

static Logger logger("MAIN");

DZButton button;
DZWIFIControl wifiControl;
DZNTPControl ntpControl;
DZNFCControl nfcControl;
DZConfigManager ConfigManager;
DZLEDControl ledControl;

void setup() {
  if(!loggerMutex) loggerMutex = xSemaphoreCreateMutex();
  Serial.begin(115200);
  //esp_task_wdt_init(30, true);
  logger.info("Starting setup...");
  pinMode(2, OUTPUT);
  pinMode(DOOR_PIN, OUTPUT);
  digitalWrite(2, HIGH);
  digitalWrite(DOOR_PIN, LOW);
  ledControl.begin();
  button.begin();
  stateControl.begin();
  ConfigManager.begin();
  wifiControl.begin();
  wsControl.begin();
  ntpControl.setup();
  nfcControl.begin();
  dbControl.begin();
  otaControl.begin();
  logger.info("Setup complete");
  digitalWrite(2, LOW);

}

void NetworkTask(void *pvParameters) {
  (void) pvParameters;
  for (;;) {
    wifiControl.handle();
    wsControl.handle();
    ntpControl.handle();
    otaControl.handle();
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}

void AppTask(void *pvParameters) {
  (void) pvParameters;
  for (;;) {
    button.handle();
    stateControl.handle();
    nfcControl.handle();
    ledControl.handle();
    //vTaskDelay(pdMS_TO_TICKS(50));
  }
}

void loop() {
  static bool tasksCreated = false;
  if (!tasksCreated) {
    // Network task: pin to core 0 (PRO_CPU)
    xTaskCreatePinnedToCore(
      NetworkTask,
      "NetworkTask",
      8192,
      NULL,
      1,
      NULL,
      0
    );

    // App task: pin to core 1 (APP_CPU)
    xTaskCreatePinnedToCore(
      AppTask,
      "AppTask",
      8192,
      NULL,
      1,
      NULL,
      1
    );

    tasksCreated = true;
    vTaskDelete(NULL);
  }
}
