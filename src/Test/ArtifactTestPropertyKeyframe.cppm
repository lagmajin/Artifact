module;

#include <memory>
#include <utility>
#include <limits>

#include <QColor>
#include <QPointF>

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
import Math.Interpolate;
import Animation.Transform3D;
import Frame.Position;
import Frame.Rate;
import Memory.SharedPtr;
import Utils.Id;
import Artifact.Composition.Abstract;
import Artifact.Composition.InitParams;
import Artifact.Layer.Abstract;
import Artifact.Layer.Null;
import Artifact.Widgets.CompositionGizmoUndoCommands;

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

    const struct {
        double fps;
        int scale;
    } fractionalRates[] = {{23.976, 24}, {29.97, 30}, {59.94, 60}};
    for (const auto& rate : fractionalRates) {
        const QString prefix = QStringLiteral("%1 fps: ").arg(rate.fps, 0, 'f', 3);
        const auto storageScale = ArtifactCore::FrameRate::storageScaleForFps(rate.fps);
        report.check(storageScale == rate.scale,
                     prefix + QStringLiteral("canonical storage scale rounds rather than truncates FPS"));

        ArtifactCompositionInitParams params;
        params.setResolution(100, 100);
        params.setFrameRate(rate.fps);
        params.setDurationFrames(240);
        auto composition = ArtifactCore::makeShared<ArtifactAbstractComposition>(
            ArtifactCore::CompositionID(), params);
        ArtifactAbstractLayerPtr layer = ArtifactCore::makeShared<ArtifactNullLayer>();
        layer->setComposition(composition.get());
        layer->setOutPoint(FramePosition(200));
        layer->setInPoint(FramePosition(40));
        layer->setStartTime(FramePosition(7));
        const QString positionPath = QStringLiteral("transform.position.x");
        const auto position = layer->getProperty(positionPath);
        report.check(position && position.get() ==
                         layer->transform3D().channelProperty(TransformChannel::PositionX).get(),
                     prefix + QStringLiteral("layer property and native transform share the same channel"));
        if (!position) continue;

        const RationalTime firstTime(60, rate.scale);
        const RationalTime secondTime(84, rate.scale);
        const RationalTime middleTime(72, rate.scale);
        const RationalTime laterTime(120, rate.scale);
        composition->goToFrame(60);
        layer->goToFrame(60);
        report.check(composition->framePosition().framePosition() == 60 &&
                         layer->currentFrame() == 27,
                     prefix + QStringLiteral("offset layer has distinct composition and relative frames"));
        position->addKeyFrame(firstTime, QVariant(100.0));

        composition->goToFrame(84);
        layer->goToFrame(84);
        report.check(composition->framePosition().framePosition() == 84 &&
                         layer->currentFrame() == 51,
                     prefix + QStringLiteral("second authoring frame remains distinct from layer time"));
        GizmoTransformSnapshot before;
        captureGizmoPropertyKeys(layer, secondTime, before);
        report.check(before.propertyAnimated(positionPath) && !before.properties[0].hasKey,
                     prefix + QStringLiteral("gizmo captures animation without a key at the new authoring time"));
        report.check(layer->setLayerPropertyValue(positionPath, QVariant(140.0)),
                     prefix + QStringLiteral("animated layer property accepts a second authoring edit"));
        report.check(position->getKeyFrames().size() == 2 &&
                         position->hasKeyFrameAt(firstTime) && position->hasKeyFrameAt(secondTime),
                     prefix + QStringLiteral("two composition-time keys survive the shared-property edit"));
        report.check(!position->hasKeyFrameAt(RationalTime(27, rate.scale)) &&
                         !position->hasKeyFrameAt(RationalTime(51, rate.scale)),
                     prefix + QStringLiteral("authoring does not write layer-relative keys"));
        report.check(position->interpolateValue(firstTime).toDouble() == 100.0 &&
                         position->interpolateValue(secondTime).toDouble() == 140.0 &&
                         position->interpolateValue(middleTime).toDouble() == 120.0 &&
                         layer->transform3D().snapshotAt(middleTime).positionX == 120.0f,
                     prefix + QStringLiteral("shared property and native transform interpolate composition-time keys"));

        GizmoTransformSnapshot after;
        captureGizmoPropertyKeys(layer, secondTime, after);
        report.check(after.properties[0].hasKey &&
                         after.properties[0].key.time == secondTime &&
                         after.properties[0].key.value.toDouble() == 140.0,
                     prefix + QStringLiteral("gizmo captures the key at the authoring time"));
        GizmoTransformSnapshot relativeSnapshot;
        captureGizmoPropertyKeys(layer, RationalTime(layer->currentFrame(), rate.scale),
                                 relativeSnapshot);
        report.check(!relativeSnapshot.properties[0].hasKey,
                     prefix + QStringLiteral("layer-relative capture cannot find the composition-time key"));

        composition->goToFrame(120);
        layer->goToFrame(120);
        position->addKeyFrame(laterTime, QVariant(180.0));
        restoreGizmoPropertyKeys(layer, secondTime, before);
        report.check(position->getKeyFrames().size() == 2 &&
                         !position->hasKeyFrameAt(secondTime) &&
                         position->interpolateValue(firstTime).toDouble() == 100.0 &&
                         position->interpolateValue(laterTime).toDouble() == 180.0,
                     prefix + QStringLiteral("gizmo restore removes only the newly authored key after seeking"));
        restoreGizmoPropertyKeys(layer, secondTime, after);
        report.check(position->getKeyFrames().size() == 3 &&
                         position->hasKeyFrameAt(secondTime) &&
                         position->interpolateValue(secondTime).toDouble() == 140.0,
                     prefix + QStringLiteral("gizmo restore reinstates the captured key"));
        position->addKeyFrame(secondTime, QVariant(999.0));
        restoreGizmoPropertyKeys(layer, secondTime, after);
        report.check(position->getKeyFrames().size() == 3 &&
                         position->interpolateValue(secondTime).toDouble() == 140.0,
                     prefix + QStringLiteral("gizmo restore replaces an edited existing key without duplication"));

        GizmoTransformUndoCommand command(layer, 84, before, after);
        command.undo();
        report.check(command.lastOperationSucceeded() &&
                         position->getKeyFrames().size() == 2 &&
                         !position->hasKeyFrameAt(secondTime) &&
                         position->hasKeyFrameAt(firstTime) && position->hasKeyFrameAt(laterTime),
                     prefix + QStringLiteral("undo uses the saved authoring frame rather than either current frame"));
        command.redo();
        report.check(command.lastOperationSucceeded() &&
                         position->getKeyFrames().size() == 3 &&
                         position->hasKeyFrameAt(secondTime) &&
                         position->interpolateValue(firstTime).toDouble() == 100.0 &&
                         position->interpolateValue(secondTime).toDouble() == 140.0 &&
                         position->interpolateValue(laterTime).toDouble() == 180.0 &&
                         layer->transform3D().snapshotAt(middleTime).positionX == 120.0f,
                     prefix + QStringLiteral("redo restores the saved key and interpolation without changing other keys"));
        report.check(composition->framePosition().framePosition() == 120 &&
                         layer->currentFrame() == 87,
                     prefix + QStringLiteral("gizmo undo and redo leave both current frames unchanged"));
    }
    // Regression (VP drag on a keyed frame): the gizmo authors keys in the
    // composition frame domain, so the transform must advertise exactly that
    // storage scale. When the two roundings disagreed (29.97fps -> 29 vs 30),
    // a drag on a frame that already had a key could not find the key and
    // rewrote the initial value instead of recording the transform.
    {
        ArtifactCompositionInitParams params;
        params.setResolution(100, 100);
        params.setFrameRate(29.97);
        params.setDurationFrames(240);
        auto composition = ArtifactCore::makeShared<ArtifactAbstractComposition>(
            ArtifactCore::CompositionID(), params);
        ArtifactAbstractLayerPtr layer =
            ArtifactCore::makeShared<ArtifactNullLayer>();
        layer->setComposition(composition.get());
        const auto scale = ArtifactCore::FrameRate::storageScaleForFps(29.97);
        auto& t3d = layer->transform3D();
        report.check(t3d.keyframeTimeScale() == scale,
                     QStringLiteral("29.97fps layer transform advertises the rounded storage scale"));
        t3d.setInitialPosition(RationalTime(0, scale), 10.0f, 20.0f);
        const RationalTime keyedTime(15, scale);
        t3d.channelProperty(TransformChannel::PositionX)
            ->addKeyFrame(keyedTime, 110.0f);
        report.check(t3d.hasPositionKeyFrameAt(keyedTime) &&
                         t3d.snapshotAt(keyedTime).positionX == 110.0f,
                     QStringLiteral("keyed frame resolves in the composition frame domain"));
        const float initialX =
            t3d.snapshotAt(keyedTime).positionX - t3d.positionXAt(keyedTime);
        const float dragX = 175.0f;
        t3d.setPosition(keyedTime, dragX - initialX, 0.0f);
        report.check(t3d.channelProperty(TransformChannel::PositionX)
                             ->getKeyFrames()
                             .size() == 1 &&
                         t3d.hasPositionKeyFrameAt(keyedTime) &&
                         t3d.snapshotAt(keyedTime).positionX == dragX,
                     QStringLiteral("gizmo-style drag on a keyed frame records into the existing key"));
        report.check(layer->keyframeTimeScale() == scale &&
                         layer->keyframeTimeAtFrame(84) == RationalTime(84, scale),
                     QStringLiteral("layer keyframe time accessors agree with the storage domain"));
        // Re-pinning the composition rate must carry every layer's transform
        // domain with it, so later drags keep addressing the stored keys.
        composition->appendLayerTop(layer);
        composition->setFrameRate(ArtifactCore::FrameRate(30.0f));
        report.check(layer->keyframeTimeScale() == 30 &&
                         layer->keyframeTimeAtFrame(15) == RationalTime(15, 30) &&
                         t3d.hasPositionKeyFrameAt(RationalTime(15, 30)),
                     QStringLiteral("composition rate change re-pins the layer keyframe domain"));
        // Keys keep their authored instant across a rate change: the domain
        // moves, the stored key does not.
        composition->setFrameRate(ArtifactCore::FrameRate(24.0f));
        report.check(layer->keyframeTimeScale() == 24 &&
                         layer->keyframeTimeAtFrame(15) == RationalTime(15, 24) &&
                         t3d.hasPositionKeyFrameAt(RationalTime(15, 30)) &&
                         !t3d.hasPositionKeyFrameAt(RationalTime(15, 24)),
                     QStringLiteral("rate change moves the domain without moving stored keys"));
    }

    // Value-only edits preserve authored interpolation and every metadata field.
    using ArtifactCore::InterpolationType;
    using ArtifactCore::KeyFrame;
    AbstractProperty edited;
    edited.addKeyFrame(RationalTime(12, 24), 1.0, InterpolationType::Bezier,
                       0.2f, -0.3f, 0.8f, 1.3f, true);
    edited.setKeyFrameAnchorAt(RationalTime(12, 24), KeyFrame::Anchor::LockToOut);
    edited.setKeyFrameColorLabelAt(RationalTime(12, 24), KeyFrame::ColorLabel::Blue);
    edited.addKeyFrame(RationalTime(24, 48), 2.0);
    const auto editedKeys = edited.getKeyFrames();
    report.check(editedKeys.size() == 1 && editedKeys.front().value.toDouble() == 2.0 &&
                     editedKeys.front().time.value() == 12 && editedKeys.front().time.scale() == 24 &&
                     editedKeys.front().interpolation == InterpolationType::Bezier &&
                     editedKeys.front().cp1_x == 0.2f && editedKeys.front().cp1_y == -0.3f &&
                     editedKeys.front().cp2_x == 0.8f && editedKeys.front().cp2_y == 1.3f &&
                     editedKeys.front().roving &&
                     editedKeys.front().anchor == KeyFrame::Anchor::LockToOut &&
                     editedKeys.front().colorLabel == KeyFrame::ColorLabel::Blue,
                 QStringLiteral("value-only replacement preserves authored metadata and time representation"));
    edited.addKeyFrame(RationalTime(12, 24), std::numeric_limits<double>::quiet_NaN());
    report.check(!edited.lastError().isEmpty() &&
                     edited.interpolateValue(RationalTime(12, 24)).toDouble() == 2.0,
                 QStringLiteral("invalid value-only update leaves the key unchanged"));
    edited.addKeyFrame(RationalTime(24, 24), 3.0);
    const auto insertedKeys = edited.getKeyFrames();
    report.check(edited.lastError().isEmpty() && insertedKeys.size() == 2 &&
                     insertedKeys.back().interpolation == InterpolationType::Linear &&
                     !insertedKeys.back().roving,
                 QStringLiteral("new value-only keys retain defaults and clear prior errors"));
    edited.addKeyFrame(RationalTime(12, 24), 4.0, InterpolationType::Constant);
    report.check(edited.getKeyFrames().front().interpolation == InterpolationType::Constant,
                 QStringLiteral("explicit interpolation updates still change interpolation"));

    // Hold switches at the exact key, for every supported property value type.
    const struct {
        PropertyType type;
        QVariant first;
        QVariant middle;
        QVariant last;
    } holdCases[] = {
        {PropertyType::Float, 0.0, 100.0, 200.0},
        {PropertyType::Integer, 0, 100, 200},
        {PropertyType::Boolean, false, true, false},
        {PropertyType::Color, QVariant::fromValue(QColor(Qt::red)),
         QVariant::fromValue(QColor(Qt::green)), QVariant::fromValue(QColor(Qt::blue))},
        {PropertyType::String, QStringLiteral("a"), QStringLiteral("b"), QStringLiteral("c")},
        {PropertyType::ObjectReference, QStringLiteral("id-a"), QStringLiteral("id-b"), QStringLiteral("id-c")},
        {PropertyType::Point2D, QPointF(0, 0), QPointF(10, 20), QPointF(30, 40)}
    };
    for (const auto& c : holdCases) {
        AbstractProperty held;
        held.setType(c.type);
        held.addKeyFrame(RationalTime(0, 24), c.first, InterpolationType::Constant);
        held.addKeyFrame(RationalTime(10, 24), c.middle, InterpolationType::Constant);
        held.addKeyFrame(RationalTime(20, 24), c.last, InterpolationType::Constant);
        report.check(held.interpolateValue(RationalTime(9, 24)) == c.first &&
                         held.interpolateValue(RationalTime(20, 48)) == c.middle &&
                         held.interpolateValue(RationalTime(11, 24)) == c.middle &&
                         held.interpolateValue(RationalTime(20, 24)) == c.last,
                     QStringLiteral("Hold before/exact/after boundary for type %1").arg(static_cast<int>(c.type)));
    }
    ArtifactCore::KeyframeInterpolator<float> heldInterpolator;
    AnimatableValueT<float> heldTemplate;
    for (int i = 0; i < 3; ++i) {
        heldInterpolator.addKeyframe({static_cast<double>(i * 10), static_cast<float>(i * 100),
                                      InterpolationType::Constant});
        heldTemplate.addKeyFrame(FramePosition(i * 10), static_cast<float>(i * 100),
                                 InterpolationType::Constant, 0.42f, 0.0f, 0.58f, 1.0f);
    }
    const int holdFrames[] = {9, 10, 11, 20};
    for (int frame : holdFrames) {
        report.check(heldInterpolator.evaluate(frame) == heldTemplate.at(FramePosition(frame)),
                     QStringLiteral("shared interpolator and template agree at Hold frame %1").arg(frame));
    }

    // Absolute keys keep their original rational representation during retime.
    AbstractProperty retime;
    retime.addKeyFrame(RationalTime(1, 48), 1.0);
    retime.addKeyFrame(RationalTime(1, 24), 2.0);
    retime.addKeyFrame(RationalTime(9007199254740993LL, 1), 3.0);
    retime.retimeKeyFramesForLayerPointChange(
        RationalTime(0, 24), RationalTime(24, 24), RationalTime(12, 24), RationalTime(48, 24));
    const auto absoluteKeys = retime.getKeyFrames();
    report.check(absoluteKeys.size() == 3 && absoluteKeys[0].time.value() == 1 &&
                     absoluteKeys[0].time.scale() == 48 && absoluteKeys[1].time.value() == 1 &&
                     absoluteKeys[1].time.scale() == 24 &&
                     absoluteKeys[2].time.value() == 9007199254740993LL &&
                     retime.lastError().isEmpty(),
                 QStringLiteral("Absolute retime preserves subframe and large integer times exactly"));

    AbstractProperty collision;
    collision.addKeyFrame(RationalTime(1, 24), 10.0);
    collision.addKeyFrame(RationalTime(2, 24), 20.0, InterpolationType::Bezier,
                          0.2f, 0.3f, 0.7f, 0.8f, true);
    collision.setKeyFrameAnchorAt(RationalTime(2, 24), KeyFrame::Anchor::LockToIn);
    collision.setKeyFrameColorLabelAt(RationalTime(2, 24), KeyFrame::ColorLabel::Red);
    collision.retimeKeyFramesForLayerPointChange(
        RationalTime(0, 24), RationalTime(24, 24), RationalTime(-1, 24), RationalTime(24, 24));
    const auto collisionKeys = collision.getKeyFrames();
    report.check(!collision.lastError().isEmpty() && collisionKeys.size() == 2 &&
                     collisionKeys[0].time == RationalTime(1, 24) &&
                     collisionKeys[1].time == RationalTime(2, 24) &&
                     collisionKeys[1].value.toDouble() == 20.0 &&
                     collisionKeys[1].interpolation == InterpolationType::Bezier &&
                     collisionKeys[1].cp1_x == 0.2f && collisionKeys[1].cp2_y == 0.8f &&
                     collisionKeys[1].roving &&
                     collisionKeys[1].colorLabel == KeyFrame::ColorLabel::Red,
                 QStringLiteral("retime collision rejects the entire property edit without losing keys"));
    collision.retimeKeyFramesForLayerPointChange(
        RationalTime(0, 24), RationalTime(24, 24), RationalTime(1, 24), RationalTime(24, 24));
    report.check(collision.lastError().isEmpty() && collision.hasKeyFrameAt(RationalTime(1, 24)) &&
                     collision.hasKeyFrameAt(RationalTime(3, 24)),
                 QStringLiteral("successful retime preserves Absolute and moves LockToIn"));

    AbstractProperty anchored;
    anchored.addKeyFrame(RationalTime(6, 24), 1.0);
    anchored.setKeyFrameAnchorAt(RationalTime(6, 24), KeyFrame::Anchor::StretchWithLayer);
    anchored.addKeyFrame(RationalTime(18, 24), 2.0);
    anchored.setKeyFrameAnchorAt(RationalTime(18, 24), KeyFrame::Anchor::LockToOut);
    anchored.retimeKeyFramesForLayerPointChange(
        RationalTime(0, 24), RationalTime(24, 24), RationalTime(0, 24), RationalTime(48, 24));
    report.check(anchored.lastError().isEmpty() && anchored.hasKeyFrameAt(RationalTime(12, 24)) &&
                     anchored.hasKeyFrameAt(RationalTime(42, 24)),
                 QStringLiteral("StretchWithLayer and LockToOut retain their retime behavior"));

    qInfo().noquote() << "[PropertyKeyframe Test] failures:" << report.failures;
    return report.failures;
}

} // namespace Artifact
