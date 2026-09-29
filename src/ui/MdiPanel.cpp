#include "ui/MdiPanel.h"

#include "ui/PanelStyle.h"

#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QList>
#include <QLineEdit>
#include <QPushButton>
#include <QSizePolicy>
#include <QStringList>
#include <QVBoxLayout>

MdiPanel::MdiPanel(QWidget *parent)
    : QWidget(parent)
{
    setMinimumWidth(400);
    setMaximumWidth(450);
    setMinimumHeight(460);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);

    QVBoxLayout *root = new QVBoxLayout(this);
    root->setContentsMargins(6, 6, 6, 0);
    root->setSpacing(12);

    QGroupBox *inputGroup = new QGroupBox(QStringLiteral("MDI 输入"));
    QVBoxLayout *inputLayout = new QVBoxLayout(inputGroup);
    inputLayout->setContentsMargins(10, 10, 10, 10);
    inputEdit_ = new QLineEdit(inputGroup);
    inputEdit_->setClearButtonEnabled(true);
    inputEdit_->setMaximumHeight(32);
    inputLayout->addWidget(inputEdit_);
    connect(inputEdit_, &QLineEdit::returnPressed, this, [this]() {
        submitCommand();
    });
    root->addWidget(inputGroup);

    QGroupBox *keyGroup = new QGroupBox(QStringLiteral("MDI 键盘"));
    QGridLayout *keys = new QGridLayout(keyGroup);
    keys->setContentsMargins(8, 10, 8, 8);
    keys->setHorizontalSpacing(5);
    keys->setVerticalSpacing(5);

    const QStringList keyRows[] = {
        { QStringLiteral("O\nP"), QStringLiteral("N\nQ"),
          QStringLiteral("G\nR"), QStringLiteral("7\nA"),
          QStringLiteral("8\nB"), QStringLiteral("9\nD") },
        { QStringLiteral("X\nC"), QStringLiteral("Z\nY"),
          QStringLiteral("F\nL"), QStringLiteral("4\n["),
          QStringLiteral("5\n]"), QStringLiteral("6\nSP") },
        { QStringLiteral("M\nI"), QStringLiteral("S\nK"),
          QStringLiteral("T\nJ"), QStringLiteral("1"),
          QStringLiteral("2\n#"), QStringLiteral("3\n=") },
        { QStringLiteral("U\nH"), QStringLiteral("W\nV"),
          QStringLiteral("EOB\nE"), QStringLiteral("-\n+"),
          QStringLiteral("0\n*"), QStringLiteral(".\n/") }
    };

    for (int row = 0; row < 4; ++row) {
        for (int column = 0; column < keyRows[row].size(); ++column) {
            QPushButton *button = addKey(keyRows[row].at(column));
            keys->addWidget(button, row, column);
        }
    }

    const QStringList functionRow1 = {
        QStringLiteral("POS"), QStringLiteral("PROG"),
        QStringLiteral("OFS/SET"), QStringLiteral("SHIFT"),
        QStringLiteral("CAN"), QStringLiteral("INPUT")
    };
    for (int column = 0; column < functionRow1.size(); ++column) {
        QPushButton *button = new QPushButton(functionRow1.at(column), keyGroup);
        button->setMinimumHeight(34);
        button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        QString style = panelButtonStyle(QStringLiteral("#28323a"));
        style += QStringLiteral("QPushButton { font-size:10px; padding:2px; }");
        button->setStyleSheet(style);
        keys->addWidget(button, 4, column);

        const QString action = functionRow1.at(column);
        if (action == QStringLiteral("SHIFT")) {
            shiftButton_ = button;
            button->setCheckable(true);
            connect(button, &QPushButton::toggled, this, [this](bool checked) {
                shiftActive_ = checked;
            });
        } else {
            connect(button, &QPushButton::clicked, this, [this, action]() {
                if (action == QStringLiteral("POS")) {
                    handlePage(QStringLiteral("POS"));
                } else if (action == QStringLiteral("PROG")) {
                    handlePage(QStringLiteral("PROG"));
                } else if (action == QStringLiteral("OFS/SET")) {
                    handlePage(QStringLiteral("OFFSET"));
                } else {
                    handleKey(action);
                }
            });
        }
    }

    const QStringList functionRow2 = {
        QStringLiteral("SYSTEM"), QStringLiteral("MESSAGE"),
        QStringLiteral("CSTM/GR"), QStringLiteral("ALTER"),
        QStringLiteral("INSERT"), QStringLiteral("DELETE")
    };
    for (int column = 0; column < functionRow2.size(); ++column) {
        QPushButton *button = new QPushButton(functionRow2.at(column), keyGroup);
        button->setMinimumHeight(34);
        button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        QString style = panelButtonStyle(QStringLiteral("#28323a"));
        style += QStringLiteral("QPushButton { font-size:10px; padding:2px; }");
        button->setStyleSheet(style);
        keys->addWidget(button, 5, column);

        const QString action = functionRow2.at(column);
        connect(button, &QPushButton::clicked, this, [this, action]() {
            if (action == QStringLiteral("SYSTEM")) {
                handlePage(QStringLiteral("SYSTEM"));
            } else if (action == QStringLiteral("MESSAGE")) {
                handlePage(QStringLiteral("MESSAGE"));
            } else if (action == QStringLiteral("CSTM/GR")) {
                handlePage(QStringLiteral("GRAPH"));
            } else if (action == QStringLiteral("DELETE")) {
                handleKey(QStringLiteral("DEL"));
            } else if (action == QStringLiteral("ALTER")) {
                handleKey(QStringLiteral("ALTER"));
            } else {
                handleKey(action);
            }
        });
    }
    root->addWidget(keyGroup, 1);

    QGroupBox *navigationGroup = new QGroupBox(QStringLiteral("翻页 / 光标"));
    QGridLayout *navigation = new QGridLayout(navigationGroup);
    navigation->setContentsMargins(10, 10, 10, 10);
    navigation->setHorizontalSpacing(6);
    navigation->setVerticalSpacing(6);

    QPushButton *pageUp = new QPushButton(QStringLiteral("PAGE ↑"), navigationGroup);
    QPushButton *pageDown = new QPushButton(QStringLiteral("PAGE ↓"), navigationGroup);
    QPushButton *arrowUp = new QPushButton(QStringLiteral("↑"), navigationGroup);
    QPushButton *arrowDown = new QPushButton(QStringLiteral("↓"), navigationGroup);
    QPushButton *arrowLeft = new QPushButton(QStringLiteral("←"), navigationGroup);
    QPushButton *arrowRight = new QPushButton(QStringLiteral("→"), navigationGroup);
    QPushButton *help = new QPushButton(QStringLiteral("HELP"), navigationGroup);
    QPushButton *reset = new QPushButton(QStringLiteral("RESET"), navigationGroup);
    const QList<QPushButton *> navigationButtons = {
        pageUp, pageDown, arrowUp, arrowDown,
        arrowLeft, arrowRight, help, reset
    };
    for (QPushButton *button : navigationButtons) {
        button->setMinimumHeight(32);
        button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        QString style = panelButtonStyle(QStringLiteral("#28323a"));
        style += QStringLiteral("QPushButton { font-size:10px; padding:2px; }");
        button->setStyleSheet(style);
    }

    connect(arrowUp, &QPushButton::clicked, this, [this]() {
        handleKey(QStringLiteral("↑"));
    });
    connect(arrowDown, &QPushButton::clicked, this, [this]() {
        handleKey(QStringLiteral("↓"));
    });
    connect(arrowLeft, &QPushButton::clicked, this, [this]() {
        handleKey(QStringLiteral("←"));
    });
    connect(arrowRight, &QPushButton::clicked, this, [this]() {
        handleKey(QStringLiteral("→"));
    });

    navigation->addWidget(pageUp, 0, 0);
    navigation->addWidget(pageDown, 1, 0);
    navigation->addWidget(arrowUp, 0, 2);
    navigation->addWidget(arrowLeft, 1, 1);
    navigation->addWidget(arrowDown, 1, 2);
    navigation->addWidget(arrowRight, 1, 3);
    navigation->addWidget(help, 0, 5);
    navigation->addWidget(reset, 1, 5);
    root->addWidget(navigationGroup);

    connect(reset, &QPushButton::clicked, this, [this]() {
        if (commandHandler_) {
            commandHandler_(QStringLiteral("RESET"));
        }
    });
}

void MdiPanel::setPageHandler(std::function<void(const QString &)> handler)
{
    pageHandler_ = std::move(handler);
}

void MdiPanel::setCommandHandler(std::function<void(const QString &)> handler)
{
    commandHandler_ = std::move(handler);
}

void MdiPanel::setEditHandler(std::function<void(const QString &)> handler)
{
    editHandler_ = std::move(handler);
}

QString MdiPanel::inputText() const
{
    return inputEdit_ ? inputEdit_->text() : QString();
}

void MdiPanel::clearInput()
{
    if (inputEdit_) {
        inputEdit_->clear();
    }
}

void MdiPanel::handlePage(const QString &page)
{
    if (pageHandler_) {
        pageHandler_(page);
    }
}

QPushButton *MdiPanel::addKey(const QString &text)
{
    QPushButton *button = new QPushButton(text);
    button->setMinimumSize(42, 38);
    button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    button->setStyleSheet(panelButtonStyle(
        QStringLiteral("#171c20"), QStringLiteral("#f5f7f8")));

    connect(button, &QPushButton::clicked, this, [this, text]() {
        handleKey(text);
    });
    return button;
}

void MdiPanel::handleKey(const QString &text)
{
    const QStringList parts = text.split(QLatin1Char('\n'));
    const QString primary = parts.value(0);
    const QString alternate = parts.size() > 1 ? parts.value(1) : QString();

    if (text == QStringLiteral("SHIFT")) {
        shiftActive_ = !shiftActive_;
        if (shiftButton_) {
            shiftButton_->setChecked(shiftActive_);
        }
        return;
    }
    if (text == QStringLiteral("INPUT")) {
        shiftActive_ = false;
        if (shiftButton_) {
            shiftButton_->setChecked(false);
        }
        submitCommand();
        return;
    }
    if (text == QStringLiteral("DEL")
        || text == QStringLiteral("CAN")) {
        QString value = inputEdit_->text();
        value.chop(1);
        inputEdit_->setText(value);
        shiftActive_ = false;
        if (shiftButton_) {
            shiftButton_->setChecked(false);
        }
        return;
    }
    if (text == QStringLiteral("ALTER")
        && editHandler_) {
        editHandler_(QStringLiteral("ALTER"));
        return;
    }
    if (text == QStringLiteral("INSERT")
        || text == QStringLiteral("SHIFT")) {
        if (text == QStringLiteral("INSERT") && editHandler_) {
            editHandler_(QStringLiteral("INSERT"));
        }
        return;
    }
    if (text == QStringLiteral("DELETE")) {
        if (editHandler_) {
            editHandler_(QStringLiteral("DELETE"));
        }
        return;
    }
    if (text == QStringLiteral("RESET")) {
        if (commandHandler_) {
            commandHandler_(QStringLiteral("RESET"));
        }
        return;
    }
    if (text == QStringLiteral("↑")
        || text == QStringLiteral("↓")
        || text == QStringLiteral("←")
        || text == QStringLiteral("→")) {
        if (editHandler_) {
            const QString action =
                text == QStringLiteral("↑") ? QStringLiteral("UP")
                : text == QStringLiteral("↓") ? QStringLiteral("DOWN")
                : text == QStringLiteral("←") ? QStringLiteral("LEFT")
                                              : QStringLiteral("RIGHT");
            editHandler_(action);
        }
        return;
    }
    if (text.startsWith(QStringLiteral("PAGE"))) {
        return;
    }

    QString output = (shiftActive_ && !alternate.isEmpty()) ? alternate : primary;
    if (output == QStringLiteral("EOB")) {
        output = QStringLiteral(";");
    } else if (output == QStringLiteral("SP")) {
        output = QStringLiteral(" ");
    }
    shiftActive_ = false;
    if (shiftButton_) {
        shiftButton_->setChecked(false);
    }

    QString value = inputEdit_->text();
    value += output;
    inputEdit_->setText(value);
}

void MdiPanel::submitCommand()
{
    const QString command = inputEdit_->text().trimmed();
    if (command.isEmpty()) {
        return;
    }
    if (commandHandler_) {
        commandHandler_(command);
    }
    inputEdit_->clear();
}
