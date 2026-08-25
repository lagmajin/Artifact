module;
#include <wobjectdefs.h>
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
#include <QStringList>

export module Artifact.Layer.InitParams;





import Utils.String.Like;
import Utils.String.UniString;
import Artifact.Layer.Abstract;
import Artifact.Layers.Model3D;
import Color.Float;
import ImageProcessing.ProceduralTexture;

export namespace Artifact {

 enum class ArtifactSolidFillType {
  Solid = 0,
  LinearGradient = 1,
  RadialGradient = 2,
  ConicalGradient = 3,
  RepeatingGradient = 4,
  MirroredGradient = 5
 };


 class ArtifactLayerInitParams
 {
 private:
  class Impl;
  Impl* impl_;
 public:
  ArtifactLayerInitParams(const QString& name, LayerType type);
  ArtifactLayerInitParams(const UniString& name, LayerType type);
  ArtifactLayerInitParams(const ArtifactLayerInitParams& other);
  ArtifactLayerInitParams(ArtifactLayerInitParams&& other) noexcept;
  ArtifactLayerInitParams& operator=(const ArtifactLayerInitParams& other);
  ArtifactLayerInitParams& operator=(ArtifactLayerInitParams&& other) noexcept;
  virtual ~ArtifactLayerInitParams();
  LayerType layerType() const;
  UniString name() const;
  void setName(const UniString& name);
 };



 class ArtifactSolidLayerInitParams : public ArtifactLayerInitParams
 {
 private:
  class Impl;
  Impl* impl_;
 public:
  explicit ArtifactSolidLayerInitParams(const QString& name);
  ArtifactSolidLayerInitParams(const ArtifactSolidLayerInitParams& other);
  ArtifactSolidLayerInitParams(ArtifactSolidLayerInitParams&& other) noexcept;
  ArtifactSolidLayerInitParams& operator=(const ArtifactSolidLayerInitParams& other);
  ArtifactSolidLayerInitParams& operator=(ArtifactSolidLayerInitParams&& other) noexcept;
  ~ArtifactSolidLayerInitParams();
  
  int width() const;
  void setWidth(int width);
  int height() const;
  void setHeight(int height);
  FloatColor color() const;
  void setColor(const FloatColor& color);
  ArtifactSolidFillType fillType() const;
  void setFillType(ArtifactSolidFillType fillType);
  FloatColor gradientStartColor() const;
  void setGradientStartColor(const FloatColor& color);
  FloatColor gradientEndColor() const;
  void setGradientEndColor(const FloatColor& color);
  float gradientAngleDegrees() const;
  void setGradientAngleDegrees(float degrees);
  bool gradientReverse() const;
  void setGradientReverse(bool reverse);
  float gradientCenterX() const;
  void setGradientCenterX(float value);
  float gradientCenterY() const;
  void setGradientCenterY(float value);
  float gradientScale() const;
  void setGradientScale(float value);
  float gradientOffset() const;
  void setGradientOffset(float value);
 };

 class ArtifactTextLayerInitParams : public ArtifactLayerInitParams
 {
 public:
  explicit ArtifactTextLayerInitParams(const QString& name);
  ~ArtifactTextLayerInitParams() = default;
 };

 class ArtifactNoiseLayerInitParams : public ArtifactLayerInitParams
 {
 private:
  class Impl;
  Impl* impl_;
 public:
  explicit ArtifactNoiseLayerInitParams(const QString& name);
  ArtifactNoiseLayerInitParams(const ArtifactNoiseLayerInitParams& other);
  ArtifactNoiseLayerInitParams(ArtifactNoiseLayerInitParams&& other) noexcept;
  ArtifactNoiseLayerInitParams& operator=(const ArtifactNoiseLayerInitParams& other);
  ArtifactNoiseLayerInitParams& operator=(ArtifactNoiseLayerInitParams&& other) noexcept;
  ~ArtifactNoiseLayerInitParams();
  int width() const;
  void setWidth(int width);
  int height() const;
  void setHeight(int height);
  std::uint32_t seed() const;
  void setSeed(std::uint32_t seed);
  ArtifactCore::ProceduralTextureGeneratorKind kind() const;
  void setKind(ArtifactCore::ProceduralTextureGeneratorKind kind);
  bool hasPreset() const;
  ArtifactCore::ProceduralTexturePreset preset() const;
  void setPreset(ArtifactCore::ProceduralTexturePreset preset);
 };

 class ArtifactNullLayerInitParams : public ArtifactLayerInitParams
 {
 private:
  class Impl;
  Impl* impl_;
 public:
  explicit ArtifactNullLayerInitParams(const QString& name);
  ArtifactNullLayerInitParams(const ArtifactNullLayerInitParams& other);
  ArtifactNullLayerInitParams(ArtifactNullLayerInitParams&& other) noexcept;
  ArtifactNullLayerInitParams& operator=(const ArtifactNullLayerInitParams& other);
  ArtifactNullLayerInitParams& operator=(ArtifactNullLayerInitParams&& other) noexcept;
  ~ArtifactNullLayerInitParams();
  int width() const;
  void setWidth(int width);
  int height() const;
  void setHeight(int height);
 };

 class ArtifactImageInitParams :public ArtifactLayerInitParams
 {
 private:
  QString imagePath_;
  QStringList sequencePaths_;
  double sequenceFrameRate_ = 0.0;
  int psdSubimageIndex_ = -1;
  QString inputColorSpace_;
  QString inputTransferFunction_;

 public:
  ArtifactImageInitParams(const QString& name);
  ~ArtifactImageInitParams();
  QString imagePath() const { return imagePath_; }
  void setImagePath(const QString& path) { imagePath_ = path; }
  QStringList sequencePaths() const { return sequencePaths_; }
  void setSequencePaths(const QStringList& paths) { sequencePaths_ = paths; }
  double sequenceFrameRate() const { return sequenceFrameRate_; }
  void setSequenceFrameRate(double frameRate) { sequenceFrameRate_ = frameRate; }
  int psdSubimageIndex() const { return psdSubimageIndex_; }
  void setPsdSubimageIndex(int index) { psdSubimageIndex_ = index < 0 ? -1 : index; }
  QString inputColorSpace() const { return inputColorSpace_; }
  void setInputColorSpace(const QString& value) { inputColorSpace_ = value; }
  QString inputTransferFunction() const { return inputTransferFunction_; }
  void setInputTransferFunction(const QString& value) { inputTransferFunction_ = value; }
 };

 class ArtifactSvgInitParams : public ArtifactLayerInitParams
 {
 private:
  QString svgPath_;

 public:
  explicit ArtifactSvgInitParams(const QString& name);
  ~ArtifactSvgInitParams();
  QString svgPath() const { return svgPath_; }
  void setSvgPath(const QString& path) { svgPath_ = path; }
 };

 class ArtifactAudioInitParams :public ArtifactLayerInitParams
 {
 private:
  QString audioPath_;

 public:
  ArtifactAudioInitParams(const QString& name);
  ~ArtifactAudioInitParams();
  QString audioPath() const { return audioPath_; }
  void setAudioPath(const QString& path) { audioPath_ = path; }
 };

 class ArtifactVideoInitParams : public ArtifactLayerInitParams
 {
 private:
  QString videoPath_;

 public:
  explicit ArtifactVideoInitParams(const QString& name);
  ~ArtifactVideoInitParams();
  QString videoPath() const { return videoPath_; }
  void setVideoPath(const QString& path) { videoPath_ = path; }
 };


 class ArtifactCameraLayerInitParams :public ArtifactLayerInitParams
 {
 private:

 public:
  ArtifactCameraLayerInitParams();
  ~ArtifactCameraLayerInitParams();
 };

 class ArtifactCompositionLayerInitParams :public ArtifactLayerInitParams {
 private:

 public:
  ArtifactCompositionLayerInitParams();
  ~ArtifactCompositionLayerInitParams();
 };

 class ArtifactCompositionBackgroundLayerInitParams :public ArtifactLayerInitParams {
 private:

 public:
  ArtifactCompositionBackgroundLayerInitParams();
  ~ArtifactCompositionBackgroundLayerInitParams();
 };

 class ArtifactModel3DLayerInitParams : public ArtifactLayerInitParams {
 private:
  QString modelPath_;

 public:
  explicit ArtifactModel3DLayerInitParams(const QString& name);
  ~ArtifactModel3DLayerInitParams();
  QString modelPath() const { return modelPath_; }
  void setModelPath(const QString& path) { modelPath_ = path; }
 };

 class ArtifactFixedGeometry3DLayerInitParams : public ArtifactLayerInitParams {
 private:
  FixedGeometry3D geometry_ = FixedGeometry3D::Cube;

 public:
  explicit ArtifactFixedGeometry3DLayerInitParams(const QString& name, FixedGeometry3D geometry);
  ~ArtifactFixedGeometry3DLayerInitParams();
  FixedGeometry3D geometry() const;
  void setGeometry(FixedGeometry3D geometry);
 };

 class ArtifactParametricCompositionLayerInitParams : public ArtifactLayerInitParams {
 private:

 public:
  ArtifactParametricCompositionLayerInitParams();
  ~ArtifactParametricCompositionLayerInitParams();
 };

};

W_REGISTER_ARGTYPE(Artifact::ArtifactSolidLayerInitParams)
