module;
#include <wobjectimpl.h>
#include <QApplication>
#include <QPainter>
#include <QWidget>
#include <QString>
#include <QVector>
#include <QScrollArea>
#include <QScrollBar>
#include <QBoxLayout>
#include <QPushButton>
#include <QMenu>
#include <QContextMenuEvent>
#include <QMouseEvent>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QDragLeaveEvent>
#include <QMimeData>
#include <QUrl>
#include <QDebug>
#include <QFileInfo>
#include <QHash>
#include <QSet>
#include <QPolygon>
#include <QIcon>
#include <QtSVG/QSvgRenderer>
#include <QComboBox>
#include <QPointer>
#include <QLineEdit>
#include <QKeyEvent>
#include <QWheelEvent>
#include <QInputDialog>
#include <QFileDialog>
#include <QTimer>
#include <QDrag>
module Artifact.Widgets.LayerPanelWidget;

import std;

import Utils.Path;
import Artifact.Service.Project;
import Artifact.Project.Manager;
import Artifact.Composition.Abstract;
import Artifact.Layer.Abstract;
import Artifact.Layer.Image;
import Artifact.Layer.Audio;
import Artifact.Layer.Text;
import Artifact.Layer.Shape;
import Artifact.Layer.Null;
import Artifact.Layer.AdjustableLayer;
import Artifact.Layer.Group;
import Artifact.Layer.Clone;
import Artifact.Layer.Camera;
import Artifact.Layer.Light;
import Artifact.Layer.Particle;
import Artifact.Layer.Svg;
import Artifact.Layer.Video;
import Artifact.Layers.SolidImage;
import Layer.Blend;
import Artifact.Layer.InitParams;
import File.TypeDetector;
import Event.Bus;
import Artifact.Event.Types;

namespace Artifact
{
 using namespace ArtifactCore;
namespace {
  constexpr int kLayerRowHeight = 28;
  constexpr int kLayerHeaderHeight = 26;
  constexpr int kLayerHeaderButtonSize = 24;
  constexpr int kLayerColumnWidth = 28;
  constexpr int kLayerPropertyColumnCount = 5;
  constexpr int kInlineComboHeight = 24;
  constexpr int kInlineBlendWidth = 120;
  constexpr int kInlineParentWidth = 150;
  constexpr int kInlineComboGap = 6;
  constexpr int kInlineComboMarginY = 2;
  constexpr int kInlineComboReserve = kInlineParentWidth + kInlineBlendWidth + kInlineComboGap + 10;
  constexpr int kLayerNameMinWidth = 120;
  constexpr char kLayerReorderMimeType[] = "application/x-artifact-layer-reorder";

 QIcon loadSvgAsIcon(const QString& path, int size = 16)
 {
  if (path.isEmpty()) {
   return QIcon();
  }
  if (path.endsWith(QStringLiteral(".svg"), Qt::CaseInsensitive)) {
   QSvgRenderer renderer(path);
   if (renderer.isValid()) {
    QPixmap pixmap(size, size);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    renderer.render(&painter);
    painter.end();
    if (!pixmap.isNull()) {
     return QIcon(pixmap);
    }
   }
   return QIcon();
  }
  return QIcon(path);
 }

 QIcon loadLayerPanelIcon(const QString& resourceRelativePath, const QString& fallbackFileName = {})
 {
  QIcon icon = loadSvgAsIcon(resolveIconResourcePath(resourceRelativePath));
  if (!icon.isNull()) {
   return icon;
  }
  if (!fallbackFileName.isEmpty()) {
   icon = loadSvgAsIcon(resolveIconPath(fallbackFileName));
  }
  return icon;
 }

 QPixmap loadLayerPanelPixmap(const QString& resourceRelativePath, const QString& fallbackFileName = {})
 {
  QIcon icon = loadLayerPanelIcon(resourceRelativePath, fallbackFileName);
  if (icon.isNull()) {
   return QPixmap();
  }
  // Increase pixmap resolution for High DPI displays
  QPixmap pix = icon.pixmap(32, 32);
  if (pix.isNull()) {
   pix = icon.pixmap(48, 48);
  }
  return pix;
 }
 }

 namespace {
  class LayerPanelWheelFilter final : public QObject
  {
  public:
   explicit LayerPanelWheelFilter(QScrollArea* scrollArea, QObject* parent = nullptr)
    : QObject(parent), scrollArea_(scrollArea)
   {
   }

  protected:
   bool eventFilter(QObject* watched, QEvent* event) override
   {
    Q_UNUSED(watched);
    if (!scrollArea_ || event->type() != QEvent::Wheel) {
     return QObject::eventFilter(watched, event);
    }

    auto* wheelEvent = static_cast<QWheelEvent*>(event);
    auto* bar = scrollArea_->verticalScrollBar();
    if (!bar || bar->maximum() <= 0) {
     return QObject::eventFilter(watched, event);
    }

    int delta = 0;
    if (!wheelEvent->pixelDelta().isNull()) {
     delta = wheelEvent->pixelDelta().y();
    } else {
     delta = bar->singleStep() * (wheelEvent->angleDelta().y() / 120);
     if (delta == 0) {
      delta = wheelEvent->angleDelta().y() / 6;
     }
    }

    if (delta == 0) {
     return QObject::eventFilter(watched, event);
    }

    bar->setValue(bar->value() - delta);
    wheelEvent->accept();
    return true;
   }

  private:
   QScrollArea* scrollArea_ = nullptr;
  };

  class LayerPanelDragForwardFilter final : public QObject
  {
  public:
   explicit LayerPanelDragForwardFilter(QWidget* target, QObject* parent = nullptr)
    : QObject(parent), target_(target)
   {
   }

  protected:
   bool eventFilter(QObject* watched, QEvent* event) override
   {
    if (!target_ || !watched) return false;
    
    auto* sourceWidget = qobject_cast<QWidget*>(watched);
    if (!sourceWidget) return false;

    // ビューポート上のドラッグイベントをターゲットパネルに転送
    // 転送先が accept した場合は元の event も acceptProposedAction() で受理する。
    // Qt のドラッグシステムは元の event->isAccepted() を見てドロップ可否を判断するため、
    // これを省略するとドラッグが即座に IgnoreAction で終了してしまう。
    switch (event->type()) {
     case QEvent::DragEnter: {
      auto* dragEvent = static_cast<QDragEnterEvent*>(event);
      QDragEnterEvent forwardedEvent(
          target_->mapFromGlobal(sourceWidget->mapToGlobal(dragEvent->position().toPoint())),
          dragEvent->possibleActions(),
          dragEvent->mimeData(),
          dragEvent->buttons(),
          dragEvent->modifiers());
      QCoreApplication::sendEvent(target_, &forwardedEvent);
      if (forwardedEvent.isAccepted()) {
        dragEvent->acceptProposedAction();
      }
      return true;
     }
     case QEvent::DragMove: {
      auto* dragEvent = static_cast<QDragMoveEvent*>(event);
      QDragMoveEvent forwardedEvent(
          target_->mapFromGlobal(sourceWidget->mapToGlobal(dragEvent->position().toPoint())),
          dragEvent->possibleActions(),
          dragEvent->mimeData(),
          dragEvent->buttons(),
          dragEvent->modifiers());
      QCoreApplication::sendEvent(target_, &forwardedEvent);
      if (forwardedEvent.isAccepted()) {
        dragEvent->acceptProposedAction();
      }
      return true;
     }
     case QEvent::DragLeave:
      QCoreApplication::sendEvent(target_, static_cast<QDragLeaveEvent*>(event));
      return false;
     case QEvent::Drop: {
      auto* dropEvent = static_cast<QDropEvent*>(event);
      QDropEvent forwardedEvent(
          target_->mapFromGlobal(sourceWidget->mapToGlobal(dropEvent->position().toPoint())),
          dropEvent->possibleActions(),
          dropEvent->mimeData(),
          dropEvent->buttons(),
          dropEvent->modifiers());
      QCoreApplication::sendEvent(target_, &forwardedEvent);
      if (forwardedEvent.isAccepted()) {
        dropEvent->acceptProposedAction();
      }
      return true;
     }
     default:
      return false;
    }
   }

  private:
   QWidget* target_ = nullptr;
  };

  std::shared_ptr<ArtifactAbstractComposition> safeCompositionLookup(const CompositionID& id)
  {
    auto* service = ArtifactProjectService::instance();
    if (!service) return nullptr;

    if (!id.isNil()) {
      auto result = service->findComposition(id);
      if (result.success) {
        if (auto comp = result.ptr.lock()) {
          return comp;
        }
      }
    }

    return service->currentComposition().lock();
  }

  LayerType inferLayerTypeFromFile(const QString& filePath)
  {
    if (filePath.endsWith(QStringLiteral(".svg"), Qt::CaseInsensitive)) {
      return LayerType::Shape;
    }
    ArtifactCore::FileTypeDetector detector;
    const auto type = detector.detect(filePath);
    switch (type) {
    case ArtifactCore::FileType::Image:
      return LayerType::Image;
    case ArtifactCore::FileType::Video:
      return LayerType::Video;
    case ArtifactCore::FileType::Audio:
      return LayerType::Audio;
    default:
      return LayerType::Video;
    }
  }

  QString blendModeToText(const LAYER_BLEND_TYPE mode)
  {
    switch (mode) {
    case LAYER_BLEND_TYPE::BLEND_NORMAL: return QStringLiteral("Normal");
    case LAYER_BLEND_TYPE::BLEND_ADD: return QStringLiteral("Add");
    case LAYER_BLEND_TYPE::BLEND_MULTIPLY: return QStringLiteral("Multiply");
    case LAYER_BLEND_TYPE::BLEND_SCREEN: return QStringLiteral("Screen");
    case LAYER_BLEND_TYPE::BLEND_OVERLAY: return QStringLiteral("Overlay");
    case LAYER_BLEND_TYPE::BLEND_DARKEN: return QStringLiteral("Darken");
    case LAYER_BLEND_TYPE::BLEND_LIGHTEN: return QStringLiteral("Lighten");
    case LAYER_BLEND_TYPE::BLEND_COLOR_DODGE: return QStringLiteral("Color Dodge");
    case LAYER_BLEND_TYPE::BLEND_COLOR_BURN: return QStringLiteral("Color Burn");
    case LAYER_BLEND_TYPE::BLEND_HARD_LIGHT: return QStringLiteral("Hard Light");
    case LAYER_BLEND_TYPE::BLEND_SOFT_LIGHT: return QStringLiteral("Soft Light");
    case LAYER_BLEND_TYPE::BLEND_DIFFERENCE: return QStringLiteral("Difference");
    case LAYER_BLEND_TYPE::BLEND_EXCLUSION: return QStringLiteral("Exclusion");
    case LAYER_BLEND_TYPE::BLEND_HUE: return QStringLiteral("Hue");
    case LAYER_BLEND_TYPE::BLEND_SATURATION: return QStringLiteral("Saturation");
    case LAYER_BLEND_TYPE::BLEND_COLOR: return QStringLiteral("Color");
    case LAYER_BLEND_TYPE::BLEND_LUMINOSITY: return QStringLiteral("Luminosity");
    default: return QStringLiteral("Unknown");
    }
  }

  std::vector<std::pair<QString, LAYER_BLEND_TYPE>> blendModeItems()
  {
    return {
      {QStringLiteral("Normal"), LAYER_BLEND_TYPE::BLEND_NORMAL},
      {QStringLiteral("Add"), LAYER_BLEND_TYPE::BLEND_ADD},
      {QStringLiteral("Multiply"), LAYER_BLEND_TYPE::BLEND_MULTIPLY},
      {QStringLiteral("Screen"), LAYER_BLEND_TYPE::BLEND_SCREEN},
      {QStringLiteral("Overlay"), LAYER_BLEND_TYPE::BLEND_OVERLAY},
      {QStringLiteral("Darken"), LAYER_BLEND_TYPE::BLEND_DARKEN},
      {QStringLiteral("Lighten"), LAYER_BLEND_TYPE::BLEND_LIGHTEN},
      {QStringLiteral("Color Dodge"), LAYER_BLEND_TYPE::BLEND_COLOR_DODGE},
      {QStringLiteral("Color Burn"), LAYER_BLEND_TYPE::BLEND_COLOR_BURN},
      {QStringLiteral("Hard Light"), LAYER_BLEND_TYPE::BLEND_HARD_LIGHT},
      {QStringLiteral("Soft Light"), LAYER_BLEND_TYPE::BLEND_SOFT_LIGHT},
      {QStringLiteral("Difference"), LAYER_BLEND_TYPE::BLEND_DIFFERENCE},
      {QStringLiteral("Exclusion"), LAYER_BLEND_TYPE::BLEND_EXCLUSION},
      {QStringLiteral("Hue"), LAYER_BLEND_TYPE::BLEND_HUE},
      {QStringLiteral("Saturation"), LAYER_BLEND_TYPE::BLEND_SATURATION},
      {QStringLiteral("Color"), LAYER_BLEND_TYPE::BLEND_COLOR},
      {QStringLiteral("Luminosity"), LAYER_BLEND_TYPE::BLEND_LUMINOSITY}
    };
  }

  QString humanizeTimelinePropertyLabel(QString name)
  {
   static const QHash<QString, QString> explicitLabels = {
    { QStringLiteral("transform.position.x"), QStringLiteral("Position X") },
    { QStringLiteral("transform.position.y"), QStringLiteral("Position Y") },
    { QStringLiteral("transform.scale.x"),    QStringLiteral("Scale X") },
    { QStringLiteral("transform.scale.y"),    QStringLiteral("Scale Y") },
    { QStringLiteral("transform.rotation"),   QStringLiteral("Rotation") },
    { QStringLiteral("transform.anchor.x"),   QStringLiteral("Anchor X") },
    { QStringLiteral("transform.anchor.y"),   QStringLiteral("Anchor Y") },
    { QStringLiteral("layer.opacity"),        QStringLiteral("Opacity") },
    { QStringLiteral("time.inPoint"),         QStringLiteral("In Point") },
    { QStringLiteral("time.outPoint"),        QStringLiteral("Out Point") },
    { QStringLiteral("time.startTime"),       QStringLiteral("Start Time") }
   };
   if (const auto it = explicitLabels.constFind(name); it != explicitLabels.constEnd()) {
    return it.value();
   }

   const int dot = name.lastIndexOf('.');
   if (dot >= 0 && dot + 1 < name.size()) {
    name = name.mid(dot + 1);
   }

   QString out;
   out.reserve(name.size() * 2);
   for (int i = 0; i < name.size(); ++i) {
    const QChar ch = name.at(i);
    if (ch == '_' || ch == '-') {
     out += ' ';
     continue;
    }
    if (i > 0 && ch.isUpper() && name.at(i - 1).isLetterOrNumber()) {
     out += ' ';
    }
    out += ch;
   }

   bool cap = true;
   for (int i = 0; i < out.size(); ++i) {
    if (out.at(i).isSpace()) {
     cap = true;
     continue;
    }
    if (cap) {
     out[i] = out.at(i).toUpper();
     cap = false;
    }
   }
   return out;
  }

  enum class LayerPanelRole
  {
   Source,
   Structural,
   Utility
  };

  LayerPanelRole layerPanelRoleFor(const ArtifactAbstractLayerPtr& layer)
  {
   if (!layer) {
    return LayerPanelRole::Source;
   }

   const QString className = layer->className().toQString();
   if (layer->isGroupLayer() || layer->isNullLayer() || layer->isAdjustmentLayer() ||
       layer->isCloneLayer() || className == QStringLiteral("ArtifactCameraLayer") ||
       className == QStringLiteral("ArtifactLightLayer")) {
    return LayerPanelRole::Structural;
   }

   if (layer->hasAudio()) {
    return LayerPanelRole::Source;
   }

   if (layer->hasVideo()) {
    return LayerPanelRole::Source;
   }

   return LayerPanelRole::Utility;
  }

  QString layerPanelTypeLabel(const ArtifactAbstractLayerPtr& layer)
  {
   if (!layer) {
    return QStringLiteral("SRC");
   }

   if (dynamic_cast<ArtifactGroupLayer*>(layer.get())) {
    return QStringLiteral("GRP");
   }
   if (dynamic_cast<ArtifactNullLayer*>(layer.get())) {
    return QStringLiteral("NUL");
   }
   if (dynamic_cast<ArtifactAdjustableLayer*>(layer.get())) {
    return QStringLiteral("ADJ");
   }
   if (dynamic_cast<ArtifactCloneLayer*>(layer.get())) {
    return QStringLiteral("CLN");
   }
   if (dynamic_cast<ArtifactCameraLayer*>(layer.get())) {
    return QStringLiteral("CAM");
   }
   if (dynamic_cast<ArtifactLightLayer*>(layer.get())) {
    return QStringLiteral("LGT");
   }
   if (dynamic_cast<ArtifactAudioLayer*>(layer.get())) {
    return QStringLiteral("AUD");
   }
   if (dynamic_cast<ArtifactVideoLayer*>(layer.get())) {
    return QStringLiteral("VID");
   }
   if (dynamic_cast<ArtifactImageLayer*>(layer.get())) {
    return QStringLiteral("IMG");
   }
   if (dynamic_cast<ArtifactSolidImageLayer*>(layer.get())) {
    return QStringLiteral("SOL");
   }
   if (dynamic_cast<ArtifactSvgLayer*>(layer.get())) {
    return QStringLiteral("SVG");
   }
   if (dynamic_cast<ArtifactShapeLayer*>(layer.get())) {
    return QStringLiteral("SHP");
   }
   if (dynamic_cast<ArtifactParticleLayer*>(layer.get())) {
    return QStringLiteral("PRT");
   }

   switch (layerPanelRoleFor(layer)) {
   case LayerPanelRole::Structural:
    return QStringLiteral("CTRL");
   case LayerPanelRole::Utility:
    return QStringLiteral("UTIL");
   case LayerPanelRole::Source:
   default:
    return QStringLiteral("SRC");
   }
  }

  QColor layerPanelRoleColor(const ArtifactAbstractLayerPtr& layer)
  {
   switch (layerPanelRoleFor(layer)) {
   case LayerPanelRole::Structural:
    return QColor(118, 170, 255);
   case LayerPanelRole::Utility:
    return QColor(192, 160, 255);
   case LayerPanelRole::Source:
   default:
    return QColor(98, 208, 163);
   }
  }

  QVector<QString> layerPanelGroupLabels(const ArtifactAbstractLayerPtr& layer)
  {
   QVector<QString> labels;
   if (!layer) {
    return labels;
   }
   QSet<QString> seenPropertyNames;

   for (const auto& group : layer->getLayerPropertyGroups()) {
    for (const auto& property : group.sortedProperties()) {
     if (!property || !property->isAnimatable()) {
      continue;
     }
     const QString propertyName = property->getName();
     if (propertyName.isEmpty()) {
      continue;
     }
     if (seenPropertyNames.contains(propertyName)) {
      continue;
     }
     seenPropertyNames.insert(propertyName);
     const QString label = humanizeTimelinePropertyLabel(propertyName);
     labels.push_back(label);
    }
   }

   return labels;
  }

  bool layerHasAnimatableProperties(const ArtifactAbstractLayerPtr& layer)
  {
   if (!layer) {
    return false;
   }

   for (const auto& group : layer->getLayerPropertyGroups()) {
    for (const auto& property : group.sortedProperties()) {
     if (property && property->isAnimatable()) {
      return true;
     }
    }
   }

   return false;
  }

  bool layerHasKeyframes(const ArtifactAbstractLayerPtr& layer)
  {
   if (!layer) {
    return false;
   }

   for (const auto& group : layer->getLayerPropertyGroups()) {
    for (const auto& property : group.sortedProperties()) {
     if (!property || !property->isAnimatable()) {
      continue;
     }
     if (!property->getKeyFrames().empty()) {
      return true;
     }
    }
   }

   return false;
  }

  bool layerMatchesDisplayMode(const ArtifactAbstractLayerPtr& layer,
                               const TimelineLayerDisplayMode mode,
                               const LayerID& selectedLayerId)
  {
   if (!layer) {
    return false;
   }

   switch (mode) {
   case TimelineLayerDisplayMode::AllLayers:
    return true;
   case TimelineLayerDisplayMode::SelectedOnly:
    return !selectedLayerId.isNil() && layer->id() == selectedLayerId;
   case TimelineLayerDisplayMode::AnimatedOnly:
    return layerHasAnimatableProperties(layer);
   case TimelineLayerDisplayMode::ImportantAndKeyframed:
    return layerHasKeyframes(layer) || layerPanelRoleFor(layer) != LayerPanelRole::Utility;
   case TimelineLayerDisplayMode::KeyframedOnly:
    return layerHasKeyframes(layer);
   case TimelineLayerDisplayMode::AudioOnly:
    return layer->hasAudio();
   case TimelineLayerDisplayMode::VideoOnly:
    return layer->hasVideo();
   }

   return true;
  }
 }

 // ============================================================================
 // ArtifactLayerPanelHeaderWidget Implementation
 // ============================================================================

 class ArtifactLayerPanelHeaderWidget::Impl
 {
 public:
  Impl()
  {
    visibilityIcon = loadLayerPanelPixmap(QStringLiteral("MaterialVS/neutral/visibility.svg"), QStringLiteral("visibility.png"));
    lockIcon = loadLayerPanelPixmap(QStringLiteral("MaterialVS/yellow/lock.svg"), QStringLiteral("lock.png"));
    if (lockIcon.isNull()) lockIcon = loadLayerPanelPixmap(QStringLiteral("MaterialVS/yellow/lock_open.svg"), QStringLiteral("unlock.png"));
    soloIcon = loadLayerPanelPixmap(QStringLiteral("MaterialVS/purple/group.svg"), QStringLiteral("solo.png"));
    shyIcon = loadLayerPanelPixmap(QStringLiteral("MaterialVS/orange/visibility_off.svg"));
  }
  ~Impl() = default;

  QPixmap visibilityIcon;
  QPixmap lockIcon;
  QPixmap soloIcon;
  QPixmap audioIcon;
  QPixmap shyIcon;
  
  QPushButton* visibilityButton = nullptr;
  QPushButton* lockButton = nullptr;
  QPushButton* soloButton = nullptr;
  QPushButton* audioButton = nullptr;
  QPushButton* layerNameButton = nullptr;
  QPushButton* shyButton = nullptr;
  QPushButton* parentHeaderButton = nullptr;
  QPushButton* blendHeaderButton = nullptr;
  std::function<void(bool)> shyToggledHandler_;
 };

 W_OBJECT_IMPL(ArtifactLayerPanelHeaderWidget)

 ArtifactLayerPanelHeaderWidget::ArtifactLayerPanelHeaderWidget(QWidget* parent)
  : QWidget(parent), impl_(new Impl())
 {
  impl_->visibilityIcon = loadLayerPanelPixmap(QStringLiteral("MaterialVS/neutral/visibility.svg"), QStringLiteral("visibility.png"));
  impl_->lockIcon = loadLayerPanelPixmap(QStringLiteral("MaterialVS/yellow/lock.svg"), QStringLiteral("lock.png"));
  if (impl_->lockIcon.isNull()) impl_->lockIcon = loadLayerPanelPixmap(QStringLiteral("MaterialVS/yellow/lock_open.svg"), QStringLiteral("unlock.png"));
  impl_->soloIcon = loadLayerPanelPixmap(QStringLiteral("MaterialVS/purple/group.svg"), QStringLiteral("solo.png"));
  impl_->audioIcon = loadLayerPanelPixmap(QStringLiteral("MaterialVS/neutral/volume.svg"),         QStringLiteral("volume.png"));
  impl_->shyIcon = loadLayerPanelPixmap(QStringLiteral("MaterialVS/orange/visibility_off.svg"));

  auto visButton = impl_->visibilityButton = new QPushButton();
  visButton->setFixedSize(QSize(kLayerColumnWidth, kLayerHeaderButtonSize));
  visButton->setIcon(impl_->visibilityIcon);
  visButton->setStyleSheet("background-color: #2D2D30; border: none; border-right: 1px solid #1a1a1a;");
  visButton->setFlat(true);
  visButton->setEnabled(false);

  auto lockButton = impl_->lockButton = new QPushButton();
  lockButton->setFixedSize(QSize(kLayerColumnWidth, kLayerHeaderButtonSize));
  if (!impl_->lockIcon.isNull()) lockButton->setIcon(impl_->lockIcon);
  lockButton->setStyleSheet("background-color: #2D2D30; border: none; border-right: 1px solid #1a1a1a;");
  lockButton->setEnabled(false);

  auto soloButton = impl_->soloButton = new QPushButton();
  soloButton->setFixedSize(QSize(kLayerColumnWidth, kLayerHeaderButtonSize));
  if (!impl_->soloIcon.isNull()) soloButton->setIcon(impl_->soloIcon);
  soloButton->setStyleSheet("background-color: #2D2D30; border: none; border-right: 1px solid #1a1a1a;");
  soloButton->setEnabled(false);

  auto audioButton = impl_->audioButton = new QPushButton();
  audioButton->setFixedSize(QSize(kLayerColumnWidth, kLayerHeaderButtonSize));
  if (!impl_->audioIcon.isNull()) audioButton->setIcon(impl_->audioIcon);
  audioButton->setStyleSheet("background-color: #2D2D30; border: none; border-right: 1px solid #1a1a1a;");
  audioButton->setEnabled(false);

  auto shyButton = impl_->shyButton = new QPushButton;
  shyButton->setFixedSize(QSize(kLayerColumnWidth, kLayerHeaderButtonSize));
  shyButton->setCheckable(true);
  if (!impl_->shyIcon.isNull()) shyButton->setIcon(impl_->shyIcon);
  shyButton->setToolTip("Master Shy Switch");
  shyButton->setStyleSheet("QPushButton { background-color: #2D2D30; border: none; border-right: 1px solid #1a1a1a; } QPushButton:checked { background-color: #3b3bef; }");

  auto layerNameButton = impl_->layerNameButton = new QPushButton("Layer Name");
  QString btnStyle = "QPushButton { background-color: #2D2D30; color: #CCC; border: none; border-right: 1px solid #1a1a1a; font-size: 11px; text-align: left; padding-left: 5px; }";
  layerNameButton->setStyleSheet(btnStyle);
  layerNameButton->setEnabled(false);
  
  auto parentHeader = impl_->parentHeaderButton = new QPushButton("Parent");
  parentHeader->setFixedWidth(kInlineParentWidth);
  parentHeader->setStyleSheet(btnStyle);
  parentHeader->setEnabled(false);
  
  auto blendHeader = impl_->blendHeaderButton = new QPushButton("Blend");
  blendHeader->setFixedWidth(kInlineBlendWidth);
  blendHeader->setStyleSheet(btnStyle);
  blendHeader->setEnabled(false);

  auto* layout = new QHBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(0);
  layout->addWidget(visButton);
  layout->addWidget(lockButton);
  layout->addWidget(soloButton);
  layout->addWidget(audioButton);
  layout->addWidget(shyButton);
  layout->addWidget(layerNameButton, 1);
  
  // These should match the spacing in paintEvent (kInlineComboGap = 6)
  layout->addWidget(parentHeader);
  layout->addSpacing(kInlineComboGap);
  layout->addWidget(blendHeader);
  layout->addSpacing(10); // Right margin in paintEvent logic

  QObject::connect(shyButton, &QPushButton::toggled, this, [this](bool checked) {
    if (impl_->shyToggledHandler_) {
      impl_->shyToggledHandler_(checked);
    }
  });

  setStyleSheet("background-color: #2D2D30; border-bottom: 1px solid #1a1a1a;");
  setFixedHeight(kLayerHeaderHeight);
}

 ArtifactLayerPanelHeaderWidget::~ArtifactLayerPanelHeaderWidget()
 {
  delete impl_;
 }

int ArtifactLayerPanelHeaderWidget::buttonSize() const { return kLayerHeaderButtonSize; }
int ArtifactLayerPanelHeaderWidget::iconSize() const { return 14; }
int ArtifactLayerPanelHeaderWidget::totalHeaderHeight() const
{
 return minimumHeight() > 0 ? minimumHeight() : sizeHint().height();
}

void ArtifactLayerPanelHeaderWidget::mousePressEvent(QMouseEvent* event)
{
 QWidget::mousePressEvent(event);
}

void ArtifactLayerPanelHeaderWidget::setShyToggledHandler(std::function<void(bool)> handler)
{
  impl_->shyToggledHandler_ = std::move(handler);
}

void ArtifactLayerPanelHeaderWidget::mouseMoveEvent(QMouseEvent* event)
{
 QWidget::mouseMoveEvent(event);
}

void ArtifactLayerPanelHeaderWidget::mouseReleaseEvent(QMouseEvent* event)
{
 QWidget::mouseReleaseEvent(event);
}

void ArtifactLayerPanelHeaderWidget::leaveEvent(QEvent* event)
{
 QWidget::leaveEvent(event);
}

// ============================================================================
// ArtifactLayerPanelWidget Implementation
// ============================================================================

 class ArtifactLayerPanelWidget::Impl
 {
 public:
  enum class RowKind
  {
   Layer,
   Group
  };

  struct VisibleRow
  {
   ArtifactAbstractLayerPtr layer;
   int depth = 0;
   bool hasChildren = false;
   bool expanded = true;
   RowKind kind = RowKind::Layer;
   QString label;
  };

  Impl()
  {
    visibilityIcon    = loadLayerPanelPixmap(QStringLiteral("MaterialVS/neutral/visibility.svg"),     QStringLiteral("eye.png"));
    lockIcon          = loadLayerPanelPixmap(QStringLiteral("MaterialVS/yellow/lock.svg"),            QStringLiteral("lock.png"));
    soloIcon          = loadLayerPanelPixmap(QStringLiteral("MaterialVS/purple/group.svg"),           QStringLiteral("solo.png"));
    audioIcon         = loadLayerPanelPixmap(QStringLiteral("MaterialVS/neutral/volume.svg"),         QStringLiteral("volume.png"));
    shyIcon           = loadLayerPanelPixmap(QStringLiteral("MaterialVS/neutral/shy.svg"),            QStringLiteral("shy.png"));
    // [Fix B] 右クリックメニュー用アイコンを構築時にキャッシュ（毎回 SVG パースを防ぐ）
    iconRename        = loadLayerPanelIcon(QStringLiteral("MaterialVS/blue/edit.svg"));
    iconCopy          = loadLayerPanelIcon(QStringLiteral("MaterialVS/neutral/content_copy.svg"));
    iconDelete        = loadLayerPanelIcon(QStringLiteral("MaterialVS/red/delete.svg"));
    iconFileOpen      = loadLayerPanelIcon(QStringLiteral("MaterialVS/blue/file_open.svg"));
    iconVisOn         = loadLayerPanelIcon(QStringLiteral("MaterialVS/neutral/visibility.svg"));
    iconVisOff        = loadLayerPanelIcon(QStringLiteral("MaterialVS/neutral/visibility_off.svg"));
    iconLock          = loadLayerPanelIcon(QStringLiteral("MaterialVS/yellow/lock.svg"));
    iconUnlock        = loadLayerPanelIcon(QStringLiteral("MaterialVS/yellow/lock_open.svg"));
    iconSolo          = loadLayerPanelIcon(QStringLiteral("MaterialVS/purple/group.svg"));
    iconShy           = loadLayerPanelIcon(QStringLiteral("MaterialVS/orange/visibility_off.svg"));
    iconLink          = loadLayerPanelIcon(QStringLiteral("MaterialVS/neutral/link.svg"));
    iconLinkOff       = loadLayerPanelIcon(QStringLiteral("MaterialVS/orange/link_off.svg"));
    iconCreateSolid   = loadLayerPanelIcon(QStringLiteral("MaterialVS/green/format_shapes.svg"));
    iconCreateNull    = loadLayerPanelIcon(QStringLiteral("MaterialVS/purple/group.svg"));
    iconCreateAdjust  = loadLayerPanelIcon(QStringLiteral("MaterialVS/orange/warning.svg"));
    iconCreateText    = loadLayerPanelIcon(QStringLiteral("MaterialVS/purple/title.svg"));
  }
  ~Impl() = default;

  CompositionID compositionId;
  QPixmap visibilityIcon;
  QPixmap lockIcon;
  QPixmap soloIcon;
  QPixmap audioIcon;
  QPixmap shyIcon;
  // [Fix B] 右クリックメニュー用アイコンキャッシュ
  QIcon iconRename, iconCopy, iconDelete, iconFileOpen;
  QIcon iconVisOn, iconVisOff, iconLock, iconUnlock, iconSolo, iconShy;
  QIcon iconLink, iconLinkOff;
  QIcon iconCreateSolid, iconCreateNull, iconCreateAdjust, iconCreateText;
  bool shyHidden = false;
  QString filterText;
  SearchMatchMode searchMatchMode = SearchMatchMode::AllVisible;
  TimelineLayerDisplayMode displayMode = TimelineLayerDisplayMode::AllLayers;
  int rowHeight = kLayerRowHeight;
  int propertyColumnWidth = kLayerColumnWidth * kLayerPropertyColumnCount;
  int hoveredLayerIndex = -1;
  LayerID selectedLayerId;
  QVector<VisibleRow> visibleRows;
  QHash<QString, bool> expandedByLayerId;
  QPointer<QComboBox> inlineParentEditor;
  QPointer<QComboBox> inlineBlendEditor;
  QPointer<QLineEdit> inlineNameEditor;
  LayerID editingLayerId;
  bool layerNameEditable = true;
  QPoint dragStartPos;
  LayerID dragCandidateLayerId;
  LayerID draggedLayerId;
  int dragInsertVisibleRow = -1;
  bool dragStarted_ = false;
  bool updatingLayout = false;  // 再帰呼び出し防止フラグ
  QTimer* layoutDebounceTimer = nullptr;
  QHash<QString, QMetaObject::Connection> layerChangedConnections;
  int lastContentHeight = -1;
  // EventBus 購読リスト。Qt シグナル接続は廃止し全て EventBus 経由で受け取る。
  ArtifactCore::EventBus* eventBus_ = nullptr;
  std::vector<ArtifactCore::EventBus::Subscription> eventBusSubscriptions_;
  
  void clearInlineEditors()
  {
   auto* parentEditor = inlineParentEditor.data();
   auto* blendEditor = inlineBlendEditor.data();
   auto* nameEditor = inlineNameEditor.data();

   inlineParentEditor = nullptr;
   inlineBlendEditor = nullptr;
   inlineNameEditor = nullptr;

   if (parentEditor) {
    parentEditor->hide();
    parentEditor->deleteLater();
   }

   if (blendEditor) {
    blendEditor->hide();
    blendEditor->deleteLater();
   }

   if (nameEditor) {
    nameEditor->hide();
    nameEditor->deleteLater();
   }

   editingLayerId = LayerID();
  }

  void setLayerNameEditable(bool enabled, ArtifactLayerPanelWidget* owner)
  {
   if (layerNameEditable == enabled) {
    return;
   }
   layerNameEditable = enabled;
   if (!layerNameEditable) {
    clearInlineEditors();
    if (owner) {
     owner->update();
    }
   }
  }

  void clearDragState()
  {
   dragCandidateLayerId = LayerID();
   draggedLayerId = LayerID();
   dragInsertVisibleRow = -1;
   dragStarted_ = false;
  }

  void clearLayerChangedSubscriptions()
  {
   for (auto it = layerChangedConnections.begin(); it != layerChangedConnections.end(); ++it) {
    QObject::disconnect(it.value());
   }
   layerChangedConnections.clear();
  }

  void refreshLayerChangedSubscriptions(ArtifactLayerPanelWidget* owner)
  {
   if (!owner) {
    clearLayerChangedSubscriptions();
    return;
   }

   auto comp = safeCompositionLookup(compositionId);
   if (!comp) {
    clearLayerChangedSubscriptions();
    return;
   }

   QSet<QString> activeIds;
   for (auto& layer : comp->allLayer()) {
    if (!layer) {
      continue;
    }
    const QString idStr = layer->id().toString();
    activeIds.insert(idStr);
    if (layerChangedConnections.contains(idStr)) {
      continue;
    }
    // [Optimization] layer::changed (property changes) should ONLY trigger repaint,
    // not a full layout rebuild. Structural changes are handled by separate signals.
    layerChangedConnections.insert(
      idStr,
      QObject::connect(layer.get(), &ArtifactAbstractLayer::changed, owner, [owner]() {
       owner->update();
      }));
   }

   const auto knownIds = layerChangedConnections.keys();
   for (const auto& idStr : knownIds) {
    if (activeIds.contains(idStr)) {
      continue;
    }
    QObject::disconnect(layerChangedConnections.take(idStr));
   }
  }

  int insertionVisibleRowForY(const int y) const
  {
   if (visibleRows.isEmpty()) {
    return 0;
   }
   return std::clamp<int>(
    (y + (kLayerRowHeight / 2)) / kLayerRowHeight,
    0,
    static_cast<int>(visibleRows.size()));
  }

  int layerCountBeforeVisibleRow(const int visibleRowIndex) const
  {
   int count = 0;
   const int limit = std::clamp<int>(visibleRowIndex, 0, static_cast<int>(visibleRows.size()));
   for (int i = 0; i < limit; ++i) {
    const auto& row = visibleRows[i];
    if (row.kind == RowKind::Layer && row.layer) {
     ++count;
    }
   }
   return count;
  }

  int layerCountBeforeVisibleRowExcluding(const int visibleRowIndex, const LayerID& excludedLayerId) const
  {
   int count = 0;
   const int limit = std::clamp<int>(visibleRowIndex, 0, static_cast<int>(visibleRows.size()));
   for (int i = 0; i < limit; ++i) {
    const auto& row = visibleRows[i];
    if (row.kind != RowKind::Layer || !row.layer) {
     continue;
    }
    if (!excludedLayerId.isNil() && row.layer->id() == excludedLayerId) {
     continue;
    }
    ++count;
   }
   return count;
  }

  void rebuildVisibleRows()
  {
   visibleRows.clear();

   auto comp = safeCompositionLookup(compositionId);
   if (!comp) {
    return;
   }

   QVector<ArtifactAbstractLayerPtr> layers;
    const QString needle = filterText.trimmed();
   for (auto& l : comp->allLayer()) {
     if (!l) continue;
     if (shyHidden && l->isShy()) continue;
     if (!layerMatchesDisplayMode(l, displayMode, selectedLayerId)) continue;
     if (!needle.isEmpty() && !l->layerName().contains(needle, Qt::CaseInsensitive)) continue;
     layers.push_back(l);
    }
   std::reverse(layers.begin(), layers.end());
   if (layers.isEmpty()) {
    return;
   }

   QHash<QString, ArtifactAbstractLayerPtr> byId;
   for (const auto& l : layers) {
    byId.insert(l->id().toString(), l);
   }

   QHash<QString, QVector<ArtifactAbstractLayerPtr>> children;
   QVector<ArtifactAbstractLayerPtr> roots;
   for (const auto& l : layers) {
    const QString parentId = l->parentLayerId().toString();
    if (parentId.isEmpty() || !byId.contains(parentId)) {
      roots.push_back(l);
    } else {
      children[parentId].push_back(l);
    }
   }

   QSet<QString> emitted;
   std::function<void(const ArtifactAbstractLayerPtr&, int, QSet<QString>&)> appendNode =
    [&](const ArtifactAbstractLayerPtr& node, int depth, QSet<QString>& stack) {
     if (!node) return;
     const QString nodeId = node->id().toString();
     if (stack.contains(nodeId)) return; // cycle guard
     if (emitted.contains(nodeId)) return;

     const auto nodeChildren = children.value(nodeId);
     const auto panelGroups = layerPanelGroupLabels(node);
     const bool hasChildren = !nodeChildren.isEmpty() || !panelGroups.isEmpty();
     const bool expanded = expandedByLayerId.value(nodeId, true);
     visibleRows.push_back(VisibleRow{ node, depth, hasChildren, expanded, RowKind::Layer, QString() });
     emitted.insert(nodeId);

     if (!hasChildren || !expanded) return;

     for (const auto& groupLabel : panelGroups) {
      visibleRows.push_back(VisibleRow{
       node,
       depth + 1,
       false,
       false,
       RowKind::Group,
       groupLabel
      });
     }

     stack.insert(nodeId);
     for (const auto& child : nodeChildren) {
      appendNode(child, depth + 1, stack);
     }
     stack.remove(nodeId);
   };

   for (const auto& root : roots) {
    QSet<QString> stack;
    appendNode(root, 0, stack);
   }

   // fallback: if malformed hierarchy exists, ensure all nodes are still shown once.
   for (const auto& l : layers) {
    const QString id = l->id().toString();
    if (!emitted.contains(id)) {
      QSet<QString> stack;
      appendNode(l, 0, stack);
    }
   }
  }
 };

 W_OBJECT_IMPL(ArtifactLayerPanelWidget)

 ArtifactLayerPanelWidget::ArtifactLayerPanelWidget(QWidget* parent)
  : QWidget(parent), impl_(new Impl())
 {
  setMouseTracking(true);
  setAcceptDrops(true);
  setFocusPolicy(Qt::StrongFocus);

  impl_->layoutDebounceTimer = new QTimer(this);
  impl_->layoutDebounceTimer->setSingleShot(true);
  // [Fix C] 100ms → 16ms（～60fps相当）。最小限の遅延でレイアウトの連続要求をまとめて 1 回に回す。
  impl_->layoutDebounceTimer->setInterval(16);
  QObject::connect(impl_->layoutDebounceTimer, &QTimer::timeout, this, [this]() {
    this->performUpdateLayout();
  });

  // Qt シグナルは使用しない。全てのサービスイベントは EventBus 経由で受け取る。
  // ArtifactProjectService の Qt signal (layerCreated / layerRemoved / layerSelected /
  // compositionCreated / projectChanged) への QObject::connect は廃止済み。
  impl_->eventBusSubscriptions_.push_back(
    impl_->eventBus_.subscribe<LayerChangedEvent>([this](const LayerChangedEvent& event) {
      if (event.changeType == LayerChangedEvent::ChangeType::Created) {
        if (impl_->compositionId.isNil() ||
            event.compositionId == impl_->compositionId.toString()) {
          updateLayout();
          const LayerID layerId(event.layerId);
          QMetaObject::invokeMethod(this, [this, layerId]() {
            const auto widgets = QApplication::allWidgets();
            for (QWidget* w : widgets) {
              if (!w) continue;
              const QString className =
                  QString::fromLatin1(w->metaObject()->className());
              if (className.contains("ArtifactInspectorWidget",
                                     Qt::CaseInsensitive)) {
                w->show();
                w->raise();
                w->activateWindow();
                break;
              }
            }
            if (impl_->layerNameEditable) {
              editLayerName(layerId);
            }
          }, Qt::QueuedConnection);
        }
      } else if (event.changeType == LayerChangedEvent::ChangeType::Removed) {
        if (event.compositionId == impl_->compositionId.toString()) {
          updateLayout();
        }
      }
    }));
  impl_->eventBusSubscriptions_.push_back(
    impl_->eventBus_.subscribe<LayerSelectionChangedEvent>(
        [this](const LayerSelectionChangedEvent& event) {
          const LayerID layerId(event.layerId);
          if (impl_->selectedLayerId != layerId) {
            impl_->selectedLayerId = layerId;
            update();
          }
        }));
  impl_->eventBusSubscriptions_.push_back(
    impl_->eventBus_.subscribe<CompositionCreatedEvent>(
        [this](const CompositionCreatedEvent& event) {
          if (impl_->compositionId.isNil()) {
            impl_->compositionId = CompositionID(event.compositionId);
          }
          updateLayout();
        }));
  impl_->eventBusSubscriptions_.push_back(
    impl_->eventBus_.subscribe<ProjectChangedEvent>(
        [this](const ProjectChangedEvent&) {
          updateLayout();
        }));
 }

ArtifactLayerPanelWidget::~ArtifactLayerPanelWidget()
{
  impl_->clearLayerChangedSubscriptions();
  delete impl_;
}

void ArtifactLayerPanelWidget::setComposition(const CompositionID& id)
{
  impl_->compositionId = id;
  impl_->selectedLayerId = LayerID();
  impl_->refreshLayerChangedSubscriptions(this);
  updateLayout();
}

void ArtifactLayerPanelWidget::setShyHidden(bool hidden)
{
  impl_->shyHidden = hidden;
  updateLayout();
}

void ArtifactLayerPanelWidget::setFilterText(const QString& text)
{
  if (impl_->filterText == text) {
    return;
  }
  impl_->filterText = text;
  updateLayout();
}

void ArtifactLayerPanelWidget::setSearchMatchMode(SearchMatchMode mode)
{
  if (impl_->searchMatchMode == mode) {
    return;
  }
  impl_->searchMatchMode = mode;
  updateLayout();
}

SearchMatchMode ArtifactLayerPanelWidget::searchMatchMode() const
{
  return impl_->searchMatchMode;
}

void ArtifactLayerPanelWidget::setDisplayMode(TimelineLayerDisplayMode mode)
{
  if (impl_->displayMode == mode) {
    return;
  }
  impl_->displayMode = mode;
  updateLayout();
}

TimelineLayerDisplayMode ArtifactLayerPanelWidget::displayMode() const
{
  return impl_->displayMode;
}

void ArtifactLayerPanelWidget::setRowHeight(int rowHeight)
{
  if (rowHeight <= 0 || impl_->rowHeight == rowHeight) {
    return;
  }
  impl_->rowHeight = rowHeight;
  updateLayout();
}

int ArtifactLayerPanelWidget::rowHeight() const
{
  return impl_->rowHeight;
}

void ArtifactLayerPanelWidget::setPropertyColumnWidth(int width)
{
  if (width <= 0 || impl_->propertyColumnWidth == width) {
    return;
  }
  impl_->propertyColumnWidth = width;
  updateLayout();
}

void ArtifactLayerPanelWidget::setEventBus(ArtifactCore::EventBus* eventBus)
{
  impl_->eventBus_ = eventBus;
}

void ArtifactLayerPanelWidget::setVisibleRowsChangedHandler(std::function<void()> handler)
{
  impl_->visibleRowsChangedHandler_ = std::move(handler);
}

int ArtifactLayerPanelWidget::propertyColumnWidth() const
{
  return impl_->propertyColumnWidth;
}

void ArtifactLayerPanelWidget::updateLayout()
{
  if (!impl_->layoutDebounceTimer) {
    performUpdateLayout();
    return;
  }
  impl_->layoutDebounceTimer->start();
}

void ArtifactLayerPanelWidget::performUpdateLayout()
{
  // 再帰呼び出しを防止
  if (impl_->updatingLayout) return;
  impl_->updatingLayout = true;

  const auto rowsEqual = [](const QVector<Impl::VisibleRow>& lhs,
                            const QVector<Impl::VisibleRow>& rhs) -> bool {
    if (lhs.size() != rhs.size()) {
      return false;
    }
    for (int i = 0; i < lhs.size(); ++i) {
      const auto& a = lhs[i];
      const auto& b = rhs[i];
      const QString aId = a.layer ? a.layer->id().toString() : QString();
      const QString bId = b.layer ? b.layer->id().toString() : QString();
      if (aId != bId ||
          a.depth != b.depth ||
          a.hasChildren != b.hasChildren ||
          a.expanded != b.expanded ||
          a.kind != b.kind ||
          a.label != b.label) {
        return false;
      }
    }
    return true;
  };

  const QVector<Impl::VisibleRow> oldRows = impl_->visibleRows;
  impl_->refreshLayerChangedSubscriptions(this);
  
  impl_->clearInlineEditors();
  impl_->rebuildVisibleRows();
  const bool structureChanged = !rowsEqual(oldRows, impl_->visibleRows);
  const int count = impl_->visibleRows.size();
  const int contentHeight = std::max(kLayerRowHeight, count * kLayerRowHeight);
  if (contentHeight != impl_->lastContentHeight) {
    setMinimumHeight(0);
    setMinimumHeight(contentHeight);
    setMaximumHeight(QWIDGETSIZE_MAX);
    updateGeometry();
    impl_->lastContentHeight = contentHeight;
  }
  update();
  if (structureChanged) {
    const auto event = TimelineVisibleRowsChangedEvent{};
    if (impl_->eventBus_) {
      impl_->eventBus_->post<TimelineVisibleRowsChangedEvent>(event);
    } else {
      ArtifactCore::globalEventBus().post<TimelineVisibleRowsChangedEvent>(event);
    }
    if (impl_->visibleRowsChangedHandler_) {
      impl_->visibleRowsChangedHandler_();
    }
  }
  
  impl_->updatingLayout = false;
 }

QVector<LayerID> ArtifactLayerPanelWidget::visibleTimelineRows() const
{
  QVector<LayerID> rows;
  rows.reserve(impl_->visibleRows.size());
  for (const auto& row : impl_->visibleRows) {
   if (row.kind == Impl::RowKind::Layer && row.layer) {
    rows.append(row.layer->id());
   } else {
    rows.append(LayerID::Nil());
   }
  }
  return rows;
}

QVector<LayerID> ArtifactLayerPanelWidget::matchingTimelineRows() const
{
  QVector<LayerID> rows;
  rows.reserve(impl_->visibleRows.size());
  for (const auto& row : impl_->visibleRows) {
    if (row.kind == Impl::RowKind::Layer && row.layer) {
      rows.append(row.layer->id());
    }
  }
  return rows;
}

 int ArtifactLayerPanelWidget::layerRowIndex(const LayerID& id) const
 {
  for (int i = 0; i < impl_->visibleRows.size(); ++i) {
   if (impl_->visibleRows[i].layer && impl_->visibleRows[i].layer->id() == id) {
    return i;
   }
  }
  return -1;
 }

void ArtifactLayerPanelWidget::editLayerName(const LayerID& id)
 {
  if (!impl_->layerNameEditable) {
   return;
  }
  int idx = layerRowIndex(id);
  if (idx >= 0) {
   impl_->selectedLayerId = id;
   update();

   // Fire a dummy F2 event or replicate F2 logic
   if (!impl_->inlineNameEditor) {
    auto comp = safeCompositionLookup(impl_->compositionId);
    if (!comp) return;
    auto l = comp->layerById(id);
    if (!l) return;

    impl_->inlineNameEditor = new QLineEdit(this);
    impl_->inlineNameEditor->setText(l->layerName());
    impl_->inlineNameEditor->selectAll();
    impl_->inlineNameEditor->setStyleSheet("background-color: #2D2D30; color: white; border: 1px solid #007ACC;");

    // Position it
    const int rowIndent = impl_->visibleRows[idx].depth * 14;
    const int nameStartX = kLayerColumnWidth * kLayerPropertyColumnCount;
    const int textX = nameStartX + rowIndent + (impl_->visibleRows[idx].hasChildren ? 16 : 4);
    const int editorWidth = std::max(60, width() - textX - kInlineParentWidth - kInlineBlendWidth - 8);
    impl_->inlineNameEditor->setGeometry(textX, idx * kLayerRowHeight + 2, editorWidth, kLayerRowHeight - 4);

    QObject::connect(impl_->inlineNameEditor, &QLineEdit::editingFinished, this, [this, id]() {
      if (!impl_->inlineNameEditor) return;
      QString newName = impl_->inlineNameEditor->text();
      impl_->inlineNameEditor->deleteLater();
      impl_->inlineNameEditor = nullptr;
      if (auto* svc = ArtifactProjectService::instance()) {
        svc->renameLayerInCurrentComposition(id, newName);
      }
      setFocus();
    });

    impl_->inlineNameEditor->show();
    impl_->inlineNameEditor->setFocus();
   }
  }
}

void ArtifactLayerPanelWidget::scrollToLayer(const LayerID& id)
{
  const int idx = layerRowIndex(id);
  if (idx < 0) {
    return;
  }
  impl_->selectedLayerId = id;
  update();
}

 void ArtifactLayerPanelWidget::setLayerNameEditable(bool enabled)
 {
  impl_->setLayerNameEditable(enabled, this);
 }

 bool ArtifactLayerPanelWidget::isLayerNameEditable() const
 {
  return impl_->layerNameEditable;
 }


void ArtifactLayerPanelWidget::mousePressEvent(QMouseEvent* event)
{
  setFocus();
  const int rowH = kLayerRowHeight;
  const int colW = kLayerColumnWidth;
  int idx = event->pos().y() / rowH;
  int clickX = event->pos().x();

  if (idx < 0 || idx >= impl_->visibleRows.size()) {
    impl_->clearDragState();
    return;
  }
  const auto& row = impl_->visibleRows[idx];
  auto layer = row.layer;
  if (!layer) {
    impl_->clearDragState();
    return;
  }
  auto* service = ArtifactProjectService::instance();
  if (row.kind != Impl::RowKind::Layer) {
   impl_->clearDragState();
   if (event->button() == Qt::LeftButton) {
    if (service) {
     service->selectLayer(layer->id());
    }
    update();
   }
   event->accept();
   return;
  }
  
  //名前エリアまたはスイッチ列でドラッグを開始可能にするための準備
  if (event->button() == Qt::LeftButton) {
    impl_->dragStartPos = event->pos();
    impl_->dragCandidateLayerId = layer->id();
  } else {
    impl_->clearDragState();
  }

  const int y = idx * rowH;
  const int nameStartX = colW * kLayerPropertyColumnCount;
  const int nameX = nameStartX + row.depth * 14;
  const bool showInlineCombos = (width() - (nameX + 8)) >= (kInlineComboReserve + kLayerNameMinWidth);
  const int parentRectX = width() - kInlineComboReserve;
  const QRect parentRect(parentRectX, y + kInlineComboMarginY, kInlineParentWidth, kInlineComboHeight);
  const QRect blendRect(parentRect.right() + kInlineComboGap, y + kInlineComboMarginY, kInlineBlendWidth, kInlineComboHeight);
  const bool clickInInlineCombo = parentRect.contains(event->pos()) || blendRect.contains(event->pos());

  if (event->button() == Qt::LeftButton) {
    if (!clickInInlineCombo) {
      impl_->clearInlineEditors();
    }
    if (showInlineCombos && parentRect.contains(event->pos())) {
      impl_->clearInlineEditors();
      auto* combo = new QComboBox(this);
      combo->setGeometry(parentRect);
      combo->setStyleSheet(
        "QComboBox { background:#2d2d30; color:#ddd; border:1px solid #4a4a4f; padding:1px 6px; }"
        "QComboBox::drop-down { width:18px; border-left:1px solid #4a4a4f; }");
      combo->addItem(QStringLiteral("<None>"), QString());
      if (auto comp = safeCompositionLookup(impl_->compositionId)) {
        for (const auto& candidate : comp->allLayer()) {
          if (!candidate) continue;
          if (candidate->id() == layer->id()) continue;
          combo->addItem(candidate->layerName(), candidate->id().toString());
        }
      }
      const QString currentParentId = layer->parentLayerId().toString();
      for (int i = 0; i < combo->count(); ++i) {
        if (combo->itemData(i).toString() == currentParentId) {
          combo->setCurrentIndex(i);
          break;
        }
      }
      QObject::connect(combo, QOverload<int>::of(&QComboBox::activated), this, [this, service, layer, combo](int i) {
        const QString parentId = combo->itemData(i).toString();
        if (service) {
          if (parentId.isEmpty()) {
            service->clearLayerParentInCurrentComposition(layer->id());
          } else {
            service->setLayerParentInCurrentComposition(layer->id(), LayerID(parentId));
          }
        }
        combo->deleteLater();
        updateLayout();
      });
      impl_->inlineParentEditor = combo;
      combo->show();
      combo->setFocus();
      combo->showPopup();
      event->accept();
      return;
    }
    if (showInlineCombos && blendRect.contains(event->pos())) {
      impl_->clearInlineEditors();
      auto* combo = new QComboBox(this);
      combo->setGeometry(blendRect);
      combo->setStyleSheet(
        "QComboBox { background:#2d2d30; color:#ddd; border:1px solid #4a4a4f; padding:1px 6px; }"
        "QComboBox::drop-down { width:18px; border-left:1px solid #4a4a4f; }");
      const auto items = blendModeItems();
      for (const auto& [name, mode] : items) {
        combo->addItem(name, static_cast<int>(mode));
      }
      const int currentMode = static_cast<int>(layer->layerBlendType());
      for (int i = 0; i < combo->count(); ++i) {
        if (combo->itemData(i).toInt() == currentMode) {
          combo->setCurrentIndex(i);
          break;
        }
      }
      QObject::connect(combo, QOverload<int>::of(&QComboBox::activated), this, [this, service, layer, combo](int i) {
        const auto mode = static_cast<LAYER_BLEND_TYPE>(combo->itemData(i).toInt());
        layer->setBlendMode(mode);
        // Qt signal / ProjectChangedEvent は使用しない。
        // ブレンドモードはレンダリング属性のみの変更であり、
        // layer->changed() でレンダラーに直接通知するだけで十分。
        // project->projectChanged() を呼ぶと全ウィジェットがフルリビルドされるため不適切。
        emit layer->changed();
        combo->deleteLater();
        update();
      });
      impl_->inlineBlendEditor = combo;
      combo->show();
      combo->setFocus();
      combo->showPopup();
      event->accept();
      return;
    }
    if (clickX < colW) {
      if (service) service->setLayerVisibleInCurrentComposition(layer->id(), !layer->isVisible());
    } else if (clickX < colW * 2) {
      if (service) service->setLayerLockedInCurrentComposition(layer->id(), !layer->isLocked());
    } else if (clickX < colW * 3) {
      if (service) service->setLayerSoloInCurrentComposition(layer->id(), !layer->isSolo());
    } else if (clickX < colW * 4) {
      if (service) service->setLayerShyInCurrentComposition(layer->id(), !layer->isShy());
    } else {
      const int toggleSize = 10;
      const int toggleX = nameX + 2;
      const QRect toggleRect(toggleX, idx * rowH + (rowH - toggleSize) / 2, toggleSize, toggleSize);
      if (row.hasChildren && toggleRect.contains(event->pos())) {
        const QString idStr = layer->id().toString();
        impl_->expandedByLayerId[idStr] = !impl_->expandedByLayerId.value(idStr, true);
        updateLayout();
        event->accept();
        return;
      }
    }
    // 名前エリアまたはスイッチ列でドラッグを開始可能に
    if (service) {
      service->selectLayer(layer->id());
      impl_->dragStartPos = event->pos();
      impl_->dragCandidateLayerId = layer->id();
    }
    update();
  } else if (event->button() == Qt::RightButton) {
    if (service) {
      service->selectLayer(layer->id());
    }

    QMenu menu(this);
    QAction* renameAct = menu.addAction("Rename Layer...");
    QAction* replaceSourceAct = nullptr;
    QAction* duplicateAct = menu.addAction("Duplicate Layer");
    QAction* deleteAct = menu.addAction("Delete Layer");
    QAction* expandAct = nullptr;
    QAction* collapseAct = nullptr;
    QAction* expandAllAct = nullptr;
    QAction* collapseAllAct = nullptr;
    // [Fix B] キャッシュ済みアイコンを使用（毎回 SVG パースを防止）
    renameAct->setIcon(impl_->iconRename);
    duplicateAct->setIcon(impl_->iconCopy);
    deleteAct->setIcon(impl_->iconDelete);

    const bool supportsSourceReplacement =
      static_cast<bool>(std::dynamic_pointer_cast<ArtifactImageLayer>(layer)) ||
      static_cast<bool>(std::dynamic_pointer_cast<ArtifactSvgLayer>(layer)) ||
      static_cast<bool>(std::dynamic_pointer_cast<ArtifactVideoLayer>(layer));
    if (supportsSourceReplacement) {
      replaceSourceAct = menu.addAction("Replace Source...");
      replaceSourceAct->setIcon(impl_->iconFileOpen);
    }

    if (row.hasChildren) {
      expandAct = menu.addAction("Expand Children");
      collapseAct = menu.addAction("Collapse Children");
      expandAct->setEnabled(!row.expanded);
      collapseAct->setEnabled(row.expanded);
    }
    expandAllAct = menu.addAction("Expand All");
    collapseAllAct = menu.addAction("Collapse All");

    menu.addSeparator();
    QAction* visAct  = menu.addAction(layer->isVisible() ? "Hide Layer"    : "Show Layer");
    QAction* lockAct = menu.addAction(layer->isLocked()  ? "Unlock Layer"  : "Lock Layer");
    QAction* soloAct = menu.addAction(layer->isSolo()    ? "Disable Solo"  : "Enable Solo");
    QAction* shyAct  = menu.addAction(layer->isShy()     ? "Disable Shy"   : "Enable Shy");
    visAct->setIcon( layer->isVisible()  ? impl_->iconVisOff  : impl_->iconVisOn);
    lockAct->setIcon(layer->isLocked()   ? impl_->iconUnlock  : impl_->iconLock);
    soloAct->setIcon(impl_->iconSolo);
    shyAct->setIcon( impl_->iconShy);

    QMenu* parentMenu = menu.addMenu("Parent");
    QAction* selectParentAct = parentMenu->addAction("Select Parent");
    QAction* clearParentAct  = parentMenu->addAction("Clear Parent");
    parentMenu->setIcon(impl_->iconLink);
    selectParentAct->setIcon(impl_->iconLink);
    clearParentAct->setIcon(impl_->iconLinkOff);
    selectParentAct->setEnabled(layer->hasParent());
    clearParentAct->setEnabled(layer->hasParent());

    QMenu* createMenu = menu.addMenu("Create Layer");
    QAction* createSolidAct  = createMenu->addAction("Solid Layer");
    QAction* createNullAct   = createMenu->addAction("Null Layer");
    QAction* createAdjustAct = createMenu->addAction("Adjustment Layer");
    QAction* createTextAct   = createMenu->addAction("Text Layer");
    QAction* createParticleAct = createMenu->addAction("Particle Layer");
    QAction* createCameraAct = createMenu->addAction("Camera Layer");
    createMenu->setIcon(impl_->iconCreateSolid);
    createSolidAct->setIcon(impl_->iconCreateSolid);
    createNullAct->setIcon(impl_->iconCreateNull);
    createAdjustAct->setIcon(impl_->iconCreateAdjust);
    createTextAct->setIcon(impl_->iconCreateText);
    createCameraAct->setIcon(impl_->iconCreateText);

    QAction* chosen = menu.exec(event->globalPosition().toPoint());
    auto comp = safeCompositionLookup(impl_->compositionId);

    if (chosen == renameAct) {
      bool ok = false;
      const QString newName = QInputDialog::getText(
       this,
       "Rename Layer",
       "Layer name:",
       QLineEdit::Normal,
       layer->layerName(),
       &ok);
      if (ok) {
       const QString trimmed = newName.trimmed();
       if (!trimmed.isEmpty()) {
        if (service) service->renameLayerInCurrentComposition(layer->id(), trimmed);
        update();
       }
      }
    } else if (chosen == replaceSourceAct) {
      QString filter;
      if (std::dynamic_pointer_cast<ArtifactImageLayer>(layer)) {
       filter = QStringLiteral("Images (*.png *.jpg *.jpeg *.bmp *.tif *.tiff *.webp);;All Files (*.*)");
      } else if (std::dynamic_pointer_cast<ArtifactSvgLayer>(layer)) {
       filter = QStringLiteral("SVG (*.svg);;All Files (*.*)");
      } else {
       filter = QStringLiteral("Media Files (*.mp4 *.mov *.mkv *.avi *.webm *.mp3 *.wav *.flac *.aac *.m4a *.ogg);;All Files (*.*)");
      }

      const QString filePath = QFileDialog::getOpenFileName(
       this,
       QStringLiteral("Replace Layer Source"),
       QString(),
       filter);
      if (!filePath.isEmpty() && service) {
       if (!service->replaceLayerSourceInCurrentComposition(layer->id(), filePath)) {
        qWarning() << "Replace source failed for layer" << layer->id().toString() << filePath;
       }
      }
    } else if (chosen == duplicateAct) {
      if (service) {
       if (!service->duplicateLayerInCurrentComposition(layer->id())) {
        qWarning() << "Duplicate layer failed";
       }
      }
    } else if (chosen == deleteAct) {
      if (auto* service = ArtifactProjectService::instance()) {
       const CompositionID compId = comp ? comp->id() : impl_->compositionId;
       service->removeLayerFromComposition(compId, layer->id());
      }
    } else if (chosen == expandAct && row.hasChildren) {
      impl_->expandedByLayerId[layer->id().toString()] = true;
      updateLayout();
    } else if (chosen == collapseAct && row.hasChildren) {
      impl_->expandedByLayerId[layer->id().toString()] = false;
      updateLayout();
    } else if (chosen == expandAllAct) {
      for (const auto& vr : impl_->visibleRows) {
       if (vr.layer && vr.hasChildren) {
        impl_->expandedByLayerId[vr.layer->id().toString()] = true;
       }
      }
      updateLayout();
    } else if (chosen == collapseAllAct) {
      for (const auto& vr : impl_->visibleRows) {
       if (vr.layer && vr.hasChildren) {
        impl_->expandedByLayerId[vr.layer->id().toString()] = false;
       }
      }
      updateLayout();
    } else if (chosen == visAct) {
      if (service) service->setLayerVisibleInCurrentComposition(layer->id(), !layer->isVisible());
      update();
    } else if (chosen == lockAct) {
      if (service) service->setLayerLockedInCurrentComposition(layer->id(), !layer->isLocked());
      update();
    } else if (chosen == soloAct) {
      if (service) service->setLayerSoloInCurrentComposition(layer->id(), !layer->isSolo());
      update();
    } else if (chosen == shyAct) {
      if (service) service->setLayerShyInCurrentComposition(layer->id(), !layer->isShy());
      updateLayout();
    } else if (chosen == selectParentAct) {
      if (layer->hasParent()) {
       if (service) {
        service->selectLayer(layer->parentLayerId());
       }
      }
    } else if (chosen == clearParentAct) {
      if (service) service->clearLayerParentInCurrentComposition(layer->id());
      updateLayout();
    } else if (chosen == createSolidAct) {
      ArtifactSolidLayerInitParams params(QStringLiteral("Solid"));
      if (comp) {
       auto sz = comp->settings().compositionSize();
       params.setWidth(sz.width());
       params.setHeight(sz.height());
      }
      if (service) {
       service->addLayerToCurrentComposition(params);
      }
    } else if (chosen == createNullAct) {
      ArtifactNullLayerInitParams params(QStringLiteral("Null"));
      if (comp) {
       auto sz = comp->settings().compositionSize();
       params.setWidth(sz.width());
       params.setHeight(sz.height());
      }
      if (service) {
       service->addLayerToCurrentComposition(params);
      }
    } else if (chosen == createAdjustAct) {
      ArtifactLayerInitParams params(QStringLiteral("Adjustment Layer"), LayerType::Adjustment);
      if (service) {
       service->addLayerToCurrentComposition(params);
      }
    } else if (chosen == createTextAct) {
      ArtifactTextLayerInitParams params(QStringLiteral("Text"));
      if (service) {
       service->addLayerToCurrentComposition(params);
      }
    } else if (chosen == createParticleAct) {
      ArtifactLayerInitParams params(QStringLiteral("Particle"), LayerType::Particle);
      if (service) {
       service->addLayerToCurrentComposition(params);
      }
    } else if (chosen == createCameraAct) {
      ArtifactCameraLayerInitParams params;
      if (service) {
       service->addLayerToCurrentComposition(params);
      }
    }
  }
  event->accept();
 }

void ArtifactLayerPanelWidget::mouseDoubleClickEvent(QMouseEvent* event)
{
   if (event->button() != Qt::LeftButton) {
    QWidget::mouseDoubleClickEvent(event);
    return;
   }
  const int rowH = kLayerRowHeight;
  const int colW = kLayerColumnWidth;
  const int idx = event->pos().y() / rowH;
  if (idx < 0 || idx >= impl_->visibleRows.size()) {
   QWidget::mouseDoubleClickEvent(event);
   return;
  }
  const auto& row = impl_->visibleRows[idx];
  auto layer = row.layer;
  if (!layer) {
   QWidget::mouseDoubleClickEvent(event);
   return;
  }
  if (row.kind != Impl::RowKind::Layer) {
   QWidget::mouseDoubleClickEvent(event);
   return;
  }

   if (row.hasChildren) {
    const int nameStartX = colW * kLayerPropertyColumnCount;
    const int nameX = nameStartX + row.depth * 14;
    const QRect treeHitRect(nameX, idx * rowH, std::max(40, width() - nameX), rowH);
    if (treeHitRect.contains(event->pos())) {
    const QString idStr = layer->id().toString();
    impl_->expandedByLayerId[idStr] = !impl_->expandedByLayerId.value(idStr, true);
    updateLayout();
    event->accept();
    return;
    }
   }

   if (!impl_->layerNameEditable) {
    event->accept();
    return;
   }

   const int nameStartX = colW * kLayerPropertyColumnCount;
  const bool showInlineCombos = width() >= (kLayerColumnWidth * kLayerPropertyColumnCount + kInlineComboReserve + kLayerNameMinWidth);
  const int parentRectX = width() - kInlineComboReserve;
  const int nameX = nameStartX + row.depth * 14 + (row.hasChildren ? 16 : 4);
  const int nameWidth = showInlineCombos ? std::max(20, parentRectX - nameX - 8) : std::max(20, width() - nameX - 8);
  const QRect editRect(nameX + 2, idx * rowH + 2, nameWidth, rowH - 4);

  if (!editRect.contains(event->pos())) {
   QWidget::mouseDoubleClickEvent(event);
   return;
  }

  impl_->clearInlineEditors();
  auto* editor = new QLineEdit(layer->layerName(), this);
  editor->setGeometry(editRect);
  editor->setStyleSheet(
   "QLineEdit { background:#2d2d30; color:#f0f0f0; border:1px solid #4a8bc2; padding:0 4px; }");
  impl_->inlineNameEditor = editor;
  impl_->editingLayerId = layer->id();
  editor->show();
  editor->setFocus();
  editor->selectAll();

  QObject::connect(editor, &QLineEdit::editingFinished, this, [this, editor]() {
   if (!editor || !editor->isVisible()) return;
   const QString newName = editor->text().trimmed();
   if (!newName.isEmpty()) {
    if (auto* service = ArtifactProjectService::instance()) {
     service->renameLayerInCurrentComposition(impl_->editingLayerId, newName);
    }
   }
   impl_->clearInlineEditors();
   update();
  });
  QObject::connect(editor, &QLineEdit::returnPressed, this, [editor]() {
   if (editor) editor->clearFocus();
  });
  event->accept();
 }

void ArtifactLayerPanelWidget::mouseMoveEvent(QMouseEvent* event)
{
  if ((event->buttons() & Qt::LeftButton) && !impl_->dragCandidateLayerId.isNil()) {
    const int dragDistance = (event->pos() - impl_->dragStartPos).manhattanLength();
    if (!impl_->dragStarted_ && dragDistance >= QApplication::startDragDistance()) {
      impl_->dragStarted_ = true;
      impl_->draggedLayerId = impl_->dragCandidateLayerId;

      auto* drag = new QDrag(this);
      auto* mime = new QMimeData();
      mime->setData(kLayerReorderMimeType,
                    impl_->draggedLayerId.toString().toUtf8());
      drag->setMimeData(mime);
      drag->setHotSpot(event->pos());

      const Qt::DropAction result = drag->exec(Qt::MoveAction);
      Q_UNUSED(result);

      impl_->clearDragState();
      unsetCursor();
      update();
      event->accept();
      return;
    }
    if (impl_->dragStarted_) {
      impl_->dragInsertVisibleRow = impl_->insertionVisibleRowForY(event->pos().y());
      setCursor(Qt::DragMoveCursor);
      update();
      event->accept();
      return;
    }
  }

  int idx = event->pos().y() / kLayerRowHeight;
  if (idx != impl_->hoveredLayerIndex) {
    const int previousHoveredIndex = impl_->hoveredLayerIndex;
    impl_->hoveredLayerIndex = idx;
    if (previousHoveredIndex >= 0 && previousHoveredIndex < impl_->visibleRows.size()) {
      update(0, previousHoveredIndex * kLayerRowHeight, width(), kLayerRowHeight);
    }
    if (idx >= 0 && idx < impl_->visibleRows.size()) {
      update(0, idx * kLayerRowHeight, width(), kLayerRowHeight);
    }
  }
  bool pointer = event->pos().x() < kLayerColumnWidth * kLayerPropertyColumnCount;
  if (!pointer && idx >= 0 && idx < impl_->visibleRows.size()) {
    const auto& row = impl_->visibleRows[idx];
    if (row.hasChildren) {
      const int nameStartX = kLayerColumnWidth * kLayerPropertyColumnCount;
      const int indent = 14;
      const int toggleSize = 10;
      const int toggleX = nameStartX + row.depth * indent + 2;
      const QRect toggleRect(toggleX, idx * kLayerRowHeight + (kLayerRowHeight - toggleSize) / 2, toggleSize, toggleSize);
      pointer = toggleRect.contains(event->pos());
    }
  }
  setCursor(pointer ? Qt::PointingHandCursor : Qt::ArrowCursor);
 }

 void ArtifactLayerPanelWidget::mouseReleaseEvent(QMouseEvent* event)
 {
  if (event->button() == Qt::LeftButton) {
    if (impl_->dragStarted_ && !impl_->draggedLayerId.isNil()) {
      auto* svc = ArtifactProjectService::instance();
      auto comp = safeCompositionLookup(impl_->compositionId);
      const LayerID dragLayerId = impl_->draggedLayerId;
      if (svc && comp && !dragLayerId.isNil()) {
        QVector<LayerID> visibleLayerIds;
        visibleLayerIds.reserve(impl_->visibleRows.size());
        for (const auto& row : impl_->visibleRows) {
          if (row.kind == Impl::RowKind::Layer && row.layer) {
            visibleLayerIds.push_back(row.layer->id());
          }
        }
        const auto allLayers = comp->allLayer();
        int oldIndex = -1;
        for (int i = 0; i < allLayers.size(); ++i) {
          if (allLayers[i] && allLayers[i]->id() == dragLayerId) {
            oldIndex = i;
            break;
          }
        }
        if (oldIndex >= 0 && !visibleLayerIds.isEmpty()) {
          QVector<LayerID> remainingVisibleLayerIds;
          remainingVisibleLayerIds.reserve(visibleLayerIds.size());
          for (const auto& layerId : visibleLayerIds) {
            if (layerId != dragLayerId) {
              remainingVisibleLayerIds.push_back(layerId);
            }
          }
          const int targetVisibleIndex = std::clamp(
            impl_->layerCountBeforeVisibleRowExcluding(impl_->dragInsertVisibleRow, dragLayerId),
            0,
            static_cast<int>(remainingVisibleLayerIds.size()));
          int newIndex = oldIndex;
          if (targetVisibleIndex >= static_cast<int>(remainingVisibleLayerIds.size())) {
            newIndex = static_cast<int>(allLayers.size()) - 1;
          } else {
            const LayerID targetLayerId = remainingVisibleLayerIds[targetVisibleIndex];
            int targetIndex = -1;
            for (int i = 0; i < allLayers.size(); ++i) {
              if (allLayers[i] && allLayers[i]->id() == targetLayerId) {
                targetIndex = i;
                break;
              }
            }
            if (targetIndex >= 0) {
              if (oldIndex < targetIndex) --targetIndex;
              newIndex = targetIndex;
            }
          }
          newIndex = std::clamp(newIndex, 0, std::max(0, static_cast<int>(allLayers.size()) - 1));
          if (newIndex != oldIndex) {
            svc->moveLayerInCurrentComposition(dragLayerId, newIndex);
            updateLayout();
          }
        }
      }
      impl_->clearDragState();
      unsetCursor();
      update();
      event->accept();
      return;
    }
  }

  impl_->clearDragState();
  unsetCursor();
  QWidget::mouseReleaseEvent(event);
 }

void ArtifactLayerPanelWidget::keyPressEvent(QKeyEvent* event)
{
  if (event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace) {
    if (!impl_->selectedLayerId.isNil()) {
      if (auto* service = ArtifactProjectService::instance()) {
        auto comp = safeCompositionLookup(impl_->compositionId);
        const CompositionID compId = comp ? comp->id() : impl_->compositionId;
        if (!compId.isNil()) {
          service->removeLayerFromComposition(compId, impl_->selectedLayerId);
          event->accept();
          return;
        }
      }
    }
  }

  auto moveSelectedLayerBy = [this](int delta) -> bool {
    if (impl_->selectedLayerId.isNil()) {
      return false;
    }

    auto* service = ArtifactProjectService::instance();
    auto comp = service ? service->currentComposition().lock() : nullptr;
    if (!service || !comp) {
      return false;
    }

    const auto layers = comp->allLayer();
    int currentLayerIndex = -1;
    for (int i = 0; i < layers.size(); ++i) {
      if (layers[i] && layers[i]->id() == impl_->selectedLayerId) {
        currentLayerIndex = i;
        break;
      }
    }
    if (currentLayerIndex < 0) {
      return false;
    }

    const int newIndex = std::clamp(currentLayerIndex + delta, 0, static_cast<int>(layers.size()) - 1);
    if (newIndex == currentLayerIndex) {
      return true;
    }

    if (!service->moveLayerInCurrentComposition(impl_->selectedLayerId, newIndex)) {
      return false;
    }

    updateLayout();
    if (auto* svc = ArtifactProjectService::instance()) {
      svc->selectLayer(impl_->selectedLayerId);
    }
    return true;
  };

  if ((event->modifiers() & Qt::AltModifier) &&
      !(event->modifiers() & (Qt::ControlModifier | Qt::ShiftModifier | Qt::MetaModifier))) {
    if (event->key() == Qt::Key_Up) {
      if (moveSelectedLayerBy(-1)) {
        event->accept();
        return;
      }
    } else if (event->key() == Qt::Key_Down) {
      if (moveSelectedLayerBy(+1)) {
        event->accept();
        return;
      }
    }
  }

  // Ctrl + [ / ] でレイヤー順序を移動
  if (event->modifiers() & Qt::ControlModifier) {
    if (event->key() == Qt::Key_BracketLeft || event->key() == Qt::Key_BracketRight) {
      if (moveSelectedLayerBy(event->key() == Qt::Key_BracketLeft ? -1 : +1)) {
        event->accept();
        return;
      }
    }
  }

  if (event->key() == Qt::Key_Left || event->key() == Qt::Key_Right) {
   int selectedIdx = -1;
   for (int i = 0; i < impl_->visibleRows.size(); ++i) {
    if (impl_->visibleRows[i].layer && impl_->visibleRows[i].layer->id() == impl_->selectedLayerId) {
     selectedIdx = i;
     break;
    }
   }
   if (selectedIdx >= 0) {
    const auto& row = impl_->visibleRows[selectedIdx];
    if (row.layer && row.hasChildren) {
     const QString idStr = row.layer->id().toString();
     const bool current = impl_->expandedByLayerId.value(idStr, true);
     const bool next = (event->key() == Qt::Key_Right) ? true : false;
     if (current != next) {
      impl_->expandedByLayerId[idStr] = next;
      updateLayout();
     }
     event->accept();
     return;
    }
   }
  }

  if (impl_->layerNameEditable && event->key() == Qt::Key_F2 && !impl_->inlineNameEditor) {
   int selectedIdx = -1;
   for (int i = 0; i < impl_->visibleRows.size(); ++i) {
    if (impl_->visibleRows[i].layer && impl_->visibleRows[i].layer->id() == impl_->selectedLayerId) {
     selectedIdx = i;
     break;
    }
   }
   if (selectedIdx >= 0) {
    const int y = selectedIdx * kLayerRowHeight + kLayerRowHeight / 2;
    const int x = kLayerColumnWidth * kLayerPropertyColumnCount + 20;
    QMouseEvent fakeEvent(QEvent::MouseButtonDblClick, QPointF(x, y), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    mouseDoubleClickEvent(&fakeEvent);
    event->accept();
    return;
   }
  } else if (event->key() == Qt::Key_Escape && impl_->inlineNameEditor) {
   impl_->clearInlineEditors();
   update();
   event->accept();
   return;
  }
  QWidget::keyPressEvent(event);
  }

  void ArtifactLayerPanelWidget::wheelEvent(QWheelEvent* event)
  {
   const int delta = event->angleDelta().y();
   if (delta == 0 || impl_->visibleRows.isEmpty()) {
    QWidget::wheelEvent(event);
    return;
   }
   
   // マウスの位置をチェック（ブレンドモードエリアか？）
   const int mouseX = event->position().x();
   const int mouseY = event->position().y();

   // ホイール操作対象の行を取得
   const int rowIdx = mouseY / kLayerRowHeight;
   const bool showInlineCombos = width() >= (kLayerColumnWidth * kLayerPropertyColumnCount + kInlineComboReserve + kLayerNameMinWidth);
   const int parentRectX = width() - kInlineComboReserve;
   const QRect parentRect(parentRectX, rowIdx * kLayerRowHeight + kInlineComboMarginY, kInlineParentWidth, kInlineComboHeight);
   const QRect blendRect(parentRect.right() + kInlineComboGap, rowIdx * kLayerRowHeight + kInlineComboMarginY, kInlineBlendWidth, kInlineComboHeight);
   const bool isBlendModeArea = showInlineCombos &&
                                rowIdx >= 0 && rowIdx < impl_->visibleRows.size() &&
                                blendRect.contains(QPoint(mouseX, mouseY));
   
   if (isBlendModeArea) {
    // ブレンドモードエリア：ホイールでブレンドモードを変更
    const auto& row = impl_->visibleRows[rowIdx];
    if (row.kind == Impl::RowKind::Layer && row.layer) {
      auto* service = ArtifactProjectService::instance();
      auto comp = service ? service->currentComposition().lock() : nullptr;
      if (comp) {
        auto layer = comp->layerById(row.layer->id());
        if (layer) {
          const auto items = blendModeItems();
          const int currentMode = static_cast<int>(layer->layerBlendType());
          int currentIndex = 0;
          for (int i = 0; i < items.size(); ++i) {
            if (static_cast<int>(items[i].second) == currentMode) {
              currentIndex = i;
              break;
            }
          }
          const int dir = (delta > 0) ? -1 : 1;
          int newIndex = (currentIndex + dir + items.size()) % items.size();
          const auto newMode = items[newIndex].second;
          layer->setBlendMode(newMode);
          // [Fix 2] projectChanged() の代わりに layer->changed() を発火。
          // projectChanged → updateLayout() 連鎖を避け、再描画のみに留める。
          emit layer->changed();
          update();
          event->accept();
          return;
        }
      }
    }
   }
   
   // それ以外：選択レイヤーを変更
   // 現在の選択インデックスを探す
   int selectedIdx = -1;
   for (int i = 0; i < impl_->visibleRows.size(); ++i) {
    if (impl_->visibleRows[i].kind == Impl::RowKind::Layer &&
        impl_->visibleRows[i].layer &&
        impl_->visibleRows[i].layer->id() == impl_->selectedLayerId) {
     selectedIdx = i;
     break;
    }
   }
   // ホイール上 → 前(index小)、下 → 次(index大)
   const int dir = (delta > 0) ? -1 : 1;
   int newIdx = (selectedIdx < 0) ? (dir > 0 ? 0 : impl_->visibleRows.size() - 1)
                                  : (selectedIdx + dir);
   // RowKind::Layerの行を探す
   while (newIdx >= 0 && newIdx < impl_->visibleRows.size()) {
    if (impl_->visibleRows[newIdx].kind == Impl::RowKind::Layer &&
        impl_->visibleRows[newIdx].layer)
     break;
    newIdx += dir;
   }
   if (newIdx < 0 || newIdx >= impl_->visibleRows.size()) {
    event->accept();
    return;
   }
   const auto& row = impl_->visibleRows[newIdx];
   if (row.layer) {
    impl_->selectedLayerId = row.layer->id();
    if (auto* svc = ArtifactProjectService::instance()) {
     svc->selectLayer(row.layer->id());
    }
    update();
    event->accept();
   }
  }

  void ArtifactLayerPanelWidget::leaveEvent(QEvent*)
  {
   const int previousHoveredIndex = impl_->hoveredLayerIndex;
   impl_->hoveredLayerIndex = -1;
   if (previousHoveredIndex >= 0 && previousHoveredIndex < impl_->visibleRows.size()) {
    update(0, previousHoveredIndex * kLayerRowHeight, width(), kLayerRowHeight);
   }
  }

void ArtifactLayerPanelWidget::paintEvent(QPaintEvent* event)
{
  QPainter p(this);
  p.setRenderHint(QPainter::SmoothPixmapTransform);
  const QFont baseFont = p.font();
  const int rowH = kLayerRowHeight;
    const int colW = kLayerColumnWidth;
    const int iconSize = 16;
    const int offset = (colW - iconSize) / 2;
    const int nameStartX = colW * kLayerPropertyColumnCount;
    const int indent = 14;
    const int toggleSize = 10;
    const int roleBadgeWidth = 46;
    const int roleBadgeHeight = 18;
    const int roleBadgeMargin = 6;
  
  const QRect dirtyRect = event->rect();
  p.fillRect(dirtyRect, QColor(42, 42, 42));

  if (impl_->visibleRows.isEmpty()) {
    auto comp = safeCompositionLookup(impl_->compositionId);
    if (!comp) {
      p.setPen(QColor(150, 150, 150));
      p.drawText(rect(), Qt::AlignCenter, "No composition selected");
      return;
    }
    p.setPen(QColor(150, 150, 150));
    p.drawText(rect(), Qt::AlignCenter, "No layers");
    return;
  }

  // 可視範囲のみループを回す（仮想化）
  const int startRow = std::max(0, dirtyRect.top() / rowH);
  const int endRow = std::min(static_cast<int>(impl_->visibleRows.size() - 1), (dirtyRect.bottom() + rowH - 1) / rowH);

  for (int i = startRow; i <= endRow; ++i) {
    int y = i * rowH;
    const auto& row = impl_->visibleRows[i];
    auto l = row.layer;
    if (!l) continue;
    const bool isGroupRow = (row.kind == Impl::RowKind::Group);
    bool sel = (l->id() == impl_->selectedLayerId);
    const auto role = layerPanelRoleFor(l);

    QColor rowBase = (i % 2 == 0) ? QColor(42, 42, 42) : QColor(45, 45, 45);
    if (role == LayerPanelRole::Structural && !isGroupRow) {
      rowBase = QColor(39, 42, 48);
    } else if (role == LayerPanelRole::Utility) {
      rowBase = QColor(43, 40, 49);
    }
    if (sel && !isGroupRow) {
      rowBase = QColor(180, 110, 45);
    } else if (i == impl_->hoveredLayerIndex) {
      rowBase = rowBase.lighter(112);
    }
    p.fillRect(0, y, width(), rowH, rowBase);

    p.setPen(QColor(60, 60, 60));
    p.drawLine(0, y + rowH, width(), y + rowH);

    if (isGroupRow) {
      const int textX = nameStartX + row.depth * indent + 4;
      p.setPen(QColor(196, 196, 196));
      p.drawText(textX, y, std::max(20, width() - textX - 8), rowH, Qt::AlignVCenter | Qt::AlignLeft, row.label);
      continue;
    }

    int curX = 0;
    // Visibility
    p.setOpacity(l->isVisible() ? 1.0 : 0.3);
    if (!impl_->visibilityIcon.isNull()) {
      p.drawPixmap(QRect(curX + offset, y + offset, iconSize, iconSize), impl_->visibilityIcon);
    }
    curX += colW;
    p.setOpacity(1.0);
    p.drawLine(curX - 1, y, curX - 1, y + rowH);

    // Lock
    bool locked = l->isLocked();
    p.setOpacity(locked ? 1.0 : 0.15);
    if (!impl_->lockIcon.isNull()) {
      p.drawPixmap(QRect(curX + offset, y + offset, iconSize, iconSize), impl_->lockIcon);
    }
    curX += colW;
    p.setOpacity(1.0);
    p.drawLine(curX - 1, y, curX - 1, y + rowH);

    // Solo
    bool solo = l->isSolo();
    p.setOpacity(solo ? 1.0 : 0.15);
    if (!impl_->soloIcon.isNull()) {
      p.drawPixmap(QRect(curX + offset, y + offset, iconSize, iconSize), impl_->soloIcon);
    }
    curX += colW;
    p.setOpacity(1.0);
    p.drawLine(curX - 1, y, curX - 1, y + rowH);

    // Sound/Audio
    p.setOpacity(0.15);
    if (!impl_->audioIcon.isNull()) {
      p.drawPixmap(QRect(curX + offset, y + offset, iconSize, iconSize), impl_->audioIcon);
    }
    curX += colW;
    p.setOpacity(1.0);
    p.drawLine(curX - 1, y, curX - 1, y + rowH);

    // Shy
    bool shy = l->isShy();
    p.setOpacity(shy ? 1.0 : 0.15);
    if (!impl_->shyIcon.isNull()) {
      p.drawPixmap(QRect(curX + offset, y + offset, iconSize, iconSize), impl_->shyIcon);
    }
    curX += colW;
    p.setOpacity(1.0);
    p.drawLine(curX - 1, y, curX - 1, y + rowH);

    // Name
    const int nameX = nameStartX + row.depth * indent;
    if (row.hasChildren) {
      const int tx = nameX + 2;
      const int ty = y + (rowH - toggleSize) / 2;
      QPolygon tri;
      if (row.expanded) {
        tri << QPoint(tx, ty + 2) << QPoint(tx + toggleSize, ty + 2) << QPoint(tx + toggleSize / 2, ty + toggleSize - 1);
      } else {
        tri << QPoint(tx + 2, ty) << QPoint(tx + 2, ty + toggleSize) << QPoint(tx + toggleSize - 1, ty + toggleSize / 2);
      }
      p.setPen(Qt::NoPen);
      p.setBrush(QColor(180, 180, 180));
      p.drawPolygon(tri);
    }

    p.setPen(Qt::white);
    const int textX = nameX + (row.hasChildren ? 16 : 4);
    const bool showRoleBadge = !isGroupRow && (width() - textX) >= (roleBadgeWidth + kLayerNameMinWidth + 16);
    const bool showInlineCombos = (width() - (nameX + 8)) >= (kInlineComboReserve + kLayerNameMinWidth);
    const int parentRectX = width() - kInlineComboReserve;
    const QRect parentRect(parentRectX, y + kInlineComboMarginY, kInlineParentWidth, kInlineComboHeight);
    const QRect blendRect(parentRect.right() + kInlineComboGap, y + kInlineComboMarginY, kInlineBlendWidth, kInlineComboHeight);
    const int roleBadgeX = showInlineCombos
      ? parentRect.left() - roleBadgeWidth - 8
      : width() - roleBadgeWidth - roleBadgeMargin;
    const QRect roleRect(roleBadgeX, y + (rowH - roleBadgeHeight) / 2, roleBadgeWidth, roleBadgeHeight);
    const bool canShowRoleBadge = showRoleBadge && roleBadgeX > textX + 40;

    auto drawInlineCombo = [&](const QRect& r, const QString& label) {
      p.setPen(QColor(80, 80, 86));
      p.setBrush(QColor(38, 38, 42));
      p.drawRoundedRect(r, 3, 3);
      p.setPen(QColor(210, 210, 210));
      p.drawText(r.adjusted(6, 0, -16, 0), Qt::AlignVCenter | Qt::AlignLeft, p.fontMetrics().elidedText(label, Qt::ElideRight, r.width() - 20));
      QPolygon arrow;
      const int ax = r.right() - 10;
      const int ay = r.center().y();
      arrow << QPoint(ax - 4, ay - 2) << QPoint(ax + 4, ay - 2) << QPoint(ax, ay + 3);
      p.setBrush(QColor(170, 170, 170));
      p.setPen(Qt::NoPen);
      p.drawPolygon(arrow);
    };

    auto drawRoleBadge = [&](const QRect& r, const QString& label, const QColor& accent) {
      QColor fill = accent;
      fill.setAlpha(45);
      QColor border = accent;
      border.setAlpha(160);
      p.setPen(border);
      p.setBrush(fill);
      p.drawRoundedRect(r, 9, 9);
      p.setPen(accent.lighter(185));
      QFont badgeFont = p.font();
      badgeFont.setPointSizeF(std::max(7.0, badgeFont.pointSizeF() - 1.0));
      badgeFont.setBold(true);
      p.setFont(badgeFont);
      p.drawText(r.adjusted(6, 0, -6, 0), Qt::AlignVCenter | Qt::AlignHCenter, label);
      p.setFont(baseFont);
    };

    const QString parentId = l->parentLayerId().toString();
    QString parentName = QStringLiteral("<None>");
    if (!parentId.isEmpty()) {
      if (auto comp = safeCompositionLookup(impl_->compositionId)) {
        for (const auto& candidate : comp->allLayer()) {
          if (candidate && candidate->id().toString() == parentId) {
            parentName = candidate->layerName();
            break;
          }
        }
      }
    }

    if (canShowRoleBadge) {
      drawRoleBadge(roleRect, layerPanelTypeLabel(l), layerPanelRoleColor(l));
    }

    if (showInlineCombos) {
      drawInlineCombo(parentRect, QStringLiteral("Parent: %1").arg(parentName));
      drawInlineCombo(blendRect, QStringLiteral("Blend: %1").arg(blendModeToText(l->layerBlendType())));
    }
    p.setPen(Qt::white);
    const int rightLimit = showInlineCombos ? (canShowRoleBadge ? roleRect.left() : parentRect.left())
                                            : (canShowRoleBadge ? roleRect.left() : width());
    const int textWidth = std::max(20, rightLimit - textX - 8);
    p.drawText(textX + 4, y, textWidth, rowH, Qt::AlignVCenter | Qt::AlignLeft, l->layerName());
  }

  if (!impl_->draggedLayerId.isNil() && impl_->dragInsertVisibleRow >= 0) {
    const int lineY = std::clamp(impl_->dragInsertVisibleRow * rowH, 1, std::max(1, height() - 2));
    const QColor accent(0, 153, 255);
    QPen pen(accent, 2);
    p.setPen(pen);
    p.drawLine(0, lineY, width(), lineY);

    p.setPen(Qt::NoPen);
    p.setBrush(accent);
    const int markerSize = 6;
    p.drawEllipse(QPoint(markerSize, lineY), markerSize / 2, markerSize / 2);
    p.drawEllipse(QPoint(std::max(markerSize, width() - markerSize), lineY), markerSize / 2, markerSize / 2);
  }
}

 void ArtifactLayerPanelWidget::dragEnterEvent(QDragEnterEvent* e)
 {
  const QMimeData* mime = e->mimeData();
  if (mime && mime->hasFormat(kLayerReorderMimeType)) {
    e->acceptProposedAction();
    update();
    return;
  }
  if (mime->hasUrls()) {
    for (const auto& url : mime->urls()) {
        if (url.isLocalFile()) {
         const QString filePath = url.toLocalFile();
         const LayerType type = inferLayerTypeFromFile(filePath);
         if (type == LayerType::Image || type == LayerType::Video || type == LayerType::Audio || type == LayerType::Shape) {
           e->acceptProposedAction();
           update();
           return;
         }
       }
     }
   }
   if (mime->hasText()) {
     const QStringList paths = mime->text().split(QStringLiteral("\n"), Qt::SkipEmptyParts);
     for (const QString& path : paths) {
       const QString trimmed = path.trimmed();
       if (!trimmed.isEmpty()) {
         const LayerType type = inferLayerTypeFromFile(trimmed);
         if (type == LayerType::Image || type == LayerType::Video || type == LayerType::Audio || type == LayerType::Shape) {
           e->acceptProposedAction();
           update();
           return;
         }
       }
     }
   }
   e->ignore();
 }

 void ArtifactLayerPanelWidget::dragMoveEvent(QDragMoveEvent* e)
 {
  const QMimeData* mime = e->mimeData();
  if (mime && mime->hasFormat(kLayerReorderMimeType)) {
    impl_->dragInsertVisibleRow = impl_->insertionVisibleRowForY(e->position().y());
    e->acceptProposedAction();
    update();
    return;
  }
  if (mime && (mime->hasUrls() || mime->hasText())) {
    e->acceptProposedAction();
  } else {
     e->ignore();
   }
 }

 void ArtifactLayerPanelWidget::dragLeaveEvent(QDragLeaveEvent* e)
 {
  e->accept();
  impl_->dragInsertVisibleRow = -1;
  update();  // ビジュアルフィードバック解除
 }

 void ArtifactLayerPanelWidget::dropEvent(QDropEvent* event)
 {
  const QMimeData* mime = event->mimeData();
  if (!mime) {
    event->ignore();
    return;
  }

  if (mime->hasFormat(kLayerReorderMimeType)) {
    auto* svc = ArtifactProjectService::instance();
    auto comp = safeCompositionLookup(impl_->compositionId);

    // MIME からドラッグ中のレイヤーIDを取得（impl_->draggedLayerId は別インスタンスでは無効）
    const LayerID dragLayerId = LayerID(QString::fromUtf8(mime->data(kLayerReorderMimeType)));
    if (svc && comp && !dragLayerId.isNil()) {
      QVector<LayerID> visibleLayerIds;
      visibleLayerIds.reserve(impl_->visibleRows.size());
      for (const auto& row : impl_->visibleRows) {
        if (row.kind == Impl::RowKind::Layer && row.layer) {
          visibleLayerIds.push_back(row.layer->id());
        }
      }

      const auto allLayers = comp->allLayer();
      int oldIndex = -1;
      for (int i = 0; i < allLayers.size(); ++i) {
        if (allLayers[i] && allLayers[i]->id() == dragLayerId) {
          oldIndex = i;
          break;
        }
      }

      if (oldIndex >= 0 && !visibleLayerIds.isEmpty()) {
        // ドラッグ中のレイヤーを除いた可視レイヤーリスト
        QVector<LayerID> remainingVisibleLayerIds;
        remainingVisibleLayerIds.reserve(visibleLayerIds.size());
        for (const auto& layerId : visibleLayerIds) {
          if (layerId != dragLayerId) {
            remainingVisibleLayerIds.push_back(layerId);
          }
        }

        // ドラッグ中のレイヤーを除いた中での挿入位置
        const int targetVisibleIndex = std::clamp(
          impl_->layerCountBeforeVisibleRowExcluding(impl_->dragInsertVisibleRow, dragLayerId),
          0,
          static_cast<int>(remainingVisibleLayerIds.size()));

        int newIndex = oldIndex;
        if (targetVisibleIndex >= static_cast<int>(remainingVisibleLayerIds.size())) {
          // 末尾に挿入
          newIndex = static_cast<int>(allLayers.size()) - 1;
        } else {
          // remainingVisibleLayerIds からターゲットレイヤーを取得し、allLayers でのインデックスを求める
          const LayerID targetLayerId = remainingVisibleLayerIds[targetVisibleIndex];
          int targetIndex = -1;
          for (int i = 0; i < allLayers.size(); ++i) {
            if (allLayers[i] && allLayers[i]->id() == targetLayerId) {
              targetIndex = i;
              break;
            }
          }
          if (targetIndex >= 0) {
            if (oldIndex < targetIndex) {
              --targetIndex;
            }
            newIndex = targetIndex;
          }
        }

        newIndex = std::clamp(newIndex, 0, std::max(0, static_cast<int>(allLayers.size()) - 1));
        if (newIndex != oldIndex) {
          svc->moveLayerInCurrentComposition(dragLayerId, newIndex);
          updateLayout();
        }
      }
    }
    impl_->clearDragState();
    event->acceptProposedAction();
    update();
    return;
  }

  QStringList validPaths;

  if (mime->hasUrls()) {
    for (const auto& url : mime->urls()) {
      if (url.isLocalFile()) {
        const QString filePath = url.toLocalFile();
        const LayerType type = inferLayerTypeFromFile(filePath);
        if (type == LayerType::Image || type == LayerType::Video || type == LayerType::Audio || type == LayerType::Shape) {
          validPaths.append(filePath);
        }
      }
    }
  }

  if (validPaths.isEmpty() && mime->hasText()) {
    const QStringList paths = mime->text().split(QStringLiteral("\n"), Qt::SkipEmptyParts);
    for (const QString& path : paths) {
      const QString trimmed = path.trimmed();
      if (trimmed.isEmpty()) continue;
      const LayerType type = inferLayerTypeFromFile(trimmed);
      if (type == LayerType::Image || type == LayerType::Video || type == LayerType::Audio || type == LayerType::Shape) {
        validPaths.append(trimmed);
      }
    }
  }

  if (validPaths.isEmpty()) {
    event->ignore();
    return;
  }

  auto* svc = ArtifactProjectService::instance();
  if (!svc) {
    event->ignore();
    return;
  }

  QPointer<ArtifactLayerPanelWidget> self(this);
  svc->importAssetsFromPathsAsync(validPaths, [self, svc](QStringList imported) {
    if (!self || !svc) {
      return;
    }

    if (imported.isEmpty()) {
      return;
    }

    for (const auto& path : imported) {
      const LayerType type = inferLayerTypeFromFile(path);
      if (type == LayerType::Image) {
        ArtifactImageInitParams params(QFileInfo(path).baseName());
        params.setImagePath(path);
        svc->addLayerToCurrentComposition(params);
      } else if (type == LayerType::Shape) {
        ArtifactSvgInitParams params(QFileInfo(path).baseName());
        params.setSvgPath(path);
        svc->addLayerToCurrentComposition(params);
      } else if (type == LayerType::Audio) {
        ArtifactAudioInitParams params(QFileInfo(path).baseName());
        params.setAudioPath(path);
        svc->addLayerToCurrentComposition(params);
      } else {
        ArtifactLayerInitParams params(QFileInfo(path).baseName(), type);
        svc->addLayerToCurrentComposition(params);
      }
    }
  });

  event->acceptProposedAction();
 }

 // ============================================================================
 // ArtifactLayerTimelinePanelWrapper Implementation
 // ============================================================================

 class ArtifactLayerTimelinePanelWrapper::Impl
 {
 public:
  QScrollArea* scroll = nullptr;
  ArtifactLayerPanelHeaderWidget* header = nullptr;
  ArtifactLayerPanelWidget* panel = nullptr;
  CompositionID id;
  std::function<void()> visibleRowsChangedHandler_;
 };

 W_OBJECT_IMPL(ArtifactLayerTimelinePanelWrapper)

 ArtifactLayerTimelinePanelWrapper::ArtifactLayerTimelinePanelWrapper(QWidget* parent)
  : QWidget(parent), impl_(new Impl)
 {
  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(0,0,0,0);
  layout->setSpacing(0);

  impl_->header = new ArtifactLayerPanelHeaderWidget();
  impl_->panel = new ArtifactLayerPanelWidget();
  impl_->scroll = new QScrollArea();
   impl_->scroll->setWidget(impl_->panel);
   impl_->scroll->setWidgetResizable(true);
   impl_->scroll->setAlignment(Qt::AlignLeft | Qt::AlignTop);
   impl_->scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
   impl_->scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
   impl_->scroll->setFrameShape(QFrame::NoFrame);
   impl_->scroll->viewport()->setAcceptDrops(true);
  impl_->panel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

  impl_->panel->setVisibleRowsChangedHandler([this]() {
    const bool isEmpty = impl_->panel->visibleTimelineRows().isEmpty();
    impl_->scroll->setAlignment(isEmpty ? (Qt::AlignHCenter | Qt::AlignVCenter)
                                        : (Qt::AlignLeft | Qt::AlignTop));
    if (impl_->visibleRowsChangedHandler_) {
      impl_->visibleRowsChangedHandler_();
    }
  });

  impl_->header->setShyToggledHandler([panel = impl_->panel](bool hidden) {
    if (panel) {
      panel->setShyHidden(hidden);
    }
  });

   auto* wheelFilter = new LayerPanelWheelFilter(impl_->scroll, this);
   this->installEventFilter(wheelFilter);
   impl_->header->installEventFilter(wheelFilter);
   impl_->panel->installEventFilter(wheelFilter);
   impl_->scroll->viewport()->installEventFilter(wheelFilter);

   auto* dragFilter = new LayerPanelDragForwardFilter(impl_->panel, this);
   impl_->scroll->viewport()->installEventFilter(dragFilter);

  layout->addWidget(impl_->header);
  layout->addWidget(impl_->scroll, 1);

}

 ArtifactLayerTimelinePanelWrapper::ArtifactLayerTimelinePanelWrapper(const CompositionID& id, QWidget* parent)
  : ArtifactLayerTimelinePanelWrapper(parent)
 {
  setComposition(id);
 }

 ArtifactLayerTimelinePanelWrapper::~ArtifactLayerTimelinePanelWrapper()
  {
   delete impl_;
  }

  void ArtifactLayerTimelinePanelWrapper::setComposition(const CompositionID& id)
  {
   impl_->id = id;
   impl_->panel->setComposition(id);
  }

  void ArtifactLayerTimelinePanelWrapper::setFilterText(const QString& text)
  {
   if (impl_ && impl_->panel) {
    impl_->panel->setFilterText(text);
   }
  }

  void ArtifactLayerTimelinePanelWrapper::setSearchMatchMode(SearchMatchMode mode)
  {
   if (impl_ && impl_->panel) {
    impl_->panel->setSearchMatchMode(mode);
   }
  }

  SearchMatchMode ArtifactLayerTimelinePanelWrapper::searchMatchMode() const
  {
   return impl_ && impl_->panel ? impl_->panel->searchMatchMode() : SearchMatchMode::AllVisible;
  }

  void ArtifactLayerTimelinePanelWrapper::setDisplayMode(TimelineLayerDisplayMode mode)
  {
   if (impl_ && impl_->panel) {
    impl_->panel->setDisplayMode(mode);
   }
  }

  TimelineLayerDisplayMode ArtifactLayerTimelinePanelWrapper::displayMode() const
  {
   return impl_ && impl_->panel ? impl_->panel->displayMode() : TimelineLayerDisplayMode::AllLayers;
  }

  void ArtifactLayerTimelinePanelWrapper::setRowHeight(int rowHeight)
  {
   if (impl_ && impl_->panel) {
    impl_->panel->setRowHeight(rowHeight);
   }
  }

  int ArtifactLayerTimelinePanelWrapper::rowHeight() const
  {
   return impl_ && impl_->panel ? impl_->panel->rowHeight() : 0;
  }

  void ArtifactLayerTimelinePanelWrapper::setPropertyColumnWidth(int width)
  {
   if (impl_ && impl_->panel) {
    impl_->panel->setPropertyColumnWidth(width);
   }
  }

  int ArtifactLayerTimelinePanelWrapper::propertyColumnWidth() const
  {
   return impl_ && impl_->panel ? impl_->panel->propertyColumnWidth() : 0;
  }

  void ArtifactLayerTimelinePanelWrapper::setLayerNameEditable(bool enabled)
  {
    if (impl_ && impl_->panel) {
     impl_->panel->setLayerNameEditable(enabled);
   }
  }

  bool ArtifactLayerTimelinePanelWrapper::isLayerNameEditable() const
  {
   return impl_ && impl_->panel ? impl_->panel->isLayerNameEditable() : false;
  }

 QScrollBar* ArtifactLayerTimelinePanelWrapper::verticalScrollBar() const
 {
  return impl_->scroll->verticalScrollBar();
 }

 QVector<LayerID> ArtifactLayerTimelinePanelWrapper::visibleTimelineRows() const
 {
  if (!impl_ || !impl_->panel) {
   return {};
  }
  return impl_->panel->visibleTimelineRows();
 }

 QVector<LayerID> ArtifactLayerTimelinePanelWrapper::matchingTimelineRows() const
 {
  if (!impl_ || !impl_->panel) {
   return {};
  }
  return impl_->panel->matchingTimelineRows();
 }

  void ArtifactLayerTimelinePanelWrapper::scrollToLayer(const LayerID& id)
  {
  if (impl_ && impl_->panel) {
    impl_->panel->scrollToLayer(id);
  }
  }

void ArtifactLayerTimelinePanelWrapper::setEventBus(ArtifactCore::EventBus* eventBus)
{
  if (!impl_) {
    return;
  }
  if (impl_->panel) {
    impl_->panel->setEventBus(eventBus);
  }
}

void ArtifactLayerTimelinePanelWrapper::setVisibleRowsChangedHandler(std::function<void()> handler)
{
  if (!impl_) {
    return;
  }
  impl_->visibleRowsChangedHandler_ = std::move(handler);
  if (impl_->panel) {
    impl_->panel->setVisibleRowsChangedHandler([this]() {
      const bool isEmpty = impl_->panel->visibleTimelineRows().isEmpty();
      impl_->scroll->setAlignment(isEmpty ? (Qt::AlignHCenter | Qt::AlignVCenter)
                                          : (Qt::AlignLeft | Qt::AlignTop));
      if (impl_->visibleRowsChangedHandler_) {
        impl_->visibleRowsChangedHandler_();
      }
    });
  }
}

} // namespace Artifact
