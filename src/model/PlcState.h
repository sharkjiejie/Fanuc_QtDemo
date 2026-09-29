#ifndef MODEL_PLCSTATE_H
#define MODEL_PLCSTATE_H

struct PlcState
{
    int mCode = -1;
    bool mStrobe = false;
    bool mFin = false;
    bool spindleForward = false;
    bool spindleReverse = false;
    bool spindleStopped = true;
    bool coolantOn = false;
    bool highPressurePumpOn = false;
    int scanCount = 0;
};

#endif
