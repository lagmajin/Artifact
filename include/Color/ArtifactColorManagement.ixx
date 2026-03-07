module;

#include <QString>
#include <QVector>
#include <QMatrix4x4>

export module Artifact.Color.Management;

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
#include <mutex>
#include <thread>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <variant>
#include <any>
#include <atomic>
#include <condition_variable>
#include <queue>
#include <deque>
#include <list>
#include <tuple>
#include <numeric>
#include <regex>
#include <random>




export namespace Artifact
{

// カラースペース
enum class ColorSpace
{
    Linear,         // リニア
    sRGB,           // sRGB
    Rec709,         // Rec.709 / Rec.1886
    Rec2020,        // Rec.2020 / Rec.2100 PQ
    P3,             // DCI-P3
    ACES_AP0,       // ACEScg
    ACES_AP1,       // ACEScct
};

// ガンマ関数
enum class GammaFunction
{
    Linear,
    sRGB,
    Gamma22,
    Gamma24,
    Gamma26,
    PQ,             // Perceptual Quantizer (HDR)
    HLG,            // Hybrid Log-Gamma (HDR)
};

// HDRダイナミックレンジ
enum class HDRMode
{
    SDR,            // Standard Dynamic Range
    HDR10,          // 1000 nits max
    HDR10Plus,      // 4000 nits max
    HLG,            // Hybrid Log-Gamma
    DolbyVision,    // Dolby Vision
};

// LUT类型
enum class LUTType
{
    IDT,            // Input Device Transform
    ADT,            // Appearance Device Transform
    ACESLook,       // ACES Look
    Cube1D,         // 1D LUT (.look, .cube)
    Cube3D,         // 3D LUT (.cube)
    CDL,            // Color Decision List
};

// カラー設定
class ColorSettings
{
private:
    class Impl;
    Impl* impl_;

public:
    ColorSettings();
    ~ColorSettings();

    // 作業カラースペース
    void setWorkingColorSpace(ColorSpace space);
    ColorSpace workingColorSpace() const;

    // 入力カラースペース（ソース）
    void setSourceColorSpace(ColorSpace space);
    ColorSpace sourceColorSpace() const;

    // 出力カラースペース（書き出し）
    void setOutputColorSpace(ColorSpace space);
    ColorSpace outputColorSpace() const;

    // ガンマ設定
    void setGammaFunction(GammaFunction gamma);
    GammaFunction gammaFunction() const;

    // HDR設定
    void setHDRMode(HDRMode mode);
    HDRMode hdrMode() const;

    void setMaxNits(float nits);  // 最大明るさ（nit）
    float maxNits() const;

    void setMinNits(float nits);  // 最小明るさ（nit）
    float minNits() const;

    // ビット深度
    void setBitDepth(int bits);
    int bitDepth() const;
};

// LUTファイル
class LUTData
{
private:
    class Impl;
    Impl* impl_;

public:
    LUTData();
    ~LUTData();

    bool loadFromFile(const QString& filePath);
    bool saveToFile(const QString& filePath) const;

    LUTType type() const;
    int size() const;  // LUTサイズ（例：33, 65, 257）

    // LUT適用（ピクセルごとに呼叫）
    QVector3D apply(float r, float g, float b) const;

    // 3D LUTデータ直接アクセス
    const float* data3D() const;
    int dataSize() const;

    // 1D LUTデータ
    const float* data1D() const;
    bool is1D() const;
};

// LUTエフェクト（レイヤー適用用）
class ColorLUTEffect
{
private:
    class Impl;
    Impl* impl_;

public:
    ColorLUTEffect();
    ~ColorLUTEffect();

    void setLUT(std::shared_ptr<LUTData> lut);
    std::shared_ptr<LUTData> lut() const;

    void setIntensity(float intensity);  // 0.0 - 1.0
    float intensity() const;

    void setInputGamma(GammaFunction gamma);
    GammaFunction inputGamma() const;

    void setOutputGamma(GammaFunction gamma);
    GammaFunction outputGamma() const;

    // 適用
    QVector3D transform(float r, float g, float b) const;
};

// カラーマネージャー（グローバル設定）
class ColorManager
{
private:
    class Impl;
    Impl* impl_;

public:
    static ColorManager& instance();

    ColorManager();
    ~ColorManager();

    // グローバル設定
    ColorSettings* settings();
    const ColorSettings* settings() const;

    // カラースペース変換行列取得
    QMatrix4x4 getConversionMatrix(ColorSpace from, ColorSpace to) const;

    // ガンマ適用
    float applyGamma(float value, GammaFunction gamma) const;
    float removeGamma(float value, GammaFunction gamma) const;

    // HDRメタデータ
    void setHDRMetadata(float maxCll, float maxFall, float avgBrightness);
    float maxContentLightLevel() const;
    float maxFrameAverageLightLevel() const;
    float averageBrightness() const;

    // ワークスペース設定
    void setWorkingSpace(ColorSpace space);
    ColorSpace workingSpace() const;

Q_SIGNALS:
    void colorSpaceChanged(ColorSpace space);
    void hdrModeChanged(HDRMode mode);

private:
    ColorManager(const ColorManager&) = delete;
    ColorManager& operator=(const ColorManager&) = delete;
};

} // namespace Artifact
