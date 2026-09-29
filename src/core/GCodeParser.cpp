#include "core/GCodeParser.h"

#include <QtMath>

#include <limits>

bool GCodeBlock::hasWord(QChar letter) const
{
    const QChar expected = letter.toUpper();
    for (const GCodeWord &word : words) {
        if (word.letter == expected) {
            return true;
        }
    }
    return false;
}

double GCodeBlock::wordValue(QChar letter, bool *ok) const
{
    const QChar expected = letter.toUpper();
    for (const GCodeWord &word : words) {
        if (word.letter == expected) {
            if (ok) {
                *ok = true;
            }
            return word.value;
        }
    }
    if (ok) {
        *ok = false;
    }
    return std::numeric_limits<double>::quiet_NaN();
}

QString GCodeBlock::wordRawValue(QChar letter) const
{
    const QChar expected = letter.toUpper();
    for (const GCodeWord &word : words) {
        if (word.letter == expected) {
            return word.rawValue;
        }
    }
    return QString();
}

bool GCodeParseResult::isValid() const
{
    return errors.isEmpty();
}

GCodeParseResult GCodeParser::parseProgram(const QStringList &lines) const
{
    GCodeParseResult result;
    for (int i = 0; i < lines.size(); ++i) {
        QVector<GCodeParseError> blockErrors;
        const GCodeBlock block = parseBlock(lines.at(i), i + 1, &blockErrors);
        if (!block.rawText.trimmed().isEmpty() || !block.words.isEmpty()
            || !block.comments.isEmpty()) {
            result.blocks.append(block);
        }
        result.errors += blockErrors;
    }
    return result;
}

GCodeBlock GCodeParser::parseBlock(const QString &line, int sourceLine,
                                   QVector<GCodeParseError> *errors) const
{
    GCodeBlock block;
    block.rawText = line;
    block.lineNumber = sourceLine;

    QString macroCheck = line.trimmed();
    const QString upperCheck = macroCheck.toUpper();
    if (upperCheck.startsWith(QLatin1Char('N'))) {
        int index = 1;
        while (index < upperCheck.size() && upperCheck.at(index).isDigit()) {
            ++index;
        }
        bool ok = false;
        const int sequence = upperCheck.mid(1, index - 1).toInt(&ok);
        if (ok) {
            block.sequenceNumber = sequence;
        }
        macroCheck = macroCheck.mid(index).trimmed();
    }
    const QString upperMacro = macroCheck.toUpper();
    if (upperMacro.startsWith(QLatin1Char('#'))
        || upperMacro.startsWith(QStringLiteral("IF"))
        || upperMacro.startsWith(QStringLiteral("WHILE"))
        || upperMacro.startsWith(QStringLiteral("GOTO"))
        || upperMacro.startsWith(QStringLiteral("END"))) {
        return block;
    }

    auto addError = [errors, sourceLine](int column, const QString &message) {
        if (errors) {
            errors->append({ sourceLine, column, message });
        }
    };

    int index = 0;
    bool firstToken = true;
    while (index < line.size()) {
        const QChar current = line.at(index);
        if (current.isSpace() || current == QLatin1Char(',')) {
            ++index;
            continue;
        }
        if (current == QLatin1Char(';')) {
            const QString comment = line.mid(index + 1).trimmed();
            if (!comment.isEmpty()) {
                block.comments.append(comment);
            }
            break;
        }
        if (current == QLatin1Char('(')) {
            const int end = line.indexOf(QLatin1Char(')'), index + 1);
            if (end < 0) {
                addError(index + 1, QStringLiteral("括号注释没有结束"));
                break;
            }
            block.comments.append(line.mid(index + 1, end - index - 1).trimmed());
            index = end + 1;
            firstToken = false;
            continue;
        }
        if (current == QLatin1Char('/') && firstToken) {
            block.optionalBlock = true;
            ++index;
            continue;
        }
        if (current == QLatin1Char('%')) {
            ++index;
            continue;
        }
        if (!current.isLetter()) {
            addError(index + 1,
                     QStringLiteral("无法识别的字符 %1").arg(current));
            ++index;
            continue;
        }

        GCodeWord word;
        word.letter = current.toUpper();
        word.column = index + 1;
        ++index;

        if (index < line.size()
            && (line.at(index) == QLatin1Char('#')
                || line.at(index) == QLatin1Char('['))) {
            const int start = index;
            if (line.at(index) == QLatin1Char('#')) {
                ++index;
                while (index < line.size() && line.at(index).isDigit()) {
                    ++index;
                }
            } else {
                int depth = 0;
                while (index < line.size()) {
                    if (line.at(index) == QLatin1Char('[')) {
                        ++depth;
                    } else if (line.at(index) == QLatin1Char(']')) {
                        --depth;
                        if (depth == 0) {
                            ++index;
                            break;
                        }
                    }
                    ++index;
                }
                if (depth != 0) {
                    addError(word.column,
                             QStringLiteral("宏表达式缺少右括号"));
                    continue;
                }
            }
            word.rawValue = line.mid(start, index - start);
            word.value = 0.0;
            block.words.append(word);
            firstToken = false;
            continue;
        }

        QString number;
        if (index < line.size()
            && (line.at(index) == QLatin1Char('+')
                || line.at(index) == QLatin1Char('-'))) {
            number.append(line.at(index));
            ++index;
        }
        while (index < line.size()
               && (line.at(index).isDigit()
                   || line.at(index) == QLatin1Char('.'))) {
            number.append(line.at(index));
            ++index;
        }

        if (number.isEmpty()
            || number == QStringLiteral("+")
            || number == QStringLiteral("-")
            || number == QStringLiteral(".")) {
            addError(word.column,
                     QStringLiteral("代码 %1 缺少有效数值").arg(word.letter));
            continue;
        }

        bool ok = false;
        const double value = number.toDouble(&ok);
        if (!ok) {
            addError(word.column,
                     QStringLiteral("代码 %1 的数值格式错误: %2")
                         .arg(word.letter)
                         .arg(number));
            continue;
        }

        word.rawValue = number;
        word.value = value;
        block.words.append(word);

        const int rounded = qRound(value);
        if (word.letter == QLatin1Char('O')) {
            block.programNumber = rounded;
        } else if (word.letter == QLatin1Char('N')) {
            block.sequenceNumber = rounded;
        }

        firstToken = false;
    }
    return block;
}
