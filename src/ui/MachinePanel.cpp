#include "ui/MachinePanel.h"

#include "ui/PanelStyle.h"
#include "ui/RotaryDial.h"

#include <QButtonGroup>
#include <QFrame>
#include <QFont>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSizePolicy>
#include <QVBoxLayout>

QPushButton *MachinePanel::makeButton(const QString &text, bool checkable,
                                      const QString &colorStyle)
{
    QPushButton *button = new QPushButton(text);
    button->setCheckable(checkable);
    button->setMinimumSize(58, 32);
    button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    button->setStyleSheet(panelButtonStyle(
        colorStyle.isEmpty() ? QStringLiteral("#111111") : colorStyle));
    return button;
}

QWidget *MachinePanel::makeOperationGroup()
{
    QGroupBox *group = new QGroupBox;
    group->setStyleSheet(QStringLiteral("QGroupBox { margin-top:0; padding-top:0; }"));
    QGridLayout *layout = new QGridLayout(group);
    layout->setContentsMargins(10, 12, 10, 10);
    layout->setHorizontalSpacing(6);
    layout->setVerticalSpacing(7);

    const QStringList modeLabels = {
        QStringLiteral("编辑模式"), QStringLiteral("自动模式"),
        QStringLiteral("MDI")
    };
    const QStringList modeCodes = {
        QStringLiteral("EDIT"), QStringLiteral("MEM"),
        QStringLiteral("MDI")
    };
    modeGroup_ = new QButtonGroup(this);
    modeGroup_->setExclusive(true);

    for (int i = 0; i < modeLabels.size(); ++i) {
        QPushButton *button = new QPushButton(modeLabels.at(i), group);
        button->setCheckable(true);
        button->setMinimumSize(82, 32);
        button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        button->setStyleSheet(panelButtonStyle(QStringLiteral("#111111")));
        modeGroup_->addButton(button, i);
        layout->addWidget(button, 0, i);

        const QString code = modeCodes.at(i);
        connect(button, &QPushButton::clicked, this, [this, code]() {
            if (modeHandler_) {
                modeHandler_(code);
            }
        });
        if (code == QStringLiteral("MEM")) {
            button->setChecked(true);
        }
    }

    const QStringList labels = {
        QStringLiteral("连续运转"), QStringLiteral("切削油"),
        QStringLiteral("程序确认"), QStringLiteral("单程序块"),
        QStringLiteral("试运行"), QStringLiteral("选择停止")
    };
    QPushButton *singleBlock = nullptr;
    for (int i = 0; i < labels.size(); ++i) {
        QPushButton *button = makeButton(labels.at(i), true);
        layout->addWidget(button, 1 + i / 3, i % 3);
        if (labels.at(i) == QStringLiteral("单程序块")) {
            singleBlock = button;
        }
        if (labels.at(i) == QStringLiteral("选择停止")) {
            connect(button, &QPushButton::toggled, this, [this](bool checked) {
                if (optionalStopHandler_) {
                    optionalStopHandler_(checked);
                }
            });
        }
    }
    if (singleBlock) {
        connect(singleBlock, &QPushButton::toggled, this, [this](bool checked) {
            if (singleBlockHandler_) {
                singleBlockHandler_(checked);
            }
        });
    }

    QLabel *blockSkipLabel = new QLabel(QStringLiteral("单块跳转"), group);
    blockSkipLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(blockSkipLabel, 3, 0, 1, 3);

    QPushButton *restart1 = makeButton(QStringLiteral("/1"), true);
    QPushButton *restart2 = makeButton(QStringLiteral("/2"), true);
    QPushButton *restart3 = makeButton(QStringLiteral("/3"), true);
    layout->addWidget(restart1, 4, 0);
    layout->addWidget(restart2, 4, 1);
    layout->addWidget(restart3, 4, 2);

    QPushButton *start = makeButton(QStringLiteral("自动启动"),
                                    false, QStringLiteral("#1e7a40"));
    QPushButton *hold = makeButton(QStringLiteral("暂停"),
                                   false, QStringLiteral("#b7790b"));
    start->setMinimumSize(90, 44);
    hold->setMinimumSize(90, 44);
    QHBoxLayout *runLayout = new QHBoxLayout;
    runLayout->setSpacing(6);
    runLayout->addWidget(start, 1);
    runLayout->addWidget(hold, 1);
    layout->addLayout(runLayout, 5, 0, 1, 3);

    connect(start, &QPushButton::clicked, this, [this]() {
        if (cycleStartHandler_) {
            cycleStartHandler_();
        }
    });
    connect(hold, &QPushButton::clicked, this, [this]() {
        if (feedHoldHandler_) {
            feedHoldHandler_();
        }
    });
    return group;
}

QWidget *MachinePanel::makeCenterGroup()
{
    QGroupBox *group = new QGroupBox;
    group->setStyleSheet(QStringLiteral("QGroupBox { margin-top:0; padding-top:0; }"));
    QGridLayout *layout = new QGridLayout(group);
    layout->setContentsMargins(10, 12, 10, 10);
    layout->setHorizontalSpacing(6);
    layout->setVerticalSpacing(7);

    QPushButton *hd1 = makeButton(QStringLiteral("HD1"), true);
    QPushButton *hd2 = makeButton(QStringLiteral("HD2"), true);
    QPushButton *hd3 = makeButton(QStringLiteral("HD3"), true);
    hd1->setChecked(true);
    layout->addWidget(hd1, 0, 0);
    layout->addWidget(hd2, 0, 1);
    layout->addWidget(hd3, 0, 2);

    QPushButton *turret = makeButton(QStringLiteral("刀塔"));
    layout->addWidget(turret, 1, 1);

    QLabel *axisTitle = new QLabel(QStringLiteral("轴向选择"), group);
    axisTitle->setAlignment(Qt::AlignCenter);
    layout->addWidget(axisTitle, 2, 0, 1, 3);

    const QStringList axes = {
        QStringLiteral("X"), QStringLiteral("Z"), QStringLiteral("Y"),
        QStringLiteral("B"), QStringLiteral("C"), QStringLiteral("ZS")
    };
    QButtonGroup *axisGroup = new QButtonGroup(group);
    axisGroup->setExclusive(true);
    for (int i = 0; i < axes.size(); ++i) {
        QPushButton *button = makeButton(axes.at(i), true);
        axisGroup->addButton(button, i);
        layout->addWidget(button, 3 + i / 3, i % 3);
        if (i == 0) {
            button->setChecked(true);
        }
    }

    QPushButton *auxiliary = makeButton(QStringLiteral("辅助装置"), true);
    layout->addWidget(auxiliary, 5, 1);
    return group;
}

QWidget *MachinePanel::makeMotionGroup()
{
    QGroupBox *group = new QGroupBox;
    group->setStyleSheet(QStringLiteral("QGroupBox { margin-top:0; padding-top:0; }"));
    QVBoxLayout *outer = new QVBoxLayout(group);
    outer->setContentsMargins(10, 12, 10, 10);
    outer->setSpacing(8);

    QHBoxLayout *content = new QHBoxLayout;
    content->setSpacing(10);

    QWidget *jogArea = new QWidget(group);
    QGridLayout *jog = new QGridLayout(jogArea);
    jog->setContentsMargins(0, 0, 0, 0);
    jog->setHorizontalSpacing(6);
    jog->setVerticalSpacing(6);

    QPushButton *manual = makeButton(QStringLiteral("手动"), true);
    QPushButton *handwheel = makeButton(QStringLiteral("手轮"), true);
    QPushButton *origin = makeButton(QStringLiteral("原点复位"), true);
    QButtonGroup *manualGroup = new QButtonGroup(jogArea);
    manualGroup->setExclusive(true);
    manualGroup->addButton(manual, 0);
    manualGroup->addButton(handwheel, 1);
    manual->setChecked(true);
    jog->addWidget(manual, 0, 0);
    jog->addWidget(handwheel, 0, 1);
    jog->addWidget(origin, 0, 2);

    QLabel *manualLabel = new QLabel(QStringLiteral("手动"), jogArea);
    manualLabel->setAlignment(Qt::AlignCenter);
    QPushButton *up = makeButton(QStringLiteral("↑"));
    QPushButton *move = makeButton(QStringLiteral("移动"));
    QPushButton *left = makeButton(QStringLiteral("←"));
    QPushButton *rapid = makeButton(QStringLiteral("快进"));
    QPushButton *right = makeButton(QStringLiteral("→"));
    QPushButton *down = makeButton(QStringLiteral("↓"));
    QPushButton *returnButton = makeButton(QStringLiteral("返回"));
    jog->addWidget(manualLabel, 1, 0);
    jog->addWidget(up, 1, 1);
    jog->addWidget(move, 1, 2);
    jog->addWidget(left, 2, 0);
    jog->addWidget(rapid, 2, 1);
    jog->addWidget(right, 2, 2);
    jog->addWidget(down, 3, 1);
    jog->addWidget(returnButton, 3, 2);
    content->addWidget(jogArea, 3);

    QFrame *divider = new QFrame(group);
    divider->setFrameShape(QFrame::VLine);
    divider->setFrameShadow(QFrame::Plain);
    divider->setLineWidth(2);
    content->addWidget(divider);

    QWidget *spindleArea = new QWidget(group);
    QVBoxLayout *spindle = new QVBoxLayout(spindleArea);
    spindle->setContentsMargins(0, 0, 0, 0);
    spindle->setSpacing(6);
    QLabel *spindleTitle = new QLabel(QStringLiteral("主轴"), spindleArea);
    spindleTitle->setAlignment(Qt::AlignCenter);
    QFont titleFont = spindleTitle->font();
    titleFont.setBold(true);
    spindleTitle->setFont(titleFont);
    spindle->addWidget(spindleTitle);

    QPushButton *sp1 = makeButton(QStringLiteral("SP1"), true);
    QPushButton *sp2 = makeButton(QStringLiteral("SP2"), true);
    sp1->setChecked(true);
    QHBoxLayout *spindleSelection = new QHBoxLayout;
    spindleSelection->setSpacing(6);
    spindleSelection->addWidget(sp1, 1);
    spindleSelection->addWidget(sp2, 1);
    spindle->addLayout(spindleSelection);

    QPushButton *powerHead = makeButton(QStringLiteral("动力头"), true);
    QPushButton *spindleStart = makeButton(QStringLiteral("启动"),
                                           false, QStringLiteral("#1e7a40"));
    QPushButton *spindleStop = makeButton(QStringLiteral("停止"),
                                          false, QStringLiteral("#a12f25"));
    QPushButton *spindleJog = makeButton(QStringLiteral("寸动"),
                                         false, QStringLiteral("#b7790b"));
    QPushButton *spindleReverse = makeButton(QStringLiteral("反转"),
                                             false, QStringLiteral("#315f9e"));
    spindle->addWidget(powerHead);
    spindle->addWidget(spindleStart);
    spindle->addWidget(spindleStop);
    spindle->addWidget(spindleJog);
    spindle->addWidget(spindleReverse);
    content->addWidget(spindleArea, 1);
    outer->addLayout(content, 1);
    return group;
}

QWidget *MachinePanel::makeOverrideGroup()
{
    QGroupBox *group = new QGroupBox;
    group->setStyleSheet(QStringLiteral("QGroupBox { margin-top:0; padding-top:0; }"));
    QVBoxLayout *layout = new QVBoxLayout(group);
    layout->setContentsMargins(8, 12, 8, 10);
    layout->setSpacing(7);

    QHBoxLayout *titleRow = new QHBoxLayout;
    QFrame *leftLine = new QFrame(group);
    leftLine->setFrameShape(QFrame::HLine);
    QLabel *icon = new QLabel(QStringLiteral("◉"), group);
    QFrame *rightLine = new QFrame(group);
    rightLine->setFrameShape(QFrame::HLine);
    titleRow->addWidget(leftLine, 1);
    titleRow->addWidget(icon);
    titleRow->addWidget(rightLine, 1);
    layout->addLayout(titleRow);

    QPushButton *x1 = makeButton(QStringLiteral("x1"), true);
    QPushButton *x10 = makeButton(QStringLiteral("x10"), true);
    QPushButton *x100 = makeButton(QStringLiteral("x100"), true);
    QButtonGroup *mpgGroup = new QButtonGroup(group);
    mpgGroup->setExclusive(true);
    mpgGroup->addButton(x1, 1);
    mpgGroup->addButton(x10, 10);
    mpgGroup->addButton(x100, 100);
    x10->setChecked(true);
    QHBoxLayout *mpgLayout = new QHBoxLayout;
    mpgLayout->setSpacing(5);
    mpgLayout->addWidget(x1);
    mpgLayout->addWidget(x10);
    mpgLayout->addWidget(x100);
    layout->addLayout(mpgLayout);

    RotaryDial *handwheel = new RotaryDial(group);
    handwheel->setRange(0, 100);
    handwheel->setValue(50);
    handwheel->setLabel(QStringLiteral("手轮"));
    layout->addWidget(handwheel, 2);

    RotaryDial *feedDial = new RotaryDial(group);
    feedDial->setRange(0, 150);
    feedDial->setValue(100);
    feedDial->setLabel(QStringLiteral("进给"));
    layout->addWidget(feedDial, 1);
    return group;
}

MachinePanel::MachinePanel(QWidget *parent)
    : QWidget(parent)
{
    setMinimumHeight(245);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    QHBoxLayout *root = new QHBoxLayout(this);
    root->setContentsMargins(0, 10, 0, 0);
    root->setSpacing(12);

    auto addGroup = [root](QWidget *group, int stretch) {
        group->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        root->addWidget(group, stretch);
    };
    addGroup(makeOperationGroup(), 3);
    addGroup(makeCenterGroup(), 2);
    addGroup(makeMotionGroup(), 4);
    addGroup(makeOverrideGroup(), 2);
}

void MachinePanel::setModeHandler(std::function<void(const QString &)> handler)
{
    modeHandler_ = std::move(handler);
}

void MachinePanel::setCycleStartHandler(std::function<void()> handler)
{
    cycleStartHandler_ = std::move(handler);
}

void MachinePanel::setFeedHoldHandler(std::function<void()> handler)
{
    feedHoldHandler_ = std::move(handler);
}

void MachinePanel::setResetHandler(std::function<void()> handler)
{
    resetHandler_ = std::move(handler);
}

void MachinePanel::setEmergencyStopHandler(std::function<void()> handler)
{
    emergencyStopHandler_ = std::move(handler);
}

void MachinePanel::setOptionalStopHandler(std::function<void(bool)> handler)
{
    optionalStopHandler_ = std::move(handler);
}

void MachinePanel::setSingleBlockHandler(std::function<void(bool)> handler)
{
    singleBlockHandler_ = std::move(handler);
}
