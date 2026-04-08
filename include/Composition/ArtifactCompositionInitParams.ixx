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
#include <cstdint>
#include <QSize>
export module Artifact.Composition.InitParams;




import Utils.String.UniString;
import Utils.Id;
import Size;
import Frame.Rate;
import Color.Float;
import Time.Code;
import Time.Rational;
import Core.AspectRatio;
import Preview.Quality;

export namespace Artifact {

 using namespace ArtifactCore;
 using Size = ArtifactCore::Size_2D;

 // [Vu[ݒ
 struct MotionBlurSettings {
  float shutterAngle = 180.0f;      // Vb^[AOi0-720xj
  float shutterPhase = -90.0f;      // Vb^[tF[Yi-360`360xj
  int samplesPerFrame = 16;         // t[̃Tv
  float adaptiveSampleLimit = 128;  // KTv
  bool enabled = false;             // [Vu[L
 };

 // 3D_[ݒ
 enum class Renderer3DType {
  Classic3D,
  Advanced3D,
  RayTraced
 };

 // 𑜓x_ETvO
 enum class ResolutionFactor {
  Full,        // 1:1
  Half,        // 1:2
  Third,       // 1:3
  Quarter,     // 1:4
  Custom
 };

 // ƃGAi[NGAj
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
  // === RXgN^EfXgN^ ===
  ArtifactCompositionInitParams();
  explicit ArtifactCompositionInitParams(const UniString& name, const FloatColor& backgroundColor);
  ArtifactCompositionInitParams(const ArtifactCompositionInitParams& other);
  ArtifactCompositionInitParams(ArtifactCompositionInitParams&& other) noexcept;
  ArtifactCompositionInitParams& operator=(const ArtifactCompositionInitParams& other);
  ArtifactCompositionInitParams& operator=(ArtifactCompositionInitParams&& other) noexcept;
  ~ArtifactCompositionInitParams();

  // === {ݒ ===
  
  // R|WV
  UniString compositionName() const;
  void setCompositionName(const UniString& name);

  // 𑜓xiEj
  Size resolution() const;
  void setResolution(const Size& size);
  void setResolution(int width, int height);
  int width() const;
  int height() const;

  // sNZAXyNg
  AspectRatio pixelAspectRatio() const;
  void setPixelAspectRatio(const AspectRatio& ratio);

  // t[[g
  FrameRate frameRate() const;
  void setFrameRate(const FrameRate& rate);
  void setFrameRate(double fps);

  // === Ԑݒ ===

  // f[Vij
  RationalTime duration() const;
  void setDuration(const RationalTime& duration);
  void setDurationFrames(int64_t frames);
  void setDurationSeconds(double seconds);
  int64_t durationFrames() const;
  double durationSeconds() const;

  // Jn^CR[h
  const TimeCode& startTimeCode() const;
  void setStartTimeCode(const TimeCode& tc);

  // [NGA
  WorkArea workArea() const;
  void setWorkArea(const WorkArea& area);
  void setWorkArea(const RationalTime& inPoint, const RationalTime& outPoint);

  // === \ݒ ===

  // wiF
  FloatColor backgroundColor() const;
  void setBackgroundColor(const FloatColor& color);

  // vr[iE𑜓x
  PreviewQuality previewQuality() const;
  void setPreviewQuality(const PreviewQuality& quality);
  ResolutionFactor resolutionFactor() const;
  void setResolutionFactor(ResolutionFactor factor);

  // === [Vu[ݒ ===

  MotionBlurSettings motionBlurSettings() const;
  void setMotionBlurSettings(const MotionBlurSettings& settings);
  bool motionBlurEnabled() const;
  void setMotionBlurEnabled(bool enabled);
  float shutterAngle() const;
  void setShutterAngle(float angle);
  float shutterPhase() const;
  void setShutterPhase(float phase);

  // === 3Dݒ ===

  Renderer3DType renderer3D() const;
  void setRenderer3D(Renderer3DType type);

  // === vZbg ===

  static ArtifactCompositionInitParams hdPreset();       // 1920x1080, 30fps
  static ArtifactCompositionInitParams fullHd60Preset(); // 1920x1080, 60fps
  static ArtifactCompositionInitParams fourKPreset();    // 3840x2160, 30fps
  static ArtifactCompositionInitParams squarePreset();   // 1080x1080, 30fps (SNS)
  static ArtifactCompositionInitParams verticalPreset(); // 1080x1920, 30fps (c)
  static ArtifactCompositionInitParams cinemaPreset();   // 2048x858, 24fps (VlXR)

  // === of[V ===

  bool isValid() const;
  UniString validationError() const;

  // === rZq ===

  bool operator==(const ArtifactCompositionInitParams& other) const;
  bool operator!=(const ArtifactCompositionInitParams& other) const;
 };

}
