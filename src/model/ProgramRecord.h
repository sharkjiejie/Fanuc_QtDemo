#ifndef MODEL_PROGRAMRECORD_H
#define MODEL_PROGRAMRECORD_H

#include <QString>

struct ProgramRecord
{
    int number = 0;
    QString name;
    int lineCount = 0;
    QString updatedAt;
    QString content;
};

#endif
