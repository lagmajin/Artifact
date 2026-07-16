module;

#include <cassert>
#include <cstdint>
#include <vector>

import NLE.Core;
import Video.AbstractTransition;
import Video.TransitionFactory;
import Video.CpuFrameView;
import Image.ImageF32x4_RGBA;

// M0: Video.ixx で漏れていた3実装をブリッジから import して静的登録を強制する。
// (Video.ixx を変更せず、実装ファイル側で登録漏れを解消する方針)
import Video.Transitions.GradientWipeTransition;
import Video.Transitions.IrisWipeTransition;
import Video.Transitions.BlockDissolveTransition;

export namespace Artifact {

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

} // namespace Artifact
