#include "core/GCodeParser.h"

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
    expect(qAbs(actual - expected) < 0.000001, message);
}

} // namespace

int main()
{
    GCodeParser parser;

    const GCodeParseResult basic = parser.parseProgram({
        QStringLiteral("O1000 (SHAFT DEMO)"),
        QStringLiteral("N120 G01X50.0Z-2.0M08"),
        QStringLiteral("G00 X51, Z3 ; RETRACT")
    });

    expect(basic.isValid(), "basic program should be valid");
    expect(basic.blocks.size() == 3, "should parse three blocks");
    if (basic.blocks.size() == 3) {
        expect(basic.blocks.at(0).programNumber == 1000,
               "program number should be parsed");
        expect(basic.blocks.at(1).sequenceNumber == 120,
               "sequence number should be parsed");
        expectNear(basic.blocks.at(1).wordValue(QLatin1Char('G')), 1.0,
                   "G code should be parsed");
        expectNear(basic.blocks.at(1).wordValue(QLatin1Char('X')), 50.0,
                   "X coordinate should be parsed");
        expectNear(basic.blocks.at(1).wordValue(QLatin1Char('Z')), -2.0,
                   "Z coordinate should be parsed");
        expectNear(basic.blocks.at(1).wordValue(QLatin1Char('M')), 8.0,
                   "M code should be parsed");
        expectNear(basic.blocks.at(2).wordValue(QLatin1Char('X')), 51.0,
                   "comma-separated value should be parsed");
        expect(basic.blocks.at(2).comments.contains(QStringLiteral("RETRACT")),
               "semicolon comment should be parsed");
    }

    QVector<GCodeParseError> errors;
    parser.parseBlock(QStringLiteral("G01 X Z2"), 7, &errors);
    expect(!errors.isEmpty(), "missing X value should report an error");

    QVector<GCodeParseError> bracketErrors;
    parser.parseBlock(QStringLiteral("G01 X10 (ROUGH"), 9, &bracketErrors);
    expect(!bracketErrors.isEmpty(), "unfinished bracket comment should fail");

    const GCodeBlock omittedDecimal =
        parser.parseBlock(QStringLiteral("X50 Z2"), 3);
    expectNear(omittedDecimal.wordValue(QLatin1Char('X')), 50.0,
               "omitted decimal should remain 50");

    return failures == 0 ? 0 : 1;
}
