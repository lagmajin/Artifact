module;
#include <cstddef>
#include <utility>
#include <QDebug>
#include <QString>

module Artifact.Layer.MaskMatteState;

namespace Artifact {

void LayerMaskMatteState::addMask(const LayerMask &mask) {
  masks_.push_back(mask);
  ++maskRevision_;
  qDebug("%s", qPrintable(QStringLiteral("[ArtifactAbstractLayer] Mask added, count: %1")
                              .arg(masks_.size())));
}

void LayerMaskMatteState::removeMask(int index) {
  if (index < 0 || index >= static_cast<int>(masks_.size())) {
    return;
  }
  masks_.removeAt(static_cast<std::size_t>(index));
  ++maskRevision_;
  qDebug("%s", qPrintable(QStringLiteral("[ArtifactAbstractLayer] Mask removed at index: %1")
                              .arg(index)));
}

bool LayerMaskMatteState::moveMask(int fromIndex, int toIndex) {
  const int count = static_cast<int>(masks_.size());
  if (fromIndex < 0 || fromIndex >= count || toIndex < 0 ||
      toIndex >= count || fromIndex == toIndex) {
    return false;
  }
  auto maskValue = std::move(masks_[static_cast<std::size_t>(fromIndex)]);
  masks_.removeAt(static_cast<std::size_t>(fromIndex));
  masks_.insert(static_cast<std::size_t>(toIndex), std::move(maskValue));
  ++maskRevision_;
  return true;
}

void LayerMaskMatteState::setMask(int index, const LayerMask &maskValue) {
  if (index >= 0 && index < static_cast<int>(masks_.size())) {
    masks_[static_cast<std::size_t>(index)] = maskValue;
    ++maskRevision_;
  }
}

LayerMask LayerMaskMatteState::mask(int index) const {
  if (index >= 0 && index < static_cast<int>(masks_.size())) {
    return masks_[static_cast<std::size_t>(index)];
  }
  return {};
}

const LayerMask* LayerMaskMatteState::maskView(int index) const noexcept {
  if (index < 0) {
    return nullptr;
  }
  return masks_.at(static_cast<std::size_t>(index));
}

int LayerMaskMatteState::maskCount() const {
  return static_cast<int>(masks_.size());
}

void LayerMaskMatteState::clearMasks() {
  if (!masks_.empty()) {
    masks_.clear();
    ++maskRevision_;
  }
}

std::vector<LayerMask> LayerMaskMatteState::masks() const {
  return masks_.toStdVector();
}

void LayerMaskMatteState::setMasks(const std::vector<LayerMask> &masks) {
  masks_ = NamedVector<LayerMask>::fromStdVector(ContainerName{"Layer.Masks"},
                                                  masks);
  ++maskRevision_;
}

std::uint64_t LayerMaskMatteState::maskRevision() const noexcept {
  return maskRevision_;
}

std::vector<LayerMatteReference> LayerMaskMatteState::matteReferences() const {
  return mattes_.toStdVector();
}

void LayerMaskMatteState::setMatteReferences(
    const std::vector<LayerMatteReference> &refs) {
  mattes_ = NamedVector<LayerMatteReference>::fromStdVector(
      ContainerName{"Layer.MatteReferences"}, refs);
}

void LayerMaskMatteState::addMatteReference(const LayerMatteReference &ref) {
  mattes_.push_back(ref);
}

void LayerMaskMatteState::clearMatteReferences() { mattes_.clear(); }

const NamedVector<LayerMatteReference> &LayerMaskMatteState::mattes() const noexcept {
  return mattes_;
}

} // namespace Artifact
