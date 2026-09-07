module;
#include <cstdint>
#include <vector>
#include <QJsonObject>
#include <QString>
#include <QVariant>

export module Artifact.Layer.Accumulator;

import Artifact.Layer.Abstract;
import Layer.Blend;

export namespace Artifact {

class ArtifactAccumulatorLayer final : public ArtifactAbstractLayer {
  class Impl;
  Impl* impl_ = nullptr;

public:
  ArtifactAccumulatorLayer();
  ~ArtifactAccumulatorLayer() override;

  float inputGain() const;
  void setInputGain(float value);
  float feedback() const;
  void setFeedback(float value);
  float decay() const;
  void setDecay(float value);
  ArtifactCore::LAYER_BLEND_TYPE accumulatorBlendMode() const;
  void setAccumulatorBlendMode(ArtifactCore::LAYER_BLEND_TYPE value);
  bool clearOnSeek() const;
  void setClearOnSeek(bool value);
  bool clearOnDisable() const;
  void setClearOnDisable(bool value);

  std::uint64_t accumulationInvalidationSerial() const;
  void resetAccumulation();
  void invalidateAccumulation();
  bool requiresSequentialEvaluation() const override { return true; }

  QJsonObject toJson() const override;
  void fromJsonProperties(const QJsonObject& object) override;
  std::vector<ArtifactCore::PropertyGroup> getLayerPropertyGroups() const override;
  bool setLayerPropertyValue(const QString& propertyPath,
                             const QVariant& value) override;
  void draw(ArtifactIRenderer* renderer) override;
};

}
