module;

#include <cassert>
#include <cstdint>
#include <QFileInfo>
#include <QHash>
#include <QJsonObject>
#include <QSet>
#include <QUrl>
#include <utility>
#include <variant>
#include <vector>

module Artifact.Layer.NLETransitionBridge;

import NLE.Core;
import Video.AbstractTransition;
import Video.TransitionFactory;
import Video.CpuFrameView;
import Video.VideoFrame;
import Image.ImageF32x4_RGBA;
import Artifact.Composition.Abstract;
import NLE.OTIO;

// M0: Video.ixx で漏れていた3実装をブリッジから import して静的登録を強制する。
// (Video.ixx を変更せず、実装ファイル側で登録漏れを解消する方針)
import Video.Transitions.GradientWipeTransition;
import Video.Transitions.IrisWipeTransition;
import Video.Transitions.BlockDissolveTransition;

namespace Artifact {

using namespace ArtifactCore;
using namespace ArtifactCore::NLE;

namespace {

// ImageF32x4_RGBA の正体は CV_32FC4 (RGBA float)。Video.Transitions.* の
// process() は uint8 RGBA8 を前提とするため、ここで 32F→8 へ変換する。
// rgba8Data() は CV_8UC4 でないと nullptr を返すため使えない点に注意。
DecodedVideoFrame makeDecodedFrame(const ImageF32x4_RGBA& rgba32f)
{
    const int w = rgba32f.width();
    const int h = rgba32f.height();
    const float* src = rgba32f.rgba32fData();

    CpuVideoFrame frame;
    frame.meta.width = w;
    frame.meta.height = h;
    frame.meta.pixelFormat = VideoFramePixelFormat::RGBA8;
    frame.strideBytes = w * 4;
    frame.bytes.resize(static_cast<std::size_t>(w) * h * 4);

    if (src) {
        const std::size_t total = static_cast<std::size_t>(w) * h * 4;
        std::uint8_t* dst = frame.bytes.data();
        for (std::size_t i = 0; i < total; ++i) {
            const float v = src[i];
            dst[i] = v <= 0.0f ? 0u
                   : (v >= 1.0f ? 255u : static_cast<std::uint8_t>(v * 255.0f + 0.5f));
        }
    }
    return DecodedVideoFrame{std::move(frame)};
}

ImageF32x4_RGBA frameToImage(const DecodedVideoFrame& frame)
{
    assert(std::holds_alternative<CpuVideoFrame>(frame));
    const CpuVideoFrame& cpu = std::get<CpuVideoFrame>(frame);
    ImageF32x4_RGBA out;
    out.setFromRGBA8(cpu.bytes.data(), cpu.meta.width, cpu.meta.height);
    return out;
}

} // namespace

ImageF32x4_RGBA applyNLETransition(TransitionKind kind,
                                    const ImageF32x4_RGBA& left,
                                    const ImageF32x4_RGBA& right,
                                    const TransitionContext& ctx)
{
    AbstractTransition* transition = TransitionFactory::instance().create(kind);
    if (!transition) {
        // Cut など具象の無い種別は左フレームをそのまま返す（パススルー）。
        return left.DeepCopy();
    }

    // RGBA32F (ImageF32x4_RGBA) -> RGBA8 (CpuVideoFrame) に変換。
    // Video.Transitions.* の process() は uint8 RGBA8 を前提とする。
    ImageF32x4_RGBA left8 = left;
    ImageF32x4_RGBA right8 = right;
    DecodedVideoFrame leftFrame = makeDecodedFrame(left8);
    DecodedVideoFrame rightFrame = makeDecodedFrame(right8);

    transition->process(leftFrame, rightFrame, ctx);

    ImageF32x4_RGBA result = frameToImage(leftFrame);
    delete transition;
    return result;
}

TransitionKind nleTransitionKindFromName(const QString& name)
{
    const QString normalized = name.trimmed().toLower();
    if (normalized == QStringLiteral("cut")) return TransitionKind::Cut;
    if (normalized == QStringLiteral("dissolve")) return TransitionKind::Dissolve;
    if (normalized == QStringLiteral("crossfade") || normalized == QStringLiteral("cross fade")) {
        return TransitionKind::Crossfade;
    }
    if (normalized == QStringLiteral("wipe")) return TransitionKind::Wipe;
    if (normalized == QStringLiteral("slide")) return TransitionKind::Slide;
    if (normalized == QStringLiteral("zoom")) return TransitionKind::Zoom;
    if (normalized == QStringLiteral("glitch") || normalized == QStringLiteral("glitchdisplace")) {
        return TransitionKind::GlitchDisplace;
    }
    if (normalized == QStringLiteral("spin")) return TransitionKind::Spin;
    if (normalized == QStringLiteral("linearwipe")) return TransitionKind::LinearWipe;
    if (normalized == QStringLiteral("radialwipe")) return TransitionKind::RadialWipe;
    if (normalized == QStringLiteral("flip")) return TransitionKind::Flip;
    if (normalized == QStringLiteral("cube")) return TransitionKind::Cube;
    if (normalized == QStringLiteral("doors")) return TransitionKind::Doors;
    if (normalized == QStringLiteral("lightleak")) return TransitionKind::LightLeak;
    if (normalized == QStringLiteral("gradientwipe")) return TransitionKind::GradientWipe;
    if (normalized == QStringLiteral("iriswipe") || normalized == QStringLiteral("iris")) return TransitionKind::IrisWipe;
    if (normalized == QStringLiteral("blockdissolve")) return TransitionKind::BlockDissolve;
    return TransitionKind::Crossfade;
}

ImageF32x4_RGBA applyNLETransitionByName(const QString& name,
                                         const ImageF32x4_RGBA& left,
                                         const ImageF32x4_RGBA& right,
                                         const TransitionContext& ctx)
{
    return applyNLETransition(nleTransitionKindFromName(name), left, right, ctx);
}

bool exportCompositionToOtioFile(const ArtifactAbstractComposition& composition,
                                 const QString& filePath,
                                 QVector<QString>* warnings)
{
    const double fps = composition.frameRate().framerate();
    if (fps <= 0.0 || filePath.trimmed().isEmpty()) {
        if (warnings) warnings->push_back(QStringLiteral("Invalid composition frame rate or OTIO path"));
        return false;
    }

    TimeBase timeBase;
    timeBase.denominator = 1000;
    timeBase.numerator = qMax(1, qRound(fps * 1000.0));
    NLEProjectStore store;
    const SequenceId sequenceId = store.createSequence(QStringLiteral("Artifact Composition"), timeBase);
    QHash<QString, ClipId> clipIdsByName;
    QHash<QString, TrackId> trackIdsByName;
    QSet<QString> ambiguousClipNames;

    for (const auto& layer : composition.allLayer()) {
        if (!layer) continue;
        const QJsonObject layerJson = layer->toJson();
        const QString type = layerJson.value(QStringLiteral("type")).toString().toLower();
        const TrackKind trackKind = type.contains(QStringLiteral("audio"))
            ? TrackKind::Audio : TrackKind::Video;
        const QString layerName = layer->layerName();
        const TrackId trackId = store.createTrack(sequenceId, trackKind, layerName);
        const qint64 inPoint = layer->inPoint().framePosition();
        const qint64 duration = qMax<qint64>(1, layer->outPoint().framePosition() - inPoint);

        QString sourcePath = layerJson.value(QStringLiteral("sourcePath")).toString();
        for (const QString& key : {QStringLiteral("image.sourcePath"), QStringLiteral("video.sourcePath"),
                                   QStringLiteral("audio.sourcePath"), QStringLiteral("svg.sourcePath")}) {
            if (sourcePath.isEmpty()) sourcePath = layerJson.value(key).toString();
        }
        if (sourcePath.isEmpty()) {
            sourcePath = QStringLiteral("artifact://layer/%1").arg(layer->id().toString());
            if (warnings) warnings->push_back(
                QStringLiteral("Layer has no external source; exported as placeholder: %1").arg(layerName));
        }

        SourceRef source;
        source.uri = sourcePath.startsWith(QStringLiteral("artifact://"))
            ? sourcePath : QUrl::fromLocalFile(QFileInfo(sourcePath).absoluteFilePath()).toString();
        source.displayName = layerName;
        source.timeBase = timeBase;
        const SourceId sourceId = store.registerSource(source);

        ClipDraft draft;
        draft.sourceId = sourceId;
        draft.sourceRange = FrameRange::fromDuration(layer->startTime().framePosition(), duration);
        draft.timelineRange = FrameRange::fromDuration(inPoint, duration);
        draft.trimRange = draft.sourceRange;
        draft.name = layerName;
        draft.enabled = layer->isVisible();
        const ClipId clipId = store.addClip(sequenceId, trackId, draft);
        if (!clipId.isValid() && warnings) {
            warnings->push_back(QStringLiteral("Failed to export layer: %1").arg(layerName));
        }
        if (clipId.isValid()) {
            if (clipIdsByName.contains(layerName)) {
                ambiguousClipNames.insert(layerName);
                if (warnings) warnings->push_back(
                    QStringLiteral("Duplicate layer name makes OTIO transition references ambiguous: %1")
                        .arg(layerName));
            } else {
                clipIdsByName.insert(layerName, clipId);
                trackIdsByName.insert(layerName, trackId);
            }
        }
    }

    const auto transitionKind = [](const QString& name) {
        const QString normalized = name.trimmed().toLower().remove(QLatin1Char(' '));
        if (normalized == QStringLiteral("cut")) return TransitionKind::Cut;
        if (normalized == QStringLiteral("dissolve")) return TransitionKind::Dissolve;
        if (normalized == QStringLiteral("wipe")) return TransitionKind::Wipe;
        if (normalized == QStringLiteral("slide")) return TransitionKind::Slide;
        if (normalized == QStringLiteral("zoom")) return TransitionKind::Zoom;
        if (normalized == QStringLiteral("glitch") || normalized == QStringLiteral("glitchdisplace")) return TransitionKind::GlitchDisplace;
        if (normalized == QStringLiteral("spin")) return TransitionKind::Spin;
        if (normalized == QStringLiteral("linearwipe")) return TransitionKind::LinearWipe;
        if (normalized == QStringLiteral("radialwipe")) return TransitionKind::RadialWipe;
        if (normalized == QStringLiteral("flip")) return TransitionKind::Flip;
        if (normalized == QStringLiteral("cube")) return TransitionKind::Cube;
        if (normalized == QStringLiteral("doors")) return TransitionKind::Doors;
        if (normalized == QStringLiteral("lightleak")) return TransitionKind::LightLeak;
        if (normalized == QStringLiteral("gradientwipe")) return TransitionKind::GradientWipe;
        if (normalized == QStringLiteral("iriswipe") || normalized == QStringLiteral("iris")) return TransitionKind::IrisWipe;
        if (normalized == QStringLiteral("blockdissolve")) return TransitionKind::BlockDissolve;
        return TransitionKind::Crossfade;
    };

    for (const auto& transition : composition.timelineTransitions()) {
        const auto left = clipIdsByName.constFind(transition.leftClipName);
        const auto right = clipIdsByName.constFind(transition.rightClipName);
        const auto leftTrack = trackIdsByName.constFind(transition.leftClipName);
        const auto rightTrack = trackIdsByName.constFind(transition.rightClipName);
        if (ambiguousClipNames.contains(transition.leftClipName) ||
            ambiguousClipNames.contains(transition.rightClipName) ||
            left == clipIdsByName.cend() || right == clipIdsByName.cend() ||
            leftTrack == trackIdsByName.cend() || rightTrack == trackIdsByName.cend() ||
            leftTrack.value() != rightTrack.value()) {
            if (warnings) warnings->push_back(QStringLiteral("Skipped transition because its clips are missing or on different tracks"));
            continue;
        }
        const qint64 duration = qMax<qint64>(1, transition.range.duration());
        store.createTransition(leftTrack.value(), left.value(), right.value(),
                               transition.range, transitionKind(transition.kind),
                               static_cast<double>(duration),
                               Transition::Direction::LeftToRight);
    }
    return OtioAdapter::exportTimelineFile(store, sequenceId, filePath, warnings);
}

} // namespace Artifact
