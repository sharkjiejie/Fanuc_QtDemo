#include "ui/CncScreen.h"

#include "core/MachineController.h"
#include "model/ProgramRecord.h"
#include "model/ToolOffset.h"

#include <QFileInfo>
#include <QFont>
#include <QFontMetrics>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QSizePolicy>
#include <QStringList>

namespace {

const QColor kScreenBackground(244, 246, 248);
const QColor kScreenHeading(28, 34, 39);
const QColor kScreenMuted(100, 110, 118);
const QColor kScreenStrong(18, 22, 26);
const QColor kScreenText(42, 50, 56);
const QColor kStateGreen(30, 120, 70);
const QColor kStateRed(190, 42, 38);
const QColor kHighlight(220, 224, 228);
const QColor kSelection(186, 222, 245);

} // namespace

CncScreen::CncScreen(MachineController *controller, QWidget *parent)
    : QWidget(parent)
    , controller_(controller)
{
    setMinimumSize(640, 460);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

QSize CncScreen::minimumSizeHint() const
{
    return QSize(640, 460);
}

QSize CncScreen::sizeHint() const
{
    return QSize(1024, 768);
}

void CncScreen::setController(MachineController *controller)
{
    controller_ = controller;
    update();
}

void CncScreen::refresh()
{
    if (!controller_) {
        update();
        return;
    }
    if (selectedProgramIndex_ >= controller_->programRecords().size()) {
        selectedProgramIndex_ = qMax(0, controller_->programRecords().size() - 1);
    }
    update();
}

void CncScreen::setPage(const QString &page)
{
    screenPage_ = page;
    update();
}

QString CncScreen::page() const
{
    return screenPage_;
}

void CncScreen::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, false);
    painter.fillRect(rect(), QColor(8, 8, 8));

    const double scale = qMin(double(width()) / 1024.0, double(height()) / 768.0);
    painter.translate((width() - 1024.0 * scale) / 2.0,
                      (height() - 768.0 * scale) / 2.0);
    painter.scale(scale, scale);

    painter.fillRect(0, 0, 1024, 768, kScreenBackground);
    painter.setPen(QColor(180, 188, 194));
    painter.drawRect(0, 0, 1023, 767);

    const int programNo = controller_ ? controller_->currentProgramNo() : 0;
    const int lineNo = controller_ ? (controller_->currentLine() + 1) * 10 : 0;
    drawText(painter, 18, 14, 600, 26, 19,
             QStringLiteral("FANUC Series Oi-TF"), kScreenHeading);
    drawText(painter, 680, 14, 326, 26, 19,
             QStringLiteral("O%1 N%2")
                 .arg(programNo, 4, 10, QLatin1Char('0'))
                 .arg(lineNo, 5, 10, QLatin1Char('0')),
             kScreenHeading, true);
    painter.fillRect(18, 48, 988, 2, QColor(180, 188, 194));

    QString pageTitle;
    if (screenPage_ == QStringLiteral("PROG")) {
        pageTitle = QStringLiteral("PROGRAM (MEM)");
    } else if (screenPage_ == QStringLiteral("POS")) {
        pageTitle = QStringLiteral("POSITION (MEM)");
    } else if (screenPage_ == QStringLiteral("DIR")) {
        pageTitle = QStringLiteral("PROGRAM DIRECTORY");
    } else if (screenPage_ == QStringLiteral("SYSTEM")) {
        pageTitle = QStringLiteral("SETTING (MDI)");
    } else {
        pageTitle = QStringLiteral("%1 (MEM)").arg(screenPage_);
    }

    drawText(painter, 18, 62, 620, 24, 16, pageTitle, kScreenStrong);
    drawText(painter, 650, 62, 360, 24, 16,
             QStringLiteral("绝对坐标"), kScreenStrong);

    drawPage(painter);
    drawPositionPanel(painter);
    drawCommandLine(painter);
    drawStatusLine(painter);
    drawSoftKeys(painter);
}

void CncScreen::mousePressEvent(QMouseEvent *event)
{
    if (!controller_) {
        return;
    }

    const double scale = qMin(double(width()) / 1024.0, double(height()) / 768.0);
    const double offsetX = (width() - 1024.0 * scale) / 2.0;
    const double offsetY = (height() - 768.0 * scale) / 2.0;
    const double baseX = (event->pos().x() - offsetX) / scale;
    const double baseY = (event->pos().y() - offsetY) / scale;

    if (screenPage_ == QStringLiteral("PROG")) {
        if (baseY < 728.0 || baseY > 758.0) {
            return;
        }
        const int index = softKeyIndexAt(baseX);
        if (index == 1) {
            setPage(QStringLiteral("DIR"));
        }
        return;
    }

    if (screenPage_ == QStringLiteral("DIR")) {
        const int programCount = controller_->programRecords().size();
        if (baseY >= 188.0 && baseY <= 610.0) {
            const int row = int((baseY - 190.0) / 42.0);
            if (row >= 0 && row < programCount) {
                selectedProgramIndex_ = row;
                update();
            }
            return;
        }
        if (baseY < 728.0 || baseY > 758.0) {
            return;
        }

        const int index = softKeyIndexAt(baseX);
        if (index == 0 && selectedProgramIndex_ >= 0
            && selectedProgramIndex_ < programCount) {
            controller_->loadProgram(
                controller_->programRecords().at(selectedProgramIndex_).number);
        } else if (index == 1) {
            controller_->saveCurrentProgram(controller_->currentProgramNo(),
                                            controller_->programName());
        } else if (index == 2 && selectedProgramIndex_ >= 0
                   && selectedProgramIndex_ < programCount) {
            controller_->deleteProgram(
                controller_->programRecords().at(selectedProgramIndex_).number);
        } else if (index == 3) {
            controller_->refreshPrograms();
        } else if (index == 4) {
            setPage(QStringLiteral("PROG"));
        }
        update();
        return;
    }

    if (screenPage_ == QStringLiteral("SYSTEM")) {
        if (baseY >= 158.0 && baseY <= 608.0) {
            const int row = int((baseY - 162.0) / 44.0);
            if (row >= 0 && row < controller_->parameters().size()) {
                controller_->selectParameter(row);
            }
            return;
        }
        if (baseY < 728.0 || baseY > 758.0) {
            return;
        }
        const int index = softKeyIndexAt(baseX);
        if (index == 0) {
            controller_->setParameterWriteEnabled(
                !controller_->parameterWriteEnabled());
        } else if (index == 1) {
            controller_->moveParameterCursor(-1);
        } else if (index == 2) {
            controller_->moveParameterCursor(1);
        } else if (index == 3) {
            controller_->resetParameters();
        } else if (index == 4) {
            setPage(QStringLiteral("POS"));
        }
        update();
        return;
    }

    if (screenPage_ != QStringLiteral("OFFSET")) {
        return;
    }
    if (baseY < 728.0 || baseY > 758.0) {
        return;
    }

    const int index = softKeyIndexAt(baseX);
    if (index == 0) {
        offsetMode_ = QStringLiteral("GEOMETRY");
    } else if (index == 1) {
        offsetMode_ = QStringLiteral("WEAR");
    }
    update();
}

void CncScreen::drawPage(QPainter &painter)
{
    if (!controller_) {
        return;
    }
    if (screenPage_ == QStringLiteral("PROG")) {
        drawProgram(painter);
    } else if (screenPage_ == QStringLiteral("POS")) {
        drawPositionPage(painter);
    } else if (screenPage_ == QStringLiteral("OFFSET")) {
        drawOffsetPage(painter);
    } else if (screenPage_ == QStringLiteral("SYSTEM")) {
        drawSystemPage(painter);
    } else if (screenPage_ == QStringLiteral("MESSAGE")) {
        drawMessagePage(painter);
    } else if (screenPage_ == QStringLiteral("GRAPH")) {
        drawGraphPage(painter);
    } else if (screenPage_ == QStringLiteral("DIR")) {
        drawDirectoryPage(painter);
    }
}

void CncScreen::drawProgram(QPainter &painter)
{
    const QStringList &program = controller_->program();
    const int lineHeight = 28;
    for (int i = 0; i < program.size(); ++i) {
        const int y = 96 + i * lineHeight;
        if (y > 600) {
            break;
        }

        bool parseError = false;
        for (const GCodeParseError &error : controller_->parseResult().errors) {
            if (error.line == i + 1) {
                parseError = true;
                break;
            }
        }

        const bool current = (i == controller_->currentLine());
        const bool selected =
            controller_->modeCode() == QStringLiteral("EDIT")
            && i == controller_->selectedProgramLine();
        if (current) {
            painter.fillRect(20, y - 3, 620, lineHeight, kHighlight);
        } else if (selected) {
            painter.fillRect(20, y - 3, 620, lineHeight, QColor(235, 241, 246));
        }
        if (selected && controller_->selectedProgramLength() > 0) {
            const QString &lineText = program.at(i);
            QFont selectionFont(QStringLiteral("Consolas"));
            selectionFont.setPixelSize(15);
            const QFontMetrics metrics(selectionFont);
            const int prefixWidth = metrics.horizontalAdvance(
                lineText.left(controller_->selectedProgramColumn()));
            const int selectionWidth = metrics.horizontalAdvance(
                lineText.mid(controller_->selectedProgramColumn(),
                             controller_->selectedProgramLength()));
            painter.fillRect(42 + prefixWidth, y - 1, selectionWidth, 22,
                             kSelection);
        } else if (selected) {
            const QString &lineText = program.at(i);
            QFont caretFont(QStringLiteral("Consolas"));
            caretFont.setPixelSize(15);
            const QFontMetrics metrics(caretFont);
            const int caretX = 42 + metrics.horizontalAdvance(
                lineText.left(controller_->selectedProgramColumn()));
            painter.setPen(QPen(QColor(40, 120, 210), 2));
            painter.drawLine(caretX, y - 1, caretX, y + 21);
        }
        if (current || selected) {
            drawText(painter, 18, y - 2, 14, 24, 15,
                     QStringLiteral(">"), kScreenStrong);
        }
        QColor lineColor = current ? kScreenStrong : kScreenText;
        if (parseError) {
            lineColor = kStateRed;
        }
        drawText(painter, 42, y - 2, 596, 24, 15,
                 program.at(i), lineColor, true);
    }
}

void CncScreen::drawPositionPage(QPainter &painter)
{
    const ToolOffset &tool = controller_->tool(controller_->currentToolIndex());
    drawText(painter, 24, 96, 600, 26, 18,
             QStringLiteral("位置显示"), kScreenHeading);

    drawCoordinateBlock(painter, 34, 136,
                        QStringLiteral("机械坐标 (MACHINE)"),
                        controller_->machineX(), controller_->machineZ());
    drawCoordinateBlock(painter, 326, 136,
                        QStringLiteral("绝对坐标 (ABSOLUTE)"),
                        controller_->absoluteX(), controller_->absoluteZ());
    drawCoordinateBlock(painter, 34, 274,
                        QStringLiteral("相对坐标 (G%1)")
                            .arg(53 + controller_->activeWorkOffset()),
                        controller_->relativeX(), controller_->relativeZ());
    drawToolOffsetBlock(painter, 326, 274,
                        QStringLiteral("刀偏 / 磨耗 (%1)").arg(controller_->toolName()),
                        tool);

    drawText(painter, 34, 404, 590, 24, 15,
             QStringLiteral("ABS = 基准 + (MACHINE - 对刀机械) + 磨耗"),
             kScreenMuted, true);
    drawText(painter, 34, 434, 590, 24, 15,
             QStringLiteral("X  %1 = %2 + (%3 - %4) + %5")
                 .arg(QString::number(controller_->absoluteX(), 'f', 3))
                 .arg(QString::number(tool.absoluteX, 'f', 3))
                 .arg(QString::number(controller_->machineX(), 'f', 3))
                 .arg(QString::number(tool.touchMachineX, 'f', 3))
                 .arg(QString::number(tool.wearX, 'f', 3)),
             kScreenText, true);
    drawText(painter, 34, 464, 590, 24, 15,
             QStringLiteral("Z  %1 = %2 + (%3 - %4) + %5")
                 .arg(QString::number(controller_->absoluteZ(), 'f', 3))
                 .arg(QString::number(tool.absoluteZ, 'f', 3))
                 .arg(QString::number(controller_->machineZ(), 'f', 3))
                 .arg(QString::number(tool.touchMachineZ, 'f', 3))
                 .arg(QString::number(tool.wearZ, 'f', 3)),
             kScreenText, true);

    drawCartesianAxes(painter);
}

void CncScreen::drawOffsetPage(QPainter &painter)
{
    const bool wearMode = offsetMode_ == QStringLiteral("WEAR");
    const ToolOffset &currentTool = controller_->tool(controller_->currentToolIndex());
    const double shapeX = currentTool.absoluteX - currentTool.touchMachineX;
    const double shapeZ = currentTool.absoluteZ - currentTool.touchMachineZ;
    drawText(painter, 24, 96, 600, 26, 18,
                 wearMode
                     ? QStringLiteral("刀具补偿 (12 工位刀塔) - 磨损")
                     : QStringLiteral("刀具补偿 (12 工位刀塔) - 形状"),
             kScreenHeading);
    drawText(painter, 24, 126, 600, 22, 12,
             QStringLiteral("%1  形状偏置 X %2  Z %3")
                 .arg(controller_->toolName(),
                      QString::number(shapeX, 'f', 3),
                      QString::number(shapeZ, 'f', 3)),
             kScreenMuted, true);
    drawText(painter, 24, 152, 600, 22, 14,
             wearMode
                 ? QStringLiteral("T         X 磨损          Z 磨损          R       刀尖")
                 : QStringLiteral("T      X 对刀/基准       Z 对刀/基准       R      刀尖"),
             kScreenHeading, true);

    const QVector<ToolOffset> &tools = controller_->tools();
    for (int i = 0; i < tools.size(); ++i) {
        const int y = 178 + i * 32;
        if (i == controller_->currentToolIndex()) {
            painter.fillRect(20, y - 3, 600, 32, kHighlight);
        }
        const ToolOffset &tool = tools.at(i);
        drawText(painter, 26, y, 54, 26, 14,
                 QStringLiteral("T%1").arg(i + 1, 2, 10, QLatin1Char('0')),
                 kScreenText, true);

        if (wearMode) {
            drawText(painter, 80, y, 166, 26, 14,
                     QString::number(tool.wearX, 'f', 3), kScreenText, true);
            drawText(painter, 246, y, 166, 26, 14,
                     QString::number(tool.wearZ, 'f', 3), kScreenText, true);
        } else {
            drawText(painter, 80, y, 166, 26, 13,
                     QStringLiteral("%1 / %2")
                         .arg(QString::number(tool.touchMachineX, 'f', 3),
                              QString::number(tool.absoluteX, 'f', 3)),
                     kScreenText, true);
            drawText(painter, 246, y, 166, 26, 13,
                     QStringLiteral("%1 / %2")
                         .arg(QString::number(tool.touchMachineZ, 'f', 3),
                              QString::number(tool.absoluteZ, 'f', 3)),
                     kScreenText, true);
        }
        drawText(painter, 414, y, 80, 26, 14,
                 QString::number(wearMode ? tool.wearRadius : tool.radius, 'f', 3),
                 kScreenText, true);
        drawText(painter, 506, y, 70, 26, 14,
                 QString::number(tool.tip), kScreenText, true);
    }
}

void CncScreen::drawSystemPage(QPainter &painter)
{
    drawText(painter, 24, 90, 600, 26, 18,
             QStringLiteral("参数设定"), kScreenHeading);
    drawText(painter, 620, 90, 360, 26, 14,
             QStringLiteral("模式: %1    写参数: %2")
                 .arg(controller_->parameterEditingAllowed()
                          ? QStringLiteral("可修改")
                          : QStringLiteral("禁止修改"),
                      controller_->parameterWriteEnabled()
                          ? QStringLiteral("1")
                          : QStringLiteral("0")),
             controller_->parameterEditingAllowed()
                 ? kStateGreen
                 : kStateRed,
             true);
    painter.fillRect(24, 122, 950, 2, QColor(180, 188, 194));

    drawText(painter, 34, 132, 560, 22, 13,
             QStringLiteral("参数号     名称                    当前值        说明"),
             kScreenHeading, true);

    const QVector<MachineParameter> &parameters = controller_->parameters();
    for (int i = 0; i < parameters.size(); ++i) {
        const int y = 166 + i * 44;
        if (i == controller_->selectedParameterIndex()) {
            painter.fillRect(20, y - 4, 620, 40, kHighlight);
        }
        const MachineParameter &parameter = parameters.at(i);
        const QColor valueColor =
            i == 0 && parameter.value == QStringLiteral("1")
                ? kStateRed
                : kScreenText;
        drawText(painter, 32, y, 16, 26, 16,
                 i == controller_->selectedParameterIndex()
                     ? QStringLiteral(">")
                     : QString(),
                 kScreenStrong, true);
        drawText(painter, 50, y, 70, 26, 13,
                 QStringLiteral("%1").arg(parameter.number, 4, 10,
                                          QLatin1Char('0')),
                 kScreenText, true);
        drawText(painter, 128, y, 180, 26, 13,
                 parameter.name, kScreenText, true);
        drawText(painter, 322, y, 80, 26, 14,
                 parameter.value, valueColor, true);
        drawText(painter, 418, y, 240, 26, 12,
                 parameter.description, kScreenMuted, true);
    }

}

void CncScreen::drawMessagePage(QPainter &painter)
{
    drawText(painter, 24, 96, 600, 26, 18,
             QStringLiteral("报警 / 信息"), kScreenHeading);

    QStringList rows;
    QVector<bool> errorRows;
    rows.append(controller_->databaseError().isEmpty()
                    ? QStringLiteral("DB   %1")
                          .arg(QFileInfo(controller_->databasePath()).fileName())
                    : QStringLiteral("DB   数据库错误: %1")
                          .arg(controller_->databaseError()));
    errorRows.append(false);

    if (!controller_->parseResult().errors.isEmpty()) {
        const QVector<GCodeParseError> &errors = controller_->parseResult().errors;
        for (int i = 0; i < errors.size() && i < 5; ++i) {
            const GCodeParseError &error = errors.at(i);
            rows.append(QStringLiteral("L%1 C%2  %3")
                            .arg(error.line)
                            .arg(error.column)
                            .arg(error.message));
            errorRows.append(true);
        }
    } else {
        rows.append(QStringLiteral("09:31:08  1000  程序语法检查通过"));
        rows.append(QStringLiteral("09:30:42  O1000 程序读取完成"));
        rows.append(QStringLiteral("09:30:18  2001  刀具补偿已载入"));
        rows.append(QStringLiteral("09:29:55  3000  X 轴参考点完成"));
        rows.append(QStringLiteral("09:29:31  3000  Z 轴参考点完成"));
        rows.append(QStringLiteral("09:28:06  1000  机床准备就绪"));
        for (int i = 0; i < 6; ++i) {
            errorRows.append(false);
        }
    }

    if (!controller_->commandLog().isEmpty()) {
        for (const QString &command : controller_->commandLog()) {
            rows.append(QStringLiteral("MDI  %1").arg(command));
            errorRows.append(false);
        }
    }

    for (int i = 0; i < rows.size(); ++i) {
        QColor rowColor = kScreenText;
        if (i == 0) {
            rowColor = kStateGreen;
        } else if (errorRows.value(i)) {
            rowColor = kStateRed;
        }
        drawText(painter, 34, 142 + i * 52, 570, 34, 15,
                 rows.at(i), rowColor, true);
    }
}

void CncScreen::drawGraphPage(QPainter &painter)
{
    drawText(painter, 24, 96, 600, 26, 18,
             QStringLiteral("刀具轨迹"), kScreenHeading);

    const QRectF graph(34, 138, 570, 480);
    painter.fillRect(graph, QColor(255, 255, 255));
    painter.setPen(QColor(150, 158, 164));
    painter.drawRect(graph);
    for (int i = 1; i < 6; ++i) {
        const double x = graph.left() + graph.width() * i / 6.0;
        const double y = graph.top() + graph.height() * i / 6.0;
        painter.drawLine(QPointF(x, graph.top()), QPointF(x, graph.bottom()));
        painter.drawLine(QPointF(graph.left(), y), QPointF(graph.right(), y));
    }

    auto mapPoint = [&](double xValue, double zValue) {
        const double px = graph.left() + (zValue + 70.0) / 75.0 * graph.width();
        const double py = graph.bottom() - (xValue - 20.0) / 50.0 * graph.height();
        return QPointF(px, py);
    };

    if (controller_->motionPath().isEmpty()) {
        QPainterPath fallback;
        fallback.moveTo(mapPoint(62.0, 2.0));
        fallback.lineTo(mapPoint(62.0, 0.0));
        fallback.lineTo(mapPoint(28.0, 0.0));
        fallback.lineTo(mapPoint(32.0, -2.0));
        fallback.lineTo(mapPoint(32.0, -35.0));
        fallback.lineTo(mapPoint(46.0, -50.0));
        fallback.lineTo(mapPoint(46.0, -65.0));
        painter.setPen(QPen(kScreenStrong, 3));
        painter.drawPath(fallback);
    } else {
        painter.setPen(QPen(kScreenStrong, 3));
        for (const MotionSegment &segment : controller_->motionPath()) {
            QPainterPath path;
            path.moveTo(mapPoint(segment.start.x(), segment.start.z()));
            const int steps = segment.length() > 20.0 ? 28 : 12;
            for (int i = 1; i <= steps; ++i) {
                const QVector3D point =
                    segment.positionAt(double(i) / double(steps));
                path.lineTo(mapPoint(point.x(), point.z()));
            }
            painter.drawPath(path);
        }
    }
    painter.setPen(QPen(kStateGreen, 5));
    painter.drawEllipse(mapPoint(controller_->absoluteX(), controller_->absoluteZ()), 6, 6);
}

void CncScreen::drawDirectoryPage(QPainter &painter)
{
    drawText(painter, 24, 96, 600, 26, 18,
             QStringLiteral("程序目录"), kScreenHeading);
    drawText(painter, 34, 128, 570, 22, 13,
             QStringLiteral("目录: %1").arg(controller_->programDirectory()),
             kScreenMuted, true);
    drawText(painter, 34, 158, 570, 22, 14,
             QStringLiteral("程序号      名称                 行数     更新时间"),
             kScreenHeading, true);

    const QVector<ProgramRecord> &records = controller_->programRecords();
    if (records.isEmpty()) {
        drawText(painter, 34, 198, 570, 28, 16,
                 QStringLiteral("数据库中没有程序"), kScreenMuted, true);
        return;
    }

    for (int i = 0; i < records.size(); ++i) {
        const int y = 190 + i * 42;
        if (y > 610) {
            break;
        }
        if (i == selectedProgramIndex_) {
            painter.fillRect(20, y - 3, 600, 36, kHighlight);
        }
        const ProgramRecord &record = records.at(i);
        drawText(painter, 34, y, 90, 30, 14,
                 QStringLiteral("O%1").arg(record.number, 4, 10, QLatin1Char('0')),
                 kScreenText, true);
        drawText(painter, 132, y, 180, 30, 14, record.name, kScreenText, true);
        drawText(painter, 324, y, 70, 30, 14,
                 QString::number(record.lineCount), kScreenText, true);
        drawText(painter, 404, y, 200, 30, 13, record.updatedAt, kScreenText, true);
    }
}

void CncScreen::drawPositionPanel(QPainter &painter)
{
    if (!controller_) {
        return;
    }
    const int right = 690;
    drawText(painter, right, 124, 300, 26, 16,
             QStringLiteral("X  %1").arg(QString::number(controller_->absoluteX(), 'f', 3)),
             kScreenStrong, true);
    drawText(painter, right, 160, 300, 26, 16,
             QStringLiteral("Z  %1").arg(QString::number(controller_->absoluteZ(), 'f', 3)),
             kScreenStrong, true);
    painter.fillRect(680, 206, 326, 2, QColor(180, 188, 194));

    const int rowY = 230;
    drawText(painter, right, rowY + 0, 190, 24, 15,
             QStringLiteral("加工件数"), kScreenHeading);
    drawText(painter, right + 160, rowY + 0, 140, 24, 15,
             QString::number(controller_->partCount()), kScreenText, true);
    drawText(painter, right, rowY + 32, 190, 24, 15,
             QStringLiteral("运行时间"), kScreenHeading);
    drawText(painter, right + 160, rowY + 32, 140, 24, 15,
             formatTime(controller_->runSeconds()), kScreenText, true);
    drawText(painter, right, rowY + 64, 190, 24, 15,
             QStringLiteral("循环时间"), kScreenHeading);
    drawText(painter, right + 160, rowY + 64, 140, 24, 15,
             formatTime(controller_->cycleSeconds()), kScreenText, true);

    drawText(painter, right, 390, 320, 24, 15,
             QStringLiteral("ACT F %1 MM/M")
                 .arg(QString::number(controller_->feed(), 'f', 2)),
             kScreenText);
    drawText(painter, right, 420, 320, 24, 15,
             QStringLiteral("S %1").arg(QString::number(controller_->spindleSpeed(), 'f', 0)),
             kScreenText);
    drawText(painter, right, 450, 320, 24, 15,
             controller_->toolName(), kScreenText);

    painter.fillRect(680, 490, 326, 2, QColor(180, 188, 194));
    drawText(painter, right, 508, 320, 24, 15,
             controller_->modalState().gCodeSummary(),
             kScreenMuted, true);
    drawText(painter, right, 538, 320, 24, 13,
             controller_->modalState().stateSummary(),
             kScreenMuted, true);
}

void CncScreen::drawStatusLine(QPainter &painter)
{
    if (!controller_) {
        return;
    }
    drawText(painter, 20, 668, 640, 26, 16,
             controller_->modeLine(), kScreenStrong, true);
    drawText(painter, 700, 668, 300, 26, 16,
             controller_->stateText(), controller_->stateColor());
}

void CncScreen::drawCommandLine(QPainter &painter)
{
    if (!controller_) {
        return;
    }
    const bool hasError = !controller_->commandError().isEmpty();
    const QString text = hasError
        ? controller_->commandError()
        : QStringLiteral("MDI> %1").arg(controller_->commandLine());
    drawText(painter, 24, 624, 620, 24, 14, text,
             hasError ? kStateRed : kScreenMuted, true);
}

QStringList CncScreen::softKeyLabels() const
{
    if (screenPage_ == QStringLiteral("PROG")) {
        return {
            QStringLiteral("程序"),
            QStringLiteral("目录"),
            QStringLiteral("下一步"),
            QStringLiteral("程序检查"),
            QStringLiteral("操作")
        };
    }
    if (screenPage_ == QStringLiteral("SYSTEM")) {
        return {
            QStringLiteral("写参数"),
            QStringLiteral("上一条"),
            QStringLiteral("下一条"),
            QStringLiteral("重置"),
            QStringLiteral("返回")
        };
    }
    if (screenPage_ == QStringLiteral("OFFSET")) {
        return {
            QStringLiteral("形状"),
            QStringLiteral("磨损"),
            QStringLiteral("设定"),
            QStringLiteral("操作"),
            QStringLiteral("返回")
        };
    }
    if (screenPage_ == QStringLiteral("DIR")) {
        return {
            QStringLiteral("载入"),
            QStringLiteral("保存"),
            QStringLiteral("删除"),
            QStringLiteral("刷新"),
            QStringLiteral("返回")
        };
    }
    return {
        QStringLiteral("ABS"),
        QStringLiteral("REL"),
        QStringLiteral("ALL"),
        QStringLiteral("HNDL"),
        QStringLiteral("OP")
    };
}

int CncScreen::softKeyIndexAt(double baseX) const
{
    const int count = softKeyLabels().size();
    if (count <= 0) {
        return -1;
    }
    const int gap = 4;
    const int totalWidth = 1008;
    const int cellWidth = (totalWidth - (count - 1) * gap) / count;
    const int x = int(baseX) - 8;
    if (x < 0) {
        return -1;
    }
    const int stride = cellWidth + gap;
    const int index = x / stride;
    if (index < 0 || index >= count
        || x - index * stride > cellWidth) {
        return -1;
    }
    return index;
}

void CncScreen::drawSoftKeys(QPainter &painter)
{
    const QStringList labels = softKeyLabels();
    const int count = labels.size();
    const int gap = 4;
    const int totalWidth = 1008;
    const int cellWidth = (totalWidth - (count - 1) * gap) / count;

    int x = 8;
    for (int i = 0; i < count; ++i) {
        const bool active = screenPage_ == QStringLiteral("OFFSET")
                            && ((i == 0 && offsetMode_ == QStringLiteral("GEOMETRY"))
                                || (i == 1 && offsetMode_ == QStringLiteral("WEAR")));
        painter.fillRect(x, 728, cellWidth, 30,
                         active ? kHighlight : QColor(255, 255, 255));
        painter.setPen(active ? kScreenStrong : QColor(180, 188, 194));
        painter.drawRect(x, 728, cellWidth - 1, 29);
        drawText(painter, x, 733, cellWidth, 22, 14,
                 QStringLiteral("[ %1 ]").arg(labels.at(i)),
                 active ? kScreenStrong : kScreenHeading, true);
        x += cellWidth + gap;
    }
}

void CncScreen::drawCoordinateBlock(QPainter &painter, int x, int y,
                                    const QString &title,
                                    double xValue, double zValue)
{
    drawText(painter, x, y, 280, 24, 16, title, kScreenHeading);
    drawText(painter, x + 18, y + 34, 260, 28, 22,
             QStringLiteral("X  %1").arg(QString::number(xValue, 'f', 3)),
             kScreenStrong, true);
    drawText(painter, x + 18, y + 70, 260, 28, 22,
             QStringLiteral("Z  %1").arg(QString::number(zValue, 'f', 3)),
             kScreenStrong, true);
}

void CncScreen::drawToolOffsetBlock(QPainter &painter, int x, int y,
                                    const QString &title, const ToolOffset &tool)
{
    drawText(painter, x, y, 280, 24, 16, title, kScreenHeading);
    drawText(painter, x + 12, y + 28, 270, 22, 14,
             QStringLiteral("X 对刀 %1   基准 %2")
                 .arg(QString::number(tool.touchMachineX, 'f', 3),
                      QString::number(tool.absoluteX, 'f', 3)),
             kScreenText, true);
    drawText(painter, x + 12, y + 54, 270, 22, 14,
             QStringLiteral("Z 对刀 %1   基准 %2")
                 .arg(QString::number(tool.touchMachineZ, 'f', 3),
                      QString::number(tool.absoluteZ, 'f', 3)),
             kScreenText, true);
    drawText(painter, x + 12, y + 80, 270, 22, 14,
             QStringLiteral("X 磨耗 %1   Z 磨耗 %2")
                 .arg(QString::number(tool.wearX, 'f', 3),
                      QString::number(tool.wearZ, 'f', 3)),
             kScreenText, true);
}

void CncScreen::drawCartesianAxes(QPainter &painter)
{
    const QPointF origin(500.0, 612.0);
    const QPointF xEnd(500.0, 538.0);
    const QPointF yEnd(432.0, 562.0);
    const QPointF zEnd(620.0, 612.0);

    painter.setPen(QPen(kScreenStrong, 2));
    painter.drawLine(origin, xEnd);
    painter.drawLine(origin, yEnd);
    painter.drawLine(origin, zEnd);
    painter.drawEllipse(origin, 4, 4);

    drawText(painter, 498, 508, 40, 24, 15, QStringLiteral("X"), kScreenHeading);
    drawText(painter, 392, 536, 40, 24, 15, QStringLiteral("Y"), kScreenHeading);
    drawText(painter, 628, 588, 40, 24, 15, QStringLiteral("Z"), kScreenHeading);
}

void CncScreen::drawText(QPainter &painter, int x, int y, int w, int h,
                         int pixelSize, const QString &text,
                         const QColor &color, bool mono) const
{
    QFont font(QStringLiteral("Microsoft YaHei"));
    font.setPixelSize(pixelSize);
    if (mono) {
        font.setStyleHint(QFont::Monospace);
        font.setFamily(QStringLiteral("Consolas"));
    }
    painter.setFont(font);
    painter.setPen(color);
    painter.drawText(QRect(x, y, w, h), Qt::AlignLeft | Qt::AlignVCenter, text);
}

QString CncScreen::formatTime(int totalSeconds) const
{
    const int hours = totalSeconds / 3600;
    const int minutes = (totalSeconds % 3600) / 60;
    const int seconds = totalSeconds % 60;
    return QStringLiteral("%1H %2M %3S")
        .arg(hours)
        .arg(minutes, 2, 10, QLatin1Char('0'))
        .arg(seconds, 2, 10, QLatin1Char('0'));
}
