module;

#include <QDialog>
#include <QByteArray>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QMouseEvent>
#include <QPalette>
#include <QPointF>
#include <QRectF>
#include <QSize>
#include <QToolButton>
#include <QTreeWidget>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <functional>
#include <memory>
#include <utility>
#include <vector>

module Artifact.Widgets.CompositionCleanup;

import Artifact.Composition.Abstract;
import Artifact.Layer.Abstract;
import Artifact.Layer.Text;
import Artifact.Render.IRenderer;
import Artifact.Widgets.CompositionRenderController;
import Color.Float;
import Memory.SharedPtr;
import Time.Rational;
import Undo.UndoManager;
import Utils.Id;

namespace Artifact {

ViewportLayoutButton::ViewportLayoutButton(QWidget* parent) : QToolButton(parent) {
  setCursor(Qt::PointingHandCursor);
}

void ViewportLayoutButton::setActivatedCallback(std::function<void()> callback) {
  activatedCallback_ = std::move(callback);
}

void ViewportLayoutButton::mousePressEvent(QMouseEvent* event) {
  pressedInside_ = event && event->button() == Qt::LeftButton &&
                   rect().contains(event->position().toPoint());
  QToolButton::mousePressEvent(event);
}

void ViewportLayoutButton::mouseReleaseEvent(QMouseEvent* event) {
  const bool activate = pressedInside_ && event &&
                        event->button() == Qt::LeftButton &&
                        rect().contains(event->position().toPoint());
  pressedInside_ = false;
  QToolButton::mouseReleaseEvent(event);
  if (activate && activatedCallback_) {
    activatedCallback_();
  }
}

std::vector<CompositionCleanupCandidate> analyzeCompositionCleanup(
    const ArtifactCompositionPtr& composition) {
  std::vector<CompositionCleanupCandidate> results;
  if (!composition) {
    return results;
  }

  const QSize compositionSize = composition->effectiveCompositionSize();
  if (!compositionSize.isValid()) {
    return results;
  }

  struct VisibleLayer {
    ArtifactAbstractLayerPtr layer;
    QRectF bounds;
  };
  std::vector<VisibleLayer> layers;
  for (const auto& layer : composition->allLayer()) {
    if (!layer || !layer->isVisible() || layer->isLocked()) {
      continue;
    }
    const QRectF bounds = layer->transformedBoundingBox();
    if (bounds.isEmpty() || !bounds.isValid()) {
      continue;
    }
    layers.push_back({layer, bounds});
  }

  const float width = static_cast<float>(compositionSize.width());
  const float height = static_cast<float>(compositionSize.height());
  const float safeMargin = std::max(12.0f, std::min(width, height) * 0.05f);

  for (const auto& item : layers) {
    const float left = static_cast<float>(item.bounds.left());
    const float right = width - static_cast<float>(item.bounds.right());
    const float top = static_cast<float>(item.bounds.top());
    const float bottom = height - static_cast<float>(item.bounds.bottom());
    const float horizontalDifference = std::abs(left - right);
    const float verticalDifference = std::abs(top - bottom);
    const bool hasHorizontalFrame = left >= 0.0f && right >= 0.0f &&
                                    item.bounds.width() >= width * 0.5f;
    const bool hasVerticalFrame = top >= 0.0f && bottom >= 0.0f &&
                                  item.bounds.height() >= height * 0.5f;
    if ((!hasHorizontalFrame || horizontalDifference < 1.0f) &&
        (!hasVerticalFrame || verticalDifference < 1.0f)) {
      continue;
    }

    float dx = 0.0f;
    float dy = 0.0f;
    QString axis;
    float difference = 0.0f;
    if (hasHorizontalFrame &&
        (!hasVerticalFrame || horizontalDifference >= verticalDifference)) {
      dx = (right - left) * 0.5f;
      axis = QStringLiteral("horizontal");
      difference = horizontalDifference;
    } else {
      dy = (bottom - top) * 0.5f;
      axis = QStringLiteral("vertical");
      difference = verticalDifference;
    }
    const float beforeX = item.layer->transform3D().positionX();
    const float beforeY = item.layer->transform3D().positionY();
    results.push_back({
        QStringLiteral("cleanup.uneven-margin"),
        QStringLiteral("%1 has %2px uneven %3 margins.")
            .arg(item.layer->layerName())
            .arg(QString::number(difference, 'f', 1))
            .arg(axis),
        QStringLiteral("Equalize %1 margins").arg(axis),
        {{item.layer->id().toString(), beforeX, beforeY, beforeX + dx,
          beforeY + dy}}});
  }

  for (const auto& item : layers) {
    const float left = static_cast<float>(item.bounds.left());
    const float right = width - static_cast<float>(item.bounds.right());
    const float top = static_cast<float>(item.bounds.top());
    const float bottom = height - static_cast<float>(item.bounds.bottom());
    const float minimum = std::min(std::min(left, right), std::min(top, bottom));
    if (minimum < 0.0f || minimum >= safeMargin) {
      continue;
    }

    float dx = 0.0f;
    float dy = 0.0f;
    QString direction;
    if (minimum == left) {
      dx = safeMargin - left;
      direction = QStringLiteral("right");
    } else if (minimum == right) {
      dx = -(safeMargin - right);
      direction = QStringLiteral("left");
    } else if (minimum == top) {
      dy = safeMargin - top;
      direction = QStringLiteral("down");
    } else {
      dy = -(safeMargin - bottom);
      direction = QStringLiteral("up");
    }

    const float beforeX = item.layer->transform3D().positionX();
    const float beforeY = item.layer->transform3D().positionY();
    const int amount = std::max(1, static_cast<int>(std::lround(std::abs(dx + dy))));
    results.push_back({
        QStringLiteral("cleanup.edge-risk"),
        QStringLiteral("%1 is only %2px from the composition edge.")
            .arg(item.layer->layerName())
            .arg(QString::number(minimum, 'f', 1)),
        QStringLiteral("Move %1 %2px").arg(direction).arg(amount),
        {{item.layer->id().toString(), beforeX, beforeY, beforeX + dx,
          beforeY + dy}}});
  }

  const QPointF compositionCenter(width * 0.5f, height * 0.5f);
  const float centerBand = std::max(2.0f, std::min(width, height) * 0.01f);
  for (const auto& item : layers) {
    const QPointF center = item.bounds.center();
    const float dx = static_cast<float>(compositionCenter.x() - center.x());
    const float dy = static_cast<float>(compositionCenter.y() - center.y());
    if ((std::abs(dx) < 1.0f && std::abs(dy) < 1.0f) ||
        (std::abs(dx) > centerBand || std::abs(dy) > centerBand)) {
      continue;
    }
    const float beforeX = item.layer->transform3D().positionX();
    const float beforeY = item.layer->transform3D().positionY();
    results.push_back({
        QStringLiteral("cleanup.near-center"),
        QStringLiteral("%1 is %2px from the composition center.")
            .arg(item.layer->layerName())
            .arg(QString::number(std::hypot(dx, dy), 'f', 1)),
        QStringLiteral("Center %1").arg(item.layer->layerName()),
        {{item.layer->id().toString(), beforeX, beforeY, beforeX + dx,
          beforeY + dy}}});
  }

  constexpr float kMinimumTextSize = 18.0f;
  const FloatColor compositionBackground = composition->backgroundColor();
  const auto relativeLuminance = [](const FloatColor& color) {
    return 0.2126f * std::max(0.0f, color.r()) +
           0.7152f * std::max(0.0f, color.g()) +
           0.0722f * std::max(0.0f, color.b());
  };
  const float backgroundLuminance = relativeLuminance(compositionBackground);
  for (const auto& item : layers) {
    const auto textLayer = ArtifactCore::dynamicPointerCast<ArtifactTextLayer>(item.layer);
    if (!textLayer) {
      continue;
    }
    if (textLayer->fontSize() < kMinimumTextSize) {
      results.push_back({
          QStringLiteral("cleanup.text-too-small"),
          QStringLiteral("%1 uses %2px text; the current minimum is %3px.")
              .arg(textLayer->layerName())
              .arg(QString::number(textLayer->fontSize(), 'f', 1))
              .arg(QString::number(kMinimumTextSize, 'f', 0)),
          QStringLiteral("Review in Text Inspector"),
          {}});
    }

    const FloatColor textColor = textLayer->textColor();
    const float textAlpha = std::clamp(textColor.a(), 0.0f, 1.0f);
    const float effectiveTextLuminance =
        relativeLuminance(textColor) * textAlpha +
        backgroundLuminance * (1.0f - textAlpha);
    const float contrastRatio =
        (std::max(effectiveTextLuminance, backgroundLuminance) + 0.05f) /
        (std::min(effectiveTextLuminance, backgroundLuminance) + 0.05f);
    if (contrastRatio < 4.5f) {
      results.push_back({
          QStringLiteral("cleanup.low-title-contrast"),
          QStringLiteral("%1 has an approximate %2:1 contrast ratio against the composition background.")
              .arg(textLayer->layerName())
              .arg(QString::number(contrastRatio, 'f', 2)),
          QStringLiteral("Review text or background color (approximate)"),
          {}});
    }
  }

  const float focalAreaThreshold = width * height * 0.15f;
  const float focalCenterRadius = std::min(width, height) * 0.25f;
  std::vector<const VisibleLayer*> focalCandidates;
  for (const auto& item : layers) {
    const float area = static_cast<float>(item.bounds.width() * item.bounds.height());
    if (area < focalAreaThreshold ||
        std::hypot(item.bounds.center().x() - compositionCenter.x(),
                   item.bounds.center().y() - compositionCenter.y()) >
            focalCenterRadius) {
      continue;
    }
    focalCandidates.push_back(&item);
  }
  if (focalCandidates.size() >= 2) {
    results.push_back({
        QStringLiteral("cleanup.multiple-focal-points"),
        QStringLiteral("%1 large layers compete near the composition center.")
            .arg(QString::number(focalCandidates.size())),
        QStringLiteral("Review visual hierarchy"),
        {}});
  }

  const float clusterDistance = std::max(8.0f, std::min(width, height) * 0.04f);
  int clusteredPairCount = 0;
  for (size_t lhsIndex = 0; lhsIndex < layers.size(); ++lhsIndex) {
    for (size_t rhsIndex = lhsIndex + 1; rhsIndex < layers.size(); ++rhsIndex) {
      const QPointF lhsCenter = layers[lhsIndex].bounds.center();
      const QPointF rhsCenter = layers[rhsIndex].bounds.center();
      if (std::hypot(lhsCenter.x() - rhsCenter.x(), lhsCenter.y() - rhsCenter.y()) <
          clusterDistance) {
        ++clusteredPairCount;
      }
    }
  }
  if (clusteredPairCount >= 2) {
    results.push_back({
        QStringLiteral("cleanup.clustered-elements"),
        QStringLiteral("%1 element pairs occupy nearly the same position.")
            .arg(QString::number(clusteredPairCount)),
        QStringLiteral("Review grouping or spacing"),
        {}});
  }

  if (layers.size() >= 3) {
    std::sort(layers.begin(), layers.end(), [](const VisibleLayer& lhs,
                                               const VisibleLayer& rhs) {
      return lhs.bounds.left() < rhs.bounds.left();
    });
    const auto [minimumCenterIt, maximumCenterIt] = std::minmax_element(
        layers.begin(), layers.end(), [](const VisibleLayer& lhs,
                                         const VisibleLayer& rhs) {
          return lhs.bounds.center().y() < rhs.bounds.center().y();
        });
    const float rowTolerance = std::max(12.0f, height * 0.02f);
    if (maximumCenterIt->bounds.center().y() -
            minimumCenterIt->bounds.center().y() >
        rowTolerance) {
      return results;
    }
    std::vector<float> gaps;
    gaps.reserve(layers.size() - 1);
    for (size_t index = 1; index < layers.size(); ++index) {
      gaps.push_back(static_cast<float>(layers[index].bounds.left() -
                                        layers[index - 1].bounds.right()));
    }
    const auto [minimumIt, maximumIt] = std::minmax_element(gaps.begin(), gaps.end());
    if (*minimumIt >= 0.0f && *maximumIt - *minimumIt >= 1.0f &&
        *maximumIt - *minimumIt <= 2.5f) {
      std::vector<float> sortedGaps = gaps;
      std::sort(sortedGaps.begin(), sortedGaps.end());
      const float targetGap = sortedGaps[sortedGaps.size() / 2];
      std::vector<CompositionCleanupMove> moves;
      float expectedLeft = static_cast<float>(layers.front().bounds.left());
      for (size_t index = 1; index < layers.size(); ++index) {
        expectedLeft += static_cast<float>(layers[index - 1].bounds.width()) + targetGap;
        const float delta = expectedLeft - static_cast<float>(layers[index].bounds.left());
        if (std::abs(delta) < 0.5f) {
          continue;
        }
        const float beforeX = layers[index].layer->transform3D().positionX();
        const float beforeY = layers[index].layer->transform3D().positionY();
        moves.push_back({layers[index].layer->id().toString(), beforeX, beforeY,
                         beforeX + delta, beforeY});
      }
      if (!moves.empty()) {
        results.push_back({
            QStringLiteral("cleanup.spacing-drift"),
            QStringLiteral("Horizontal gaps vary by %1px.")
                .arg(QString::number(*maximumIt - *minimumIt, 'f', 1)),
            QStringLiteral("Make horizontal gaps %1px").arg(
                QString::number(targetGap, 'f', 1)),
            std::move(moves)});
      }
    }
  }

  return results;
}

bool applyCompositionCleanupCandidate(
    const ArtifactCompositionPtr& composition,
    const CompositionCleanupCandidate& candidate) {
  if (!composition || candidate.moves.empty()) {
    return false;
  }

  QJsonArray beforeRecords;
  QJsonArray afterRecords;
  for (const auto& move : candidate.moves) {
    const auto layer = composition->layerById(LayerID(move.layerId));
    if (!layer || layer->isLocked()) {
      return false;
    }
    const float currentX = layer->transform3D().positionX();
    const float currentY = layer->transform3D().positionY();
    if (std::abs(currentX - move.beforeX) > 0.01f ||
        std::abs(currentY - move.beforeY) > 0.01f) {
      return false;
    }
    beforeRecords.append(QJsonObject{{QStringLiteral("id"), move.layerId},
                                     {QStringLiteral("x"), currentX},
                                     {QStringLiteral("y"), currentY}});
    afterRecords.append(QJsonObject{{QStringLiteral("id"), move.layerId},
                                    {QStringLiteral("x"), move.afterX},
                                    {QStringLiteral("y"), move.afterY}});
  }
  if (afterRecords.isEmpty()) {
    return false;
  }

  const ArtifactCompositionWeakPtr weakComposition(composition);
  const auto restore = [weakComposition](const QByteArray& state) {
    const auto targetComposition = weakComposition.lock();
    if (!targetComposition) {
      return false;
    }
    const QJsonArray records = QJsonDocument::fromJson(state).array();
    for (const auto& value : records) {
      const QJsonObject record = value.toObject();
      const auto layer = targetComposition->layerById(
          LayerID(record.value(QStringLiteral("id")).toString()));
      if (!layer) {
        continue;
      }
      layer->transform3D().setPosition(
          ArtifactCore::RationalTime(0, 30000),
          static_cast<float>(record.value(QStringLiteral("x")).toDouble()),
          static_cast<float>(record.value(QStringLiteral("y")).toDouble()));
      layer->changed();
    }
    targetComposition->changed();
    return true;
  };
  if (auto* manager = UndoManager::instance()) {
    return manager->push(std::make_unique<LayoutSnapshotCommand>(
        QStringLiteral("Composition Cleanup: %1").arg(candidate.actionLabel),
        QJsonDocument(beforeRecords).toJson(QJsonDocument::Compact),
        QJsonDocument(afterRecords).toJson(QJsonDocument::Compact), restore));
  }
  const bool applied = restore(
      QJsonDocument(afterRecords).toJson(QJsonDocument::Compact));
  if (!applied) {
    restore(QJsonDocument(beforeRecords).toJson(QJsonDocument::Compact));
  }
  return applied;
}

void previewCompositionCleanupCandidate(
    CompositionRenderController* controller,
    const ArtifactCompositionPtr& composition,
    const CompositionCleanupCandidate& candidate) {
  if (!controller || !composition || candidate.moves.empty()) {
    if (controller) {
      controller->clearDropGhostPreview();
    }
    return;
  }
  const auto* renderer = controller->renderer();
  if (!renderer) {
    controller->clearDropGhostPreview();
    return;
  }

  QRectF previewBounds;
  for (const auto& move : candidate.moves) {
    const auto layer = composition->layerById(LayerID(move.layerId));
    if (!layer) {
      continue;
    }
    QRectF movedBounds = layer->transformedBoundingBox();
    movedBounds.translate(move.afterX - move.beforeX, move.afterY - move.beforeY);
    previewBounds = previewBounds.isNull() ? movedBounds
                                            : previewBounds.united(movedBounds);
  }
  if (!previewBounds.isValid() || previewBounds.isEmpty()) {
    controller->clearDropGhostPreview();
    return;
  }

  const auto topLeft = renderer->canvasToViewport(
      {static_cast<float>(previewBounds.left()),
       static_cast<float>(previewBounds.top())});
  const auto bottomRight = renderer->canvasToViewport(
      {static_cast<float>(previewBounds.right()),
       static_cast<float>(previewBounds.bottom())});
  const QRectF viewportBounds(
      QPointF(std::min(topLeft.x, bottomRight.x),
              std::min(topLeft.y, bottomRight.y)),
      QPointF(std::max(topLeft.x, bottomRight.x),
              std::max(topLeft.y, bottomRight.y)));
  controller->setDropGhostPreview(viewportBounds, candidate.actionLabel,
                                  candidate.message,
                                  QStringLiteral("Cleanup preview"));
}

void showCompositionCleanupDialog(
    QWidget* parent,
    CompositionRenderController* controller,
    const ArtifactCompositionPtr& composition) {
  const auto candidates = analyzeCompositionCleanup(composition);
  QDialog dialog(parent);
  dialog.setWindowTitle(QStringLiteral("Composition Cleanup"));
  dialog.setModal(true);
  dialog.resize(560, 360);

  auto* layout = new QVBoxLayout(&dialog);
  auto* summary = new QLabel(
      candidates.empty()
          ? QStringLiteral("No deterministic geometry issues were found.")
          : QStringLiteral("Select a suggestion, then apply it as one Undo step."),
      &dialog);
  layout->addWidget(summary);
  auto* list = new QTreeWidget(&dialog);
  list->setColumnCount(2);
  list->setHeaderLabels({QStringLiteral("Issue"), QStringLiteral("Suggested fix")});
  for (int index = 0; index < static_cast<int>(candidates.size()); ++index) {
    auto* item = new QTreeWidgetItem(list);
    item->setText(0, candidates[index].message);
    item->setText(1, candidates[index].actionLabel);
    item->setData(0, Qt::UserRole, index);
  }
  if (list->topLevelItemCount() > 0) {
    list->setCurrentItem(list->topLevelItem(0));
  }
  list->resizeColumnToContents(1);
  layout->addWidget(list, 1);

  auto* buttonRow = new QHBoxLayout();
  buttonRow->addStretch();
  auto* previewButton = new ViewportLayoutButton(&dialog);
  previewButton->setText(QStringLiteral("Preview"));
  previewButton->setEnabled(!candidates.empty());
  previewButton->setActivatedCallback([&dialog]() { dialog.done(2); });
  buttonRow->addWidget(previewButton);
  auto* closeButton = new ViewportLayoutButton(&dialog);
  closeButton->setText(QStringLiteral("Close"));
  closeButton->setActivatedCallback([&dialog]() { dialog.reject(); });
  buttonRow->addWidget(closeButton);
  auto* applyButton = new ViewportLayoutButton(&dialog);
  applyButton->setText(QStringLiteral("Apply"));
  applyButton->setEnabled(!candidates.empty());
  applyButton->setActivatedCallback([&dialog]() { dialog.accept(); });
  buttonRow->addWidget(applyButton);
  layout->addLayout(buttonRow);

  while (true) {
    const int result = dialog.exec();
    if (result == 2) {
      const auto* selected = list->currentItem();
      if (!selected || !controller) {
        continue;
      }
      const int index = selected->data(0, Qt::UserRole).toInt();
      if (index >= 0 && index < static_cast<int>(candidates.size())) {
        const auto& candidate = candidates[index];
        previewCompositionCleanupCandidate(controller, composition, candidate);
        controller->setInfoOverlayText(
            QStringLiteral("Composition Cleanup Preview"),
            QStringLiteral("%1 — %2").arg(candidate.actionLabel,
                                             candidate.message));
      }
      continue;
    }
    if (result != QDialog::Accepted) {
      if (controller) {
        controller->clearInfoOverlayText();
        controller->clearDropGhostPreview();
      }
      return;
    }
    const auto* selected = list->currentItem();
    if (!selected) {
      return;
    }
    const int index = selected->data(0, Qt::UserRole).toInt();
    if (index >= 0 && index < static_cast<int>(candidates.size())) {
      const bool applied =
          applyCompositionCleanupCandidate(composition, candidates[index]);
      if (controller) {
        controller->setInfoOverlayText(
            applied ? QStringLiteral("Composition Cleanup Applied")
                    : QStringLiteral("Composition Cleanup Failed"),
            candidates[index].actionLabel);
        if (applied) {
          controller->clearDropGhostPreview();
        }
      }
    }
    return;
  }
}

}
