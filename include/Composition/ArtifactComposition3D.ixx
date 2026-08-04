module;
#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <unordered_map>
#include <set>
#include <unordered_set>
#include <memory>
#include <QVector3D>
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
export module Composition3D;






import Artifact.Composition.Abstract;

export namespace Artifact {

 enum eCompositionType {
  OriginalComposition,
  PreCompose,
 };

 //class ArtifactCompositionPrivate;

 class ArtifactComposition3D :public ArtifactAbstractComposition{
 private: 
  class Impl;
 	
  //std::unique_ptr<ArtifactCompositionPrivate> pImpl_;
 public:
  explicit ArtifactComposition3D(const CompositionID& id,
                                 const ArtifactCompositionInitParams& params);
  ~ArtifactComposition3D();

  ArtifactComposition3D(const ArtifactComposition3D&) = delete;
  ArtifactComposition3D& operator=(const ArtifactComposition3D&) = delete;

  QVector3D cameraPosition() const;
  void setCameraPosition(const QVector3D& position);
  QVector3D cameraTarget() const;
  void setCameraTarget(const QVector3D& target);
  QVector3D cameraUp() const;
  void setCameraUp(const QVector3D& up);
  float cameraFieldOfView() const;
  void setCameraFieldOfView(float degrees);

 };

}
