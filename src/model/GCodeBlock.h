#ifndef MODEL_GCODEBLOCK_H
#define MODEL_GCODEBLOCK_H

#include <QChar>
#include <QString>
#include <QStringList>
#include <QVector>

struct GCodeWord
{
    QChar letter;
    QString rawValue;
    double value = 0.0;
    int column = 1;
};

struct GCodeParseError
{
    int line = 0;
    int column = 0;
    QString message;
};

struct GCodeBlock
{
    QString rawText;
    QStringList comments;
    QVector<GCodeWord> words;
    int lineNumber = 0;
    int programNumber = -1;
    int sequenceNumber = -1;
    bool optionalBlock = false;

    bool hasWord(QChar letter) const;
    double wordValue(QChar letter, bool *ok = nullptr) const;
    QString wordRawValue(QChar letter) const;
};

struct GCodeParseResult
{
    QVector<GCodeBlock> blocks;
    QVector<GCodeParseError> errors;

    bool isValid() const;
};

#endif
