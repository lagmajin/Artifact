module;

#include <QString>
#include <QSize>
#include <cstdint>
#include <limits>
#include <QVector>
#include <vector>
#include <cmath>
#include <algorithm>

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
module Composition.MetadataVectorizer;




import Artifact.Composition.Abstract;

namespace Artifact {

 class CompositionMetadataVectorizer::Impl {
 public:
  int maxVectorSize = 256;

  Impl() {}
  ~Impl() {}

  // メタデータの正規化ヘルパー
  float normalizeValue(float value, float maxValue, float minValue = 0.0f) const {
   if (maxValue == minValue) return 0.0f;
   float normalized = (value - minValue) / (maxValue - minValue);
   return std::clamp(normalized, 0.0f, 1.0f);
  }

  // 複雑度スコアを計算
  float calculateComplexity(float layerCount, float keyframeCount, float duration, float avgSize) const {
   float complexity = 0.0f;
   complexity += layerCount * 0.3f;
   complexity += keyframeCount * 0.3f;
   complexity += duration * 0.2f;
   complexity += avgSize * 0.2f;
   return std::clamp(complexity, 0.0f, 1.0f);
  }
 };

 CompositionMetadataVectorizer::CompositionMetadataVectorizer()
  : impl_(new Impl()) {
 }

 CompositionMetadataVectorizer::~CompositionMetadataVectorizer() {
  delete impl_;
 }

 std::vector<float> CompositionMetadataVectorizer::vectorizeMetadata(
  const ArtifactAbstractComposition* composition) const {
  if (!composition) {
   return std::vector<float>();
  }

  auto components = extractMetadataComponents(composition);

  std::vector<float> vector;
  vector.push_back(components.duration);
  vector.push_back(components.frameRate);
  vector.push_back(components.width);
  vector.push_back(components.height);
  vector.push_back(components.layerCount);
  vector.push_back(components.keyframeCount);
  vector.push_back(components.complexity);

  // 指定されたサイズにパディング
  if (vector.size() < static_cast<size_t>(impl_->maxVectorSize)) {
   vector.resize(impl_->maxVectorSize, 0.0f);
  }

  return vector;
 }

 std::vector<float> CompositionMetadataVectorizer::vectorizeMetadataWithSize(
  const ArtifactAbstractComposition* composition, int targetSize) const {
  auto vector = vectorizeMetadata(composition);

  if (static_cast<int>(vector.size()) != targetSize) {
   vector.resize(targetSize, 0.0f);
  }

  return vector;
 }

 CompositionMetadataVectorizer::MetadataComponents CompositionMetadataVectorizer::extractMetadataComponents(
  const ArtifactAbstractComposition* composition) const {
  MetadataComponents components;

  if (!composition) {
   return components;
  }

  const QSize size = composition->effectiveCompositionSize();
  const float fps = std::max(1.0f, composition->frameRate().framerate());
  const auto range = composition->frameRange();
  const float frameCount = static_cast<float>(std::max<int64_t>(0, range.frameCount()));

  // Compositionの実データから正規化する。取得できない値を固定値で捏造しない。
  components.duration = impl_->normalizeValue(frameCount / fps, 300.0f, 0.0f);
  components.frameRate = impl_->normalizeValue(fps, 120.0f, 1.0f);
  components.width = impl_->normalizeValue(static_cast<float>(size.width()), 4096.0f, 320.0f);
  components.height = impl_->normalizeValue(static_cast<float>(size.height()), 2160.0f, 240.0f);
  components.layerCount = impl_->normalizeValue(static_cast<float>(composition->layerCount()), 100.0f, 0.0f);
  std::size_t keyframeCount = 0;
  for (const auto& layer : composition->allLayer()) {
   if (!layer) continue;
   for (const auto& group : layer->getLayerPropertyGroups()) {
    for (const auto& property : group.allProperties()) {
     if (!property) continue;
     const auto frames = property->getKeyFrames();
     if (frames.size() > std::numeric_limits<std::size_t>::max() - keyframeCount) {
      keyframeCount = std::numeric_limits<std::size_t>::max();
      break;
     }
     keyframeCount += frames.size();
    }
   }
  }
  components.keyframeCount = impl_->normalizeValue(
   static_cast<float>(std::min<std::size_t>(keyframeCount, 1000)), 1000.0f, 0.0f);
  components.complexity = impl_->calculateComplexity(
   components.layerCount,
   components.keyframeCount,
   components.duration,
   (components.width + components.height) / 2.0f
  );

  return components;
 }

 void CompositionMetadataVectorizer::setMaxVectorSize(int size) {
  if (size > 0) {
   impl_->maxVectorSize = size;
  }
 }

 int CompositionMetadataVectorizer::getMaxVectorSize() const {
  return impl_->maxVectorSize;
 }

}
