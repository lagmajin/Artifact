module;

#include <iostream>
#include <cstdint>
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
#include <QJsonObject>
#include <QImage>
#include <QVariant>
#include <wobjectimpl.h>
export module Artifact.Layer.Svg;

import Artifact.Layers.Abstract._2D;
import Image.ImageF32x4_RGBA;

export namespace Artifact {
using namespace ArtifactCore;

class ArtifactSvgLayer : public ArtifactAbstract2DLayer {
  W_OBJECT(ArtifactSvgLayer)
private:
  class Impl;
  Impl* impl_;

public:
  ArtifactSvgLayer();
  ~ArtifactSvgLayer();

  QImage toQImage() const;
  QImage getThumbnail(int width = 128, int height = 128) const override;
  const ArtifactCore::ImageF32x4_RGBA& currentFrameBuffer() const;
  bool hasCurrentFrameBuffer() const;
  bool loadFromPath(const QString& path);
  QString sourcePath() const;
  std::uint64_t sourceVersion() const;
  bool isLoaded() const;

  void setFitToLayer(bool fit);
  bool fitToLayer() const;

  QJsonObject toJson() const override;
  void fromJsonProperties(const QJsonObject& obj) override;

  std::vector<ArtifactCore::PropertyGroup> getLayerPropertyGroups() const override;
  bool setLayerPropertyValue(const QString& propertyPath, const QVariant& value) override;
  void draw(ArtifactIRenderer* renderer) override;
  QRectF localBounds() const override;

private:
  void drawRaster(ArtifactIRenderer* renderer);
};

}
