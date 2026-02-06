module;
#include <opencv2/opencv.hpp>
export module Artifact.Effect.ImplBase;

import std;
import Artifact.Effect.Context;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;

export namespace Artifact {

class ArtifactEffectImplBase {
protected:
    EffectContext context_;

public:
    ArtifactEffectImplBase() = default;
    virtual ~ArtifactEffectImplBase() = default;

    // コンテキストの設定
    void setContext(const EffectContext& context) {
        context_ = context;
    }

    // CPUバックエンドの処理（OpenCVを使用）
    virtual void applyCPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) {
        // デフォルト実装: 単純なコピー
        dst = src;
    }

    // GPUバックエンドの処理（HLSLシェーダを使用）
    virtual void applyGPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) {
        // デフォルト実装: CPUバックエンドにフォールバック
        applyCPU(src, dst);
    }

    // 初期化と解放
    virtual bool initialize() { return true; }
    virtual void release() {}
};

}