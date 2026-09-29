#include "core/MacroEngine.h"

#include "core/GCodeParser.h"

#include <QtMath>

#include <cmath>

namespace {

QString formatMacroValue(double value)
{
    QString text = QString::number(value, 'f', 4);
    while (text.contains(QLatin1Char('.')) && text.endsWith(QLatin1Char('0'))) {
        text.chop(1);
    }
    if (text.endsWith(QLatin1Char('.'))) {
        text.chop(1);
    }
    if (text == QStringLiteral("-0")) {
        text = QStringLiteral("0");
    }
    return text;
}

QString stripInlineComment(const QString &line)
{
    QString result;
    int depth = 0;
    for (int i = 0; i < line.size(); ++i) {
        const QChar ch = line.at(i);
        if (ch == QLatin1Char('(')) {
            ++depth;
            continue;
        }
        if (ch == QLatin1Char(')') && depth > 0) {
            --depth;
            continue;
        }
        if (depth == 0 && ch == QLatin1Char(';')) {
            break;
        }
        if (depth == 0) {
            result += ch;
        }
    }
    return result.trimmed();
}

QString stripSequenceNumber(const QString &line)
{
    QString result = line.trimmed();
    if (!result.startsWith(QLatin1Char('N'), Qt::CaseInsensitive)) {
        return result;
    }
    int index = 1;
    while (index < result.size() && result.at(index).isDigit()) {
        ++index;
    }
    return result.mid(index).trimmed();
}

bool extractBracketExpression(const QString &line, QString *expression)
{
    const int start = line.indexOf(QLatin1Char('['));
    if (start < 0) {
        return false;
    }
    int depth = 0;
    for (int i = start; i < line.size(); ++i) {
        if (line.at(i) == QLatin1Char('[')) {
            ++depth;
        } else if (line.at(i) == QLatin1Char(']')) {
            --depth;
            if (depth == 0) {
                if (expression) {
                    *expression = line.mid(start + 1, i - start - 1);
                }
                return true;
            }
        }
    }
    return false;
}

bool trailingInteger(const QString &line, int *value)
{
    int index = line.size() - 1;
    while (index >= 0 && line.at(index).isSpace()) {
        --index;
    }
    int end = index;
    while (index >= 0 && line.at(index).isDigit()) {
        --index;
    }
    if (end < 0 || index == end) {
        return false;
    }
    bool ok = false;
    const int result = line.mid(index + 1, end - index).toInt(&ok);
    if (ok && value) {
        *value = result;
    }
    return ok;
}

class ExpressionEvaluator
{
public:
    ExpressionEvaluator(const QString &expression,
                        const QHash<int, double> &locals,
                        const QHash<int, double> &commons)
        : expression_(expression)
        , locals_(locals)
        , commons_(commons)
    {
    }

    double evaluate(bool *ok, QString *error)
    {
        position_ = 0;
        ok_ = true;
        error_.clear();
        const double result = parseExpression();
        skipSpaces();
        if (ok_ && position_ < expression_.size()) {
            fail(QStringLiteral("无法识别的表达式: %1")
                     .arg(expression_.mid(position_)));
        }
        if (ok && error) {
            *error = error_;
        }
        if (ok) {
            *ok = ok_;
        }
        return result;
    }

private:
    void skipSpaces()
    {
        while (position_ < expression_.size()
               && expression_.at(position_).isSpace()) {
            ++position_;
        }
    }

    bool accept(QChar ch)
    {
        skipSpaces();
        if (position_ < expression_.size() && expression_.at(position_) == ch) {
            ++position_;
            return true;
        }
        return false;
    }

    bool acceptWord(const QString &word)
    {
        skipSpaces();
        if (expression_.mid(position_, word.size())
                .compare(word, Qt::CaseInsensitive) == 0) {
            position_ += word.size();
            return true;
        }
        return false;
    }

    void fail(const QString &message)
    {
        if (ok_) {
            ok_ = false;
            error_ = message;
        }
    }

    double parseExpression()
    {
        double value = parseTerm();
        while (ok_) {
            skipSpaces();
            if (accept(QLatin1Char('+'))) {
                value += parseTerm();
            } else if (accept(QLatin1Char('-'))) {
                value -= parseTerm();
            } else {
                break;
            }
        }
        return value;
    }

    double parseTerm()
    {
        double value = parseFactor();
        while (ok_) {
            skipSpaces();
            if (accept(QLatin1Char('*'))) {
                value *= parseFactor();
            } else if (accept(QLatin1Char('/'))) {
                const double divisor = parseFactor();
                if (qAbs(divisor) < 0.000000001) {
                    fail(QStringLiteral("宏程序除数为零"));
                    return 0.0;
                }
                value /= divisor;
            } else if (acceptWord(QStringLiteral("MOD"))) {
                const double divisor = parseFactor();
                if (qAbs(divisor) < 0.000000001) {
                    fail(QStringLiteral("宏程序 MOD 除数为零"));
                    return 0.0;
                }
                value = std::fmod(value, divisor);
            } else {
                break;
            }
        }
        return value;
    }

    double parseFactor()
    {
        skipSpaces();
        if (position_ >= expression_.size()) {
            fail(QStringLiteral("宏表达式不完整"));
            return 0.0;
        }

        if (accept(QLatin1Char('-'))) {
            return -parseFactor();
        }
        if (accept(QLatin1Char('+'))) {
            return parseFactor();
        }
        if (accept(QLatin1Char('('))) {
            const double value = parseExpression();
            if (!accept(QLatin1Char(')'))) {
                fail(QStringLiteral("宏表达式缺少右括号"));
            }
            return value;
        }
        if (expression_.at(position_) == QLatin1Char('#')) {
            ++position_;
            int start = position_;
            while (position_ < expression_.size()
                   && expression_.at(position_).isDigit()) {
                ++position_;
            }
            bool ok = false;
            const int number = expression_.mid(start, position_ - start).toInt(&ok);
            if (!ok) {
                fail(QStringLiteral("宏变量编号无效"));
                return 0.0;
            }
            if (locals_.contains(number)) {
                return locals_.value(number);
            }
            if (commons_.contains(number)) {
                return commons_.value(number);
            }
            fail(QStringLiteral("宏变量 #%1 未赋值").arg(number));
            return 0.0;
        }
        if (expression_.at(position_).isLetter()) {
            int start = position_;
            while (position_ < expression_.size()
                   && expression_.at(position_).isLetter()) {
                ++position_;
            }
            const QString function =
                expression_.mid(start, position_ - start).toUpper();
            if (!accept(QLatin1Char('('))) {
                fail(QStringLiteral("宏函数 %1 缺少参数").arg(function));
                return 0.0;
            }
            const double argument = parseExpression();
            if (!accept(QLatin1Char(')'))) {
                fail(QStringLiteral("宏函数 %1 缺少右括号").arg(function));
                return 0.0;
            }
            if (function == QStringLiteral("SIN")) {
                return qSin(qDegreesToRadians(argument));
            }
            if (function == QStringLiteral("COS")) {
                return qCos(qDegreesToRadians(argument));
            }
            if (function == QStringLiteral("TAN")) {
                return qTan(qDegreesToRadians(argument));
            }
            if (function == QStringLiteral("ATAN")) {
                return qRadiansToDegrees(qAtan(argument));
            }
            if (function == QStringLiteral("ABS")) {
                return qAbs(argument);
            }
            if (function == QStringLiteral("SQRT")) {
                if (argument < 0.0) {
                    fail(QStringLiteral("SQRT 参数不能为负数"));
                    return 0.0;
                }
                return qSqrt(argument);
            }
            if (function == QStringLiteral("ROUND")) {
                return qRound(argument);
            }
            if (function == QStringLiteral("FIX")) {
                return qFloor(argument);
            }
            if (function == QStringLiteral("FUP")) {
                return qCeil(argument);
            }
            fail(QStringLiteral("不支持的宏函数 %1").arg(function));
            return 0.0;
        }

        bool ok = false;
        int start = position_;
        while (position_ < expression_.size()
               && (expression_.at(position_).isDigit()
                   || expression_.at(position_) == QLatin1Char('.'))) {
            ++position_;
        }
        const double value =
            expression_.mid(start, position_ - start).toDouble(&ok);
        if (!ok) {
            fail(QStringLiteral("宏表达式数值无效"));
            return 0.0;
        }
        return value;
    }

    QString expression_;
    const QHash<int, double> &locals_;
    const QHash<int, double> &commons_;
    int position_ = 0;
    bool ok_ = true;
    QString error_;
};

} // namespace

MacroEngine::MacroEngine() = default;

void MacroEngine::reset()
{
    localVariables_.clear();
    commonVariables_.clear();
}

bool MacroEngine::hasVariable(int number) const
{
    return localVariables_.contains(number)
           || commonVariables_.contains(number);
}

double MacroEngine::variable(int number) const
{
    if (localVariables_.contains(number)) {
        return localVariables_.value(number);
    }
    return commonVariables_.value(number, 0.0);
}

void MacroEngine::setVariable(int number, double value)
{
    if (number >= 1 && number <= 33) {
        localVariables_.insert(number, value);
    } else {
        commonVariables_.insert(number, value);
    }
}

bool MacroEngine::evaluateExpression(const QString &expression,
                                     double *value,
                                     QString *error) const
{
    QString normalized = expression.trimmed();
    if (normalized.startsWith(QLatin1Char('['))) {
        int depth = 0;
        for (int i = 0; i < normalized.size(); ++i) {
            if (normalized.at(i) == QLatin1Char('[')) {
                ++depth;
            } else if (normalized.at(i) == QLatin1Char(']')) {
                --depth;
                if (depth == 0) {
                    if (i == normalized.size() - 1) {
                        normalized = normalized.mid(1, normalized.size() - 2);
                    }
                    break;
                }
            }
        }
    }
    ExpressionEvaluator evaluator(normalized, localVariables_, commonVariables_);
    bool ok = true;
    const double result = evaluator.evaluate(&ok, error);
    if (value) {
        *value = result;
    }
    return ok;
}

bool MacroEngine::evaluateCondition(const QString &condition,
                                    bool *result, QString *error) const
{
    QString expression = condition.trimmed();
    if (expression.startsWith(QLatin1Char('['))
        && expression.endsWith(QLatin1Char(']'))) {
        expression = expression.mid(1, expression.size() - 2);
    }

    const QStringList operators = {
        QStringLiteral("EQ"), QStringLiteral("NE"),
        QStringLiteral("GE"), QStringLiteral("GT"),
        QStringLiteral("LE"), QStringLiteral("LT")
    };
    const QString upper = expression.toUpper();
    int depth = 0;
    for (int i = 0; i < upper.size(); ++i) {
        if (upper.at(i) == QLatin1Char('(')) {
            ++depth;
        } else if (upper.at(i) == QLatin1Char(')')) {
            --depth;
        }
        if (depth != 0) {
            continue;
        }
        for (const QString &op : operators) {
            if (!upper.mid(i, op.size()).startsWith(op)) {
                continue;
            }
            const QString leftText = expression.left(i).trimmed();
            const QString rightText =
                expression.mid(i + op.size()).trimmed();
            double left = 0.0;
            double right = 0.0;
            if (!evaluateExpression(leftText, &left, error)
                || !evaluateExpression(rightText, &right, error)) {
                return false;
            }
            if (op == QStringLiteral("EQ")) {
                *result = qAbs(left - right) < 0.000001;
            } else if (op == QStringLiteral("NE")) {
                *result = qAbs(left - right) >= 0.000001;
            } else if (op == QStringLiteral("GE")) {
                *result = left >= right;
            } else if (op == QStringLiteral("GT")) {
                *result = left > right;
            } else if (op == QStringLiteral("LE")) {
                *result = left <= right;
            } else {
                *result = left < right;
            }
            return true;
        }
    }

    double value = 0.0;
    if (!evaluateExpression(expression, &value, error)) {
        return false;
    }
    *result = qAbs(value) > 0.000001;
    return true;
}

QString MacroEngine::substituteVariables(const QString &line, bool *ok,
                                         QString *error) const
{
    QString result;
    int position = 0;
    bool success = true;
    while (position < line.size()) {
        const QChar ch = line.at(position);
        if (ch == QLatin1Char('(')) {
            const int end = line.indexOf(QLatin1Char(')'), position + 1);
            if (end < 0) {
                success = false;
                if (error) {
                    *error = QStringLiteral("宏程序括号注释没有结束");
                }
                break;
            }
            result += line.mid(position, end - position + 1);
            position = end + 1;
            continue;
        }
        if (ch == QLatin1Char(';')) {
            result += line.mid(position);
            break;
        }
        if (ch == QLatin1Char('#')) {
            int index = position + 1;
            while (index < line.size() && line.at(index).isDigit()) {
                ++index;
            }
            bool numberOk = false;
            const int number = line.mid(position + 1, index - position - 1)
                                   .toInt(&numberOk);
            if (!numberOk || !hasVariable(number)) {
                success = false;
                if (error) {
                    *error = QStringLiteral("宏变量 #%1 未赋值").arg(number);
                }
                break;
            }
            result += formatMacroValue(variable(number));
            position = index;
            continue;
        }
        if (ch == QLatin1Char('[')) {
            int depth = 0;
            int end = -1;
            for (int i = position; i < line.size(); ++i) {
                if (line.at(i) == QLatin1Char('[')) {
                    ++depth;
                } else if (line.at(i) == QLatin1Char(']')) {
                    --depth;
                    if (depth == 0) {
                        end = i;
                        break;
                    }
                }
            }
            if (end < 0) {
                success = false;
                if (error) {
                    *error = QStringLiteral("宏表达式缺少右括号");
                }
                break;
            }
            double value = 0.0;
            if (!evaluateExpression(
                    line.mid(position + 1, end - position - 1), &value, error)) {
                success = false;
                break;
            }
            result += formatMacroValue(value);
            position = end + 1;
            continue;
        }
        result += ch;
        ++position;
    }
    if (ok) {
        *ok = success;
    }
    return result;
}

QVector<GCodeBlock> MacroEngine::expandProgram(const QVector<GCodeBlock> &blocks,
                                               QStringList *errors)
{
    localVariables_.clear();
    QHash<int, int> labelIndex;
    for (int i = 0; i < blocks.size(); ++i) {
        if (blocks.at(i).sequenceNumber >= 0) {
            labelIndex.insert(blocks.at(i).sequenceNumber, i);
        }
    }

    struct WhileFrame
    {
        int startIndex = -1;
        int endIndex = -1;
        int iterations = 0;
    };

    QVector<GCodeBlock> expanded;
    QVector<WhileFrame> whileFrames;
    GCodeParser parser;
    int programCounter = 0;
    int executedLines = 0;

    auto addError = [errors](const GCodeBlock &block, const QString &message) {
        if (errors) {
            errors->append(QStringLiteral("L%1 宏程序: %2")
                               .arg(block.lineNumber)
                               .arg(message));
        }
    };

    auto findEnd = [&blocks](int startIndex, int doNumber) {
        int depth = 1;
        for (int i = startIndex + 1; i < blocks.size(); ++i) {
            QString text = stripInlineComment(
                stripSequenceNumber(blocks.at(i).rawText)).toUpper();
            if (!text.startsWith(QStringLiteral("WHILE"))) {
                // Check END below.
            } else {
                int nestedDo = 0;
                if (trailingInteger(text, &nestedDo) && nestedDo == doNumber) {
                    ++depth;
                }
            }
            if (text.startsWith(QStringLiteral("END"))) {
                int endDo = 0;
                if (trailingInteger(text, &endDo) && endDo == doNumber) {
                    --depth;
                    if (depth == 0) {
                        return i;
                    }
                }
            }
        }
        return -1;
    };

    while (programCounter >= 0 && programCounter < blocks.size()) {
        if (++executedLines > 20000) {
            if (errors) {
                errors->append(QStringLiteral("宏程序执行超过 20000 行，已中止"));
            }
            break;
        }
        if (expanded.size() > 50000) {
            if (errors) {
                errors->append(QStringLiteral("宏程序展开超过 50000 段，已中止"));
            }
            break;
        }

        const GCodeBlock &block = blocks.at(programCounter);
        const QString macroText = stripInlineComment(
            stripSequenceNumber(block.rawText));
        const QString upper = macroText.toUpper();

        if (upper.startsWith(QStringLiteral("WHILE"))) {
            QString condition;
            if (!extractBracketExpression(upper, &condition)) {
                addError(block, QStringLiteral("WHILE 缺少条件表达式"));
                break;
            }
            int doNumber = 0;
            if (!trailingInteger(upper, &doNumber)) {
                addError(block, QStringLiteral("WHILE 缺少 DO 编号"));
                break;
            }
            const int endIndex = findEnd(programCounter, doNumber);
            if (endIndex < 0) {
                addError(block, QStringLiteral("WHILE 找不到对应 END"));
                break;
            }

            bool conditionResult = false;
            QString conditionError;
            if (!evaluateCondition(condition, &conditionResult, &conditionError)) {
                addError(block, conditionError);
                break;
            }
            if (!conditionResult) {
                if (!whileFrames.isEmpty()
                    && whileFrames.last().startIndex == programCounter) {
                    whileFrames.removeLast();
                }
                programCounter = endIndex + 1;
                continue;
            }

            WhileFrame *frame = nullptr;
            if (!whileFrames.isEmpty()
                && whileFrames.last().startIndex == programCounter) {
                frame = &whileFrames.last();
            } else {
                whileFrames.append({ programCounter, endIndex, 0 });
                frame = &whileFrames.last();
            }
            ++frame->iterations;
            if (frame->iterations > 1000) {
                addError(block, QStringLiteral("WHILE 循环次数超过 1000"));
                break;
            }
            ++programCounter;
            continue;
        }

        if (upper.startsWith(QStringLiteral("END"))) {
            if (whileFrames.isEmpty()) {
                addError(block, QStringLiteral("END 没有对应 WHILE"));
                break;
            }
            const WhileFrame frame = whileFrames.last();
            programCounter = frame.startIndex;
            continue;
        }

        if (upper.startsWith(QStringLiteral("IF"))) {
            const int gotoIndex = upper.indexOf(QStringLiteral("GOTO"));
            if (gotoIndex < 0) {
                addError(block, QStringLiteral("IF 缺少 GOTO"));
                break;
            }
            QString condition;
            if (!extractBracketExpression(upper.left(gotoIndex), &condition)) {
                addError(block, QStringLiteral("IF 缺少条件表达式"));
                break;
            }
            int target = 0;
            if (!trailingInteger(upper.mid(gotoIndex + 4), &target)
                || !labelIndex.contains(target)) {
                addError(block, QStringLiteral("IF GOTO 目标 N%1 不存在")
                                    .arg(target));
                break;
            }
            bool conditionResult = false;
            QString conditionError;
            if (!evaluateCondition(condition, &conditionResult, &conditionError)) {
                addError(block, conditionError);
                break;
            }
            if (conditionResult) {
                whileFrames.clear();
                programCounter = labelIndex.value(target);
            } else {
                ++programCounter;
            }
            continue;
        }

        if (upper.startsWith(QStringLiteral("GOTO"))) {
            int target = 0;
            if (!trailingInteger(upper.mid(4), &target)
                || !labelIndex.contains(target)) {
                addError(block, QStringLiteral("GOTO 目标 N%1 不存在")
                                    .arg(target));
                break;
            }
            whileFrames.clear();
            programCounter = labelIndex.value(target);
            continue;
        }

        if (upper.startsWith(QLatin1Char('#'))) {
            const int equals = upper.indexOf(QLatin1Char('='));
            if (equals <= 1) {
                addError(block, QStringLiteral("宏变量赋值格式错误"));
                break;
            }
            bool numberOk = false;
            const int variableNumber =
                upper.mid(1, equals - 1).trimmed().toInt(&numberOk);
            if (!numberOk) {
                addError(block, QStringLiteral("宏变量编号无效"));
                break;
            }
            double value = 0.0;
            QString expressionError;
            if (!evaluateExpression(upper.mid(equals + 1), &value,
                                    &expressionError)) {
                addError(block, expressionError);
                break;
            }
            setVariable(variableNumber, value);
            ++programCounter;
            continue;
        }

        bool substitutionOk = true;
        QString substitutionError;
        const QString substituted = substituteVariables(
            block.rawText, &substitutionOk, &substitutionError);
        if (!substitutionOk) {
            addError(block, substitutionError);
            ++programCounter;
            continue;
        }

        QVector<GCodeParseError> parseErrors;
        const GCodeBlock parsed = parser.parseBlock(
            substituted, block.lineNumber, &parseErrors);
        if (!parseErrors.isEmpty()) {
            addError(block, parseErrors.first().message);
        } else {
            expanded.append(parsed);
        }
        ++programCounter;
    }

    if (!whileFrames.isEmpty() && errors) {
        errors->append(QStringLiteral("宏程序存在未闭合的 WHILE 循环"));
    }
    return expanded;
}
