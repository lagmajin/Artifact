module;
#include <QVector>
#include <QMatrix4x4>
#include <QString>
#include <QVector3D>
#include <cmath>
#include <random>
#include <memory>

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
module Generator.Clone;




import Artifact.Effect.Clone.Core; // MoGraphコアをインポート
import Generator.DistributionModes;

namespace Artifact
{
 class CloneGenerator::Impl
 {
 public:
  // 基本
  int count_ = 1;
  float spacing_ = 100.0f;
  QString prototypeName_;

  // 分布モード
  DistributionMode mode_ = DistributionMode::Linear;
  TransformSpace space_ = TransformSpace::Local;

  // Radial
  float radius_ = 500.0f;

  // Grid
  int gridColumns_ = 5;
  int gridRows_ = 5;
  int gridDepth_ = 5;
  float gridSpacingX_ = 100.0f;
  float gridSpacingY_ = 100.0f;
  float gridSpacingZ_ = 100.0f;

  // Random/Noise
  int randomSeed_ = 1234;
  float variation_ = 0.5f;
  QVector3D bounds_ = QVector3D(1000, 1000, 0);
  bool usePoissonDisk_ = false;

  // Spiral
  float spiralRotations_ = 3.0f;

  // Transform
  QVector3D offset_ = QVector3D(0, 0, 0);
  float rotationStep_ = 0.0f;

  // Spline
  std::shared_ptr<SimpleSpline> spline_;

  // ヘルパー: 線形補間
  float lerp(float a, float b, float t) const { return a + (b - a) * t; }

  // ヘルパー: 度をラジアンに
  float degToRad(float deg) const { return deg * static_cast<float>(M_PI) / 180.0f; }

  // 1D noise (simple hash-based)
  float noise1D(float x) const
  {
   int ix = static_cast<int>(std::floor(x));
   float fx = x - ix;
   float u = fx * fx * (3.0f - 2.0f * fx);

   auto hash = [](int n) -> float
   {
    n = (n << 13) ^ n;
    return (1.0f - ((n * (n * n * 15731 + 789221) + 1376312589) & 0x7fffffff) / 1073741824.0f);
   };

   return lerp(hash(ix), hash(ix + 1), u);
  }
 };

 CloneGenerator::CloneGenerator() :impl_(new Impl())
 {

 }

 CloneGenerator::~CloneGenerator()
 {
  delete impl_;
 }

 // 基本パラメータ
 void CloneGenerator::setCount(int count)
 {
  if (count < 1) count = 1;
  impl_->count_ = count;
 }

 int CloneGenerator::count() const
 {
  return impl_->count_;
 }

 void CloneGenerator::setSpacing(float spacing)
 {
  impl_->spacing_ = spacing;
 }

 float CloneGenerator::spacing() const
 {
  return impl_->spacing_;
 }

 void CloneGenerator::setPrototypeName(const QString& name)
 {
  impl_->prototypeName_ = name;
 }

 QString CloneGenerator::prototypeName() const
 {
  return impl_->prototypeName_;
 }

 // 分布モード
 void CloneGenerator::setDistributionMode(DistributionMode mode)
 {
  impl_->mode_ = mode;
 }

 DistributionMode CloneGenerator::distributionMode() const
 {
  return impl_->mode_;
 }

 void CloneGenerator::setTransformSpace(TransformSpace space)
 {
  impl_->space_ = space;
 }

 TransformSpace CloneGenerator::transformSpace() const
 {
  return impl_->space_;
 }

 // Radial
 void CloneGenerator::setRadius(float radius)
 {
  impl_->radius_ = radius;
 }

 float CloneGenerator::radius() const
 {
  return impl_->radius_;
 }

 // Grid
 void CloneGenerator::setGridColumns(int cols)
 {
  impl_->gridColumns_ = std::max(1, cols);
 }

 int CloneGenerator::gridColumns() const
 {
  return impl_->gridColumns_;
 }

 void CloneGenerator::setGridRows(int rows)
 {
  impl_->gridRows_ = std::max(1, rows);
 }

 int CloneGenerator::gridRows() const
 {
  return impl_->gridRows_;
 }

 void CloneGenerator::setGridDepth(int depth)
 {
  impl_->gridDepth_ = std::max(1, depth);
 }

 int CloneGenerator::gridDepth() const
 {
  return impl_->gridDepth_;
 }

 void CloneGenerator::setGridSpacingX(float spacing)
 {
  impl_->gridSpacingX_ = spacing;
 }

 float CloneGenerator::gridSpacingX() const
 {
  return impl_->gridSpacingX_;
 }

 void CloneGenerator::setGridSpacingY(float spacing)
 {
  impl_->gridSpacingY_ = spacing;
 }

 float CloneGenerator::gridSpacingY() const
 {
  return impl_->gridSpacingY_;
 }

 void CloneGenerator::setGridSpacingZ(float spacing)
 {
  impl_->gridSpacingZ_ = spacing;
 }

 float CloneGenerator::gridSpacingZ() const
 {
  return impl_->gridSpacingZ_;
 }

 // Random/Noise
 void CloneGenerator::setRandomSeed(int seed)
 {
  impl_->randomSeed_ = seed;
 }

 int CloneGenerator::randomSeed() const
 {
  return impl_->randomSeed_;
 }

 void CloneGenerator::setVariation(float variation)
 {
  impl_->variation_ = std::max(0.0f, std::min(1.0f, variation));
 }

 float CloneGenerator::variation() const
 {
  return impl_->variation_;
 }

 void CloneGenerator::setBounds(const QVector3D& bounds)
 {
  impl_->bounds_ = bounds;
 }

 QVector3D CloneGenerator::bounds() const
 {
  return impl_->bounds_;
 }

 void CloneGenerator::setUsePoissonDisk(bool use)
 {
  impl_->usePoissonDisk_ = use;
 }

 bool CloneGenerator::usePoissonDisk() const
 {
  return impl_->usePoissonDisk_;
 }

 // Spiral
 void CloneGenerator::setSpiralRotations(float rotations)
 {
  impl_->spiralRotations_ = rotations;
 }

 float CloneGenerator::spiralRotations() const
 {
  return impl_->spiralRotations_;
 }

 // Transform
 void CloneGenerator::setOffset(const QVector3D& offset)
 {
  impl_->offset_ = offset;
 }

 QVector3D CloneGenerator::offset() const
 {
  return impl_->offset_;
 }

 void CloneGenerator::setRotationStep(float degrees)
 {
  impl_->rotationStep_ = degrees;
 }

 float CloneGenerator::rotationStep() const
 {
  return impl_->rotationStep_;
 }

 // Spline
 void CloneGenerator::setSpline(std::shared_ptr<SimpleSpline> spline)
 {
  impl_->spline_ = spline;
 }

 std::shared_ptr<SimpleSpline> CloneGenerator::spline() const
 {
  return impl_->spline_;
 }

 QVector<QMatrix4x4> CloneGenerator::generateTransforms() const
 {
  QVector<QMatrix4x4> transforms;
  const int count = impl_->count_;
  transforms.reserve(count);

  switch (impl_->mode_)
  {
   case DistributionMode::Linear:
   {
    for (int i = 0; i < count; ++i) {
     QMatrix4x4 m;
     m.translate(i * impl_->spacing_, 0.0f, 0.0f);
     m.rotate(i * impl_->rotationStep_, 0.0f, 0.0f, 1.0f);
     m.translate(impl_->offset_.x(), impl_->offset_.y(), impl_->offset_.z());
     transforms.push_back(m);
    }
    break;
   }

   case DistributionMode::Radial:
   {
    float angleStep = 360.0f / count;
    for (int i = 0; i < count; ++i) {
     float angle = angleStep * i;
     float rad = impl_->degToRad(angle);

     QMatrix4x4 m;
     m.rotate(angle, 0.0f, 0.0f, 1.0f);
     m.translate(impl_->radius_, 0.0f, 0.0f);
     m.rotate(i * impl_->rotationStep_, 0.0f, 0.0f, 1.0f);
     m.translate(impl_->offset_.x(), impl_->offset_.y(), impl_->offset_.z());
     transforms.push_back(m);
    }
    break;
   }

   case DistributionMode::Grid2D:
   {
    int cols = impl_->gridColumns_;
    int rows = impl_->gridRows_;
    float offsetX = (cols - 1) * impl_->gridSpacingX_ / 2.0f;
    float offsetY = (rows - 1) * impl_->gridSpacingY_ / 2.0f;

    for (int row = 0; row < rows; ++row) {
     for (int col = 0; col < cols; ++col) {
      QMatrix4x4 m;
      m.translate(
       col * impl_->gridSpacingX_ - offsetX + impl_->offset_.x(),
       row * impl_->gridSpacingY_ - offsetY + impl_->offset_.y(),
       impl_->offset_.z()
      );
      transforms.push_back(m);
     }
    }
    break;
   }

   case DistributionMode::Grid3D:
   {
    int cols = impl_->gridColumns_;
    int rows = impl_->gridRows_;
    int depth = impl_->gridDepth_;
    float offsetX = (cols - 1) * impl_->gridSpacingX_ / 2.0f;
    float offsetY = (rows - 1) * impl_->gridSpacingY_ / 2.0f;
    float offsetZ = (depth - 1) * impl_->gridSpacingZ_ / 2.0f;

    for (int z = 0; z < depth; ++z) {
     for (int row = 0; row < rows; ++row) {
      for (int col = 0; col < cols; ++col) {
       QMatrix4x4 m;
       m.translate(
        col * impl_->gridSpacingX_ - offsetX + impl_->offset_.x(),
        row * impl_->gridSpacingY_ - offsetY + impl_->offset_.y(),
        z * impl_->gridSpacingZ_ - offsetZ + impl_->offset_.z()
       );
       transforms.push_back(m);
      }
     }
    }
    break;
   }

   case DistributionMode::Spiral:
   {
    for (int i = 0; i < count; ++i) {
     float t = static_cast<float>(i) / std::max(1, count - 1);
     float angle = t * impl_->spiralRotations_ * 360.0f;
     float currentRadius = impl_->radius_ * (1.0f - t * 0.5f);
     float currentHeight = impl_->offset_.z() + t * impl_->spacing_ * 10.0f;

     QMatrix4x4 m;
     m.rotate(angle, 0.0f, 0.0f, 1.0f);
     m.translate(currentRadius, 0.0f, currentHeight);
     m.rotate(i * impl_->rotationStep_, 0.0f, 0.0f, 1.0f);
     transforms.push_back(m);
    }
    break;
   }

   case DistributionMode::Hexagonal:
   {
    int rings = static_cast<int>(std::sqrt(count)) + 1;
    float hexRadius = impl_->spacing_ / std::sqrt(3.0f);

    int idx = 0;
    for (int ring = 0; ring <= rings && idx < count; ++ring) {
     int pointsInRing = (ring == 0) ? 1 : ring * 6;
     for (int i = 0; i < pointsInRing && idx < count; ++i) {
      float angle = i * 360.0f / pointsInRing;
      if (ring % 2 == 1) angle += 180.0f / 6.0f;

      float r = ring * impl_->spacing_;
      float rad = impl_->degToRad(angle);

      QMatrix4x4 m;
      m.rotate(angle, 0.0f, 0.0f, 1.0f);
      m.translate(r, 0.0f, 0.0f);
      m.translate(impl_->offset_.x(), impl_->offset_.y(), impl_->offset_.z());
      transforms.push_back(m);
      idx++;
     }
    }
    break;
   }

   case DistributionMode::Random:
   {
    std::mt19937 rng(impl_->randomSeed_);
    std::uniform_real_distribution<float> distX(-impl_->bounds_.x() / 2, impl_->bounds_.x() / 2);
    std::uniform_real_distribution<float> distY(-impl_->bounds_.y() / 2, impl_->bounds_.y() / 2);
    std::uniform_real_distribution<float> distZ(-impl_->bounds_.z() / 2, impl_->bounds_.z() / 2);
    std::uniform_real_distribution<float> distRot(0.0f, 360.0f);
    std::uniform_real_distribution<float> distScale(0.5f, 1.5f);

    for (int i = 0; i < count; ++i) {
     QMatrix4x4 m;
     m.translate(
      distX(rng) + impl_->offset_.x(),
      distY(rng) + impl_->offset_.y(),
      distZ(rng) + impl_->offset_.z()
     );

     // Variation: ランダム回転・スケール
     if (impl_->variation_ > 0.0f) {
      float rot = distRot(rng) * impl_->variation_;
      float scale = 1.0f + (distScale(rng) - 1.0f) * impl_->variation_;
      m.rotate(rot, 0.0f, 0.0f, 1.0f);
      m.scale(scale);
     }

     transforms.push_back(m);
    }
    break;
   }

   case DistributionMode::Noise:
   {
    for (int i = 0; i < count; ++i) {
     // ノイズベースの配置
     float nx = impl_->noise1D(i * 0.1f + impl_->randomSeed_) * impl_->bounds_.x() / 2;
     float ny = impl_->noise1D(i * 0.1f + impl_->randomSeed_ + 1000) * impl_->bounds_.y() / 2;
     float nz = impl_->noise1D(i * 0.1f + impl_->randomSeed_ + 2000) * impl_->bounds_.z() / 2;

     QMatrix4x4 m;
     m.translate(nx + impl_->offset_.x(), ny + impl_->offset_.y(), nz + impl_->offset_.z());
     transforms.push_back(m);
    }
    break;
   }

   case DistributionMode::Spline:
   {
    if (impl_->spline_ && impl_->spline_->pointCount() > 1) {
     for (int i = 0; i < count; ++i) {
      float t = static_cast<float>(i) / std::max(1, count - 1);
      auto point = impl_->spline_->getPoint(t);

      QMatrix4x4 m;
      m.translate(
       point.position.x() + impl_->offset_.x(),
       point.position.y() + impl_->offset_.y(),
       point.position.z() + impl_->offset_.z()
      );

      // 接線方向に ориентация
      if (point.tangent.length() > 0.001f) {
       QVector3D tangent(point.tangent.x(), point.tangent.y(), point.tangent.z());
       tangent.normalize();
       QVector3D up(0, 1, 0);
       if (std::abs(QVector3D::dotProduct(tangent, up)) > 0.99f) {
        up = QVector3D(1, 0, 0);
       }
       const QVector3D rightCross = QVector3D::crossProduct(up, tangent);
       QVector3D right(rightCross.x(), rightCross.y(), rightCross.z());
       right.normalize();
       const QVector3D upCross = QVector3D::crossProduct(tangent, right);
       up = QVector3D(upCross.x(), upCross.y(), upCross.z());
       up.normalize();

       QMatrix4x4 orient;
       orient.setColumn(0, QVector4D(right, 0));
       orient.setColumn(1, QVector4D(up, 0));
       orient.setColumn(2, QVector4D(tangent, 0));
       orient.setColumn(3, QVector4D(0, 0, 0, 1));

       m = orient * m;
      }

      m.rotate(i * impl_->rotationStep_, 0.0f, 0.0f, 1.0f);
      transforms.push_back(m);
     }
    } else {
     // Splineがない場合はLinearに戻る
     for (int i = 0; i < count; ++i) {
      QMatrix4x4 m;
      m.translate(i * impl_->spacing_, 0.0f, 0.0f);
      m.translate(impl_->offset_.x(), impl_->offset_.y(), impl_->offset_.z());
      transforms.push_back(m);
     }
    }
    break;
   }
  }
  return transforms;
 }

 std::vector<CloneData> CloneGenerator::generateCloneData() const
 {
  auto transforms = generateTransforms();
  std::vector<CloneData> clones;
  clones.reserve(transforms.size());

  for (int i = 0; i < transforms.size(); ++i) {
   CloneData data;
   data.index = i;
   data.transform = transforms[i];
   data.color = Qt::white; // デフォルトカラー
   data.weight = 1.0f;
   data.timeOffset = 0.0f;
   data.visible = true;
   clones.push_back(data);
  }

  return clones;
 }

};
