#include "data/AppDatabase.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

namespace {

const char *kConnectionName = "fanuc_oitf_demo_connection";

QString writableDataDirectory(const QString &preferredDirectory)
{
    QString directory = preferredDirectory;
    if (!directory.isEmpty()
        && QDir().mkpath(directory)
        && QFileInfo(directory).isWritable()) {
        return directory;
    }

    directory = QCoreApplication::applicationDirPath();
    QDir().mkpath(directory);
    return directory;
}

} // namespace

AppDatabase::AppDatabase()
    : connectionName_(QString::fromLatin1(kConnectionName))
{
}

AppDatabase::~AppDatabase()
{
    if (database_.isOpen()) {
        database_.close();
    }
    database_ = QSqlDatabase();
    if (!connectionName_.isEmpty()) {
        QSqlDatabase::removeDatabase(connectionName_);
    }
}

bool AppDatabase::open(const QString &preferredDirectory)
{
    if (database_.isOpen()) {
        return true;
    }

    const QString dataDirectory = writableDataDirectory(preferredDirectory);
    databasePath_ = QDir(dataDirectory).filePath(QStringLiteral("fanuc_oitf_demo_v3.db"));

    if (QSqlDatabase::contains(connectionName_)) {
        database_ = QSqlDatabase::database(connectionName_);
    } else {
        database_ = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName_);
    }
    database_.setDatabaseName(databasePath_);

    if (!database_.open()) {
        lastError_ = database_.lastError().text();
        return false;
    }
    return true;
}

bool AppDatabase::initialize()
{
    if (!isOpen() && !database_.open()) {
        lastError_ = database_.lastError().text();
        return false;
    }
    if (!createTables()) {
        return false;
    }
    return ensureProgramDirectory();
}

bool AppDatabase::isOpen() const
{
    return database_.isOpen();
}

QString AppDatabase::databasePath() const
{
    return databasePath_;
}

QString AppDatabase::programDirectory() const
{
    return programDirectory_;
}

QString AppDatabase::lastError() const
{
    return lastError_;
}

bool AppDatabase::createTables()
{
    QSqlQuery query(database_);
    const QStringList statements = {
        QStringLiteral(
            "CREATE TABLE IF NOT EXISTS tools ("
            "tool_no INTEGER PRIMARY KEY,"
            "touch_machine_x REAL NOT NULL,"
            "touch_machine_z REAL NOT NULL,"
            "absolute_x REAL NOT NULL,"
            "absolute_z REAL NOT NULL,"
            "wear_x REAL NOT NULL,"
            "wear_z REAL NOT NULL,"
            "radius REAL NOT NULL,"
            "wear_radius REAL NOT NULL,"
            "tip INTEGER NOT NULL)"),
        QStringLiteral(
            "CREATE TABLE IF NOT EXISTS machine_state ("
            "id INTEGER PRIMARY KEY CHECK (id = 1),"
            "machine_x REAL NOT NULL,"
            "machine_z REAL NOT NULL,"
            "active_tool INTEGER NOT NULL)"),
        QStringLiteral(
            "CREATE TABLE IF NOT EXISTS work_offsets ("
            "offset_no INTEGER PRIMARY KEY,"
            "machine_x REAL NOT NULL,"
            "machine_z REAL NOT NULL)"),
        QStringLiteral(
            "CREATE TABLE IF NOT EXISTS coordinate_state ("
            "id INTEGER PRIMARY KEY CHECK (id = 1),"
            "active_work_offset INTEGER NOT NULL,"
            "relative_origin_machine_x REAL NOT NULL,"
            "relative_origin_machine_z REAL NOT NULL)"),
        QStringLiteral(
            "CREATE TABLE IF NOT EXISTS programs ("
            "program_no INTEGER PRIMARY KEY,"
            "program_name TEXT NOT NULL,"
            "line_count INTEGER NOT NULL,"
            "updated_at TEXT NOT NULL,"
            "content TEXT NOT NULL)"),
        QStringLiteral(
            "CREATE TABLE IF NOT EXISTS settings ("
            "key TEXT PRIMARY KEY,"
            "value TEXT NOT NULL)")
    };

    for (const QString &statement : statements) {
        if (!query.exec(statement)) {
            lastError_ = query.lastError().text();
            return false;
        }
    }

    QSqlQuery countWorkOffsets(database_);
    if (countWorkOffsets.exec(QStringLiteral(
            "SELECT COUNT(*) FROM work_offsets"))
        && countWorkOffsets.next()
        && countWorkOffsets.value(0).toInt() == 0) {
        for (int number = 1; number <= 6; ++number) {
            QSqlQuery insert(database_);
            insert.prepare(QStringLiteral(
                "INSERT INTO work_offsets "
                "(offset_no, machine_x, machine_z) VALUES (?, 0, 0)"));
            insert.addBindValue(number);
            if (!insert.exec()) {
                lastError_ = insert.lastError().text();
                return false;
            }
        }
    }

    QSqlQuery coordinateState(database_);
    if (!coordinateState.exec(QStringLiteral(
            "INSERT OR IGNORE INTO coordinate_state "
            "(id, active_work_offset, relative_origin_machine_x, "
            "relative_origin_machine_z) VALUES (1, 1, 0, 0)"))) {
        lastError_ = coordinateState.lastError().text();
        return false;
    }
    return true;
}

bool AppDatabase::ensureProgramDirectory()
{
    programDirectory_ = QDir(QFileInfo(databasePath_).absolutePath())
                            .filePath(QStringLiteral("programs"));

    QSqlQuery query(database_);
    query.prepare(QStringLiteral(
        "INSERT OR IGNORE INTO settings (key, value) "
        "VALUES ('program_directory', ?)"));
    query.addBindValue(programDirectory_);
    if (!query.exec()) {
        lastError_ = query.lastError().text();
        return false;
    }

    if (query.exec(QStringLiteral(
            "SELECT value FROM settings WHERE key = 'program_directory'"))
        && query.next()) {
        programDirectory_ = query.value(0).toString();
    }
    if (!QDir().mkpath(programDirectory_)) {
        lastError_ = QStringLiteral("无法创建程序目录: %1").arg(programDirectory_);
        return false;
    }
    return true;
}

QVector<ToolOffset> AppDatabase::loadTools() const
{
    QVector<ToolOffset> tools;
    QSqlQuery query(database_);
    if (!query.exec(QStringLiteral(
            "SELECT tool_no, touch_machine_x, touch_machine_z, "
            "absolute_x, absolute_z, wear_x, wear_z, "
            "radius, wear_radius, tip FROM tools ORDER BY tool_no"))) {
        lastError_ = query.lastError().text();
        return tools;
    }

    while (query.next()) {
        const int index = query.value(0).toInt() - 1;
        if (index < 0 || index >= 12) {
            continue;
        }
        if (tools.size() < 12) {
            tools.resize(12);
        }
        ToolOffset &tool = tools[index];
        tool.touchMachineX = query.value(1).toDouble();
        tool.touchMachineZ = query.value(2).toDouble();
        tool.absoluteX = query.value(3).toDouble();
        tool.absoluteZ = query.value(4).toDouble();
        tool.wearX = query.value(5).toDouble();
        tool.wearZ = query.value(6).toDouble();
        tool.radius = query.value(7).toDouble();
        tool.wearRadius = query.value(8).toDouble();
        tool.tip = query.value(9).toInt();
    }
    return tools;
}

int AppDatabase::toolCount() const
{
    QSqlQuery query(database_);
    if (!query.exec(QStringLiteral("SELECT COUNT(*) FROM tools")) || !query.next()) {
        return 0;
    }
    return query.value(0).toInt();
}

bool AppDatabase::saveTool(int index, const ToolOffset &tool)
{
    if (index < 0 || index >= 12) {
        return false;
    }

    QSqlQuery query(database_);
    query.prepare(QStringLiteral(
        "INSERT OR REPLACE INTO tools "
        "(tool_no, touch_machine_x, touch_machine_z, absolute_x, absolute_z, "
        "wear_x, wear_z, radius, wear_radius, tip) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"));
    query.addBindValue(index + 1);
    query.addBindValue(tool.touchMachineX);
    query.addBindValue(tool.touchMachineZ);
    query.addBindValue(tool.absoluteX);
    query.addBindValue(tool.absoluteZ);
    query.addBindValue(tool.wearX);
    query.addBindValue(tool.wearZ);
    query.addBindValue(tool.radius);
    query.addBindValue(tool.wearRadius);
    query.addBindValue(tool.tip);
    if (!query.exec()) {
        lastError_ = query.lastError().text();
        return false;
    }
    return true;
}

bool AppDatabase::loadMachineState(double *machineX, double *machineZ,
                                   int *activeTool, bool *found) const
{
    QSqlQuery query(database_);
    if (!query.exec(QStringLiteral(
            "SELECT machine_x, machine_z, active_tool "
            "FROM machine_state WHERE id = 1"))) {
        lastError_ = query.lastError().text();
        if (found) {
            *found = false;
        }
        return false;
    }

    if (!query.next()) {
        if (found) {
            *found = false;
        }
        return true;
    }

    if (machineX) {
        *machineX = query.value(0).toDouble();
    }
    if (machineZ) {
        *machineZ = query.value(1).toDouble();
    }
    if (activeTool) {
        *activeTool = query.value(2).toInt();
    }
    if (found) {
        *found = true;
    }
    return true;
}

bool AppDatabase::saveMachineState(double machineX, double machineZ, int activeTool)
{
    QSqlQuery query(database_);
    query.prepare(QStringLiteral(
        "INSERT OR REPLACE INTO machine_state "
        "(id, machine_x, machine_z, active_tool) VALUES (1, ?, ?, ?)"));
    query.addBindValue(machineX);
    query.addBindValue(machineZ);
    query.addBindValue(activeTool);
    if (!query.exec()) {
        lastError_ = query.lastError().text();
        return false;
    }
    return true;
}

QVector<WorkOffset> AppDatabase::loadWorkOffsets() const
{
    QVector<WorkOffset> offsets;
    QSqlQuery query(database_);
    if (!query.exec(QStringLiteral(
            "SELECT offset_no, machine_x, machine_z "
            "FROM work_offsets ORDER BY offset_no"))) {
        lastError_ = query.lastError().text();
        return offsets;
    }
    while (query.next()) {
        WorkOffset offset;
        offset.number = query.value(0).toInt();
        offset.machineX = query.value(1).toDouble();
        offset.machineZ = query.value(2).toDouble();
        offsets.append(offset);
    }
    return offsets;
}

bool AppDatabase::saveWorkOffset(const WorkOffset &offset)
{
    QSqlQuery query(database_);
    query.prepare(QStringLiteral(
        "INSERT OR REPLACE INTO work_offsets "
        "(offset_no, machine_x, machine_z) VALUES (?, ?, ?)"));
    query.addBindValue(offset.number);
    query.addBindValue(offset.machineX);
    query.addBindValue(offset.machineZ);
    if (!query.exec()) {
        lastError_ = query.lastError().text();
        return false;
    }
    return true;
}

CoordinateState AppDatabase::loadCoordinateState() const
{
    CoordinateState state;
    QSqlQuery query(database_);
    if (query.exec(QStringLiteral(
            "SELECT active_work_offset, relative_origin_machine_x, "
            "relative_origin_machine_z FROM coordinate_state WHERE id = 1"))
        && query.next()) {
        state.activeWorkOffset = query.value(0).toInt();
        state.relativeOriginMachineX = query.value(1).toDouble();
        state.relativeOriginMachineZ = query.value(2).toDouble();
    }
    return state;
}

bool AppDatabase::saveCoordinateState(const CoordinateState &state)
{
    QSqlQuery query(database_);
    query.prepare(QStringLiteral(
        "INSERT OR REPLACE INTO coordinate_state "
        "(id, active_work_offset, relative_origin_machine_x, "
        "relative_origin_machine_z) VALUES (1, ?, ?, ?)"));
    query.addBindValue(state.activeWorkOffset);
    query.addBindValue(state.relativeOriginMachineX);
    query.addBindValue(state.relativeOriginMachineZ);
    if (!query.exec()) {
        lastError_ = query.lastError().text();
        return false;
    }
    return true;
}

QVector<ProgramRecord> AppDatabase::loadPrograms() const
{
    QVector<ProgramRecord> programs;
    QSqlQuery query(database_);
    if (!query.exec(QStringLiteral(
            "SELECT program_no, program_name, line_count, updated_at, content "
            "FROM programs ORDER BY program_no"))) {
        lastError_ = query.lastError().text();
        return programs;
    }

    while (query.next()) {
        ProgramRecord record;
        record.number = query.value(0).toInt();
        record.name = query.value(1).toString();
        record.lineCount = query.value(2).toInt();
        record.updatedAt = query.value(3).toString();
        record.content = query.value(4).toString();
        programs.append(record);
    }
    return programs;
}

bool AppDatabase::saveProgram(const ProgramRecord &record)
{
    QSqlQuery query(database_);
    query.prepare(QStringLiteral(
        "INSERT OR REPLACE INTO programs "
        "(program_no, program_name, line_count, updated_at, content) "
        "VALUES (?, ?, ?, ?, ?)"));
    query.addBindValue(record.number);
    query.addBindValue(record.name);
    query.addBindValue(record.lineCount);
    query.addBindValue(record.updatedAt);
    query.addBindValue(record.content);
    if (!query.exec()) {
        lastError_ = query.lastError().text();
        return false;
    }
    return writeProgramFile(record);
}

bool AppDatabase::deleteProgram(int number)
{
    QSqlQuery query(database_);
    query.prepare(QStringLiteral("DELETE FROM programs WHERE program_no = ?"));
    query.addBindValue(number);
    if (!query.exec()) {
        lastError_ = query.lastError().text();
        return false;
    }
    return true;
}

bool AppDatabase::setProgramDirectory(const QString &directory)
{
    const QString cleanDirectory = QDir::cleanPath(directory.trimmed());
    if (cleanDirectory.isEmpty() || !QDir().mkpath(cleanDirectory)) {
        lastError_ = QStringLiteral("无法创建程序目录");
        return false;
    }

    QSqlQuery query(database_);
    query.prepare(QStringLiteral(
        "INSERT OR REPLACE INTO settings (key, value) "
        "VALUES ('program_directory', ?)"));
    query.addBindValue(cleanDirectory);
    if (!query.exec()) {
        lastError_ = query.lastError().text();
        return false;
    }
    programDirectory_ = cleanDirectory;
    return true;
}

QString AppDatabase::settingValue(const QString &key,
                                  const QString &defaultValue) const
{
    QSqlQuery query(database_);
    query.prepare(QStringLiteral(
        "SELECT value FROM settings WHERE key = ?"));
    query.addBindValue(key);
    if (!query.exec()) {
        lastError_ = query.lastError().text();
        return defaultValue;
    }
    if (query.next()) {
        return query.value(0).toString();
    }
    return defaultValue;
}

bool AppDatabase::saveSettingValue(const QString &key, const QString &value)
{
    QSqlQuery query(database_);
    query.prepare(QStringLiteral(
        "INSERT OR REPLACE INTO settings (key, value) VALUES (?, ?)"));
    query.addBindValue(key);
    query.addBindValue(value);
    if (!query.exec()) {
        lastError_ = query.lastError().text();
        return false;
    }
    return true;
}

bool AppDatabase::writeProgramFile(const ProgramRecord &record) const
{
    if (programDirectory_.isEmpty()) {
        return false;
    }
    const QString path = QDir(programDirectory_).filePath(
        QStringLiteral("O%1.nc").arg(record.number, 4, 10, QLatin1Char('0')));
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }
    return file.write(record.content.toUtf8()) >= 0;
}
