#include <Arduino.h>
#include "dz_config.h"
#include "dz_button.h"
#include "dz_state.h"
#include "dz_ws.h"

DZButton::DZButton() : logger("BTN") {}

void DZButton::begin() {
  logger.info("Initializing Button on pin %d", BUTTON_PIN);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
}

void DZButton::handle() {
  int reading = digitalRead(BUTTON_PIN);

  if (reading != lastButtonState) {
    lastDebounceTime = millis();
  }

  if ((millis() - lastDebounceTime) > 50) { // 50ms debounce time
    if (reading == LOW && buttonState == HIGH) {
      logger.info("Button pressed, opening door");
      if(!stateControl.isDoorOpen()) wsControl.sendCardRead("", true, true);
      stateControl.openDoor();
      stateControl.setHeader("Aegis  <<");
      stateControl.setMessage("Door Open");
    }
    buttonState = reading;
  }

  lastButtonState = reading;
}