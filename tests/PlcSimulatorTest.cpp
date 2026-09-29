#include "core/PlcSimulator.h"

#include <iostream>

namespace {

int failures = 0;

void expect(bool condition, const char *message)
{
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

} // namespace

int main()
{
    PlcSimulator plc;
    plc.requestMCode(3);
    expect(plc.state().mStrobe, "M03 request should set strobe");
    expect(!plc.state().mFin, "M03 request should wait for PLC finish");
    expect(plc.state().spindleForward, "M03 should request forward spindle");
    plc.scan();
    expect(plc.mFin(), "simulated PLC should finish in one scan");
    plc.clearRequest();
    expect(!plc.waiting(), "request should be cleared after finish");

    plc.requestMCode(28);
    plc.scan();
    expect(plc.state().highPressurePumpOn,
           "M28 should turn on high-pressure pump");

    ModalState nextState;
    nextState.highPressurePumpOn = false;
    plc.syncState(nextState);
    expect(!plc.state().highPressurePumpOn,
           "next block without M28 should turn high-pressure pump off");

    plc.requestMCode(93);
    plc.scan();
    expect(plc.state().spindleForward && plc.state().coolantOn,
           "M93 should set forward spindle and coolant");

    plc.requestMCode(30);
    plc.scan();
    expect(plc.state().spindleStopped && !plc.state().coolantOn,
           "M30 should stop spindle and coolant");

    return failures == 0 ? 0 : 1;
}
