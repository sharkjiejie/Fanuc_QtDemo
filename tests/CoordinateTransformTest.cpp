#include "core/CoordinateTransform.h"

#include <QtMath>

#include <iostream>

namespace {

int failures = 0;

void expect(bool condition, const char *message)
{
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

void expectNear(double actual, double expected, const char *message)
{
    expect(qAbs(actual - expected) < 0.00001, message);
}

} // namespace

int main()
{
    CoordinateTransform transform;
    transform.setAxisScale(1.0, 1.0, 1.0);
    transform.setToolReference(
        QVector3D(100.0, 0.0, -60.0),
        QVector3D(50.0, 0.0, 0.0),
        QVector3D(0.5, 0.0, -0.2));

    expect(transform.isValid(), "transform should be invertible");

    const QVector3D absolute =
        transform.machineToAbsolute(QVector3D(99.5, 0.0, -34.5));
    expectNear(absolute.x(), 50.0, "absolute X should include geometry and wear");
    expectNear(absolute.z(), 25.3, "absolute Z should include geometry and wear");

    const QVector3D machine =
        transform.absoluteToMachine(QVector3D(49.5, 0.0, 24.5));
    expectNear(machine.x(), 99.0, "inverse machine X should be correct");
    expectNear(machine.z(), -35.3, "inverse machine Z should be correct");

    const QVector3D roundTrip =
        transform.machineToAbsolute(transform.absoluteToMachine(
            QVector3D(12.5, 0.0, -8.25)));
    expectNear(roundTrip.x(), 12.5, "X round trip should match");
    expectNear(roundTrip.z(), -8.25, "Z round trip should match");

    transform.setWorkOffsetDelta(QVector3D(10.0, 0.0, -5.0));
    const QVector3D shifted =
        transform.machineToAbsolute(QVector3D(99.5, 0.0, -34.5));
    expectNear(shifted.x(), 40.0,
               "positive work-origin shift should reduce absolute X");
    expectNear(shifted.z(), 30.3,
               "negative work-origin shift should increase absolute Z");

    return failures == 0 ? 0 : 1;
}
