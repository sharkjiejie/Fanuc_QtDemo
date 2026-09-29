#ifndef CORE_COORDINATETRANSFORM_H
#define CORE_COORDINATETRANSFORM_H

#include <QMatrix4x4>
#include <QVector3D>

class CoordinateTransform
{
public:
    CoordinateTransform();

    void setAxisScale(double xScale, double yScale, double zScale);
    void setToolReference(const QVector3D &touchMachine,
                          const QVector3D &referenceAbsolute,
                          const QVector3D &wear);
    void setWorkOffsetDelta(const QVector3D &machineDelta);

    QVector3D machineToAbsolute(const QVector3D &machinePosition) const;
    QVector3D absoluteToMachine(const QVector3D &absolutePosition) const;

    bool isValid() const;

private:
    void rebuild();

    QVector3D touchMachine_;
    QVector3D referenceAbsolute_;
    QVector3D wear_;
    QVector3D workOffsetDelta_;
    double xScale_ = 1.0;
    double yScale_ = 1.0;
    double zScale_ = 1.0;
    QMatrix4x4 machineToAbsolute_;
    QMatrix4x4 absoluteToMachine_;
    bool valid_ = false;
};

#endif
