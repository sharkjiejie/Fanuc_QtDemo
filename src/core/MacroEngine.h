#ifndef CORE_MACROENGINE_H
#define CORE_MACROENGINE_H

#include "model/GCodeBlock.h"

#include <QHash>
#include <QStringList>
#include <QVector>

class MacroEngine
{
public:
    MacroEngine();

    void reset();
    QVector<GCodeBlock> expandProgram(const QVector<GCodeBlock> &blocks,
                                      QStringList *errors = nullptr);
    bool evaluateExpression(const QString &expression, double *value,
                            QString *error = nullptr) const;

    bool hasVariable(int number) const;
    double variable(int number) const;
    void setVariable(int number, double value);

private:
    bool evaluateCondition(const QString &condition, bool *result,
                           QString *error = nullptr) const;
    QString substituteVariables(const QString &line, bool *ok,
                                QString *error = nullptr) const;

    QHash<int, double> localVariables_;
    QHash<int, double> commonVariables_;
};

#endif
