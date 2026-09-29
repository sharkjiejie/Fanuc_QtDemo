#ifndef CORE_MACHINECONTROLLER_H
#define CORE_MACHINECONTROLLER_H

#include <QColor>
#include <QHash>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVector>

#include <functional>

#include "core/CoordinateTransform.h"
#include "core/GCodeModalEngine.h"
#include "core/GCodeParser.h"
#include "core/MacroEngine.h"
#include "core/MotionPlanner.h"
#include "core/PlcSimulator.h"
#include "model/ProgramRecord.h"
#include "model/MachineParameter.h"
#include "model/ToolOffset.h"
#include "model/WorkOffset.h"

class AppDatabase;
class QTimer;

class MachineController : public QObject
{
public:
    explicit MachineController(AppDatabase *database, QObject *parent = nullptr);

    bool initialize();

    void setChangeCallback(std::function<void()> callback);
    void setPageRequestCallback(std::function<void(const QString &)> callback);

    const QVector<ToolOffset> &tools() const;
    const ToolOffset &tool(int index) const;
    const QVector<ProgramRecord> &programRecords() const;
    const QStringList &program() const;
    const QVector<ProgramInstruction> &programInstructions() const;
    const QVector<MotionSegment> &motionPath() const;
    const QVector<MachineParameter> &parameters() const;
    const QStringList &commandLog() const;
    const QStringList &modalMessages() const;
    const GCodeParseResult &parseResult() const;
    const ModalState &modalState() const;
    ModalState modalStateForLine(int lineNumber) const;

    double machineX() const;
    double machineZ() const;
    double absoluteX() const;
    double absoluteZ() const;
    double relativeX() const;
    double relativeZ() const;
    int activeWorkOffset() const;
    const QVector<WorkOffset> &workOffsets() const;
    double feed() const;
    double spindleSpeed() const;

    int currentLine() const;
    int selectedProgramLine() const;
    int selectedProgramColumn() const;
    int selectedProgramLength() const;
    int selectedParameterIndex() const;
    int currentProgramNo() const;
    int currentToolIndex() const;
    int partCount() const;
    int runSeconds() const;
    int cycleSeconds() const;
    bool isRunning() const;

    QString programName() const;
    QString toolName() const;
    QString modeLine() const;
    QString modeCode() const;
    QString stateText() const;
    QString lastGCode() const;
    QString lastMCode() const;
    QString commandLine() const;
    QString commandError() const;
    QString databasePath() const;
    QString programDirectory() const;
    QString databaseError() const;
    QColor stateColor() const;

    void setModeCode(const QString &code);
    void setOptionalStopSelected(bool selected);
    bool optionalStopSelected() const;
    void setSingleBlockSelected(bool selected);
    bool singleBlockSelected() const;
    bool parameterPageActive() const;
    void setParameterPageActive(bool active);
    void moveParameterCursor(int delta);
    void selectParameter(int index);
    bool applyParameterInput(const QString &value);
    bool setParameterWriteEnabled(bool enabled);
    bool parameterWriteEnabled() const;
    bool parameterEditingAllowed() const;
    void resetParameters();
    void setActiveWorkOffset(int number);
    void setActiveWorkOffsetOrigin(double machineX, double machineZ);
    void clearRelativeCoordinate(const QString &axis);
    void startProgram();
    void pauseProgram();
    void emergencyStop();
    void resetProgram();
    void executeCommand(const QString &command);
    void moveProgramCursor(int delta);
    void moveCharacterCursor(int delta);
    void searchProgramText(const QString &query, bool backward);
    void replaceSelectedProgramText(const QString &replacement);
    void insertProgramText(const QString &text);
    void deleteProgramText();

    void loadProgram(int number);
    void saveCurrentProgram(int number, const QString &name);
    void deleteProgram(int number);
    void refreshPrograms();

private:
    enum class InputMode { Absolute, Machine, Geometry, Reference, Wear };

    void loadDefaultTools();
    void ensureDefaultPrograms();
    void loadParameters();
    void saveParameter(int index);
    void parseCurrentProgram();
    void applyMdiModalState(const QString &command);
    void syncLegacyStateFromModal();
    void applyInstructionState(const ProgramInstruction &instruction);
    void pauseAtProgramStop(const QString &reason);
    void pauseForSingleBlock();
    bool isSingleBlockBoundary(const ProgramInstruction &instruction) const;
    void finishProgram();
    void updateCoordinateTransform();
    void setWorkOffsetState(int number);
    QVector3D workOffsetDelta() const;
    void updateAbsoluteFromMachine();
    void setMachinePosition(double x, double z, bool persist);
    void setAbsolutePosition(double x, double z, bool persist);
    void selectTool(int index, const QString &toolName);
    void saveTool(int index);
    void saveMachineState();
    void tickProgram();
    void notify();
    void rememberCommand(const QString &command);
    bool ensureProgramEditMode();
    void persistCurrentProgram();

    AppDatabase *database_ = nullptr;
    QTimer *timer_ = nullptr;
    std::function<void()> changeCallback_;
    std::function<void(const QString &)> pageRequestCallback_;

    QVector<ToolOffset> tools_;
    QVector<ProgramRecord> programs_;
    QStringList program_;
    CoordinateTransform coordinateTransform_;
    GCodeParser gcodeParser_;
    MacroEngine macroEngine_;
    GCodeModalEngine modalEngine_;
    MotionPlanner motionPlanner_;
    PlcSimulator plc_;
    GCodeParseResult parseResult_;
    ModalState modalState_;
    QHash<int, ModalState> modalStateByLine_;
    QStringList modalMessages_;
    QVector<ProgramInstruction> programInstructions_;
    QVector<MotionSegment> motionPath_;
    QVector<WorkOffset> workOffsets_;
    QVector<MachineParameter> parameters_;

    double machineX_ = 99.5;
    double machineZ_ = -35.5;
    double absoluteX_ = 49.5;
    double absoluteZ_ = 24.5;
    double feed_ = 0.10;
    double spindleSpeed_ = 500.0;

    QString toolName_ = QStringLiteral("T0101");
    QString lastGCode_ = QStringLiteral("G01");
    QString lastMCode_ = QStringLiteral("M05");
    QString modeLine_ = QStringLiteral("MEM STRT MTN ABS");
    QString modeCode_ = QStringLiteral("MEM");
    QString stateText_ = QStringLiteral("就绪");
    QColor stateColor_ = QColor(30, 120, 70);
    QString commandLine_;
    QString commandError_;
    QString databaseError_;
    QString programName_ = QStringLiteral("SHAFT DEMO");

    QStringList commandLog_;
    int currentLine_ = 0;
    int selectedProgramLine_ = 0;
    int selectedProgramColumn_ = 0;
    int selectedProgramLength_ = 0;
    int selectedParameterIndex_ = 0;
    int currentProgramNo_ = 1000;
    int currentToolIndex_ = 0;
    int partCount_ = 0;
    int runSeconds_ = 0;
    int cycleSeconds_ = 0;
    int tickCounter_ = 0;
    int instructionIndex_ = 0;
    int lastAppliedInstruction_ = -1;
    double instructionProgress_ = 0.0;
    bool optionalStopSelected_ = false;
    bool singleBlockSelected_ = false;
    bool parameterPageActive_ = false;
    bool parameterWriteEnabled_ = false;
    bool pausedByProgram_ = false;
    bool pausedBySingleBlock_ = false;
    bool waitingForPlc_ = false;
    int activeWorkOffset_ = 1;
    double relativeOriginMachineX_ = 0.0;
    double relativeOriginMachineZ_ = 0.0;
    bool running_ = false;
};

#endif
