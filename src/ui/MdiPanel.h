#ifndef UI_MDIPANEL_H
#define UI_MDIPANEL_H

#include <functional>
#include <QWidget>

class QLineEdit;
class QPushButton;

class MdiPanel : public QWidget
{
public:
    explicit MdiPanel(QWidget *parent = nullptr);

    void setPageHandler(std::function<void(const QString &)> handler);
    void setCommandHandler(std::function<void(const QString &)> handler);
    void setEditHandler(std::function<void(const QString &)> handler);
    QString inputText() const;
    void clearInput();

private:
    void handlePage(const QString &page);
    void handleKey(const QString &text);
    void submitCommand();
    QPushButton *addKey(const QString &text);

    QLineEdit *inputEdit_ = nullptr;
    QPushButton *shiftButton_ = nullptr;
    bool shiftActive_ = false;
    std::function<void(const QString &)> pageHandler_;
    std::function<void(const QString &)> commandHandler_;
    std::function<void(const QString &)> editHandler_;
};

#endif
