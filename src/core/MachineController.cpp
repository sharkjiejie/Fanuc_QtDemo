#include "core/MachineController.h"

#include "data/AppDatabase.h"
#include "data/SamplePrograms.h"

#include <QDateTime>
#include <QDir>
#include <QStandardPaths>
#include <QTimer>
#include <QtMath>

namespace {

const ToolOffset kDefaultTools[12] = {
    { 100.000, -60.000, 50.0, 0.0, 0.000, 0.000, 0.400, 0.000, 3 },
    { 102.250, -58.500, 50.0, 0.0, -0.010, 0.008, 0.800, 0.000, 3 },
    { 98.740, -65.200, 50.0, 0.0, 0.005, -0.004, 0.400, 0.002, 6 },
    { 101.320, -57.800, 50.0, 0.0, 0.000, 0.002, 0.800, 0.000, 8 },
    { 99.880, -62.450, 50.0, 0.0, -0.002, 0.000, 0.200, -0.001, 3 },
    { 100.540, -59.700, 50.0, 0.0, 0.008, -0.005, 0.400, 0.000, 3 },
    { 103.110, -61.180, 50.0, 0.0, 0.000, 0.001, 0.800, 0.002, 6 },
    { 97.960, -56.930, 50.0, 0.0, 0.003, 0.000, 0.400, 0.000, 8 },
    { 100.220, -64.020, 50.0, 0.0, 0.004, -0.003, 0.200, -0.001, 3 },
    { 102.680, -60.340, 50.0, 0.0, -0.006, 0.004, 0.800, 0.000, 6 },
    { 99.420, -58.760, 50.0, 0.0, 0.002, 0.000, 0.400, 0.001, 3 },
    { 101.750, -63.510, 50.0, 0.0, 0.000, 0.002, 0.400, 0.000, 3 }
};

QString formatToolName(int toolNumber)
{
    return QStringLiteral("T%1%2")
        .arg(toolNumber, 2, 10, QLatin1Char('0'))
        .arg(toolNumber, 2, 10, QLatin1Char('0'));
}

QStringList splitCommand(const QString &command)
{
    QStringList tokens;
    QString token;
    for (const QChar ch : command) {
        if (ch.isSpace() || ch == QLatin1Char(',') || ch == QLatin1Char(';')) {
            if (!token.isEmpty()) {
                tokens.append(token);
                token.clear();
            }
        } else {
            token.append(ch);
        }
    }
    if (!token.isEmpty()) {
        tokens.append(token);
    }
    return tokens;
}

int parseProgramNumber(const QString &value)
{
    QString digits = value.trimmed().toUpper();
    if (digits.startsWith(QLatin1Char('O'))) {
        digits.remove(0, 1);
    }
    bool ok = false;
    const int number = digits.toInt(&ok);
    return ok ? number : -1;
}

int explicitMCode(const GCodeBlock &block)
{
    int code = -1;
    for (const GCodeWord &word : block.words) {
        if (word.letter == QLatin1Char('M')) {
            code = qRound(word.value);
        }
    }
    return code;
}

QString legacyShaftProgram()
{
    return QStringList {
        QStringLiteral("O1000 ;"),
        QStringLiteral(" G99 G21 G40 ;"),
        QStringLiteral(" G28 U0 W0 ;"),
        QStringLiteral(" T0101 ;"),
        QStringLiteral(" G96 S180 M03 ;"),
        QStringLiteral(" G00 X62.0 Z2.0 ;"),
        QStringLiteral(" G71 U2.5 R0.5 ;"),
        QStringLiteral(" G71 P80 Q130 U0.5 W0.1 F0.25 ;"),
        QStringLiteral(" G00 X28.0 ;"),
        QStringLiteral(" G01 Z0.0 F0.15 ;"),
        QStringLiteral(" X32.0 Z-2.0 ;"),
        QStringLiteral(" Z-35.0 ;"),
        QStringLiteral(" X46.0 Z-50.0 ;"),
        QStringLiteral(" Z-65.0 ;"),
        QStringLiteral(" G70 P80 Q130 ;"),
        QStringLiteral(" G28 U0 W0 ;"),
        QStringLiteral(" M30 ;")
    }.join(QLatin1Char('\n'));
}

} // namespace

MachineController::MachineController(AppDatabase *database, QObject *parent)
    : QObject(parent)
    , database_(database)
{
    timer_ = new QTimer(this);
    timer_->setInterval(120);
    connect(timer_, &QTimer::timeout, this, [this]() {
        tickProgram();
    });

    loadParameters();
    program_ = samplePrograms().constFirst().content.split(QLatin1Char('\n'));
    parseCurrentProgram();
}

bool MachineController::initialize()
{
    loadDefaultTools();

    const QString preferredDirectory =
        QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (!database_->open(preferredDirectory) || !database_->initialize()) {
        databaseError_ = database_->lastError();
        return false;
    }

    loadParameters();

    const QVector<ToolOffset> storedTools = database_->loadTools();
    if (storedTools.size() == 12 && database_->toolCount() == 12) {
        tools_ = storedTools;
    } else {
        for (int i = 0; i < tools_.size(); ++i) {
            saveTool(i);
        }
    }

    workOffsets_ = database_->loadWorkOffsets();
    const CoordinateState coordinateState = database_->loadCoordinateState();
    activeWorkOffset_ = qBound(1, coordinateState.activeWorkOffset, 6);
    relativeOriginMachineX_ = coordinateState.relativeOriginMachineX;
    relativeOriginMachineZ_ = coordinateState.relativeOriginMachineZ;

    double machineX = machineX_;
    double machineZ = machineZ_;
    int activeTool = currentToolIndex_ + 1;
    bool found = false;
    if (database_->loadMachineState(&machineX, &machineZ, &activeTool, &found)) {
        if (found) {
            machineX_ = machineX;
            machineZ_ = machineZ;
            currentToolIndex_ = qBound(0, activeTool - 1, 11);
            toolName_ = formatToolName(currentToolIndex_ + 1);
        } else {
            saveMachineState();
        }
    } else {
        databaseError_ = database_->lastError();
    }

    refreshPrograms();
    ensureDefaultPrograms();
    updateAbsoluteFromMachine();
    parseCurrentProgram();
    notify();
    return databaseError_.isEmpty();
}

void MachineController::setChangeCallback(std::function<void()> callback)
{
    changeCallback_ = std::move(callback);
}

void MachineController::setPageRequestCallback(std::function<void(const QString &)> callback)
{
    pageRequestCallback_ = std::move(callback);
}

const QVector<ToolOffset> &MachineController::tools() const
{
    return tools_;
}

const ToolOffset &MachineController::tool(int index) const
{
    static const ToolOffset fallback;
    if (index < 0 || index >= tools_.size()) {
        return fallback;
    }
    return tools_.at(index);
}

const QVector<ProgramRecord> &MachineController::programRecords() const
{
    return programs_;
}

const QStringList &MachineController::program() const
{
    return program_;
}

const QVector<ProgramInstruction> &MachineController::programInstructions() const
{
    return programInstructions_;
}

const QVector<MotionSegment> &MachineController::motionPath() const
{
    return motionPath_;
}

const QVector<MachineParameter> &MachineController::parameters() const
{
    return parameters_;
}

const QStringList &MachineController::commandLog() const
{
    return commandLog_;
}

const QStringList &MachineController::modalMessages() const
{
    return modalMessages_;
}

const GCodeParseResult &MachineController::parseResult() const
{
    return parseResult_;
}

const ModalState &MachineController::modalState() const
{
    return modalState_;
}

ModalState MachineController::modalStateForLine(int lineNumber) const
{
    return modalStateByLine_.value(lineNumber, modalState_);
}

double MachineController::machineX() const
{
    return machineX_;
}

double MachineController::machineZ() const
{
    return machineZ_;
}

double MachineController::absoluteX() const
{
    return absoluteX_;
}

double MachineController::absoluteZ() const
{
    return absoluteZ_;
}

double MachineController::feed() const
{
    return feed_;
}

double MachineController::spindleSpeed() const
{
    return spindleSpeed_;
}

int MachineController::currentLine() const
{
    return currentLine_;
}

int MachineController::selectedProgramLine() const
{
    return selectedProgramLine_;
}

int MachineController::selectedProgramColumn() const
{
    return selectedProgramColumn_;
}

int MachineController::selectedProgramLength() const
{
    return selectedProgramLength_;
}

int MachineController::selectedParameterIndex() const
{
    return selectedParameterIndex_;
}

int MachineController::currentProgramNo() const
{
    return currentProgramNo_;
}

int MachineController::currentToolIndex() const
{
    return currentToolIndex_;
}

int MachineController::partCount() const
{
    return partCount_;
}

int MachineController::runSeconds() const
{
    return runSeconds_;
}

int MachineController::cycleSeconds() const
{
    return cycleSeconds_;
}

bool MachineController::isRunning() const
{
    return running_;
}

QString MachineController::programName() const
{
    return programName_;
}

QString MachineController::toolName() const
{
    return toolName_;
}

QString MachineController::modeLine() const
{
    return modeLine_;
}

QString MachineController::modeCode() const
{
    return modeCode_;
}

QString MachineController::stateText() const
{
    return stateText_;
}

QString MachineController::lastGCode() const
{
    return lastGCode_;
}

QString MachineController::lastMCode() const
{
    return lastMCode_;
}

QString MachineController::commandLine() const
{
    return commandLine_;
}

QString MachineController::commandError() const
{
    return commandError_;
}

QString MachineController::databasePath() const
{
    return database_->databasePath();
}

QString MachineController::programDirectory() const
{
    return database_->programDirectory();
}

QString MachineController::databaseError() const
{
    return databaseError_;
}

QColor MachineController::stateColor() const
{
    return stateColor_;
}

void MachineController::setModeCode(const QString &code)
{
    modeCode_ = code;
    if (modeCode_ != QStringLiteral("EDIT")) {
        selectedProgramLength_ = 0;
    }
    modeLine_ = code == QStringLiteral("MEM")
        ? QStringLiteral("MEM STRT MTN ABS")
        : code;
    if (parameterWriteEnabled_ && !parameterEditingAllowed()) {
        setParameterWriteEnabled(false);
    }
    notify();
}

void MachineController::setOptionalStopSelected(bool selected)
{
    optionalStopSelected_ = selected;
    modalEngine_.setOptionalStopSelected(selected);
    parseCurrentProgram();
    notify();
}

bool MachineController::optionalStopSelected() const
{
    return optionalStopSelected_;
}

void MachineController::setSingleBlockSelected(bool selected)
{
    singleBlockSelected_ = selected;
    commandError_.clear();
    notify();
}

bool MachineController::singleBlockSelected() const
{
    return singleBlockSelected_;
}

bool MachineController::parameterPageActive() const
{
    return parameterPageActive_;
}

void MachineController::setParameterPageActive(bool active)
{
    parameterPageActive_ = active;
    if (parameterPageActive_ && !parameters_.isEmpty()) {
        selectedParameterIndex_ =
            qBound(0, selectedParameterIndex_, parameters_.size() - 1);
    }
    notify();
}

void MachineController::moveParameterCursor(int delta)
{
    if (parameters_.isEmpty()) {
        return;
    }
    selectedParameterIndex_ =
        qBound(0, selectedParameterIndex_ + delta, parameters_.size() - 1);
    notify();
}

void MachineController::selectParameter(int index)
{
    if (parameters_.isEmpty()) {
        return;
    }
    selectedParameterIndex_ =
        qBound(0, index, parameters_.size() - 1);
    notify();
}

bool MachineController::parameterEditingAllowed() const
{
    return modeCode_ == QStringLiteral("MDI")
           || stateText_ == QStringLiteral("急停中");
}

bool MachineController::parameterWriteEnabled() const
{
    return parameterWriteEnabled_;
}

bool MachineController::setParameterWriteEnabled(bool enabled)
{
    if (enabled && !parameterEditingAllowed()) {
        commandError_ =
            QStringLiteral("请选择 MDI 模式或急停状态后再修改参数");
        notify();
        return false;
    }

    parameterWriteEnabled_ = enabled;
    if (!parameters_.isEmpty()) {
        parameters_[0].value = enabled ? QStringLiteral("1")
                                       : QStringLiteral("0");
        saveParameter(0);
    }
    commandError_.clear();
    notify();
    return true;
}

bool MachineController::applyParameterInput(const QString &value)
{
    if (!parameterEditingAllowed()) {
        commandError_ =
            QStringLiteral("请选择 MDI 模式或急停状态后再修改参数");
        notify();
        return false;
    }
    if (parameters_.isEmpty()) {
        commandError_ = QStringLiteral("没有可编辑参数");
        notify();
        return false;
    }
    if (selectedParameterIndex_ != 0 && !parameterWriteEnabled_) {
        commandError_ = QStringLiteral("请先将“写参数”设为 1");
        notify();
        return false;
    }

    const QString normalized = value.trimmed().toUpper();
    if (normalized.isEmpty()) {
        commandError_ = QStringLiteral("请输入参数值");
        notify();
        return false;
    }

    MachineParameter &parameter = parameters_[selectedParameterIndex_];
    bool valid = parameter.allowedValues.contains(normalized);
    if (parameter.key == QStringLiteral("io_channel")) {
        bool ok = false;
        const int channel = normalized.toInt(&ok);
        valid = ok && channel >= 0 && channel <= 35;
    } else if (parameter.key.startsWith(QStringLiteral("sequence_stop"))) {
        bool ok = false;
        const int number = normalized.toInt(&ok);
        valid = ok && number >= 0 && number <= 99999999;
    }
    if (!valid) {
        commandError_ = QStringLiteral("参数值无效: %1").arg(normalized);
        notify();
        return false;
    }

    parameter.value = normalized;
    if (selectedParameterIndex_ == 0) {
        parameterWriteEnabled_ = normalized == QStringLiteral("1");
    }
    saveParameter(selectedParameterIndex_);
    commandError_.clear();
    commandLine_ = QStringLiteral("PARAM %1=%2")
                       .arg(parameter.number)
                       .arg(parameter.value);
    notify();
    return true;
}

void MachineController::resetParameters()
{
    if (!parameterEditingAllowed()) {
        commandError_ =
            QStringLiteral("请选择 MDI 模式或急停状态后再修改参数");
        notify();
        return;
    }
    if (!parameterWriteEnabled_) {
        commandError_ = QStringLiteral("请先将“写参数”设为 1");
        notify();
        return;
    }

    parameterWriteEnabled_ = false;
    loadParameters();
    commandError_.clear();
    notify();
}

double MachineController::relativeX() const
{
    return machineX_ - relativeOriginMachineX_;
}

double MachineController::relativeZ() const
{
    return machineZ_ - relativeOriginMachineZ_;
}

int MachineController::activeWorkOffset() const
{
    return activeWorkOffset_;
}

const QVector<WorkOffset> &MachineController::workOffsets() const
{
    return workOffsets_;
}

void MachineController::setActiveWorkOffset(int number)
{
    setWorkOffsetState(number);
    CoordinateState state;
    state.activeWorkOffset = activeWorkOffset_;
    state.relativeOriginMachineX = relativeOriginMachineX_;
    state.relativeOriginMachineZ = relativeOriginMachineZ_;
    database_->saveCoordinateState(state);
    saveMachineState();
    notify();
}

void MachineController::setActiveWorkOffsetOrigin(double machineX, double machineZ)
{
    for (WorkOffset &offset : workOffsets_) {
        if (offset.number != activeWorkOffset_) {
            continue;
        }
        offset.machineX = machineX;
        offset.machineZ = machineZ;
        database_->saveWorkOffset(offset);
        updateAbsoluteFromMachine();
        notify();
        return;
    }
}

void MachineController::clearRelativeCoordinate(const QString &axis)
{
    const QString normalized = axis.trimmed().toUpper();
    if (normalized == QStringLiteral("X") || normalized == QStringLiteral("ALL")) {
        relativeOriginMachineX_ = machineX_;
    }
    if (normalized == QStringLiteral("Z") || normalized == QStringLiteral("ALL")) {
        relativeOriginMachineZ_ = machineZ_;
    }
    CoordinateState state;
    state.activeWorkOffset = activeWorkOffset_;
    state.relativeOriginMachineX = relativeOriginMachineX_;
    state.relativeOriginMachineZ = relativeOriginMachineZ_;
    database_->saveCoordinateState(state);
    notify();
}

void MachineController::startProgram()
{
    if (stateText_ == QStringLiteral("急停中")) {
        return;
    }
    if (running_) {
        notify();
        return;
    }
    const bool resume = pausedByProgram_
                        || pausedBySingleBlock_
                        || stateText_ == QStringLiteral("暂停中");
    if (!resume) {
        parseCurrentProgram();
        instructionIndex_ = 0;
        instructionProgress_ = 0.0;
        lastAppliedInstruction_ = -1;
        currentLine_ = 0;
    }
    if (programInstructions_.isEmpty()) {
        stateText_ = QStringLiteral("没有可执行程序");
        stateColor_ = QColor(180, 125, 10);
        notify();
        return;
    }
    pausedByProgram_ = false;
    pausedBySingleBlock_ = false;
    running_ = true;
    stateText_ = QStringLiteral("运行中");
    stateColor_ = QColor(30, 120, 70);
    timer_->start();
    notify();
}

void MachineController::pauseProgram()
{
    pausedByProgram_ = false;
    pausedBySingleBlock_ = false;
    running_ = false;
    timer_->stop();
    stateText_ = QStringLiteral("暂停中");
    stateColor_ = QColor(180, 125, 10);
    modalState_.highPressurePumpOn = false;
    plc_.syncState(modalState_);
    saveMachineState();
    notify();
}

void MachineController::emergencyStop()
{
    pausedByProgram_ = false;
    pausedBySingleBlock_ = false;
    running_ = false;
    timer_->stop();
    stateText_ = QStringLiteral("急停中");
    stateColor_ = QColor(190, 42, 38);
    modalState_.highPressurePumpOn = false;
    plc_.reset();
    saveMachineState();
    notify();
}

void MachineController::resetProgram()
{
    const bool editMode = modeCode_ == QStringLiteral("EDIT");

    pausedByProgram_ = false;
    pausedBySingleBlock_ = false;
    running_ = false;
    timer_->stop();
    plc_.reset();
    waitingForPlc_ = false;
    modalState_.spindleDirection = SpindleDirection::Stopped;
    modalState_.coolantOn = false;
    modalState_.highPressurePumpOn = false;
    modalState_.programStopRequested = false;
    modalState_.programEnded = false;
    lastMCode_ = QStringLiteral("M05");

    if (editMode) {
        currentLine_ = 0;
        selectedProgramLine_ = 0;
        selectedProgramColumn_ = 0;
        selectedProgramLength_ = 0;
        parseCurrentProgram();
        instructionIndex_ = 0;
        instructionProgress_ = 0.0;
        lastAppliedInstruction_ = -1;
        modalState_.spindleDirection = SpindleDirection::Stopped;
        modalState_.coolantOn = false;
        modalState_.highPressurePumpOn = false;
    }

    modalState_.lastMCode = 5;
    plc_.syncState(modalState_);
    runSeconds_ = 0;
    cycleSeconds_ = 0;
    stateText_ = editMode ? QStringLiteral("就绪")
                          : QStringLiteral("复位停止");
    stateColor_ = QColor(30, 120, 70);
    parameterWriteEnabled_ = false;
    if (!parameters_.isEmpty()) {
        parameters_[0].value = QStringLiteral("0");
    }
    saveMachineState();
    notify();
}

void MachineController::executeCommand(const QString &command)
{
    const QString original = command.trimmed();
    const QString normalized = original.toUpper();
    if (normalized.isEmpty()) {
        return;
    }

    commandLine_ = normalized;
    commandError_.clear();
    const QStringList tokens = splitCommand(normalized);

    if (!tokens.isEmpty() && tokens.first() == QStringLiteral("WORK")) {
        bool changed = false;
        double workX = machineX_;
        double workZ = machineZ_;
        for (int i = 1; i < tokens.size(); ++i) {
            const QString &part = tokens.at(i);
            bool ok = false;
            if (part.startsWith(QLatin1Char('X')) && part.size() > 1) {
                workX = part.mid(1).toDouble(&ok);
            } else if (part.startsWith(QLatin1Char('Z')) && part.size() > 1) {
                workZ = part.mid(1).toDouble(&ok);
            }
            if (!ok) {
                continue;
            }
            changed = true;
        }
        if (changed) {
            setActiveWorkOffsetOrigin(workX, workZ);
        } else {
            commandError_ = QStringLiteral("用法: WORK X10 Z20");
        }
    } else if (!tokens.isEmpty() && tokens.first() == QStringLiteral("REL")) {
        QString axis;
        if (tokens.size() >= 3
            && (tokens.at(2) == QStringLiteral("X")
                || tokens.at(2) == QStringLiteral("Z")
                || tokens.at(2) == QStringLiteral("ALL"))) {
            axis = tokens.at(2);
        }
        if (!axis.isEmpty()) {
            clearRelativeCoordinate(axis);
        } else {
            commandError_ = QStringLiteral("用法: REL 0 X/Z/ALL");
        }
    } else if (!tokens.isEmpty() && tokens.first() == QStringLiteral("CHECK")) {
        parseCurrentProgram();
        if (parseResult_.isValid()) {
            commandError_.clear();
        } else {
            const GCodeParseError &error = parseResult_.errors.first();
            commandError_ = QStringLiteral("程序语法错误 L%1 C%2: %3")
                                .arg(error.line)
                                .arg(error.column)
                                .arg(error.message);
        }
    } else if (!tokens.isEmpty() && tokens.first() == QStringLiteral("RESET")) {
        resetProgram();
        rememberCommand(normalized);
        return;
    } else if (!tokens.isEmpty()
        && (tokens.first() == QStringLiteral("MEASURE")
            || tokens.first() == QStringLiteral("SET"))) {
        if (tokens.size() < 2) {
            commandError_ = QStringLiteral("用法: MEASURE X49.5 Z0");
        } else {
            bool measured = false;
            ToolOffset &tool = tools_[currentToolIndex_];
            const QVector3D workDelta = workOffsetDelta();
            for (int i = 1; i < tokens.size(); ++i) {
                const QString &part = tokens.at(i);
                bool ok = false;
                if (part.startsWith(QLatin1Char('X')) && part.size() > 1) {
                    const double value = part.mid(1).toDouble(&ok);
                    if (ok) {
                        tool.touchMachineX = machineX_;
                        tool.absoluteX = value + workDelta.x();
                        measured = true;
                    }
                } else if (part.startsWith(QLatin1Char('Z')) && part.size() > 1) {
                    const double value = part.mid(1).toDouble(&ok);
                    if (ok) {
                        tool.touchMachineZ = machineZ_;
                        tool.absoluteZ = value + workDelta.z();
                        measured = true;
                    }
                }
                if (!ok && part.size() > 1
                    && (part.startsWith(QLatin1Char('X'))
                        || part.startsWith(QLatin1Char('Z')))) {
                    commandError_ = QStringLiteral("测量值格式错误: %1").arg(part);
                    break;
                }
            }
            if (commandError_.isEmpty() && measured) {
                saveTool(currentToolIndex_);
                updateAbsoluteFromMachine();
            } else if (commandError_.isEmpty()) {
                commandError_ = QStringLiteral("没有可保存的测量轴");
            }
        }
    } else if (!tokens.isEmpty() && tokens.first() == QStringLiteral("SAVE")) {
        if (tokens.size() < 2) {
            commandError_ = QStringLiteral("用法: SAVE O1001 [名称]");
        } else {
            const int number = parseProgramNumber(tokens.at(1));
            const QString name = tokens.size() > 2
                ? tokens.mid(2).join(QLatin1Char(' '))
                : QString();
            saveCurrentProgram(number, name);
        }
    } else if (!tokens.isEmpty() && tokens.first() == QStringLiteral("LOAD")) {
        if (tokens.size() < 2) {
            commandError_ = QStringLiteral("用法: LOAD O1001");
        } else {
            loadProgram(parseProgramNumber(tokens.at(1)));
        }
    } else if (!tokens.isEmpty()
               && (tokens.first() == QStringLiteral("DEL")
                   || tokens.first() == QStringLiteral("DELETE"))) {
        if (tokens.size() < 2) {
            commandError_ = QStringLiteral("用法: DEL O1001");
        } else {
            deleteProgram(parseProgramNumber(tokens.at(1)));
        }
    } else if (!tokens.isEmpty() && tokens.first() == QStringLiteral("PATH")) {
        const int firstSpace = original.indexOf(QLatin1Char(' '));
        if (firstSpace < 0 || !database_->setProgramDirectory(original.mid(firstSpace + 1))) {
            commandError_ = database_->lastError().isEmpty()
                ? QStringLiteral("程序目录不能为空")
                : database_->lastError();
        }
    } else if (!tokens.isEmpty() && tokens.first() == QStringLiteral("DIR")) {
        if (pageRequestCallback_) {
            pageRequestCallback_(QStringLiteral("DIR"));
        }
    } else {
        InputMode mode = InputMode::Absolute;
        for (const QString &part : tokens) {
            bool ok = false;

            if (part == QStringLiteral("MACHINE") || part == QStringLiteral("G53")) {
                mode = InputMode::Machine;
                continue;
            }
            if (part == QStringLiteral("GEO")) {
                mode = InputMode::Geometry;
                continue;
            }
            if (part == QStringLiteral("REF") || part == QStringLiteral("ABSREF")) {
                mode = InputMode::Reference;
                continue;
            }
            if (part == QStringLiteral("WEAR")) {
                mode = InputMode::Wear;
                continue;
            }

            if ((part.startsWith(QLatin1Char('X')) || part.startsWith(QLatin1Char('Z')))
                && part.size() > 1) {
                const double value = part.mid(1).toDouble(&ok);
                if (!ok) {
                    commandError_ = QStringLiteral("格式错误: %1").arg(part);
                    break;
                }

                const bool isX = part.startsWith(QLatin1Char('X'));
                switch (mode) {
                case InputMode::Machine:
                    setMachinePosition(isX ? value : machineX_,
                                       isX ? machineZ_ : value, true);
                    break;
                case InputMode::Geometry:
                    if (isX) {
                        tools_[currentToolIndex_].touchMachineX = value;
                    } else {
                        tools_[currentToolIndex_].touchMachineZ = value;
                    }
                    saveTool(currentToolIndex_);
                    updateAbsoluteFromMachine();
                    break;
                case InputMode::Reference: {
                    const QVector3D referenceWorkDelta = workOffsetDelta();
                    if (isX) {
                        tools_[currentToolIndex_].absoluteX =
                            value + referenceWorkDelta.x();
                    } else {
                        tools_[currentToolIndex_].absoluteZ =
                            value + referenceWorkDelta.z();
                    }
                    saveTool(currentToolIndex_);
                    updateAbsoluteFromMachine();
                    break;
                }
                case InputMode::Wear:
                    if (isX) {
                        tools_[currentToolIndex_].wearX = value;
                    } else {
                        tools_[currentToolIndex_].wearZ = value;
                    }
                    saveTool(currentToolIndex_);
                    updateAbsoluteFromMachine();
                    break;
                case InputMode::Absolute:
                    setAbsolutePosition(isX ? value : absoluteX_,
                                        isX ? absoluteZ_ : value, true);
                    break;
                }
            } else if (part.startsWith(QLatin1Char('F')) && part.size() > 1) {
                feed_ = part.mid(1).toDouble(&ok);
                if (!ok) {
                    commandError_ = QStringLiteral("进给格式错误: %1").arg(part);
                    break;
                }
            } else if (part.startsWith(QLatin1Char('S')) && part.size() > 1) {
                spindleSpeed_ = part.mid(1).toDouble(&ok);
                if (!ok) {
                    commandError_ = QStringLiteral("转速格式错误: %1").arg(part);
                    break;
                }
            } else if (part.startsWith(QLatin1Char('G')) && part.size() > 1) {
                lastGCode_ = part;
            } else if (part.startsWith(QLatin1Char('M')) && part.size() > 1) {
                lastMCode_ = part;
                if (part == QStringLiteral("M30")) {
                    running_ = false;
                    timer_->stop();
                    stateText_ = QStringLiteral("就绪");
                    stateColor_ = QColor(30, 120, 70);
                }
            } else if (part.startsWith(QLatin1Char('T')) && part.size() > 1) {
                const QString digits = part.mid(1);
                const int toolNumber = digits.left(2).toInt(&ok);
                if (!ok || toolNumber < 1 || toolNumber > 12) {
                    commandError_ = QStringLiteral("刀号范围应为 T01-T12");
                    break;
                }
                selectTool(toolNumber - 1, formatToolName(toolNumber));
                mode = InputMode::Absolute;
            }
        }
    }
    if (commandError_.isEmpty()) {
        applyMdiModalState(normalized);
        rememberCommand(normalized);
    }
    notify();
}

void MachineController::moveProgramCursor(int delta)
{
    if (!ensureProgramEditMode() || program_.isEmpty()) {
        return;
    }
    selectedProgramLine_ =
        qBound(0, selectedProgramLine_ + delta, program_.size() - 1);
    selectedProgramColumn_ = 0;
    selectedProgramLength_ = 0;
    currentLine_ = selectedProgramLine_;
    notify();
}

void MachineController::moveCharacterCursor(int delta)
{
    if (!ensureProgramEditMode() || program_.isEmpty()) {
        return;
    }
    const QString &line = program_.at(selectedProgramLine_);
    if (selectedProgramLength_ > 0) {
        selectedProgramColumn_ =
            delta > 0
                ? qMin(line.size(), selectedProgramColumn_ + selectedProgramLength_)
                : selectedProgramColumn_;
        selectedProgramLength_ = 0;
    } else {
        selectedProgramColumn_ =
            qBound(0, selectedProgramColumn_ + delta, line.size());
    }
    notify();
}

void MachineController::searchProgramText(const QString &queryText, bool backward)
{
    if (!ensureProgramEditMode() || program_.isEmpty()) {
        return;
    }
    const QString query = queryText.trimmed();
    if (query.isEmpty()) {
        commandError_ = QStringLiteral("请先输入查找内容");
        notify();
        return;
    }

    auto selectMatch = [this, &query](int lineIndex, int column) {
        selectedProgramLine_ = lineIndex;
        selectedProgramColumn_ = column;
        selectedProgramLength_ = query.size();
        currentLine_ = lineIndex;
        commandError_.clear();
        notify();
    };

    if (backward) {
        const QString &currentLine = program_.at(selectedProgramLine_);
        int start = selectedProgramColumn_ - 1;
        if (start >= currentLine.size()) {
            start = currentLine.size() - 1;
        }
        if (start >= 0) {
            const int match = currentLine.lastIndexOf(query, start, Qt::CaseInsensitive);
            if (match >= 0) {
                selectMatch(selectedProgramLine_, match);
                return;
            }
        }
        for (int lineIndex = selectedProgramLine_ - 1; lineIndex >= 0; --lineIndex) {
            const int match = program_.at(lineIndex).lastIndexOf(
                query, -1, Qt::CaseInsensitive);
            if (match >= 0) {
                selectMatch(lineIndex, match);
                return;
            }
        }
        for (int lineIndex = program_.size() - 1;
             lineIndex > selectedProgramLine_; --lineIndex) {
            const int match = program_.at(lineIndex).lastIndexOf(
                query, -1, Qt::CaseInsensitive);
            if (match >= 0) {
                selectMatch(lineIndex, match);
                return;
            }
        }
    } else {
        const QString &currentLine = program_.at(selectedProgramLine_);
        int start = selectedProgramColumn_ + selectedProgramLength_;
        if (start < 0) {
            start = 0;
        }
        if (start < currentLine.size()) {
            const int match = currentLine.indexOf(query, start, Qt::CaseInsensitive);
            if (match >= 0) {
                selectMatch(selectedProgramLine_, match);
                return;
            }
        }
        for (int lineIndex = selectedProgramLine_ + 1;
             lineIndex < program_.size(); ++lineIndex) {
            const int match = program_.at(lineIndex).indexOf(
                query, 0, Qt::CaseInsensitive);
            if (match >= 0) {
                selectMatch(lineIndex, match);
                return;
            }
        }
        for (int lineIndex = 0; lineIndex < selectedProgramLine_; ++lineIndex) {
            const int match = program_.at(lineIndex).indexOf(
                query, 0, Qt::CaseInsensitive);
            if (match >= 0) {
                selectMatch(lineIndex, match);
                return;
            }
        }
    }

    commandError_ = QStringLiteral("未找到: %1").arg(query);
    notify();
}

void MachineController::replaceSelectedProgramText(const QString &replacement)
{
    if (!ensureProgramEditMode()) {
        return;
    }
    if (selectedProgramLine_ < 0
        || selectedProgramLine_ >= program_.size()) {
        commandError_ = QStringLiteral("程序行选择无效");
        notify();
        return;
    }

    QString &line = program_[selectedProgramLine_];
    if (selectedProgramLength_ > 0) {
        if (selectedProgramColumn_ + selectedProgramLength_ > line.size()) {
            commandError_ = QStringLiteral("选中范围无效");
            notify();
            return;
        }
        line.replace(selectedProgramColumn_, selectedProgramLength_, replacement);
    } else if (selectedProgramColumn_ < line.size()) {
        line.replace(selectedProgramColumn_, 1, replacement);
    } else {
        line += replacement;
    }
    selectedProgramLength_ = replacement.size();
    currentLine_ = selectedProgramLine_;
    parseCurrentProgram();
    persistCurrentProgram();
    notify();
}

void MachineController::insertProgramText(const QString &text)
{
    if (!ensureProgramEditMode()
        || selectedProgramLine_ < 0
        || selectedProgramLine_ >= program_.size()) {
        return;
    }
    QString &line = program_[selectedProgramLine_];
    selectedProgramColumn_ =
        qBound(0, selectedProgramColumn_, line.size());
    line.insert(selectedProgramColumn_, text);
    selectedProgramLength_ = text.size();
    currentLine_ = selectedProgramLine_;
    parseCurrentProgram();
    persistCurrentProgram();
    notify();
}

void MachineController::deleteProgramText()
{
    if (!ensureProgramEditMode()
        || selectedProgramLine_ < 0
        || selectedProgramLine_ >= program_.size()) {
        return;
    }
    QString &line = program_[selectedProgramLine_];
    if (selectedProgramLength_ > 0
        && selectedProgramColumn_ + selectedProgramLength_ <= line.size()) {
        line.remove(selectedProgramColumn_, selectedProgramLength_);
        selectedProgramLength_ = 0;
    } else if (selectedProgramColumn_ < line.size()) {
        line.remove(selectedProgramColumn_, 1);
    } else if (selectedProgramColumn_ > 0 && !line.isEmpty()) {
        --selectedProgramColumn_;
        line.remove(selectedProgramColumn_, 1);
    }
    currentLine_ = selectedProgramLine_;
    parseCurrentProgram();
    persistCurrentProgram();
    notify();
}

bool MachineController::ensureProgramEditMode()
{
    if (running_) {
        commandError_ = QStringLiteral("程序运行中不能编辑");
        notify();
        return false;
    }
    if (modeCode_ != QStringLiteral("EDIT")) {
        commandError_ = QStringLiteral("请先切换到编辑模式");
        notify();
        return false;
    }
    return true;
}

void MachineController::persistCurrentProgram()
{
    ProgramRecord record;
    record.number = currentProgramNo_;
    record.name = programName_;
    record.lineCount = program_.size();
    record.updatedAt = QDateTime::currentDateTime()
                           .toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
    record.content = program_.join(QLatin1Char('\n'));
    if (!database_->saveProgram(record)) {
        commandError_ = database_->lastError();
    } else {
        refreshPrograms();
    }
}

void MachineController::loadProgram(int number)
{
    if (number < 1) {
        commandError_ = QStringLiteral("程序号无效");
        notify();
        return;
    }
    for (const ProgramRecord &record : programs_) {
        if (record.number != number) {
            continue;
        }
        programName_ = record.name;
        program_ = record.content.split(QLatin1Char('\n'));
        parseCurrentProgram();
        currentProgramNo_ = number;
        currentLine_ = 0;
        selectedProgramLine_ = 0;
        selectedProgramColumn_ = 0;
        selectedProgramLength_ = 0;
        if (parseResult_.isValid()) {
            commandError_.clear();
        } else {
            const GCodeParseError &error = parseResult_.errors.first();
            commandError_ = QStringLiteral("程序语法错误 L%1 C%2: %3")
                                .arg(error.line)
                                .arg(error.column)
                                .arg(error.message);
        }
        if (pageRequestCallback_) {
            pageRequestCallback_(QStringLiteral("PROG"));
        }
        notify();
        return;
    }
    commandError_ = QStringLiteral("找不到程序 O%1")
                        .arg(number, 4, 10, QLatin1Char('0'));
    notify();
}

void MachineController::saveCurrentProgram(int number, const QString &name)
{
    if (number < 1 || number > 9999) {
        commandError_ = QStringLiteral("程序号范围应为 O0001-O9999");
        notify();
        return;
    }

    ProgramRecord record;
    record.number = number;
    record.name = name.isEmpty()
        ? QStringLiteral("PROGRAM_%1").arg(number)
        : name;
    record.lineCount = program_.size();
    record.updatedAt = QDateTime::currentDateTime()
                           .toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
    record.content = program_.join(QLatin1Char('\n'));
    parseCurrentProgram();
    if (!database_->saveProgram(record)) {
        commandError_ = database_->lastError();
    } else {
        currentProgramNo_ = number;
        programName_ = record.name;
        refreshPrograms();
    }
    notify();
}

void MachineController::deleteProgram(int number)
{
    if (!database_->deleteProgram(number)) {
        commandError_ = database_->lastError();
    } else {
        refreshPrograms();
    }
    notify();
}

void MachineController::refreshPrograms()
{
    programs_ = database_->loadPrograms();
}

void MachineController::loadDefaultTools()
{
    tools_.clear();
    for (const ToolOffset &tool : kDefaultTools) {
        tools_.append(tool);
    }
}

void MachineController::ensureDefaultPrograms()
{
    const QString updatedAt = QDateTime::currentDateTime()
                                  .toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
    const QString legacyShaft = legacyShaftProgram();
    for (ProgramRecord record : samplePrograms()) {
        bool exists = false;
        bool replaceLegacy = false;
        for (const ProgramRecord &stored : programs_) {
            if (stored.number == record.number) {
                exists = true;
                replaceLegacy =
                    record.number == 1000
                    && stored.name == QStringLiteral("SHAFT DEMO")
                    && stored.content == legacyShaft;
                break;
            }
        }
        if (exists && !replaceLegacy) {
            continue;
        }

        record.updatedAt = updatedAt;
        if (!database_->saveProgram(record)) {
            databaseError_ = database_->lastError();
            continue;
        }
        if (!exists) {
            programs_.append(record);
        }
    }
    refreshPrograms();
}

void MachineController::loadParameters()
{
    parameters_ = {
        { 1, QStringLiteral("parameter_write"),
          QStringLiteral("写参数"), QStringLiteral("0"),
          QStringLiteral("0:不可以  1:可以"),
          { QStringLiteral("0"), QStringLiteral("1") } },
        { 2, QStringLiteral("tv_check"),
          QStringLiteral("TV 检查"), QStringLiteral("0"),
          QStringLiteral("0:关断  1:接通"),
          { QStringLiteral("0"), QStringLiteral("1") } },
        { 3, QStringLiteral("punch_code"),
          QStringLiteral("穿孔代码"), QStringLiteral("1"),
          QStringLiteral("0:EIA  1:ISO"),
          { QStringLiteral("0"), QStringLiteral("1") } },
        { 4, QStringLiteral("input_unit"),
          QStringLiteral("输入单位"), QStringLiteral("0"),
          QStringLiteral("0:毫米  1:英寸"),
          { QStringLiteral("0"), QStringLiteral("1") } },
        { 5, QStringLiteral("io_channel"),
          QStringLiteral("I/O 通道"), QStringLiteral("0"),
          QStringLiteral("0-35:通道号"),
          {} },
        { 6, QStringLiteral("sequence_number"),
          QStringLiteral("顺序号"), QStringLiteral("0"),
          QStringLiteral("0:关断  1:接通"),
          { QStringLiteral("0"), QStringLiteral("1") } },
        { 7, QStringLiteral("tape_format"),
          QStringLiteral("纸带格式"), QStringLiteral("0"),
          QStringLiteral("0:无变换  1:F15"),
          { QStringLiteral("0"), QStringLiteral("1") } },
        { 8, QStringLiteral("sequence_stop_program"),
          QStringLiteral("顺序号停止"), QStringLiteral("0"),
          QStringLiteral("程序号"),
          {} },
        { 9, QStringLiteral("sequence_stop_sequence"),
          QStringLiteral("顺序号停止"), QStringLiteral("0"),
          QStringLiteral("顺序号"),
          {} }
    };

    parameterWriteEnabled_ = false;
    if (!database_) {
        selectedParameterIndex_ = 0;
        return;
    }
    for (int index = 1; index < parameters_.size(); ++index) {
        MachineParameter &parameter = parameters_[index];
        parameter.value =
            database_->settingValue(parameter.key, parameter.value);
    }
    selectedParameterIndex_ =
        qBound(0, selectedParameterIndex_, parameters_.size() - 1);
}

void MachineController::saveParameter(int index)
{
    if (!database_ || index <= 0 || index >= parameters_.size()) {
        return;
    }
    const MachineParameter &parameter = parameters_.at(index);
    if (!database_->saveSettingValue(parameter.key, parameter.value)) {
        databaseError_ = database_->lastError();
    }
}

void MachineController::parseCurrentProgram()
{
    pausedByProgram_ = false;
    pausedBySingleBlock_ = false;
    parseResult_ = gcodeParser_.parseProgram(program_);
    modalMessages_.clear();
    modalStateByLine_.clear();
    programInstructions_.clear();
    motionPath_.clear();
    instructionIndex_ = 0;
    instructionProgress_ = 0.0;
    lastAppliedInstruction_ = -1;

    const QVector<GCodeBlock> expandedBlocks =
        macroEngine_.expandProgram(parseResult_.blocks, &modalMessages_);
    programInstructions_ = motionPlanner_.buildProgram(
        expandedBlocks, QVector3D(absoluteX_, 0.0, absoluteZ_),
        &modalMessages_, optionalStopSelected_);
    for (const ProgramInstruction &instruction : programInstructions_) {
        modalStateByLine_.insert(instruction.lineNumber, instruction.modalState);
        if (instruction.hasMotion) {
            motionPath_.append(instruction.segment);
        }
    }
    if (!programInstructions_.isEmpty()) {
        modalState_ = programInstructions_.last().modalState;
    } else {
        modalEngine_.reset();
        modalState_ = modalEngine_.state();
    }
    modalState_.highPressurePumpOn = false;
    syncLegacyStateFromModal();
}

void MachineController::applyMdiModalState(const QString &command)
{
    QVector<GCodeParseError> errors;
    const GCodeBlock block = gcodeParser_.parseBlock(command, 0, &errors);
    if (!errors.isEmpty()) {
        return;
    }
    const ModalUpdateResult update = modalEngine_.applyBlock(block);
    modalMessages_ += update.messages;
    modalState_ = modalEngine_.state();
    if (modalState_.workOffsetNo != activeWorkOffset_) {
        setWorkOffsetState(modalState_.workOffsetNo);
    }
    syncLegacyStateFromModal();
}

void MachineController::syncLegacyStateFromModal()
{
    lastGCode_ = modalState_.motionCode();
    lastMCode_ = modalState_.mCodeText();
    feed_ = modalState_.feed;
    spindleSpeed_ = modalState_.spindleSpeed;
    if (modalState_.toolNumber > 0) {
        toolName_ = modalState_.toolCode();
    }
}

void MachineController::updateAbsoluteFromMachine()
{
    updateCoordinateTransform();
    const QVector3D absolute =
        coordinateTransform_.machineToAbsolute(QVector3D(machineX_, 0.0, machineZ_));
    absoluteX_ = absolute.x();
    absoluteZ_ = absolute.z();
}

void MachineController::setMachinePosition(double x, double z, bool persist)
{
    machineX_ = x;
    machineZ_ = z;
    updateAbsoluteFromMachine();
    if (persist) {
        saveMachineState();
    }
    notify();
}

void MachineController::setAbsolutePosition(double x, double z, bool persist)
{
    updateCoordinateTransform();
    const QVector3D machine =
        coordinateTransform_.absoluteToMachine(QVector3D(x, 0.0, z));
    machineX_ = machine.x();
    machineZ_ = machine.z();
    updateAbsoluteFromMachine();
    if (persist) {
        saveMachineState();
    }
    notify();
}

void MachineController::selectTool(int index, const QString &toolName)
{
    if (index < 0 || index >= tools_.size()) {
        return;
    }
    currentToolIndex_ = index;
    toolName_ = toolName;
    updateAbsoluteFromMachine();
    saveMachineState();
    notify();
}

void MachineController::saveTool(int index)
{
    if (database_ && !database_->saveTool(index, tool(index))) {
        databaseError_ = database_->lastError();
    }
}

void MachineController::saveMachineState()
{
    if (database_
        && !database_->saveMachineState(machineX_, machineZ_, currentToolIndex_ + 1)) {
        databaseError_ = database_->lastError();
    }
}

void MachineController::tickProgram()
{
    if (!running_) {
        return;
    }
    if (waitingForPlc_) {
        plc_.scan();
        if (!plc_.mFin()) {
            notify();
            return;
        }
        waitingForPlc_ = false;
        plc_.clearRequest();
    }
    if (instructionIndex_ < 0
        || instructionIndex_ >= programInstructions_.size()) {
        finishProgram();
        return;
    }

    const ProgramInstruction &instruction =
        programInstructions_.at(instructionIndex_);
    currentLine_ = qMax(0, instruction.lineNumber - 1);

    if (lastAppliedInstruction_ != instructionIndex_) {
        applyInstructionState(instruction);
        lastAppliedInstruction_ = instructionIndex_;

        const int mCode = explicitMCode(instruction.block);
        if (mCode >= 0 && mCode != 0 && mCode != 1 && mCode != 30) {
            plc_.requestMCode(mCode);
            waitingForPlc_ = true;
            plc_.scan();
            if (plc_.mFin()) {
                waitingForPlc_ = false;
                plc_.clearRequest();
            } else {
                notify();
                return;
            }
        }
    }

    if (!instruction.hasMotion) {
        ++instructionIndex_;
        instructionProgress_ = 0.0;
        if (instruction.modalState.programEnded) {
            finishProgram();
            return;
        }
        if (instruction.modalState.programStopRequested) {
            pauseAtProgramStop(instruction.modalState.lastMCode == 0
                                   ? QStringLiteral("M00 程序暂停")
                                   : QStringLiteral("M01 选择停止"));
            return;
        }
        if (singleBlockSelected_
            && isSingleBlockBoundary(instruction)) {
            pauseForSingleBlock();
            return;
        }
        if (instructionIndex_ >= programInstructions_.size()) {
            finishProgram();
            return;
        }
        notify();
        return;
    }

    const MotionSegment &segment = instruction.segment;
    const double segmentLength = qMax(0.001, segment.length());
    double stepDistance = qMax(0.25, segmentLength / 70.0);
    if (segment.command == MotionCommand::Rapid) {
        stepDistance *= 2.5;
    }
    instructionProgress_ += stepDistance / segmentLength;

    const QVector3D position = segment.positionAt(instructionProgress_);
    setAbsolutePosition(position.x(), position.z(), false);

    ++tickCounter_;
    if (tickCounter_ % 10 == 0) {
        ++runSeconds_;
        ++cycleSeconds_;
    }

    if (instructionProgress_ >= 1.0) {
        setAbsolutePosition(segment.end.x(), segment.end.z(), false);
        ++instructionIndex_;
        instructionProgress_ = 0.0;
        lastAppliedInstruction_ = -1;
        if (instruction.modalState.programEnded) {
            finishProgram();
            return;
        }
        if (instruction.modalState.programStopRequested) {
            pauseAtProgramStop(instruction.modalState.lastMCode == 0
                                   ? QStringLiteral("M00 程序暂停")
                                   : QStringLiteral("M01 选择停止"));
            return;
        }
        if (singleBlockSelected_
            && isSingleBlockBoundary(instruction)) {
            pauseForSingleBlock();
            return;
        }
        if (instructionIndex_ >= programInstructions_.size()) {
            finishProgram();
            return;
        }
    }
    notify();
}

void MachineController::applyInstructionState(const ProgramInstruction &instruction)
{
    modalState_ = instruction.modalState;
    if (modalState_.workOffsetNo != activeWorkOffset_) {
        setWorkOffsetState(modalState_.workOffsetNo);
    }
    plc_.syncState(modalState_);
    feed_ = modalState_.feed;
    spindleSpeed_ = modalState_.spindleSpeed;
    lastGCode_ = modalState_.motionCode();
    lastMCode_ = modalState_.mCodeText();

    if (modalState_.toolNumber > 0
        && modalState_.toolNumber <= tools_.size()) {
        currentToolIndex_ = modalState_.toolNumber - 1;
        toolName_ = modalState_.toolCode();
        updateAbsoluteFromMachine();
    }
}

void MachineController::finishProgram()
{
    ++partCount_;
    currentLine_ = 0;
    instructionIndex_ = 0;
    instructionProgress_ = 0.0;
    lastAppliedInstruction_ = -1;
    pausedByProgram_ = false;
    pausedBySingleBlock_ = false;
    running_ = false;
    timer_->stop();
    stateText_ = QStringLiteral("就绪");
    stateColor_ = QColor(30, 120, 70);
    modalState_.highPressurePumpOn = false;
    plc_.syncState(modalState_);
    saveMachineState();
    notify();
}

void MachineController::pauseAtProgramStop(const QString &reason)
{
    running_ = false;
    pausedByProgram_ = true;
    pausedBySingleBlock_ = false;
    timer_->stop();
    stateText_ = reason;
    stateColor_ = QColor(180, 125, 10);
    modalState_.highPressurePumpOn = false;
    plc_.syncState(modalState_);
    saveMachineState();
    notify();
}

void MachineController::pauseForSingleBlock()
{
    running_ = false;
    pausedByProgram_ = false;
    pausedBySingleBlock_ = true;
    timer_->stop();
    stateText_ = QStringLiteral("暂停中");
    stateColor_ = QColor(180, 125, 10);
    modalState_.spindleDirection = SpindleDirection::Stopped;
    modalState_.coolantOn = false;
    modalState_.highPressurePumpOn = false;
    modalState_.lastMCode = 5;
    lastMCode_ = QStringLiteral("M05");
    plc_.reset();
    plc_.syncState(modalState_);
    saveMachineState();
    notify();
}

bool MachineController::isSingleBlockBoundary(
    const ProgramInstruction &instruction) const
{
    if (instruction.cycleStep == CycleStep::Cut
        || instruction.cycleStep == CycleStep::RetractX) {
        return true;
    }
    if (instruction.cycleId >= 0) {
        return false;
    }
    return instruction.cycleStep == CycleStep::None;
}

void MachineController::notify()
{
    if (changeCallback_) {
        changeCallback_();
    }
}

void MachineController::updateCoordinateTransform()
{
    const ToolOffset &currentTool = tool(currentToolIndex_);
    coordinateTransform_.setAxisScale(1.0, 1.0, 1.0);
    coordinateTransform_.setWorkOffsetDelta(workOffsetDelta());
    coordinateTransform_.setToolReference(
        QVector3D(currentTool.touchMachineX, 0.0, currentTool.touchMachineZ),
        QVector3D(currentTool.absoluteX, 0.0, currentTool.absoluteZ),
        QVector3D(currentTool.wearX, 0.0, currentTool.wearZ));
}

void MachineController::setWorkOffsetState(int number)
{
    activeWorkOffset_ = qBound(1, number, 6);
    updateAbsoluteFromMachine();
}

QVector3D MachineController::workOffsetDelta() const
{
    WorkOffset base;
    WorkOffset active;
    for (const WorkOffset &offset : workOffsets_) {
        if (offset.number == 1) {
            base = offset;
        }
        if (offset.number == activeWorkOffset_) {
            active = offset;
        }
    }
    return QVector3D(active.machineX - base.machineX,
                     0.0,
                     active.machineZ - base.machineZ);
}

void MachineController::rememberCommand(const QString &command)
{
    commandLog_.prepend(command);
    while (commandLog_.size() > 8) {
        commandLog_.removeLast();
    }
}
