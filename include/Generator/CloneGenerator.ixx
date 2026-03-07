module;
#include <QVector>
#include <QMatrix4x4>
#include <QString>
#include <QVector3D>

export module Generator.Clone;

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


import Generator.DistributionModes;
import Artifact.Effect.Clone.Core;


export namespace Artifact
{

export class SimpleSpline;

 class CloneGenerator
 {
 private:
  class Impl;
  Impl* impl_;
 public:
  CloneGenerator();
  virtual ~CloneGenerator();

  // 基本パラメータ
  void setCount(int count);
  int count() const;

  void setSpacing(float spacing);
  float spacing() const;

  void setPrototypeName(const QString& name);
  QString prototypeName() const;

  // 分布モード
  void setDistributionMode(DistributionMode mode);
  DistributionMode distributionMode() const;

  void setTransformSpace(TransformSpace space);
  TransformSpace transformSpace() const;

  // Radial パラメータ
  void setRadius(float radius);
  float radius() const;

  // Grid パラメータ
  void setGridColumns(int cols);
  int gridColumns() const;
  void setGridRows(int rows);
  int gridRows() const;
  void setGridDepth(int depth);
  int gridDepth() const;
  void setGridSpacingX(float spacing);
  float gridSpacingX() const;
  void setGridSpacingY(float spacing);
  float gridSpacingY() const;
  void setGridSpacingZ(float spacing);
  float gridSpacingZ() const;

  // Random/Noise パラメータ
  void setRandomSeed(int seed);
  int randomSeed() const;
  void setVariation(float variation);
  float variation() const;
  void setBounds(const QVector3D& bounds);
  QVector3D bounds() const;
  void setUsePoissonDisk(bool use);
  bool usePoissonDisk() const;

  // Spiral パラメータ
  void setSpiralRotations(float rotations);
  float spiralRotations() const;

  // Transform パラメータ
  void setOffset(const QVector3D& offset);
  QVector3D offset() const;
  void setRotationStep(float degrees);
  float rotationStep() const;

  // Spline
  void setSpline(std::shared_ptr<SimpleSpline> spline);
  std::shared_ptr<SimpleSpline> spline() const;

  // 変換行列生成
  virtual QVector<QMatrix4x4> generateTransforms() const;
  virtual std::vector<CloneData> generateCloneData() const;
 };

};
