#include "ui/RotaryDial.h"

#include <QFont>
#include <QMouseEvent>
#include <QPainter>
#include <QPointF>
#include <QtMath>

RotaryDial::RotaryDial(QWidget *parent)
    : QWidget(parent)
{
    setMinimumSize(90, 90);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

void RotaryDial::setRange(int minimum, int maximum)
{
    minimum_ = minimum;
    maximum_ = qMax(minimum + 1, maximum);
    value_ = qBound(minimum_, value_, maximum_);
    update();
}

void RotaryDial::setValue(int value)
{
    const int next = qBound(minimum_, value, maximum_);
    if (next == value_) {
        return;
    }
    value_ = next;
    if (valueChangedHandler_) {
        valueChangedHandler_(value_);
    }
    update();
}

int RotaryDial::value() const
{
    return value_;
}

void RotaryDial::setLabel(const QString &label)
{
    label_ = label;
    update();
}

void RotaryDial::setValueChangedHandler(std::function<void(int)> handler)
{
    valueChangedHandler_ = std::move(handler);
}

QSize RotaryDial::minimumSizeHint() const
{
    return QSize(90, 90);
}

QSize RotaryDial::sizeHint() const
{
    return QSize(120, 120);
}

void RotaryDial::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const double side = qMin(width(), height());
    const QRectF dialRect((width() - side) / 2.0,
                          (height() - side) / 2.0,
                          side,
                          side);
    const QPointF center = dialRect.center();
    const double radius = side * 0.46;

    painter.setPen(QPen(QColor(35, 40, 44), 3));
    painter.setBrush(QColor(24, 27, 30));
    painter.drawEllipse(center, radius, radius);

    painter.setPen(QPen(QColor(70, 76, 82), 2));
    painter.setBrush(QColor(36, 40, 44));
    painter.drawEllipse(center, radius * 0.78, radius * 0.78);

    const double startAngle = 225.0;
    const double sweepAngle = -270.0;
    for (int i = 0; i <= 18; ++i) {
        const double fraction = i / 18.0;
        const double angle = qDegreesToRadians(startAngle + sweepAngle * fraction);
        const QPointF outer(center.x() + qCos(angle) * radius * 0.93,
                            center.y() - qSin(angle) * radius * 0.93);
        const QPointF inner(center.x() + qCos(angle) * radius * 0.80,
                            center.y() - qSin(angle) * radius * 0.80);
        painter.setPen(QPen(i % 3 == 0 ? QColor(235, 238, 240)
                                      : QColor(120, 128, 134),
                            i % 3 == 0 ? 2.0 : 1.0));
        painter.drawLine(inner, outer);
    }

    const double pointerAngle = qDegreesToRadians(angleForValue());
    const QPointF pointerStart(center.x() + qCos(pointerAngle) * radius * 0.22,
                               center.y() - qSin(pointerAngle) * radius * 0.22);
    const QPointF pointerEnd(center.x() + qCos(pointerAngle) * radius * 0.78,
                             center.y() - qSin(pointerAngle) * radius * 0.78);
    painter.setPen(QPen(QColor(242, 245, 247), 3, Qt::SolidLine, Qt::RoundCap));
    painter.drawLine(pointerStart, pointerEnd);

    QFont labelFont(QStringLiteral("Microsoft YaHei"));
    labelFont.setPixelSize(qMax(10, int(side * 0.11)));
    labelFont.setBold(true);
    painter.setFont(labelFont);
    painter.setPen(QColor(235, 238, 240));
    painter.drawText(QRectF(center.x() - radius * 0.6,
                            center.y() - radius * 0.2,
                            radius * 1.2,
                            radius * 0.4),
                     Qt::AlignCenter, label_);

}

void RotaryDial::mousePressEvent(QMouseEvent *event)
{
    updateValueFromPosition(event->pos());
}

void RotaryDial::mouseMoveEvent(QMouseEvent *event)
{
    if (event->buttons() & Qt::LeftButton) {
        updateValueFromPosition(event->pos());
    }
}

void RotaryDial::updateValueFromPosition(const QPoint &position)
{
    const QPointF center(width() / 2.0, height() / 2.0);
    const QPointF delta(position.x() - center.x(), center.y() - position.y());
    double angle = qRadiansToDegrees(qAtan2(delta.y(), delta.x()));
    if (angle < 0.0) {
        angle += 360.0;
    }

    double fromStart = 225.0 - angle;
    if (fromStart < 0.0) {
        fromStart += 360.0;
    }
    const double fraction = qBound(0.0, fromStart / 270.0, 1.0);
    setValue(minimum_ + qRound((maximum_ - minimum_) * fraction));
}

double RotaryDial::angleForValue() const
{
    const double fraction = double(value_ - minimum_)
                            / double(maximum_ - minimum_);
    return 225.0 - 270.0 * fraction;
}
