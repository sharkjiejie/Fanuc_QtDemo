#ifndef MODEL_TOOLOFFSET_H
#define MODEL_TOOLOFFSET_H

struct ToolOffset
{
    double touchMachineX = 0.0;
    double touchMachineZ = 0.0;
    double absoluteX = 0.0;
    double absoluteZ = 0.0;
    double wearX = 0.0;
    double wearZ = 0.0;
    double radius = 0.0;
    double wearRadius = 0.0;
    int tip = 3;
};

#endif
