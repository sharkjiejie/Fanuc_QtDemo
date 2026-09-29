#ifndef CORE_GCODEPARSER_H
#define CORE_GCODEPARSER_H

#include "model/GCodeBlock.h"

class GCodeParser
{
public:
    GCodeParseResult parseProgram(const QStringList &lines) const;
    GCodeBlock parseBlock(const QString &line, int sourceLine,
                          QVector<GCodeParseError> *errors = nullptr) const;
};

#endif
