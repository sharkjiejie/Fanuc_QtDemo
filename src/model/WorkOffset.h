#ifndef MODEL_WORKOFFSET_H
#define MODEL_WORKOFFSET_H

struct WorkOffset
{
    int number = 1;
    double machineX = 0.0;
    double machineZ = 0.0;
};

struct CoordinateState
{
    int activeWorkOffset = 1;
    double relativeOriginMachineX = 0.0;
    double relativeOriginMachineZ = 0.0;
};

#endif
