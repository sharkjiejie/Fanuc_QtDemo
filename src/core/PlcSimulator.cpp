#include "core/PlcSimulator.h"

PlcSimulator::PlcSimulator()
{
    reset();
}

void PlcSimulator::reset()
{
    state_ = PlcState();
}

void PlcSimulator::requestMCode(int code)
{
    state_.mCode = code;
    state_.mStrobe = true;
    state_.mFin = false;
    state_.scanCount = 0;

    switch (code) {
    case 3:
        state_.spindleForward = true;
        state_.spindleReverse = false;
        state_.spindleStopped = false;
        break;
    case 4:
        state_.spindleForward = false;
        state_.spindleReverse = true;
        state_.spindleStopped = false;
        break;
    case 5:
        state_.spindleForward = false;
        state_.spindleReverse = false;
        state_.spindleStopped = true;
        break;
    case 8:
        state_.coolantOn = true;
        break;
    case 9:
        state_.coolantOn = false;
        break;
    case 28:
        state_.highPressurePumpOn = true;
        break;
    case 93:
        state_.spindleForward = true;
        state_.spindleReverse = false;
        state_.spindleStopped = false;
        state_.coolantOn = true;
        break;
    case 30:
        state_.spindleForward = false;
        state_.spindleReverse = false;
        state_.spindleStopped = true;
        state_.coolantOn = false;
        state_.highPressurePumpOn = false;
        break;
    default:
        break;
    }
}

void PlcSimulator::scan()
{
    if (!state_.mStrobe) {
        return;
    }
    ++state_.scanCount;

    // The simulated PLC completes simple M-code outputs in one scan.
    // A real implementation would wait for spindle, valve or turret feedback.
    state_.mFin = true;
}

void PlcSimulator::clearRequest()
{
    state_.mCode = -1;
    state_.mStrobe = false;
    state_.mFin = false;
    state_.scanCount = 0;
}

void PlcSimulator::syncState(const ModalState &state)
{
    state_.spindleForward =
        state.spindleDirection == SpindleDirection::Forward;
    state_.spindleReverse =
        state.spindleDirection == SpindleDirection::Reverse;
    state_.spindleStopped =
        state.spindleDirection == SpindleDirection::Stopped;
    state_.coolantOn = state.coolantOn;
    state_.highPressurePumpOn = state.highPressurePumpOn;
}

const PlcState &PlcSimulator::state() const
{
    return state_;
}

bool PlcSimulator::mFin() const
{
    return state_.mFin;
}

bool PlcSimulator::waiting() const
{
    return state_.mStrobe && !state_.mFin;
}
