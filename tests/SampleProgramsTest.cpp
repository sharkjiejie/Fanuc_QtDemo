#include "core/GCodeParser.h"
#include "core/MacroEngine.h"
#include "core/MotionPlanner.h"
#include "data/SamplePrograms.h"

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

} // namespace

int main()
{
    const QVector<ProgramRecord> programs = samplePrograms();
    expect(programs.size() >= 9, "sample catalog should contain nine programs");

    QString combined;
    for (const ProgramRecord &record : programs) {
        combined += record.content;
        combined += QLatin1Char('\n');
    }

    expect(combined.contains(QStringLiteral("G71")),
           "catalog should include roughing cycles");
    expect(combined.contains(QStringLiteral("G70")),
           "catalog should include finishing cycles");
    expect(combined.contains(QStringLiteral("G02"))
               && combined.contains(QStringLiteral("G03")),
           "catalog should include circular interpolation");
    expect(combined.contains(QStringLiteral("G74")),
           "catalog should include peck drilling");
    expect(combined.contains(QStringLiteral("G76")),
           "catalog should include threading");
    expect(combined.contains(QStringLiteral("WHILE")),
           "catalog should include a macro loop");

    GCodeParser parser;
    MacroEngine macroEngine;
    MotionPlanner planner;

    for (const ProgramRecord &record : programs) {
        const QStringList lines = record.content.split(QLatin1Char('\n'));
        const GCodeParseResult parsed = parser.parseProgram(lines);
        expect(parsed.isValid(),
               qPrintable(QStringLiteral("program O%1 should parse")
                              .arg(record.number, 4, 10, QLatin1Char('0'))));

        QStringList macroErrors;
        const QVector<GCodeBlock> expanded =
            macroEngine.expandProgram(parsed.blocks, &macroErrors);
        expect(macroErrors.isEmpty(),
               qPrintable(QStringLiteral("program O%1 macro should expand")
                              .arg(record.number, 4, 10, QLatin1Char('0'))));

        QStringList planningErrors;
        planner.buildProgram(expanded, QVector3D(0.0, 0.0, 0.0),
                             &planningErrors);
        bool hasHardError = false;
        for (const QString &message : planningErrors) {
            if (message.contains(QStringLiteral("找不到"))
                || message.contains(QStringLiteral("缺少"))
                || message.contains(QStringLiteral("无效"))
                || message.contains(QStringLiteral("嵌套"))) {
                hasHardError = true;
                std::cerr << "Planning detail: "
                          << message.toStdString() << '\n';
            }
        }
        expect(!hasHardError,
               qPrintable(QStringLiteral("program O%1 should plan without hard errors")
                              .arg(record.number, 4, 10, QLatin1Char('0'))));
    }

    return failures == 0 ? 0 : 1;
}
