module;

#include <QAbstractItemView>
#include <QColor>
#include <QFont>
#include <QHeaderView>
#include <QLabel>
#include <QPalette>
#include <QSignalBlocker>
#include <QTableWidget>
#include <QVBoxLayout>

module Artifact.Widgets.DopeSheetWidget;

import std;
import Artifact.Application.Manager;
import Artifact.Composition.Abstract;
import Artifact.Layer.Abstract;
import Artifact.Layers.Selection.Manager;
import Artifact.Service.Project;
import Artifact.Timeline.KeyframeModel;
import Widgets.Utils.CSS;

namespace Artifact {
namespace {
constexpr int kMinimumVisibleFrame = 120;
constexpr int kMaximumVisibleColumns = 600;

QString compactPropertyLabel(const QString& propertyPath) {
  QString label =
      ArtifactTimelineKeyframeModel::displayLabelForPropertyPath(propertyPath);
  const QString prefix = QStringLiteral("Transform / ");
  if (label.startsWith(prefix)) label.remove(0, prefix.size());
  return label;
}

void applyDopeSheetPalette(QTableWidget* table) {
  const auto& theme = ArtifactCore::currentDCCTheme();
  QPalette palette = table->palette();
  palette.setColor(QPalette::Base, QColor(theme.secondaryBackgroundColor));
  palette.setColor(QPalette::AlternateBase, QColor(theme.backgroundColor));
  palette.setColor(QPalette::Text, QColor(theme.textColor));
  palette.setColor(QPalette::Highlight, QColor(theme.accentColor));
  palette.setColor(QPalette::Mid, QColor(theme.borderColor));
  table->setPalette(palette);
}
} // namespace

class ArtifactDopeSheetWidget::Impl {
public:
  ArtifactCore::CompositionID compositionId_;
  int64_t currentFrame_ = 0;
  QLabel* contextLabel_ = nullptr;
  QTableWidget* sheet_ = nullptr;
};

ArtifactDopeSheetWidget::ArtifactDopeSheetWidget(QWidget* parent)
    : QWidget(parent), impl_(new Impl()) {
  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(8, 8, 8, 8);
  layout->setSpacing(5);

  auto* title = new QLabel(QStringLiteral("Dope Sheet"), this);
  QFont titleFont = title->font();
  titleFont.setBold(true);
  titleFont.setPointSizeF(titleFont.pointSizeF() + 1.0);
  title->setFont(titleFont);
  layout->addWidget(title);

  impl_->contextLabel_ = new QLabel(this);
  layout->addWidget(impl_->contextLabel_);

  impl_->sheet_ = new QTableWidget(this);
  impl_->sheet_->setAlternatingRowColors(true);
  impl_->sheet_->setEditTriggers(QAbstractItemView::NoEditTriggers);
  impl_->sheet_->setSelectionMode(QAbstractItemView::NoSelection);
  impl_->sheet_->setShowGrid(true);
  impl_->sheet_->setCornerButtonEnabled(false);
  impl_->sheet_->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
  impl_->sheet_->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
  impl_->sheet_->verticalHeader()->setDefaultSectionSize(25);
  impl_->sheet_->verticalHeader()->setMinimumWidth(168);
  impl_->sheet_->horizontalHeader()->setDefaultSectionSize(18);
  impl_->sheet_->horizontalHeader()->setMinimumSectionSize(18);
  applyDopeSheetPalette(impl_->sheet_);
  layout->addWidget(impl_->sheet_, 1);
  setMinimumHeight(240);
  refreshFromCurrentContext();
}

ArtifactDopeSheetWidget::~ArtifactDopeSheetWidget() { delete impl_; }

void ArtifactDopeSheetWidget::setComposition(
    const ArtifactCore::CompositionID& id) {
  impl_->compositionId_ = id;
  refreshFromCurrentContext();
}

ArtifactCore::CompositionID ArtifactDopeSheetWidget::composition() const {
  return impl_ ? impl_->compositionId_ : ArtifactCore::CompositionID{};
}

void ArtifactDopeSheetWidget::setCurrentFrame(int64_t frame) {
  if (!impl_) return;
  impl_->currentFrame_ = std::max<int64_t>(0, frame);
  refreshFromCurrentContext();
}

int64_t ArtifactDopeSheetWidget::currentFrame() const {
  return impl_ ? impl_->currentFrame_ : 0;
}

void ArtifactDopeSheetWidget::refreshFromCurrentContext() {
  if (!impl_ || !impl_->contextLabel_ || !impl_->sheet_) return;
  QString layerName = QStringLiteral("No layer selected");
  std::vector<DopeSheetKeyframeEntry> entries;
  auto* projectService = ArtifactProjectService::instance();
  auto* app = ArtifactApplicationManager::instance();
  auto* selectionManager = app ? app->layerSelectionManager() : nullptr;
  const auto currentLayer = selectionManager
                                ? selectionManager->currentLayer()
                                : ArtifactAbstractLayerPtr{};

  if (projectService && !impl_->compositionId_.isNil()) {
    const auto found = projectService->findComposition(impl_->compositionId_);
    if (found.success) {
      if (const auto composition = found.ptr.lock()) {
        if (currentLayer && currentLayer->composition() == composition.get()) {
          layerName = currentLayer->layerName();
          ArtifactTimelineKeyframeModel model;
          const auto collected = model.collectDopeSheetKeyframesForLayer(
              impl_->compositionId_, currentLayer->id());
          for (const auto& entry : collected) {
            if (ArtifactTimelineKeyframeModel::isTransformPropertyPath(
                    entry.propertyPath)) {
              entries.push_back(entry);
            }
          }
        }
      }
    }
  }

  impl_->contextLabel_->setText(
      QStringLiteral("Layer: %1    Keys: %2    Frame: %3")
          .arg(layerName)
          .arg(static_cast<int>(entries.size()))
          .arg(impl_->currentFrame_));

  std::vector<QString> paths;
  int64_t lastFrame =
      std::max<int64_t>(kMinimumVisibleFrame, impl_->currentFrame_ + 10);
  for (const auto& entry : entries) {
    if (std::find(paths.begin(), paths.end(), entry.propertyPath) == paths.end())
      paths.push_back(entry.propertyPath);
    lastFrame = std::max(lastFrame, entry.keyframe.time.value() + 10);
  }
  int64_t firstFrame = 0;
  if (lastFrame > kMaximumVisibleColumns) {
    firstFrame = std::max<int64_t>(0, impl_->currentFrame_ - 60);
    lastFrame = firstFrame + kMaximumVisibleColumns - 1;
  }

  const QSignalBlocker blocker(impl_->sheet_);
  impl_->sheet_->clear();
  constexpr int groupRowCount = 2;
  impl_->sheet_->setRowCount(
      static_cast<int>(paths.size()) + groupRowCount);
  impl_->sheet_->setColumnCount(static_cast<int>(lastFrame - firstFrame + 1));
  QStringList frameLabels;
  for (int64_t frame = firstFrame; frame <= lastFrame; ++frame)
    frameLabels.push_back(frame % 10 == 0 ? QString::number(frame) : QString());
  impl_->sheet_->setHorizontalHeaderLabels(frameLabels);

  QStringList propertyLabels;
  propertyLabels.push_back(QStringLiteral("▾  %1").arg(layerName));
  propertyLabels.push_back(QStringLiteral("    ▾  Transform"));
  for (const QString& path : paths)
    propertyLabels.push_back(QStringLiteral("        ◇  %1").arg(
        compactPropertyLabel(path)));
  impl_->sheet_->setVerticalHeaderLabels(propertyLabels);
  for (int row = 0; row < groupRowCount; ++row) {
    if (auto* headerItem = impl_->sheet_->verticalHeaderItem(row)) {
      QFont font = headerItem->font();
      font.setBold(true);
      headerItem->setFont(font);
    }
  }

  const QColor accent(ArtifactCore::currentDCCTheme().accentColor);
  const QColor keyColor(154, 160, 166);
  const QColor playheadTint(accent.red(), accent.green(), accent.blue(), 38);
  const int currentColumn =
      static_cast<int>(impl_->currentFrame_ - firstFrame);
  for (int row = 0; row < impl_->sheet_->rowCount(); ++row) {
    auto* cell = new QTableWidgetItem();
    cell->setBackground(playheadTint);
    impl_->sheet_->setItem(row, currentColumn, cell);
  }
  for (const auto& entry : entries) {
    const auto found = std::find(paths.begin(), paths.end(), entry.propertyPath);
    if (found == paths.end()) continue;
    const int row =
        static_cast<int>(std::distance(paths.begin(), found)) + groupRowCount;
    const int64_t frame = entry.keyframe.time.value();
    if (frame < firstFrame || frame > lastFrame) continue;
    const int column = static_cast<int>(frame - firstFrame);
    auto* item = impl_->sheet_->item(row, column);
    if (!item) item = new QTableWidgetItem();
    item->setText(QStringLiteral("◆"));
    item->setTextAlignment(Qt::AlignCenter);
    item->setForeground(frame == impl_->currentFrame_ ? accent : keyColor);
    item->setToolTip(QStringLiteral("%1 — frame %2")
                         .arg(compactPropertyLabel(entry.propertyPath))
                         .arg(frame));
    impl_->sheet_->setItem(row, column, item);
    for (int groupRow = 0; groupRow < groupRowCount; ++groupRow) {
      auto* summary = impl_->sheet_->item(groupRow, column);
      if (!summary) summary = new QTableWidgetItem();
      summary->setText(QStringLiteral("◆"));
      summary->setTextAlignment(Qt::AlignCenter);
      summary->setForeground(frame == impl_->currentFrame_ ? accent : keyColor);
      impl_->sheet_->setItem(groupRow, column, summary);
    }
  }
  if (entries.empty()) {
    impl_->sheet_->setRowCount(2);
    impl_->sheet_->setVerticalHeaderLabels(
        {QStringLiteral("▾  %1").arg(layerName),
         QStringLiteral("    ▾  Transform")});
    auto* empty = new QTableWidgetItem(
        QStringLiteral("Select a layer with Transform keyframes"));
    empty->setForeground(keyColor);
    impl_->sheet_->setItem(1, 0, empty);
  }
  if (auto* playheadItem = impl_->sheet_->item(0, currentColumn)) {
    impl_->sheet_->scrollToItem(playheadItem,
                                QAbstractItemView::PositionAtCenter);
  }
}

} // namespace Artifact
