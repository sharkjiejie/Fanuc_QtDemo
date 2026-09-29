#ifndef MODEL_MACHINEPARAMETER_H
#define MODEL_MACHINEPARAMETER_H

#include <QString>
#include <QStringList>

struct MachineParameter
{
    int number = 0;
    QString key;
    QString name;
    QString value;
    QString description;
    QStringList allowedValues;
};

#endif
