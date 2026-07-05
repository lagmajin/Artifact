module;

#include <QLabel>
#include <QListWidget>
#include <QVBoxLayout>
#include <QSignalBlocker>

module Artifact.Widgets.DopeSheetWidget;

import std;

import Artifact.Application.Manager;
import Artifact.Composition.Abstract;
import Artifact.Layer.Abstract;
import Artifact.Layers.Selection.Manager;
import Artifact.Service.Project;
import Artifact.Timeline.KeyframeModel;

namespace Artifact {

namespace {

QString dopeSheetHeaderText(const ArtifactCore::CompositionID& compositionId,
                            const QString& layerName,
                            const int entryCount,
                            const int64_t currentFrame) {
  const QString compText = compositionId.isNil()
                               ? QStringLiteral("None")
                               : compositionId.toString();
  const QString layerText =
      layerName.trimmed().isEmpty() ? QStringLiteral("No layer") : layerName;
  return QStringLiteral("Comp: %1 | Layer: %2 | Keys: %3 | Frame: %4")
      .arg(compText, layerText)
      .arg(entryCount)
      .arg(currentFrame);
}

QString dopeSheetEntryText(const DopeSheetKeyframeEntry& entry) {
  const QString label =
      ArtifactTimelineKeyframeModel::displayLabelForPropertyPath(
          entry.propertyPath);
  return QStringLiteral("F%1  %2  %3")
      .arg(entry.keyframe.time.value())
      .arg(label, entry.keyframe.value.toString());
}

} // namespace

class ArtifactDopeSheetWidget::Impl {
public:
  ArtifactCore::CompositionID compositionId_;
  int64_t currentFrame_ = 0;
  QLabel* summaryLabel_ = nullptr;
  QListWidget* keyList_ = nullptr;
};

ArtifactDopeSheetWidget::ArtifactDopeSheetWidget(QWidget* parent)
    : QWidget(parent), impl_(new Impl()) {
  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(8, 8, 8, 8);
  layout->setSpacing(6);

  impl_->summaryLabel_ = new QLabel(this);
  impl_->summaryLabel_->setWordWrap(true);
  layout->addWidget(impl_->summaryLabel_);

  impl_->keyList_ = new QListWidget(this);
  impl_->keyList_->setSelectionMode(QAbstractItemView::NoSelection);
  layout->addWidget(impl_->keyList_, 1);

  setMinimumHeight(180);
  refreshFromCurrentContext();
}

ArtifactDopeSheetWidget::~ArtifactDopeSheetWidget() { delete impl_; }

void ArtifactDopeSheetWidget::setComposition(
    const ArtifactCore::CompositionID& id) {
  if (impl_->compositionId_ == id) {
    refreshFromCurrentContext();
    return;
  }
  impl_->compositionId_ = id;
  refreshFromCurrentContext();
}

ArtifactCore::CompositionID ArtifactDopeSheetWidget::composition() const {
  return impl_ ? impl_->compositionId_ : ArtifactCore::CompositionID{};
}

void ArtifactDopeSheetWidget::setCurrentFrame(int64_t frame) {
  if (!impl_) {
    return;
  }
  impl_->currentFrame_ = std::max<int64_t>(0, frame);
  refreshFromCurrentContext();
}

int64_t ArtifactDopeSheetWidget::currentFrame() const {
  return impl_ ? impl_->currentFrame_ : 0;
}

void ArtifactDopeSheetWidget::refreshFromCurrentContext() {
  if (!impl_ || !impl_->summaryLabel_ || !impl_->keyList_) {
    return;
  }

  QString layerName = QStringLiteral("No layer");
  std::vector<DopeSheetKeyframeEntry> entries;

  auto* projectService = ArtifactProjectService::instance();
  auto* app = ArtifactApplicationManager::instance();
  auto* selectionManager = app ? app->layerSelectionManager() : nullptr;
  const auto currentLayer =
      selectionManager ? selectionManager->currentLayer()
                       : ArtifactAbstractLayerPtr{};

  if (projectService && !impl_->compositionId_.isNil()) {
    const auto found = projectService->findComposition(impl_->compositionId_);
    if (found.success) {
      if (const auto composition = found.ptr.lock()) {
        if (currentLayer && currentLayer->composition() == composition.get()) {
          layerName = currentLayer->layerName();
          ArtifactTimelineKeyframeModel model;
          entries = model.collectDopeSheetKeyframesForLayer(
              impl_->compositionId_, currentLayer->id());
        } else {
          layerName = QStringLiteral("Select a layer in Timeline/Inspector");
        }
      }
    }
  }

  impl_->summaryLabel_->setText(dopeSheetHeaderText(
      impl_->compositionId_, layerName, static_cast<int>(entries.size()),
      impl_->currentFrame_));

  const QSignalBlocker blocker(impl_->keyList_);
  impl_->keyList_->clear();
  if (entries.empty()) {
    impl_->keyList_->addItem(
        QStringLiteral("No dope sheet rows yet. Select a layer with keyframes."));
    return;
  }

  for (const auto& entry : entries) {
    impl_->keyList_->addItem(dopeSheetEntryText(entry));
  }
}

} // namespace Artifact
