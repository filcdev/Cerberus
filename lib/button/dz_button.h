#ifndef DZ_BUTTON_H
#define DZ_BUTTON_H

#include "dz_logger.h"

class DZButton
{
public:
  DZButton();
  void begin();
  void handle();
private:
  Logger logger;
  int buttonState = HIGH;
  int lastButtonState = LOW;
  unsigned long lastDebounceTime = 0;
};

#endif