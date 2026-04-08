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
#include <QString>
#include <QMatrix4x4>
export module Artifact.Color.Settings;




import Color.LUT;

export namespace Artifact
{

// カラースペース（ArtifactCoreと差別化）
enum class WorkingColorSpace
{
    Linear,
    sRGB,
    Rec709,
    Rec2020,
    P3,
    ACESAP0,
    ACESAP1,
};

// ガンマ関数
enum class WorkingGamma
{
    Linear,
    sRGB,
    Gamma22,
    Gamma24,
    PQ,
    HLG,
};

// HDRモード
enum class WorkingHDRMode
{
    SDR,
    HDR10,
    HDR10Plus,
    HLG,
    DolbyVision,
};

// カラー設定（Artifact用）
class ColorSettings
{
private:
    class Impl;
    Impl* impl_;

public:
    ColorSettings();
    ~ColorSettings();

    // 作業カラースペース
    void setWorkingSpace(WorkingColorSpace space);
    WorkingColorSpace workingSpace() const;

    // 入力・出力
    void setSourceSpace(WorkingColorSpace space);
    WorkingColorSpace sourceSpace() const;

    void setOutputSpace(WorkingColorSpace space);
    WorkingColorSpace outputSpace() const;

    // ガンマ
    void setGamma(WorkingGamma gamma);
    WorkingGamma gamma() const;

    // HDR
    void setHDRMode(WorkingHDRMode mode);
    WorkingHDRMode hdrMode() const;

    void setMaxNits(float nits);
    float maxNits() const;

    // ビット深度
    void setBitDepth(int bits);
    int bitDepth() const;

    // Core LUTへのブリッジ
    void setLUT(void* lut);
    void* lut() const;
};

// Core LUTを使用したエフェクト
class LUTColorEffect
{
private:
    class Impl;
    Impl* impl_;

public:
    LUTColorEffect();
    ~LUTColorEffect();

    // LUT設定
    void setLUT(void* lut);
    void* lut() const;

    void setLUTByName(const QString& name);

    // 強度
    void setIntensity(float value);
    float intensity() const;

    // 適用
    void apply(float& r, float& g, float& b) const;

    // イメージに適用
    // void applyToImage(ImageF32x4_RGBA& image) const;
};

} // namespace Artifact
