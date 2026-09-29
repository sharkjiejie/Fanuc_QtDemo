#include "data/SamplePrograms.h"

#include <QStringList>

namespace {

ProgramRecord makeProgram(int number, const QString &name,
                          const QStringList &lines)
{
    ProgramRecord record;
    record.number = number;
    record.name = name;
    record.lineCount = lines.size();
    record.content = lines.join(QLatin1Char('\n'));
    return record;
}

} // namespace

QVector<ProgramRecord> samplePrograms()
{
    return {
        makeProgram(1000, QStringLiteral("SHAFT DEMO"), {
            QStringLiteral("O1000 ;"),
            QStringLiteral(" G99 G21 G40 ;"),
            QStringLiteral(" G28 U0 W0 ;"),
            QStringLiteral(" T0101 ;"),
            QStringLiteral(" G96 S180 M03 ;"),
            QStringLiteral(" G00 X62.0 Z2.0 ;"),
            QStringLiteral(" G71 U2.5 R0.5 ;"),
            QStringLiteral(" G71 P80 Q130 U0.5 W0.1 F0.25 ;"),
            QStringLiteral(" N80 G00 X28.0 ;"),
            QStringLiteral(" N90 G01 Z0.0 F0.15 ;"),
            QStringLiteral(" N100 X32.0 Z-2.0 ;"),
            QStringLiteral(" N110 Z-35.0 ;"),
            QStringLiteral(" N120 X46.0 Z-50.0 ;"),
            QStringLiteral(" N130 Z-65.0 ;"),
            QStringLiteral(" G70 P80 Q130 ;"),
            QStringLiteral(" G28 U0 W0 ;"),
            QStringLiteral(" M30 ;")
        }),
        makeProgram(1001, QStringLiteral("FLANGE"), {
            QStringLiteral("O1001 ;"),
            QStringLiteral(" G99 G21 G40 ;"),
            QStringLiteral(" T0202 ;"),
            QStringLiteral(" G96 S220 M03 ;"),
            QStringLiteral(" G00 X80.0 Z2.0 ;"),
            QStringLiteral(" G01 Z-12.0 F0.20 ;"),
            QStringLiteral(" G00 X100.0 ;"),
            QStringLiteral(" M30 ;")
        }),
        makeProgram(1002, QStringLiteral("BUSHING"), {
            QStringLiteral("O1002 ;"),
            QStringLiteral(" G99 G21 G40 ;"),
            QStringLiteral(" T0303 ;"),
            QStringLiteral(" G97 S600 M03 ;"),
            QStringLiteral(" G00 X42.0 Z2.0 ;"),
            QStringLiteral(" G01 Z-30.0 F0.15 ;"),
            QStringLiteral(" G00 X60.0 ;"),
            QStringLiteral(" M30 ;")
        }),
        makeProgram(1003, QStringLiteral("STEP SHAFT COMPLETE"), {
            QStringLiteral("O1003 (STEP SHAFT OD ROUGH/FINISH) ;"),
            QStringLiteral(" G99 G21 G40 ;"),
            QStringLiteral(" G28 U0 W0 ;"),
            QStringLiteral(" G50 S2200 ;"),
            QStringLiteral(" T0101 ;"),
            QStringLiteral(" G96 S180 M03 ;"),
            QStringLiteral(" G00 X70.0 Z3.0 ;"),
            QStringLiteral(" G71 U2.2 R0.6 ;"),
            QStringLiteral(" G71 P100 Q180 U0.6 W0.1 F0.25 ;"),
            QStringLiteral(" N100 G00 X32.0 ;"),
            QStringLiteral(" N110 G01 Z0.0 F0.15 ;"),
            QStringLiteral(" N120 X40.0 ;"),
            QStringLiteral(" N130 X44.0 Z-2.0 ;"),
            QStringLiteral(" N140 Z-28.0 ;"),
            QStringLiteral(" N150 X52.0 ;"),
            QStringLiteral(" N160 Z-46.0 ;"),
            QStringLiteral(" N170 X62.0 ;"),
            QStringLiteral(" N180 X68.0 Z-52.0 ;"),
            QStringLiteral(" G70 P100 Q180 ;"),
            QStringLiteral(" G00 X100.0 Z50.0 ;"),
            QStringLiteral(" M09 ;"),
            QStringLiteral(" M05 ;"),
            QStringLiteral(" M30 ;")
        }),
        makeProgram(1004, QStringLiteral("RADIUS PROFILE"), {
            QStringLiteral("O1004 (RADIUS NOSE PROFILE) ;"),
            QStringLiteral(" G99 G21 G40 ;"),
            QStringLiteral(" T0101 ;"),
            QStringLiteral(" G96 S200 M03 ;"),
            QStringLiteral(" G00 X62.0 Z3.0 ;"),
            QStringLiteral(" G71 U2.0 R0.5 ;"),
            QStringLiteral(" G71 P100 Q170 U0.6 W0.1 F0.22 ;"),
            QStringLiteral(" N100 G00 X18.0 ;"),
            QStringLiteral(" N110 G01 Z0.0 F0.15 ;"),
            QStringLiteral(" N120 G03 X36.0 Z-9.0 R11.0 ;"),
            QStringLiteral(" N130 G01 Z-24.0 ;"),
            QStringLiteral(" N140 G02 X48.0 Z-30.0 R8.0 ;"),
            QStringLiteral(" N150 G01 Z-42.0 ;"),
            QStringLiteral(" N160 X56.0 ;"),
            QStringLiteral(" N170 X62.0 Z-45.0 ;"),
            QStringLiteral(" G70 P100 Q170 ;"),
            QStringLiteral(" G00 X100.0 Z50.0 ;"),
            QStringLiteral(" M09 ;"),
            QStringLiteral(" M05 ;"),
            QStringLiteral(" M30 ;")
        }),
        makeProgram(1005, QStringLiteral("DEEP BORE CYCLE"), {
            QStringLiteral("O1005 (DEEP BORE AND FINISH) ;"),
            QStringLiteral(" G99 G21 G40 ;"),
            QStringLiteral(" G28 U0 W0 ;"),
            QStringLiteral(" T0707 ;"),
            QStringLiteral(" G97 S700 M03 ;"),
            QStringLiteral(" G00 X0.0 Z5.0 ;"),
            QStringLiteral(" G74 R1.0 ;"),
            QStringLiteral(" G74 X0.0 Z-45.0 Q4.0 F0.08 M28 ;"),
            QStringLiteral(" G00 Z3.0 ;"),
            QStringLiteral(" G01 X34.0 Z0.0 F0.12 ;"),
            QStringLiteral(" Z-42.0 ;"),
            QStringLiteral(" X30.0 ;"),
            QStringLiteral(" G00 Z3.0 ;"),
            QStringLiteral(" X0.0 ;"),
            QStringLiteral(" G00 X100.0 Z50.0 ;"),
            QStringLiteral(" M09 ;"),
            QStringLiteral(" M05 ;"),
            QStringLiteral(" M30 ;")
        }),
        makeProgram(1006, QStringLiteral("METRIC THREAD M20X1.5"), {
            QStringLiteral("O1006 (M20X1.5 THREAD) ;"),
            QStringLiteral(" G99 G21 G40 ;"),
            QStringLiteral(" T0303 ;"),
            QStringLiteral(" G97 S550 M03 ;"),
            QStringLiteral(" G00 X24.0 Z3.0 ;"),
            QStringLiteral(" G01 X18.4 Z0.0 F0.10 ;"),
            QStringLiteral(" G00 X24.0 ;"),
            QStringLiteral(" Z3.0 ;"),
            QStringLiteral(" G76 P020060 Q80 R30 ;"),
            QStringLiteral(" G76 X18.376 Z-24.0 P929 Q300 F1.5 ;"),
            QStringLiteral(" G00 X100.0 Z50.0 ;"),
            QStringLiteral(" M09 ;"),
            QStringLiteral(" M05 ;"),
            QStringLiteral(" M30 ;")
        }),
        makeProgram(1007, QStringLiteral("MACRO STEP TURN"), {
            QStringLiteral("O1007 (MACRO STEP TURN) ;"),
            QStringLiteral(" G99 G21 G40 ;"),
            QStringLiteral(" T0505 ;"),
            QStringLiteral(" G97 S650 M03 ;"),
            QStringLiteral(" #100=0 ;"),
            QStringLiteral(" #101=5.0 ;"),
            QStringLiteral(" #102=42.0 ;"),
            QStringLiteral(" N10 WHILE [#100 LT 5.0] DO 1 ;"),
            QStringLiteral(" N20 #100=[#100+1.0] ;"),
            QStringLiteral(" N30 #103=[#102-#101*#100] ;"),
            QStringLiteral(" N40 G00 X#103 Z2.0 ;"),
            QStringLiteral(" N50 G01 Z-36.0 F0.12 ;"),
            QStringLiteral(" N60 G00 X50.0 ;"),
            QStringLiteral(" N70 Z2.0 ;"),
            QStringLiteral(" N80 END 1 ;"),
            QStringLiteral(" G00 X46.0 Z1.0 ;"),
            QStringLiteral(" G01 X12.0 Z-17.0 F0.10 ;"),
            QStringLiteral(" G00 X60.0 ;"),
            QStringLiteral(" G00 X100.0 Z50.0 ;"),
            QStringLiteral(" M09 ;"),
            QStringLiteral(" M05 ;"),
            QStringLiteral(" M30 ;")
        }),
        makeProgram(1008, QStringLiteral("MULTI TOOL ADAPTER"), {
            QStringLiteral("O1008 (MULTI TOOL ADAPTER) ;"),
            QStringLiteral(" G99 G21 G40 ;"),
            QStringLiteral(" G28 U0 W0 ;"),
            QStringLiteral(" G50 S2400 ;"),
            QStringLiteral(" T0101 ;"),
            QStringLiteral(" G96 S190 M03 ;"),
            QStringLiteral(" G00 X94.0 Z3.0 ;"),
            QStringLiteral(" G71 U2.5 R0.6 ;"),
            QStringLiteral(" G71 P100 Q180 U0.8 W0.1 F0.25 ;"),
            QStringLiteral(" N100 G00 X44.0 ;"),
            QStringLiteral(" N110 G01 Z0.0 F0.15 ;"),
            QStringLiteral(" N120 X56.0 ;"),
            QStringLiteral(" N130 X62.0 Z-3.0 ;"),
            QStringLiteral(" N140 Z-32.0 ;"),
            QStringLiteral(" N150 X76.0 ;"),
            QStringLiteral(" N160 Z-50.0 ;"),
            QStringLiteral(" N170 X88.0 ;"),
            QStringLiteral(" N180 X92.0 Z-52.0 ;"),
            QStringLiteral(" G70 P100 Q180 ;"),
            QStringLiteral(" G00 X120.0 Z80.0 ;"),
            QStringLiteral(" T0303 ;"),
            QStringLiteral(" G97 S650 M03 ;"),
            QStringLiteral(" G00 X64.0 Z4.0 ;"),
            QStringLiteral(" G76 P020060 Q80 R30 ;"),
            QStringLiteral(" G76 X59.8 Z-26.0 P1100 Q320 F2.0 ;"),
            QStringLiteral(" G00 X120.0 Z80.0 ;"),
            QStringLiteral(" T0707 ;"),
            QStringLiteral(" G97 S700 M03 ;"),
            QStringLiteral(" G00 X0.0 Z5.0 ;"),
            QStringLiteral(" G74 R1.0 ;"),
            QStringLiteral(" G74 X0.0 Z-16.0 Q3.0 F0.08 M28 ;"),
            QStringLiteral(" G00 X120.0 Z80.0 ;"),
            QStringLiteral(" M09 ;"),
            QStringLiteral(" M05 ;"),
            QStringLiteral(" M30 ;")
        })
    };
}
