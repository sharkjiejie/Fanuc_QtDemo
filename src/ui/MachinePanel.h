#ifndef UI_MACHINEPANEL_H
#define UI_MACHINEPANEL_H

#include <functional>
#include <QWidget>

class QButtonGroup;
class QPushButton;

class MachinePanel : public QWidget
{
public:
    explicit MachinePanel(QWidget *parent = nullptr);

    void setModeHandler(std::function<void(const QString &)> handler);
    void setCycleStartHandler(std::function<void()> handler);
    void setFeedHoldHandler(std::function<void()> handler);
    void setResetHandler(std::function<void()> handler);
    void setEmergencyStopHandler(std::function<void()> handler);
    void setOptionalStopHandler(std::function<void(bool)> handler);
    void setSingleBlockHandler(std::function<void(bool)> handler);

private:
    QWidget *makeOperationGroup();
    QWidget *makeCenterGroup();
    QWidget *makeMotionGroup();
    QWidget *makeOverrideGroup();
    QPushButton *makeButton(const QString &text, bool checkable = false,
                            const QString &colorStyle = QString());

    QButtonGroup *modeGroup_ = nullptr;
    std::function<void(const QString &)> modeHandler_;
    std::function<void()> cycleStartHandler_;
    std::function<void()> feedHoldHandler_;
    std::function<void()> resetHandler_;
    std::function<void()> emergencyStopHandler_;
    std::function<void(bool)> optionalStopHandler_;
    std::function<void(bool)> singleBlockHandler_;
};

#endif
