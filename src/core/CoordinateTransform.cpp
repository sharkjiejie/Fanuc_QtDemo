#include "core/CoordinateTransform.h"

CoordinateTransform::CoordinateTransform()
{
    rebuild();
}

void CoordinateTransform::setAxisScale(double xScale, double yScale, double zScale)
{
    xScale_ = xScale;
    yScale_ = yScale;
    zScale_ = zScale;
    rebuild();
}

void CoordinateTransform::setToolReference(const QVector3D &touchMachine,
                                           const QVector3D &referenceAbsolute,
                                           const QVector3D &wear)
{
    touchMachine_ = touchMachine;
    referenceAbsolute_ = referenceAbsolute;
    wear_ = wear;
    rebuild();
}

void CoordinateTransform::setWorkOffsetDelta(const QVector3D &machineDelta)
{
    workOffsetDelta_ = machineDelta;
    rebuild();
}

QVector3D CoordinateTransform::machineToAbsolute(const QVector3D &machinePosition) const
{
    return machineToAbsolute_.map(machinePosition);
}

QVector3D CoordinateTransform::absoluteToMachine(const QVector3D &absolutePosition) const
{
    return absoluteToMachine_.map(absolutePosition);
}

bool CoordinateTransform::isValid() const
{
    return valid_;
}

void CoordinateTransform::rebuild()
{
    machineToAbsolute_.setToIdentity();

    // ABS = reference + scale * (MACHINE - touch) + wear
    const QVector3D scaledWorkOffset(
        xScale_ * workOffsetDelta_.x(),
        yScale_ * workOffsetDelta_.y(),
        zScale_ * workOffsetDelta_.z());
    machineToAbsolute_.translate(referenceAbsolute_ + wear_ - scaledWorkOffset);
    machineToAbsolute_.scale(float(xScale_), float(yScale_), float(zScale_));
    machineToAbsolute_.translate(-touchMachine_);

    bool invertible = false;
    absoluteToMachine_ = machineToAbsolute_.inverted(&invertible);
    valid_ = invertible;
}
