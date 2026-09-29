#ifndef DATA_APPDATABASE_H
#define DATA_APPDATABASE_H

#include <QSqlDatabase>
#include <QString>
#include <QVector>

#include "model/ProgramRecord.h"
#include "model/ToolOffset.h"
#include "model/WorkOffset.h"

class AppDatabase
{
public:
    AppDatabase();
    ~AppDatabase();

    bool open(const QString &preferredDirectory);
    bool initialize();

    bool isOpen() const;
    QString databasePath() const;
    QString programDirectory() const;
    QString lastError() const;

    QVector<ToolOffset> loadTools() const;
    int toolCount() const;
    bool saveTool(int index, const ToolOffset &tool);

    bool loadMachineState(double *machineX, double *machineZ,
                          int *activeTool, bool *found) const;
    bool saveMachineState(double machineX, double machineZ, int activeTool);

    QVector<WorkOffset> loadWorkOffsets() const;
    bool saveWorkOffset(const WorkOffset &offset);
    CoordinateState loadCoordinateState() const;
    bool saveCoordinateState(const CoordinateState &state);

    QVector<ProgramRecord> loadPrograms() const;
    bool saveProgram(const ProgramRecord &record);
    bool deleteProgram(int number);
    bool setProgramDirectory(const QString &directory);
    QString settingValue(const QString &key,
                         const QString &defaultValue = QString()) const;
    bool saveSettingValue(const QString &key, const QString &value);

private:
    bool createTables();
    bool ensureProgramDirectory();
    bool writeProgramFile(const ProgramRecord &record) const;

    QString connectionName_;
    QSqlDatabase database_;
    QString databasePath_;
    QString programDirectory_;
    mutable QString lastError_;
};

#endif
