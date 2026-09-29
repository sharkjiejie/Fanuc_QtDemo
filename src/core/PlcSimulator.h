#ifndef CORE_PLCSIMULATOR_H
#define CORE_PLCSIMULATOR_H

#include "model/ModalState.h"
#include "model/PlcState.h"

class PlcSimulator
{
public:
    PlcSimulator();

    void reset();
    void requestMCode(int code);
    void scan();
    void clearRequest();
    void syncState(const ModalState &state);

    const PlcState &state() const;
    bool mFin() const;
    bool waiting() const;

private:
    PlcState state_;
};

#endif
