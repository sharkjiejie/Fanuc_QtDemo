#include "core/MotionPlanner.h"

#include "core/GCodeModalEngine.h"

#include <QHash>
#include <QSet>
#include <QtMath>

namespace {

double unitScale(const ModalState &state)
{
    return state.units == UnitMode::Inch ? 25.4 : 1.0;
}

double resolvedCoordinate(const GCodeBlock &block, QChar letter,
                          double current, const ModalState &state,
                          bool *present)
{
    bool ok = false;
    const double value = block.wordValue(letter, &ok);
    if (!ok) {
        if (present) {
            *present = false;
        }
        return current;
    }
    if (present) {
        *present = true;
    }
    const double scaled = value * unitScale(state);
    return state.distance == DistanceMode::Absolute
        ? scaled
        : current + scaled;
}

bool hasGCode(const GCodeBlock &block, int code)
{
    for (const GCodeWord &word : block.words) {
        if (word.letter == QLatin1Char('G') && qRound(word.value) == code) {
            return true;
        }
    }
    return false;
}

bool hasCycleCode(const GCodeBlock &block)
{
    return hasGCode(block, 70) || hasGCode(block, 71)
           || hasGCode(block, 74) || hasGCode(block, 76);
}

double wordOr(const GCodeBlock &block, QChar letter, double fallback)
{
    bool ok = false;
    const double value = block.wordValue(letter, &ok);
    return ok ? value : fallback;
}

} // namespace

QVector3D MotionSegment::positionAt(double progress) const
{
    const double t = qBound(0.0, progress, 1.0);
    if (command == MotionCommand::ArcCw || command == MotionCommand::ArcCcw) {
        const double angle = arcStartAngle + arcSweep * t;
        return QVector3D(center.x() + radius * qCos(angle),
                         0.0,
                         center.z() + radius * qSin(angle));
    }
    return start + (end - start) * float(t);
}

double MotionSegment::length() const
{
    if (command == MotionCommand::ArcCw || command == MotionCommand::ArcCcw) {
        return qAbs(arcSweep) * radius;
    }
    return (end - start).length();
}

QVector<ProgramInstruction> MotionPlanner::buildProgram(
    const QVector<GCodeBlock> &blocks,
    const QVector3D &initialAbsolute,
    QStringList *errors,
    bool optionalStopSelected) const
{
    QVector<ProgramInstruction> instructions;
    GCodeModalEngine engine;
    engine.setOptionalStopSelected(optionalStopSelected);
    QVector3D position = initialAbsolute;
    QSet<int> profileOnlyBlocks;

    auto appendInstruction = [&instructions](const ProgramInstruction &instruction) {
        instructions.append(instruction);
    };

    auto makeSegmentInstruction = [](const GCodeBlock &source,
                                     const ModalState &state,
                                     const MotionSegment &segment) {
        ProgramInstruction instruction;
        instruction.block = source;
        instruction.modalState = state;
        instruction.segment = segment;
        instruction.lineNumber = source.lineNumber;
        instruction.hasMotion = segment.hasMotion;
        return instruction;
    };

    auto makeCycleInstruction = [&makeSegmentInstruction](
                                    const GCodeBlock &source,
                                    const ModalState &state,
                                    const MotionSegment &segment,
                                    CycleStep step,
                                    int cycleId,
                                    int passIndex) {
        ProgramInstruction instruction =
            makeSegmentInstruction(source, state, segment);
        instruction.cycleStep = step;
        instruction.cycleId = cycleId;
        instruction.cyclePass = passIndex;
        return instruction;
    };

    auto findSequenceIndex = [&blocks](int sequenceNumber) {
        for (int i = 0; i < blocks.size(); ++i) {
            if (blocks.at(i).sequenceNumber == sequenceNumber) {
                return i;
            }
        }
        return -1;
    };

    auto profileSegments = [&](int startIndex, int endIndex,
                               const ModalState &baseState,
                               const QVector3D &startPosition,
                               QStringList *profileErrors) {
        QVector<MotionSegment> segments;
        GCodeModalEngine profileEngine;
        profileEngine.setState(baseState);
        QVector3D profilePosition = startPosition;

        for (int i = startIndex; i <= endIndex; ++i) {
            const GCodeBlock &profileBlock = blocks.at(i);
            if (hasCycleCode(profileBlock)) {
                if (profileErrors) {
                    profileErrors->append(
                        QStringLiteral("L%1 暂不支持循环内嵌套循环")
                            .arg(profileBlock.lineNumber));
                }
                return segments;
            }
            profileEngine.applyBlock(profileBlock);
            QStringList errorsForBlock;
            const ProgramInstruction profileInstruction = planBlock(
                profileBlock, profileEngine.state(), profilePosition,
                &errorsForBlock);
            if (profileErrors) {
                *profileErrors += errorsForBlock;
            }
            if (profileInstruction.hasMotion) {
                segments.append(profileInstruction.segment);
                profilePosition = profileInstruction.segment.end;
            }
        }
        return segments;
    };

    for (int i = 0; i < blocks.size(); ++i) {
        const GCodeBlock &block = blocks.at(i);
        const ModalUpdateResult modalUpdate = engine.applyBlock(block);
        if (errors) {
            *errors += modalUpdate.messages;
        }
        const ModalState stateAfter = engine.state();

        if (profileOnlyBlocks.contains(i)) {
            continue;
        }

        if (!hasCycleCode(block)) {
            QStringList blockErrors;
            const ProgramInstruction instruction =
                planBlock(block, stateAfter, position, &blockErrors);
            if (errors) {
                *errors += blockErrors;
            }
            appendInstruction(instruction);
            if (instruction.hasMotion) {
                position = instruction.segment.end;
            }
            continue;
        }

        ProgramInstruction cycleInstruction;
        cycleInstruction.block = block;
        cycleInstruction.modalState = stateAfter;
        cycleInstruction.lineNumber = block.lineNumber;
        cycleInstruction.cycleId = block.lineNumber;
        appendInstruction(cycleInstruction);

        const bool hasP = block.hasWord(QLatin1Char('P'));
        const bool hasQ = block.hasWord(QLatin1Char('Q'));
        if (hasGCode(block, 70) || hasGCode(block, 71)) {
            if (!hasP || !hasQ) {
                continue;
            }
            const int startIndex = findSequenceIndex(qRound(
                block.wordValue(QLatin1Char('P'))));
            const int endIndex = findSequenceIndex(qRound(
                block.wordValue(QLatin1Char('Q'))));
            if (startIndex < 0 || endIndex < 0 || endIndex < startIndex) {
                if (errors) {
                    errors->append(
                        QStringLiteral("L%1 找不到 P/Q 轮廓段")
                            .arg(block.lineNumber));
                }
                continue;
            }

            QStringList profileErrors;
            QVector<MotionSegment> profile =
                profileSegments(startIndex, endIndex, stateAfter, position,
                                &profileErrors);
            if (errors) {
                *errors += profileErrors;
            }
            if (profile.isEmpty()) {
                continue;
            }

            if (hasGCode(block, 71)) {
                for (int profileIndex = startIndex;
                     profileIndex <= endIndex; ++profileIndex) {
                    profileOnlyBlocks.insert(profileIndex);
                }

                double depthPerSide = 2.0;
                double retractPerSide = 1.0;
                const double scale = unitScale(stateAfter);
                for (int previous = i - 1; previous >= 0; --previous) {
                    if (hasGCode(blocks.at(previous), 71)) {
                        depthPerSide =
                            qAbs(wordOr(blocks.at(previous),
                                        QLatin1Char('U'), 2.0))
                            * scale;
                        retractPerSide =
                            qAbs(wordOr(blocks.at(previous),
                                        QLatin1Char('R'), 1.0))
                            * scale;
                        break;
                    }
                }
                depthPerSide = qMax(0.1, depthPerSide);
                retractPerSide = qMax(0.1, retractPerSide);
                // X is diameter-programmed, so a radial U/R becomes 2U/2R
                // in displayed X coordinates.
                const double depthPerPassX = depthPerSide * 2.0;
                const double retractPerPassX = retractPerSide * 2.0;
                const double finishX =
                    wordOr(block, QLatin1Char('U'), 0.0) * scale;
                const double finishZ =
                    wordOr(block, QLatin1Char('W'), 0.0) * scale;
                const double feed = wordOr(block, QLatin1Char('F'),
                                           stateAfter.feed > 0.0
                                               ? stateAfter.feed
                                               : 0.2);

                double minX = position.x();
                double minZ = position.z();
                for (const MotionSegment &segment : profile) {
                    minX = qMin(minX, segment.start.x());
                    minX = qMin(minX, segment.end.x());
                    minZ = qMin(minZ, segment.start.z());
                    minZ = qMin(minZ, segment.end.z());
                }

                const double finalX = minX + finishX;
                const double finalZ = minZ + finishZ;
                const double cycleStartZ = position.z();
                double passX = position.x();
                int passIndex = 0;
                const int cycleId = block.lineNumber;
                while (passX > finalX + 0.0001 && passIndex < 200) {
                    const double nextX =
                        qMax(finalX, passX - depthPerPassX);
                    MotionSegment rapidToStart;
                    rapidToStart.command = MotionCommand::Rapid;
                    rapidToStart.start = position;
                    rapidToStart.end = QVector3D(nextX, 0.0, cycleStartZ);
                    rapidToStart.lineNumber = block.lineNumber;
                    rapidToStart.hasMotion = true;
                    appendInstruction(makeCycleInstruction(
                        block, stateAfter, rapidToStart,
                        CycleStep::Approach, cycleId, passIndex));
                    position = rapidToStart.end;

                    MotionSegment roughCut;
                    roughCut.command = MotionCommand::Linear;
                    roughCut.start = position;
                    roughCut.end = QVector3D(nextX, 0.0, finalZ);
                    roughCut.feed = feed;
                    roughCut.lineNumber = block.lineNumber;
                    roughCut.hasMotion = true;
                    appendInstruction(makeCycleInstruction(
                        block, stateAfter, roughCut,
                        CycleStep::Cut, cycleId, passIndex));
                    position = roughCut.end;

                    MotionSegment retractZ;
                    retractZ.command = MotionCommand::Rapid;
                    retractZ.start = position;
                    retractZ.end = QVector3D(nextX, 0.0, cycleStartZ);
                    retractZ.lineNumber = block.lineNumber;
                    retractZ.hasMotion = true;
                    appendInstruction(makeCycleInstruction(
                        block, stateAfter, retractZ,
                        CycleStep::RetractZ, cycleId, passIndex));
                    position = retractZ.end;

                    MotionSegment retractX;
                    retractX.command = MotionCommand::Rapid;
                    retractX.start = position;
                    retractX.end =
                        QVector3D(nextX + retractPerPassX, 0.0,
                                  cycleStartZ);
                    retractX.lineNumber = block.lineNumber;
                    retractX.hasMotion = true;
                    appendInstruction(makeCycleInstruction(
                        block, stateAfter, retractX,
                        CycleStep::RetractX, cycleId, passIndex));
                    position = retractX.end;
                    passX = nextX;
                    ++passIndex;
                }
            } else if (hasGCode(block, 70)) {
                const double finishFeed = wordOr(
                    block, QLatin1Char('F'),
                    stateAfter.feed > 0.0 ? stateAfter.feed : 0.15);
                for (MotionSegment segment : profile) {
                    segment.feed = finishFeed;
                    appendInstruction(makeSegmentInstruction(
                        block, stateAfter, segment));
                    position = segment.end;
                }
            }
            continue;
        }

        if (hasGCode(block, 74)) {
            if (!block.hasWord(QLatin1Char('X'))
                && !block.hasWord(QLatin1Char('Z'))) {
                continue;
            }
            if (!block.hasWord(QLatin1Char('Z'))) {
                if (errors) {
                    errors->append(QStringLiteral("L%1 G74 缺少 Z 深度")
                                       .arg(block.lineNumber));
                }
                continue;
            }
            const double targetX =
                block.hasWord(QLatin1Char('X'))
                    ? block.wordValue(QLatin1Char('X')) * unitScale(stateAfter)
                    : position.x();
            const double targetZ =
                block.wordValue(QLatin1Char('Z')) * unitScale(stateAfter);
            double peck = qAbs(wordOr(block, QLatin1Char('Q'),
                                      wordOr(block, QLatin1Char('P'), 5.0)))
                         * unitScale(stateAfter);
            peck = qMax(0.1, peck);
            double retract = 1.0;
            for (int previous = i - 1; previous >= 0; --previous) {
                if (hasGCode(blocks.at(previous), 74)) {
                    retract = qAbs(wordOr(blocks.at(previous),
                                          QLatin1Char('R'), 1.0))
                              * unitScale(stateAfter);
                    break;
                }
            }
            const double feed = wordOr(block, QLatin1Char('F'),
                                       stateAfter.feed > 0.0
                                           ? stateAfter.feed
                                           : 0.1);

            MotionSegment rapidToHole;
            rapidToHole.command = MotionCommand::Rapid;
            rapidToHole.start = position;
            rapidToHole.end = QVector3D(targetX, 0.0, position.z());
            rapidToHole.lineNumber = block.lineNumber;
            rapidToHole.hasMotion = true;
            appendInstruction(makeSegmentInstruction(
                block, stateAfter, rapidToHole));
            position = rapidToHole.end;

            double currentZ = position.z();
            int peckCount = 0;
            while (currentZ > targetZ + 0.0001 && peckCount < 200) {
                const double nextZ = qMax(targetZ, currentZ - peck);
                MotionSegment cut;
                cut.command = MotionCommand::Linear;
                cut.start = position;
                cut.end = QVector3D(targetX, 0.0, nextZ);
                cut.feed = feed;
                cut.lineNumber = block.lineNumber;
                cut.hasMotion = true;
                appendInstruction(makeSegmentInstruction(
                    block, stateAfter, cut));
                position = cut.end;

                MotionSegment retractCut;
                retractCut.command = MotionCommand::Rapid;
                retractCut.start = position;
                retractCut.end = QVector3D(targetX, 0.0, nextZ + retract);
                retractCut.lineNumber = block.lineNumber;
                retractCut.hasMotion = true;
                appendInstruction(makeSegmentInstruction(
                    block, stateAfter, retractCut));
                position = retractCut.end;

                if (nextZ > targetZ) {
                    MotionSegment rapidBack;
                    rapidBack.command = MotionCommand::Rapid;
                    rapidBack.start = position;
                    rapidBack.end = QVector3D(targetX, 0.0, nextZ);
                    rapidBack.lineNumber = block.lineNumber;
                    rapidBack.hasMotion = true;
                    appendInstruction(makeSegmentInstruction(
                        block, stateAfter, rapidBack));
                    position = rapidBack.end;
                }
                currentZ = nextZ;
                ++peckCount;
            }
            continue;
        }

        if (hasGCode(block, 76)) {
            if (!block.hasWord(QLatin1Char('X'))
                && !block.hasWord(QLatin1Char('Z'))
                && !block.hasWord(QLatin1Char('F'))) {
                continue;
            }
            if (!block.hasWord(QLatin1Char('Z'))) {
                if (errors) {
                    errors->append(QStringLiteral("L%1 G76 缺少 Z 终点")
                                       .arg(block.lineNumber));
                }
                continue;
            }
            const double targetX =
                block.hasWord(QLatin1Char('X'))
                    ? block.wordValue(QLatin1Char('X')) * unitScale(stateAfter)
                    : position.x();
            const double targetZ =
                block.wordValue(QLatin1Char('Z')) * unitScale(stateAfter);
            const double pitch =
                wordOr(block, QLatin1Char('F'), 1.0) * unitScale(stateAfter);
            if (pitch <= 0.0) {
                if (errors) {
                    errors->append(QStringLiteral("L%1 G76 螺距无效")
                                       .arg(block.lineNumber));
                }
                continue;
            }
            const int passes = 6;
            const double startX = position.x();
            const double startZ = position.z();
            for (int pass = 1; pass <= passes; ++pass) {
                const double x = startX
                                 + (targetX - startX) * pass / passes;
                MotionSegment threading;
                threading.command = MotionCommand::Linear;
                threading.start = QVector3D(x, 0.0, startZ);
                threading.end = QVector3D(x, 0.0, targetZ);
                threading.feed = pitch;
                threading.lineNumber = block.lineNumber;
                threading.hasMotion = true;
                appendInstruction(makeSegmentInstruction(
                    block, stateAfter, threading));

                MotionSegment retract;
                retract.command = MotionCommand::Rapid;
                retract.start = threading.end;
                retract.end = QVector3D(targetX + 2.0, 0.0, startZ);
                retract.lineNumber = block.lineNumber;
                retract.hasMotion = true;
                appendInstruction(makeSegmentInstruction(
                    block, stateAfter, retract));
                position = retract.end;
            }
            if (errors) {
                errors->append(
                    QStringLiteral("L%1 G76 当前使用简化六刀螺纹展开")
                        .arg(block.lineNumber));
            }
        }
    }

    return instructions;
}

ProgramInstruction MotionPlanner::planBlock(const GCodeBlock &block,
                                            const ModalState &modalState,
                                            const QVector3D &currentAbsolute,
                                            QStringList *errors) const
{
    ProgramInstruction instruction;
    instruction.block = block;
    instruction.modalState = modalState;
    instruction.lineNumber = block.lineNumber;

    bool hasX = false;
    bool hasY = false;
    bool hasZ = false;
    bool hasI = false;
    bool hasK = false;
    bool hasR = false;
    bool hasCoordinate = block.hasWord(QLatin1Char('X'))
                         || block.hasWord(QLatin1Char('Y'))
                         || block.hasWord(QLatin1Char('Z'));

    QVector3D target = currentAbsolute;
    target.setX(resolvedCoordinate(block, QLatin1Char('X'), currentAbsolute.x(),
                                   modalState, &hasX));
    target.setY(resolvedCoordinate(block, QLatin1Char('Y'), currentAbsolute.y(),
                                   modalState, &hasY));
    target.setZ(resolvedCoordinate(block, QLatin1Char('Z'), currentAbsolute.z(),
                                   modalState, &hasZ));

    if (!hasCoordinate) {
        return instruction;
    }

    MotionSegment segment;
    segment.start = currentAbsolute;
    segment.end = target;
    segment.feed = modalState.feed;
    segment.lineNumber = block.lineNumber;
    segment.hasMotion = true;

    switch (modalState.motion) {
    case MotionMode::Rapid:
        segment.command = MotionCommand::Rapid;
        break;
    case MotionMode::Linear:
        segment.command = MotionCommand::Linear;
        break;
    case MotionMode::CircularCw:
        segment.command = MotionCommand::ArcCw;
        break;
    case MotionMode::CircularCcw:
        segment.command = MotionCommand::ArcCcw;
        break;
    }

    if (segment.command == MotionCommand::ArcCw
        || segment.command == MotionCommand::ArcCcw) {
        const double scale = unitScale(modalState);
        const double iValue = block.wordValue(QLatin1Char('I'), &hasI) * scale;
        const double kValue = block.wordValue(QLatin1Char('K'), &hasK) * scale;
        const double rValue = block.wordValue(QLatin1Char('R'), &hasR) * scale;

        if (hasI || hasK) {
            segment.center = QVector3D(segment.start.x() + (hasI ? iValue : 0.0),
                                       0.0,
                                       segment.start.z() + (hasK ? kValue : 0.0));
            segment.radius = (segment.start - segment.center).length();
        } else if (hasR) {
            const double chordX = segment.end.x() - segment.start.x();
            const double chordZ = segment.end.z() - segment.start.z();
            const double chord = qSqrt(chordX * chordX + chordZ * chordZ);
            const double radius = qAbs(rValue);
            if (chord < 0.000001 || radius < chord / 2.0 - 0.000001) {
                if (errors) {
                    errors->append(QStringLiteral("L%1 圆弧半径或终点无效")
                                       .arg(block.lineNumber));
                }
                segment.hasMotion = false;
                instruction.segment = segment;
                return instruction;
            }
            const double height = qSqrt(qMax(0.0, radius * radius - chord * chord / 4.0));
            const QVector3D midpoint = (segment.start + segment.end) / 2.0f;
            double direction = (segment.command == MotionCommand::ArcCw) ? -1.0 : 1.0;
            if (rValue < 0.0) {
                direction *= -1.0;
            }
            segment.center = QVector3D(
                midpoint.x() - direction * height * chordZ / chord,
                0.0,
                midpoint.z() + direction * height * chordX / chord);
            segment.radius = radius;
        } else {
            if (errors) {
                errors->append(QStringLiteral("L%1 圆弧缺少 I/K 或 R")
                                   .arg(block.lineNumber));
            }
            segment.hasMotion = false;
            instruction.segment = segment;
            return instruction;
        }

        const double startX = segment.start.x() - segment.center.x();
        const double startZ = segment.start.z() - segment.center.z();
        const double endX = segment.end.x() - segment.center.x();
        const double endZ = segment.end.z() - segment.center.z();
        segment.arcStartAngle = qAtan2(startZ, startX);
        const double endAngle = qAtan2(endZ, endX);
        if (segment.command == MotionCommand::ArcCw) {
            segment.arcSweep = endAngle - segment.arcStartAngle;
            const double twoPi = 2.0 * qAcos(-1.0);
            while (segment.arcSweep >= 0.0) {
                segment.arcSweep -= twoPi;
            }
        } else {
            segment.arcSweep = endAngle - segment.arcStartAngle;
            const double twoPi = 2.0 * qAcos(-1.0);
            while (segment.arcSweep <= 0.0) {
                segment.arcSweep += twoPi;
            }
        }
    }

    instruction.segment = segment;
    instruction.hasMotion = segment.hasMotion;
    Q_UNUSED(hasX);
    Q_UNUSED(hasY);
    Q_UNUSED(hasZ);
    return instruction;
}
