#ifndef MODEL_MODALSTATE_H
#define MODEL_MODALSTATE_H

#include <QString>

enum class MotionMode
{
    Rapid,
    Linear,
    CircularCw,
    CircularCcw
};

enum class UnitMode
{
    Millimeter,
    Inch
};

enum class DistanceMode
{
    Absolute,
    Incremental
};

enum class FeedMode
{
    PerMinute,
    PerRevolution
};

enum class SpindleMode
{
    ConstantSurfaceSpeed,
    ConstantRpm
};

enum class SpindleDirection
{
    Stopped,
    Forward,
    Reverse
};

struct ModalState
{
    MotionMode motion = MotionMode::Rapid;
    UnitMode units = UnitMode::Millimeter;
    DistanceMode distance = DistanceMode::Absolute;
    FeedMode feedMode = FeedMode::PerMinute;
    SpindleMode spindleMode = SpindleMode::ConstantRpm;
    SpindleDirection spindleDirection = SpindleDirection::Stopped;

    double feed = 0.0;
    double spindleSpeed = 0.0;
    int toolNumber = 0;
    int toolOffset = 0;
    int workOffsetNo = 1;
    int lastMCode = -1;
    bool coolantOn = false;
    bool highPressurePumpOn = false;
    bool optionalStopSelected = false;
    bool programStopRequested = false;
    bool programEnded = false;

    QString motionCode() const;
    QString unitCode() const;
    QString distanceCode() const;
    QString feedModeCode() const;
    QString spindleModeCode() const;
    QString toolCode() const;
    QString mCodeText() const;
    QString gCodeSummary() const;
    QString stateSummary() const;
};

#endif
