module;

#include <QString>
#include <QVector>
#include <vector>
#include <cmath>
#include <algorithm>

module Composition.MetadataVectorizer;

import std;
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

  // メタデータの抽出と正規化
  // 注: 実装時にArtifactAbstractCompositionのAPIを確認して適切に抽出してください

  // 仮の実装
  components.duration = 0.5f;       // 正規化値（0-1）
  components.frameRate = impl_->normalizeValue(30.0f, 120.0f, 1.0f);  // 1-120 fpsの範囲で正規化
  components.width = impl_->normalizeValue(1920.0f, 4096.0f, 320.0f);  // 320-4096pxの範囲で正規化
  components.height = impl_->normalizeValue(1080.0f, 2160.0f, 240.0f); // 240-2160pxの範囲で正規化
  components.layerCount = impl_->normalizeValue(5.0f, 100.0f, 0.0f);   // 0-100レイヤーの範囲で正規化
  components.keyframeCount = impl_->normalizeValue(20.0f, 1000.0f, 0.0f); // 0-1000キーフレームの範囲で正規化
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
