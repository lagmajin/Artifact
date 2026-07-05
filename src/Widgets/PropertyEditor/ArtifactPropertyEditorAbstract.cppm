module;
#include <QWidget>
#include <QVariant>
#include <functional>

module Artifact.Widgets.PropertyEditor;

namespace Artifact {

ArtifactAbstractPropertyEditor::ArtifactAbstractPropertyEditor(QWidget *parent)
    : QWidget(parent) {}

ArtifactAbstractPropertyEditor::~ArtifactAbstractPropertyEditor() = default;

void ArtifactAbstractPropertyEditor::setCommitHandler(CommitHandler handler) {
  commitHandler_ = std::move(handler);
}

void ArtifactAbstractPropertyEditor::setPreviewHandler(PreviewHandler handler) {
  previewHandler_ = std::move(handler);
}

void ArtifactAbstractPropertyEditor::previewCurrentValue() const {
  previewValue(value());
}

void ArtifactAbstractPropertyEditor::previewValueFromVariant(
    const QVariant &value) const {
  previewValue(value);
}

void ArtifactAbstractPropertyEditor::commitCurrentValue() const {
  commitValue(value());
}

void ArtifactAbstractPropertyEditor::commitValue(const QVariant &value) const {
  if (commitHandler_) {
    commitHandler_(value);
  }
}

void ArtifactAbstractPropertyEditor::previewValue(const QVariant &value) const {
  if (previewHandler_) {
    previewHandler_(value);
  }
}

bool ArtifactAbstractPropertyEditor::supportsScrub() const { return false; }

void ArtifactAbstractPropertyEditor::scrubByPixels(
    int deltaPixels, Qt::KeyboardModifiers modifiers) {
  Q_UNUSED(deltaPixels);
  Q_UNUSED(modifiers);
}

QWidget *ArtifactAbstractPropertyEditor::scrubTargetWidget() const {
  return const_cast<ArtifactAbstractPropertyEditor *>(this);
}


} // namespace Artifact
