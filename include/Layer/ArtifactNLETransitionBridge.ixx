module;

#include <cstdint>
#include <variant>

#include "../Define/DllExportMacro.hpp"

export module Artifact.Layer.NLETransitionBridge;

import NLE.Core;
import Video.AbstractTransition;
import Video.TransitionFactory;
import Video.CpuFrameView;
import Image.ImageF32x4_RGBA;

export namespace Artifact {

using namespace ArtifactCore;
using namespace ArtifactCore::NLE;

/**
 * @brief NLE の Transition を ArtifactCore Video エンジン経由で適用するブリッジ。
 *
 * NLE.Core の Transition(TransitionKind) はデータモデルの正であり、
 * ArtifactCore::Video の AbstractTransition::process() は RGBA8 の DecodedVideoFrame を
 * 前提としている。Artifact 側のレイヤバッファは RGBA32F (ImageF32x4_RGBA) なので、
 * このブリッジで両者のフォーマット変換を行う。
 */

// 左フレームは in-place で合成されるため、out には left の変換結果を渡すこと。
LIBRARY_DLL_API ImageF32x4_RGBA applyNLETransition(TransitionKind kind,
                                                  const ImageF32x4_RGBA& left,
                                                  const ImageF32x4_RGBA& right,
                                                  const TransitionContext& ctx);

} // namespace Artifact
