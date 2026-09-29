#include "core/GCodeParser.h"
#include "core/MacroEngine.h"

#include <QtMath>

#include <iostream>

namespace {

int failures = 0;

void expect(bool condition, const char *message)
{
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

void expectNear(double actual, double expected, const char *message)
{
    expect(qAbs(actual - expected) < 0.00001, message);
}

} // namespace

int main()
{
    MacroEngine engine;
    engine.setVariable(100, 10.0);

    double value = 0.0;
    QString error;
    expect(engine.evaluateExpression(QStringLiteral("#100+5*2"),
                                     &value, &error),
           "macro arithmetic should evaluate");
    expectNear(value, 20.0, "operator precedence should be correct");
    expect(engine.evaluateExpression(QStringLiteral("SIN(30)"),
                                     &value, &error),
           "macro SIN should evaluate");
    expectNear(value, 0.5, "SIN(30) should be 0.5");
    expect(!engine.evaluateExpression(QStringLiteral("MOD(17,5)"),
                                      &value, &error),
           "MOD function syntax should be rejected");
    expect(engine.evaluateExpression(QStringLiteral("17 MOD 5"),
                                     &value, &error),
           "MOD operator should evaluate");
    expectNear(value, 2.0, "17 MOD 5 should be 2");

    GCodeParser parser;
    const GCodeParseResult parsed = parser.parseProgram({
        QStringLiteral("O3000"),
        QStringLiteral("#100=10"),
        QStringLiteral("#101=5"),
        QStringLiteral("N10 WHILE [#100 GT 0] DO 1"),
        QStringLiteral("N20 G01 X#100 Z[#100+#101] F0.2"),
        QStringLiteral("N30 #100=[#100-5]"),
        QStringLiteral("N40 END 1"),
        QStringLiteral("N50 IF [#101 EQ 5] GOTO 200"),
        QStringLiteral("N60 G00 X1"),
        QStringLiteral("N200 M30")
    });
    expect(parsed.isValid(), "macro source should parse without syntax errors");
    if (!parsed.isValid()) {
        for (const GCodeParseError &parseError : parsed.errors) {
            std::cerr << "Macro parse detail L" << parseError.line
                      << " C" << parseError.column << ": "
                      << parseError.message.toStdString() << '\n';
        }
    }

    QStringList expansionErrors;
    const QVector<GCodeBlock> expanded =
        engine.expandProgram(parsed.blocks, &expansionErrors);
    expect(expansionErrors.isEmpty(), "valid macro program should expand");
    if (!expansionErrors.isEmpty()) {
        for (const QString &expansionError : expansionErrors) {
            std::cerr << "Macro expansion detail: "
                      << expansionError.toStdString() << '\n';
        }
    }
    expect(expanded.size() == 4,
           "macro should keep program header and expand to two cuts and M30");
    if (expanded.size() >= 4) {
        expectNear(expanded.at(1).wordValue(QLatin1Char('X')), 10.0,
                   "first macro X should be 10");
        expectNear(expanded.at(1).wordValue(QLatin1Char('Z')), 15.0,
                   "first macro Z should be 15");
        expectNear(expanded.at(2).wordValue(QLatin1Char('X')), 5.0,
                   "second macro X should be 5");
        expectNear(expanded.at(2).wordValue(QLatin1Char('Z')), 10.0,
                   "second macro Z should be 10");
        expect(expanded.at(3).wordValue(QLatin1Char('M')) == 30.0,
               "IF GOTO should skip N60 and reach M30");
    }

    const GCodeParseResult loopPar = parser.parseProgram({
        QStringLiteral("N10 WHILE [1 EQ 1] DO 1"),
        QStringLiteral("N20 G00 X1"),
        QStringLiteral("N30 END 1")
    });
    QStringList loopErrors;
    engine.expandProgram(loopPar.blocks, &loopErrors);
    expect(!loopErrors.isEmpty(), "infinite macro loop should be stopped");

    return failures == 0 ? 0 : 1;
}
