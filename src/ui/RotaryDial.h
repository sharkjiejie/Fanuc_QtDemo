#ifndef UI_ROTARYDIAL_H
#define UI_ROTARYDIAL_H

#include <QWidget>

#include <functional>

class QMouseEvent;

class RotaryDial : public QWidget
{
public:
    explicit RotaryDial(QWidget *parent = nullptr);

    void setRange(int minimum, int maximum);
    void setValue(int value);
    int value() const;
    void setLabel(const QString &label);
    void setValueChangedHandler(std::function<void(int)> handler);

    QSize minimumSizeHint() const override;
    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;

private:
    void updateValueFromPosition(const QPoint &position);
    double angleForValue() const;

    int minimum_ = 0;
    int maximum_ = 100;
    int value_ = 100;
    QString label_ = QStringLiteral("MPG");
    std::function<void(int)> valueChangedHandler_;
};

#endif
