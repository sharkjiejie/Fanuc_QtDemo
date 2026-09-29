#include "app/MainWindow.h"

#include <QApplication>
#include <QDir>
#include <QFileInfo>
#include <QTimer>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    MainWindow window;
    window.show();

    const QString screenshotPath = qEnvironmentVariable("CNC_DEMO_SCREENSHOT");
    if (!screenshotPath.isEmpty()) {
        QTimer::singleShot(200, &window, [&window, &app, screenshotPath]() {
            QDir().mkpath(QFileInfo(screenshotPath).absolutePath());
            window.grab().save(screenshotPath);
            app.quit();
        });
    }

    return app.exec();
}
