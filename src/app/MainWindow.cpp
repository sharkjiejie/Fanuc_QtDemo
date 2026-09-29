#include "app/MainWindow.h"

#include "core/MachineController.h"
#include "data/AppDatabase.h"
#include "ui/CncScreen.h"
#include "ui/MachinePanel.h"
#include "ui/MdiPanel.h"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QWidget>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle(QStringLiteral("Fanuc Oi-TF 界面 Demo"));
    setMinimumSize(1240, 740);
    resize(1460, 900);

    database_ = std::make_unique<AppDatabase>();
    controller_ = std::make_unique<MachineController>(database_.get());

    buildUi();
    connectCallbacks();
    applyStyle();

    controller_->initialize();
    controller_->refreshPrograms();
}

MainWindow::~MainWindow() = default;

void MainWindow::setPage(const QString &page)
{
    if (!screen_) {
        return;
    }
    screen_->setPage(page);
    controller_->setParameterPageActive(
        page == QStringLiteral("SYSTEM"));
}

void MainWindow::setMode(const QString &mode)
{
    if (controller_) {
        controller_->setModeCode(mode);
    }
}

void MainWindow::buildUi()
{
    QWidget *central = new QWidget(this);
    central->setObjectName(QStringLiteral("appBackground"));

    QVBoxLayout *rootLayout = new QVBoxLayout(central);
    rootLayout->setContentsMargins(16, 16, 16, 16);
    rootLayout->setSpacing(14);

    QHBoxLayout *topLayout = new QHBoxLayout;
    topLayout->setSpacing(16);
    screen_ = new CncScreen(controller_.get());
    mdiPanel_ = new MdiPanel;
    topLayout->addWidget(screen_, 1);
    topLayout->addWidget(mdiPanel_, 0);
    rootLayout->addLayout(topLayout, 1);

    machinePanel_ = new MachinePanel;
    rootLayout->addWidget(machinePanel_, 0);
    setCentralWidget(central);
}

void MainWindow::connectCallbacks()
{
    controller_->setChangeCallback([this]() {
        screen_->refresh();
    });
    auto applyPage = [this](const QString &page) {
        screen_->setPage(page);
        controller_->setParameterPageActive(
            page == QStringLiteral("SYSTEM"));
    };
    controller_->setPageRequestCallback(applyPage);

    mdiPanel_->setPageHandler(applyPage);
    mdiPanel_->setCommandHandler([this](const QString &command) {
        if (screen_->page() == QStringLiteral("SYSTEM")
            && !command.trimmed().isEmpty()) {
            controller_->applyParameterInput(command);
            return;
        }
        controller_->executeCommand(command);
    });
    mdiPanel_->setEditHandler([this](const QString &action) {
        if (screen_->page() == QStringLiteral("SYSTEM")
            && (action == QStringLiteral("UP")
                || action == QStringLiteral("DOWN"))) {
            controller_->moveParameterCursor(
                action == QStringLiteral("UP") ? -1 : 1);
        } else if (action == QStringLiteral("UP")
            || action == QStringLiteral("DOWN")) {
            const QString query = mdiPanel_->inputText();
            if (query.trimmed().isEmpty()) {
                controller_->moveProgramCursor(
                    action == QStringLiteral("UP") ? -1 : 1);
            } else {
                controller_->searchProgramText(
                    query, action == QStringLiteral("UP"));
            }
        } else if (action == QStringLiteral("LEFT")
                   || action == QStringLiteral("RIGHT")) {
            controller_->moveCharacterCursor(
                action == QStringLiteral("LEFT") ? -1 : 1);
        } else if (action == QStringLiteral("ALTER")) {
            controller_->replaceSelectedProgramText(mdiPanel_->inputText());
        } else if (action == QStringLiteral("INSERT")) {
            controller_->insertProgramText(mdiPanel_->inputText());
        } else if (action == QStringLiteral("DELETE")) {
            controller_->deleteProgramText();
        }
    });
    machinePanel_->setModeHandler([this](const QString &mode) {
        controller_->setModeCode(mode);
    });
    machinePanel_->setCycleStartHandler([this]() {
        controller_->startProgram();
    });
    machinePanel_->setFeedHoldHandler([this]() {
        controller_->pauseProgram();
    });
    machinePanel_->setResetHandler([this]() {
        controller_->resetProgram();
    });
    machinePanel_->setEmergencyStopHandler([this]() {
        controller_->emergencyStop();
    });
    machinePanel_->setOptionalStopHandler([this](bool selected) {
        controller_->setOptionalStopSelected(selected);
    });
    machinePanel_->setSingleBlockHandler([this](bool selected) {
        controller_->setSingleBlockSelected(selected);
    });
}

void MainWindow::applyStyle()
{
    setStyleSheet(
        QStringLiteral(
            "QMainWindow { background: #000000; }"
            "QWidget#appBackground { background: #000000; }"
            "QGroupBox {"
            "  background: qlineargradient("
            "    x1:0, y1:0, x2:0, y2:1,"
            "    stop:0 #ffffff, stop:0.55 #eef1f3, stop:1 #cdd3d8);"
            "  color: #000000; font-weight: bold;"
            "  border-top:2px solid #ffffff;"
            "  border-left:2px solid #ffffff;"
            "  border-right:2px solid #737b83;"
            "  border-bottom:3px solid #59616a;"
            "  border-radius:5px;"
            "  margin-top: 14px; padding-top: 5px;"
            "}"
            "QGroupBox::title { subcontrol-origin: margin; left: 8px; }"
            "QLineEdit {"
            "  background: #000000; color: #ffffff;"
            "  border-top:2px solid #343b42;"
            "  border-left:2px solid #343b42;"
            "  border-right:2px solid #ffffff;"
            "  border-bottom:2px solid #ffffff;"
            "  padding: 2px 6px;"
            "}"
            "QSlider::groove:vertical {"
            "  background:#b9c0c7; width:8px; border-radius:4px;"
            "  border:1px solid #737b83;"
            "}"
            "QSlider::handle:vertical {"
            "  background:qlineargradient("
            "    x1:0, y1:0, x2:0, y2:1,"
            "    stop:0 #ffffff, stop:1 #9fa8b1);"
            "  width:18px; height:18px; margin:0 -6px;"
            "  border:1px solid #5d656d; border-radius:3px;"
            "}"));
}
