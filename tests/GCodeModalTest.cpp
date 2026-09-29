#include "core/GCodeModalEngine.h"
#include "core/GCodeParser.h"

#include <QtMath>

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

void expectNear(double actual, double expected, const char *message)
{
    expect(qAbs(actual - expected) < 0.000001, message);
}

} // namespace

int main()
{
    GCodeParser parser;
    GCodeModalEngine engine;

    const QStringList lines = {
        QStringLiteral("G21 G90 G94 G97"),
        QStringLiteral("T0303"),
        QStringLiteral("S800 M03"),
        QStringLiteral("G01 X50 Z-2 F0.2 M08"),
        QStringLiteral("G91 G95 G96"),
        QStringLiteral("M05 M09")
    };

    for (int i = 0; i < lines.size(); ++i) {
        const GCodeBlock block = parser.parseBlock(lines.at(i), i + 1);
        engine.applyBlock(block);
    }

    const ModalState state = engine.state();
    expect(state.motion == MotionMode::Linear,
           "motion should remain G01");
    expect(state.units == UnitMode::Millimeter,
           "units should remain G21");
    expect(state.distance == DistanceMode::Incremental,
           "distance should switch to G91");
    expect(state.feedMode == FeedMode::PerRevolution,
           "feed mode should switch to G95");
    expect(state.spindleMode == SpindleMode::ConstantSurfaceSpeed,
           "spindle mode should switch to G96");
    expect(state.spindleDirection == SpindleDirection::Stopped,
           "M05 should stop the spindle");
    expect(!state.coolantOn, "M09 should turn coolant off");
    expect(state.toolNumber == 3, "T03 should select tool 3");
    expect(state.toolOffset == 3, "T03 should select offset 3");
    expectNear(state.feed, 0.2, "F value should be retained");
    expectNear(state.spindleSpeed, 800.0, "S value should be retained");
    expect(state.toolCode() == QStringLiteral("T0303"),
           "tool code should be formatted as T0303");
    expect(state.gCodeSummary() == QStringLiteral("G01 G21 G91 G95 G96 G54"),
           "G code summary should reflect modal state");
    if (state.gCodeSummary() != QStringLiteral("G01 G21 G91 G95 G96 G54")) {
        std::cerr << "G summary detail: "
                  << state.gCodeSummary().toStdString() << '\n';
    }

    GCodeModalEngine workOffsetEngine;
    workOffsetEngine.applyBlock(parser.parseBlock(QStringLiteral("G55"), 1));
    expect(workOffsetEngine.state().workOffsetNo == 2,
           "G55 should select the second work offset");

    GCodeModalEngine resetEngine;
    const GCodeBlock rapid = parser.parseBlock(QStringLiteral("G00 M04 M08"), 1);
    resetEngine.applyBlock(rapid);
    expect(resetEngine.state().motionCode() == QStringLiteral("G00"),
           "reset motion should be G00");
    expect(resetEngine.state().spindleDirection == SpindleDirection::Reverse,
           "M04 should set reverse spindle");
    expect(resetEngine.state().coolantOn, "M08 should turn coolant on");

    GCodeModalEngine stopEngine;
    stopEngine.applyBlock(parser.parseBlock(QStringLiteral("M00"), 1));
    expect(stopEngine.state().programStopRequested,
           "M00 should request a program stop");

    GCodeModalEngine optionalEngine;
    optionalEngine.applyBlock(parser.parseBlock(QStringLiteral("M01"), 1));
    expect(!optionalEngine.state().programStopRequested,
           "M01 should not stop when optional stop is disabled");

    optionalEngine.setOptionalStopSelected(true);
    optionalEngine.applyBlock(parser.parseBlock(QStringLiteral("M01"), 2));
    expect(optionalEngine.state().programStopRequested,
           "M01 should stop when optional stop is selected");

    GCodeModalEngine auxiliaryEngine;
    auxiliaryEngine.applyBlock(parser.parseBlock(QStringLiteral("M28"), 1));
    expect(auxiliaryEngine.state().highPressurePumpOn,
           "M28 should turn on high-pressure pump");
    auxiliaryEngine.applyBlock(parser.parseBlock(QStringLiteral("M03"), 2));
    expect(!auxiliaryEngine.state().highPressurePumpOn,
           "M28 should turn off when the next block has no M28");
    auxiliaryEngine.applyBlock(parser.parseBlock(QStringLiteral("M28"), 3));
    expect(auxiliaryEngine.state().highPressurePumpOn,
           "consecutive M28 block should keep high-pressure pump on");
    auxiliaryEngine.applyBlock(parser.parseBlock(QStringLiteral("M93"), 4));
    expect(auxiliaryEngine.state().spindleDirection
               == SpindleDirection::Forward,
           "M93 should start forward spindle");
    expect(auxiliaryEngine.state().coolantOn,
           "M93 should turn coolant on");
    auxiliaryEngine.applyBlock(parser.parseBlock(QStringLiteral("M30"), 3));
    expect(auxiliaryEngine.state().programEnded,
           "M30 should mark program end");
    expect(!auxiliaryEngine.state().highPressurePumpOn,
           "M30 should reset high-pressure pump");
    expect(!auxiliaryEngine.state().coolantOn,
           "M30 should reset coolant");

    return failures == 0 ? 0 : 1;
}
