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
export module Artifact.Layer.Solid2D;




import Color.Float;
import Artifact.Layers.Abstract._2D;

export namespace Artifact
{
 using namespace ArtifactCore;
	
 class ArtifactSolid2DLayer:public ArtifactAbstract2DLayer
 {
 private:
  class Impl;
  Impl* impl_;
 public:
  ArtifactSolid2DLayer();
  ~ArtifactSolid2DLayer();
  FloatColor color() const;
  void setColor(const FloatColor& color);
  void setSize(int width, int height);
  QJsonObject toJson() const override;
  void fromJsonProperties(const QJsonObject& obj) override;
  std::vector<ArtifactCore::PropertyGroup> getLayerPropertyGroups() const override;
  bool setLayerPropertyValue(const QString& propertyPath, const QVariant& value) override;

  void draw(ArtifactIRenderer* renderer) override;

 };




}
