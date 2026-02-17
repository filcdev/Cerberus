#include <LiquidCrystal_I2C.h>
#include <Wire.h>
#include "dz_lcd.h"
#include "dz_state.h"
#include "dz_config.h"
#include <Arduino.h>

DZLCDControl::DZLCDControl() : lcd(LCD_ADDRESS, LCD_COLUMNS, LCD_ROWS), logger("LCD") {}

void DZLCDControl::begin()
{
  logger.info("Initializing LCD");
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  lcd.init();
  lcd.backlight();
  handle();
}

void DZLCDControl::printLn(const char* msg) {
  const size_t max = LCD_COLUMNS; // 16
  size_t len = strlen(msg);
  size_t toWrite = (len < max) ? len : max;
  for (size_t i = 0; i < toWrite; ++i) {
    lcd.write(msg[i]);
  }
  for (size_t i = toWrite; i < max; ++i) {
    lcd.write(' ');
  }
}

bool DZLCDControl::isUpdateNeeded(const GlobalState& currentState) {
  return (
    currentState.deviceState != lastState.deviceState || 
    currentState.deviceState != DEVICE_STATE_IDLE ||
    currentState.doorOpen != lastState.doorOpen ||
    currentState.message != lastState.message
  );
}

void DZLCDControl::updateHeader(const GlobalState& currentState) {
  if(
    (backlightOn && 
    currentState.deviceState != DEVICE_STATE_BOOTING && 
    currentState.deviceState != DEVICE_STATE_UPDATING &&
    lastTime != currentState.time) || currentState.header != lastState.header
  ) {
    lcd.setCursor(0, 0);
    if(currentState.header != "") {
      printLn(currentState.header.c_str());
    } else {
      printLn("Aegis");
    }
    lcd.setCursor(11, 0);
    lcd.print(currentState.time.c_str());
    lastTime = currentState.time;
  }
}

void DZLCDControl::cycleErrors(const GlobalState& currentState) {
  const int MAX_ERRORS = static_cast<int>(ErrorSource::COUNT);
  if (erIndex < 0 || erIndex >= MAX_ERRORS) erIndex = 0;
  
  if (currentState.error.messages()[erIndex].empty()) {
    for (int i = 0; i < MAX_ERRORS; ++i) {
      if (!currentState.error.messages()[i].empty()) { erIndex = i; break; }
    }
  }
  
  unsigned long now = millis();
  if (now - lastErrorSwitch >= switchInterval) {
    int next = erIndex;
    for (int i = 1; i <= MAX_ERRORS; ++i) {
      int idx = (erIndex + i) % MAX_ERRORS;
      if (!currentState.error.messages()[idx].empty()) { next = idx; break; }
    }
    erIndex = next;
    lastErrorSwitch = now;
    lcd.setCursor(0, 1);
    lcd.print("E: ");
    printLn(currentState.error.messages()[erIndex].c_str());
  }
}

void DZLCDControl::displayCurrentState(const GlobalState& currentState) {
  if (currentState.message != "") {
      lcd.setCursor(0, 1);
      printLn(currentState.message.c_str());
      return;
  }
  
  switch (currentState.deviceState)
  {
    case DEVICE_STATE_BOOTING:
      lcd.clear();
      lcd.setCursor(0, 0);
      printLn("SzakiWare Aegis");
      lcd.setCursor(0, 1);
      printLn("Please wait...");
      break;
    case DEVICE_STATE_ERROR:
      cycleErrors(currentState);
      break;
    case DEVICE_STATE_UPDATING:
      lcd.setCursor(0, 0);
      printLn("OTA Updating");
      lcd.setCursor(0, 1);
      printLn("Please wait...");
      break;
    default:
      lcd.setCursor(0, 1);
      printLn("");
      break;
  }
}

void DZLCDControl::handle()
{ 
  static unsigned long lastHandle = 0;
  if (millis() - lastHandle < 200) return;
  lastHandle = millis();

  GlobalState currentState = stateControl.getSnapshot();
  updateHeader(currentState);

  if(isUpdateNeeded(currentState)) {
    if(!backlightOn) {
      lcd.backlight();
      lcd.display();
      backlightOn = true;
    }
    lcd.display();
    backlightTmr = millis();
    lastState = currentState;
  } else {
    if(millis() - backlightTmr > BACKLIGHT_TIMEOUT) {
      lcd.noBacklight();
      lcd.noDisplay();
      backlightOn = false;
    }
    return;
  }
  
  displayCurrentState(currentState);
}