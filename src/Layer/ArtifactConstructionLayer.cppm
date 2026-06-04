module;
#include <utility>

module Artifact.Layer.Construction;

import std;
import Artifact.Layer.Abstract;
import Artifact.Layers.Abstract._2D;

namespace Artifact {

class ArtifactConstructionLayer::Impl {
public:
  Impl() = default;
  ~Impl() = default;
};

ArtifactConstructionLayer::ArtifactConstructionLayer()
    : impl_(new Impl()) {
  setSourceSize(Size_2D(100, 100));
  setLayerName(QString("Construction Layer"));
}

ArtifactConstructionLayer::~ArtifactConstructionLayer() {
  delete impl_;
}

void ArtifactConstructionLayer::draw(ArtifactIRenderer* renderer) {
  (void)renderer;
}

bool ArtifactConstructionLayer::isNullLayer() const {
  return false;
}

bool ArtifactConstructionLayer::hasVideo() const {
  return false;
}

QJsonObject ArtifactConstructionLayer::toJson() const {
  QJsonObject obj = ArtifactAbstract2DLayer::toJson();
  obj["type"] = static_cast<int>(LayerType::Construction);
  obj["isConstruction"] = true;
  return obj;
}

void ArtifactConstructionLayer::fromJsonProperties(const QJsonObject& obj) {
  ArtifactAbstract2DLayer::fromJsonProperties(obj);
}

bool ArtifactConstructionLayer::isConstructionLayer() const {
  return true;
}

} // namespace Artifact
