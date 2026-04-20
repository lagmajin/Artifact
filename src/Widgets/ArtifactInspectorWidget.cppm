module;
#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QColorDialog>
#include <QContextMenuEvent>
#include <QCursor>
#include <QDoubleSpinBox>
#include <QFont>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMessageBox>
#include <QMetaObject>
#include <QObject>
#include <QPalette>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QTabWidget>
#include <QTimer>
#include <QVBoxLayout>
#include <QVariant>
#include <QWidget>
#include <cstdlib>
#include <wobjectimpl.h>


#include <algorithm>
#include <any>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <deque>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <numeric>
#include <optional>
#include <queue>
#include <random>
#include <regex>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <variant>
#include <vector>

module Widgets.Inspector;

import Utils.Id;
import Utils.String.UniString;
import Widgets.Utils.CSS;

import Artifact.Service.Project;
import Artifact.Composition.Abstract;
import Artifact.Effect.Abstract;
import Artifact.Event.Types;
import Event.Bus;
import Undo.UndoManager;
import Generator.Effector;
import Artifact.Effect.Generator.Cloner;
import Artifact.Effect.Generator.FractalNoise;
import Artifact.Effect.Transform.Twist;
import Artifact.Effect.Transform.Bend;
import Artifact.Effect.Render.PBRMaterial;
import Artifact.Effect.LayerTransform.Transform2D;
import Artifact.Effect.Rasterizer.Blur;
import Artifact.Effect.Rasterizer.DropShadow;
import BrightnessEffect;
import ExposureEffect;
import HueAndSaturation;
import Artifact.Effect.Glow;
import Artifact.Effect.GauusianBlur;
import Artifact.Effect.Keying.ChromaKey;
import Artifact.Effect.Wave;
import Artifact.Effect.Spherize;
import Artifact.Widgets.ArtifactPropertyWidget;
import Artifact.Widgets.AppDialogs;

namespace Artifact {

using namespace ArtifactCore;

// using namespace ArtifactWidgets;

namespace {
constexpr int kEffectRackCount = 5;
constexpr int kInspectorSectionMarginL = 8;
constexpr int kInspectorSectionMarginT = 8;
constexpr int kInspectorSectionMarginR = 8;
constexpr int kInspectorSectionMarginB = 8;
constexpr int kInspectorSectionSpacing = 4;
constexpr int kInspectorNoteMargin = 6;
constexpr int kInspectorRackMarginL = 6;
constexpr int kInspectorRackMarginT = 10;
constexpr int kInspectorRackMarginR = 6;
constexpr int kInspectorRackMarginB = 6;

QColor themeColor(const QString &value, const QColor &fallback) {
  const QColor color(value);
  return color.isValid() ? color : fallback;
}

QColor blendColor(const QColor &a, const QColor &b, const qreal t) {
  const qreal clamped = std::clamp(t, 0.0, 1.0);
  return QColor::fromRgbF(a.redF() * (1.0 - clamped) + b.redF() * clamped,
                          a.greenF() * (1.0 - clamped) + b.greenF() * clamped,
                          a.blueF() * (1.0 - clamped) + b.blueF() * clamped,
                          a.alphaF() * (1.0 - clamped) + b.alphaF() * clamped);
}

void applyInspectorPalette(QWidget *widget, const bool elevated = false) {
  if (!widget) {
    return;
  }
  const auto &theme = ArtifactCore::currentDCCTheme();
  const QColor background =
      themeColor(theme.backgroundColor, QColor(QStringLiteral("#20242A")));
  const QColor surface = themeColor(theme.secondaryBackgroundColor,
                                    QColor(QStringLiteral("#2B3038")));
  const QColor text =
      themeColor(theme.textColor, QColor(QStringLiteral("#E3E7EC")));
  const QColor selection =
      themeColor(theme.selectionColor, QColor(QStringLiteral("#3C5B76")));
  const QColor border =
      themeColor(theme.borderColor, QColor(QStringLiteral("#404754")));
  const QColor accent =
      themeColor(theme.accentColor, QColor(QStringLiteral("#5E94C7")));

  widget->setAttribute(Qt::WA_StyledBackground, true);
  widget->setAutoFillBackground(true);
  QPalette pal = widget->palette();
  const QColor window =
      elevated ? blendColor(surface, background, 0.16) : background;
  pal.setColor(QPalette::Window, window);
  pal.setColor(QPalette::WindowText, text);
  pal.setColor(QPalette::Base, surface);
  pal.setColor(QPalette::AlternateBase, blendColor(surface, background, 0.12));
  pal.setColor(QPalette::Text, text);
  pal.setColor(QPalette::Button, surface);
  pal.setColor(QPalette::ButtonText, text);
  pal.setColor(QPalette::Highlight, selection);
  pal.setColor(QPalette::HighlightedText, background);
  pal.setColor(QPalette::Mid, border);
  pal.setColor(QPalette::Light, accent.lighter(120));
  widget->setPalette(pal);
}

void applyInspectorLabelPalette(QLabel *label, const bool prominent = false) {
  if (!label) {
    return;
  }
  const auto &theme = ArtifactCore::currentDCCTheme();
  const QColor text =
      themeColor(theme.textColor, QColor(QStringLiteral("#E3E7EC")));
  const QColor accent =
      themeColor(theme.accentColor, QColor(QStringLiteral("#5E94C7")));
  QPalette pal = label->palette();
  pal.setColor(QPalette::WindowText, prominent ? accent : text);
  label->setPalette(pal);
}

void applyInspectorSectionBox(QGroupBox *box) {
  if (!box) {
    return;
  }
  applyInspectorPalette(box, true);
  QFont font = box->font();
  font.setPointSize(10);
  font.setWeight(QFont::DemiBold);
  box->setFont(font);
}

void applyInspectorTextEdit(QPlainTextEdit *edit) {
  if (!edit) {
    return;
  }
  applyInspectorPalette(edit, true);
  edit->setTabChangesFocus(true);
}

void applyInspectorList(QListWidget *list) {
  if (!list) {
    return;
  }
  applyInspectorPalette(list, true);
  list->setAlternatingRowColors(true);
}

void applyInspectorButton(QPushButton *button, const bool accent = false) {
  if (!button) {
    return;
  }
  const auto &theme = ArtifactCore::currentDCCTheme();
  const QColor background =
      themeColor(theme.backgroundColor, QColor(QStringLiteral("#20242A")));
  const QColor surface = themeColor(theme.secondaryBackgroundColor,
                                    QColor(QStringLiteral("#2B3038")));
  const QColor text =
      themeColor(theme.textColor, QColor(QStringLiteral("#E3E7EC")));
  const QColor selection =
      themeColor(theme.selectionColor, QColor(QStringLiteral("#3C5B76")));
  const QColor border =
      themeColor(theme.borderColor, QColor(QStringLiteral("#404754")));
  const QColor fill =
      accent ? themeColor(theme.accentColor, QColor(QStringLiteral("#5E94C7")))
             : surface;
  const QColor contrast = accent ? background : text;

  button->setAttribute(Qt::WA_StyledBackground, true);
  button->setAutoFillBackground(true);
  QPalette pal = button->palette();
  pal.setColor(QPalette::Button, fill);
  pal.setColor(QPalette::ButtonText, contrast);
  pal.setColor(QPalette::Window, surface);
  pal.setColor(QPalette::WindowText, text);
  pal.setColor(QPalette::Highlight, selection);
  pal.setColor(QPalette::HighlightedText, background);
  pal.setColor(QPalette::Mid, border);
  button->setPalette(pal);
}

int rackIndexFromStage(EffectPipelineStage stage) {
  const int stageIndex = static_cast<int>(stage);
  if (stageIndex <= static_cast<int>(EffectPipelineStage::PreProcess)) {
    return -1;
  }
  const int rackIndex = stageIndex - 1;
  return (rackIndex >= 0 && rackIndex < kEffectRackCount) ? rackIndex : -1;
}

EffectPipelineStage stageFromRackIndex(int rackIndex) {
  return static_cast<EffectPipelineStage>(rackIndex + 1);
}
} // namespace

W_OBJECT_IMPL(ArtifactInspectorWidget)

class ArtifactInspectorWidget::Impl {
private:
public:
  Impl();
  ~Impl();
  QWidget *containerWidget = nullptr;
  QTabWidget *tabWidget = nullptr;

  // Layer Info Tab
  QGroupBox *compositionNoteGroup = nullptr;
  QPlainTextEdit *compositionNoteEdit = nullptr;
  QGroupBox *layerNoteGroup = nullptr;
  QPlainTextEdit *layerNoteEdit = nullptr;
  QLabel *layerNameLabel = nullptr;
  QLabel *layerTypeLabel = nullptr;
  QLabel *statusLabel = nullptr;

  // Effects Pipeline Tab
  QScrollArea *effectsScrollArea = nullptr;
  QWidget *effectsTabWidget = nullptr;
  QLabel *effectsStateLabel = nullptr;
  QLabel *effectParametersHintLabel = nullptr;
  ArtifactPropertyWidget *effectPropertyWidget = nullptr;
  QString focusedEffectId_;

  struct EffectRack {
    QListWidget *listWidget = nullptr;
    QPushButton *addButton = nullptr;
    QPushButton *removeButton = nullptr;
    QPushButton *moveUpButton = nullptr;
    QPushButton *moveDownButton = nullptr;
  };
  EffectRack racks[kEffectRackCount];
  QMenu *inspectorMenu_ = nullptr;

  CompositionID currentCompositionId_;
  LayerID currentLayerId_;
  QMetaObject::Connection compositionNoteConnection_;
  QMetaObject::Connection layerNoteConnection_;
  ArtifactCore::EventBus eventBus_ = ArtifactCore::globalEventBus();
  std::vector<ArtifactCore::EventBus::Subscription> eventBusSubscriptions_;
  QString lastLayerInfoSignature_;
  std::array<QString, kEffectRackCount> lastRackSignatures_{};
  QString lastCompositionNoteText_;
  QString lastLayerNoteText_;
  int refreshMask_ = 0;
  bool refreshQueued_ = false;

  enum RefreshReason {
    CompositionNoteDirty = 1 << 0,
    LayerNoteDirty = 1 << 1,
    LayerInfoDirty = 1 << 2,
    EffectsDirty = 1 << 3
  };

  void rebuildMenu();
  void defaultHandleKeyPressEvent(QKeyEvent *event);
  void defaultHandleMousePressEvent(QMouseEvent *event);

  void showContextMenu();
  void showContextMenu(const QPoint &globalPos);
  void showRackContextMenu(int rackIndex, QListWidgetItem *item,
                           const QPoint &globalPos);
  bool removeEffectById(const QString &effectId);
  bool setEffectEnabledById(const QString &effectId, bool enabled);
  bool moveEffectById(const QString &effectId, int direction);
  void handleProjectCreated();
  void handleProjectClosed();
  void handleCompositionCreated(const CompositionID &id);
  void handleCompositionChanged(const CompositionID &id);
  void handleLayerSelected(const LayerSelectionChangedEvent &event);
  void updateCompositionNote();
  void updateLayerNote();
  void updateLayerInfo();
  void updateEffectsList();
  void updatePropertiesForEffect(const QString &effectId);
  void syncEffectPropertyWidget();
  void handleAddEffectClicked(int rackIndex);
  void handleAddGeneratorEffect(int rackIndex);
  void handleRemoveEffectClicked(int rackIndex);
  void refreshRackButtons();
  void setEffectRackEnabled(bool enabled);
  void setEffectsStateText(const QString &text, bool visible);
  void setNoProjectState();
  void setNoLayerState();
  void scheduleRefresh(int reasonMask = CompositionNoteDirty | LayerNoteDirty |
                                        LayerInfoDirty | EffectsDirty);
  void refreshNow();
  QString
  computeLayerInfoSignature(const ArtifactAbstractLayerPtr &layer) const;
  QString computeRackSignature(
      int rackIndex,
      const std::vector<ArtifactAbstractEffectPtr> &effects) const;
};

ArtifactInspectorWidget::Impl::Impl() {}

void ArtifactInspectorWidget::Impl::scheduleRefresh(int reasonMask) {
  QObject *context = containerWidget ? static_cast<QObject *>(containerWidget)
                                     : static_cast<QObject *>(tabWidget);
  if (!context) {
    refreshNow();
    return;
  }
  refreshMask_ |= reasonMask;
  if (refreshQueued_) {
    return;
  }
  refreshQueued_ = true;
  QTimer::singleShot(0, context, [this]() {
    if (!refreshQueued_) {
      return;
    }
    refreshNow();
  });
}

void ArtifactInspectorWidget::Impl::refreshNow() {
  const int mask = refreshMask_;
  refreshMask_ = 0;
  refreshQueued_ = false;
  if (mask & CompositionNoteDirty) {
    updateCompositionNote();
  }
  if (mask & LayerNoteDirty) {
    updateLayerNote();
  }
  if (mask & LayerInfoDirty) {
    updateLayerInfo();
  }
  if (mask & EffectsDirty) {
    updateEffectsList();
  }
}

void ArtifactInspectorWidget::Impl::updatePropertiesForEffect(
    const QString &effectId) {
  focusedEffectId_ = effectId.trimmed();
  syncEffectPropertyWidget();
}

void ArtifactInspectorWidget::Impl::syncEffectPropertyWidget() {
  if (!effectPropertyWidget) {
    return;
  }

  auto projectService = ArtifactProjectService::instance();
  if (!projectService || currentCompositionId_.isNil() ||
      currentLayerId_.isNil()) {
    effectPropertyWidget->clear();
    effectPropertyWidget->setVisible(false);
    if (effectParametersHintLabel) {
      effectParametersHintLabel->setText(
          QStringLiteral("Select a layer and effect to edit parameters."));
      effectParametersHintLabel->setVisible(true);
    }
    return;
  }

  auto findResult = projectService->findComposition(currentCompositionId_);
  if (!findResult.success) {
    effectPropertyWidget->clear();
    effectPropertyWidget->setVisible(false);
    if (effectParametersHintLabel) {
      effectParametersHintLabel->setText(
          QStringLiteral("Select a layer and effect to edit parameters."));
      effectParametersHintLabel->setVisible(true);
    }
    return;
  }

  auto comp = findResult.ptr.lock();
  if (!comp) {
    effectPropertyWidget->clear();
    effectPropertyWidget->setVisible(false);
    if (effectParametersHintLabel) {
      effectParametersHintLabel->setText(
          QStringLiteral("Select a layer and effect to edit parameters."));
      effectParametersHintLabel->setVisible(true);
    }
    return;
  }

  auto layer = comp->layerById(currentLayerId_);
  if (!layer) {
    effectPropertyWidget->clear();
    effectPropertyWidget->setVisible(false);
    if (effectParametersHintLabel) {
      effectParametersHintLabel->setText(
          QStringLiteral("Select a layer and effect to edit parameters."));
      effectParametersHintLabel->setVisible(true);
    }
    return;
  }

  bool effectExists = false;
  if (!focusedEffectId_.trimmed().isEmpty()) {
    for (const auto &effect : layer->getEffects()) {
      if (effect && effect->effectID().toQString() == focusedEffectId_) {
        effectExists = true;
        break;
      }
    }
  }

  if (!effectExists) {
    focusedEffectId_.clear();
  }

  effectPropertyWidget->setLayer(layer);
  effectPropertyWidget->setFocusedEffectId(focusedEffectId_);

  const bool hasFocus = !focusedEffectId_.trimmed().isEmpty();
  effectPropertyWidget->setVisible(hasFocus);
  if (effectParametersHintLabel) {
    effectParametersHintLabel->setText(
        hasFocus ? QStringLiteral("Editing effect parameters.")
                 : QStringLiteral("Select an effect to edit its parameters."));
    effectParametersHintLabel->setVisible(!hasFocus);
  }
}

void ArtifactInspectorWidget::Impl::setEffectsStateText(const QString &text,
                                                        bool visible) {
  if (!effectsStateLabel)
    return;
  if (effectsStateLabel->text() == text &&
      effectsStateLabel->isVisible() == visible) {
    return;
  }
  effectsStateLabel->setText(text);
  effectsStateLabel->setVisible(visible);
}

QString ArtifactInspectorWidget::Impl::computeLayerInfoSignature(
    const ArtifactAbstractLayerPtr &layer) const {
  if (!layer) {
    return QStringLiteral("<no-layer>");
  }

  QString signature;
  signature.reserve(256);
  signature += currentCompositionId_.toString();
  signature += QLatin1Char('|');
  signature += layer->id().toString();
  signature += QLatin1Char('|');
  signature += layer->layerName();
  signature += QLatin1Char('|');

  QString layerType = QStringLiteral("Layer");
  if (layer->isNullLayer()) {
    layerType = QStringLiteral("Null Layer");
  } else if (layer->isAdjustmentLayer()) {
    layerType = QStringLiteral("Adjustment Layer");
  }
  signature += layerType;
  signature += QLatin1Char('|');
  signature += QString::number(layer->maskCount());
  signature += QLatin1Char('|');
  signature += layer->layerNote();
  return signature;
}

QString ArtifactInspectorWidget::Impl::computeRackSignature(
    int rackIndex,
    const std::vector<ArtifactAbstractEffectPtr> &effects) const {
  QString signature;
  signature.reserve(512);
  signature += currentCompositionId_.toString();
  signature += QLatin1Char('|');
  signature += currentLayerId_.toString();
  signature += QLatin1Char('|');
  signature += QString::number(rackIndex);
  signature += QLatin1Char('|');
  for (const auto &effect : effects) {
    if (!effect) {
      continue;
    }
    signature += effect->effectID().toQString();
    signature += QLatin1Char('|');
    signature += effect->displayName().toQString();
    signature += QLatin1Char('|');
    signature +=
        effect->isEnabled() ? QStringLiteral("1") : QStringLiteral("0");
    signature += QLatin1Char('|');
  }
  return signature;
}

ArtifactInspectorWidget::Impl::~Impl() {}

void ArtifactInspectorWidget::Impl::rebuildMenu() {}

void ArtifactInspectorWidget::Impl::defaultHandleKeyPressEvent(
    QKeyEvent *event) {}

void ArtifactInspectorWidget::Impl::showContextMenu() {
  showContextMenu(QCursor::pos());
}

void ArtifactInspectorWidget::Impl::showContextMenu(const QPoint &globalPos) {
  QMenu menu;
  menu.addAction("Refresh Inspector", [this]() {
    updateLayerInfo();
    updateEffectsList();
  });
  menu.addSeparator();
  menu.addAction("Show Layer Info Tab", [this]() {
    if (tabWidget)
      tabWidget->setCurrentIndex(0);
  });
  menu.addAction("Show Effects Tab", [this]() {
    if (tabWidget)
      tabWidget->setCurrentIndex(1);
  });
  menu.addSeparator();
  menu.addAction("Expand All Racks", [this]() {
    for (auto &rack : racks) {
      if (rack.listWidget)
        rack.listWidget->setMaximumHeight(10000);
    }
  });
  menu.addAction("Collapse All Racks", [this]() {
    for (auto &rack : racks) {
      if (rack.listWidget)
        rack.listWidget->setMaximumHeight(100);
    }
  });
  menu.exec(globalPos);
}

void ArtifactInspectorWidget::Impl::showRackContextMenu(
    int rackIndex, QListWidgetItem *item, const QPoint &globalPos) {
  QMenu menu;

  if (rackIndex >= 0 && rackIndex < kEffectRackCount) {
    menu.addAction("Add Effect...",
                   [this, rackIndex]() { handleAddEffectClicked(rackIndex); });
  }

  if (!item) {
    menu.addSeparator();
    menu.addAction("Refresh Inspector", [this]() {
      updateLayerInfo();
      updateEffectsList();
    });
    menu.exec(globalPos);
    return;
  }

  const QString effectId = item->data(Qt::UserRole).toString();
  if (effectId.isEmpty()) {
    menu.exec(globalPos);
    return;
  }

  bool isEnabled = false;
  bool found = false;
  auto projectService = ArtifactProjectService::instance();
  if (projectService && !currentCompositionId_.isNil() &&
      !currentLayerId_.isNil()) {
    auto findResult = projectService->findComposition(currentCompositionId_);
    if (findResult.success) {
      auto comp = findResult.ptr.lock();
      if (comp) {
        auto layer = comp->layerById(currentLayerId_);
        if (layer) {
          for (const auto &effect : layer->getEffects()) {
            if (effect && effect->effectID().toQString() == effectId) {
              isEnabled = effect->isEnabled();
              found = true;
              break;
            }
          }
        }
      }
    }
  }

  if (found) {
    QAction *toggleAction =
        menu.addAction(isEnabled ? "Disable Effect" : "Enable Effect");
    QObject::connect(toggleAction, &QAction::triggered,
                     [this, effectId, isEnabled]() {
                       if (setEffectEnabledById(effectId, !isEnabled)) {
                         updateEffectsList();
                       }
                     });
  }

  QAction *moveUpAction = menu.addAction("Move Up");
  QObject::connect(moveUpAction, &QAction::triggered, [this, effectId]() {
    if (moveEffectById(effectId, -1)) {
      updateEffectsList();
      if (statusLabel) {
        statusLabel->setText(QStringLiteral("Status: Effect moved up"));
      }
    }
  });

  QAction *moveDownAction = menu.addAction("Move Down");
  QObject::connect(moveDownAction, &QAction::triggered, [this, effectId]() {
    if (moveEffectById(effectId, 1)) {
      updateEffectsList();
      if (statusLabel) {
        statusLabel->setText(QStringLiteral("Status: Effect moved down"));
      }
    }
  });

  QAction *removeAction = menu.addAction("Remove Effect");
  QObject::connect(removeAction, &QAction::triggered, [this, effectId]() {
    if (removeEffectById(effectId)) {
      updateEffectsList();
    }
  });

  menu.addSeparator();
  QAction *copyIdAction = menu.addAction("Copy Effect ID");
  QObject::connect(copyIdAction, &QAction::triggered, [effectId]() {
    if (auto *cb = QApplication::clipboard()) {
      cb->setText(effectId);
    }
  });

  menu.exec(globalPos);
}

bool ArtifactInspectorWidget::Impl::removeEffectById(const QString &effectId) {
  if (effectId.isEmpty() || currentLayerId_.isNil() ||
      currentCompositionId_.isNil())
    return false;

  auto projectService = ArtifactProjectService::instance();
  if (!projectService)
    return false;

  auto findResult = projectService->findComposition(currentCompositionId_);
  if (!findResult.success)
    return false;

  auto comp = findResult.ptr.lock();
  if (!comp)
    return false;
  Q_UNUSED(comp);

  return projectService->removeEffectFromLayerInCurrentComposition(
      currentLayerId_, effectId);
}

bool ArtifactInspectorWidget::Impl::setEffectEnabledById(
    const QString &effectId, bool enabled) {
  if (effectId.isEmpty() || currentLayerId_.isNil() ||
      currentCompositionId_.isNil())
    return false;

  auto projectService = ArtifactProjectService::instance();
  if (!projectService)
    return false;

  auto findResult = projectService->findComposition(currentCompositionId_);
  if (!findResult.success)
    return false;

  auto comp = findResult.ptr.lock();
  if (!comp)
    return false;
  Q_UNUSED(comp);

  return projectService->setEffectEnabledInLayerInCurrentComposition(
      currentLayerId_, effectId, enabled);
}

bool ArtifactInspectorWidget::Impl::moveEffectById(const QString &effectId,
                                                   int direction) {
  if (effectId.isEmpty() || currentLayerId_.isNil() ||
      currentCompositionId_.isNil())
    return false;

  auto projectService = ArtifactProjectService::instance();
  if (!projectService)
    return false;

  auto findResult = projectService->findComposition(currentCompositionId_);
  if (!findResult.success)
    return false;
  auto comp = findResult.ptr.lock();
  if (!comp)
    return false;
  Q_UNUSED(comp);

  return projectService->moveEffectInLayerInCurrentComposition(
      currentLayerId_, effectId, direction);
}

void ArtifactInspectorWidget::Impl::handleProjectCreated() {
  qDebug() << "[Inspector] Project created";
  containerWidget->setEnabled(true);
  scheduleRefresh(CompositionNoteDirty | LayerNoteDirty | LayerInfoDirty |
                  EffectsDirty);
}

void ArtifactInspectorWidget::Impl::handleProjectClosed() {
  qDebug() << "[Inspector] Project closed";
  setNoProjectState();
}

void ArtifactInspectorWidget::Impl::handleCompositionCreated(
    const CompositionID &id) {
  qDebug() << "[Inspector] Composition created:" << id.toString();
  currentCompositionId_ = id;
  scheduleRefresh(CompositionNoteDirty | LayerNoteDirty | LayerInfoDirty |
                  EffectsDirty);
}

void ArtifactInspectorWidget::Impl::handleCompositionChanged(
    const CompositionID &id) {
  qDebug() << "[Inspector] Composition changed:" << id.toString();
  currentCompositionId_ = id;
  scheduleRefresh(CompositionNoteDirty | LayerNoteDirty | LayerInfoDirty |
                  EffectsDirty);
}

void ArtifactInspectorWidget::Impl::handleLayerSelected(
    const LayerSelectionChangedEvent &event) {
  const LayerID id(event.layerId);
  qDebug() << "[Inspector] Layer selected:" << id.toString()
           << "reason="
           << layerSelectionChangeReasonToString(event.reason);
  if (id.isNil()) {
    auto projectService = ArtifactProjectService::instance();
    if (projectService && !currentCompositionId_.isNil() &&
        !currentLayerId_.isNil()) {
      auto findResult = projectService->findComposition(currentCompositionId_);
      if (findResult.success) {
        auto comp = findResult.ptr.lock();
        if (comp && comp->containsLayerById(currentLayerId_)) {
          syncEffectPropertyWidget();
          scheduleRefresh(LayerNoteDirty | LayerInfoDirty | EffectsDirty);
          return;
        }
      }
    }
    qDebug() << "[Inspector] NoLayer reason="
             << layerSelectionChangeReasonToString(event.reason);
    setNoLayerState();
    scheduleRefresh(LayerNoteDirty | LayerInfoDirty | EffectsDirty);
    return;
  }
  currentLayerId_ = id;
  focusedEffectId_.clear();
  syncEffectPropertyWidget();
  scheduleRefresh(LayerNoteDirty | LayerInfoDirty | EffectsDirty);
}

void ArtifactInspectorWidget::Impl::updateCompositionNote() {
  auto disconnectNoteConnection = [this]() {
    if (compositionNoteConnection_) {
      QObject::disconnect(compositionNoteConnection_);
      compositionNoteConnection_ = {};
    }
  };

  if (!compositionNoteEdit) {
    return;
  }

  auto projectService = ArtifactProjectService::instance();
  if (!projectService || currentCompositionId_.isNil()) {
    disconnectNoteConnection();
    compositionNoteEdit->blockSignals(true);
    compositionNoteEdit->clear();
    compositionNoteEdit->setEnabled(false);
    compositionNoteEdit->blockSignals(false);
    if (compositionNoteGroup) {
      compositionNoteGroup->setEnabled(false);
    }
    return;
  }

  auto findResult = projectService->findComposition(currentCompositionId_);
  if (!findResult.success) {
    disconnectNoteConnection();
    compositionNoteEdit->blockSignals(true);
    compositionNoteEdit->clear();
    compositionNoteEdit->setEnabled(false);
    compositionNoteEdit->blockSignals(false);
    if (compositionNoteGroup) {
      compositionNoteGroup->setEnabled(false);
    }
    return;
  }

  auto comp = findResult.ptr.lock();
  if (!comp) {
    disconnectNoteConnection();
    compositionNoteEdit->blockSignals(true);
    compositionNoteEdit->clear();
    compositionNoteEdit->setEnabled(false);
    compositionNoteEdit->blockSignals(false);
    if (compositionNoteGroup) {
      compositionNoteGroup->setEnabled(false);
    }
    return;
  }

  disconnectNoteConnection();
  compositionNoteConnection_ = QObject::connect(
      comp.get(), &ArtifactAbstractComposition::compositionNoteChanged,
      compositionNoteEdit, [this](const QString &note) {
        if (!compositionNoteEdit) {
          return;
        }
        QSignalBlocker blocker(compositionNoteEdit);
        compositionNoteEdit->setPlainText(note);
        compositionNoteEdit->setEnabled(true);
        if (compositionNoteGroup) {
          compositionNoteGroup->setEnabled(true);
        }
      });

  const QString note = comp->compositionNote();
  if (note == lastCompositionNoteText_) {
    compositionNoteEdit->setEnabled(true);
    if (compositionNoteGroup) {
      compositionNoteGroup->setEnabled(true);
    }
    return;
  }
  lastCompositionNoteText_ = note;
  {
    QSignalBlocker blocker(compositionNoteEdit);
    compositionNoteEdit->setPlainText(note);
    compositionNoteEdit->setEnabled(true);
  }
  if (compositionNoteGroup) {
    compositionNoteGroup->setEnabled(true);
  }
}

void ArtifactInspectorWidget::Impl::updateLayerNote() {
  auto disconnectNoteConnection = [this]() {
    if (layerNoteConnection_) {
      QObject::disconnect(layerNoteConnection_);
      layerNoteConnection_ = {};
    }
  };

  if (!layerNoteEdit) {
    return;
  }

  auto projectService = ArtifactProjectService::instance();
  if (!projectService || currentCompositionId_.isNil() ||
      currentLayerId_.isNil()) {
    disconnectNoteConnection();
    layerNoteEdit->blockSignals(true);
    layerNoteEdit->clear();
    layerNoteEdit->setEnabled(false);
    layerNoteEdit->blockSignals(false);
    if (layerNoteGroup) {
      layerNoteGroup->setEnabled(false);
    }
    return;
  }

  auto findResult = projectService->findComposition(currentCompositionId_);
  if (!findResult.success) {
    disconnectNoteConnection();
    layerNoteEdit->blockSignals(true);
    layerNoteEdit->clear();
    layerNoteEdit->setEnabled(false);
    layerNoteEdit->blockSignals(false);
    if (layerNoteGroup) {
      layerNoteGroup->setEnabled(false);
    }
    return;
  }

  auto comp = findResult.ptr.lock();
  if (!comp || !comp->containsLayerById(currentLayerId_)) {
    disconnectNoteConnection();
    layerNoteEdit->blockSignals(true);
    layerNoteEdit->clear();
    layerNoteEdit->setEnabled(false);
    layerNoteEdit->blockSignals(false);
    if (layerNoteGroup) {
      layerNoteGroup->setEnabled(false);
    }
    return;
  }

  auto layer = comp->layerById(currentLayerId_);
  if (!layer) {
    disconnectNoteConnection();
    layerNoteEdit->blockSignals(true);
    layerNoteEdit->clear();
    layerNoteEdit->setEnabled(false);
    layerNoteEdit->blockSignals(false);
    if (layerNoteGroup) {
      layerNoteGroup->setEnabled(false);
    }
    return;
  }

  disconnectNoteConnection();
  layerNoteConnection_ =
      QObject::connect(layer.get(), &ArtifactAbstractLayer::layerNoteChanged,
                       layerNoteEdit, [this](const QString &note) {
                         if (!layerNoteEdit) {
                           return;
                         }
                         QSignalBlocker blocker(layerNoteEdit);
                         layerNoteEdit->setPlainText(note);
                         layerNoteEdit->setEnabled(true);
                         if (layerNoteGroup) {
                           layerNoteGroup->setEnabled(true);
                         }
                       });

  const QString note = layer->layerNote();
  if (note == lastLayerNoteText_) {
    layerNoteEdit->setEnabled(true);
    if (layerNoteGroup) {
      layerNoteGroup->setEnabled(true);
    }
    return;
  }
  lastLayerNoteText_ = note;
  {
    QSignalBlocker blocker(layerNoteEdit);
    layerNoteEdit->setPlainText(note);
    layerNoteEdit->setEnabled(true);
  }
  if (layerNoteGroup) {
    layerNoteGroup->setEnabled(true);
  }
}

void ArtifactInspectorWidget::Impl::updateLayerInfo() {
  if (currentLayerId_.isNil()) {
    setNoLayerState();
    return;
  }

  // レイヤー情報を取得
  auto projectService = ArtifactProjectService::instance();
  if (!projectService) {
    setNoProjectState();
    return;
  }

  // コンポジションを取得
  if (currentCompositionId_.isNil()) {
    // イベントで compositionId が届かなかった場合のフォールバック
    if (auto comp = projectService->currentComposition().lock()) {
      currentCompositionId_ = comp->id();
    } else {
      setNoLayerState();
      return;
    }
  }

  auto findResult = projectService->findComposition(currentCompositionId_);
  if (!findResult.success) {
    setNoLayerState();
    return;
  }

  auto comp = findResult.ptr.lock();
  if (!comp) {
    setNoLayerState();
    return;
  }

  // レイヤーを取得
  if (!comp->containsLayerById(currentLayerId_)) {
    setNoLayerState();
    return;
  }

  auto layer = comp->layerById(currentLayerId_);
  if (!layer) {
    setNoLayerState();
    return;
  }

  const QString nextSignature = computeLayerInfoSignature(layer);
  if (nextSignature == lastLayerInfoSignature_) {
    return;
  }
  lastLayerInfoSignature_ = nextSignature;

  // レイヤー情報を表示
  QString layerName = layer->layerName();
  layerNameLabel->setText(
      QString("Layer: %1").arg(layerName.isEmpty() ? "(Unnamed)" : layerName));
  {
    const auto theme = ArtifactCore::currentDCCTheme();
    QFont nameFont = layerNameLabel->font();
    nameFont.setBold(true);
    nameFont.setPointSize(nameFont.pointSize() + 1);
    layerNameLabel->setFont(nameFont);
    applyInspectorLabelPalette(layerNameLabel, true);

    QFont typeFont = layerTypeLabel->font();
    typeFont.setBold(true);
    layerTypeLabel->setFont(typeFont);
    applyInspectorLabelPalette(layerTypeLabel, false);
  }

  // レイヤータイプを判定
  QString layerType = "Unknown";
  if (layer->isNullLayer()) {
    layerType = "Null Layer";
  } else if (layer->isAdjustmentLayer()) {
    layerType = "Adjustment Layer";
  } else {
    // TODO: 他のレイヤータイプも判定
    layerType = "Layer";
  }
  layerTypeLabel->setText(QString("Type: %1").arg(layerType));

  const int maskCount = layer->maskCount();
  const QString maskText = maskCount > 0
                               ? QStringLiteral("Masks: %1").arg(maskCount)
                               : QStringLiteral("Masks: none");
  statusLabel->setText(QString("Status: Layer selected - ID: %1 | %2")
                           .arg(currentLayerId_.toString(), maskText));
  {
    const auto theme = ArtifactCore::currentDCCTheme();
    applyInspectorLabelPalette(statusLabel, true);
  }
  setEffectsStateText(
      maskCount > 0
          ? QStringLiteral("Mask / roto editing is available for this layer.")
          : QStringLiteral(
                "No masks on this layer. Use Mask tool to create one."),
      true);

  qDebug() << "[Inspector] Updated layer info:" << layerName
           << "Type:" << layerType;
}

void ArtifactInspectorWidget::Impl::setNoProjectState() {
  containerWidget->setEnabled(false);
  if (compositionNoteConnection_) {
    QObject::disconnect(compositionNoteConnection_);
    compositionNoteConnection_ = {};
  }
  if (layerNoteConnection_) {
    QObject::disconnect(layerNoteConnection_);
    layerNoteConnection_ = {};
  }
  if (compositionNoteEdit) {
    compositionNoteEdit->blockSignals(true);
    compositionNoteEdit->clear();
    compositionNoteEdit->setEnabled(false);
    compositionNoteEdit->blockSignals(false);
  }
  if (compositionNoteGroup) {
    compositionNoteGroup->setEnabled(false);
  }
  if (layerNoteEdit) {
    layerNoteEdit->blockSignals(true);
    layerNoteEdit->clear();
    layerNoteEdit->setEnabled(false);
    layerNoteEdit->blockSignals(false);
  }
  if (layerNoteGroup) {
    layerNoteGroup->setEnabled(false);
  }
  layerNameLabel->setText("Layer: (No project)");
  layerTypeLabel->setText("Type: N/A");
  statusLabel->setText("Status: Create or open a project");
  currentCompositionId_ = CompositionID();
  currentLayerId_ = LayerID();
  lastLayerInfoSignature_.clear();
  lastCompositionNoteText_.clear();
  lastLayerNoteText_.clear();
  lastRackSignatures_.fill(QString());
  refreshMask_ = 0;
  refreshQueued_ = false;
  focusedEffectId_.clear();
  if (effectPropertyWidget) {
    effectPropertyWidget->clear();
    effectPropertyWidget->setVisible(false);
  }
  if (effectParametersHintLabel) {
    effectParametersHintLabel->setText(
        QStringLiteral("Select a layer and effect to edit parameters."));
    effectParametersHintLabel->setVisible(true);
  }
  setEffectRackEnabled(false);
  setEffectsStateText("Create or open a project to manage effects.", true);
}

void ArtifactInspectorWidget::Impl::setNoLayerState() {
  layerNameLabel->setText("Layer: (No layer selected)");
  layerTypeLabel->setText("Type: N/A");
  statusLabel->setText("Status: Select a layer or create one");
  currentLayerId_ = LayerID();
  if (layerNoteConnection_) {
    QObject::disconnect(layerNoteConnection_);
    layerNoteConnection_ = {};
  }
  if (layerNoteEdit) {
    layerNoteEdit->blockSignals(true);
    layerNoteEdit->clear();
    layerNoteEdit->setEnabled(false);
    layerNoteEdit->blockSignals(false);
  }
  if (layerNoteGroup) {
    layerNoteGroup->setEnabled(false);
  }

  // エフェクトリストもクリア
  for (auto &rack : racks) {
    if (rack.listWidget) {
      rack.listWidget->clear();
    }
  }
  lastLayerInfoSignature_.clear();
  lastLayerNoteText_.clear();
  lastRackSignatures_.fill(QString());
  refreshMask_ = 0;
  refreshQueued_ = false;
  focusedEffectId_.clear();
  if (effectPropertyWidget) {
    effectPropertyWidget->clear();
    effectPropertyWidget->setVisible(false);
  }
  if (effectParametersHintLabel) {
    effectParametersHintLabel->setText(
        QStringLiteral("Select an effect to edit its parameters."));
    effectParametersHintLabel->setVisible(true);
  }
  setEffectRackEnabled(false);
  setEffectsStateText("Select a layer to manage effects.", true);
  refreshRackButtons();
}

void ArtifactInspectorWidget::Impl::setEffectRackEnabled(bool enabled) {
  for (auto &rack : racks) {
    if (rack.listWidget) {
      rack.listWidget->setEnabled(enabled);
    }
    if (rack.addButton) {
      rack.addButton->setEnabled(enabled);
    }
    if (rack.removeButton) {
      rack.removeButton->setEnabled(false);
    }
    if (rack.moveUpButton) {
      rack.moveUpButton->setEnabled(false);
    }
    if (rack.moveDownButton) {
      rack.moveDownButton->setEnabled(false);
    }
  }
}

void ArtifactInspectorWidget::Impl::refreshRackButtons() {
  const bool canEdit =
      !currentLayerId_.isNil() && !currentCompositionId_.isNil();
  for (auto &rack : racks) {
    if (rack.addButton) {
      rack.addButton->setEnabled(canEdit);
    }
    if (!rack.removeButton || !rack.listWidget) {
      continue;
    }
    auto *current = rack.listWidget->currentItem();
    const bool hasEffectItem =
        canEdit && current &&
        current->data(Qt::UserRole).toString().trimmed().size() > 0;
    rack.removeButton->setEnabled(hasEffectItem);
    if (rack.moveUpButton) {
      rack.moveUpButton->setEnabled(hasEffectItem);
    }
    if (rack.moveDownButton) {
      rack.moveDownButton->setEnabled(hasEffectItem);
    }
  }
}

void ArtifactInspectorWidget::Impl::updateEffectsList() {
  if (currentLayerId_.isNil()) {
    setEffectRackEnabled(false);
    setEffectsStateText("Select a layer to manage effects.", true);
    refreshRackButtons();
    return;
  }

  auto projectService = ArtifactProjectService::instance();
  if (!projectService) {
    setEffectRackEnabled(false);
    setEffectsStateText("Create or open a project to manage effects.", true);
    refreshRackButtons();
    return;
  }

  if (currentCompositionId_.isNil()) {
    setEffectRackEnabled(false);
    setEffectsStateText("Open a composition to manage effects.", true);
    refreshRackButtons();
    return;
  }

  auto findResult = projectService->findComposition(currentCompositionId_);
  if (!findResult.success) {
    setEffectRackEnabled(false);
    setEffectsStateText("Open a composition to manage effects.", true);
    refreshRackButtons();
    return;
  }

  auto comp = findResult.ptr.lock();
  if (!comp) {
    setEffectRackEnabled(false);
    setEffectsStateText("Open a composition to manage effects.", true);
    refreshRackButtons();
    return;
  }

  auto layer = comp->layerById(currentLayerId_);
  if (!layer) {
    setEffectRackEnabled(false);
    setEffectsStateText("Select a layer to manage effects.", true);
    refreshRackButtons();
    return;
  }

  auto effects = layer->getEffects();
  setEffectRackEnabled(true);
  int effectCount = 0;
  std::array<std::vector<ArtifactAbstractEffectPtr>, kEffectRackCount>
      rackEffects;

  for (const auto &effect : effects) {
    if (effect) {
      ++effectCount;
      const int rackIdx = rackIndexFromStage(effect->pipelineStage());
      if (rackIdx >= 0) {
        rackEffects[rackIdx].push_back(effect);
      }
    }
  }

  for (int i = 0; i < kEffectRackCount; ++i) {
    const QString rackSignature = computeRackSignature(i, rackEffects[i]);
    if (rackSignature == lastRackSignatures_[i]) {
      continue;
    }
    lastRackSignatures_[i] = rackSignature;

    if (!racks[i].listWidget) {
      continue;
    }
    racks[i].listWidget->clear();
    if (rackEffects[i].empty()) {
      auto item = new QListWidgetItem("(No effects)");
      item->setFlags(item->flags() & ~Qt::ItemIsSelectable);
      racks[i].listWidget->addItem(item);
      continue;
    }
    for (const auto &effect : rackEffects[i]) {
      if (!effect) {
        continue;
      }
      QString effectName = effect->displayName().toQString();
      QString effectStatus = effect->isEnabled() ? "✓" : "✗";
      QString itemText = QString("[%1] %2").arg(effectStatus, effectName);
      auto *item = new QListWidgetItem(itemText);
      item->setData(Qt::UserRole, effect->effectID().toQString());
      racks[i].listWidget->addItem(item);
    }
  }

  if (effectCount == 0) {
    setEffectsStateText("No effects on the selected layer yet.", true);
  } else {
    setEffectsStateText(QString(), false);
  }
  refreshRackButtons();

  if (!focusedEffectId_.trimmed().isEmpty()) {
    for (int rackIndex = 0; rackIndex < kEffectRackCount; ++rackIndex) {
      auto *list = racks[rackIndex].listWidget;
      if (!list) {
        continue;
      }
      for (int row = 0; row < list->count(); ++row) {
        auto *item = list->item(row);
        if (!item) {
          continue;
        }
        if (item->data(Qt::UserRole).toString().trimmed() == focusedEffectId_) {
          const QSignalBlocker blocker(list);
          list->setCurrentItem(item);
          break;
        }
      }
    }
  }

  syncEffectPropertyWidget();
}

void ArtifactInspectorWidget::Impl::handleAddEffectClicked(int rackIndex) {
  if (currentLayerId_.isNil() || currentCompositionId_.isNil())
    return;

  auto projectService = ArtifactProjectService::instance();
  if (!projectService)
    return;

  auto findResult = projectService->findComposition(currentCompositionId_);
  if (!findResult.success)
    return;

  auto comp = findResult.ptr.lock();
  if (!comp)
    return;

  auto layer = comp->layerById(currentLayerId_);
  if (!layer)
    return;

  QMenu effectMenu;

  auto addAndRefresh = [this, projectService](
                           std::shared_ptr<ArtifactAbstractEffect> newEffect) {
    if (newEffect) {
      // Generate a simple unique ID for the effect for now
      newEffect->setEffectID(
          ArtifactCore::UniString(std::to_string(std::rand()).c_str()));
      if (projectService->addEffectToLayerInCurrentComposition(currentLayerId_,
                                                               newEffect)) {
        updateEffectsList();
        if (statusLabel) {
          statusLabel->setText(QStringLiteral("Status: Effect added - %1")
                                   .arg(newEffect->displayName().toQString()));
        }
        if (tabWidget) {
          tabWidget->setCurrentIndex(1); // Effects
        }
      }
    }
  };

  switch (stageFromRackIndex(rackIndex)) {
  case EffectPipelineStage::Generator:
    effectMenu.addAction("Cloner", [addAndRefresh]() {
      addAndRefresh(std::make_shared<ClonerGenerator>());
    });
    effectMenu.addAction("Fractal Noise", [addAndRefresh]() {
      addAndRefresh(std::make_shared<FractalNoiseGenerator>());
    });
    break;
  case EffectPipelineStage::GeometryTransform:
    effectMenu.addAction("Twist", [addAndRefresh]() {
      addAndRefresh(std::make_shared<TwistTransform>());
    });
    effectMenu.addAction("Bend", [addAndRefresh]() {
      addAndRefresh(std::make_shared<BendTransform>());
    });
    break;
  case EffectPipelineStage::MaterialRender:
    effectMenu.addAction("PBR Material", [addAndRefresh]() {
      addAndRefresh(std::make_shared<PBRMaterialEffect>());
    });
    break;
  case EffectPipelineStage::Rasterizer:
    effectMenu.addAction("Blur", [addAndRefresh]() {
      addAndRefresh(std::make_shared<BlurEffect>());
    });
    effectMenu.addAction("Gaussian Blur", [addAndRefresh]() {
      addAndRefresh(std::make_shared<GaussianBlur>());
    });
    effectMenu.addAction("Glow", [addAndRefresh]() {
      addAndRefresh(std::make_shared<GlowEffect>());
    });
    effectMenu.addAction("Drop Shadow", [addAndRefresh]() {
      addAndRefresh(std::make_shared<DropShadowEffect>());
    });
    effectMenu.addSeparator();
    effectMenu.addAction("Brightness / Contrast", [addAndRefresh]() {
      addAndRefresh(std::make_shared<BrightnessEffect>());
    });
    effectMenu.addAction("Exposure", [addAndRefresh]() {
      addAndRefresh(std::make_shared<ExposureEffect>());
    });
    effectMenu.addAction("Hue / Saturation", [addAndRefresh]() {
      addAndRefresh(std::make_shared<HueAndSaturation>());
    });
    effectMenu.addSeparator();
    effectMenu.addAction("Chroma Key", [addAndRefresh]() {
      addAndRefresh(std::make_shared<ChromaKeyEffect>());
    });
    effectMenu.addAction("Wave", [addAndRefresh]() {
      addAndRefresh(std::make_shared<WaveEffect>());
    });
    effectMenu.addAction("Spherize", [addAndRefresh]() {
      addAndRefresh(std::make_shared<SpherizeEffect>());
    });
    break;
  case EffectPipelineStage::LayerTransform:
    effectMenu.addAction("Transform 2D", [addAndRefresh]() {
      addAndRefresh(std::make_shared<LayerTransform2D>());
    });
    break;
  }

  effectMenu.exec(QCursor::pos());
}

void ArtifactInspectorWidget::Impl::handleAddGeneratorEffect(int rackIndex) {
  // Obsolete function. Kept temporarily to appease class signature.
}

void ArtifactInspectorWidget::Impl::handleRemoveEffectClicked(int rackIndex) {
  if (rackIndex < 0 || rackIndex >= kEffectRackCount)
    return;
  if (!racks[rackIndex].listWidget)
    return;

  auto selectedItems = racks[rackIndex].listWidget->selectedItems();
  if (selectedItems.isEmpty())
    return;

  if (currentLayerId_.isNil() || currentCompositionId_.isNil())
    return;

  auto projectService = ArtifactProjectService::instance();
  if (!projectService)
    return;

  auto findResult = projectService->findComposition(currentCompositionId_);
  if (!findResult.success)
    return;

  auto comp = findResult.ptr.lock();
  if (!comp)
    return;

  auto layer = comp->layerById(currentLayerId_);
  if (!layer)
    return;
  Q_UNUSED(layer);

  if (!ArtifactMessageBox::confirmDelete(
          containerWidget, QStringLiteral("Remove Effect"),
          QStringLiteral("選択したエフェクトを削除しますか？"))) {
    return;
  }

  int removedCount = 0;
  for (auto item : selectedItems) {
    UniString effectID(item->data(Qt::UserRole).toString().toStdString());
    if (effectID.length() > 0) {
      if (projectService->removeEffectFromLayerInCurrentComposition(
              currentLayerId_, effectID.toQString())) {
        qDebug() << "[Inspector] Effect removed:" << effectID.toQString();
        ++removedCount;
      }
    }
  }

  updateEffectsList();
  if (removedCount > 0 && statusLabel) {
    statusLabel->setText(
        QStringLiteral("Status: Removed %1 effect(s)").arg(removedCount));
  }
}

void ArtifactInspectorWidget::update() {}

ArtifactInspectorWidget::ArtifactInspectorWidget(QWidget *parent /*= nullptr*/)
    : QScrollArea(parent), impl_(new Impl()) {
  // メインレイアウト
  auto mainLayout = new QVBoxLayout();
  impl_->containerWidget = new QWidget();
  applyInspectorPalette(impl_->containerWidget);

  // タブウィジェットを作成
  impl_->tabWidget = new QTabWidget();
  applyInspectorPalette(impl_->tabWidget);

  // ================== Layer Info Tab ==================
  auto layerInfoWidget = new QWidget();
  auto layerInfoLayout = new QVBoxLayout();

  impl_->compositionNoteGroup = new QGroupBox("Composition Note");
  applyInspectorSectionBox(impl_->compositionNoteGroup);
  auto compositionNoteLayout = new QVBoxLayout();
  impl_->compositionNoteEdit = new QPlainTextEdit();
  impl_->compositionNoteEdit->setPlaceholderText(
      "Write quick notes for this composition...");
  impl_->compositionNoteEdit->setMinimumHeight(120);
  applyInspectorTextEdit(impl_->compositionNoteEdit);
  compositionNoteLayout->addWidget(impl_->compositionNoteEdit);
  compositionNoteLayout->setContentsMargins(
      kInspectorNoteMargin, kInspectorNoteMargin, kInspectorNoteMargin,
      kInspectorNoteMargin);
  impl_->compositionNoteGroup->setLayout(compositionNoteLayout);
  impl_->compositionNoteGroup->hide();
  layerInfoLayout->addWidget(impl_->compositionNoteGroup);

  impl_->layerNoteGroup = new QGroupBox("Layer Note");
  applyInspectorSectionBox(impl_->layerNoteGroup);
  auto layerNoteLayout = new QVBoxLayout();
  impl_->layerNoteEdit = new QPlainTextEdit();
  impl_->layerNoteEdit->setPlaceholderText(
      "Write quick notes for the selected layer...");
  impl_->layerNoteEdit->setMinimumHeight(110);
  applyInspectorTextEdit(impl_->layerNoteEdit);
  layerNoteLayout->addWidget(impl_->layerNoteEdit);
  layerNoteLayout->setContentsMargins(
      kInspectorNoteMargin, kInspectorNoteMargin, kInspectorNoteMargin,
      kInspectorNoteMargin);
  impl_->layerNoteGroup->setLayout(layerNoteLayout);
  impl_->layerNoteGroup->hide();
  layerInfoLayout->addWidget(impl_->layerNoteGroup);

  // ステータスラベル
  impl_->statusLabel = new QLabel("Status: No project");
  {
    QFont f = impl_->statusLabel->font();
    f.setItalic(true);
    impl_->statusLabel->setFont(f);
    applyInspectorLabelPalette(impl_->statusLabel, false);
  }
  layerInfoLayout->addWidget(impl_->statusLabel);

  // レイヤー名ラベル
  impl_->layerNameLabel = new QLabel("Layer: (No project)");
  {
    QFont f = impl_->layerNameLabel->font();
    f.setBold(true);
    impl_->layerNameLabel->setFont(f);
    applyInspectorLabelPalette(impl_->layerNameLabel, true);
  }
  layerInfoLayout->addWidget(impl_->layerNameLabel);

  // レイヤータイプラベル
  impl_->layerTypeLabel = new QLabel("Type: N/A");
  applyInspectorLabelPalette(impl_->layerTypeLabel, false);
  layerInfoLayout->addWidget(impl_->layerTypeLabel);

  layerInfoLayout->setAlignment(Qt::AlignTop);
  layerInfoLayout->setContentsMargins(
      kInspectorSectionMarginL, kInspectorSectionMarginT,
      kInspectorSectionMarginR, kInspectorSectionMarginB);
  layerInfoLayout->setSpacing(kInspectorSectionSpacing);

  QObject::connect(
      impl_->compositionNoteEdit, &QPlainTextEdit::textChanged, this, [this]() {
        if (!impl_->compositionNoteEdit ||
            impl_->currentCompositionId_.isNil()) {
          return;
        }
        auto projectService = ArtifactProjectService::instance();
        if (!projectService) {
          return;
        }
        auto findResult =
            projectService->findComposition(impl_->currentCompositionId_);
        if (!findResult.success) {
          return;
        }
        auto comp = findResult.ptr.lock();
        if (!comp) {
          return;
        }
        comp->setCompositionNote(impl_->compositionNoteEdit->toPlainText());
      });

  QObject::connect(
      impl_->layerNoteEdit, &QPlainTextEdit::textChanged, this, [this]() {
        if (!impl_->layerNoteEdit || impl_->currentCompositionId_.isNil() ||
            impl_->currentLayerId_.isNil()) {
          return;
        }
        auto projectService = ArtifactProjectService::instance();
        if (!projectService) {
          return;
        }
        auto findResult =
            projectService->findComposition(impl_->currentCompositionId_);
        if (!findResult.success) {
          return;
        }
        auto comp = findResult.ptr.lock();
        if (!comp) {
          return;
        }
        auto layer = comp->layerById(impl_->currentLayerId_);
        if (!layer) {
          return;
        }
        layer->setLayerNote(impl_->layerNoteEdit->toPlainText());
      });

  layerInfoWidget->setLayout(layerInfoLayout);
  impl_->tabWidget->addTab(layerInfoWidget, "Layer Info");

  // ================== Effects Pipeline Tab ==================
  impl_->effectsScrollArea = new QScrollArea();
  impl_->effectsScrollArea->setWidgetResizable(true);
  impl_->effectsTabWidget = new QWidget();
  auto effectsLayout = new QVBoxLayout();
  impl_->effectsStateLabel =
      new QLabel("Create or open a project to manage effects.");
  impl_->effectsStateLabel->setWordWrap(true);
  applyInspectorLabelPalette(impl_->effectsStateLabel, false);
  effectsLayout->addWidget(impl_->effectsStateLabel);

  impl_->effectParametersHintLabel =
      new QLabel("Select an effect to edit its parameters.");
  impl_->effectParametersHintLabel->setWordWrap(true);
  applyInspectorLabelPalette(impl_->effectParametersHintLabel, false);
  effectsLayout->addWidget(impl_->effectParametersHintLabel);

  impl_->effectPropertyWidget = new ArtifactPropertyWidget();
  impl_->effectPropertyWidget->setVisible(false);
  impl_->effectPropertyWidget->setMinimumHeight(220);
  effectsLayout->addWidget(impl_->effectPropertyWidget);

  QString rackNames[5] = {"1. Generator", "2. Geometry Transform",
                          "3. Material & Render", "4. Rasterizer",
                          "5. Layer Transform"};

  for (int i = 0; i < 5; ++i) {
    auto rackGroup = new QGroupBox(rackNames[i]);
    applyInspectorSectionBox(rackGroup);
    auto rackLayout = new QVBoxLayout();

    impl_->racks[i].listWidget = new QListWidget();
    impl_->racks[i].listWidget->setMaximumHeight(100);
    applyInspectorList(impl_->racks[i].listWidget);
    impl_->racks[i].listWidget->setContextMenuPolicy(Qt::CustomContextMenu);
    if (impl_->racks[i].listWidget->viewport()) {
      impl_->racks[i].listWidget->viewport()->setContextMenuPolicy(
          Qt::CustomContextMenu);
    }

    auto btnLayout = new QHBoxLayout();
    impl_->racks[i].addButton = new QPushButton("+ Add");
    impl_->racks[i].removeButton = new QPushButton("- Remove");
    impl_->racks[i].moveUpButton = new QPushButton("Up");
    impl_->racks[i].moveDownButton = new QPushButton("Down");
    applyInspectorButton(impl_->racks[i].addButton, true);
    applyInspectorButton(impl_->racks[i].removeButton, false);
    applyInspectorButton(impl_->racks[i].moveUpButton, false);
    applyInspectorButton(impl_->racks[i].moveDownButton, false);
    btnLayout->addWidget(impl_->racks[i].addButton);
    btnLayout->addWidget(impl_->racks[i].removeButton);
    btnLayout->addWidget(impl_->racks[i].moveUpButton);
    btnLayout->addWidget(impl_->racks[i].moveDownButton);

    rackLayout->addWidget(impl_->racks[i].listWidget);
    rackLayout->addLayout(btnLayout);
    rackLayout->setContentsMargins(kInspectorRackMarginL, kInspectorRackMarginT,
                                   kInspectorRackMarginR,
                                   kInspectorRackMarginB);
    rackGroup->setLayout(rackLayout);

    effectsLayout->addWidget(rackGroup);

    // Button signals
    QObject::connect(impl_->racks[i].addButton, &QPushButton::clicked, this,
                     [this, i]() { impl_->handleAddEffectClicked(i); });
    QObject::connect(impl_->racks[i].removeButton, &QPushButton::clicked, this,
                     [this, i]() { impl_->handleRemoveEffectClicked(i); });
    QObject::connect(
        impl_->racks[i].moveUpButton, &QPushButton::clicked, this, [this, i]() {
          auto *list = impl_->racks[i].listWidget;
          if (!list)
            return;
          auto *item = list->currentItem();
          if (!item)
            return;
          const QString effectId = item->data(Qt::UserRole).toString();
          if (effectId.trimmed().isEmpty())
            return;
          if (impl_->moveEffectById(effectId, -1)) {
            impl_->updateEffectsList();
            if (impl_->statusLabel) {
              impl_->statusLabel->setText(
                  QStringLiteral("Status: Effect moved up"));
            }
          }
        });
    QObject::connect(impl_->racks[i].moveDownButton, &QPushButton::clicked,
                     this, [this, i]() {
                       auto *list = impl_->racks[i].listWidget;
                       if (!list)
                         return;
                       auto *item = list->currentItem();
                       if (!item)
                         return;
                       const QString effectId =
                           item->data(Qt::UserRole).toString();
                       if (effectId.trimmed().isEmpty())
                         return;
                       if (impl_->moveEffectById(effectId, 1)) {
                         impl_->updateEffectsList();
                         if (impl_->statusLabel) {
                           impl_->statusLabel->setText(
                               QStringLiteral("Status: Effect moved down"));
                         }
                       }
                     });
    QObject::connect(
        impl_->racks[i].listWidget, &QListWidget::customContextMenuRequested,
        this, [this, i](const QPoint &pos) {
          auto *lw = impl_->racks[i].listWidget;
          if (!lw)
            return;
          QListWidgetItem *item = lw->itemAt(pos);
          impl_->showRackContextMenu(i, item, lw->viewport()->mapToGlobal(pos));
        });
    if (impl_->racks[i].listWidget->viewport()) {
      QObject::connect(impl_->racks[i].listWidget->viewport(),
                       &QWidget::customContextMenuRequested, this,
                       [this, i](const QPoint &pos) {
                         auto *lw = impl_->racks[i].listWidget;
                         if (!lw)
                           return;
                         QListWidgetItem *item = lw->itemAt(pos);
                         impl_->showRackContextMenu(
                             i, item, lw->viewport()->mapToGlobal(pos));
                       });
    }
    QObject::connect(
        impl_->racks[i].listWidget, &QListWidget::currentItemChanged, this,
        [this](QListWidgetItem *, QListWidgetItem *) {
          QString focusedEffectId;
          for (int rackIndex = 0; rackIndex < kEffectRackCount; ++rackIndex) {
            auto *list = impl_->racks[rackIndex].listWidget;
            if (!list)
              continue;
            auto *item = list->currentItem();
            if (!item)
              continue;
            const QString id = item->data(Qt::UserRole).toString().trimmed();
            if (!id.isEmpty()) {
              focusedEffectId = id;
              break;
            }
          }
          impl_->updatePropertiesForEffect(focusedEffectId);
          impl_->refreshRackButtons();
        });
    QObject::connect(
        impl_->racks[i].listWidget, &QListWidget::itemDoubleClicked, this,
        [this](QListWidgetItem *item) {
          if (!item)
            return;
          const QString effectId = item->data(Qt::UserRole).toString();
          if (effectId.trimmed().isEmpty())
            return;
          const bool isEnabled = item->text().startsWith("[✓]");
          if (impl_->setEffectEnabledById(effectId, !isEnabled)) {
            impl_->updateEffectsList();
            if (impl_->statusLabel) {
              impl_->statusLabel->setText(
                  QStringLiteral("Status: Effect %1")
                      .arg(!isEnabled ? "enabled" : "disabled"));
            }
          }
        });
  }

  effectsLayout->addStretch();
  effectsLayout->setContentsMargins(
      kInspectorSectionMarginL, kInspectorSectionMarginT,
      kInspectorSectionMarginR, kInspectorSectionMarginB);
  effectsLayout->setSpacing(8);

  impl_->effectsTabWidget->setLayout(effectsLayout);
  impl_->effectsScrollArea->setWidget(impl_->effectsTabWidget);
  impl_->tabWidget->addTab(impl_->effectsScrollArea, "Effects");

  // タブをメインレイアウトに追加
  mainLayout->addWidget(impl_->tabWidget);
  mainLayout->setContentsMargins(0, 0, 0, 0);
  mainLayout->setSpacing(0);

  impl_->containerWidget->setLayout(mainLayout);

  setWidget(impl_->containerWidget);
  setWidgetResizable(true);

  // 初期状態: プロジェクトなし -> 無効化
  impl_->setNoProjectState();

  impl_->eventBusSubscriptions_.push_back(
      impl_->eventBus_.subscribe<ProjectChangedEvent>(
          [this](const ProjectChangedEvent &) {
            if (!impl_) {
              return;
            }
            impl_->handleProjectCreated();
          }));
  impl_->eventBusSubscriptions_.push_back(
      impl_->eventBus_.subscribe<CurrentCompositionChangedEvent>(
          [this](const CurrentCompositionChangedEvent &event) {
            if (!impl_) {
              return;
            }
            const CompositionID cid(event.compositionId);
            impl_->handleCompositionChanged(cid);
          }));
  impl_->eventBusSubscriptions_.push_back(
      impl_->eventBus_.subscribe<LayerSelectionChangedEvent>(
          [this](const LayerSelectionChangedEvent &event) {
            if (!impl_) {
              return;
            }
            const CompositionID cid(event.compositionId);
            // compositionId が nil の場合は既存の currentCompositionId_
            // を上書きしない。 nil を代入すると updateLayerInfo の nil
            // チェックで即 return してしまう。
            if (!cid.isNil()) {
              impl_->currentCompositionId_ = cid;
            } else if (impl_->currentCompositionId_.isNil()) {
              // フォールバック: サービスから直接取得
              if (auto *svc = ArtifactProjectService::instance()) {
                if (auto comp = svc->currentComposition().lock()) {
                  impl_->currentCompositionId_ = comp->id();
                }
              }
            }
            impl_->handleLayerSelected(event);
          }));
  impl_->eventBusSubscriptions_.push_back(
      impl_->eventBus_.subscribe<LayerChangedEvent>(
          [this](const LayerChangedEvent &event) {
            if (!impl_ ||
                event.changeType != LayerChangedEvent::ChangeType::Created) {
              return;
            }
            const CompositionID cid(event.compositionId);
            const LayerID lid(event.layerId);
            if (cid.isNil() || lid.isNil())
              return;
            // 追加先コンポジションが現在表示中のコンポジションと一致する場合、追加レイヤーを自動選択
            const bool cidMatches = !impl_->currentCompositionId_.isNil() &&
                                    cid == impl_->currentCompositionId_;
            if (cidMatches) {
              impl_->handleLayerSelected(LayerSelectionChangedEvent{
                  event.compositionId,
                  event.layerId,
                  LayerSelectionChangeReason::SelectionBridgeSync});
            }
          }));
  impl_->refreshRackButtons();
}

ArtifactInspectorWidget::~ArtifactInspectorWidget() { delete impl_; }

QSize ArtifactInspectorWidget::sizeHint() const { return QSize(300, 600); }

void ArtifactInspectorWidget::clear() { update(); }

void ArtifactInspectorWidget::contextMenuEvent(QContextMenuEvent *event) {
  if (!impl_ || !event)
    return;
  impl_->showContextMenu(event->globalPos());
}

} // namespace Artifact
