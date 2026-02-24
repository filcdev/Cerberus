#ifndef DZ_LED_H
#define DZ_LED_H
#include <Adafruit_NeoPixel.h>
#include "dz_config.h"
#include "dz_logger.h"

class DZLEDControl
{
public:
    DZLEDControl();
    void begin();
    void handle();
private:
    Logger logger;
    Adafruit_NeoPixel pixels;
    void testSequence();

    unsigned long lastMillis;
    float breathePhase;
    int doorSeqState;
    unsigned long doorSeqStart;
    float doorPulsePhase;
    int doorPulsesDone;
    int errSeqState;
    unsigned long errSeqStart;

    int deniedSeqState;
    unsigned long deniedSeqStart;
    int deniedFlashCount;

    void handleErrorState(unsigned long now);
    void handleDoorState(unsigned long now, unsigned long doorOpenedAt);
    void handleIdleState(unsigned long dt);
    void handleAccessDeniedState(unsigned long now);
    void handleUpdatingState(unsigned long now);

    int otaSeqState;
    unsigned long otaSeqStart;
    float otaSpinPhase;
    int lastOtaProgress;
};

#endif