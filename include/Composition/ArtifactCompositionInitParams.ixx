module;
#include <QSize>
export module Artifact.Composition.InitParams;

import std;
import Utils.String.UniString;
import Utils.Id;
import Size;
import Frame.Rate;
import Color.Float;
import Time.Code;
import Time.Rational;
import Core.AspectRatio;
import Preview.Quality;

import <cstdint>;

export namespace Artifact {

 using namespace ArtifactCore;
 using Size = ArtifactCore::Size_2D;

 // モーションブラー設定
 struct MotionBlurSettings {
  float shutterAngle = 180.0f;      // シャッターアングル（0-720度）
  float shutterPhase = -90.0f;      // シャッターフェーズ（-360～360度）
  int samplesPerFrame = 16;         // フレームあたりのサンプル数
  float adaptiveSampleLimit = 128;  // 適応サンプル上限
  bool enabled = false;             // モーションブラー有効
 };

 // 3Dレンダラー設定
 enum class Renderer3DType {
  Classic3D,
  Advanced3D,
  RayTraced
 };

 // 解像度ダウンサンプリング
 enum class ResolutionFactor {
  Full,        // 1:1
  Half,        // 1:2
  Third,       // 1:3
  Quarter,     // 1:4
  Custom
 };

 // 作業エリア（ワークエリア）
 struct WorkArea {
  RationalTime inPoint;
  RationalTime outPoint;
  bool enabled = true;
 };

 // Initialization parameters for an artifact composition
 class ArtifactCompositionInitParams {
 private:
  class Impl;
  Impl* impl_;

 public:
  // === コンストラクタ・デストラクタ ===
  ArtifactCompositionInitParams();
  explicit ArtifactCompositionInitParams(const UniString& name, const FloatColor& backgroundColor);
  ArtifactCompositionInitParams(const ArtifactCompositionInitParams& other);
  ArtifactCompositionInitParams(ArtifactCompositionInitParams&& other) noexcept;
  ArtifactCompositionInitParams& operator=(const ArtifactCompositionInitParams& other);
  ArtifactCompositionInitParams& operator=(ArtifactCompositionInitParams&& other) noexcept;
  ~ArtifactCompositionInitParams();

  // === 基本設定 ===
  
  // コンポジション名
  UniString compositionName() const;
  void setCompositionName(const UniString& name);

  // 解像度（幅・高さ）
  Size resolution() const;
  void setResolution(const Size& size);
  void setResolution(int width, int height);
  int width() const;
  int height() const;

  // ピクセルアスペクト比
  AspectRatio pixelAspectRatio() const;
  void setPixelAspectRatio(const AspectRatio& ratio);

  // フレームレート
  FrameRate frameRate() const;
  void setFrameRate(const FrameRate& rate);
  void setFrameRate(double fps);

  // === 時間設定 ===

  // デュレーション（長さ）
  RationalTime duration() const;
  void setDuration(const RationalTime& duration);
  void setDurationFrames(int64_t frames);
  void setDurationSeconds(double seconds);
  int64_t durationFrames() const;
  double durationSeconds() const;

  // 開始タイムコード
  TimeCode startTimeCode() const;
  void setStartTimeCode(const TimeCode& tc);

  // ワークエリア
  WorkArea workArea() const;
  void setWorkArea(const WorkArea& area);
  void setWorkArea(const RationalTime& inPoint, const RationalTime& outPoint);

  // === 表示設定 ===

  // 背景色
  FloatColor backgroundColor() const;
  void setBackgroundColor(const FloatColor& color);

  // プレビュー品質・解像度
  PreviewQuality previewQuality() const;
  void setPreviewQuality(const PreviewQuality& quality);
  ResolutionFactor resolutionFactor() const;
  void setResolutionFactor(ResolutionFactor factor);

  // === モーションブラー設定 ===

  MotionBlurSettings motionBlurSettings() const;
  void setMotionBlurSettings(const MotionBlurSettings& settings);
  bool motionBlurEnabled() const;
  void setMotionBlurEnabled(bool enabled);
  float shutterAngle() const;
  void setShutterAngle(float angle);
  float shutterPhase() const;
  void setShutterPhase(float phase);

  // === 3D設定 ===

  Renderer3DType renderer3D() const;
  void setRenderer3D(Renderer3DType type);

  // === プリセット ===

  static ArtifactCompositionInitParams hdPreset();       // 1920x1080, 30fps
  static ArtifactCompositionInitParams fullHd60Preset(); // 1920x1080, 60fps
  static ArtifactCompositionInitParams fourKPreset();    // 3840x2160, 30fps
  static ArtifactCompositionInitParams squarePreset();   // 1080x1080, 30fps (SNS)
  static ArtifactCompositionInitParams verticalPreset(); // 1080x1920, 30fps (縦動画)
  static ArtifactCompositionInitParams cinemaPreset();   // 2048x858, 24fps (シネスコ)

  // === バリデーション ===

  bool isValid() const;
  UniString validationError() const;

  // === 比較演算子 ===

  bool operator==(const ArtifactCompositionInitParams& other) const;
  bool operator!=(const ArtifactCompositionInitParams& other) const;
 };

}
