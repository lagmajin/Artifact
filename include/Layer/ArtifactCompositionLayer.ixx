module;

#include <algorithm>
#include <any>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <deque>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <list>
#include <map>
#include <memory>
#include <numeric>
#include <optional>
#include <queue>
#include <random>
#include <regex>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <variant>
#include <vector>
#include <QVariant>
export module Artifact.Layer.Composition;


import Utils;
import Artifact.Layer.Abstract;
import Artifact.Composition.Abstract;

export namespace Artifact {

class ArtifactCompositionLayer : public ArtifactAbstractLayer {
private:
  class Impl;
  Impl *impl_;

public:
  ArtifactCompositionLayer();
  ~ArtifactCompositionLayer();
  CompositionID sourceCompositionId() const;
  void setCompositionId(const CompositionID &id);

  std::shared_ptr<ArtifactAbstractComposition> sourceComposition() const;

  // ArtifactAbstractLayer overrides
  void draw(ArtifactIRenderer *renderer) override;
  QRectF localBounds() const override;
  std::vector<ArtifactCore::PropertyGroup>
  getLayerPropertyGroups() const override;
  bool setLayerPropertyValue(const QString &propertyPath,
                             const QVariant &value) override;
};

} // namespace Artifact
