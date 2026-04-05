module;
#include <QString>
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
export module Artifact.Layer.InitParams;





import Utils.String.Like;
import Utils.String.UniString;
import Artifact.Layer.Abstract;
import Color.Float;

export namespace Artifact {


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
 };

 class ArtifactTextLayerInitParams : public ArtifactLayerInitParams
 {
 public:
  explicit ArtifactTextLayerInitParams(const QString& name);
  ~ArtifactTextLayerInitParams() = default;
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

 public:
  ArtifactImageInitParams(const QString& name);
  ~ArtifactImageInitParams();
  QString imagePath() const { return imagePath_; }
  void setImagePath(const QString& path) { imagePath_ = path; }
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

};

W_REGISTER_ARGTYPE(Artifact::ArtifactSolidLayerInitParams)
