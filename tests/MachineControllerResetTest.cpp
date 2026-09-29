#include "core/MachineController.h"

#include <QCoreApplication>

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

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    MachineController controller(nullptr);

    controller.setModeCode(QStringLiteral("MEM"));
    controller.executeCommand(QStringLiteral("M03 M08"));
    expect(controller.modalState().spindleDirection
               == SpindleDirection::Forward,
           "automatic setup should start the spindle");
    expect(controller.modalState().coolantOn,
           "automatic setup should turn coolant on");

    controller.executeCommand(QStringLiteral("RESET"));
    expect(!controller.isRunning(),
           "automatic reset should stop program execution");
    expect(controller.modalState().spindleDirection
               == SpindleDirection::Stopped,
           "automatic reset should stop the spindle");
    expect(!controller.modalState().coolantOn,
           "automatic reset should stop coolant");
    expect(!controller.modalState().highPressurePumpOn,
           "automatic reset should stop high-pressure pump");
    expect(controller.stateText() == QStringLiteral("复位停止"),
           "automatic reset should report stopped state");

    controller.setModeCode(QStringLiteral("EDIT"));
    controller.moveProgramCursor(2);
    controller.moveCharacterCursor(3);
    expect(controller.selectedProgramLine() == 2,
           "edit cursor should move before reset");
    expect(controller.selectedProgramColumn() == 3,
           "character cursor should move before reset");

    controller.resetProgram();
    expect(controller.currentLine() == 0,
           "edit reset should return the program cursor to the first line");
    expect(controller.selectedProgramLine() == 0,
           "edit reset should return the selected line to the first line");
    expect(controller.selectedProgramColumn() == 0,
           "edit reset should clear the character cursor");
    expect(controller.selectedProgramLength() == 0,
           "edit reset should clear the character selection");
    expect(controller.stateText() == QStringLiteral("就绪"),
           "edit reset should return to ready state");

    controller.setModeCode(QStringLiteral("MEM"));
    controller.selectParameter(0);
    expect(!controller.applyParameterInput(QStringLiteral("1")),
           "parameter writing should be rejected outside MDI or emergency");

    controller.setModeCode(QStringLiteral("MDI"));
    expect(controller.applyParameterInput(QStringLiteral("1")),
           "write-enable parameter should accept 1 in MDI mode");
    expect(controller.parameterWriteEnabled(),
           "parameter write should be enabled after setting parameter 0");

    controller.selectParameter(4);
    expect(controller.applyParameterInput(QStringLiteral("35")),
           "I/O channel 35 should be accepted");
    expect(!controller.applyParameterInput(QStringLiteral("36")),
           "I/O channel 36 should be rejected");

    controller.setModeCode(QStringLiteral("MEM"));
    expect(!controller.parameterWriteEnabled(),
           "leaving MDI should disable parameter writing");

    return failures == 0 ? 0 : 1;
}
