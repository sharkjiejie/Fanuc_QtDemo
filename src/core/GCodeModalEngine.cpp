#include "core/GCodeModalEngine.h"

#include <QtMath>

namespace {

bool isKnownNonModalGCode(int code)
{
    switch (code) {
    case 28:
    case 40:
    case 50:
    case 54:
    case 55:
    case 56:
    case 57:
    case 58:
    case 59:
    case 70:
    case 71:
    case 72:
    case 73:
    case 74:
    case 75:
    case 76:
    case 80:
    case 83:
    case 84:
    case 85:
    case 86:
    case 87:
    case 88:
    case 89:
    case 92:
    case 98:
    case 99:
        return true;
    default:
        return false;
    }
}

} // namespace

QString ModalState::motionCode() const
{
    switch (motion) {
    case MotionMode::Rapid: return QStringLiteral("G00");
    case MotionMode::Linear: return QStringLiteral("G01");
    case MotionMode::CircularCw: return QStringLiteral("G02");
    case MotionMode::CircularCcw: return QStringLiteral("G03");
    }
    return QStringLiteral("G00");
}

QString ModalState::unitCode() const
{
    return units == UnitMode::Millimeter
        ? QStringLiteral("G21")
        : QStringLiteral("G20");
}

QString ModalState::distanceCode() const
{
    return distance == DistanceMode::Absolute
        ? QStringLiteral("G90")
        : QStringLiteral("G91");
}

QString ModalState::feedModeCode() const
{
    return feedMode == FeedMode::PerMinute
        ? QStringLiteral("G94")
        : QStringLiteral("G95");
}

QString ModalState::spindleModeCode() const
{
    return spindleMode == SpindleMode::ConstantSurfaceSpeed
        ? QStringLiteral("G96")
        : QStringLiteral("G97");
}

QString ModalState::toolCode() const
{
    if (toolNumber <= 0) {
        return QStringLiteral("T----");
    }
    return QStringLiteral("T%1%2")
        .arg(toolNumber, 2, 10, QLatin1Char('0'))
        .arg(toolOffset, 2, 10, QLatin1Char('0'));
}

QString ModalState::mCodeText() const
{
    if (lastMCode < 0) {
        return QStringLiteral("M--");
    }
    return QStringLiteral("M%1").arg(lastMCode, 2, 10, QLatin1Char('0'));
}

QString ModalState::gCodeSummary() const
{
    return QStringLiteral("%1 %2 %3 %4 %5 G%6")
        .arg(motionCode(), unitCode(), distanceCode(),
             feedModeCode(), spindleModeCode())
        .arg(53 + workOffsetNo);
}

QString ModalState::stateSummary() const
{
    QString summary = QStringLiteral("F %1  S %2  %3  %4")
        .arg(QString::number(feed, 'f', 2),
             QString::number(spindleSpeed, 'f', 0),
             toolCode(),
             mCodeText());
    if (coolantOn) {
        summary += QStringLiteral("  CL");
    }
    if (highPressurePumpOn) {
        summary += QStringLiteral("  HP");
    }
    return summary;
}

GCodeModalEngine::GCodeModalEngine()
{
    reset();
}

void GCodeModalEngine::reset()
{
    const bool optionalStopSelected = state_.optionalStopSelected;
    state_ = ModalState();
    state_.optionalStopSelected = optionalStopSelected;
}

void GCodeModalEngine::setState(const ModalState &state)
{
    state_ = state;
}

void GCodeModalEngine::setOptionalStopSelected(bool selected)
{
    state_.optionalStopSelected = selected;
}

ModalUpdateResult GCodeModalEngine::applyBlock(const GCodeBlock &block)
{
    ModalUpdateResult result;
    state_.programStopRequested = false;
    state_.programEnded = false;
    state_.highPressurePumpOn = false;
    for (const GCodeWord &word : block.words) {
        if (word.letter == QLatin1Char('G')) {
            const int code = qRound(word.value);
            switch (code) {
            case 0: state_.motion = MotionMode::Rapid; break;
            case 1: state_.motion = MotionMode::Linear; break;
            case 2: state_.motion = MotionMode::CircularCw; break;
            case 3: state_.motion = MotionMode::CircularCcw; break;
            case 20: state_.units = UnitMode::Inch; break;
            case 21: state_.units = UnitMode::Millimeter; break;
            case 54:
            case 55:
            case 56:
            case 57:
            case 58:
            case 59:
                state_.workOffsetNo = code - 53;
                break;
            case 90: state_.distance = DistanceMode::Absolute; break;
            case 91: state_.distance = DistanceMode::Incremental; break;
            case 94: state_.feedMode = FeedMode::PerMinute; break;
            case 95: state_.feedMode = FeedMode::PerRevolution; break;
            case 96: state_.spindleMode = SpindleMode::ConstantSurfaceSpeed; break;
            case 97: state_.spindleMode = SpindleMode::ConstantRpm; break;
            default:
                if (!isKnownNonModalGCode(code)) {
                    result.messages.append(
                        QStringLiteral("L%1 暂未处理的 G 代码 G%2")
                            .arg(block.lineNumber)
                            .arg(code));
                }
                break;
            }
        } else if (word.letter == QLatin1Char('M')) {
            const int code = qRound(word.value);
            state_.lastMCode = code;
            switch (code) {
            case 0:
                state_.programStopRequested = true;
                result.messages.append(
                    QStringLiteral("L%1 M00 程序暂停")
                        .arg(block.lineNumber));
                break;
            case 1:
                if (state_.optionalStopSelected) {
                    state_.programStopRequested = true;
                    result.messages.append(
                        QStringLiteral("L%1 M01 选择停止生效")
                            .arg(block.lineNumber));
                } else {
                    result.messages.append(
                        QStringLiteral("L%1 M01 未选择，程序继续")
                            .arg(block.lineNumber));
                }
                break;
            case 3: state_.spindleDirection = SpindleDirection::Forward; break;
            case 4: state_.spindleDirection = SpindleDirection::Reverse; break;
            case 5: state_.spindleDirection = SpindleDirection::Stopped; break;
            case 8: state_.coolantOn = true; break;
            case 9: state_.coolantOn = false; break;
            case 28: state_.highPressurePumpOn = true; break;
            case 93:
                state_.spindleDirection = SpindleDirection::Forward;
                state_.coolantOn = true;
                break;
            case 30:
                state_.programEnded = true;
                state_.spindleDirection = SpindleDirection::Stopped;
                state_.coolantOn = false;
                state_.highPressurePumpOn = false;
                result.messages.append(
                    QStringLiteral("L%1 M30 程序结束").arg(block.lineNumber));
                break;
            default:
                result.messages.append(
                    QStringLiteral("L%1 暂未处理的 M 代码 M%2")
                        .arg(block.lineNumber)
                        .arg(code, 2, 10, QLatin1Char('0')));
                break;
            }
        } else if (word.letter == QLatin1Char('F')) {
            state_.feed = word.value;
        } else if (word.letter == QLatin1Char('S')) {
            state_.spindleSpeed = word.value;
        } else if (word.letter == QLatin1Char('T')) {
            const int raw = qRound(word.value);
            state_.toolNumber = raw >= 100 ? raw / 100 : raw;
            state_.toolOffset = raw >= 100 ? raw % 100 : 0;
        }
    }
    return result;
}

const ModalState &GCodeModalEngine::state() const
{
    return state_;
}
