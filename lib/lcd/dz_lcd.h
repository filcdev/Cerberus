#ifndef DZ_LCD_H
#define DZ_LCD_H

#include <LiquidCrystal_I2C.h>
#include <string>
#include "dz_state.h"
#include "dz_logger.h"

class DZLCDControl
{
public:
  DZLCDControl();
  void begin();
  void handle();
  void printLn(const char *msg);
  void clear()
  {
    lcd.clear();
  }

private:
  Logger logger;
  bool isUpdateNeeded(const GlobalState& currentState);
  void updateHeader(const GlobalState& currentState);
  void manageBacklight();
  void displayCurrentState(const GlobalState& currentState);
  void cycleErrors(const GlobalState& currentState);

  bool probeI2C();
  void reinitLCD();
  void healthCheck();

  LiquidCrystal_I2C lcd;
  GlobalState lastState;
  std::string lastTime;
  
  int erIndex = 0;
  unsigned long lastErrorSwitch = 0;
  unsigned long backlightTmr = 0;
  bool backlightOn = true;

  bool wasDisconnected = false;
  unsigned long lastProbeTime = 0;
  unsigned long lastPeriodicReinit = 0;
  
  static const int switchInterval = 3000;
  static const unsigned long BACKLIGHT_TIMEOUT = 30000;
  static const unsigned long PROBE_INTERVAL = 1000;           // check I2C every 1s
  static const unsigned long PERIODIC_REINIT_INTERVAL = 60000; // safety reinit every 60s
};

#endif