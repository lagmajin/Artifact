module;
#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <unordered_map>
#include <set>
#include <unordered_set>
#include <memory>
#include <algorithm>
#include <cmath>
#include <functional>
#include <optional>
#include <utility>
#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <variant>
#include <any>
#include <atomic>
#include <queue>
#include <deque>
#include <list>
#include <tuple>
#include <numeric>
#include <regex>
#include <random>
export module Artifact.Effect.ImplBase;




import Artifact.Effect.Context;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;

export namespace Artifact {

 using namespace ArtifactCore;

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
        //dst = src;
    }

    // GPUバックエンドの処理（HLSLシェーダを使用）
    virtual void applyGPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) {
        // デフォルト実装: CPUバックエンドにフォールバック
        //applyCPU(src, dst);
    }

    // 初期化と解放
    virtual bool initialize() { return true; }
    virtual void release() {}
};

}