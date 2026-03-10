#include "dz_ws.h"
#include "dz_configMgr.h"
#include "dz_state.h"
#include "dz_db.h"
#include "dz_ota.h"
#include "dz_config.h"

DZWSControl wsControl;

DZWSControl::DZWSControl() : logger("WS") {}

void DZWSControl::begin() {
  logger.info("Initializing WebSocket");
  _wsQueue = xQueueCreate(10, sizeof(WSEvent));
  if (cfg.ws_addr.length() > 0 && cfg.ws_port > 0 && cfg.ws_path.length() > 0 && cfg.api_key.length() > 0) {
    webSocket.begin(cfg.ws_addr.c_str(), cfg.ws_port, cfg.ws_path.c_str());
    webSocket.onEvent(webSocketEvent);
    webSocket.setReconnectInterval(RECONNECT_INTERVAL);
    _extraHeaders = "X-Aegis-Device-Token: ";
    _extraHeaders += cfg.api_key.c_str();
    webSocket.setExtraHeaders(_extraHeaders.c_str());
    webSocket.enableHeartbeat(5000, 5000, 1); // Disable built-in heartbeat
  } else {
    logger.error("WebSocket configuration missing");
    stateControl.setError(ErrorSource::WEBSOCKET, true, "No WS Config");
  }
}

void DZWSControl::handle() {
  if(stateControl.hasError(ErrorSource::WIFI)) {
    return;
  }
  vTaskDelay(1); 
  webSocket.loop();

  if (_wsQueue != NULL) {
    WSEvent event;
    while (xQueueReceive(_wsQueue, &event, 0) == pdTRUE) {
      if (event.type == WS_EVT_CARD_READ) {
        JsonDocument doc;
        doc["type"] = "card-read";
        doc["uid"] = event.uid;
        doc["authorized"] = event.authorized;
        doc["buttonPressed"] = event.isButton;
        std::string msg;
        serializeJson(doc, msg);
        send(msg);
      }
    }
  }

  if (!stateControl.hasError(ErrorSource::WEBSOCKET) && millis() - lastPingTime > PING_INTERVAL) {
    logger.info("Sending WebSocket ping");
    sendPing();
  }
}

void DZWSControl::send(const std::string& message) {
  if (!stateControl.hasError(ErrorSource::WIFI) && !stateControl.hasError(ErrorSource::WEBSOCKET))
  {
    webSocket.sendTXT(message.c_str());
  }
}

void DZWSControl::sendPing() {
  lastPingTime = millis();
  
  JsonDocument doc;
  doc["type"] = "ping";

  JsonObject data = doc["data"].to<JsonObject>();
  data["fwVersion"] = FW_VERSION;
  data["uptime"] = millis();
  data["ramFree"] = ESP.getFreeHeap();

  JsonObject storage = data["storage"].to<JsonObject>();
  storage["total"] = SPIFFS.totalBytes();
  storage["used"] = SPIFFS.usedBytes();

  JsonObject debug = data["debug"].to<JsonObject>();
  debug["deviceState"] = stateControl.getDeviceState();
  debug["lastResetReason"] = getResetReason();

  JsonObject errors = debug["errors"].to<JsonObject>();
  errors["nfc"] = stateControl.hasError(ErrorSource::NFC);
  errors["sd"] = stateControl.hasError(ErrorSource::CFG);
  errors["wifi"] = stateControl.hasError(ErrorSource::WIFI);
  errors["db"] = stateControl.hasError(ErrorSource::DB);
  errors["ota"] = stateControl.hasError(ErrorSource::OTA);

  std::string msg;
  serializeJson(doc, msg);
  send(msg);
}

void DZWSControl::sendCardRead(const std::string& uid, bool authorized, bool isButton) {
  if (_wsQueue != NULL) {
    WSEvent event;
    event.type = WS_EVT_CARD_READ;
    strncpy(event.uid, uid.c_str(), sizeof(event.uid) - 1);
    event.uid[sizeof(event.uid) - 1] = '\0';
    event.authorized = authorized;
    event.isButton = isButton;
    event.timestamp = time(nullptr);
    xQueueSend(_wsQueue, &event, portMAX_DELAY);
  }
}

std::string DZWSControl::getResetReason() {
  esp_reset_reason_t reason = esp_reset_reason();
  switch (reason) {
    case ESP_RST_UNKNOWN: return "ESP_RST_UNKNOWN";
    case ESP_RST_POWERON: return "ESP_RST_POWERON";
    case ESP_RST_EXT: return "ESP_RST_EXT";
    case ESP_RST_SW: return "ESP_RST_SW";
    case ESP_RST_PANIC: return "ESP_RST_PANIC";
    case ESP_RST_INT_WDT: return "ESP_RST_INT_WDT";
    case ESP_RST_TASK_WDT: return "ESP_RST_TASK_WDT";
    case ESP_RST_WDT: return "ESP_RST_WDT";
    case ESP_RST_DEEPSLEEP: return "ESP_RST_DEEPSLEEP";
    case ESP_RST_BROWNOUT: return "ESP_RST_BROWNOUT";
    case ESP_RST_SDIO: return "ESP_RST_SDIO";
    default: return "UNKNOWN";
  }
}

void DZWSControl::webSocketEvent(WStype_t type, uint8_t * payload, size_t length) {
  switch(type) {
    case WStype_DISCONNECTED:
      wsControl.logger.error("WebSocket Disconnected");
      stateControl.setError(ErrorSource::WEBSOCKET, true, "WS Disconn");
      break;
    case WStype_CONNECTED:
      wsControl.logger.info("WebSocket Connected");
      stateControl.setError(ErrorSource::WEBSOCKET, false);
      break;
    case WStype_TEXT:
      wsControl.logger.info("WebSocket Message Received: %s", payload);
      handleIncomingMessage(std::string((char*)payload, length));
      break;
    case WStype_BIN:
      break;
    case WStype_ERROR:
      wsControl.logger.error("WebSocket Error");
      stateControl.setError(ErrorSource::WEBSOCKET, true, "WS Error");
      break;
    case WStype_PING:
    case WStype_PONG:
      break;
  }
}

void DZWSControl::handleIncomingMessage(const std::string& message) {
  JsonDocument wsDoc;
  DeserializationError error = deserializeJson(wsDoc, message);
  if (error) {
    wsControl.logger.error("Failed to parse WebSocket message");
    return;
  }
  
  const char* type = wsDoc["type"];
  if (!type) return;

  wsControl.logger.info("Processing message type: %s", type);

  if (strcmp(type, "sync-database") == 0) {
    wsControl.logger.info("Syncing database");
    if (wsDoc["db"].is<JsonArray>()) {
      dbControl.updateFromJSON(wsDoc["db"].as<JsonArray>());
    }
  } else if (strcmp(type, "open-door") == 0) {
    wsControl.logger.info("Remote open door request");
    stateControl.setHeader("Welcome >>");
    stateControl.openDoor();
    if (wsDoc["name"].is<const char*>()) {
      stateControl.setMessage(wsDoc["name"].as<const char*>());
    } else {
      stateControl.setMessage("WebUser");
    }
  } else if (strcmp(type, "update") == 0) {
    wsControl.logger.info("Remote update request");
    if (wsDoc["url"].is<const char*>())
    {
      otaControl.startUpdate(wsDoc["url"].as<const char*>());
    } else {
      otaControl.startUpdate(cfg.ota_url.c_str());
    }
  }
}
