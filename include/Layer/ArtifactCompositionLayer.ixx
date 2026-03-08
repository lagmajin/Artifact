module;

#include <QVariant>

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
export module Artifact.Layer.Composition;

import Utils;
import Artifact.Layer.Abstract;

export namespace Artifact {

 class ArtifactCompositionLayer:public ArtifactAbstractLayer {
 private:
  class Impl;
  Impl* impl_;
 public:
  ArtifactCompositionLayer();
  ~ArtifactCompositionLayer();
  CompositionID sourceCompositionId() const;
  void setCompositionId(const CompositionID& id);
  std::vector<ArtifactCore::PropertyGroup> getLayerPropertyGroups() const override;
  bool setLayerPropertyValue(const QString& propertyPath, const QVariant& value) override;
 };

}
