#ifndef CORE_MOTIONPLANNER_H
#define CORE_MOTIONPLANNER_H

#include "model/GCodeBlock.h"
#include "model/ModalState.h"
#include "model/MotionSegment.h"

class MotionPlanner
{
public:
    QVector<ProgramInstruction> buildProgram(const QVector<GCodeBlock> &blocks,
                                             const QVector3D &initialAbsolute,
                                             QStringList *errors = nullptr,
                                             bool optionalStopSelected = false) const;

    ProgramInstruction planBlock(const GCodeBlock &block,
                                 const ModalState &modalState,
                                 const QVector3D &currentAbsolute,
                                 QStringList *errors = nullptr) const;
};

#endif
