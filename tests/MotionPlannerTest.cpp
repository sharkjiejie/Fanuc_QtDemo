#include "core/GCodeParser.h"
#include "core/MotionPlanner.h"

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
    expect(qAbs(actual - expected) < 0.00001, message);
}

} // namespace

int main()
{
    GCodeParser parser;
    MotionPlanner planner;

    ModalState linearState;
    linearState.motion = MotionMode::Linear;
    linearState.distance = DistanceMode::Absolute;
    linearState.units = UnitMode::Millimeter;

    const GCodeBlock linear = parser.parseBlock(QStringLiteral("G01 X50 Z-2"), 1);
    const ProgramInstruction linearPlan = planner.planBlock(
        linear, linearState, QVector3D(10.0, 0.0, 5.0));
    expect(linearPlan.hasMotion, "G01 should create a motion segment");
    expectNear(linearPlan.segment.end.x(), 50.0, "absolute X should be 50");
    expectNear(linearPlan.segment.end.z(), -2.0, "absolute Z should be -2");
    expectNear(linearPlan.segment.length(), qSqrt(40.0 * 40.0 + 7.0 * 7.0),
               "linear length should be correct");

    ModalState incrementalState = linearState;
    incrementalState.distance = DistanceMode::Incremental;
    const GCodeBlock incremental = parser.parseBlock(QStringLiteral("X2 Z-3"), 2);
    const ProgramInstruction incrementalPlan = planner.planBlock(
        incremental, incrementalState, QVector3D(10.0, 0.0, 5.0));
    expectNear(incrementalPlan.segment.end.x(), 12.0, "incremental X should be 12");
    expectNear(incrementalPlan.segment.end.z(), 2.0, "incremental Z should be 2");

    ModalState inchState = linearState;
    inchState.units = UnitMode::Inch;
    const GCodeBlock inch = parser.parseBlock(QStringLiteral("X1 Z2"), 3);
    const ProgramInstruction inchPlan = planner.planBlock(
        inch, inchState, QVector3D(0.0, 0.0, 0.0));
    expectNear(inchPlan.segment.end.x(), 25.4, "inch X should convert to mm");
    expectNear(inchPlan.segment.end.z(), 50.8, "inch Z should convert to mm");

    ModalState arcState;
    arcState.motion = MotionMode::CircularCw;
    const GCodeBlock arc = parser.parseBlock(QStringLiteral("G02 X5 Z5 I5 K0"), 4);
    const ProgramInstruction arcPlan = planner.planBlock(
        arc, arcState, QVector3D(0.0, 0.0, 0.0));
    expect(arcPlan.hasMotion, "G02 should create an arc");
    expectNear(arcPlan.segment.center.x(), 5.0, "arc center X should be 5");
    expectNear(arcPlan.segment.center.z(), 0.0, "arc center Z should be 0");
    expectNear(arcPlan.segment.radius, 5.0, "arc radius should be 5");
    expect(qAbs(arcPlan.segment.arcSweep) > 0.1, "arc sweep should not be zero");

    QStringList arcErrors;
    const GCodeBlock invalidArc = parser.parseBlock(QStringLiteral("G03 X10 Z10"), 5);
    const ProgramInstruction invalidPlan = planner.planBlock(
        invalidArc, arcState, QVector3D(0.0, 0.0, 0.0), &arcErrors);
    expect(!invalidPlan.hasMotion, "arc without I/K/R should not create motion");
    expect(!arcErrors.isEmpty(), "invalid arc should report an error");

    const GCodeParseResult cycleResult = parser.parseProgram({
        QStringLiteral("O2000"),
        QStringLiteral("N10 G00 X62 Z2"),
        QStringLiteral("N20 G71 U2 R1"),
        QStringLiteral("N30 G71 P40 Q60 U0.5 W0.1 F0.2"),
        QStringLiteral("N40 G00 X28"),
        QStringLiteral("N50 G01 Z0"),
        QStringLiteral("N60 X46 Z-20"),
        QStringLiteral("N70 G70 P40 Q60"),
        QStringLiteral("N80 M30")
    });
    QStringList cycleErrors;
    const QVector<ProgramInstruction> expanded = planner.buildProgram(
        cycleResult.blocks, QVector3D(0.0, 0.0, 0.0), &cycleErrors);
    expect(!expanded.isEmpty(), "G71/G70 program should expand");
    int roughSegments = 0;
    int finishSegments = 0;
    for (const ProgramInstruction &instruction : expanded) {
        if (!instruction.hasMotion) {
            continue;
        }
        if (instruction.block.sequenceNumber == 30) {
            ++roughSegments;
        }
        if (instruction.block.sequenceNumber == 70) {
            ++finishSegments;
        }
    }
    expect(roughSegments > 0, "G71 should expand roughing segments");
    expect(finishSegments > 0, "G70 should expand finishing segments");

    int cutPasses = 0;
    int retractXPasses = 0;
    const ProgramInstruction *firstApproach = nullptr;
    const ProgramInstruction *firstCut = nullptr;
    const ProgramInstruction *firstRetractZ = nullptr;
    const ProgramInstruction *firstRetractX = nullptr;
    for (const ProgramInstruction &instruction : expanded) {
        if (instruction.block.sequenceNumber != 30 || !instruction.hasMotion) {
            continue;
        }
        if (instruction.cycleStep == CycleStep::Approach && !firstApproach) {
            firstApproach = &instruction;
        } else if (instruction.cycleStep == CycleStep::Cut) {
            ++cutPasses;
            if (!firstCut) {
                firstCut = &instruction;
            }
        } else if (instruction.cycleStep == CycleStep::RetractZ
                   && !firstRetractZ) {
            firstRetractZ = &instruction;
        } else if (instruction.cycleStep == CycleStep::RetractX) {
            ++retractXPasses;
            if (!firstRetractX) {
                firstRetractX = &instruction;
            }
        }
    }
    expect(cutPasses > 0, "G71 should have rough cutting passes");
    expect(cutPasses == retractXPasses,
           "every G71 cut should have one R retract");
    if (firstApproach && firstCut && firstRetractZ && firstRetractX) {
        expectNear(firstApproach->segment.end.x(), 58.0,
                   "G71 U2 radial depth should remove 4 mm of X diameter");
        expectNear(firstCut->segment.end.x(), 58.0,
                   "first G71 cut should stay at the programmed pass X");
        expectNear(firstCut->segment.end.z(), -19.9,
                   "G71 W0.1 should leave Z finishing allowance");
        expectNear(firstRetractZ->segment.end.x(), 58.0,
                   "G71 should retract Z before moving X");
        expectNear(firstRetractZ->segment.end.z(), 2.0,
                   "G71 Z retract should return to the cycle start Z");
        expectNear(firstRetractX->segment.end.x(), 60.0,
                   "G71 R1 radial retract should clear 2 mm of X diameter");
        expectNear(firstRetractX->segment.end.z(), 2.0,
                   "G71 X retract should occur after the Z retract");
    } else {
        expect(false, "G71 should expose approach, cut, Z retract and X retract");
    }

    bool cycleHasHardError = false;
    for (const QString &message : cycleErrors) {
        if (message.contains(QStringLiteral("找不到"))
            || message.contains(QStringLiteral("缺少"))
            || message.contains(QStringLiteral("无效"))
            || message.contains(QStringLiteral("嵌套"))) {
            cycleHasHardError = true;
        }
    }
    expect(!cycleHasHardError, "valid G71/G70 program should not report hard errors");

    const GCodeParseResult drillingResult = parser.parseProgram({
        QStringLiteral("N10 G00 X0 Z5"),
        QStringLiteral("N20 G74 R1"),
        QStringLiteral("N30 G74 X0 Z-10 Q2 F0.1")
    });
    QStringList drillingErrors;
    const QVector<ProgramInstruction> drilling = planner.buildProgram(
        drillingResult.blocks, QVector3D(10.0, 0.0, 5.0), &drillingErrors);
    double deepestZ = 5.0;
    for (const ProgramInstruction &instruction : drilling) {
        if (instruction.hasMotion) {
            deepestZ = qMin(deepestZ, qMin(instruction.segment.start.z(),
                                           instruction.segment.end.z()));
        }
    }
    expectNear(deepestZ, -10.0, "G74 should reach programmed depth");
    expect(drillingErrors.isEmpty(), "valid G74 should not report errors");

    const GCodeParseResult threadingResult = parser.parseProgram({
        QStringLiteral("N10 G00 X30 Z2"),
        QStringLiteral("N20 G76 P010060 Q0.1 R0.05"),
        QStringLiteral("N30 G76 X27 Z-20 P1.5 Q0.3 F1.5")
    });
    QStringList threadingErrors;
    const QVector<ProgramInstruction> threading = planner.buildProgram(
        threadingResult.blocks, QVector3D(30.0, 0.0, 2.0), &threadingErrors);
    int threadingPasses = 0;
    for (const ProgramInstruction &instruction : threading) {
        if (instruction.hasMotion && instruction.block.sequenceNumber == 30) {
            ++threadingPasses;
        }
    }
    expect(threadingPasses >= 12, "G76 should generate thread and return passes");
    expect(!threadingErrors.isEmpty(),
           "G76 simplification should be reported to the user");
    if (threadingPasses < 12) {
        for (const QString &error : threadingErrors) {
            std::cerr << "G76 detail: " << error.toStdString() << '\n';
        }
    }

    QStringList missingProfileErrors;
    const GCodeParseResult invalidCycleResult = parser.parseProgram({
        QStringLiteral("N10 G71 P100 Q200 U0.5 W0.1 F0.2")
    });
    planner.buildProgram(invalidCycleResult.blocks,
                         QVector3D(0.0, 0.0, 0.0),
                         &missingProfileErrors);
    expect(!missingProfileErrors.isEmpty(),
           "missing G71 profile should report an error");

    return failures == 0 ? 0 : 1;
}
