#ifndef APP_MAINWINDOW_H
#define APP_MAINWINDOW_H

#include <QMainWindow>

#include <memory>

class AppDatabase;
class CncScreen;
class MachineController;
class MachinePanel;
class MdiPanel;

class MainWindow : public QMainWindow
{
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private:
    void buildUi();
    void connectCallbacks();
    void applyStyle();

    std::unique_ptr<AppDatabase> database_;
    std::unique_ptr<MachineController> controller_;
    CncScreen *screen_ = nullptr;
    MdiPanel *mdiPanel_ = nullptr;
    MachinePanel *machinePanel_ = nullptr;
};

#endif
