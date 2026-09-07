module;
#include <algorithm>
#include <cmath>
#include <QJsonObject>

module Artifact.Layer.Accumulator;

namespace Artifact {

class ArtifactAccumulatorLayer::Impl {
public:
  float inputGain = 1.0f;
  float feedback = 0.95f;
  float decay = 1.0f;
  ArtifactCore::LAYER_BLEND_TYPE blendMode =
      ArtifactCore::LAYER_BLEND_TYPE::BLEND_ADD;
  bool clearOnSeek = true;
  bool clearOnDisable = true;
  std::uint64_t invalidationSerial = 1;
};

namespace {
float sanitizedGain(const float value, const float fallback) {
  return std::isfinite(value) ? std::max(0.0f, value) : fallback;
}
}

ArtifactAccumulatorLayer::ArtifactAccumulatorLayer() : impl_(new Impl()) {}
ArtifactAccumulatorLayer::~ArtifactAccumulatorLayer() { delete impl_; }
float ArtifactAccumulatorLayer::inputGain() const { return impl_->inputGain; }
void ArtifactAccumulatorLayer::setInputGain(float value) { impl_->inputGain = sanitizedGain(value, 1.0f); invalidateAccumulation(); }
float ArtifactAccumulatorLayer::feedback() const { return impl_->feedback; }
void ArtifactAccumulatorLayer::setFeedback(float value) { impl_->feedback = sanitizedGain(value, 0.95f); invalidateAccumulation(); }
float ArtifactAccumulatorLayer::decay() const { return impl_->decay; }
void ArtifactAccumulatorLayer::setDecay(float value) { impl_->decay = sanitizedGain(value, 1.0f); invalidateAccumulation(); }
ArtifactCore::LAYER_BLEND_TYPE ArtifactAccumulatorLayer::accumulatorBlendMode() const { return impl_->blendMode; }
void ArtifactAccumulatorLayer::setAccumulatorBlendMode(ArtifactCore::LAYER_BLEND_TYPE value) { impl_->blendMode = value; invalidateAccumulation(); }
bool ArtifactAccumulatorLayer::clearOnSeek() const { return impl_->clearOnSeek; }
void ArtifactAccumulatorLayer::setClearOnSeek(bool value) { impl_->clearOnSeek = value; }
bool ArtifactAccumulatorLayer::clearOnDisable() const { return impl_->clearOnDisable; }
void ArtifactAccumulatorLayer::setClearOnDisable(bool value) { impl_->clearOnDisable = value; }
std::uint64_t ArtifactAccumulatorLayer::accumulationInvalidationSerial() const { return impl_->invalidationSerial; }
void ArtifactAccumulatorLayer::resetAccumulation() { invalidateAccumulation(); }
void ArtifactAccumulatorLayer::invalidateAccumulation() { ++impl_->invalidationSerial; setDirty(LayerDirtyFlag::Property); }

QJsonObject ArtifactAccumulatorLayer::toJson() const {
  auto object = ArtifactAbstractLayer::toJson();
  object["type"] = static_cast<int>(LayerType::Accumulator);
  object["accumulatorInputGain"] = inputGain();
  object["accumulatorFeedback"] = feedback();
  object["accumulatorDecay"] = decay();
  object["accumulatorBlendMode"] = static_cast<int>(accumulatorBlendMode());
  object["accumulatorClearOnSeek"] = clearOnSeek();
  object["accumulatorClearOnDisable"] = clearOnDisable();
  return object;
}

void ArtifactAccumulatorLayer::fromJsonProperties(const QJsonObject& object) {
  ArtifactAbstractLayer::fromJsonProperties(object);
  setInputGain(static_cast<float>(object.value("accumulatorInputGain").toDouble(1.0)));
  setFeedback(static_cast<float>(object.value("accumulatorFeedback").toDouble(0.95)));
  setDecay(static_cast<float>(object.value("accumulatorDecay").toDouble(1.0)));
  setAccumulatorBlendMode(static_cast<ArtifactCore::LAYER_BLEND_TYPE>(object.value("accumulatorBlendMode").toInt(static_cast<int>(ArtifactCore::LAYER_BLEND_TYPE::BLEND_ADD))));
  setClearOnSeek(object.value("accumulatorClearOnSeek").toBool(true));
  setClearOnDisable(object.value("accumulatorClearOnDisable").toBool(true));
}

std::vector<ArtifactCore::PropertyGroup> ArtifactAccumulatorLayer::getLayerPropertyGroups() const {
  auto groups = ArtifactAbstractLayer::getLayerPropertyGroups();
  ArtifactCore::PropertyGroup group(QStringLiteral("Accumulator"));
  const auto addFloat = [&](const QString& path, const QString& label, const float value) {
    auto property = persistentLayerProperty(path, ArtifactCore::PropertyType::Float, value, -80);
    property->setHardRange(0.0, 1000000.0);
    property->setValue(value);
    property->setDisplayLabel(label);
    group.addProperty(property);
  };
  addFloat(QStringLiteral("accumulator.inputGain"), QStringLiteral("Input Gain"), inputGain());
  addFloat(QStringLiteral("accumulator.feedback"), QStringLiteral("Feedback"), feedback());
  addFloat(QStringLiteral("accumulator.decay"), QStringLiteral("Decay"), decay());
  auto mode = persistentLayerProperty(QStringLiteral("accumulator.blendMode"), ArtifactCore::PropertyType::Integer, static_cast<int>(accumulatorBlendMode()), -77);
  mode->setValue(static_cast<int>(accumulatorBlendMode())); mode->setDisplayLabel(QStringLiteral("Blend Mode")); group.addProperty(mode);
  auto seek = persistentLayerProperty(QStringLiteral("accumulator.clearOnSeek"), ArtifactCore::PropertyType::Boolean, clearOnSeek(), -76);
  seek->setValue(clearOnSeek()); seek->setDisplayLabel(QStringLiteral("Clear on Seek")); group.addProperty(seek);
  auto disable = persistentLayerProperty(QStringLiteral("accumulator.clearOnDisable"), ArtifactCore::PropertyType::Boolean, clearOnDisable(), -75);
  disable->setValue(clearOnDisable()); disable->setDisplayLabel(QStringLiteral("Clear on Disable")); group.addProperty(disable);
  groups.push_back(group);
  return groups;
}

bool ArtifactAccumulatorLayer::setLayerPropertyValue(const QString& path, const QVariant& value) {
  if (path == QStringLiteral("accumulator.inputGain")) setInputGain(value.toFloat());
  else if (path == QStringLiteral("accumulator.feedback")) setFeedback(value.toFloat());
  else if (path == QStringLiteral("accumulator.decay")) setDecay(value.toFloat());
  else if (path == QStringLiteral("accumulator.blendMode")) setAccumulatorBlendMode(static_cast<ArtifactCore::LAYER_BLEND_TYPE>(value.toInt()));
  else if (path == QStringLiteral("accumulator.clearOnSeek")) setClearOnSeek(value.toBool());
  else if (path == QStringLiteral("accumulator.clearOnDisable")) setClearOnDisable(value.toBool());
  else return ArtifactAbstractLayer::setLayerPropertyValue(path, value);
  Q_EMIT changed();
  return true;
}

void ArtifactAccumulatorLayer::draw(ArtifactIRenderer*) {}

}
