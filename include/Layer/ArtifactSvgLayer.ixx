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
#include <QJsonObject>
#include <QImage>
#include <QVariant>
#include <wobjectimpl.h>
export module Artifact.Layer.Svg;

import Artifact.Layer.Abstract;

export namespace Artifact {
using namespace ArtifactCore;

class ArtifactSvgLayer : public ArtifactAbstractLayer {
  W_OBJECT(ArtifactSvgLayer)
private:
  class Impl;
  Impl* impl_;

public:
  ArtifactSvgLayer();
  ~ArtifactSvgLayer();

  QImage toQImage() const;
  bool loadFromPath(const QString& path);
  QString sourcePath() const;
  bool isLoaded() const;

  void setFitToLayer(bool fit);
  bool fitToLayer() const;

  QJsonObject toJson() const;
  void fromJsonProperties(const QJsonObject& obj);

  std::vector<ArtifactCore::PropertyGroup> getLayerPropertyGroups() const override;
  bool setLayerPropertyValue(const QString& propertyPath, const QVariant& value) override;
  void draw(ArtifactIRenderer* renderer) override;
  QRectF localBounds() const override;

private:
  void drawVector(ArtifactIRenderer* renderer);
  void drawRaster(ArtifactIRenderer* renderer);
};

}
