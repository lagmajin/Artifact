module;

#include <vector>

#include <QDebug>
#include <QPointF>
#include <QString>
#include <QtGlobal>

export module Artifact.Test.ShapePath;

import Artifact.Layer.Shape;
import Shape.Operator;

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

    layer.addShapeOperator(ArtifactCore::ShapeOperatorType::TrimPaths);
    layer.addShapeOperator(ArtifactCore::ShapeOperatorType::Repeater);
    report.check(layer.shapeOperatorCount() == 2,
                 QStringLiteral("shape operators can be added to the layer"));
    const auto roundTrip = ArtifactShapeLayer::fromJson(layer.toJson());
    report.check(roundTrip && roundTrip->shapeOperatorCount() == 2,
                 QStringLiteral("shape operators survive layer JSON roundtrip"));

    ShapeContent content;
    content.name = QStringLiteral("Transform Content");
    content.geometry.type = ShapeType::Rect;
    content.transform.anchor = QPointF(10.0, 20.0);
    content.transform.position = QPointF(40.0, 50.0);
    content.transform.scale = QPointF(1.5, 0.75);
    content.transform.rotation = 22.5;
    content.transform.skew = 8.0;
    content.transform.skewAxis = 15.0;
    const int contentIndex = layer.addShapeContent(content);
    report.check(contentIndex >= 0,
                 QStringLiteral("shape content with local transform can be added"));
    const auto transformedRoundTrip = ArtifactShapeLayer::fromJson(layer.toJson());
    const auto restored = transformedRoundTrip
        ? transformedRoundTrip->shapeContentAt(contentIndex) : ShapeContent();
    report.check(transformedRoundTrip &&
                     qFuzzyCompare(restored.transform.position.x(), 40.0) &&
                     qFuzzyCompare(restored.transform.scale.y(), 0.75) &&
                     qFuzzyCompare(restored.transform.rotation, 22.5) &&
                     qFuzzyCompare(restored.transform.skew, 8.0),
                 QStringLiteral("content transform survives layer JSON roundtrip"));
    report.check(layer.setLayerPropertyValue(QStringLiteral("shape.content.%1.transform.positionX").arg(contentIndex),
                                             123.0),
                 QStringLiteral("content transform property path is writable"));
    const auto edited = layer.shapeContentAt(contentIndex);
    report.check(qFuzzyCompare(edited.transform.position.x(), 123.0),
                 QStringLiteral("content transform property updates the model"));

    qInfo().noquote() << "[Shape Path Test] failures:" << report.failures;
    return report.failures;
}

} // namespace Artifact
