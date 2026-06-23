module;
#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QContextMenuEvent>
#include <QCursor>
#include <QFocusEvent>
#include <QDesktopServices>
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
#include <QFileInfo>
#include <QObject>
#include <QPalette>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QKeyEvent>
#include <QScopeGuard>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QStringList>
#include <QTabWidget>
#include <QTimer>
#include <QVBoxLayout>
#include <QVariant>
#include <QUrl>
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
import Artifact.Layer.Matte;
import Artifact.Layer.Video;
import Artifact.Event.Types;
import Event.Bus;
import Undo.UndoManager;
import Input.Operator;
import Generator.Effector;
import Artifact.Effect.Generator.Cloner;
import Artifact.Effect.Generator.FractalNoise;
import Artifact.Effect.Generator.ProceduralTexture;
import Artifact.Effect.Transform.Twist;
import Artifact.Effect.Transform.Bend;
import Artifact.Effect.Render.PBRMaterial;
import Artifact.Effect.LayerTransform.Transform2D;
import Artifact.Effect.Rasterizer.Blur;
import Artifact.Effect.Rasterizer.DropShadow;
import Artifact.Effect.DirectionalGlow;
import BrightnessEffect;
import ExposureEffect;
import HueAndSaturation;
import ColorWheelsEffect;
import CurvesEffect;
import Artifact.Effect.WhiteBalance;
import PhotoFilterEffect;
import GradientRampEffect;
import FillEffect;
import TritoneEffect;
import ColoramaEffect;
import ColorBalanceEffect;
import LevelsEffect;
import ChannelMixerEffect;
import SelectiveColorEffect;
import Artifact.Effect.Glow;
import Artifact.Effect.Glow.EdgeBloom;
import Artifact.Effect.Glow.ChromaticGlow;
import Artifact.Effect.Glow.ReactiveGlow;
import Artifact.Effect.GauusianBlur;
import Artifact.Effect.LiftGammaGain;
import Artifact.Effect.LensDistortion;
import Artifact.Effect.Keying.ChromaKey;
import Artifact.Effect.Wave;
import Artifact.Effect.Spherize;
import Artifact.Widgets.LayerPanelWidget;
import Artifact.Widgets.ArtifactPropertyWidget;
import Artifact.Layers.Selection.Manager;
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
constexpr auto kInspectorContext = "Panel.Inspector";

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

QColor toneColor(LayerPresentationBadgeTone tone, const QColor &base,
                 const QColor &accent) {
  switch (tone) {
  case LayerPresentationBadgeTone::Container:
    return accent;
  case LayerPresentationBadgeTone::Media:
    return blendColor(base, accent, 0.22);
  case LayerPresentationBadgeTone::Motion:
    return accent.lighter(112);
  case LayerPresentationBadgeTone::Special:
    return accent.darker(112);
  case LayerPresentationBadgeTone::Neutral:
  default:
    return base;
  }
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

LayerPresentationBadgeTone toneFromRackIndex(int rackIndex) {
  switch (stageFromRackIndex(rackIndex)) {
  case EffectPipelineStage::Generator:
  case EffectPipelineStage::GeometryTransform:
    return LayerPresentationBadgeTone::Motion;
  case EffectPipelineStage::MaterialRender:
    return LayerPresentationBadgeTone::Media;
  case EffectPipelineStage::Rasterizer:
    return LayerPresentationBadgeTone::Special;
  case EffectPipelineStage::LayerTransform:
    return LayerPresentationBadgeTone::Container;
  default:
    return LayerPresentationBadgeTone::Neutral;
  }
}

QString matteTypeToText(MatteType type) {
  switch (type) {
  case MatteType::Alpha:
    return QStringLiteral("Alpha");
  case MatteType::Luma:
    return QStringLiteral("Luma");
  case MatteType::InverseAlpha:
    return QStringLiteral("Inverted Alpha");
  case MatteType::InverseLuma:
    return QStringLiteral("Inverted Luma");
  }
  return QStringLiteral("Matte");
}

QString matteReferenceSummary(const ArtifactCompositionPtr &comp,
                              const ArtifactAbstractLayerPtr &layer,
                              bool *hasInvalid = nullptr) {
  if (hasInvalid) {
    *hasInvalid = false;
  }
  if (!layer) {
    return QStringLiteral("Matte: none");
  }

  const auto refs = layer->matteReferences();
  if (refs.empty()) {
    return QStringLiteral("Matte: none");
  }

  QStringList parts;
  parts.reserve(static_cast<int>(refs.size()));
  for (const auto &ref : refs) {
    QString sourceName = QStringLiteral("<missing>");
    if (comp && !ref.sourceLayerId.isNil()) {
      if (auto source = comp->layerById(ref.sourceLayerId)) {
        sourceName = source->layerName().trimmed().isEmpty()
                         ? ref.sourceLayerId.toString()
                         : source->layerName();
      } else if (hasInvalid) {
        *hasInvalid = true;
      }
    } else if (hasInvalid) {
      *hasInvalid = true;
    }

    if (ref.sourceLayerId == layer->id() || ref.sourceLayerId.isNil()) {
      if (hasInvalid) {
        *hasInvalid = true;
      }
    }

    QString entry = QStringLiteral("%1 (%2)").arg(sourceName, matteTypeToText(ref.type));
    if (!ref.enabled) {
      entry += QStringLiteral(" off");
    }
    if (ref.invert) {
      entry += QStringLiteral(" inverted");
    }
    parts.push_back(entry);
  }

  return QStringLiteral("Matte: %1").arg(parts.join(QStringLiteral(", ")));
}

QString proxySummary(const ArtifactAbstractLayerPtr &layer,
                     QString *proxyPathOut = nullptr,
                     bool *hasProxyOut = nullptr) {
  if (proxyPathOut) {
    proxyPathOut->clear();
  }
  if (hasProxyOut) {
    *hasProxyOut = false;
  }
  const auto videoLayer = layer ? std::dynamic_pointer_cast<ArtifactVideoLayer>(layer) : nullptr;
  if (!videoLayer) {
    return QStringLiteral("Proxy: not available");
  }

  if (proxyPathOut) {
    *proxyPathOut = videoLayer->proxyPath();
  }
  const bool hasProxy = videoLayer->hasProxy();
  if (hasProxyOut) {
    *hasProxyOut = hasProxy;
  }

  const QString qualityText = [&]() -> QString {
    switch (videoLayer->proxyQuality()) {
    case ProxyQuality::Quarter:
      return QStringLiteral("1/4");
    case ProxyQuality::Half:
      return QStringLiteral("1/2");
    case ProxyQuality::Full:
      return QStringLiteral("Full");
    case ProxyQuality::None:
    default:
      return QStringLiteral("None");
    }
  }();

  if (!hasProxy) {
    return QStringLiteral("Proxy: none");
  }

  const QString path = videoLayer->proxyPath();
  const QString fileName = path.isEmpty() ? QStringLiteral("<unknown>") : QFileInfo(path).fileName();
  return QStringLiteral("Proxy: %1 | %2").arg(qualityText, fileName);
}

ArtifactCompositionPtr resolveCompositionForId(const CompositionID &compositionId) {
  auto *service = ArtifactProjectService::instance();
  if (!service || compositionId.isNil()) {
    return {};
  }
  auto result = service->findComposition(compositionId);
  if (!result.success) {
    return {};
  }
  return result.ptr.lock();
}

bool applyMatteTypeToLayer(const CompositionID &compositionId,
                           const LayerID &layerId,
                           int matteIndex,
                           MatteType matteType) {
  auto comp = resolveCompositionForId(compositionId);
  if (!comp || layerId.isNil() || matteIndex < 0) {
    return false;
  }

  auto layer = comp->layerById(layerId);
  if (!layer) {
    return false;
  }

  auto beforeRefs = layer->matteReferences();
  if (matteIndex >= static_cast<int>(beforeRefs.size())) {
    return false;
  }

  auto afterRefs = beforeRefs;
  auto &ref = afterRefs[matteIndex];
  const MatteType previousType = ref.type;
  const bool previousInvert = ref.invert;
  ref.type = matteType;
  ref.invert = false;
  if (ref.type == previousType && ref.invert == previousInvert) {
    return false;
  }

  auto *cmd = new ChangeLayerMatteReferencesCommand(layer,
                                                    std::move(beforeRefs),
                                                    std::move(afterRefs));
  UndoManager::instance()->push(std::unique_ptr<ChangeLayerMatteReferencesCommand>(cmd));
  return true;
}

class MatteInfoLabel final : public QLabel {
public:
  explicit MatteInfoLabel(QWidget *parent = nullptr)
      : QLabel(parent) {
    setCursor(Qt::PointingHandCursor);
    setToolTip(QStringLiteral("Left click: focus matte source. Right click: change matte type."));
  }

  void setMatteContext(const CompositionID &compositionId,
                       const ArtifactAbstractLayerPtr &layer,
                       const ArtifactCompositionPtr &composition) {
    compositionId_ = compositionId;
    layerId_ = layer ? layer->id() : LayerID();
    composition_ = composition;
    const bool hasMatteRefs = layer && !layer->matteReferences().empty();
    setCursor(hasMatteRefs ? Qt::PointingHandCursor : Qt::ArrowCursor);
  }

protected:
  void mousePressEvent(QMouseEvent *event) override {
    if (!event) {
      QLabel::mousePressEvent(event);
      return;
    }

    auto composition = composition_ ? composition_ : resolveCompositionForId(compositionId_);
    if (!composition || layerId_.isNil()) {
      QLabel::mousePressEvent(event);
      return;
    }

    auto layer = composition->layerById(layerId_);
    if (!layer) {
      QLabel::mousePressEvent(event);
      return;
    }

    const auto refs = layer->matteReferences();
    if (refs.empty()) {
      QLabel::mousePressEvent(event);
      return;
    }

    if (event->button() == Qt::LeftButton) {
      const auto &ref = refs.front();
      if (!ref.sourceLayerId.isNil()) {
        if (auto *service = ArtifactProjectService::instance()) {
          service->selectLayer(ref.sourceLayerId);
        }
        event->accept();
        return;
      }
    }

    if (event->button() == Qt::RightButton) {
      QMenu menu(this);
      const QStringList typeLabels = {
          QStringLiteral("Alpha"),
          QStringLiteral("Luma"),
          QStringLiteral("Inverted Alpha"),
          QStringLiteral("Inverted Luma")};

      for (int i = 0; i < refs.size(); ++i) {
        const auto &ref = refs[i];
        QString sourceName = QStringLiteral("<missing>");
        if (!ref.sourceLayerId.isNil()) {
          if (auto source = composition->layerById(ref.sourceLayerId)) {
            const QString name = source->layerName().trimmed();
            sourceName = name.isEmpty() ? ref.sourceLayerId.toString() : name;
          } else {
            sourceName = ref.sourceLayerId.toString();
          }
        }

        QMenu *refMenu = menu.addMenu(QStringLiteral("Matte %1: %2").arg(i + 1).arg(sourceName));
        if (ref.sourceLayerId.isNil()) {
          QAction *disabled = refMenu->addAction(QStringLiteral("Missing source"));
          disabled->setEnabled(false);
          continue;
        }

        QAction *focusAction = refMenu->addAction(QStringLiteral("Focus source"));
        focusAction->setData(QVariantMap{{QStringLiteral("kind"), QStringLiteral("focus")},
                                         {QStringLiteral("index"), i}});

        QMenu *typeMenu = refMenu->addMenu(QStringLiteral("Set matte type"));
        for (int typeIndex = 0; typeIndex < typeLabels.size(); ++typeIndex) {
          QAction *typeAction = typeMenu->addAction(typeLabels[typeIndex]);
          typeAction->setData(QVariantMap{{QStringLiteral("kind"), QStringLiteral("type")},
                                          {QStringLiteral("index"), i},
                                          {QStringLiteral("type"), typeIndex}});
        }
      }

      if (QAction *chosen = menu.exec(QCursor::pos())) {
        const QVariantMap data = chosen->data().toMap();
        const QString kind = data.value(QStringLiteral("kind")).toString();
        bool indexOk = false;
        const int index = data.value(QStringLiteral("index")).toInt(&indexOk);
        if (!indexOk) {
          return;
        }
        if (kind == QStringLiteral("focus")) {
          if (index >= 0 && index < static_cast<int>(refs.size())) {
            const auto &ref = refs[index];
            if (!ref.sourceLayerId.isNil()) {
              if (auto *service = ArtifactProjectService::instance()) {
                service->selectLayer(ref.sourceLayerId);
              }
            }
          }
        } else if (kind == QStringLiteral("type")) {
          bool typeOk = false;
          const int typeValue = data.value(QStringLiteral("type")).toInt(&typeOk);
          if (!typeOk) {
            return;
          }
          if (index >= 0 && index < static_cast<int>(refs.size()) &&
              typeValue >= 0 && typeValue <= static_cast<int>(MatteType::InverseLuma)) {
            applyMatteTypeToLayer(compositionId_, layerId_, index,
                                  static_cast<MatteType>(typeValue));
          }
        }
      }
      event->accept();
      return;
    }

    QLabel::mousePressEvent(event);
  }

private:
  CompositionID compositionId_;
  LayerID layerId_;
  ArtifactCompositionPtr composition_;
};

class ProxyInfoLabel final : public QLabel {
public:
  explicit ProxyInfoLabel(QWidget *parent = nullptr)
      : QLabel(parent) {
    setCursor(Qt::PointingHandCursor);
    setToolTip(QStringLiteral("Left click: open proxy folder."));
  }

  void setProxyContext(const ArtifactAbstractLayerPtr &layer) {
    layer_ = layer;
    const bool hasProxy = layer && std::dynamic_pointer_cast<ArtifactVideoLayer>(layer) &&
                          std::dynamic_pointer_cast<ArtifactVideoLayer>(layer)->hasProxy();
    setCursor(hasProxy ? Qt::PointingHandCursor : Qt::ArrowCursor);
  }

protected:
  void mousePressEvent(QMouseEvent *event) override {
    if (!event) {
      QLabel::mousePressEvent(event);
      return;
    }
    if (event->button() == Qt::LeftButton) {
      const auto videoLayer = layer_ ? std::dynamic_pointer_cast<ArtifactVideoLayer>(layer_) : nullptr;
      if (videoLayer && videoLayer->hasProxy()) {
        const QString proxyPath = videoLayer->proxyPath();
        if (!proxyPath.isEmpty() && QFileInfo::exists(proxyPath)) {
          QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(proxyPath).absolutePath()));
          event->accept();
          return;
        }
      }
    }
    QLabel::mousePressEvent(event);
  }

private:
  ArtifactAbstractLayerPtr layer_;
};
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
  MatteInfoLabel *matteInfoLabel = nullptr;
  ProxyInfoLabel *proxyInfoLabel = nullptr;
  QGroupBox *componentsGroup = nullptr;
  QLabel *componentsSummaryLabel = nullptr;
  QPushButton *physicsComponentButton = nullptr;
  QPushButton *scriptComponentButton = nullptr;
  QPushButton *layoutComponentButton = nullptr;
  QPushButton *cloneComponentButton = nullptr;
  QPushButton *openScriptButton = nullptr;
  ArtifactPropertyWidget *componentPropertyWidget = nullptr;
  QLabel *statusLabel = nullptr;

  // Effects Pipeline Tab
  QScrollArea *effectsScrollArea = nullptr;
  QWidget *effectsTabWidget = nullptr;
  QLabel *effectsStateLabel = nullptr;
  QLabel *effectParametersHintLabel = nullptr;
  ArtifactPropertyWidget *effectPropertyWidget = nullptr;
  QString focusedEffectId_;
  ArtifactAbstractLayerPtr lastSyncedLayer_;
  QString lastSyncedFocusedEffectId_;
  QString lastEffectPropertyStateSignature_;

  struct EffectRack {
    QGroupBox *groupBox = nullptr;
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
  ArtifactCore::EventBus::Subscription compositionNoteSubscription_;
  ArtifactCore::EventBus eventBus_ = ArtifactCore::globalEventBus();
  std::vector<ArtifactCore::EventBus::Subscription> eventBusSubscriptions_;
  QString lastLayerInfoSignature_;
  QString lastMatteInfoSignature_;
  std::array<QString, kEffectRackCount> lastRackSignatures_{};
  QString lastCompositionNoteText_;
  QString lastLayerNoteText_;
  int refreshMask_ = 0;
  bool refreshQueued_ = false;
  bool suppressRackSelectionSync_ = false;
  bool syncingEffectPropertyWidget_ = false;

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
  void updateComponentControls(const ArtifactAbstractLayerPtr &layer);
  void focusComponentProperties(const ArtifactAbstractLayerPtr &layer,
                                const QString &filterText);
  void updateEffectsList();
  void updateEffectRackItemEnabled(const QString &effectId, bool enabled);
  void updatePropertiesForEffect(const QString &effectId);
  QString currentSelectedEffectIdFromRacks() const;
  void syncFocusedEffectFromRackSelection();
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
  const QString normalized = effectId.trimmed();
  if (focusedEffectId_ == normalized) {
    return;
  }
  focusedEffectId_ = normalized;
  syncEffectPropertyWidget();
}

QString ArtifactInspectorWidget::Impl::currentSelectedEffectIdFromRacks() const {
  for (int rackIndex = 0; rackIndex < kEffectRackCount; ++rackIndex) {
    auto *list = racks[rackIndex].listWidget;
    if (!list) {
      continue;
    }
    auto *item = list->currentItem();
    if (!item) {
      continue;
    }
    const QString id = item->data(Qt::UserRole).toString().trimmed();
    if (!id.isEmpty()) {
      return id;
    }
  }
  return {};
}

void ArtifactInspectorWidget::Impl::syncFocusedEffectFromRackSelection() {
  if (suppressRackSelectionSync_) {
    return;
  }
  updatePropertiesForEffect(currentSelectedEffectIdFromRacks());
  refreshRackButtons();
}

void ArtifactInspectorWidget::Impl::syncEffectPropertyWidget() {
  if (!effectPropertyWidget) {
    return;
  }
  if (syncingEffectPropertyWidget_) {
    return;
  }
  syncingEffectPropertyWidget_ = true;
  const auto clearSyncing = qScopeGuard([this]() {
    syncingEffectPropertyWidget_ = false;
  });

  const auto showEffectGuidance = [this](const QString &text,
                                         const bool showPropertyWidget) {
    effectPropertyWidget->setVisible(showPropertyWidget);
    if (effectParametersHintLabel) {
      effectParametersHintLabel->setText(text);
      effectParametersHintLabel->setVisible(true);
    }
  };

  auto projectService = ArtifactProjectService::instance();
  if (!projectService || currentCompositionId_.isNil() ||
      currentLayerId_.isNil()) {
    effectPropertyWidget->clear();
    showEffectGuidance(
        QStringLiteral("Open a composition, then select a layer and effect. The selected effect's parameters appear below."),
        false);
    lastSyncedLayer_.reset();
    lastSyncedFocusedEffectId_.clear();
    lastEffectPropertyStateSignature_.clear();
    return;
  }

  auto findResult = projectService->findComposition(currentCompositionId_);
  if (!findResult.success) {
    effectPropertyWidget->clear();
    showEffectGuidance(
        QStringLiteral("Open a composition, then select a layer and effect. The selected effect's parameters appear below."),
        false);
    lastSyncedLayer_.reset();
    lastSyncedFocusedEffectId_.clear();
    lastEffectPropertyStateSignature_.clear();
    return;
  }

  auto comp = findResult.ptr.lock();
  if (!comp) {
    effectPropertyWidget->clear();
    showEffectGuidance(
        QStringLiteral("Open a composition, then select a layer and effect. The selected effect's parameters appear below."),
        false);
    lastSyncedLayer_.reset();
    lastSyncedFocusedEffectId_.clear();
    lastEffectPropertyStateSignature_.clear();
    return;
  }

  auto layer = comp->layerById(currentLayerId_);
  if (!layer) {
    effectPropertyWidget->clear();
    showEffectGuidance(
        QStringLiteral("Open a composition, then select a layer and effect. The selected effect's parameters appear below."),
        false);
    lastSyncedLayer_.reset();
    lastSyncedFocusedEffectId_.clear();
    lastEffectPropertyStateSignature_.clear();
    return;
  }

  bool effectExists = false;
  QString focusedEffectName;
  QString resolvedFocusedEffectId = focusedEffectId_.trimmed();
  if (!focusedEffectId_.trimmed().isEmpty()) {
    for (const auto &effect : layer->getEffects()) {
      if (effect && effect->effectID().toQString() == focusedEffectId_) {
        effectExists = true;
        focusedEffectName = effect->displayName().toQString();
        break;
      }
    }
  }

  if (!effectExists) {
    focusedEffectId_.clear();
    resolvedFocusedEffectId.clear();
  }

  const QString stateSignature =
      QStringLiteral("%1|%2").arg(layer->id().toString(), resolvedFocusedEffectId);
  if (layer == lastSyncedLayer_ &&
      resolvedFocusedEffectId == lastSyncedFocusedEffectId_ &&
      stateSignature == lastEffectPropertyStateSignature_) {
    return;
  }

  const bool layerChanged = layer != lastSyncedLayer_;
  lastSyncedLayer_ = layer;
  lastSyncedFocusedEffectId_ = resolvedFocusedEffectId;
  lastEffectPropertyStateSignature_ = stateSignature;

  if (layerChanged) {
    effectPropertyWidget->setLayer(layer);
  }
  effectPropertyWidget->setFocusedEffectId(resolvedFocusedEffectId);

  const bool hasFocus = !resolvedFocusedEffectId.trimmed().isEmpty();
  showEffectGuidance(
      hasFocus
          ? QStringLiteral("Editing \"%1\" below. The same parameters are also mirrored in the Properties dock.")
                .arg(focusedEffectName.isEmpty() ? resolvedFocusedEffectId
                                                 : focusedEffectName)
          : QStringLiteral("Select an effect in any rack. Its parameters will appear below and in the Properties dock."),
      hasFocus);
}

QString defaultComponentInspectorFilter(const ArtifactAbstractLayerPtr &layer) {
  if (!layer) {
    return QStringLiteral("physics|script|layout|cloner");
  }
  return QStringLiteral("physics|script|layout|cloner");
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

namespace {
bool layerBooleanProperty(const ArtifactAbstractLayerPtr &layer,
                          const QString &propertyPath) {
  if (!layer) {
    return false;
  }
  const auto groups = layer->getLayerPropertyGroups();
  Q_UNUSED(groups);
  const auto property = layer->getProperty(propertyPath);
  return property ? property->getValue().toBool() : false;
}

QString resolveScriptBindingPath(const ArtifactAbstractLayerPtr &layer) {
  if (!layer) {
    return {};
  }
  const QJsonObject binding = layer->scriptBinding();
  const QStringList keys = {
      QStringLiteral("path"),
      QStringLiteral("file"),
      QStringLiteral("scriptPath"),
      QStringLiteral("scriptFile"),
  };
  for (const auto &key : keys) {
    const QString value = binding.value(key).toString().trimmed();
    if (!value.isEmpty()) {
      return value;
    }
  }
  return {};
}
} // namespace

void ArtifactInspectorWidget::Impl::updateComponentControls(
    const ArtifactAbstractLayerPtr &layer) {
  const bool hasLayer = static_cast<bool>(layer);
  const bool canEditComponents = hasLayer;
  const bool physicsEnabled =
      hasLayer && layerBooleanProperty(layer, QStringLiteral("physics.enabled"));
  const bool scriptEnabled =
      hasLayer && layerBooleanProperty(
                      layer, QStringLiteral("component.script.enabled"));
  const bool layoutEnabled =
      hasLayer && layerBooleanProperty(
                      layer, QStringLiteral("component.layout.enabled"));
  const bool cloneEnabled =
      hasLayer && layerBooleanProperty(
                      layer, QStringLiteral("component.cloner.enabled"));

  if (componentsGroup) {
    componentsGroup->setEnabled(canEditComponents);
  }
  if (physicsComponentButton) {
    physicsComponentButton->setEnabled(canEditComponents);
    physicsComponentButton->setText(physicsEnabled ? QStringLiteral("Physics On")
                                                   : QStringLiteral("+ Physics"));
    physicsComponentButton->setToolTip(
        canEditComponents ? QStringLiteral("Toggle the layer physics component.")
                          : QStringLiteral("Select a layer inside a composition to add Physics."));
  }
  if (scriptComponentButton) {
    scriptComponentButton->setEnabled(canEditComponents);
    scriptComponentButton->setText(scriptEnabled ? QStringLiteral("Script On")
                                                 : QStringLiteral("+ Script"));
    scriptComponentButton->setToolTip(
        canEditComponents ? QStringLiteral("Toggle the layer script component.")
                          : QStringLiteral("Select a layer inside a composition to add Script."));
  }
  if (layoutComponentButton) {
    layoutComponentButton->setEnabled(canEditComponents);
    layoutComponentButton->setText(layoutEnabled ? QStringLiteral("Layout On")
                                                 : QStringLiteral("+ Layout"));
    layoutComponentButton->setToolTip(
        canEditComponents ? QStringLiteral("Toggle the layer Layout component.")
                          : QStringLiteral("Select a layer inside a composition to add Layout."));
  }
  if (cloneComponentButton) {
    cloneComponentButton->setEnabled(canEditComponents);
    cloneComponentButton->setText(cloneEnabled ? QStringLiteral("Cloner On")
                                               : QStringLiteral("+ Cloner"));
    cloneComponentButton->setToolTip(
        canEditComponents ? QStringLiteral("Toggle the layer Cloner component.")
                          : QStringLiteral("Select a layer inside a composition to add Cloner."));
  }
  if (componentsSummaryLabel) {
    QStringList active;
    if (physicsEnabled) {
      active.push_back(QStringLiteral("Physics"));
    }
    if (scriptEnabled) {
      active.push_back(QStringLiteral("Script"));
    }
    if (layoutEnabled) {
      active.push_back(QStringLiteral("Layout"));
    }
    if (cloneEnabled) {
      active.push_back(QStringLiteral("Cloner"));
    }
    componentsSummaryLabel->setText(
        hasLayer ? (active.isEmpty()
                        ? QStringLiteral("Components: none")
                        : QStringLiteral("Components: %1")
                              .arg(active.join(QStringLiteral(", "))))
                 : QStringLiteral("Components: select a layer in a composition to add components"));
    applyInspectorLabelPalette(componentsSummaryLabel, active.isEmpty());
  }

  if (componentPropertyWidget) {
    componentPropertyWidget->setVisible(hasLayer);
    if (!hasLayer) {
      componentPropertyWidget->clear();
    } else {
      // Check for multi-selection
      auto *selMgr = ArtifactLayerSelectionManager::instance();
      const auto selected = selMgr ? selMgr->selectedLayers() : QSet<ArtifactAbstractLayerPtr>{};
      if (selected.size() > 1) {
        componentPropertyWidget->setLayers(selected);
      } else {
        componentPropertyWidget->setLayer(layer);
      }
      if (componentPropertyWidget->filterText().trimmed().isEmpty()) {
        componentPropertyWidget->setFilterText(
            defaultComponentInspectorFilter(layer));
      } else {
        componentPropertyWidget->updateProperties();
      }
    }
  }

  if (openScriptButton) {
    const QString scriptPath = resolveScriptBindingPath(layer);
    const bool canOpen = hasLayer && !scriptPath.trimmed().isEmpty();
    openScriptButton->setEnabled(canOpen);
    openScriptButton->setVisible(hasLayer);
    openScriptButton->setText(canOpen ? QStringLiteral("Open Script")
                                      : QStringLiteral("Open Script"));
    openScriptButton->setToolTip(
        canOpen ? QStringLiteral("Open the script file linked to this layer.")
                : (hasLayer ? QStringLiteral("No script file is linked to this layer yet.")
                            : QStringLiteral("Select a layer inside a composition to open its script.")));
  }
}

void ArtifactInspectorWidget::Impl::focusComponentProperties(
    const ArtifactAbstractLayerPtr &layer, const QString &filterText) {
  if (!componentPropertyWidget) {
    return;
  }
  if (!layer) {
    componentPropertyWidget->clear();
    componentPropertyWidget->setVisible(false);
    return;
  }
  componentPropertyWidget->setVisible(true);
  // Check for multi-selection
  auto *selMgr = ArtifactLayerSelectionManager::instance();
  const auto selected = selMgr ? selMgr->selectedLayers() : QSet<ArtifactAbstractLayerPtr>{};
  if (selected.size() > 1) {
    componentPropertyWidget->setLayers(selected);
  } else {
    componentPropertyWidget->setLayer(layer);
  }
  componentPropertyWidget->setFilterText(filterText.trimmed().isEmpty()
                                             ? defaultComponentInspectorFilter(layer)
                                             : filterText);
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
  signature += describeLayerPresentation(layer).typeText;
  signature += QLatin1Char('|');
  signature += QString::number(layer->maskCount());
  signature += QLatin1Char('|');
  signature += layerBooleanProperty(layer, QStringLiteral("physics.enabled"))
                   ? QLatin1Char('1')
                   : QLatin1Char('0');
  signature +=
      layerBooleanProperty(layer, QStringLiteral("component.script.enabled"))
          ? QLatin1Char('1')
          : QLatin1Char('0');
  signature +=
      layerBooleanProperty(layer, QStringLiteral("component.cloner.enabled"))
          ? QLatin1Char('1')
          : QLatin1Char('0');
  signature += QLatin1Char('|');
  signature += layer->layerNote();
  signature += QLatin1Char('|');
  const auto mattes = layer->matteReferences();
  for (const auto &ref : mattes) {
    signature += ref.id.toString();
    signature += QLatin1Char(':');
    signature += ref.sourceLayerId.toString();
    signature += QLatin1Char(':');
    signature += QString::number(static_cast<int>(ref.type));
    signature += QLatin1Char(':');
    signature += QString::number(static_cast<int>(ref.blendMode));
    signature += QLatin1Char(':');
    signature += QString::number(static_cast<int>(ref.fitMode));
    signature += QLatin1Char(':');
    signature += ref.enabled ? QStringLiteral("1") : QStringLiteral("0");
    signature += QLatin1Char(':');
    signature += ref.invert ? QStringLiteral("1") : QStringLiteral("0");
    signature += QLatin1Char('|');
  }
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
                         updateEffectRackItemEnabled(effectId, !isEnabled);
                         if (statusLabel) {
                           statusLabel->setText(
                               QStringLiteral("Status: Effect %1")
                                   .arg(!isEnabled ? "enabled" : "disabled"));
                         }
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
             << layerSelectionChangeReasonToString(event.reason)
             << "composition=" << currentCompositionId_.toString()
             << "layer=" << currentLayerId_.toString()
             << "projectService=" << static_cast<bool>(projectService);
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
    compositionNoteSubscription_.disconnect();
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
      compositionNoteGroup->hide();
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
      compositionNoteGroup->hide();
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
      compositionNoteGroup->hide();
    }
    return;
  }

  disconnectNoteConnection();
  compositionNoteSubscription_ =
      eventBus_.subscribe<CompositionNoteChangedEvent>([this](const CompositionNoteChangedEvent &event) {
        if (!compositionNoteEdit || event.compositionId != currentCompositionId_.toString()) {
          return;
        }
        QSignalBlocker blocker(compositionNoteEdit);
        compositionNoteEdit->setPlainText(event.note);
        compositionNoteEdit->setEnabled(true);
        if (compositionNoteGroup) {
          compositionNoteGroup->setEnabled(true);
          compositionNoteGroup->hide();
        }
      });

  const QString note = comp->compositionNote();
  if (note == lastCompositionNoteText_) {
    compositionNoteEdit->setEnabled(true);
    if (compositionNoteGroup) {
      compositionNoteGroup->setEnabled(true);
      compositionNoteGroup->hide();
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
    compositionNoteGroup->hide();
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
      layerNoteGroup->hide();
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
      layerNoteGroup->hide();
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
      layerNoteGroup->hide();
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
      layerNoteGroup->hide();
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
                           layerNoteGroup->hide();
                         }
                       });

  const QString note = layer->layerNote();
  if (note == lastLayerNoteText_) {
    layerNoteEdit->setEnabled(true);
    if (layerNoteGroup) {
      layerNoteGroup->setEnabled(true);
      layerNoteGroup->hide();
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
    layerNoteGroup->hide();
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
    // matte 表示も同じ更新経路で同期するため、ここでは止めない
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
    nameFont.setPointSize(13);
    layerNameLabel->setFont(nameFont);
    applyInspectorLabelPalette(layerNameLabel, true);

    QFont typeFont = layerTypeLabel->font();
    typeFont.setBold(true);
    layerTypeLabel->setFont(typeFont);
    applyInspectorLabelPalette(layerTypeLabel, false);
  }

  // レイヤータイプを実態に寄せて表示する
  const auto presentation = describeLayerPresentation(layer);
  layerTypeLabel->setText(presentation.inspectorTypeLabel);

  const int maskCount = layer->maskCount();
  const QString maskText = maskCount > 0
                               ? QStringLiteral("Masks: %1").arg(maskCount)
                               : QStringLiteral("Masks: none");
  bool matteHasInvalid = false;
  const QString matteText = matteReferenceSummary(comp, layer, &matteHasInvalid);
  if (matteInfoLabel) {
    matteInfoLabel->setMatteContext(currentCompositionId_, layer, comp);
    if (matteText != lastMatteInfoSignature_) {
      lastMatteInfoSignature_ = matteText;
      matteInfoLabel->setText(matteText);
    }
    matteInfoLabel->setEnabled(true);
    QFont matteFont = matteInfoLabel->font();
    matteFont.setBold(matteHasInvalid);
    matteInfoLabel->setFont(matteFont);
    applyInspectorLabelPalette(matteInfoLabel, matteHasInvalid);
  }
  const QString proxyText = proxySummary(layer);
  if (proxyInfoLabel) {
    proxyInfoLabel->setProxyContext(layer);
    proxyInfoLabel->setText(proxyText);
    proxyInfoLabel->setEnabled(true);
    applyInspectorLabelPalette(proxyInfoLabel, proxyText.contains(QStringLiteral("none"), Qt::CaseInsensitive));
  }
  updateComponentControls(layer);
  const QString capabilityText = presentation.capabilitySummaryText.isEmpty()
                                     ? QString()
                                     : QStringLiteral(" | %1").arg(presentation.capabilitySummaryText);
  statusLabel->setText(QString("Status: Layer selected - ID: %1 | %2%3")
                           .arg(currentLayerId_.toString(), maskText, capabilityText));
  {
    const auto theme = ArtifactCore::currentDCCTheme();
    applyInspectorLabelPalette(statusLabel, true);
  }
      setEffectsStateText(
      maskCount > 0
          ? QStringLiteral("Mask editing is available for this layer. Roto fields are also available where supported.")
          : QStringLiteral(
                "No masks on this layer. Use the Mask tool to create one."),
      true);

  qDebug() << "[Inspector] Updated layer info:" << layerName
           << "Type:" << presentation.typeText;
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
    compositionNoteGroup->hide();
  }
  if (layerNoteEdit) {
    layerNoteEdit->blockSignals(true);
    layerNoteEdit->clear();
    layerNoteEdit->setEnabled(false);
    layerNoteEdit->blockSignals(false);
  }
  if (layerNoteGroup) {
    layerNoteGroup->setEnabled(false);
    layerNoteGroup->hide();
  }
  layerNameLabel->setText("Layer: Open a project to inspect layers");
  layerTypeLabel->setText("Type: N/A");
  if (matteInfoLabel) {
    matteInfoLabel->setText("Matte: none");
    matteInfoLabel->setEnabled(false);
    matteInfoLabel->setMatteContext(CompositionID(), ArtifactAbstractLayerPtr{}, ArtifactCompositionPtr{});
  }
  if (proxyInfoLabel) {
    proxyInfoLabel->setText("Proxy: not available");
    proxyInfoLabel->setEnabled(false);
    proxyInfoLabel->setProxyContext(ArtifactAbstractLayerPtr{});
  }
  updateComponentControls(ArtifactAbstractLayerPtr{});
  statusLabel->setText("Status: Open a project to inspect layers");
  currentCompositionId_ = CompositionID();
  currentLayerId_ = LayerID();
  lastLayerInfoSignature_.clear();
  lastMatteInfoSignature_.clear();
  lastCompositionNoteText_.clear();
  lastLayerNoteText_.clear();
  lastRackSignatures_.fill(QString());
  lastSyncedLayer_.reset();
  lastSyncedFocusedEffectId_.clear();
  lastEffectPropertyStateSignature_.clear();
  refreshMask_ = 0;
  refreshQueued_ = false;
  focusedEffectId_.clear();
  if (effectPropertyWidget) {
    effectPropertyWidget->clear();
    effectPropertyWidget->setVisible(false);
  }
  if (componentPropertyWidget) {
    componentPropertyWidget->clear();
    componentPropertyWidget->setVisible(false);
  }
  if (effectParametersHintLabel) {
    effectParametersHintLabel->setText(
        QStringLiteral("Open a project, then select a composition, layer, and effect to edit color controls."));
    effectParametersHintLabel->setVisible(true);
  }
  setEffectRackEnabled(false);
  setEffectsStateText("Open a project to manage color controls.", true);
}

void ArtifactInspectorWidget::Impl::setNoLayerState() {
  layerNameLabel->setText("Layer: Select a layer to continue");
  layerTypeLabel->setText("Type: N/A");
  if (matteInfoLabel) {
    matteInfoLabel->setText("Matte: none");
    matteInfoLabel->setEnabled(false);
    matteInfoLabel->setMatteContext(CompositionID(), ArtifactAbstractLayerPtr{}, ArtifactCompositionPtr{});
  }
  if (proxyInfoLabel) {
    proxyInfoLabel->setText("Proxy: not available");
    proxyInfoLabel->setEnabled(false);
    proxyInfoLabel->setProxyContext(ArtifactAbstractLayerPtr{});
  }
  updateComponentControls(ArtifactAbstractLayerPtr{});
  statusLabel->setText("Status: Select a layer to inspect details");
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
    layerNoteGroup->hide();
  }

  // エフェクトリストもクリア
  for (auto &rack : racks) {
    if (rack.listWidget) {
      rack.listWidget->clear();
    }
  }
  lastLayerInfoSignature_.clear();
  lastMatteInfoSignature_.clear();
  lastLayerNoteText_.clear();
  lastRackSignatures_.fill(QString());
  lastSyncedLayer_.reset();
  lastSyncedFocusedEffectId_.clear();
  lastEffectPropertyStateSignature_.clear();
  refreshMask_ = 0;
  refreshQueued_ = false;
  focusedEffectId_.clear();
  if (effectPropertyWidget) {
    effectPropertyWidget->clear();
    effectPropertyWidget->setVisible(false);
  }
  if (componentPropertyWidget) {
    componentPropertyWidget->clear();
    componentPropertyWidget->setVisible(false);
  }
  if (effectParametersHintLabel) {
    effectParametersHintLabel->setText(
        QStringLiteral("Select an effect row above to edit color controls."));
    effectParametersHintLabel->setVisible(true);
  }
  setEffectRackEnabled(false);
  setEffectsStateText("Select a layer to manage color controls.", true);
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
    setEffectsStateText("Open a project to manage effects.", true);
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
    const QSignalBlocker blocker(racks[i].listWidget);
    racks[i].listWidget->clear();
    if (rackEffects[i].empty()) {
      auto item = new QListWidgetItem("(No effects)");
      item->setFlags(item->flags() & ~Qt::ItemIsSelectable);
      racks[i].listWidget->addItem(item);
      continue;
    }
    const auto &theme = ArtifactCore::currentDCCTheme();
    const QColor textColor = QColor(theme.textColor.isEmpty()
                                        ? QStringLiteral("#E3E7EC")
                                        : theme.textColor);
    const QColor accentColor = QColor(theme.accentColor.isEmpty()
                                          ? QStringLiteral("#5E94C7")
                                          : theme.accentColor);
    const auto rackTone = toneFromRackIndex(i);
    const QColor rackColor = toneColor(rackTone, textColor, accentColor);
    for (const auto &effect : rackEffects[i]) {
      if (!effect) {
        continue;
      }
      QString effectName = effect->displayName().toQString();
      QString effectStatus = effect->isEnabled() ? QStringLiteral("Enabled")
                                                 : QStringLiteral("Disabled");
      QString itemText = QStringLiteral("%1 %2").arg(effectStatus, effectName);
      auto *item = new QListWidgetItem(itemText);
      item->setData(Qt::UserRole, effect->effectID().toQString());
      item->setData(Qt::UserRole + 1, effect->isEnabled());
      item->setToolTip(QStringLiteral("%1 on this layer. Single click to edit parameters below. Double click toggles enable/disable. Right click for effect actions.")
                           .arg(effectName));
      item->setForeground(effect->isEnabled() ? rackColor : rackColor.darker(140));
      racks[i].listWidget->addItem(item);
    }
  }

  if (effectCount == 0) {
    setEffectsStateText("No effects yet. Use + Add to create an effect.", true);
  } else if (focusedEffectId_.trimmed().isEmpty()) {
    setEffectsStateText("Select an effect to edit its parameters below.", true);
  } else {
    setEffectsStateText(QString(), false);
  }
  refreshRackButtons();

  if (!focusedEffectId_.trimmed().isEmpty()) {
    suppressRackSelectionSync_ = true;
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
    suppressRackSelectionSync_ = false;
  }

  syncEffectPropertyWidget();
}

void ArtifactInspectorWidget::Impl::updateEffectRackItemEnabled(
    const QString &effectId, const bool enabled) {
  const QString trimmedId = effectId.trimmed();
  if (trimmedId.isEmpty()) {
    return;
  }

  const auto &theme = ArtifactCore::currentDCCTheme();
  const QColor textColor = QColor(theme.textColor.isEmpty()
                                      ? QStringLiteral("#E3E7EC")
                                      : theme.textColor);
  const QColor accentColor = QColor(theme.accentColor.isEmpty()
                                        ? QStringLiteral("#5E94C7")
                                        : theme.accentColor);
  const QString enabledPrefix = QStringLiteral("Enabled ");
  const QString disabledPrefix = QStringLiteral("Disabled ");

  for (int rackIndex = 0; rackIndex < kEffectRackCount; ++rackIndex) {
    auto *list = racks[rackIndex].listWidget;
    if (!list) {
      continue;
    }
    for (int row = 0; row < list->count(); ++row) {
      auto *item = list->item(row);
      if (!item || item->data(Qt::UserRole).toString().trimmed() != trimmedId) {
        continue;
      }

      QString effectName = item->text().trimmed();
      if (effectName.startsWith(enabledPrefix)) {
        effectName.remove(0, enabledPrefix.size());
      } else if (effectName.startsWith(disabledPrefix)) {
        effectName.remove(0, disabledPrefix.size());
      }
      effectName = effectName.trimmed();

      const QColor rackColor =
          toneColor(toneFromRackIndex(rackIndex), textColor, accentColor);
      item->setText(QStringLiteral("%1 %2")
                        .arg(enabled ? QStringLiteral("Enabled")
                                     : QStringLiteral("Disabled"),
                             effectName));
      item->setData(Qt::UserRole + 1, enabled);
      item->setForeground(enabled ? rackColor : rackColor.darker(140));
      item->setToolTip(
          QStringLiteral("%1 on this layer. Single click to edit parameters below. Double click toggles enable/disable. Right click for effect actions.")
              .arg(effectName));
      refreshRackButtons();
      return;
    }
  }
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
      if (projectService->addEffectToLayerInCurrentComposition(currentLayerId_,
                                                               newEffect)) {
        focusedEffectId_ = newEffect->effectID().toQString();
        updateEffectsList();
        if (statusLabel) {
          statusLabel->setText(QStringLiteral("Status: Effect added - %1. Parameters are shown below in the Effects tab.")
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
    effectMenu.addAction("Procedural Texture", [addAndRefresh]() {
      addAndRefresh(std::make_shared<ProceduralTextureGeneratorEffect>());
    });
    break;
  case EffectPipelineStage::GeometryTransform:
    effectMenu.addAction("Twist", [addAndRefresh]() {
      auto effect = std::make_shared<TwistTransform>();
      effect->setEffectID(ArtifactCore::UniString("twist"));
      addAndRefresh(effect);
    });
    effectMenu.addAction("Bend", [addAndRefresh]() {
      auto effect = std::make_shared<BendTransform>();
      effect->setEffectID(ArtifactCore::UniString("bend"));
      addAndRefresh(effect);
    });
    break;
  case EffectPipelineStage::MaterialRender:
    effectMenu.addAction("PBR Material", [addAndRefresh]() {
      auto effect = std::make_shared<PBRMaterialEffect>();
      effect->setEffectID(ArtifactCore::UniString("pbr_material"));
      addAndRefresh(effect);
    });
    break;
  case EffectPipelineStage::Rasterizer:
    effectMenu.addAction("Blur", [addAndRefresh]() {
      auto effect = std::make_shared<BlurEffect>();
      effect->setEffectID(ArtifactCore::UniString("blur"));
      effect->setDisplayName(ArtifactCore::UniString("Blur"));
      addAndRefresh(effect);
    });
    effectMenu.addAction("Gaussian Blur", [addAndRefresh]() {
      auto effect = std::make_shared<GaussianBlur>();
      effect->setEffectID(ArtifactCore::UniString("effect.blur.gaussian"));
      effect->setDisplayName(ArtifactCore::UniString("Gaussian Blur"));
      addAndRefresh(effect);
    });
    effectMenu.addAction("Glow", [addAndRefresh]() {
      auto effect = std::make_shared<GlowEffect>();
      effect->setEffectID(ArtifactCore::UniString("glow"));
      effect->setDisplayName(ArtifactCore::UniString("Glow"));
      addAndRefresh(effect);
    });
    effectMenu.addAction("Drop Shadow", [addAndRefresh]() {
      auto effect = std::make_shared<DropShadowEffect>();
      effect->setEffectID(ArtifactCore::UniString("drop_shadow"));
      effect->setDisplayName(ArtifactCore::UniString("Drop Shadow"));
      addAndRefresh(effect);
    });
    effectMenu.addAction("Directional Glow / Streaks", [addAndRefresh]() {
      auto effect = std::make_shared<DirectionalGlowEffect>();
      effect->setEffectID(ArtifactCore::UniString("directional_glow"));
      effect->setDisplayName(ArtifactCore::UniString("Directional Glow / Streaks"));
      addAndRefresh(effect);
    });
    effectMenu.addAction("Edge Bloom", [addAndRefresh]() {
      auto effect = std::make_shared<EdgeBloomEffect>();
      effect->setEffectID(ArtifactCore::UniString("edge_bloom"));
      effect->setDisplayName(ArtifactCore::UniString("Edge Bloom"));
      addAndRefresh(effect);
    });
    effectMenu.addAction("Chromatic Glow", [addAndRefresh]() {
      auto effect = std::make_shared<ChromaticGlowEffect>();
      effect->setEffectID(ArtifactCore::UniString("chromatic_glow"));
      effect->setDisplayName(ArtifactCore::UniString("Chromatic Glow"));
      addAndRefresh(effect);
    });
    effectMenu.addAction("Reactive Glow", [addAndRefresh]() {
      auto effect = std::make_shared<ReactiveGlowEffect>();
      effect->setEffectID(ArtifactCore::UniString("reactive_glow"));
      effect->setDisplayName(ArtifactCore::UniString("Reactive Glow"));
      addAndRefresh(effect);
    });
    effectMenu.addSeparator();
    effectMenu.addAction("Brightness / Contrast", [addAndRefresh]() {
      auto effect = std::make_shared<BrightnessEffect>();
      effect->setEffectID(ArtifactCore::UniString("effect.colorcorrection.brightness"));
      effect->setDisplayName(ArtifactCore::UniString("Brightness / Contrast"));
      addAndRefresh(effect);
    });
    effectMenu.addAction("Exposure", [addAndRefresh]() {
      auto effect = std::make_shared<ExposureEffect>();
      effect->setEffectID(ArtifactCore::UniString("effect.colorcorrection.exposure"));
      effect->setDisplayName(ArtifactCore::UniString("Exposure"));
      addAndRefresh(effect);
    });
    effectMenu.addAction("Tint", [addAndRefresh]() {
      auto effect = std::make_shared<WhiteBalanceEffect>();
      effect->setEffectID(ArtifactCore::UniString("effect.colorcorrection.tint"));
      effect->setDisplayName(ArtifactCore::UniString("Tint"));
      addAndRefresh(effect);
    });
    effectMenu.addAction("Photo Filter", [addAndRefresh]() {
      auto effect = std::make_shared<PhotoFilterEffect>();
      effect->setEffectID(ArtifactCore::UniString("effect.colorcorrection.photofilter"));
      effect->setDisplayName(ArtifactCore::UniString("Photo Filter"));
      addAndRefresh(effect);
    });
    effectMenu.addAction("Gradient Ramp", [addAndRefresh]() {
      auto effect = std::make_shared<GradientRampEffect>();
      effect->setEffectID(ArtifactCore::UniString("effect.colorcorrection.gradientramp"));
      effect->setDisplayName(ArtifactCore::UniString("Gradient Ramp"));
      addAndRefresh(effect);
    });
    effectMenu.addAction("Fill", [addAndRefresh]() {
      auto effect = std::make_shared<FillEffect>();
      effect->setEffectID(ArtifactCore::UniString("effect.colorcorrection.fill"));
      effect->setDisplayName(ArtifactCore::UniString("Fill"));
      addAndRefresh(effect);
    });
    effectMenu.addAction("Hue / Saturation", [addAndRefresh]() {
      auto effect = std::make_shared<HueAndSaturation>();
      effect->setEffectID(ArtifactCore::UniString("effect.colorcorrection.hsl"));
      effect->setDisplayName(ArtifactCore::UniString("Hue / Saturation"));
      addAndRefresh(effect);
    });
    effectMenu.addAction("Color Wheels", [addAndRefresh]() {
      auto effect = std::make_shared<ColorWheelsEffect>();
      effect->setEffectID(ArtifactCore::UniString("effect.colorcorrection.colorwheels"));
      effect->setDisplayName(ArtifactCore::UniString("Color Wheels"));
      addAndRefresh(effect);
    });
    effectMenu.addAction("Curves", [addAndRefresh]() {
      auto effect = std::make_shared<CurvesEffect>();
      effect->setEffectID(ArtifactCore::UniString("effect.colorcorrection.curves"));
      effect->setDisplayName(ArtifactCore::UniString("Curves"));
      addAndRefresh(effect);
    });
    effectMenu.addAction("Tritone", [addAndRefresh]() {
      auto effect = std::make_shared<TritoneEffect>();
      effect->setEffectID(ArtifactCore::UniString("effect.colorcorrection.tritone"));
      effect->setDisplayName(ArtifactCore::UniString("Tritone"));
      addAndRefresh(effect);
    });
    effectMenu.addAction("Colorama", [addAndRefresh]() {
      auto effect = std::make_shared<ColoramaEffect>();
      effect->setEffectID(ArtifactCore::UniString("effect.colorcorrection.colorama"));
      effect->setDisplayName(ArtifactCore::UniString("Colorama"));
      addAndRefresh(effect);
    });
    effectMenu.addAction("Color Balance", [addAndRefresh]() {
      auto effect = std::make_shared<ColorBalanceEffect>();
      effect->setEffectID(ArtifactCore::UniString("effect.colorcorrection.colorbalance"));
      effect->setDisplayName(ArtifactCore::UniString("Color Balance"));
      addAndRefresh(effect);
    });
    effectMenu.addAction("Levels", [addAndRefresh]() {
      auto effect = std::make_shared<LevelsEffect>();
      effect->setEffectID(ArtifactCore::UniString("effect.colorcorrection.levels"));
      effect->setDisplayName(ArtifactCore::UniString("Levels"));
      addAndRefresh(effect);
    });
    effectMenu.addAction("Channel Mixer", [addAndRefresh]() {
      auto effect = std::make_shared<ChannelMixerEffect>();
      effect->setEffectID(ArtifactCore::UniString("effect.colorcorrection.channelmixer"));
      effect->setDisplayName(ArtifactCore::UniString("Channel Mixer"));
      addAndRefresh(effect);
    });
    effectMenu.addAction("Selective Color", [addAndRefresh]() {
      auto effect = std::make_shared<SelectiveColorEffect>();
      effect->setEffectID(ArtifactCore::UniString("effect.colorcorrection.selectivecolor"));
      effect->setDisplayName(ArtifactCore::UniString("Selective Color"));
      addAndRefresh(effect);
    });
    effectMenu.addSeparator();
    effectMenu.addAction("Lift / Gamma / Gain", [addAndRefresh]() {
      auto effect = std::make_shared<LiftGammaGainEffect>();
      effect->setEffectID(ArtifactCore::UniString("lift_gamma_gain"));
      effect->setDisplayName(ArtifactCore::UniString("Lift / Gamma / Gain"));
      addAndRefresh(effect);
    });
    effectMenu.addAction("Lens Distortion", [addAndRefresh]() {
      auto effect = std::make_shared<LensDistortionEffect>();
      effect->setEffectID(ArtifactCore::UniString("lens_distortion"));
      effect->setDisplayName(ArtifactCore::UniString("Lens Distortion"));
      addAndRefresh(effect);
    });
    effectMenu.addSeparator();
    effectMenu.addAction("Chroma Key", [addAndRefresh]() {
      auto effect = std::make_shared<ChromaKeyEffect>();
      effect->setEffectID(ArtifactCore::UniString("chroma_key"));
      effect->setDisplayName(ArtifactCore::UniString("Chroma Key"));
      addAndRefresh(effect);
    });
    effectMenu.addAction("Wave", [addAndRefresh]() {
      auto effect = std::make_shared<WaveEffect>();
      effect->setEffectID(ArtifactCore::UniString("wave"));
      effect->setDisplayName(ArtifactCore::UniString("Wave"));
      addAndRefresh(effect);
    });
    effectMenu.addAction("Spherize", [addAndRefresh]() {
      auto effect = std::make_shared<SpherizeEffect>();
      effect->setEffectID(ArtifactCore::UniString("spherize"));
      effect->setDisplayName(ArtifactCore::UniString("Spherize"));
      addAndRefresh(effect);
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

void ArtifactInspectorWidget::focusInEvent(QFocusEvent* event)
{
  if (auto* input = InputOperator::instance()) {
    input->setActiveContext(QString::fromLatin1(kInspectorContext));
  }
  QScrollArea::focusInEvent(event);
}

void ArtifactInspectorWidget::focusOutEvent(QFocusEvent* event)
{
  if (auto* input = InputOperator::instance()) {
    if (input->activeContext() == QString::fromLatin1(kInspectorContext)) {
      input->setActiveContext(QStringLiteral("Global"));
    }
  }
  QScrollArea::focusOutEvent(event);
}

void ArtifactInspectorWidget::keyPressEvent(QKeyEvent* event)
{
  if (auto* input = InputOperator::instance()) {
    input->setActiveContext(QString::fromLatin1(kInspectorContext));
    if (event && input->processKeyPress(this, event->key(), event->modifiers())) {
      event->accept();
      return;
    }
  }
  QScrollArea::keyPressEvent(event);
}

ArtifactInspectorWidget::ArtifactInspectorWidget(QWidget *parent /*= nullptr*/)
    : QScrollArea(parent), impl_(new Impl()) {
  setFocusPolicy(Qt::StrongFocus);
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
      "Open a composition and add a note for context.");
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
      "Select a layer, then add a note or reminder.");
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
  impl_->statusLabel = new QLabel("Status: Open a project to inspect layers");
  {
    QFont f = impl_->statusLabel->font();
    f.setItalic(true);
    impl_->statusLabel->setFont(f);
    applyInspectorLabelPalette(impl_->statusLabel, false);
  }
  layerInfoLayout->addWidget(impl_->statusLabel);

  // レイヤー名ラベル
  impl_->layerNameLabel = new QLabel("Layer: Open a project to inspect layers");
  {
    QFont f = impl_->layerNameLabel->font();
    f.setBold(true);
    f.setPointSize(13);
    impl_->layerNameLabel->setFont(f);
    applyInspectorLabelPalette(impl_->layerNameLabel, true);
  }
  layerInfoLayout->addWidget(impl_->layerNameLabel);

  // レイヤータイプラベル
  impl_->layerTypeLabel = new QLabel("Type: N/A");
  applyInspectorLabelPalette(impl_->layerTypeLabel, false);
  layerInfoLayout->addWidget(impl_->layerTypeLabel);

  impl_->matteInfoLabel = new MatteInfoLabel();
  impl_->matteInfoLabel->setText("Matte: none");
  impl_->matteInfoLabel->setWordWrap(true);
  applyInspectorLabelPalette(impl_->matteInfoLabel, false);
  layerInfoLayout->addWidget(impl_->matteInfoLabel);

  impl_->proxyInfoLabel = new ProxyInfoLabel();
  impl_->proxyInfoLabel->setText("Proxy: not available");
  impl_->proxyInfoLabel->setWordWrap(true);
  applyInspectorLabelPalette(impl_->proxyInfoLabel, false);
  layerInfoLayout->addWidget(impl_->proxyInfoLabel);

  impl_->componentsGroup = new QGroupBox("Components");
  applyInspectorSectionBox(impl_->componentsGroup);
  auto componentsLayout = new QVBoxLayout();
  impl_->componentsSummaryLabel = new QLabel("Components: select a layer");
  impl_->componentsSummaryLabel->setWordWrap(true);
  applyInspectorLabelPalette(impl_->componentsSummaryLabel, true);
  componentsLayout->addWidget(impl_->componentsSummaryLabel);

  auto componentsButtonLayout = new QHBoxLayout();
  impl_->physicsComponentButton = new QPushButton("+ Physics");
  impl_->scriptComponentButton = new QPushButton("+ Script");
  impl_->layoutComponentButton = new QPushButton("+ Layout");
  impl_->cloneComponentButton = new QPushButton("+ Cloner");
  impl_->openScriptButton = new QPushButton("Open Script");
  applyInspectorButton(impl_->physicsComponentButton, true);
  applyInspectorButton(impl_->scriptComponentButton, false);
  applyInspectorButton(impl_->layoutComponentButton, false);
  applyInspectorButton(impl_->cloneComponentButton, false);
  applyInspectorButton(impl_->openScriptButton, false);
  impl_->physicsComponentButton->setToolTip(
      QStringLiteral("Toggle the layer physics component."));
  impl_->scriptComponentButton->setToolTip(
      QStringLiteral("Toggle the layer script component."));
  impl_->layoutComponentButton->setToolTip(
      QStringLiteral("Toggle the layer Layout component."));
  impl_->cloneComponentButton->setToolTip(
      QStringLiteral("Toggle the layer Cloner component."));
  impl_->openScriptButton->setToolTip(
      QStringLiteral("Open the script file linked to this layer."));
  componentsButtonLayout->addWidget(impl_->physicsComponentButton);
  componentsButtonLayout->addWidget(impl_->scriptComponentButton);
  componentsButtonLayout->addWidget(impl_->layoutComponentButton);
  componentsButtonLayout->addWidget(impl_->cloneComponentButton);
  componentsButtonLayout->addWidget(impl_->openScriptButton);
  componentsLayout->addLayout(componentsButtonLayout);
  impl_->componentPropertyWidget = new ArtifactPropertyWidget();
  impl_->componentPropertyWidget->setVisible(false);
  impl_->componentPropertyWidget->setMinimumHeight(220);
  impl_->componentPropertyWidget->setFilterText(
      QStringLiteral("physics|script|layout|cloner"));
  componentsLayout->addWidget(impl_->componentPropertyWidget);
  componentsLayout->setContentsMargins(
      kInspectorNoteMargin, kInspectorNoteMargin, kInspectorNoteMargin,
      kInspectorNoteMargin);
  impl_->componentsGroup->setLayout(componentsLayout);
  impl_->componentsGroup->setEnabled(false);
  layerInfoLayout->addWidget(impl_->componentsGroup);

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

  auto toggleComponent = [this](const QString &propertyPath,
                                const QString &displayName) {
    if (impl_->currentCompositionId_.isNil() ||
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
    const bool nextEnabled = !layerBooleanProperty(layer, propertyPath);
    if (layer->setLayerPropertyValue(propertyPath, nextEnabled)) {
      impl_->focusComponentProperties(layer,
                                      defaultComponentInspectorFilter(layer));
      impl_->updateComponentControls(layer);
      impl_->lastLayerInfoSignature_.clear();
      impl_->scheduleRefresh(
          ArtifactInspectorWidget::Impl::LayerInfoDirty |
          ArtifactInspectorWidget::Impl::EffectsDirty);
      if (impl_->statusLabel) {
        impl_->statusLabel->setText(
            QStringLiteral("Status: %1 component %2")
                .arg(displayName, nextEnabled ? QStringLiteral("enabled")
                                              : QStringLiteral("disabled")));
      }
    }
  };
  QObject::connect(impl_->physicsComponentButton, &QPushButton::clicked, this,
                   [toggleComponent]() {
                     toggleComponent(QStringLiteral("physics.enabled"),
                                     QStringLiteral("Physics"));
                   });
  QObject::connect(impl_->scriptComponentButton, &QPushButton::clicked, this,
                   [toggleComponent]() {
                     toggleComponent(QStringLiteral("component.script.enabled"),
                                     QStringLiteral("Script"));
                   });
  QObject::connect(impl_->layoutComponentButton, &QPushButton::clicked, this,
                   [toggleComponent]() {
                     toggleComponent(QStringLiteral("component.layout.enabled"),
                                     QStringLiteral("Layout"));
                   });
  QObject::connect(impl_->cloneComponentButton, &QPushButton::clicked, this,
                   [toggleComponent]() {
                     toggleComponent(QStringLiteral("component.cloner.enabled"),
                                     QStringLiteral("Cloner"));
                   });
  QObject::connect(impl_->openScriptButton, &QPushButton::clicked, this,
                   [this]() {
                     if (impl_->currentCompositionId_.isNil() ||
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
                     const QString scriptPath = resolveScriptBindingPath(layer);
                     if (scriptPath.trimmed().isEmpty()) {
                       return;
                     }
                     const QFileInfo info(scriptPath);
                     const QString openPath =
                         info.isDir() ? info.absoluteFilePath()
                                      : info.absoluteFilePath();
                     QDesktopServices::openUrl(
                         QUrl::fromLocalFile(openPath));
                   });

  layerInfoWidget->setLayout(layerInfoLayout);
  impl_->tabWidget->addTab(layerInfoWidget, "Layer Info");

  // ================== Effects Pipeline Tab ==================
  impl_->effectsScrollArea = new QScrollArea();
  impl_->effectsScrollArea->setWidgetResizable(true);
  impl_->effectsTabWidget = new QWidget();
  auto effectsLayout = new QVBoxLayout();
  impl_->effectsStateLabel =
      new QLabel("Open a composition to manage layer effects.");
  impl_->effectsStateLabel->setWordWrap(true);
  applyInspectorLabelPalette(impl_->effectsStateLabel, false);
  effectsLayout->addWidget(impl_->effectsStateLabel);

  impl_->effectParametersHintLabel =
      new QLabel("Open a composition, then select a layer and effect. The selected effect's parameters appear below.");
  impl_->effectParametersHintLabel->setWordWrap(true);
  applyInspectorLabelPalette(impl_->effectParametersHintLabel, false);
  effectsLayout->addWidget(impl_->effectParametersHintLabel);

  impl_->effectPropertyWidget = new ArtifactPropertyWidget();
  impl_->effectPropertyWidget->setVisible(false);
  impl_->effectPropertyWidget->setMinimumHeight(220);
  effectsLayout->addWidget(impl_->effectPropertyWidget);

  QString rackNames[5] = {"Generator", "Geo Transform", "Material",
                          "Rasterizer", "Layer Transform"};

  for (int i = 0; i < 5; ++i) {
    auto rackGroup = new QGroupBox(rackNames[i]);
    impl_->racks[i].groupBox = rackGroup;
    applyInspectorSectionBox(rackGroup);
    auto rackLayout = new QVBoxLayout();

    impl_->racks[i].listWidget = new QListWidget();
    const bool rasterizerRack =
        stageFromRackIndex(i) == EffectPipelineStage::Rasterizer;
    rackGroup->setVisible(rasterizerRack);
    impl_->racks[i].listWidget->setMinimumHeight(rasterizerRack ? 72 : 36);
    impl_->racks[i].listWidget->setMaximumHeight(rasterizerRack ? 100 : 56);
    impl_->racks[i].listWidget->setUniformItemSizes(true);
    impl_->racks[i].listWidget->setSelectionMode(QAbstractItemView::ExtendedSelection);
    impl_->racks[i].listWidget->setToolTip(
        QStringLiteral("Single click an effect to edit its parameters below. Double click toggles enable/disable. Right click opens effect actions."));
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
    btnLayout->addWidget(impl_->racks[i].moveUpButton);
    btnLayout->addWidget(impl_->racks[i].moveDownButton);
    btnLayout->addWidget(impl_->racks[i].removeButton);
    impl_->racks[i].addButton->setToolTip(QStringLiteral("Add a new effect to this rack."));
    impl_->racks[i].removeButton->setToolTip(QStringLiteral("Remove the selected effect(s)."));
    impl_->racks[i].moveUpButton->setToolTip(QStringLiteral("Move the selected effect up."));
    impl_->racks[i].moveDownButton->setToolTip(QStringLiteral("Move the selected effect down."));

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
          impl_->syncFocusedEffectFromRackSelection();
        });
    QObject::connect(
        impl_->racks[i].listWidget, &QListWidget::itemDoubleClicked, this,
        [this](QListWidgetItem *item) {
          if (!item)
            return;
          const QString effectId = item->data(Qt::UserRole).toString();
          if (effectId.trimmed().isEmpty())
            return;
          const QVariant enabledData = item->data(Qt::UserRole + 1);
          const bool isEnabled = enabledData.isValid()
                                     ? enabledData.toBool()
                                     : item->text().startsWith(QStringLiteral("Enabled"));
          if (impl_->setEffectEnabledById(effectId, !isEnabled)) {
            impl_->updateEffectRackItemEnabled(effectId, !isEnabled);
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


