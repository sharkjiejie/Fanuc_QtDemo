#ifndef MODEL_MOTIONSEGMENT_H
#define MODEL_MOTIONSEGMENT_H

#include "model/GCodeBlock.h"
#include "model/ModalState.h"

#include <QVector3D>

enum class MotionCommand
{
    None,
    Rapid,
    Linear,
    ArcCw,
    ArcCcw
};

enum class CycleStep
{
    None,
    Approach,
    Cut,
    RetractZ,
    RetractX
};

struct MotionSegment
{
    MotionCommand command = MotionCommand::None;
    QVector3D start;
    QVector3D end;
    QVector3D center;
    double radius = 0.0;
    double arcStartAngle = 0.0;
    double arcSweep = 0.0;
    double feed = 0.0;
    int lineNumber = 0;
    bool hasMotion = false;

    QVector3D positionAt(double progress) const;
    double length() const;
};

struct ProgramInstruction
{
    GCodeBlock block;
    ModalState modalState;
    MotionSegment segment;
    int lineNumber = 0;
    bool hasMotion = false;
    CycleStep cycleStep = CycleStep::None;
    int cycleId = -1;
    int cyclePass = -1;
};

#endif
