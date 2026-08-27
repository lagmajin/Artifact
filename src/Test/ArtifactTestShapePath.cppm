module;

#include <vector>

#include <QDebug>
#include <QPointF>
#include <QString>
#include <QtGlobal>

export module Artifact.Test.ShapePath;

import Artifact.Layer.Shape;

namespace Artifact {

namespace {
struct ShapePathTestReport {
    int failures = 0;

    void check(bool condition, const QString& label)
    {
        if (!condition) {
            ++failures;
            qWarning().noquote() << "[Shape Path Test][FAIL]" << label;
        } else {
            qInfo().noquote() << "[Shape Path Test][OK]" << label;
        }
    }
};
} // namespace

export int runShapePathTests()
{
    ShapePathTestReport report;
    ArtifactShapeLayer layer;

    const std::vector<CustomPathVertex> first {
        {QPointF(0.0, 0.0), QPointF(), QPointF(5.0, 0.0), false},
        {QPointF(20.0, 0.0), QPointF(-5.0, 0.0), QPointF(), false},
    };
    const std::vector<CustomPathVertex> second {
        {QPointF(10.0, 10.0), QPointF(), QPointF(5.0, 0.0), false},
        {QPointF(30.0, 10.0), QPointF(-5.0, 0.0), QPointF(), true},
    };

    layer.setPathKeyframe(0, first);
    report.check(layer.hasPathKeyframes(),
                 QStringLiteral("first path keyframe creates its property"));
    const auto atFirst = layer.evaluatePathAt(0);
    report.check(atFirst.size() == first.size() && atFirst.front().pos == first.front().pos,
                 QStringLiteral("first path keyframe is evaluated exactly"));

    layer.setPathKeyframe(10, second);
    const auto halfway = layer.evaluatePathAt(5);
    report.check(halfway.size() == first.size() &&
                     qFuzzyCompare(halfway.front().pos.x(), 5.0) &&
                     qFuzzyCompare(halfway.front().pos.y(), 5.0) &&
                     halfway.back().smooth,
                 QStringLiteral("matching path keyframes interpolate at the playhead"));

    qInfo().noquote() << "[Shape Path Test] failures:" << report.failures;
    return report.failures;
}

} // namespace Artifact
