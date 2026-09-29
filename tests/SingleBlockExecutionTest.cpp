#include "core/MachineController.h"
#include "ui/MachinePanel.h"

#include <QApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QPushButton>
#include <QThread>
#include <QTimer>
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

QPushButton *findButton(QWidget *root, const QString &text)
{
    const QList<QPushButton *> buttons = root->findChildren<QPushButton *>();
    for (QPushButton *button : buttons) {
        if (button->text() == text) {
            return button;
        }
    }
    return nullptr;
}

bool waitForState(MachineController &controller, const QString &state,
                  int timeoutMs)
{
    QElapsedTimer elapsed;
    elapsed.start();
    while (elapsed.elapsed() < timeoutMs) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 5);
        if (controller.stateText() == state) {
            return true;
        }
        QThread::msleep(1);
    }
    return controller.stateText() == state;
}

} // namespace

int main(int argc, char *argv[])
{
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);

    MachineController controller(nullptr);
    QTimer *timer = controller.findChild<QTimer *>();
    if (timer) {
        timer->setInterval(0);
    }

    MachinePanel panel;
    QPushButton *singleBlockButton =
        findButton(&panel, QStringLiteral("单程序块"));
    expect(singleBlockButton != nullptr,
           "machine panel should contain the single-block button");
    if (!singleBlockButton) {
        return 1;
    }

    bool panelHandlerCalled = false;
    panel.setSingleBlockHandler([&](bool selected) {
        panelHandlerCalled = true;
        controller.setSingleBlockSelected(selected);
    });
    singleBlockButton->click();
    expect(panelHandlerCalled,
           "single-block button should call the panel handler");
    expect(controller.singleBlockSelected(),
           "machine controller should receive the single-block state");

    controller.setModeCode(QStringLiteral("MEM"));
    controller.startProgram();
    expect(waitForState(controller, QStringLiteral("暂停中"), 10000),
           "single block should stop after a non-motion program block");
    expect(controller.currentLine() == 0,
           "first single-block stop should be the first program block");

    bool foundFirstG71Cut = false;
    for (int attempt = 0; attempt < 20 && !foundFirstG71Cut; ++attempt) {
        controller.startProgram();
        if (!waitForState(controller, QStringLiteral("暂停中"), 10000)) {
            break;
        }
        foundFirstG71Cut =
            qAbs(controller.absoluteX() - 57.0) < 0.0001
            && qAbs(controller.absoluteZ() + 64.9) < 0.0001;
    }
    expect(foundFirstG71Cut,
           "single block should stop at the first G71 cutting endpoint");
    expect(!controller.isRunning(),
           "single-block pause should stop the program timer");
    expect(controller.absoluteZ() < 0.0,
           "single block should not retract immediately after cutting");
    const double firstCutX = controller.absoluteX();
    const int cycleLine = controller.currentLine();

    controller.startProgram();
    expect(waitForState(controller, QStringLiteral("暂停中"), 10000),
           "next cycle start should execute the safe retract");
    expect(qAbs(controller.absoluteX() - 58.0) < 0.0001,
           "G71 X retract should clear the tool by 2R");
    expect(qAbs(controller.absoluteZ() - 2.0) < 0.0001,
           "G71 Z retract should happen before the X retract");

    controller.startProgram();
    controller.startProgram();
    controller.startProgram();
    expect(waitForState(controller, QStringLiteral("暂停中"), 10000),
           "repeated cycle-start presses should resume only once");
    const double secondPauseX = controller.absoluteX();
    expect(secondPauseX < firstCutX,
           "resume should execute the next deeper G71 cut");
    expect(controller.absoluteZ() < 0.0,
           "next G71 cut should also stop before retracting");
    expect(controller.currentLine() == cycleLine,
           "resume should stay inside the same G71 cycle");

    return failures == 0 ? 0 : 1;
}
