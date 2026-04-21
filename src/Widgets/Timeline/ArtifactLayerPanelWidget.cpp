module;
#include <wobjectimpl.h>
#include <QApplication>
#include <QCoreApplication>
#include <QAction>
#include <QPainter>
#include <QPalette>
#include <QWidget>
#include <QString>
#include <QVector>
#include <QBoxLayout>
#include <QPushButton>
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
#include <QAbstractItemView>
#include <QMessageBox>
#include <QPointer>
#include <QLineEdit>
#include <QKeyEvent>
#include <QWheelEvent>
#include <QInputDialog>
#include <QFileDialog>
#include <QTimer>
#include <QDrag>
#include <QMenu>
module Artifact.Widgets.LayerPanelWidget;

import std;
import ArtifactCore.Utils.PerformanceProfiler;

import Utils.Path;
import Artifact.Service.Project;
import Artifact.Project.Manager;
import Artifact.Application.Manager;
import Artifact.Composition.Abstract;
import Artifact.Layer.Abstract;
import Artifact.Layer.Image;
import Artifact.Layer.Svg;
import Artifact.Layer.Video;
import Artifact.Timeline.KeyframeModel;
import Undo.UndoManager;
import Artifact.Service.Playback;
import Layer.Blend;
import Artifact.Layer.InitParams;
import File.TypeDetector;
import Event.Bus;
import Artifact.Event.Types;
import Widgets.Utils.CSS;

namespace Artifact
{
 using namespace ArtifactCore;
namespace {
  QColor themeColor(const QString& value, const QColor& fallback)
  {
    const QColor color(value);
    return color.isValid() ? color : fallback;
  }

  QColor mixColor(const QColor& a, const QColor& b, qreal t)
  {
    const qreal clamped = std::clamp(t, 0.0, 1.0);
    return QColor::fromRgbF(a.redF() * (1.0 - clamped) + b.redF() * clamped,
                            a.greenF() * (1.0 - clamped) + b.greenF() * clamped,
                            a.blueF() * (1.0 - clamped) + b.blueF() * clamped,
                            a.alphaF() * (1.0 - clamped) + b.alphaF() * clamped);
  }

  void applyLayerPanelButtonPalette(QPushButton* button, bool accent = false)
  {
    if (!button) {
      return;
    }
    const auto& theme = ArtifactCore::currentDCCTheme();
    const QColor background = themeColor(theme.secondaryBackgroundColor, QColor(QStringLiteral("#2D2D30")));
    const QColor surface = themeColor(theme.backgroundColor, QColor(QStringLiteral("#24272D")));
    const QColor text = themeColor(theme.textColor, QColor(QStringLiteral("#CCC")));
    const QColor border = themeColor(theme.borderColor, QColor(QStringLiteral("#1A1A1A")));
    const QColor selection = themeColor(theme.selectionColor, QColor(QStringLiteral("#3B3BEF")));
    const QColor buttonFill = accent ? themeColor(theme.accentColor, QColor(QStringLiteral("#3B3BEF"))) : background;

    button->setAutoFillBackground(true);
    button->setAttribute(Qt::WA_StyledBackground, true);
    QPalette pal = button->palette();
    pal.setColor(QPalette::Window, buttonFill);
    pal.setColor(QPalette::Button, buttonFill);
    pal.setColor(QPalette::ButtonText, text);
    pal.setColor(QPalette::WindowText, text);
    pal.setColor(QPalette::Text, text);
    pal.setColor(QPalette::Base, surface);
    pal.setColor(QPalette::Mid, border);
    pal.setColor(QPalette::Highlight, selection);
    pal.setColor(QPalette::HighlightedText, surface);
    button->setPalette(pal);
  }

  void applyLayerPanelComboPalette(QComboBox* combo)
  {
    if (!combo) {
      return;
    }
    const auto& theme = ArtifactCore::currentDCCTheme();
    const QColor background = themeColor(theme.secondaryBackgroundColor, QColor(QStringLiteral("#2D2D30")));
    const QColor surface = themeColor(theme.backgroundColor, QColor(QStringLiteral("#24272D")));
    const QColor text = themeColor(theme.textColor, QColor(QStringLiteral("#DDD")));
    const QColor border = themeColor(theme.borderColor, QColor(QStringLiteral("#4A4A4F")));
    const QColor selection = themeColor(theme.selectionColor, QColor(QStringLiteral("#3B3BEF")));

    combo->setAutoFillBackground(true);
    QPalette pal = combo->palette();
    pal.setColor(QPalette::Window, background);
    pal.setColor(QPalette::Button, background);
    pal.setColor(QPalette::Base, surface);
    pal.setColor(QPalette::ButtonText, text);
    pal.setColor(QPalette::WindowText, text);
    pal.setColor(QPalette::Text, text);
    pal.setColor(QPalette::Mid, border);
    pal.setColor(QPalette::Highlight, selection);
    pal.setColor(QPalette::HighlightedText, surface);
    combo->setPalette(pal);
    if (auto* view = combo->view()) {
      view->setPalette(pal);
      if (auto* viewport = view->viewport()) {
        viewport->setPalette(pal);
      }
    }
  }

  void applyLayerPanelLineEditPalette(QLineEdit* editor)
  {
    if (!editor) {
      return;
    }
    const auto& theme = ArtifactCore::currentDCCTheme();
    const QColor background = themeColor(theme.secondaryBackgroundColor, QColor(QStringLiteral("#2D2D30")));
    const QColor surface = themeColor(theme.backgroundColor, QColor(QStringLiteral("#24272D")));
    const QColor text = themeColor(theme.textColor, QColor(QStringLiteral("#F0F0F0")));
    const QColor border = themeColor(theme.accentColor, QColor(QStringLiteral("#4A8BC2")));

    editor->setAutoFillBackground(true);
    QPalette pal = editor->palette();
    pal.setColor(QPalette::Window, background);
    pal.setColor(QPalette::Base, surface);
    pal.setColor(QPalette::Button, background);
    pal.setColor(QPalette::Text, text);
    pal.setColor(QPalette::WindowText, text);
    pal.setColor(QPalette::ButtonText, text);
    pal.setColor(QPalette::Highlight, border);
    pal.setColor(QPalette::HighlightedText, surface);
    editor->setPalette(pal);
  }

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
   explicit LayerPanelWheelFilter(QWidget* target, QObject* parent = nullptr)
    : QObject(parent), target_(target)
   {
   }

  protected:
   bool eventFilter(QObject* watched, QEvent* event) override
   {
    Q_UNUSED(watched);
    if (!target_ || event->type() != QEvent::Wheel) {
     return QObject::eventFilter(watched, event);
    }

    auto* wheelEvent = static_cast<QWheelEvent*>(event);
    auto* sourceWidget = qobject_cast<QWidget*>(watched);
    if (!sourceWidget) {
      return QObject::eventFilter(watched, event);
    }

    const QPoint targetPos = target_->mapFromGlobal(sourceWidget->mapToGlobal(wheelEvent->position().toPoint()));
    QWheelEvent forwardedEvent(
        QPointF(targetPos),
        QPointF(sourceWidget->mapToGlobal(wheelEvent->position().toPoint())),
        wheelEvent->pixelDelta(),
        wheelEvent->angleDelta(),
        wheelEvent->buttons(),
        wheelEvent->modifiers(),
        wheelEvent->phase(),
        wheelEvent->inverted(),
        wheelEvent->source());
    QCoreApplication::sendEvent(target_, &forwardedEvent);
    if (forwardedEvent.isAccepted()) {
      wheelEvent->accept();
      return true;
    }
    return QObject::eventFilter(watched, event);
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

  std::vector<ArtifactCore::PropertyGroup> layerPanelPropertyGroups(const ArtifactAbstractLayerPtr& layer)
  {
   if (!layer) {
    return {};
   }
   auto groups = layer->getLayerPropertyGroups();
   std::vector<ArtifactCore::PropertyGroup> result;
   result.reserve(groups.size());
   for (const auto& group : groups) {
    const QString groupName = group.name().trimmed();
    if (groupName.compare(QStringLiteral("Parent"), Qt::CaseInsensitive) == 0 ||
        groupName.compare(QStringLiteral("Blend"), Qt::CaseInsensitive) == 0 ||
        groupName.compare(QStringLiteral("BlendMode"), Qt::CaseInsensitive) == 0 ||
        groupName.compare(QStringLiteral("Layer"), Qt::CaseInsensitive) == 0 ||
        groupName.compare(QStringLiteral("Physics"), Qt::CaseInsensitive) == 0) {
     continue;
    }
    if (group.propertyCount() == 0) {
     continue;
    }
    result.push_back(group);
   }
   return result;
  }

  QString compactPropertyRowLabel(const QString& propertyPath)
  {
   const QString fullLabel =
       ArtifactTimelineKeyframeModel::displayLabelForPropertyPath(propertyPath);
   const QString prefix = QStringLiteral("Transform / ");
   if (fullLabel.startsWith(prefix, Qt::CaseInsensitive)) {
    return fullLabel.mid(prefix.size());
   }
   return fullLabel;
  }

  QRect propertyKeyframeMarkerRect(const int widgetWidth, const int rowY, const int rowH)
  {
   constexpr int kMarkerSize = 14;
   constexpr int kMarkerMargin = 10;
   const int markerX = std::max(0, widgetWidth - kMarkerMargin - kMarkerSize);
   const int markerY = rowY + (rowH - kMarkerSize) / 2;
   return QRect(markerX, markerY, kMarkerSize, kMarkerSize);
  }

  bool togglePropertyKeyframeAtCurrentTime(const ArtifactCompositionPtr& composition,
                                           const ArtifactAbstractLayerPtr& layer,
                                           const QString& propertyPath,
                                           const RationalTime& currentTime)
  {
   if (!composition || !layer || propertyPath.trimmed().isEmpty()) {
    return false;
   }

   auto property = layer->getProperty(propertyPath);
   if (!property || !property->isAnimatable()) {
    return false;
   }

   if (property->hasKeyFrameAt(currentTime)) {
    property->removeKeyFrame(currentTime);
   } else {
    const QVariant value = property->interpolateValue(currentTime);
    property->addKeyFrame(currentTime, value.isValid() ? value : property->getValue());
   }

   layer->changed();
   ArtifactCore::globalEventBus().publish<LayerChangedEvent>(
       LayerChangedEvent{composition->id().toString(), layer->id().toString(),
                         LayerChangedEvent::ChangeType::Modified});
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
  visButton->setFlat(true);
  visButton->setEnabled(false);
  applyLayerPanelButtonPalette(visButton);

  auto lockButton = impl_->lockButton = new QPushButton();
  lockButton->setFixedSize(QSize(kLayerColumnWidth, kLayerHeaderButtonSize));
  if (!impl_->lockIcon.isNull()) lockButton->setIcon(impl_->lockIcon);
  lockButton->setEnabled(false);
  applyLayerPanelButtonPalette(lockButton);

  auto soloButton = impl_->soloButton = new QPushButton();
  soloButton->setFixedSize(QSize(kLayerColumnWidth, kLayerHeaderButtonSize));
  if (!impl_->soloIcon.isNull()) soloButton->setIcon(impl_->soloIcon);
  soloButton->setEnabled(false);
  applyLayerPanelButtonPalette(soloButton);

  auto audioButton = impl_->audioButton = new QPushButton();
  audioButton->setFixedSize(QSize(kLayerColumnWidth, kLayerHeaderButtonSize));
  if (!impl_->audioIcon.isNull()) audioButton->setIcon(impl_->audioIcon);
  audioButton->setEnabled(false);
  applyLayerPanelButtonPalette(audioButton);

  auto shyButton = impl_->shyButton = new QPushButton;
  shyButton->setFixedSize(QSize(kLayerColumnWidth, kLayerHeaderButtonSize));
  shyButton->setCheckable(true);
  if (!impl_->shyIcon.isNull()) shyButton->setIcon(impl_->shyIcon);
  shyButton->setToolTip("Master Shy Switch");
  applyLayerPanelButtonPalette(shyButton, true);

  auto layerNameButton = impl_->layerNameButton = new QPushButton("Layer Name");
  layerNameButton->setEnabled(false);
  applyLayerPanelButtonPalette(layerNameButton);
  
  auto parentHeader = impl_->parentHeaderButton = new QPushButton("Parent");
  parentHeader->setFixedWidth(kInlineParentWidth);
  parentHeader->setEnabled(false);
  applyLayerPanelButtonPalette(parentHeader);
  
  auto blendHeader = impl_->blendHeaderButton = new QPushButton("Blend");
  blendHeader->setFixedWidth(kInlineBlendWidth);
  blendHeader->setEnabled(false);
  applyLayerPanelButtonPalette(blendHeader);

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
    Q_EMIT shyToggled(checked);
  });

  setAutoFillBackground(true);
  {
    const auto& theme = ArtifactCore::currentDCCTheme();
    QPalette pal = palette();
    pal.setColor(QPalette::Window, themeColor(theme.secondaryBackgroundColor, QColor(QStringLiteral("#2D2D30"))));
    pal.setColor(QPalette::WindowText, themeColor(theme.textColor, QColor(QStringLiteral("#CCC"))));
    setPalette(pal);
  }
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

class ArtifactLayerPanelWidget::Impl;

using RowKind = TimelineRowKind;

struct VisibleRow {
 ArtifactAbstractLayerPtr layer;
 int depth = 0;
 bool hasChildren = false;
 bool expanded = true;
 RowKind kind = RowKind::Layer;
 QString label;
 QString propertyPath;
 QString groupKey;
};

class ArtifactLayerPanelWidget::Impl
{
public:
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
  ArtifactTimelineKeyframeModel* keyframeModel = nullptr;
  RationalTime currentTime{};
  QString currentPropertyPath;

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
  double verticalOffset = 0.0;
  int contentHeight = kLayerRowHeight;
  int hoveredLayerIndex = -1;
  LayerID selectedLayerId;
  QVector<VisibleRow> visibleRows;
  QHash<QString, bool> expandedByLayerId;
  QHash<QString, bool> expandedByGroupKey;
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
  int lastContentHeight = -1;
  // EventBus 購読リスト
  ArtifactCore::EventBus eventBus_ = ArtifactCore::globalEventBus();
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

  int maxVerticalOffset(const ArtifactLayerPanelWidget* owner) const
  {
   if (!owner) {
    return 0;
   }
   return std::max(0, contentHeight - owner->height());
  }

  int rowIndexFromViewportY(const int viewportY) const
  {
   if (rowHeight <= 0) {
    return -1;
   }
   return static_cast<int>(std::floor((static_cast<double>(viewportY) + verticalOffset) /
                                      static_cast<double>(rowHeight)));
  }

  int rowViewportY(const int rowIndex) const
  {
   return static_cast<int>(std::floor(static_cast<double>(rowIndex * rowHeight) - verticalOffset));
  }

  QRect rowViewportRect(const int rowIndex, const int height, const int width) const
  {
   return QRect(0, rowViewportY(rowIndex), width, height);
  }

  void setVerticalOffset(const double offset, ArtifactLayerPanelWidget* owner, const bool emitSignal = true)
  {
   const double clamped = std::max(0.0, std::min(offset, static_cast<double>(maxVerticalOffset(owner))));
   if (std::abs(verticalOffset - clamped) < 0.0001) {
    return;
   }
   verticalOffset = clamped;
   if (owner) {
    Q_EMIT owner->verticalOffsetChanged(verticalOffset);
    owner->update();
   }
   if (emitSignal) {
    ArtifactCore::globalEventBus().publish<TimelineVerticalScrollEvent>({verticalOffset, "ArtifactLayerPanelWidget"});
   }
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
   const auto panelGroups = layerPanelPropertyGroups(node);
   const bool hasChildren = !nodeChildren.isEmpty() || !panelGroups.empty();
   const bool expanded = expandedByLayerId.value(nodeId, true);
   visibleRows.push_back(VisibleRow{ node, depth, hasChildren, expanded, RowKind::Layer, QString(), QString() });
   emitted.insert(nodeId);

   if (!hasChildren || !expanded) return;

     for (const auto& groupDef : panelGroups) {
      const QString groupName = groupDef.name().trimmed().isEmpty()
                                   ? QStringLiteral("Layer")
                                   : groupDef.name().trimmed();
      const QString groupKey = nodeId + QStringLiteral("::") + groupName.toLower();
      const bool groupExpanded = expandedByGroupKey.value(groupKey, true);
      visibleRows.push_back(VisibleRow{
       node,
       depth + 1,
       !groupDef.sortedProperties().empty(),
       groupExpanded,
       RowKind::Group,
       groupName,
       QString(),
       groupKey
      });

      if (groupExpanded) {
       for (const auto& property : groupDef.sortedProperties()) {
        if (!property) {
         continue;
        }
        visibleRows.push_back(VisibleRow{
         node,
         depth + 2,
         false,
         false,
         RowKind::Property,
         compactPropertyRowLabel(property->getName()),
         property->getName(),
         QString()
        });
       }
      }
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
 impl_->keyframeModel = new ArtifactTimelineKeyframeModel(this);
 setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
 setMouseTracking(true);
 setAcceptDrops(true);

 QObject::connect(UndoManager::instance(), &UndoManager::historyChanged, this, [this]() {
  updateLayout();
 });

 if (auto* playback = ArtifactPlaybackService::instance()) {
  QObject::connect(playback, &ArtifactPlaybackService::frameChanged, this, [this]() {
   if (auto* playback = ArtifactPlaybackService::instance()) {
    const auto fps = playback->frameRate().framerate();
    impl_->currentTime = RationalTime(playback->currentFrame().framePosition(), fps);
    update();
   }
  });
 }

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
            this->visibleRowsChanged();
          }, Qt::QueuedConnection);
        }
      } else if (event.changeType == LayerChangedEvent::ChangeType::Removed) {
        if (event.compositionId == impl_->compositionId.toString()) {
          updateLayout();
        }
      } else if (event.changeType == LayerChangedEvent::ChangeType::Modified) {
        // Per-property change: lightweight repaint only (no layout rebuild)
        if (event.compositionId == impl_->compositionId.toString()) {
          update();
        }
      }
    }));
  impl_->eventBusSubscriptions_.push_back(
    impl_->eventBus_.subscribe<LayerSelectionChangedEvent>(
        [this](const LayerSelectionChangedEvent& event) {
          const LayerID layerId(event.layerId);
          // Guard: ignore spurious nil events when we already have a selection.
          // This prevents property-edit notifications from clearing the timeline
          // selection via the EventBus path.
          if (layerId.isNil() && !impl_->selectedLayerId.isNil()) {
            auto *app = ArtifactApplicationManager::instance();
            auto *sel = app ? app->layerSelectionManager() : nullptr;
            if (sel && sel->currentLayer()) {
              return;
            }
          }
          if (impl_->selectedLayerId != layerId) {
            const bool layerActuallyChanged = impl_->selectedLayerId != layerId;
            impl_->selectedLayerId = layerId;
            if (layerActuallyChanged) {
              impl_->currentPropertyPath.clear();
              this->propertyFocusChanged(impl_->selectedLayerId,
                                         impl_->currentPropertyPath);
            }
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
  delete impl_;
}

void ArtifactLayerPanelWidget::setComposition(const CompositionID& id)
{
  impl_->compositionId = id;
  impl_->selectedLayerId = LayerID();
  impl_->currentPropertyPath.clear();
  propertyFocusChanged(impl_->selectedLayerId, impl_->currentPropertyPath);
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

int ArtifactLayerPanelWidget::propertyColumnWidth() const
{
  return impl_->propertyColumnWidth;
}

void ArtifactLayerPanelWidget::setVerticalOffset(const double offset)
{
  impl_->setVerticalOffset(offset, this);
}

double ArtifactLayerPanelWidget::verticalOffset() const
{
  return impl_->verticalOffset;
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

  const auto rowsEqual = [](const QVector<VisibleRow>& lhs,
                            const QVector<VisibleRow>& rhs) -> bool {
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
          a.label != b.label ||
          a.propertyPath != b.propertyPath ||
          a.groupKey != b.groupKey) {
        return false;
      }
    }
    return true;
  };

  const QVector<VisibleRow> oldRows = impl_->visibleRows;
  
  impl_->clearInlineEditors();
  impl_->rebuildVisibleRows();
  const bool structureChanged = !rowsEqual(oldRows, impl_->visibleRows);
  const int count = impl_->visibleRows.size();
  const int contentHeight = std::max(kLayerRowHeight, count * kLayerRowHeight);
  impl_->contentHeight = contentHeight;
  if (contentHeight != impl_->lastContentHeight) {
    updateGeometry();
    impl_->lastContentHeight = contentHeight;
  }
  impl_->setVerticalOffset(impl_->verticalOffset, this);
  update();
  if (structureChanged) {
    ArtifactCore::globalEventBus().publish<TimelineVisibleRowsChangedEvent>({});
  }
  
  impl_->updatingLayout = false;
 }

QVector<LayerID> ArtifactLayerPanelWidget::visibleTimelineRows() const
{
  QVector<LayerID> rows;
  const auto descriptors = visibleTimelineRowDescriptors();
  rows.reserve(descriptors.size());
  for (const auto& row : descriptors) {
   if (row.kind == TimelineRowKind::Layer && !row.layerId.isNil()) {
    rows.append(row.layerId);
   } else {
    rows.append(LayerID::Nil());
   }
  }
  return rows;
}

QVector<TimelineRowDescriptor>
ArtifactLayerPanelWidget::visibleTimelineRowDescriptors() const
{
  QVector<TimelineRowDescriptor> rows;
  rows.reserve(impl_->visibleRows.size());
  for (const auto& row : impl_->visibleRows) {
   TimelineRowDescriptor descriptor;
   descriptor.layerId = row.layer ? row.layer->id() : LayerID::Nil();
   descriptor.kind = row.kind;
   descriptor.label = row.label;
   descriptor.propertyPath = row.propertyPath;
   rows.push_back(std::move(descriptor));
  }
  return rows;
}

QVector<LayerID> ArtifactLayerPanelWidget::matchingTimelineRows() const
{
  QVector<LayerID> rows;
  rows.reserve(impl_->visibleRows.size());
  for (const auto& row : impl_->visibleRows) {
    if (row.kind == RowKind::Layer && row.layer) {
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
    applyLayerPanelLineEditPalette(impl_->inlineNameEditor);

    // Position it
    const int rowIndent = impl_->visibleRows[idx].depth * 14;
    const int nameStartX = kLayerColumnWidth * kLayerPropertyColumnCount;
    const int textX = nameStartX + rowIndent + (impl_->visibleRows[idx].hasChildren ? 16 : 4);
    const int editorWidth = std::max(60, width() - textX - kInlineParentWidth - kInlineBlendWidth - 8);
    impl_->inlineNameEditor->setGeometry(textX, impl_->rowViewportY(idx) + 2, editorWidth, kLayerRowHeight - 4);

    QObject::connect(impl_->inlineNameEditor, &QLineEdit::editingFinished, this, [this, l, id]() {
     if (!impl_->inlineNameEditor) return;
     QString newName = impl_->inlineNameEditor->text();
     impl_->inlineNameEditor->deleteLater();
     impl_->inlineNameEditor = nullptr;
     if (newName != l->layerName()) {
      const QString oldName = l->layerName();
      auto* cmd = new RenameLayerCommand(l, oldName, newName);
      UndoManager::instance()->push(std::unique_ptr<RenameLayerCommand>(cmd));
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
  const int desiredTop = std::max(0, idx * impl_->rowHeight - (height() / 3));
  impl_->setVerticalOffset(static_cast<double>(desiredTop), this);
  update();
}

LayerID ArtifactLayerPanelWidget::selectedLayerId() const {
  return impl_ ? impl_->selectedLayerId : LayerID{};
}

QString ArtifactLayerPanelWidget::currentPropertyPath() const {
  return impl_ ? impl_->currentPropertyPath : QString{};
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
  int idx = impl_->rowIndexFromViewportY(event->pos().y());
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
  if (row.kind == RowKind::Group) {
   if (event->button() == Qt::LeftButton) {
    if (!row.groupKey.trimmed().isEmpty()) {
      impl_->expandedByGroupKey[row.groupKey] = !impl_->expandedByGroupKey.value(row.groupKey, true);
      updateLayout();
    }
   }
   event->accept();
   return;
  }
  if (row.kind == RowKind::Property) {
   if (event->button() == Qt::LeftButton) {
    RationalTime currentTime = impl_->currentTime;
    if (auto comp = safeCompositionLookup(impl_->compositionId)) {
     const auto fps = std::max(1.0, static_cast<double>(comp->frameRate().framerate()));
     currentTime = RationalTime(comp->framePosition().framePosition(), fps);
    }
    const QRect keyframeRect =
        propertyKeyframeMarkerRect(width(), impl_->rowViewportY(idx), rowH);
    if (keyframeRect.contains(event->pos())) {
     if (auto comp = safeCompositionLookup(impl_->compositionId)) {
      togglePropertyKeyframeAtCurrentTime(comp, layer, row.propertyPath, currentTime);
      auto* service = ArtifactProjectService::instance();
      if (service) {
       service->selectLayer(layer->id());
      }
      impl_->selectedLayerId = layer->id();
      impl_->currentPropertyPath = row.propertyPath.trimmed();
      propertyFocusChanged(impl_->selectedLayerId, impl_->currentPropertyPath);
      update();
     }
     event->accept();
     return;
    }
    auto* service = ArtifactProjectService::instance();
    if (service) {
     service->selectLayer(layer->id());
    }
    impl_->selectedLayerId = layer->id();
    impl_->currentPropertyPath = row.propertyPath.trimmed();
    propertyFocusChanged(impl_->selectedLayerId, impl_->currentPropertyPath);
    update();
   }
   event->accept();
   return;
  }
  auto* service = ArtifactProjectService::instance();
  if (row.kind != RowKind::Layer) {
    impl_->clearDragState();
    if (event->button() == Qt::LeftButton) {
      if (service) {
       service->selectLayer(layer->id());
      }
      impl_->selectedLayerId = layer->id();
      impl_->currentPropertyPath.clear();
      propertyFocusChanged(impl_->selectedLayerId, impl_->currentPropertyPath);
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

  const int y = impl_->rowViewportY(idx);
  const int nameStartX = colW * kLayerPropertyColumnCount;
  const int nameX = nameStartX + row.depth * 14;
  const bool showInlineCombos =
      row.kind == RowKind::Layer &&
      (width() - (nameX + 8)) >= (kInlineComboReserve + kLayerNameMinWidth);
  const int parentRectX = width() - kInlineComboReserve;
  const QRect parentRect(parentRectX, y + kInlineComboMarginY, kInlineParentWidth, kInlineComboHeight);
  const QRect blendRect(parentRect.right() + kInlineComboGap, y + kInlineComboMarginY, kInlineBlendWidth, kInlineComboHeight);
  const bool clickInInlineCombo = showInlineCombos &&
                                  (parentRect.contains(event->pos()) || blendRect.contains(event->pos()));

  if (event->button() == Qt::LeftButton) {
    impl_->selectedLayerId = layer->id();
    impl_->currentPropertyPath.clear();
    propertyFocusChanged(impl_->selectedLayerId, impl_->currentPropertyPath);
    if (!clickInInlineCombo) {
      impl_->clearInlineEditors();
    }
    if (showInlineCombos && parentRect.contains(event->pos())) {
      impl_->clearInlineEditors();
      auto* combo = new QComboBox(this);
      combo->setGeometry(parentRect);
      applyLayerPanelComboPalette(combo);
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
      applyLayerPanelComboPalette(combo);
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
        impl_->eventBus_.post<LayerChangedEvent>(LayerChangedEvent{
            impl_->compositionId.toString(), layer->id().toString(),
            LayerChangedEvent::ChangeType::Modified});
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
      const QRect toggleRect(toggleX, impl_->rowViewportY(idx) + (rowH - toggleSize) / 2, toggleSize, toggleSize);
      if (row.hasChildren && toggleRect.contains(event->pos())) {
        const QString idStr = layer->id().toString();
        impl_->expandedByLayerId[idStr] = !impl_->expandedByLayerId.value(idStr, true);
        updateLayout();
        event->accept();
        return;
      }
      
      const auto variants = layer->getVariants();
      int variantAreaW = variants.empty() ? 0 : (variants.size() * 22 + 20);
      int vx = width() - (showInlineCombos ? kInlineComboReserve : 0) - variantAreaW - 6;
      if (!variants.empty() && clickX >= vx && clickX < vx + variantAreaW) {
          int clickedIdx = (clickX - vx) / 22;
          if (clickedIdx >= 0 && clickedIdx < variants.size()) {
              auto* cmd = new ChangeActiveVariantCommand(layer, layer->getActiveVariantIndex(), clickedIdx);
              UndoManager::instance()->push(std::unique_ptr<ChangeActiveVariantCommand>(cmd));
              update();
              event->accept();
              return;
          } else if (clickX >= vx + variants.size() * 22) {
              auto* cmd = new CreateVariantCommand(layer, std::string(1, (char)('A' + layer->getVariants().size())));
              UndoManager::instance()->push(std::unique_ptr<CreateVariantCommand>(cmd));
              update();
              event->accept();
              return;
          }
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
    
    // Variant Context Menu
    const int nameStartX = colW * kLayerPropertyColumnCount;
    const int nameX = nameStartX + row.depth * 14;
    const bool showInlineCombos = width() - (nameX + 8) >= (kInlineComboReserve + kLayerNameMinWidth);
    const auto variants = layer->getVariants();
    int variantAreaW = variants.empty() ? 0 : (variants.size() * 22 + 20);
    int vx = width() - (showInlineCombos ? kInlineComboReserve : 0) - variantAreaW - 6;

    if (!variants.empty() && clickX >= vx && clickX < vx + variantAreaW) {
        QMenu menu(this);
        menu.addAction("Create Variant B from A", [this, layer]() {
            auto* cmd = new CreateVariantCommand(layer, "B");
            UndoManager::instance()->push(std::unique_ptr<CreateVariantCommand>(cmd));
            update();
        });
        menu.addAction("Create Variant C from current", [this, layer]() {
            auto* cmd = new CreateVariantCommand(layer, std::string(1, (char)('A' + layer->getVariants().size())));
            UndoManager::instance()->push(std::unique_ptr<CreateVariantCommand>(cmd));
            update();
        });
        menu.exec(event->globalPos());
        event->accept();
        return;
    }

    event->accept();
    return;
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
  const int idx = impl_->rowIndexFromViewportY(event->pos().y());
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
   if (row.kind != RowKind::Layer) {
   QWidget::mouseDoubleClickEvent(event);
   return;
  }

   if (row.hasChildren) {
    const int nameStartX = colW * kLayerPropertyColumnCount;
    const int nameX = nameStartX + row.depth * 14;
    const QRect treeHitRect(nameX, impl_->rowViewportY(idx), std::max(40, width() - nameX), rowH);
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
  const QRect editRect(nameX + 2, impl_->rowViewportY(idx) + 2, nameWidth, rowH - 4);

  if (!editRect.contains(event->pos())) {
   QWidget::mouseDoubleClickEvent(event);
   return;
  }

  impl_->clearInlineEditors();
  auto* editor = new QLineEdit(layer->layerName(), this);
  editor->setGeometry(editRect);
  applyLayerPanelLineEditPalette(editor);
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
    }
    if (impl_->dragStarted_) {
      impl_->dragInsertVisibleRow = impl_->insertionVisibleRowForY(static_cast<int>(event->pos().y() + impl_->verticalOffset));
      setCursor(Qt::DragMoveCursor);
      update();
      event->accept();
      return;
    }
  }

  int idx = impl_->rowIndexFromViewportY(event->pos().y());
  if (idx != impl_->hoveredLayerIndex) {
    const int previousHoveredIndex = impl_->hoveredLayerIndex;
    impl_->hoveredLayerIndex = idx;
    if (previousHoveredIndex >= 0 && previousHoveredIndex < impl_->visibleRows.size()) {
      update(0, impl_->rowViewportY(previousHoveredIndex), width(), kLayerRowHeight);
    }
    if (idx >= 0 && idx < impl_->visibleRows.size()) {
      update(0, impl_->rowViewportY(idx), width(), kLayerRowHeight);
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
      const QRect toggleRect(toggleX, impl_->rowViewportY(idx) + (kLayerRowHeight - toggleSize) / 2, toggleSize, toggleSize);
      pointer = toggleRect.contains(event->pos());
    }
    if (!pointer && row.kind == RowKind::Property) {
      const QRect keyframeRect =
          propertyKeyframeMarkerRect(width(), impl_->rowViewportY(idx), kLayerRowHeight);
      pointer = keyframeRect.contains(event->pos());
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
     if (row.kind == RowKind::Layer && row.layer) {
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
            auto layer = comp->layerById(dragLayerId);
            auto* cmd = new MoveLayerIndexCommand(comp, layer, oldIndex, newIndex);
            UndoManager::instance()->push(std::unique_ptr<MoveLayerIndexCommand>(cmd));
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
    const int y = impl_->rowViewportY(selectedIdx) + kLayerRowHeight / 2;
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
   if (impl_->visibleRows.isEmpty()) {
    QWidget::wheelEvent(event);
    return;
   }

   int delta = 0;
   if (!event->pixelDelta().isNull()) {
    delta = event->pixelDelta().y();
   } else {
    delta = (event->angleDelta().y() / 120) * impl_->rowHeight;
    if (delta == 0) {
     delta = event->angleDelta().y() / 6;
    }
   }
   if (delta == 0) {
    QWidget::wheelEvent(event);
    return;
   }

   impl_->setVerticalOffset(impl_->verticalOffset - static_cast<double>(delta), this);
   event->accept();
}

void ArtifactLayerPanelWidget::leaveEvent(QEvent*)
{
  const int previousHoveredIndex = impl_->hoveredLayerIndex;
  impl_->hoveredLayerIndex = -1;
  if (previousHoveredIndex >= 0 && previousHoveredIndex < impl_->visibleRows.size()) {
    update(0, impl_->rowViewportY(previousHoveredIndex), width(), kLayerRowHeight);
  }
}

void ArtifactLayerPanelWidget::paintEvent(QPaintEvent* event)
{
  ArtifactCore::ProfileTimer _profTimer("LayerPanelPaint",
                                        ArtifactCore::ProfileCategory::UI);
  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing, true);
  p.setRenderHint(QPainter::SmoothPixmapTransform);
  const int rowH = kLayerRowHeight;
  const int colW = kLayerColumnWidth;
  const int iconSize = 16;
  const int offset = (colW - iconSize) / 2;
  const int nameStartX = colW * kLayerPropertyColumnCount;
  const int indent = 14;
  const int toggleSize = 10;
  
  const QRect dirtyRect = event->rect();
  const auto& theme = ArtifactCore::currentDCCTheme();
  const QColor background = themeColor(theme.secondaryBackgroundColor, QColor(QStringLiteral("#2A2A2A")));
  const QColor surface = themeColor(theme.backgroundColor, QColor(QStringLiteral("#25272D")));
  const QColor text = themeColor(theme.textColor, QColor(QStringLiteral("#DADADA")));
  const QColor accent = themeColor(theme.accentColor, QColor(QStringLiteral("#E4B76C")));
  const QColor selection = themeColor(theme.selectionColor, QColor(QStringLiteral("#4A515C")));
  const QColor border = themeColor(theme.borderColor, QColor(QStringLiteral("#1A1A1A")));

  p.fillRect(dirtyRect, background);

  RationalTime currentTime = impl_->currentTime;
  if (auto comp = safeCompositionLookup(impl_->compositionId)) {
    const auto fps = std::max(1.0, static_cast<double>(comp->frameRate().framerate()));
    currentTime = RationalTime(comp->framePosition().framePosition(), fps);
  }

  if (impl_->visibleRows.isEmpty()) {
    auto comp = safeCompositionLookup(impl_->compositionId);
    if (!comp) {
      p.setPen(text.darker(120));
      p.drawText(rect(), Qt::AlignCenter, "No composition selected");
      return;
    }
    p.setPen(text.darker(120));
    p.drawText(rect(), Qt::AlignCenter, "No layers");
    return;
  }

  p.save();
  p.translate(0.0, -impl_->verticalOffset);

  // 可視範囲のみループを回す（仮想化）
  const int startRow = std::max(0, static_cast<int>(std::floor((dirtyRect.top() + impl_->verticalOffset) / rowH)));
  const int endRow = std::min(static_cast<int>(impl_->visibleRows.size() - 1),
                              static_cast<int>(std::floor((dirtyRect.bottom() + impl_->verticalOffset) / rowH)));

  for (int i = startRow; i <= endRow; ++i) {
    int y = i * rowH;
    const auto& row = impl_->visibleRows[i];
    auto l = row.layer;
    if (!l) continue;
    const bool isGroupRow = (row.kind == RowKind::Group);
    const bool isPropertyRow = (row.kind == RowKind::Property);
    const bool sel = (l->id() == impl_->selectedLayerId);
    const bool layerSelected = sel && row.kind == RowKind::Layer;
    const QString propertyPath =
        (isPropertyRow && !row.propertyPath.trimmed().isEmpty())
            ? row.propertyPath
            : impl_->currentPropertyPath;
    const bool propertyFocused =
        isPropertyRow && propertyPath.compare(impl_->currentPropertyPath, Qt::CaseInsensitive) == 0;
    const auto property = isPropertyRow ? l->getProperty(row.propertyPath) : nullptr;
    const bool propertyAnimatable = property && property->isAnimatable();
    const bool propertyKeyframed = propertyAnimatable && property->hasKeyFrameAt(currentTime);

    const QColor rowBase = (i % 2 == 0) ? background : mixColor(background, surface, 0.25);
    const QColor rowHover = mixColor(background, text, 0.08);
    const QColor rowSelected = mixColor(background, accent, 0.34);
    if (propertyFocused) {
      p.fillRect(0, y, width(), rowH, mixColor(background, selection, 0.32));
    } else if (layerSelected) {
      p.fillRect(0, y, width(), rowH, rowSelected); // Modo-like Amber selection
    }
    else if (i == impl_->hoveredLayerIndex) p.fillRect(0, y, width(), rowH, rowHover); // Subtle grey hover
    else p.fillRect(0, y, width(), rowH, rowBase);

    p.setPen(border.darker(120));
    p.drawLine(0, y + rowH, width(), y + rowH);

    if (isGroupRow) {
      const int toggleX = nameStartX + row.depth * indent + 2;
      const int textX = row.hasChildren ? (toggleX + toggleSize + 6) : (nameStartX + row.depth * indent + 4);
      if (row.hasChildren) {
        const int toggleY = y + (rowH - toggleSize) / 2;
        QPolygonF tri;
        if (row.expanded) {
          tri << QPointF(toggleX + 0.5, toggleY + 2.0)
              << QPointF(toggleX + toggleSize - 0.5, toggleY + 2.0)
              << QPointF(toggleX + toggleSize / 2.0, toggleY + toggleSize - 1.0);
        } else {
          tri << QPointF(toggleX + 2.0, toggleY + 0.5)
              << QPointF(toggleX + 2.0, toggleY + toggleSize - 0.5)
              << QPointF(toggleX + toggleSize - 1.0, toggleY + toggleSize / 2.0);
        }
        p.setPen(Qt::NoPen);
        p.setBrush(text.darker(25));
        p.drawPolygon(tri);
      }
      p.setPen(text);
      p.drawText(textX, y, std::max(20, width() - textX - 8), rowH, Qt::AlignVCenter | Qt::AlignLeft, row.label);
      continue;
    }

    int curX = 0;
    if (!isPropertyRow) {
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
    } else {
      curX = colW * kLayerPropertyColumnCount;
    }

    // Name
    const int nameX = nameStartX + row.depth * indent;
    if ((row.kind == RowKind::Layer || row.kind == RowKind::Group) && row.hasChildren) {
      const int tx = nameX + 2;
      const int ty = y + (rowH - toggleSize) / 2;
      QPolygonF tri;
      if (row.expanded) {
        tri << QPointF(tx + 0.5, ty + 2.0)
            << QPointF(tx + toggleSize - 0.5, ty + 2.0)
            << QPointF(tx + toggleSize / 2.0, ty + toggleSize - 1.0);
      } else {
        tri << QPointF(tx + 2.0, ty + 0.5)
            << QPointF(tx + 2.0, ty + toggleSize - 0.5)
            << QPointF(tx + toggleSize - 1.0, ty + toggleSize / 2.0);
      }
      p.setPen(Qt::NoPen);
      p.setBrush(text.darker(28));
      p.drawPolygon(tri);
    }

    p.setPen(text);
    const int textX = nameX + ((row.hasChildren && (row.kind == RowKind::Layer || row.kind == RowKind::Group)) ? 16 : 4);
    const bool showInlineCombos = row.kind == RowKind::Layer &&
                                  (width() - (nameX + 8)) >= (kInlineComboReserve + kLayerNameMinWidth);
    const int parentRectX = width() - kInlineComboReserve;
    const QRect parentRect(parentRectX, y + kInlineComboMarginY, kInlineParentWidth, kInlineComboHeight);
    const QRect blendRect(parentRect.right() + kInlineComboGap, y + kInlineComboMarginY, kInlineBlendWidth, kInlineComboHeight);

    auto drawInlineCombo = [&](const QRect& r, const QString& label) {
     p.setPen(layerSelected ? accent.darker(180) : border);
     p.setBrush(layerSelected ? mixColor(background, accent, 0.22) : mixColor(background, surface, 0.28));
     p.drawRoundedRect(r, 3, 3);
     p.setPen(layerSelected ? mixColor(text, accent, 0.42) : text);
     p.drawText(r.adjusted(6, 0, -16, 0), Qt::AlignVCenter | Qt::AlignLeft, p.fontMetrics().elidedText(label, Qt::ElideRight, r.width() - 20));
     QPolygon arrow;
     const int ax = r.right() - 10;
     const int ay = r.center().y();
     arrow << QPoint(ax - 4, ay - 2) << QPoint(ax + 4, ay - 2) << QPoint(ax, ay + 3);
     p.setBrush(layerSelected ? accent.lighter(120) : text.darker(25));
     p.setPen(Qt::NoPen);
     p.drawPolygon(arrow);
    };

    if (showInlineCombos) {
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
     drawInlineCombo(parentRect, QStringLiteral("Parent: %1").arg(parentName));
     drawInlineCombo(blendRect, QStringLiteral("Blend: %1").arg(blendModeToText(l->layerBlendType())));
    }
    if (isPropertyRow) {
     const QRect keyframeRect = propertyKeyframeMarkerRect(width(), y, rowH);
     const int textWidth = std::max(20, keyframeRect.left() - textX - 10);
     p.setPen(propertyFocused ? accent.lighter(130)
                              : (layerSelected ? text.lighter(112) : text));
     p.drawText(textX + 4, y, textWidth, rowH, Qt::AlignVCenter | Qt::AlignLeft, row.label);
     if (propertyAnimatable) {
      const QRectF marker = QRectF(keyframeRect).adjusted(2.0, 2.0, -2.0, -2.0);
      QPolygonF diamond;
      diamond << QPointF(marker.center().x(), marker.top())
              << QPointF(marker.right(), marker.center().y())
              << QPointF(marker.center().x(), marker.bottom())
              << QPointF(marker.left(), marker.center().y());
      p.setPen(QPen(propertyKeyframed ? accent.lighter(105) : text.darker(145), 1.4));
      p.setBrush(propertyKeyframed ? accent.lighter(105) : Qt::NoBrush);
      p.drawPolygon(diamond);
     }
    } else {
     const auto variants = l->getVariants();
     const int activeIdx = static_cast<int>(l->getActiveVariantIndex());
     const int variantAreaW = variants.empty() ? 0 : (variants.size() * 22 + 20);
     const int textWidth = std::max(20, width() - textX - 8 - (showInlineCombos ? kInlineComboReserve : 0) - variantAreaW);
     p.setPen(text);
     p.drawText(textX + 4, y, textWidth, rowH, Qt::AlignVCenter | Qt::AlignLeft, l->layerName());

     if (!variants.empty()) {
       int vx = width() - (showInlineCombos ? kInlineComboReserve : 0) - variantAreaW - 6;
       p.save();
       p.setFont(QFont("Inter", 8, QFont::Bold));
       for (size_t vIdx = 0; vIdx < variants.size(); ++vIdx) {
           QRect vRect(vx, y + (rowH - 16)/2, 18, 16);
           if (static_cast<int>(vIdx) == activeIdx) {
               p.setBrush(accent);
               p.setPen(Qt::NoPen);
               p.drawRoundedRect(vRect, 3, 3);
               p.setPen(background); // dark text
           } else {
               p.setBrush(surface);
               p.setPen(border);
               p.drawRoundedRect(vRect, 3, 3);
               p.setPen(text.darker(150));
           }
           const auto n = variants[vIdx]->GetName();
           p.drawText(vRect, Qt::AlignCenter, QString::fromStdString(n.empty() ? "V" : n.substr(0, 1)));
           vx += 22;
       }
       QRect plusRect(vx, y + (rowH - 16)/2, 18, 16);
       p.setBrush(surface);
       p.setPen(border);
       p.drawRoundedRect(plusRect, 3, 3);
       p.setPen(text);
       p.drawText(plusRect, Qt::AlignCenter, "+");
       p.restore();
     }
    }
   }

    if (!impl_->draggedLayerId.isNil() && impl_->dragInsertVisibleRow >= 0) {
     const int lineY = std::clamp(static_cast<int>(std::floor(impl_->dragInsertVisibleRow * rowH - impl_->verticalOffset)), 1, std::max(1, height() - 2));
    QPen pen(accent, 2);
    p.setPen(pen);
    p.drawLine(0, lineY, width(), lineY);

    p.setPen(Qt::NoPen);
    p.setBrush(accent);
    const int markerSize = 6;
    p.drawEllipse(QPoint(markerSize, lineY), markerSize / 2, markerSize / 2);
    p.drawEllipse(QPoint(std::max(markerSize, width() - markerSize), lineY), markerSize / 2, markerSize / 2);
   }

  p.restore();

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
    impl_->dragInsertVisibleRow = impl_->insertionVisibleRowForY(static_cast<int>(e->position().y() + impl_->verticalOffset));
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
        if (row.kind == RowKind::Layer && row.layer) {
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
          auto layer = comp->layerById(dragLayerId);
          auto* cmd = new MoveLayerIndexCommand(comp, layer, oldIndex, newIndex);
          UndoManager::instance()->push(std::unique_ptr<MoveLayerIndexCommand>(cmd));
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
  ArtifactLayerPanelHeaderWidget* header = nullptr;
  ArtifactLayerPanelWidget* panel = nullptr;
  CompositionID id;
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
  impl_->panel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

  auto* wheelFilter = new LayerPanelWheelFilter(impl_->panel, this);
  this->installEventFilter(wheelFilter);
  impl_->header->installEventFilter(wheelFilter);

  layout->addWidget(impl_->header);
  layout->addWidget(impl_->panel, 1);

  QObject::connect(impl_->header, &ArtifactLayerPanelHeaderWidget::shyToggled,
                   impl_->panel, &ArtifactLayerPanelWidget::setShyHidden);
  QObject::connect(impl_->panel, &ArtifactLayerPanelWidget::visibleRowsChanged,
                   this, [this]() {
                     this->visibleRowsChanged();
                   });
  QObject::connect(impl_->panel, &ArtifactLayerPanelWidget::verticalOffsetChanged,
                   this, [this](double offset) {
                      this->verticalOffsetChanged(offset);
                    });
  QObject::connect(
      impl_->panel, &ArtifactLayerPanelWidget::propertyFocusChanged, this,
      [this](const LayerID& layerId, const QString& propertyPath) {
        this->propertyFocusChanged(layerId, propertyPath);
      });
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

  void ArtifactLayerTimelinePanelWrapper::setVerticalOffset(double offset)
  {
   if (impl_ && impl_->panel) {
    impl_->panel->setVerticalOffset(offset);
   }
  }

  double ArtifactLayerTimelinePanelWrapper::verticalOffset() const
  {
   return impl_ && impl_->panel ? impl_->panel->verticalOffset() : 0.0;
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

 QVector<LayerID> ArtifactLayerTimelinePanelWrapper::visibleTimelineRows() const
 {
  if (!impl_ || !impl_->panel) {
   return {};
  }
  return impl_->panel->visibleTimelineRows();
 }

 QVector<TimelineRowDescriptor>
 ArtifactLayerTimelinePanelWrapper::visibleTimelineRowDescriptors() const
 {
  if (!impl_ || !impl_->panel) {
   return {};
  }
  return impl_->panel->visibleTimelineRowDescriptors();
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

  LayerID ArtifactLayerTimelinePanelWrapper::selectedLayerId() const
  {
   return impl_ && impl_->panel ? impl_->panel->selectedLayerId() : LayerID{};
  }

  QString ArtifactLayerTimelinePanelWrapper::currentPropertyPath() const
  {
   return impl_ && impl_->panel ? impl_->panel->currentPropertyPath() : QString{};
  }

} // namespace Artifact
