#ifndef CORE_GCODEMODALENGINE_H
#define CORE_GCODEMODALENGINE_H

#include "model/GCodeBlock.h"
#include "model/ModalState.h"

#include <QStringList>

struct ModalUpdateResult
{
    QStringList messages;
};

class GCodeModalEngine
{
public:
    GCodeModalEngine();

    void reset();
    void setState(const ModalState &state);
    void setOptionalStopSelected(bool selected);
    ModalUpdateResult applyBlock(const GCodeBlock &block);
    const ModalState &state() const;

private:
    ModalState state_;
};

#endif
