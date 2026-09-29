#ifndef UI_CNCSCREEN_H
#define UI_CNCSCREEN_H

#include <QString>
#include <QStringList>
#include <QWidget>

class MachineController;
class QMouseEvent;
class QPainter;

class CncScreen : public QWidget
{
public:
    explicit CncScreen(MachineController *controller = nullptr,
                       QWidget *parent = nullptr);

    QSize minimumSizeHint() const override;
    QSize sizeHint() const override;

    void setController(MachineController *controller);
    void refresh();
    void setPage(const QString &page);
    QString page() const;

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;

private:
    void drawPage(QPainter &painter);
    void drawProgram(QPainter &painter);
    void drawPositionPage(QPainter &painter);
    void drawOffsetPage(QPainter &painter);
    void drawSystemPage(QPainter &painter);
    void drawMessagePage(QPainter &painter);
    void drawGraphPage(QPainter &painter);
    void drawDirectoryPage(QPainter &painter);
    void drawPositionPanel(QPainter &painter);
    void drawStatusLine(QPainter &painter);
    void drawCommandLine(QPainter &painter);
    void drawSoftKeys(QPainter &painter);
    QStringList softKeyLabels() const;
    int softKeyIndexAt(double baseX) const;

    void drawCoordinateBlock(QPainter &painter, int x, int y,
                             const QString &title, double xValue, double zValue);
    void drawToolOffsetBlock(QPainter &painter, int x, int y,
                             const QString &title, const struct ToolOffset &tool);
    void drawCartesianAxes(QPainter &painter);
    void drawText(QPainter &painter, int x, int y, int w, int h, int pixelSize,
                  const QString &text, const QColor &color, bool mono = false) const;
    QString formatTime(int totalSeconds) const;

    MachineController *controller_ = nullptr;
    QString screenPage_ = QStringLiteral("PROG");
    QString offsetMode_ = QStringLiteral("GEOMETRY");
    int selectedProgramIndex_ = 0;
};

#endif
