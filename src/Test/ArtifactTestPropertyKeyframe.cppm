module;

#include <memory>
#include <utility>

#include <QDebug>
#include <QString>
#include <QVariant>
#include <QJsonObject>

export module Artifact.Test.PropertyKeyframe;

import Property.Abstract;
import Property.Group;
import Property.SerializationBridge;
import Time.Rational;
import Animation.Value;
import Animation.Transform3D;
import Frame.Position;
import Memory.SharedPtr;

namespace Artifact {

using ArtifactCore::AbstractProperty;
using ArtifactCore::AbstractPropertyPtr;
using ArtifactCore::AnimatableValueT;
using ArtifactCore::FramePosition;
using ArtifactCore::PropertySerializationBridge;
using ArtifactCore::PropertyType;
using ArtifactCore::RationalTime;

namespace {
struct PropertyKeyframeTestReport {
    int failures = 0;

    void check(bool condition, const QString& label)
    {
        if (!condition) {
            ++failures;
            qWarning().noquote() << "[PropertyKeyframe Test][FAIL]" << label;
        } else {
            qInfo().noquote() << "[PropertyKeyframe Test][OK]" << label;
        }
    }
};
} // namespace

export int runPropertyKeyframeTests()
{
    PropertyKeyframeTestReport report;

    report.check(RationalTime(24, 24) == RationalTime(48, 48),
                 QStringLiteral("RationalTime equality is scale independent"));
    report.check(RationalTime(3074457345618258602LL, 2) ==
                     RationalTime(4611686018427387903LL, 3),
                 QStringLiteral("RationalTime equality avoids cross-multiply overflow"));
    report.check(RationalTime(-24, 24) == RationalTime(-48, 48),
                 QStringLiteral("RationalTime equality preserves negative equivalents"));
    report.check(RationalTime(9007199254740992LL, 1) <
                     RationalTime(9007199254740993LL, 1),
                 QStringLiteral("RationalTime ordering preserves adjacent large values"));
    report.check(RationalTime(1, 3) < RationalTime(2, 5),
                 QStringLiteral("RationalTime ordering compares unlike scales exactly"));
    report.check(RationalTime(-2, 5) < RationalTime(-1, 3),
                 QStringLiteral("RationalTime ordering handles negative fractions"));

    AbstractProperty property;
    property.setType(PropertyType::Float);
    property.setAnimatable(true);

    property.addKeyFrame(RationalTime(24, 24), QVariant(1.0));
    report.check(property.getKeyFrames().size() == 1, QStringLiteral("first keyframe is stored"));
    report.check(property.hasKeyFrameAt(RationalTime(24, 24)), QStringLiteral("exact time lookup succeeds"));
    report.check(property.hasKeyFrameAt(RationalTime(48, 48)), QStringLiteral("same-second lookup succeeds"));

    property.addKeyFrame(RationalTime(48, 48), QVariant(2.0));
    report.check(property.getKeyFrames().size() == 1, QStringLiteral("same second replaces existing keyframe"));
    report.check(property.getKeyFrames().front().value.toDouble() == 2.0, QStringLiteral("replacement updates value"));

    property.removeKeyFrame(RationalTime(24, 24));
    report.check(property.getKeyFrames().empty(), QStringLiteral("remove by equivalent time erases keyframe"));

    property.addKeyFrame(RationalTime(12, 24), QVariant(3.0));
    property.addKeyFrame(RationalTime(36, 24), QVariant(4.0));
    property.removeKeyFrame(RationalTime(24, 48));
    report.check(property.getKeyFrames().size() == 1, QStringLiteral("remove by reduced equivalent time erases matching keyframe"));
    report.check(!property.hasKeyFrameAt(RationalTime(12, 24)), QStringLiteral("reduced equivalent keyframe is gone"));
    report.check(property.hasKeyFrameAt(RationalTime(36, 24)), QStringLiteral("second keyframe remains after unrelated remove"));

    property.addKeyFrame(RationalTime(9007199254740992LL, 1), QVariant(5.0));
    property.addKeyFrame(RationalTime(9007199254740993LL, 1), QVariant(6.0));
    report.check(property.getKeyFrames().size() == 3,
                 QStringLiteral("adjacent large integer times are not merged by floating point rounding"));

    AnimatableValueT<double> animatable;
    animatable.addKeyFrame(FramePosition(20), 2.0);
    animatable.addKeyFrame(FramePosition(10), 1.0);
    animatable.addKeyFrame(FramePosition(20), 3.0);
    auto animatableKeys = animatable.getKeyFrames();
    report.check(animatableKeys.size() == 2,
                 QStringLiteral("AnimatableValue keeps one keyframe per frame"));
    report.check(animatableKeys.size() == 2 &&
                     animatableKeys[0].frame == FramePosition(10) &&
                     animatableKeys[1].frame == FramePosition(20),
                 QStringLiteral("AnimatableValue keyframes remain sorted"));
    report.check(animatableKeys.size() == 2 && animatableKeys[1].value == 3.0,
                 QStringLiteral("AnimatableValue replacement updates the existing value"));
    report.check(animatable.moveKeyFrame(FramePosition(10), FramePosition(20)),
                 QStringLiteral("AnimatableValue move succeeds"));
    animatableKeys = animatable.getKeyFrames();
    report.check(animatableKeys.size() == 1 && animatableKeys[0].value == 1.0,
                 QStringLiteral("AnimatableValue move replaces a destination collision deterministically"));

    AbstractPropertyPtr serializable = ArtifactCore::makeShared<AbstractProperty>(property);
    const auto serialized = PropertySerializationBridge::serializeProperty(serializable);
    AbstractPropertyPtr roundTripped = ArtifactCore::makeShared<AbstractProperty>();
    roundTripped->setType(PropertyType::Float);
    roundTripped->setAnimatable(true);
    PropertySerializationBridge::deserializeProperty(roundTripped, serialized);
    report.check(roundTripped->getKeyFrames().size() == 3, QStringLiteral("serialization roundtrip preserves keyframe count"));
    report.check(roundTripped->hasKeyFrameAt(RationalTime(9007199254740992LL, 1)), QStringLiteral("roundtrip preserves first large keyframe time"));
    report.check(roundTripped->hasKeyFrameAt(RationalTime(9007199254740993LL, 1)), QStringLiteral("roundtrip preserves adjacent large keyframe time"));
    report.check(roundTripped->hasKeyFrameAt(RationalTime(36, 24)), QStringLiteral("roundtrip preserves second keyframe time"));

    // Native transforms and timeline properties must have one key store.
    using ArtifactCore::AnimatableTransform3D;
    using ArtifactCore::TransformChannel;
    AnimatableTransform3D transform;
    auto x = transform.channelProperty(TransformChannel::PositionX);
    auto y = transform.channelProperty(TransformChannel::PositionY);
    transform.setInitialPosition(RationalTime(0, 24), 100.0f, 200.0f);
    report.check(x->getKeyFrames().empty() && y->getKeyFrames().empty(),
                 QStringLiteral("initial placement does not enable animation"));
    x->addKeyFrame(RationalTime(0, 24), 100.0f);
    x->addKeyFrame(RationalTime(24, 24), 140.0f);
    report.check(transform.snapshotAt(RationalTime(12, 24)).positionX == 120.0f,
                 QStringLiteral("native evaluation reads timeline position keys"));
    report.check(y->getKeyFrames().empty(),
                 QStringLiteral("position X keys do not enable position Y"));
    transform.setPosition(RationalTime(24, 24), 60.0f, 0.0f);
    report.check(x->interpolateValue(RationalTime(24, 24)).toFloat() == 160.0f,
                 QStringLiteral("native relative position writes canonical absolute key"));
    x->clearKeyFrames();
    y->clearKeyFrames();
    report.check(transform.getPositionKeyFrameCount() == 0,
                 QStringLiteral("timeline clear leaves no native position keys"));
    transform.setInitialScale(RationalTime(0, 24), 2.0f, 3.0f);
    report.check(transform.snapshotAt(RationalTime(0, 24)).scaleX == 2.0f,
                 QStringLiteral("static scale is applied once"));
    transform.setInitialRotation(RationalTime(0, 24), 30.0f);
    report.check(transform.channelProperty(TransformChannel::Rotation)->getValue().toFloat() == 30.0f
                     && transform.snapshotAt(RationalTime(0, 24)).rotation == 30.0f,
                 QStringLiteral("initial rotation agrees with canonical property"));
    auto sx = transform.channelProperty(TransformChannel::ScaleX);
    sx->addKeyFrame(RationalTime(12, 24), 4.0f);
    transform.setKeyframeTimeScale(60);
    report.check(sx->hasKeyFrameAt(RationalTime(30, 60))
                     && transform.snapshotAt(RationalTime(30, 60)).scaleX == 4.0f,
                 QStringLiteral("frame rate change preserves key time"));
    AnimatableTransform3D copied(transform);
    copied.channelProperty(TransformChannel::ScaleX)->clearKeyFrames();
    report.check(sx->getKeyFrames().size() == 1,
                 QStringLiteral("copied layer owns independent keys"));
    AnimatableTransform3D assigned;
    const auto bound = assigned.channelProperty(TransformChannel::ScaleX);
    assigned = transform;
    report.check(bound.get() == assigned.channelProperty(TransformChannel::ScaleX).get()
                     && bound->getKeyFrames().size() == 1,
                 QStringLiteral("copy assignment preserves bound property identity"));
    assigned = AnimatableTransform3D{};
    report.check(bound.get() == assigned.channelProperty(TransformChannel::ScaleX).get()
                     && bound->getKeyFrames().empty(),
                 QStringLiteral("move reset clears keys without detaching editor"));
    const auto channelJson = PropertySerializationBridge::serializeProperty(sx);
    auto restored = assigned.channelProperty(TransformChannel::ScaleX);
    PropertySerializationBridge::deserializeProperty(restored, channelJson);
    report.check(assigned.snapshotAt(RationalTime(12, 24)).scaleX == 4.0f,
                 QStringLiteral("deserialization updates native evaluation directly"));

    qInfo().noquote() << "[PropertyKeyframe Test] failures:" << report.failures;
    return report.failures;
}

} // namespace Artifact
