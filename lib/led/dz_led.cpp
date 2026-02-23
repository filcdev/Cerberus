#include "dz_led.h"
#include "dz_state.h"

DZLEDControl::DZLEDControl() : pixels(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800), logger("LED") {}

void DZLEDControl::testSequence() {
  pixels.fill(pixels.Color(255, 0, 0));
  pixels.show();
  delay(500);
  pixels.fill(pixels.Color(0, 255, 0));
  pixels.show();
  delay(500);
  pixels.fill(pixels.Color(0, 0, 255));
  pixels.show();
  delay(500);
  pixels.clear();
  pixels.show();
}

void DZLEDControl::begin()
{
  logger.info("Initializing LED");
  pixels.begin();
  pixels.clear();
  pixels.show();
  pixels.setBrightness(50);
  
  lastMillis = millis();
  breathePhase = 0.0f;
  
  doorSeqState = 0;
  doorSeqStart = 0;
  doorPulsePhase = 0.0f;
  doorPulsesDone = 0;

  errSeqState = 0;
  errSeqStart = 0;

  testSequence();
}

void DZLEDControl::handleErrorState(unsigned long now) {
    const unsigned long PULSE_MS = 200;
    const unsigned long GAP_BETWEEN_PULSES_MS = 200;
    const unsigned long GAP_AFTER_PAIR_MS = 1000;

    if (errSeqState == 0) {
      errSeqState = 1;
      errSeqStart = now;
    }

    if (errSeqState == 1) {
      unsigned long elapsed = now - errSeqStart;
      if (elapsed >= PULSE_MS) {
        pixels.clear();
        pixels.show();
        errSeqState = 2;
        errSeqStart = now;
      } else {
        float phase = (float)elapsed / (float)PULSE_MS;
        float t = phase * 2.0f;
        if (t > 1.0f) t = 2.0f - t;
        uint8_t v = (uint8_t)(t * 255.0f);
        pixels.fill(pixels.Color(v, 0, 0));
        pixels.show();
      }
      return;
    }

    if (errSeqState == 2) {
      if (now - errSeqStart >= GAP_BETWEEN_PULSES_MS) {
        errSeqState = 3;
        errSeqStart = now;
      }
      return;
    }

    if (errSeqState == 3) {
      unsigned long elapsed = now - errSeqStart;
      if (elapsed >= PULSE_MS) {
        pixels.clear();
        pixels.show();
        errSeqState = 4;
        errSeqStart = now;
      } else {
        float phase = (float)elapsed / (float)PULSE_MS;
        float t = phase * 2.0f;
        if (t > 1.0f) t = 2.0f - t;
        uint8_t v = (uint8_t)(t * 255.0f);
        pixels.fill(pixels.Color(v, 0, 0));
        pixels.show();
      }
      return;
    }

    if (errSeqState == 4) {
      if (now - errSeqStart >= GAP_AFTER_PAIR_MS) {
        errSeqState = 1;
        errSeqStart = now;
      }
      return;
    }
}

void DZLEDControl::handleDoorState(unsigned long now, unsigned long doorOpenedAt) {
    if (doorOpenedAt == 0) {
      pixels.fill(pixels.Color(0, 255, 0));
      pixels.show();
      return;
    }

    unsigned long elapsed = now - doorOpenedAt;
    if (elapsed > DOOR_OPEN_DURATION_MS) elapsed = DOOR_OPEN_DURATION_MS;

    unsigned long remaining = DOOR_OPEN_DURATION_MS - elapsed;
    int sideCount = LED_COUNT / 2;
    if (sideCount <= 0) {
      pixels.clear();
      pixels.show();
      return;
    }

    int litPerSide = (int)((remaining * sideCount + DOOR_OPEN_DURATION_MS - 1) / DOOR_OPEN_DURATION_MS);
    if (litPerSide < 0) litPerSide = 0;
    if (litPerSide > sideCount) litPerSide = sideCount;

    pixels.clear();
    for (int i = 0; i < litPerSide; ++i) {
      pixels.setPixelColor(i, pixels.Color(0, 255, 0));
      pixels.setPixelColor(i + sideCount, pixels.Color(0, 255, 0));
    }
    pixels.show();
}

void DZLEDControl::handleIdleState(unsigned long dt) {
    const float period = 4000.0f;
    breathePhase += (dt / period);
    if (breathePhase >= 1.0f) breathePhase -= floor(breathePhase);
    float t = breathePhase * 2.0f;
    if (t > 1.0f) t = 2.0f - t;
    uint8_t v = (uint8_t)(t * 200.0f);
    pixels.clear();
    pixels.fill(pixels.Color(0, 0, v), 8, 8);
    pixels.show();
}

void DZLEDControl::handle()
{
  unsigned long now = millis();
  unsigned long dt = now - lastMillis;
  if (dt == 0) return;
  lastMillis = now;

  bool isError = (stateControl.getDeviceState() == DEVICE_STATE_ERROR);
  GlobalState snapshot = stateControl.getSnapshot();
  bool isDoor = snapshot.doorOpen;

  if (isDoor) {
    handleDoorState(now, snapshot.doorOpenTmr);
    return;
  } else {
    doorSeqState = 0;
  }

  if (isError) {
    handleErrorState(now);
    doorSeqState = 0;
    return;
  }


  if (stateControl.getDeviceState() == DEVICE_STATE_IDLE) {
    handleIdleState(dt);
    return;
  }
  pixels.clear();
  pixels.show();
}