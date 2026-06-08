module;
#include <wobjectimpl.h>
#include <QApplication>
#include <QCoreApplication>
#include <QAction>
#include <QPainter>
#include <QFontMetrics>
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
#include <QStringList>
#include <QPolygon>
#include <QIcon>
#include <QtSVG/QSvgRenderer>
#include <QComboBox>
#include <QAbstractItemView>
#include <QMessageBox>
#include <QPointer>
#include <QLineEdit>
#include <QKeyEvent>
#include <QFocusEvent>
#include <QWheelEvent>
#include <QInputDialog>
#include <QFileDialog>
#include <QDesktopServices>
#include <QToolTip>
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
import Artifact.Layers.Selection.Manager;
import Artifact.Composition.Abstract;
import Artifact.Layer.Abstract;
import Artifact.Layer.Image;
import Artifact.Layer.Text;
import Artifact.Layer.Shape;
import Artifact.Mask.LayerMask;
import Artifact.Mask.Path;
import Artifact.Layer.Svg;
import Artifact.Layer.Video;
import Artifact.Layer.Audio;
import Artifact.Layer.Camera;
import Artifact.Layer.Light;
import Artifact.Layer.Particle;
import Artifact.Layer.Composition;
import Artifact.Layer.Solid2D;
import Artifact.Layer.Construction;
import Artifact.Layer.Clone;
import Layer.Matte;
import Artifact.Timeline.KeyframeModel;
import Undo.UndoManager;
import Artifact.Service.Playback;
import Layer.Blend;
import Artifact.Layer.InitParams;
import File.TypeDetector;
import Event.Bus;
import Artifact.Event.Types;
import Input.Operator;
import Widgets.Utils.CSS;

namespace Artifact
{
 using namespace ArtifactCore;

namespace {
constexpr auto kLayerPanelContext = "Panel.LayerTree";
}

LayerPresentationDescriptor describeLayerPresentation(const ArtifactAbstractLayerPtr& layer)
{
  LayerPresentationDescriptor descriptor;
  descriptor.typeText = QStringLiteral("Layer");
  descriptor.timelineBadgeText = QStringLiteral("Layer");
  descriptor.propertySummaryTitle = QStringLiteral("Summary");
  descriptor.inspectorTypeLabel = QStringLiteral("Type: N/A");
  descriptor.capabilitySummaryText = QString();
  descriptor.badgeTone = LayerPresentationBadgeTone::Neutral;

  if (!layer) {
    return descriptor;
  }
  if (layer->isNullLayer()) {
    descriptor.typeText = QStringLiteral("Null Layer");
    descriptor.timelineBadgeText = QStringLiteral("Null");
    descriptor.propertySummaryTitle = QStringLiteral("Summary · Null Layer");
    descriptor.inspectorTypeLabel = QStringLiteral("Type: Null Layer");
    descriptor.capabilitySummaryText = QStringLiteral("Specialized");
    descriptor.badgeTone = LayerPresentationBadgeTone::Special;
    return descriptor;
  }
  if (layer->isAdjustmentLayer()) {
    descriptor.typeText = QStringLiteral("Adjustment Layer");
    descriptor.timelineBadgeText = QStringLiteral("Adjust");
    descriptor.propertySummaryTitle = QStringLiteral("Summary · Adjustment Layer");
    descriptor.inspectorTypeLabel = QStringLiteral("Type: Adjustment Layer");
    descriptor.capabilitySummaryText = QStringLiteral("Specialized");
    descriptor.badgeTone = LayerPresentationBadgeTone::Motion;
    return descriptor;
  }
  if (layer->isGroupLayer()) {
    descriptor.typeText = QStringLiteral("Group Layer");
    descriptor.timelineBadgeText = QStringLiteral("Group");
    descriptor.propertySummaryTitle = QStringLiteral("Summary · Group Layer");
    descriptor.inspectorTypeLabel = QStringLiteral("Type: Group Layer");
    descriptor.capabilitySummaryText = QStringLiteral("Container");
    descriptor.badgeTone = LayerPresentationBadgeTone::Container;
    return descriptor;
  }
  if (layer->isCloneLayer()) {
    descriptor.typeText = QStringLiteral("Clone Layer");
    descriptor.timelineBadgeText = QStringLiteral("Clone");
    descriptor.propertySummaryTitle = QStringLiteral("Summary · Clone Layer");
    descriptor.inspectorTypeLabel = QStringLiteral("Type: Clone Layer");
    descriptor.capabilitySummaryText = QStringLiteral("Instanced");
    descriptor.badgeTone = LayerPresentationBadgeTone::Special;
    return descriptor;
  }
  if (layer->isConstructionLayer()) {
    descriptor.typeText = QStringLiteral("Construction Layer");
    descriptor.timelineBadgeText = QStringLiteral("Const");
    descriptor.propertySummaryTitle = QStringLiteral("Summary · Construction Layer");
    descriptor.inspectorTypeLabel = QStringLiteral("Type: Construction Layer");
    descriptor.capabilitySummaryText = QStringLiteral("Renderless");
    descriptor.badgeTone = LayerPresentationBadgeTone::Special;
    return descriptor;
  }
  if (layer->is3D()) {
    descriptor.typeText = QStringLiteral("3D Model Layer");
    descriptor.timelineBadgeText = QStringLiteral("3D");
    descriptor.propertySummaryTitle = QStringLiteral("Summary · 3D Model Layer");
    descriptor.inspectorTypeLabel = QStringLiteral("Type: 3D Model Layer");
    descriptor.capabilitySummaryText = QStringLiteral("3D Space");
    descriptor.badgeTone = LayerPresentationBadgeTone::Motion;
    return descriptor;
  }
  if (dynamic_cast<ArtifactCompositionLayer *>(layer.get())) {
    descriptor.typeText = QStringLiteral("Precomp Layer");
    descriptor.timelineBadgeText = QStringLiteral("Precomp");
    descriptor.propertySummaryTitle = QStringLiteral("Summary · Precomp Layer");
    descriptor.inspectorTypeLabel = QStringLiteral("Type: Precomp Layer");
    descriptor.capabilitySummaryText = QStringLiteral("Nested Composition");
    descriptor.badgeTone = LayerPresentationBadgeTone::Container;
    return descriptor;
  }
  if (layer->hasAudio() && layer->hasVideo()) {
    descriptor.typeText = QStringLiteral("Audio-Video Layer");
    descriptor.timelineBadgeText = QStringLiteral("A/V");
    descriptor.propertySummaryTitle = QStringLiteral("Summary · Audio-Video Layer");
    descriptor.inspectorTypeLabel = QStringLiteral("Type: Audio-Video Layer");
    descriptor.capabilitySummaryText = QStringLiteral("Audio + Video");
    descriptor.badgeTone = LayerPresentationBadgeTone::Media;
    return descriptor;
  }
  if (layer->hasAudio()) {
    descriptor.typeText = QStringLiteral("Audio Layer");
    descriptor.timelineBadgeText = QStringLiteral("Audio");
    descriptor.propertySummaryTitle = QStringLiteral("Summary · Audio Layer · Waveform Preview");
    descriptor.inspectorTypeLabel = QStringLiteral("Type: Audio Layer");
    descriptor.capabilitySummaryText = QStringLiteral("Waveform preview");
    descriptor.badgeTone = LayerPresentationBadgeTone::Media;
    return descriptor;
  }
  if (layer->hasVideo()) {
    descriptor.typeText = QStringLiteral("Video Layer");
    descriptor.timelineBadgeText = QStringLiteral("Video");
    descriptor.propertySummaryTitle = QStringLiteral("Summary · Video Layer");
    descriptor.inspectorTypeLabel = QStringLiteral("Type: Video Layer");
    descriptor.capabilitySummaryText = QStringLiteral("Video");
    descriptor.badgeTone = LayerPresentationBadgeTone::Media;
    return descriptor;
  }
  return descriptor;
}

QString describeLayerType(const ArtifactAbstractLayerPtr& layer)
{
  return describeLayerPresentation(layer).typeText;
}
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

  QColor toneBadgeFill(LayerPresentationBadgeTone tone,
                       const QColor& background,
                       const QColor& surface,
                       const QColor& accent)
  {
    switch (tone) {
    case LayerPresentationBadgeTone::Container:
      return mixColor(background, accent, 0.22);
    case LayerPresentationBadgeTone::Media:
      return mixColor(background, surface, 0.34);
    case LayerPresentationBadgeTone::Motion:
      return mixColor(background, accent, 0.16);
    case LayerPresentationBadgeTone::Special:
      return mixColor(background, accent, 0.30);
    case LayerPresentationBadgeTone::Neutral:
    default:
      return mixColor(background, surface, 0.28);
    }
  }

  QColor toneBadgeText(LayerPresentationBadgeTone tone,
                       const QColor& text,
                       const QColor& accent)
  {
    switch (tone) {
    case LayerPresentationBadgeTone::Container:
      return mixColor(text, accent, 0.28);
    case LayerPresentationBadgeTone::Media:
      return text.darker(115);
    case LayerPresentationBadgeTone::Motion:
      return mixColor(text, accent, 0.34);
    case LayerPresentationBadgeTone::Special:
      return mixColor(text, accent, 0.22);
    case LayerPresentationBadgeTone::Neutral:
    default:
      return text.darker(120);
    }
  }

constexpr int kVariantChipWidth = 72;
constexpr int kVariantChipHeight = 16;
constexpr int kLayerTypeIconSize = 14;
constexpr int kLayerTypeIconGap = 5;

enum class TimelineLayerIconKind {
  Generic,
  Solid,
  Image,
  Svg,
  Video,
  Audio,
  Text,
  Shape,
  Precomp,
  Camera,
  Light,
  Group,
  Null,
  Adjustment,
  Particle,
  Clone,
  Model3D,
  Construction
};

TimelineLayerIconKind layerIconKindForLayer(const ArtifactAbstractLayerPtr& layer)
{
  if (!layer) return TimelineLayerIconKind::Generic;
  if (layer->isAdjustmentLayer()) return TimelineLayerIconKind::Adjustment;
  if (layer->isGroupLayer()) return TimelineLayerIconKind::Group;
  if (layer->isCloneLayer()) return TimelineLayerIconKind::Clone;
  if (layer->isConstructionLayer()) return TimelineLayerIconKind::Construction;
  if (layer->is3D()) return TimelineLayerIconKind::Model3D;
  if (dynamic_cast<ArtifactCompositionLayer*>(layer.get())) return TimelineLayerIconKind::Precomp;
  if (std::dynamic_pointer_cast<ArtifactTextLayer>(layer)) return TimelineLayerIconKind::Text;
  if (std::dynamic_pointer_cast<ArtifactShapeLayer>(layer)) return TimelineLayerIconKind::Shape;
  if (std::dynamic_pointer_cast<ArtifactSvgLayer>(layer)) return TimelineLayerIconKind::Svg;
  if (std::dynamic_pointer_cast<ArtifactImageLayer>(layer)) return TimelineLayerIconKind::Image;
  if (std::dynamic_pointer_cast<ArtifactAudioLayer>(layer)) return TimelineLayerIconKind::Audio;
  if (std::dynamic_pointer_cast<ArtifactVideoLayer>(layer)) {
    if (layer->hasAudio() && !layer->hasVideo()) return TimelineLayerIconKind::Audio;
    return TimelineLayerIconKind::Video;
  }
  if (std::dynamic_pointer_cast<ArtifactCameraLayer>(layer)) return TimelineLayerIconKind::Camera;
  if (std::dynamic_pointer_cast<ArtifactLightLayer>(layer)) return TimelineLayerIconKind::Light;
  if (std::dynamic_pointer_cast<ArtifactParticleLayer>(layer)) return TimelineLayerIconKind::Particle;
  if (std::dynamic_pointer_cast<ArtifactSolid2DLayer>(layer)) return TimelineLayerIconKind::Solid;
  if (layer->isNullLayer()) return TimelineLayerIconKind::Null;
  if (layer->hasAudio() && !layer->hasVideo()) return TimelineLayerIconKind::Audio;
  return TimelineLayerIconKind::Solid;
}

  QString variantNameForIndex(int index)
  {
    if (index < 0) {
      return QStringLiteral("A");
    }

    QString name;
    int value = index + 1;
    while (value > 0) {
      --value;
      name.prepend(QChar(static_cast<ushort>('A' + (value % 26))));
      value /= 26;
    }
    return name;
  }

  QString variantDisplayName(const ArtifactAbstractLayerPtr& layer, int index)
  {
    const auto variants = layer ? layer->getVariants() : std::vector<LayerVariant*>{};
    if (index >= 0 && index < variants.size() && variants[index]) {
      const QString name = QString::fromStdString(variants[index]->GetName()).trimmed();
      if (!name.isEmpty()) {
        return name;
      }
    }
    return variantNameForIndex(index);
  }

  QString nextVariantName(const ArtifactAbstractLayerPtr& layer)
  {
    QSet<QString> used;
    if (layer) {
      const auto variants = layer->getVariants();
      for (int i = 0; i < variants.size(); ++i) {
        used.insert(variantDisplayName(layer, i));
      }
    }

    for (int i = 0; i < 1024; ++i) {
      const QString candidate = variantNameForIndex(i);
      if (!used.contains(candidate)) {
        return candidate;
      }
    }

    return QStringLiteral("Variant %1").arg(used.size() + 1);
  }

  QString variantChipText(const ArtifactAbstractLayerPtr& layer)
  {
    if (!layer) {
      return QStringLiteral("Variant");
    }

    const auto variants = layer->getVariants();
    const int activeIdx = static_cast<int>(layer->getActiveVariantIndex());
    if (activeIdx >= 0 && activeIdx < variants.size()) {
      return variantDisplayName(layer, activeIdx);
    }
    return QStringLiteral("Variant");
  }

  QRect variantChipRect(const QRect& rowRect,
                        const ArtifactAbstractLayerPtr& layer,
                        const bool showInlineCombos,
                        const int rowH)
  {
    if (!layer) {
      return {};
    }
    constexpr int kInlineComboReserveProxy = 286;
    const int chipX = rowRect.right() - (showInlineCombos ? kInlineComboReserveProxy : 0) -
                      kVariantChipWidth - 6;
    return QRect(chipX, rowRect.top() + (rowH - kVariantChipHeight) / 2,
                 kVariantChipWidth, kVariantChipHeight);
  }

  void showVariantPickerMenu(QWidget* parent,
                             const ArtifactAbstractLayerPtr& layer,
                             const QPoint& globalPos)
  {
    if (!parent || !layer) {
      return;
    }

    QMenu menu(parent);
    const auto variants = layer->getVariants();
    const int activeIdx = static_cast<int>(layer->getActiveVariantIndex());

    for (int i = 0; i < variants.size(); ++i) {
      const QString label = variantDisplayName(layer, i);
      QAction* action = menu.addAction(label, [parent, layer, i]() {
        auto* cmd = new ChangeActiveVariantCommand(layer,
                                                   layer->getActiveVariantIndex(),
                                                   static_cast<size_t>(i));
        UndoManager::instance()->push(std::unique_ptr<ChangeActiveVariantCommand>(cmd));
        parent->update();
      });
      action->setCheckable(true);
      action->setChecked(i == activeIdx);
    }

    if (!variants.empty()) {
      menu.addSeparator();
    }

    const QString newVariantName = nextVariantName(layer);
    menu.addAction(QStringLiteral("New Variant (%1)").arg(newVariantName), [parent, layer, newVariantName]() {
      auto* cmd = new CreateVariantCommand(layer, newVariantName.toStdString());
      UndoManager::instance()->push(std::unique_ptr<CreateVariantCommand>(cmd));
      parent->update();
    });

    menu.exec(globalPos);
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
  constexpr char kLayerMatteLinkMimeType[] = "application/x-artifact-layer-matte-link";

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

  QStringList collectTimelineDroppedPaths(const QMimeData* mime)
  {
    QStringList validPaths;
    if (!mime) {
      return validPaths;
    }

    if (mime->hasUrls()) {
      for (const auto& url : mime->urls()) {
        if (!url.isLocalFile()) {
          continue;
        }
        const QString filePath = url.toLocalFile();
        const QFileInfo info(filePath);
        if (!info.exists() || info.isDir()) {
          continue;
        }
        const LayerType type = inferLayerTypeFromFile(filePath);
        if (type == LayerType::Image || type == LayerType::Video ||
            type == LayerType::Audio || type == LayerType::Shape) {
          validPaths.append(filePath);
        }
      }
    }

    if (!validPaths.isEmpty() || !mime->hasText()) {
      return validPaths;
    }

    const QStringList paths =
        mime->text().split(QStringLiteral("\n"), Qt::SkipEmptyParts);
    for (const QString& path : paths) {
      const QString trimmed = path.trimmed();
      if (trimmed.isEmpty()) {
        continue;
      }
      const QFileInfo info(trimmed);
      if (!info.exists() || info.isDir()) {
        continue;
      }
      const LayerType type = inferLayerTypeFromFile(trimmed);
      if (type == LayerType::Image || type == LayerType::Video ||
          type == LayerType::Audio || type == LayerType::Shape) {
        validPaths.append(trimmed);
      }
    }

    return validPaths;
  }

  void importTimelineDroppedPaths(const QStringList& validPaths)
  {
    auto* svc = ArtifactProjectService::instance();
    if (!svc || validPaths.isEmpty()) {
      return;
    }

    svc->importAssetsFromPathsAsync(validPaths, [svc](QStringList imported) {
      if (!svc || imported.isEmpty()) {
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
        } else if (type == LayerType::Video) {
          ArtifactVideoInitParams params(QFileInfo(path).baseName());
          params.setVideoPath(path);
          svc->addLayerToCurrentComposition(params);
        } else {
          ArtifactLayerInitParams params(QFileInfo(path).baseName(), type);
          svc->addLayerToCurrentComposition(params);
        }
      }
    });
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
      {QStringLiteral("Subtract"), LAYER_BLEND_TYPE::BLEND_SUBTRACT},
      {QStringLiteral("Multiply"), LAYER_BLEND_TYPE::BLEND_MULTIPLY},
      {QStringLiteral("Screen"), LAYER_BLEND_TYPE::BLEND_SCREEN},
      {QStringLiteral("Overlay"), LAYER_BLEND_TYPE::BLEND_OVERLAY},
      {QStringLiteral("Darken"), LAYER_BLEND_TYPE::BLEND_DARKEN},
      {QStringLiteral("Lighten"), LAYER_BLEND_TYPE::BLEND_LIGHTEN},
      {QStringLiteral("Color Dodge"), LAYER_BLEND_TYPE::BLEND_COLOR_DODGE},
      {QStringLiteral("Color Burn"), LAYER_BLEND_TYPE::BLEND_COLOR_BURN},
      {QStringLiteral("Linear Burn"), LAYER_BLEND_TYPE::BLEND_LINEAR_BURN},
      {QStringLiteral("Classic Color Burn"), LAYER_BLEND_TYPE::BLEND_CLASSIC_COLOR_BURN},
      {QStringLiteral("Divide"), LAYER_BLEND_TYPE::BLEND_DIVIDE},
      {QStringLiteral("Linear Dodge"), LAYER_BLEND_TYPE::BLEND_LINEAR_DODGE},
      {QStringLiteral("Classic Color Dodge"), LAYER_BLEND_TYPE::BLEND_CLASSIC_COLOR_DODGE},
      {QStringLiteral("Hard Light"), LAYER_BLEND_TYPE::BLEND_HARD_LIGHT},
      {QStringLiteral("Soft Light"), LAYER_BLEND_TYPE::BLEND_SOFT_LIGHT},
      {QStringLiteral("Linear Light"), LAYER_BLEND_TYPE::BLEND_LINEAR_LIGHT},
      {QStringLiteral("Vivid Light"), LAYER_BLEND_TYPE::BLEND_VIVID_LIGHT},
      {QStringLiteral("Pin Light"), LAYER_BLEND_TYPE::BLEND_PIN_LIGHT},
      {QStringLiteral("Hard Mix"), LAYER_BLEND_TYPE::BLEND_HARD_MIX},
      {QStringLiteral("Difference"), LAYER_BLEND_TYPE::BLEND_DIFFERENCE},
      {QStringLiteral("Classic Difference"), LAYER_BLEND_TYPE::BLEND_CLASSIC_DIFFERENCE},
      {QStringLiteral("Exclusion"), LAYER_BLEND_TYPE::BLEND_EXCLUSION},
      {QStringLiteral("Hue"), LAYER_BLEND_TYPE::BLEND_HUE},
      {QStringLiteral("Saturation"), LAYER_BLEND_TYPE::BLEND_SATURATION},
      {QStringLiteral("Color"), LAYER_BLEND_TYPE::BLEND_COLOR},
      {QStringLiteral("Luminosity"), LAYER_BLEND_TYPE::BLEND_LUMINOSITY},
      {QStringLiteral("Dissolve"), LAYER_BLEND_TYPE::BLEND_DISSOLVE},
      {QStringLiteral("Dancing Dissolve"), LAYER_BLEND_TYPE::BLEND_DANCING_DISSOLVE},
      {QStringLiteral("Stencil Alpha"), LAYER_BLEND_TYPE::BLEND_STENCIL_ALPHA},
      {QStringLiteral("Stencil Luma"), LAYER_BLEND_TYPE::BLEND_STENCIL_LUMA},
      {QStringLiteral("Silhouette Alpha"), LAYER_BLEND_TYPE::BLEND_SILHOUETTE_ALPHA},
      {QStringLiteral("Silhouette Luma"), LAYER_BLEND_TYPE::BLEND_SILHOUETTE_LUMA}
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

  QString maskSelectionPropertyPath(const int maskIndex)
  {
   if (maskIndex < 0) {
    return QString();
   }
   return QStringLiteral("mask.%1.enabled").arg(maskIndex);
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
    visibilityIcon = loadLayerPanelPixmap(QStringLiteral("MaterialVS/neutral/visibility.svg"));
    lockIcon = loadLayerPanelPixmap(QStringLiteral("MaterialVS/yellow/lock.svg"));
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
  QIcon parentIcon;
  QIcon blendIcon;
  
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
 setAcceptDrops(true);
  impl_->visibilityIcon = loadLayerPanelPixmap(QStringLiteral("MaterialVS/neutral/visibility.svg"));
  impl_->lockIcon = loadLayerPanelPixmap(QStringLiteral("MaterialVS/yellow/lock.svg"));
  if (impl_->lockIcon.isNull()) impl_->lockIcon = loadLayerPanelPixmap(QStringLiteral("MaterialVS/yellow/lock_open.svg"), QStringLiteral("unlock.png"));
  impl_->soloIcon = loadLayerPanelPixmap(QStringLiteral("MaterialVS/purple/group.svg"), QStringLiteral("solo.png"));
  impl_->audioIcon = loadLayerPanelPixmap(QStringLiteral("MaterialVS/neutral/volume.svg"),         QStringLiteral("volume.png"));
  impl_->shyIcon = loadLayerPanelPixmap(QStringLiteral("MaterialVS/orange/visibility_off.svg"));
  impl_->parentIcon = loadLayerPanelIcon(QStringLiteral("MaterialVS/yellow/link.svg"));
  impl_->blendIcon = loadLayerPanelIcon(QStringLiteral("MaterialVS/blue/merge_type.svg"));

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
  
  auto parentHeader = impl_->parentHeaderButton = new QPushButton();
  parentHeader->setFixedWidth(kInlineParentWidth);
  parentHeader->setIcon(impl_->parentIcon);
  parentHeader->setIconSize(QSize(16, 16));
  parentHeader->setToolTip(QStringLiteral("Parent Link"));
  parentHeader->setFlat(true);
  parentHeader->setFocusPolicy(Qt::NoFocus);
  parentHeader->setAttribute(Qt::WA_TransparentForMouseEvents, true);
  applyLayerPanelButtonPalette(parentHeader);
  
  auto blendHeader = impl_->blendHeaderButton = new QPushButton();
  blendHeader->setFixedWidth(kInlineBlendWidth);
  blendHeader->setIcon(impl_->blendIcon);
  blendHeader->setIconSize(QSize(16, 16));
  blendHeader->setToolTip(QStringLiteral("Blend Mode"));
  blendHeader->setFlat(true);
  blendHeader->setFocusPolicy(Qt::NoFocus);
  blendHeader->setAttribute(Qt::WA_TransparentForMouseEvents, true);
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

void ArtifactLayerPanelHeaderWidget::dragEnterEvent(QDragEnterEvent* event)
{
  const QStringList validPaths = collectTimelineDroppedPaths(event->mimeData());
  if (!validPaths.isEmpty()) {
    event->acceptProposedAction();
    return;
  }
  event->ignore();
}

void ArtifactLayerPanelHeaderWidget::dragMoveEvent(QDragMoveEvent* event)
{
  const QStringList validPaths = collectTimelineDroppedPaths(event->mimeData());
  if (!validPaths.isEmpty()) {
    event->acceptProposedAction();
    return;
  }
  event->ignore();
}

void ArtifactLayerPanelHeaderWidget::dropEvent(QDropEvent* event)
{
  const QStringList validPaths = collectTimelineDroppedPaths(event->mimeData());
  if (validPaths.isEmpty()) {
    event->ignore();
    return;
  }
  importTimelineDroppedPaths(validPaths);
  event->acceptProposedAction();
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
 QString auxiliaryText;
 LayerPresentationBadgeTone auxiliaryTone = LayerPresentationBadgeTone::Neutral;
};

bool layerMatchesDisplayMode(const ArtifactAbstractLayerPtr& layer,
                             const TimelineLayerDisplayMode mode,
                             const LayerID& selectedLayerId)
{
 if (!layer) {
  return false;
 }
 switch (mode) {
 case TimelineLayerDisplayMode::AudioOnly:
  return layer->hasAudio();
 case TimelineLayerDisplayMode::VideoOnly:
  return layer->hasVideo();
 case TimelineLayerDisplayMode::SelectedOnly:
  return !selectedLayerId.isNil() && layer->id() == selectedLayerId;
 case TimelineLayerDisplayMode::AllLayers:
 case TimelineLayerDisplayMode::AnimatedOnly:
 case TimelineLayerDisplayMode::ImportantAndKeyframed:
 default:
  return true;
 }
}

bool propertyMatchesDisplayMode(const std::shared_ptr<ArtifactCore::AbstractProperty>& property,
                                const TimelineLayerDisplayMode mode)
{
 if (!property) {
  return false;
 }
 switch (mode) {
 case TimelineLayerDisplayMode::AnimatedOnly:
 case TimelineLayerDisplayMode::ImportantAndKeyframed:
  return property->isAnimatable() && !property->getKeyFrames().empty();
 case TimelineLayerDisplayMode::AudioOnly:
 case TimelineLayerDisplayMode::VideoOnly:
 case TimelineLayerDisplayMode::SelectedOnly:
 case TimelineLayerDisplayMode::AllLayers:
 default:
  return true;
 }
}

bool groupHasVisibleProperties(const ArtifactCore::PropertyGroup& group,
                               const TimelineLayerDisplayMode mode)
{
 for (const auto& property : group.sortedProperties()) {
  if (propertyMatchesDisplayMode(property, mode)) {
   return true;
  }
 }
 return false;
}

ArtifactLayerSelectionManager* currentLayerSelectionManager()
{
 auto* app = ArtifactApplicationManager::instance();
 return app ? app->layerSelectionManager() : nullptr;
}

QVector<LayerID> selectedLayerIdsSnapshot()
{
 QVector<LayerID> ids;
 auto* selectionManager = currentLayerSelectionManager();
 if (!selectionManager) {
  return ids;
 }

 const auto selected = selectionManager->selectedLayers();
 ids.reserve(static_cast<int>(selected.size()));
 for (const auto& layer : selected) {
  if (layer) {
   ids.push_back(layer->id());
  }
 }
 return ids;
}

bool isLayerSelectedInSelectionManager(const LayerID& id)
{
 if (id.isNil()) {
  return false;
 }
 auto* selectionManager = currentLayerSelectionManager();
 if (!selectionManager) {
  return false;
 }
 const auto selected = selectionManager->selectedLayers();
 for (const auto& layer : selected) {
  if (layer && layer->id() == id) {
   return true;
  }
 }
 return false;
}

QVector<LayerID> selectedLayerIdsInVisibleOrder(const QVector<VisibleRow>& rows)
{
 QVector<LayerID> ids;
 for (const auto& row : rows) {
  if (row.kind != RowKind::Layer || !row.layer) {
   continue;
  }
  if (isLayerSelectedInSelectionManager(row.layer->id())) {
   ids.push_back(row.layer->id());
  }
 }
 return ids;
}

QVector<LayerID> visibleLayerIdsInRange(const QVector<VisibleRow>& rows,
                                        const LayerID& from,
                                        const LayerID& to)
{
 if (from.isNil() || to.isNil()) {
  return {};
 }

 int fromIndex = -1;
 int toIndex = -1;
 for (int i = 0; i < rows.size(); ++i) {
  const auto& row = rows[i];
  if (row.kind != RowKind::Layer || !row.layer) {
   continue;
  }
  if (fromIndex < 0 && row.layer->id() == from) {
   fromIndex = i;
  }
  if (toIndex < 0 && row.layer->id() == to) {
   toIndex = i;
  }
  if (fromIndex >= 0 && toIndex >= 0) {
   break;
  }
 }

 if (fromIndex < 0 || toIndex < 0) {
  return {};
 }

 const int begin = std::min(fromIndex, toIndex);
 const int end = std::max(fromIndex, toIndex);
 QVector<LayerID> ids;
 for (int i = begin; i <= end; ++i) {
  const auto& row = rows[i];
  if (row.kind == RowKind::Layer && row.layer) {
   ids.push_back(row.layer->id());
  }
 }
 return ids;
}

QString matteTypeToText(MatteType type)
{
 switch (type) {
 case MatteType::Alpha:
  return QStringLiteral("Alpha");
 case MatteType::Luma:
  return QStringLiteral("Luma");
 case MatteType::InverseAlpha:
  return QStringLiteral("Inv Alpha");
 case MatteType::InverseLuma:
  return QStringLiteral("Inv Luma");
 }
 return QStringLiteral("Matte");
}

QString matteBlendModeToText(MatteBlendMode mode)
{
 switch (mode) {
 case MatteBlendMode::Add:
  return QStringLiteral("Add");
 case MatteBlendMode::Subtract:
  return QStringLiteral("Sub");
 case MatteBlendMode::Intersect:
  return QStringLiteral("Int");
 case MatteBlendMode::Difference:
  return QStringLiteral("Diff");
 }
 return QStringLiteral("Blend");
}

QString maskModeToText(MaskMode mode)
{
 switch (mode) {
 case MaskMode::Add:
  return QStringLiteral("Add");
 case MaskMode::Subtract:
  return QStringLiteral("Sub");
 case MaskMode::Intersect:
  return QStringLiteral("Int");
 case MaskMode::Difference:
  return QStringLiteral("Diff");
 }
 return QStringLiteral("Mask");
}

QString matteSummaryLabel(const LayerMatteReference& ref, int index)
{
 const QString base = QStringLiteral("Matte %1").arg(index + 1);
 QStringList tags;
 tags << matteTypeToText(ref.type);
 tags << QStringLiteral("Blend %1").arg(matteBlendModeToText(ref.blendMode));
  if (ref.invert) {
  tags << QStringLiteral("Inverted");
 }
 if (!ref.enabled) {
  tags << QStringLiteral("Off");
 }
 if (ref.opacity < 0.999f) {
  tags << QStringLiteral("Opacity %1").arg(QString::number(ref.opacity, 'f', 2));
 }
 return tags.isEmpty() ? base : base + QStringLiteral("  ") + tags.join(QStringLiteral(" / "));
}

QString matteSourceBadgeLabel(const ArtifactCompositionPtr& comp, const ArtifactAbstractLayerPtr& layer)
{
 if (!layer) {
  return QStringLiteral("Matte");
 }

 const auto matteRefs = layer->matteReferences();
 if (matteRefs.empty()) {
  return QStringLiteral("Matte");
 }

 const auto resolveSourceName = [&](const LayerMatteReference& ref) -> QString {
  if (!comp || ref.sourceLayerId.isNil()) {
   return QStringLiteral("<missing>");
  }
  auto source = comp->layerById(ref.sourceLayerId);
  if (!source) {
   return QStringLiteral("<missing>");
  }
  const QString name = source->layerName().trimmed();
  return name.isEmpty() ? ref.sourceLayerId.toString() : name;
 };

 const QString sourceName = resolveSourceName(matteRefs.front());
 const QString matteType = matteTypeToText(matteRefs.front().type);
 if (matteRefs.size() == 1) {
  return QStringLiteral("Matte: %1 (%2)").arg(sourceName, matteType);
 }
 return QStringLiteral("Matte: %1 (%2) +%3").arg(sourceName, matteType).arg(matteRefs.size() - 1);
}

LayerMatteReference makeDefaultMatteReference(const LayerID& sourceLayerId)
{
 LayerMatteReference ref;
 ref.sourceLayerId = sourceLayerId;
 ref.enabled = true;
 ref.type = MatteType::Alpha;
 ref.blendMode = MatteBlendMode::Add;
 ref.fitMode = MatteFitMode::Stretch;
 ref.opacity = 1.0f;
 ref.invert = false;
 return ref;
}

bool matteDropWouldCreateCycle(const ArtifactCompositionPtr& comp,
                               const ArtifactAbstractLayerPtr& targetLayer,
                               const LayerID& sourceLayerId)
{
 if (!comp || !targetLayer || sourceLayerId.isNil()) {
  return true;
 }

 const LayerID targetLayerId = targetLayer->id();
 if (targetLayerId == sourceLayerId) {
  return true;
 }

 QSet<QString> visited;
 QVector<LayerID> pending;
 pending.push_back(sourceLayerId);

 while (!pending.isEmpty()) {
  const LayerID currentId = pending.back();
  pending.pop_back();
  if (currentId.isNil()) {
   continue;
  }

  const QString key = currentId.toString();
  if (visited.contains(key)) {
   continue;
  }
  visited.insert(key);

  if (currentId == targetLayerId) {
   return true;
  }

  auto currentLayer = comp->layerById(currentId);
  if (!currentLayer) {
   continue;
  }

  for (const auto& ref : currentLayer->matteReferences()) {
   if (!ref.enabled || ref.sourceLayerId.isNil()) {
    continue;
   }
   pending.push_back(ref.sourceLayerId);
  }
 }

 return false;
}

bool applyMatteTypeToLayer(const ArtifactCompositionPtr& comp,
                           const ArtifactAbstractLayerPtr& layer,
                           int matteIndex,
                           MatteType matteType)
{
 if (!comp || !layer || matteIndex < 0) {
  return false;
 }
 auto beforeRefs = layer->matteReferences();
 if (matteIndex >= static_cast<int>(beforeRefs.size())) {
  return false;
 }

 auto afterRefs = beforeRefs;
 auto& ref = afterRefs[matteIndex];
 const MatteType previousType = ref.type;
 const bool previousInvert = ref.invert;
 ref.type = matteType;
 ref.invert = false;
 if (ref.type == previousType && ref.invert == previousInvert) {
  return false;
 }

 auto* cmd = new ChangeLayerMatteReferencesCommand(layer,
                                                   std::move(beforeRefs),
                                                   std::move(afterRefs));
 UndoManager::instance()->push(std::unique_ptr<ChangeLayerMatteReferencesCommand>(cmd));
 return true;
}

QString maskSummaryLabel(const LayerMask& mask, int index)
{
 const QString base = QStringLiteral("Mask %1").arg(index + 1);
 QStringList tags;
 const int pathCount = mask.maskPathCount();
 tags << QStringLiteral("%1 path%2").arg(pathCount).arg(pathCount == 1 ? QString() : QStringLiteral("s"));
 QStringList modeTags;
 const int limit = std::min(pathCount, 3);
 for (int i = 0; i < limit; ++i) {
  modeTags << maskModeToText(mask.maskPath(i).mode());
 }
 if (!modeTags.isEmpty()) {
  tags << QStringLiteral("Modes %1").arg(modeTags.join(QStringLiteral(",")));
 }
 if (pathCount > 3) {
  tags << QStringLiteral("+%1 more").arg(pathCount - 3);
 }
 if (!mask.isEnabled()) {
  tags << QStringLiteral("Off");
 }
 return base + QStringLiteral("  ") + tags.join(QStringLiteral(" / "));
}

class ArtifactLayerPanelWidget::Impl
{
public:
 Impl()
  {
    visibilityIcon    = loadLayerPanelPixmap(QStringLiteral("MaterialVS/neutral/visibility.svg"),     QStringLiteral("eye.png"));
    lockIcon          = loadLayerPanelPixmap(QStringLiteral("MaterialVS/yellow/lock.svg"));
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
    iconCreateModel3D = loadLayerPanelIcon(QStringLiteral("MaterialVS/blue/layers.svg"));
    iconLayerGeneric      = loadLayerPanelIcon(QStringLiteral("Studio/timeline_layer.svg"));
    iconLayerSolid        = loadLayerPanelIcon(QStringLiteral("Studio/layer_composite.svg"));
    iconLayerImage        = loadLayerPanelIcon(QStringLiteral("Studio/photo_filter.svg"));
    iconLayerSvg          = loadLayerPanelIcon(QStringLiteral("Studio/svg_layer.svg"));
    iconLayerVideo        = loadLayerPanelIcon(QStringLiteral("Studio/videocam.svg"));
    iconLayerAudio        = loadLayerPanelIcon(QStringLiteral("Studio/audiotrack.svg"));
    iconLayerText         = loadLayerPanelIcon(QStringLiteral("Studio/text_fields.svg"));
    iconLayerShape        = loadLayerPanelIcon(QStringLiteral("Studio/shape_rect.svg"));
    iconLayerPrecomp      = loadLayerPanelIcon(QStringLiteral("Studio/composition.svg"));
    iconLayerCamera       = loadLayerPanelIcon(QStringLiteral("Studio/camera_alt.svg"));
    iconLayerLight        = loadLayerPanelIcon(QStringLiteral("Studio/wb_sunny.svg"));
    iconLayerGroup        = loadLayerPanelIcon(QStringLiteral("Studio/group.svg"));
    iconLayerNull         = loadLayerPanelIcon(QStringLiteral("Studio/transform.svg"));
    iconLayerAdjustment   = loadLayerPanelIcon(QStringLiteral("Studio/tune.svg"));
    iconLayerParticle     = loadLayerPanelIcon(QStringLiteral("Studio/particle.svg"));
    iconLayerClone        = loadLayerPanelIcon(QStringLiteral("Studio/content_copy.svg"));
    iconLayerModel3D      = loadLayerPanelIcon(QStringLiteral("Studio/model3d.svg"));
    iconLayerConstruction = loadLayerPanelIcon(QStringLiteral("Studio/draw.svg"));
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
  QIcon iconCreateSolid, iconCreateNull, iconCreateAdjust, iconCreateText, iconCreateModel3D;
  QIcon iconLayerGeneric, iconLayerSolid, iconLayerImage, iconLayerSvg, iconLayerVideo, iconLayerAudio;
  QIcon iconLayerText, iconLayerShape, iconLayerPrecomp, iconLayerCamera, iconLayerLight, iconLayerGroup;
  QIcon iconLayerNull, iconLayerAdjustment, iconLayerParticle, iconLayerClone, iconLayerModel3D;
  QIcon iconLayerConstruction;
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
  LayerID selectionAnchorLayerId;
  LayerID selectedMaskLayerId;
  int selectedMaskIndex = -1;
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
  int dragMatteHoverVisibleRow = -1;
  bool dragMatteLinkMode = false;
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

  const QIcon& iconForLayerKind(TimelineLayerIconKind kind) const
  {
    switch (kind) {
    case TimelineLayerIconKind::Solid: return iconLayerSolid;
    case TimelineLayerIconKind::Image: return iconLayerImage;
    case TimelineLayerIconKind::Svg: return iconLayerSvg;
    case TimelineLayerIconKind::Video: return iconLayerVideo;
    case TimelineLayerIconKind::Audio: return iconLayerAudio;
    case TimelineLayerIconKind::Text: return iconLayerText;
    case TimelineLayerIconKind::Shape: return iconLayerShape;
    case TimelineLayerIconKind::Precomp: return iconLayerPrecomp;
    case TimelineLayerIconKind::Camera: return iconLayerCamera;
    case TimelineLayerIconKind::Light: return iconLayerLight;
    case TimelineLayerIconKind::Group: return iconLayerGroup;
    case TimelineLayerIconKind::Null: return iconLayerNull;
    case TimelineLayerIconKind::Adjustment: return iconLayerAdjustment;
    case TimelineLayerIconKind::Particle: return iconLayerParticle;
    case TimelineLayerIconKind::Clone: return iconLayerClone;
    case TimelineLayerIconKind::Model3D: return iconLayerModel3D;
    case TimelineLayerIconKind::Construction: return iconLayerConstruction;
    case TimelineLayerIconKind::Generic:
    default: return iconLayerGeneric;
    }
  }

  void clearMaskSelection()
  {
   selectedMaskLayerId = LayerID();
   selectedMaskIndex = -1;
  }

  void focusMaskSelection(const ArtifactAbstractLayerPtr& layer, const int maskIndex)
  {
   if (!layer) {
    clearMaskSelection();
    currentPropertyPath.clear();
    return;
   }

   if (maskIndex < 0 || maskIndex >= layer->maskCount()) {
    clearMaskSelection();
    currentPropertyPath.clear();
    return;
   }

   selectedLayerId = layer->id();
   selectedMaskLayerId = layer->id();
   selectedMaskIndex = maskIndex;
   currentPropertyPath = maskSelectionPropertyPath(maskIndex);
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
   dragMatteHoverVisibleRow = -1;
   dragMatteLinkMode = false;
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
   const auto matteRefs = node->matteReferences();
   const bool hasMaskStack = node->hasMasks();
   const bool hasMatteStack = !matteRefs.empty();
   const bool hasChildren = !nodeChildren.isEmpty() || !panelGroups.empty() || hasMaskStack || hasMatteStack;
   const bool expanded = expandedByLayerId.value(nodeId, true);
   if (!layerMatchesDisplayMode(node, displayMode, selectedLayerId)) {
    return;
   }
   const auto presentation = describeLayerPresentation(node);
   visibleRows.push_back(VisibleRow{
    node,
    depth,
    hasChildren,
    expanded,
    RowKind::Layer,
    QString(),
    QString(),
    QString(),
    presentation.timelineBadgeText,
    presentation.badgeTone
   });
   emitted.insert(nodeId);

   if (!hasChildren || !expanded) return;

     for (const auto& groupDef : panelGroups) {
     const QString groupName = groupDef.name().trimmed().isEmpty()
                                   ? QStringLiteral("Layer")
                                   : groupDef.name().trimmed();
     const QString groupKey = nodeId + QStringLiteral("::") + groupName.toLower();
     const bool groupExpanded = expandedByGroupKey.value(groupKey, true);
      const bool hasVisibleProperties = groupHasVisibleProperties(groupDef, displayMode);
      if (!hasVisibleProperties) {
       continue;
      }
      visibleRows.push_back(VisibleRow{
       node,
       depth + 1,
       true,
       groupExpanded,
      RowKind::Group,
      groupName,
      QString(),
      groupKey,
       QStringLiteral("Grp"),
       LayerPresentationBadgeTone::Container
      });

      if (groupExpanded) {
       for (const auto& property : groupDef.sortedProperties()) {
        if (!property) {
         continue;
        }
        if (!propertyMatchesDisplayMode(property, displayMode)) {
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
         QString(),
         QStringLiteral("Prp"),
         LayerPresentationBadgeTone::Neutral
        });
      }
      }
    }

    if (hasMaskStack) {
      const QString maskGroupKey = nodeId + QStringLiteral("::masks");
      const bool maskExpanded = expandedByGroupKey.value(maskGroupKey, true);
      visibleRows.push_back(VisibleRow{
       node,
       depth + 1,
       node->maskCount() > 0,
       maskExpanded,
       RowKind::MaskStack,
       QStringLiteral("Masks"),
       QString(),
       maskGroupKey,
       QStringLiteral("Msk"),
       LayerPresentationBadgeTone::Special
      });
      if (maskExpanded) {
       for (int maskIndex = 0; maskIndex < node->maskCount(); ++maskIndex) {
        const LayerMask mask = node->mask(maskIndex);
        visibleRows.push_back(VisibleRow{
         node,
         depth + 2,
         false,
         false,
         RowKind::Mask,
         maskSummaryLabel(mask, maskIndex),
         QString::number(maskIndex),
         QString(),
         QStringLiteral("Msk"),
         LayerPresentationBadgeTone::Special
        });
       }
      }
    }

    if (hasMatteStack) {
      const QString matteGroupKey = nodeId + QStringLiteral("::mattes");
      const bool matteExpanded = expandedByGroupKey.value(matteGroupKey, true);
      visibleRows.push_back(VisibleRow{
       node,
       depth + 1,
       !matteRefs.empty(),
       matteExpanded,
       RowKind::MatteStack,
       QStringLiteral("Track Mattes"),
       QString(),
       matteGroupKey,
       QStringLiteral("Mat"),
       LayerPresentationBadgeTone::Special
      });
      if (matteExpanded) {
       for (size_t matteIndex = 0; matteIndex < matteRefs.size(); ++matteIndex) {
        const LayerMatteReference& ref = matteRefs[matteIndex];
        visibleRows.push_back(VisibleRow{
         node,
         depth + 2,
         false,
         false,
         RowKind::Matte,
         matteSummaryLabel(ref, static_cast<int>(matteIndex)),
         QString::number(static_cast<int>(matteIndex)),
         QString(),
         QStringLiteral("Mat"),
         LayerPresentationBadgeTone::Special
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

 impl_->eventBusSubscriptions_.push_back(
  impl_->eventBus_.subscribe<FrameChangedEvent>([this](const FrameChangedEvent& event) {
    if (!impl_->compositionId.isNil() &&
        event.compositionId != impl_->compositionId.toString()) {
      return;
    }
    if (auto* playback = ArtifactPlaybackService::instance()) {
      const auto fps = playback->frameRate().framerate();
      impl_->currentTime = RationalTime(event.frame, fps);
      update();
    }
  }));

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
        if (event.compositionId == impl_->compositionId.toString()) {
          updateLayout();
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
            impl_->selectionAnchorLayerId = layerId.isNil() ? LayerID() : layerId;
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
  impl_->selectionAnchorLayerId = LayerID();
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
          a.groupKey != b.groupKey ||
          a.auxiliaryText != b.auxiliaryText ||
          a.auxiliaryTone != b.auxiliaryTone) {
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

  if (!impl_->selectedMaskLayerId.isNil() &&
      impl_->selectedMaskLayerId == impl_->selectedLayerId) {
    auto comp = safeCompositionLookup(impl_->compositionId);
    auto layer = comp ? comp->layerById(impl_->selectedMaskLayerId)
                      : ArtifactAbstractLayerPtr{};
    if (!layer || layer->maskCount() <= 0) {
      if (impl_->selectedMaskIndex >= 0 ||
          !impl_->currentPropertyPath.isEmpty()) {
        impl_->clearMaskSelection();
        impl_->currentPropertyPath.clear();
        propertyFocusChanged(impl_->selectedLayerId, impl_->currentPropertyPath);
      }
    } else if (impl_->selectedMaskIndex >= layer->maskCount()) {
      const int clampedIndex = layer->maskCount() - 1;
      impl_->focusMaskSelection(layer, clampedIndex);
      propertyFocusChanged(impl_->selectedLayerId, impl_->currentPropertyPath);
    }
  }

  impl_->setVerticalOffset(impl_->verticalOffset, this);
  update();
  if (structureChanged) {
    ArtifactCore::globalEventBus().publish<TimelineVisibleRowsChangedEvent>({});
    visibleRowsChanged();
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
   descriptor.auxiliaryText = row.auxiliaryText;
   descriptor.auxiliaryTone = row.auxiliaryTone;
   if (descriptor.auxiliaryText.isEmpty() &&
       (row.kind == RowKind::Mask || row.kind == RowKind::Matte)) {
    descriptor.auxiliaryText = row.label;
   } else if (descriptor.auxiliaryText.isEmpty() &&
              (row.kind == RowKind::MaskStack || row.kind == RowKind::MatteStack)) {
    descriptor.auxiliaryText = row.label;
   }
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
    const int layerIconAdvance = kLayerTypeIconSize + kLayerTypeIconGap;
    const int textX = nameStartX + rowIndent +
                      (impl_->visibleRows[idx].hasChildren ? 16 : 4) +
                      layerIconAdvance;
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
  if (row.kind == RowKind::Group ||
      row.kind == RowKind::MaskStack ||
      row.kind == RowKind::MatteStack) {
   impl_->clearMaskSelection();
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
   impl_->clearMaskSelection();
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
  if (row.kind == RowKind::Mask) {
    impl_->clearDragState();
    if (event->button() == Qt::LeftButton) {
      auto* service = ArtifactProjectService::instance();
      if (service) {
        service->selectLayer(layer->id());
      }
      impl_->selectedLayerId = layer->id();
      impl_->selectedMaskLayerId = layer->id();
      impl_->selectedMaskIndex = row.propertyPath.trimmed().toInt();
      impl_->currentPropertyPath = maskSelectionPropertyPath(impl_->selectedMaskIndex);
      propertyFocusChanged(impl_->selectedLayerId, impl_->currentPropertyPath);
      update();
    }
    event->accept();
    return;
  }
  if (row.kind == RowKind::Matte) {
    impl_->clearDragState();
    impl_->clearMaskSelection();
    if (event->button() == Qt::LeftButton) {
      auto* service = ArtifactProjectService::instance();
      if (service) {
        service->selectLayer(layer->id());
      }
      impl_->selectedLayerId = layer->id();
      impl_->currentPropertyPath.clear();
      propertyFocusChanged(impl_->selectedLayerId, impl_->currentPropertyPath);
      update();
    } else if (event->button() == Qt::RightButton) {
      auto comp = safeCompositionLookup(impl_->compositionId);
      if (comp && layer) {
        bool ok = false;
        const int matteIndex = row.propertyPath.trimmed().toInt(&ok);
        if (ok && matteIndex >= 0) {
          const auto matteRefs = layer->matteReferences();
          if (matteIndex < static_cast<int>(matteRefs.size())) {
            QMenu menu(this);
            QAction *focusAction = menu.addAction(QStringLiteral("Focus source"));
            focusAction->setData(QVariantMap{{QStringLiteral("kind"), QStringLiteral("matte_focus")},
                                             {QStringLiteral("index"), matteIndex}});

            QMenu *typeMenu = menu.addMenu(QStringLiteral("Set matte type"));
            const QStringList typeLabels = {
                QStringLiteral("Alpha"),
                QStringLiteral("Luma"),
                QStringLiteral("Inverted Alpha"),
                QStringLiteral("Inverted Luma")};
            for (int typeIndex = 0; typeIndex < typeLabels.size(); ++typeIndex) {
              QAction *typeAction = typeMenu->addAction(typeLabels[typeIndex]);
              typeAction->setData(QVariantMap{{QStringLiteral("kind"), QStringLiteral("matte_type")},
                                              {QStringLiteral("index"), matteIndex},
                                              {QStringLiteral("type"), typeIndex}});
            }

            if (QAction *chosenAction = menu.exec(event->globalPos())) {
              const QVariantMap data = chosenAction->data().toMap();
              const QString kind = data.value(QStringLiteral("kind")).toString();
              bool indexOk = false;
              const int index = data.value(QStringLiteral("index")).toInt(&indexOk);
              if (!indexOk) {
                return;
              }
              if (kind == QStringLiteral("matte_focus") && index >= 0 && index < static_cast<int>(matteRefs.size())) {
                const auto &chosenRef = matteRefs[index];
                if (!chosenRef.sourceLayerId.isNil()) {
                  if (auto *service = ArtifactProjectService::instance()) {
                    service->selectLayer(chosenRef.sourceLayerId);
                  }
                }
              } else if (kind == QStringLiteral("matte_type")) {
                bool typeOk = false;
                const int typeIndex = data.value(QStringLiteral("type")).toInt(&typeOk);
                if (!typeOk) {
                  return;
                }
                if (typeIndex >= 0 && typeIndex <= static_cast<int>(MatteType::InverseLuma)) {
                  if (applyMatteTypeToLayer(comp, layer, matteIndex,
                                            static_cast<MatteType>(typeIndex))) {
                    updateLayout();
                  }
                }
              }
            }
          }
        }
      }
    }
    event->accept();
    return;
  }
  auto* service = ArtifactProjectService::instance();
  if (row.kind != RowKind::Layer) {
    impl_->clearDragState();
    impl_->clearMaskSelection();
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
    impl_->dragCandidateLayerId = LayerID();
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
    auto* selectionManager = currentLayerSelectionManager();
    auto comp = safeCompositionLookup(impl_->compositionId);
    const bool rangeSelection = (event->modifiers() & Qt::ShiftModifier) &&
                                !(event->modifiers() & (Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier));
    const bool toggleSelection = (event->modifiers() & Qt::ControlModifier) &&
                                 !(event->modifiers() & (Qt::AltModifier | Qt::ShiftModifier | Qt::MetaModifier));
    if (rangeSelection) {
      if (selectionManager) {
        selectionManager->setActiveComposition(comp);
        LayerID anchor = impl_->selectionAnchorLayerId;
        if (anchor.isNil()) {
          anchor = impl_->selectedLayerId.isNil() ? layer->id() : impl_->selectedLayerId;
        }
        const auto ids = visibleLayerIdsInRange(impl_->visibleRows, anchor, layer->id());
        if (!ids.isEmpty()) {
          selectionManager->clearSelection();
          for (const auto& id : ids) {
            if (comp) {
              selectionManager->addToSelection(comp->layerById(id));
            }
          }
          impl_->selectedLayerId = layer->id();
          impl_->selectionAnchorLayerId = anchor;
        } else {
          impl_->selectedLayerId = layer->id();
          impl_->selectionAnchorLayerId = layer->id();
          selectionManager->selectLayer(comp ? comp->layerById(layer->id()) : ArtifactAbstractLayerPtr{});
        }
      } else {
        impl_->selectedLayerId = layer->id();
        impl_->selectionAnchorLayerId = layer->id();
      }
      impl_->clearMaskSelection();
      impl_->currentPropertyPath.clear();
      propertyFocusChanged(impl_->selectedLayerId, impl_->currentPropertyPath);
      if (!clickInInlineCombo) {
        impl_->clearInlineEditors();
      }
      update();
      event->accept();
      return;
    }
    if (toggleSelection) {
      if (selectionManager) {
        selectionManager->setActiveComposition(comp);
        const auto selectedLayer = comp ? comp->layerById(layer->id()) : ArtifactAbstractLayerPtr{};
        if (selectionManager->isSelected(selectedLayer)) {
          selectionManager->removeFromSelection(selectedLayer);
        } else {
          selectionManager->addToSelection(selectedLayer);
        }
        const auto current = selectionManager->currentLayer();
        impl_->selectedLayerId = current ? current->id() : LayerID();
        impl_->selectionAnchorLayerId = current ? current->id() : LayerID();
      } else {
        impl_->selectedLayerId = layer->id();
        impl_->selectionAnchorLayerId = layer->id();
      }
      impl_->clearMaskSelection();
      impl_->currentPropertyPath.clear();
      propertyFocusChanged(impl_->selectedLayerId, impl_->currentPropertyPath);
      if (!clickInInlineCombo) {
        impl_->clearInlineEditors();
      }
      update();
      event->accept();
      return;
    }

    impl_->selectedLayerId = layer->id();
    impl_->selectionAnchorLayerId = layer->id();
    impl_->clearMaskSelection();
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
        layer->changed();
        ArtifactCore::globalEventBus().publish<LayerChangedEvent>(LayerChangedEvent{
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
    bool handledLayerSwitch = false;
    if (clickX < colW) {
      if (service) service->setLayerVisibleInCurrentComposition(layer->id(), !layer->isVisible());
      handledLayerSwitch = true;
    } else if (clickX < colW * 2) {
      if (service) service->setLayerLockedInCurrentComposition(layer->id(), !layer->isLocked());
      handledLayerSwitch = true;
    } else if (clickX < colW * 3) {
      if (service) service->setLayerSoloInCurrentComposition(layer->id(), !layer->isSolo());
      handledLayerSwitch = true;
    } else if (clickX < colW * 4) {
      if (service) service->setLayerShyInCurrentComposition(layer->id(), !layer->isShy());
      handledLayerSwitch = true;
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
      
      const QRect chipRect = variantChipRect(QRect(0, impl_->rowViewportY(idx), width(), rowH),
                                             layer, showInlineCombos, rowH);
      if (chipRect.contains(event->pos())) {
          showVariantPickerMenu(this, layer, event->globalPos());
          event->accept();
          return;
      }
    }
    if (handledLayerSwitch) {
      if (service) {
        service->selectLayer(layer->id());
      }
      impl_->clearDragState();
      update();
      event->accept();
      return;
    }

    // 名前エリアだけをドラッグ開始候補にする。スイッチ操作の微小移動で
    // レイヤー順序変更が走るのを避ける。
    if (service) {
      service->selectLayer(layer->id());
      impl_->dragStartPos = event->pos();
      impl_->dragCandidateLayerId = layer->id();
    }
    update();
  } else if (event->button() == Qt::RightButton) {
    if (service && !isLayerSelectedInSelectionManager(layer->id())) {
      service->selectLayer(layer->id());
    }

    auto triggerDeleteLayer = [this, layer]() {
      if (!layer) {
        return;
      }
      const auto response = QMessageBox::question(
          this, QStringLiteral("Delete Layer"),
          QStringLiteral("Delete layer \"%1\"?").arg(layer->layerName()),
          QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
      if (response != QMessageBox::Yes) {
        return;
      }
      if (auto *svc = ArtifactProjectService::instance()) {
        auto comp = safeCompositionLookup(impl_->compositionId);
        const CompositionID compId = comp ? comp->id() : impl_->compositionId;
        if (!compId.isNil()) {
          svc->removeLayerFromComposition(compId, layer->id());
          impl_->selectedLayerId = LayerID();
          impl_->currentPropertyPath.clear();
          impl_->clearDragState();
          updateLayout();
        }
      }
    };

    const QVector<LayerID> selectedIds = selectedLayerIdsSnapshot();
    const QVector<LayerID> selectedVisibleIds = selectedLayerIdsInVisibleOrder(impl_->visibleRows);
    auto triggerDeleteSelectedLayers = [this, selectedIds]() {
      if (selectedIds.size() <= 1) {
        return;
      }
      const auto response = QMessageBox::question(
          this, QStringLiteral("Delete Layers"),
          QStringLiteral("Delete %1 selected layers?").arg(selectedIds.size()),
          QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
      if (response != QMessageBox::Yes) {
        return;
      }
      if (auto* svc = ArtifactProjectService::instance()) {
        auto comp = safeCompositionLookup(impl_->compositionId);
        const CompositionID compId = comp ? comp->id() : impl_->compositionId;
        if (!compId.isNil()) {
          if (auto* selectionManager = currentLayerSelectionManager()) {
            selectionManager->clearSelection();
          }
          impl_->clearDragState();
          for (const auto& layerId : selectedIds) {
            svc->removeLayerFromComposition(compId, layerId);
          }
  impl_->selectedLayerId = LayerID();
  impl_->selectionAnchorLayerId = LayerID();
  impl_->currentPropertyPath.clear();
  propertyFocusChanged(impl_->selectedLayerId, impl_->currentPropertyPath);
  updateLayout();
        }
      }
    };

    auto triggerDuplicateLayer = [this, layer]() {
      if (!layer) {
        return;
      }
      if (auto *svc = ArtifactProjectService::instance()) {
        if (svc->duplicateLayerInCurrentComposition(layer->id())) {
          updateLayout();
        }
      }
    };

    auto triggerDuplicateSelectedLayers = [this, selectedVisibleIds]() {
      if (selectedVisibleIds.size() <= 1) {
        return;
      }
      if (auto* svc = ArtifactProjectService::instance()) {
        auto comp = safeCompositionLookup(impl_->compositionId);
        const CompositionID compId = comp ? comp->id() : impl_->compositionId;
        if (!compId.isNil()) {
          for (const auto& layerId : selectedVisibleIds) {
            svc->duplicateLayerInCurrentComposition(layerId);
          }
          updateLayout();
        }
      }
    };

    auto triggerGroupSelectedLayers = [this, selectedVisibleIds]() {
      if (selectedVisibleIds.size() <= 1) {
        return;
      }
      if (auto* svc = ArtifactProjectService::instance()) {
        if (svc->groupSelectedLayersInCurrentComposition()) {
          updateLayout();
        }
      }
    };

    auto triggerPrecomposeSelectedLayers = [this, selectedVisibleIds]() {
      if (selectedVisibleIds.size() <= 1) {
        return;
      }
      if (auto* svc = ArtifactProjectService::instance()) {
        if (svc->precomposeLayersInCurrentComposition(selectedVisibleIds,
                                                      UniString(QStringLiteral("Precomp")),
                                                      true, true)) {
          updateLayout();
        }
      }
    };

    auto triggerSelectedVisibility = [this, selectedVisibleIds](bool visible) {
      if (selectedVisibleIds.isEmpty()) {
        return;
      }
      if (auto* svc = ArtifactProjectService::instance()) {
        for (const auto& layerId : selectedVisibleIds) {
          svc->setLayerVisibleInCurrentComposition(layerId, visible);
        }
        updateLayout();
      }
    };

    auto triggerSelectedLock = [this, selectedVisibleIds](bool locked) {
      if (selectedVisibleIds.isEmpty()) {
        return;
      }
      if (auto* svc = ArtifactProjectService::instance()) {
        for (const auto& layerId : selectedVisibleIds) {
          svc->setLayerLockedInCurrentComposition(layerId, locked);
        }
        updateLayout();
      }
    };

    auto triggerSelectedSolo = [this, selectedVisibleIds](bool solo) {
      if (selectedVisibleIds.isEmpty()) {
        return;
      }
      if (auto* svc = ArtifactProjectService::instance()) {
        for (const auto& layerId : selectedVisibleIds) {
          svc->setLayerSoloInCurrentComposition(layerId, solo);
        }
        updateLayout();
      }
    };

    auto triggerSelectedShy = [this, selectedVisibleIds](bool shy) {
      if (selectedVisibleIds.isEmpty()) {
        return;
      }
      if (auto* svc = ArtifactProjectService::instance()) {
        for (const auto& layerId : selectedVisibleIds) {
          svc->setLayerShyInCurrentComposition(layerId, shy);
        }
        updateLayout();
      }
    };

    auto triggerReplaceLayerSource = [this, layer]() {
      if (!layer) {
        return;
      }
      const auto sourceFilter = QStringLiteral(
          "Image Files (*.png *.jpg *.jpeg *.bmp *.tif *.tiff *.webp *.exr);;"
          "All Files (*.*)");
      const QString currentPath = [&]() -> QString {
        if (auto imageLayer = std::dynamic_pointer_cast<ArtifactImageLayer>(layer)) {
          return imageLayer->sourcePath();
        }
        return {};
      }();
      const QString selectedPath = QFileDialog::getOpenFileName(
          this, QStringLiteral("Replace Image Source"),
          currentPath.isEmpty() ? QString() : QFileInfo(currentPath).absolutePath(),
          sourceFilter);
      if (selectedPath.isEmpty()) {
        return;
      }
      if (auto *svc = ArtifactProjectService::instance()) {
        if (svc->replaceLayerSourceInCurrentComposition(layer->id(), selectedPath)) {
          updateLayout();
        }
      }
    };

    auto videoSourcePath = [layer]() -> QString {
      if (auto videoLayer = std::dynamic_pointer_cast<ArtifactVideoLayer>(layer)) {
        return videoLayer->sourcePath();
      }
      return {};
    };

    auto triggerReplaceVideoSource = [this, layer, videoSourcePath]() {
      if (!layer) {
        return;
      }
      const QString currentPath = videoSourcePath();
      const QString selectedPath = QFileDialog::getOpenFileName(
          this, QStringLiteral("Replace Video Source"),
          currentPath.isEmpty() ? QString() : QFileInfo(currentPath).absolutePath(),
          QStringLiteral("Video Files (*.mp4 *.mov *.mkv *.avi *.webm *.m4v *.mpg *.mpeg *.mxf *.gif);;All Files (*.*)"));
      if (selectedPath.isEmpty()) {
        return;
      }
      if (auto *svc = ArtifactProjectService::instance()) {
        if (svc->replaceLayerSourceInCurrentComposition(layer->id(), selectedPath)) {
          updateLayout();
        }
      }
    };

    auto triggerReloadVideoSource = [this, layer, videoSourcePath]() {
      if (!layer) {
        return;
      }
      const QString currentPath = videoSourcePath();
      if (currentPath.trimmed().isEmpty()) {
        return;
      }
      if (auto *svc = ArtifactProjectService::instance()) {
        if (svc->replaceLayerSourceInCurrentComposition(layer->id(), currentPath)) {
          updateLayout();
        }
      }
    };

    auto triggerRevealVideoSource = [layer, videoSourcePath]() {
      const QString currentPath = videoSourcePath();
      if (currentPath.trimmed().isEmpty()) {
        return;
      }
      const QFileInfo info(currentPath);
      const QString folder = info.absolutePath();
      if (!folder.isEmpty()) {
        QDesktopServices::openUrl(QUrl::fromLocalFile(folder));
      }
    };

    auto triggerOpenComposition = [this, layer]() {
      if (!layer) {
        return;
      }
      auto *compLayer = dynamic_cast<ArtifactCompositionLayer *>(layer.get());
      if (!compLayer) {
        return;
      }
      const CompositionID sourceCompId = compLayer->sourceCompositionId();
      if (sourceCompId.isNil()) {
        return;
      }
      if (auto *svc = ArtifactProjectService::instance()) {
        svc->changeCurrentComposition(sourceCompId);
      }
    };

    auto triggerRenameComposition = [this, layer]() {
      if (!layer) {
        return;
      }
      auto *compLayer = dynamic_cast<ArtifactCompositionLayer *>(layer.get());
      if (!compLayer) {
        return;
      }
      const CompositionID sourceCompId = compLayer->sourceCompositionId();
      if (sourceCompId.isNil()) {
        return;
      }
      auto *svc = ArtifactProjectService::instance();
      if (!svc) {
        return;
      }
      auto found = svc->findComposition(sourceCompId);
      auto comp = found.ptr.lock();
      const QString currentName = comp
                                      ? comp->settings().compositionName().toQString()
                                      : layer->layerName();
      bool ok = false;
      const QString newName = QInputDialog::getText(
          this, QStringLiteral("Rename Composition"),
          QStringLiteral("New composition name:"),
          QLineEdit::Normal, currentName, &ok);
      if (!ok || newName.trimmed().isEmpty()) {
        return;
      }
      if (svc->renameComposition(sourceCompId, UniString(newName))) {
        updateLayout();
      }
    };

    auto triggerDuplicateComposition = [this, layer]() {
      if (!layer) {
        return;
      }
      auto *compLayer = dynamic_cast<ArtifactCompositionLayer *>(layer.get());
      if (!compLayer) {
        return;
      }
      const CompositionID sourceCompId = compLayer->sourceCompositionId();
      if (sourceCompId.isNil()) {
        return;
      }
      auto *svc = ArtifactProjectService::instance();
      if (!svc) {
        return;
      }
      if (svc->duplicateComposition(sourceCompId)) {
        updateLayout();
      }
    };

    const int nameStartX = colW * kLayerPropertyColumnCount;
    const int nameX = nameStartX + row.depth * 14;
    const bool showInlineCombos = width() - (nameX + 8) >= (kInlineComboReserve + kLayerNameMinWidth);
    const auto variants = layer->getVariants();
    const QRect chipRect = variantChipRect(QRect(0, impl_->rowViewportY(idx), width(), rowH),
                                           layer, showInlineCombos, rowH);
    if (chipRect.contains(event->pos())) {
      showVariantPickerMenu(this, layer, event->globalPos());
      event->accept();
      return;
    }

    auto comp = safeCompositionLookup(impl_->compositionId);
    QMenu menu(this);
    if (auto *compLayer = dynamic_cast<ArtifactCompositionLayer *>(layer.get())) {
      QMenu *precompMenu = menu.addMenu(QStringLiteral("プリコンポーズ"));
      precompMenu->addAction(QStringLiteral("コンポジションを開く"),
                             [triggerOpenComposition]() {
                               triggerOpenComposition();
                             });
      precompMenu->addAction(QStringLiteral("コンポジション名を変更..."),
                             [triggerRenameComposition]() {
                               triggerRenameComposition();
                             });
      precompMenu->addAction(QStringLiteral("コンポジションを複製"),
                             [triggerDuplicateComposition]() {
                               triggerDuplicateComposition();
                             });
      if (compLayer->sourceCompositionId().isNil()) {
        precompMenu->setEnabled(false);
      }
      menu.addSeparator();
    }
    const bool isImageLayer = std::dynamic_pointer_cast<ArtifactImageLayer>(layer) != nullptr;
    if (isImageLayer) {
      menu.addAction("Replace Image...", [triggerReplaceLayerSource]() {
        triggerReplaceLayerSource();
      });
      menu.addSeparator();
    }
    const bool isVideoLayer = std::dynamic_pointer_cast<ArtifactVideoLayer>(layer) != nullptr;
    if (isVideoLayer) {
      QMenu* videoMenu = menu.addMenu(QStringLiteral("Video"));
      videoMenu->setIcon(QIcon(resolveIconPath("Studio/videocam.svg")));
      QAction* replaceVideoAct = videoMenu->addAction(QIcon(resolveIconPath("Studio/file_open.svg")),
                                                      QStringLiteral("Replace Source..."));
      QAction* reloadVideoAct = videoMenu->addAction(QIcon(resolveIconPath("Studio/replay.svg")),
                                                    QStringLiteral("Reload Source"));
      QAction* revealVideoAct = videoMenu->addAction(QIcon(resolveIconPath("Studio/folder_open.svg")),
                                                    QStringLiteral("Reveal Source"));
      videoMenu->addSeparator();
      QAction* muteAudioAct = videoMenu->addAction(QIcon(resolveIconPath("Studio/settings.svg")),
                                                   QStringLiteral("Toggle Audio Mute"));
      QAction* toggleVideoAct = videoMenu->addAction(QIcon(resolveIconPath("Studio/visibility.svg")),
                                                     QStringLiteral("Toggle Video Enabled"));

      const QString currentVideoPath = videoSourcePath();
      reloadVideoAct->setEnabled(!currentVideoPath.trimmed().isEmpty());
      revealVideoAct->setEnabled(!currentVideoPath.trimmed().isEmpty());
      muteAudioAct->setEnabled(true);
      toggleVideoAct->setEnabled(true);

      QObject::connect(replaceVideoAct, &QAction::triggered, videoMenu, [triggerReplaceVideoSource](bool) {
        triggerReplaceVideoSource();
      });
      QObject::connect(reloadVideoAct, &QAction::triggered, videoMenu, [triggerReloadVideoSource](bool) {
        triggerReloadVideoSource();
      });
      QObject::connect(revealVideoAct, &QAction::triggered, videoMenu, [triggerRevealVideoSource](bool) {
        triggerRevealVideoSource();
      });
      QObject::connect(muteAudioAct, &QAction::triggered, videoMenu, [this, layer](bool) {
        auto videoLayer = std::dynamic_pointer_cast<ArtifactVideoLayer>(layer);
        if (!videoLayer) {
          return;
        }
        const bool muted = !videoLayer->isAudioMuted();
        videoLayer->setAudioMuted(muted);
        if (layer->setLayerPropertyValue(QStringLiteral("video.audioMuted"), muted)) {
          updateLayout();
        }
      });
      QObject::connect(toggleVideoAct, &QAction::triggered, videoMenu, [this, layer](bool) {
        auto videoLayer = std::dynamic_pointer_cast<ArtifactVideoLayer>(layer);
        if (!videoLayer) {
          return;
        }
        const bool enabled = !videoLayer->hasVideo();
        videoLayer->setHasVideo(enabled);
        if (layer->setLayerPropertyValue(QStringLiteral("video.videoEnabled"), enabled)) {
          updateLayout();
        }
      });
      menu.addSeparator();
    }
    if (!variants.empty()) {
      menu.addAction(QStringLiteral("Variant Picker..."), [this, layer]() {
        showVariantPickerMenu(this, layer, QCursor::pos());
      });
      menu.addSeparator();
    }
    const auto matteRefs = layer->matteReferences();
    if (!matteRefs.empty()) {
      QMenu *matteMenu = menu.addMenu(QStringLiteral("Track Matte"));
      const QStringList typeLabels = {
          QStringLiteral("Alpha"),
          QStringLiteral("Luma"),
          QStringLiteral("Inverted Alpha"),
          QStringLiteral("Inverted Luma")};
      for (int matteIndex = 0; matteIndex < static_cast<int>(matteRefs.size()); ++matteIndex) {
        const auto &ref = matteRefs[matteIndex];
        QString sourceName = QStringLiteral("<missing>");
        if (comp && !ref.sourceLayerId.isNil()) {
          if (auto source = comp->layerById(ref.sourceLayerId)) {
            const QString name = source->layerName().trimmed();
            sourceName = name.isEmpty() ? ref.sourceLayerId.toString() : name;
          }
        }
        QMenu *refMenu = matteMenu->addMenu(
            QStringLiteral("Matte %1: %2").arg(matteIndex + 1).arg(sourceName));
        QAction *focusAction = refMenu->addAction(QStringLiteral("Focus source"));
        focusAction->setData(QVariantMap{{QStringLiteral("kind"), QStringLiteral("matte_focus")},
                                         {QStringLiteral("index"), matteIndex}});

        QMenu *typeMenu = refMenu->addMenu(QStringLiteral("Set matte type"));
        for (int typeIndex = 0; typeIndex < typeLabels.size(); ++typeIndex) {
          QAction *typeAction = typeMenu->addAction(typeLabels[typeIndex]);
          typeAction->setData(QVariantMap{{QStringLiteral("kind"), QStringLiteral("matte_type")},
                                          {QStringLiteral("index"), matteIndex},
                                          {QStringLiteral("type"), typeIndex}});
        }
      }
      menu.addSeparator();
    }
    if (selectedIds.size() > 1) {
      menu.addAction(QStringLiteral("Duplicate Selected Layers"), [triggerDuplicateSelectedLayers]() {
        triggerDuplicateSelectedLayers();
      });
      menu.addAction(QStringLiteral("Group Selected Layers"), [triggerGroupSelectedLayers]() {
        triggerGroupSelectedLayers();
      });
      menu.addAction(QStringLiteral("Precompose Selected Layers"), [triggerPrecomposeSelectedLayers]() {
        triggerPrecomposeSelectedLayers();
      });
      menu.addSeparator();
      QMenu* batchStateMenu = menu.addMenu(QStringLiteral("Batch State"));
      batchStateMenu->addAction(QStringLiteral("Show Selected"), [triggerSelectedVisibility]() {
        triggerSelectedVisibility(true);
      });
      batchStateMenu->addAction(QStringLiteral("Hide Selected"), [triggerSelectedVisibility]() {
        triggerSelectedVisibility(false);
      });
      batchStateMenu->addAction(QStringLiteral("Lock Selected"), [triggerSelectedLock]() {
        triggerSelectedLock(true);
      });
      batchStateMenu->addAction(QStringLiteral("Unlock Selected"), [triggerSelectedLock]() {
        triggerSelectedLock(false);
      });
      batchStateMenu->addAction(QStringLiteral("Solo Selected"), [triggerSelectedSolo]() {
        triggerSelectedSolo(true);
      });
      batchStateMenu->addAction(QStringLiteral("Unsolo Selected"), [triggerSelectedSolo]() {
        triggerSelectedSolo(false);
      });
      batchStateMenu->addAction(QStringLiteral("Shy Selected"), [triggerSelectedShy]() {
        triggerSelectedShy(true);
      });
      batchStateMenu->addAction(QStringLiteral("Unshy Selected"), [triggerSelectedShy]() {
        triggerSelectedShy(false);
      });
      menu.addSeparator();
      menu.addAction(QStringLiteral("Delete Selected Layers"), [triggerDeleteSelectedLayers]() {
        triggerDeleteSelectedLayers();
      });
      menu.addSeparator();
    }
    menu.addAction("Duplicate Layer", [triggerDuplicateLayer]() {
      triggerDuplicateLayer();
    });
    menu.addAction("Delete Layer", [triggerDeleteLayer]() {
      triggerDeleteLayer();
    });
    QAction *chosenAction = menu.exec(event->globalPos());
    if (chosenAction) {
      const QVariantMap data = chosenAction->data().toMap();
      const QString kind = data.value(QStringLiteral("kind")).toString();
      bool matteIndexOk = false;
      const int matteIndex = data.value(QStringLiteral("index")).toInt(&matteIndexOk);
      if (!matteIndexOk) {
        return;
      }
      if (kind == QStringLiteral("matte_focus")) {
        if (matteIndex >= 0 && matteIndex < static_cast<int>(matteRefs.size())) {
          const auto &ref = matteRefs[matteIndex];
          if (!ref.sourceLayerId.isNil()) {
            if (auto *svc = ArtifactProjectService::instance()) {
              svc->selectLayer(ref.sourceLayerId);
            }
          }
        }
      } else if (kind == QStringLiteral("matte_type")) {
        bool matteTypeOk = false;
        const int matteTypeIndex = data.value(QStringLiteral("type")).toInt(&matteTypeOk);
        if (!matteTypeOk) {
          return;
        }
        if (comp && matteIndex >= 0 && matteIndex < static_cast<int>(matteRefs.size()) &&
            matteTypeIndex >= 0 && matteTypeIndex <= static_cast<int>(MatteType::InverseLuma)) {
          if (applyMatteTypeToLayer(comp, layer, matteIndex,
                                    static_cast<MatteType>(matteTypeIndex))) {
            updateLayout();
          }
        }
      }
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

   const int nameStartX = colW * kLayerPropertyColumnCount;
  const bool showInlineCombos = width() >= (kLayerColumnWidth * kLayerPropertyColumnCount + kInlineComboReserve + kLayerNameMinWidth);
  const int parentRectX = width() - kInlineComboReserve;
  const int nameX = nameStartX + row.depth * 14 + (row.hasChildren ? 16 : 4);
  const int nameWidth = showInlineCombos ? std::max(20, parentRectX - nameX - 8) : std::max(20, width() - nameX - 8);
  const QRect editRect(nameX + 2, impl_->rowViewportY(idx) + 2, nameWidth, rowH - 4);

  if (auto *compLayer = dynamic_cast<ArtifactCompositionLayer *>(layer.get())) {
    const CompositionID sourceCompId = compLayer->sourceCompositionId();
    if (!sourceCompId.isNil()) {
      if (auto *svc = ArtifactProjectService::instance()) {
        if (svc->changeCurrentComposition(sourceCompId).success) {
          event->accept();
          return;
        }
      }
    }
  }

   if (!impl_->layerNameEditable) {
    event->accept();
    return;
   }

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
      impl_->dragMatteLinkMode = (event->modifiers() & Qt::AltModifier) &&
                                 !(event->modifiers() & (Qt::ControlModifier | Qt::ShiftModifier | Qt::MetaModifier));
    }
    if (impl_->dragStarted_) {
      if (impl_->dragMatteLinkMode) {
        auto comp = safeCompositionLookup(impl_->compositionId);
        const auto sourceLayer = comp ? comp->layerById(impl_->draggedLayerId) : ArtifactAbstractLayerPtr{};
        impl_->dragMatteHoverVisibleRow = impl_->rowIndexFromViewportY(event->pos().y());
        impl_->dragInsertVisibleRow = -1;
        const bool validDropTarget =
            impl_->dragMatteHoverVisibleRow >= 0 &&
            impl_->dragMatteHoverVisibleRow < impl_->visibleRows.size() &&
            impl_->visibleRows[impl_->dragMatteHoverVisibleRow].layer &&
            !matteDropWouldCreateCycle(comp,
                                       impl_->visibleRows[impl_->dragMatteHoverVisibleRow].layer,
                                       sourceLayer ? sourceLayer->id() : LayerID());
        setCursor(validDropTarget ? Qt::CrossCursor : Qt::ForbiddenCursor);
      } else {
        impl_->dragInsertVisibleRow = impl_->insertionVisibleRowForY(static_cast<int>(event->pos().y() + impl_->verticalOffset));
        impl_->dragMatteHoverVisibleRow = -1;
        setCursor(Qt::DragMoveCursor);
      }
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

  QString toolTipText;
  if (idx >= 0 && idx < impl_->visibleRows.size()) {
    const auto& row = impl_->visibleRows[idx];
    if (row.layer && row.kind == RowKind::Layer) {
      const auto mattes = row.layer->matteReferences();
      if (!mattes.empty()) {
        QStringList parts;
        parts.reserve(static_cast<int>(mattes.size()));
        for (size_t i = 0; i < mattes.size(); ++i) {
          parts << matteSummaryLabel(mattes[i], static_cast<int>(i));
        }
        toolTipText = QStringLiteral("Track Mattes: %1").arg(parts.join(QStringLiteral(" | ")));
      }
    }
  }
  setToolTip(toolTipText);
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
        if (impl_->dragMatteLinkMode) {
          const int targetVisibleRow = impl_->dragMatteHoverVisibleRow;
          if (targetVisibleRow >= 0 && targetVisibleRow < impl_->visibleRows.size()) {
            const auto& targetRow = impl_->visibleRows[targetVisibleRow];
            auto targetLayer = targetRow.layer;
            auto sourceLayer = comp->layerById(dragLayerId);
            if (targetLayer && sourceLayer && !matteDropWouldCreateCycle(comp, targetLayer, dragLayerId)) {
              auto beforeRefs = targetLayer->matteReferences();
              auto afterRefs = beforeRefs;

              const auto replaceSource = [dragLayerId](LayerMatteReference ref) {
                ref.sourceLayerId = dragLayerId;
                ref.enabled = true;
                return ref;
              };

              bool changed = false;
              if (targetRow.kind == RowKind::Matte) {
                bool ok = false;
                const int matteIndex = targetRow.propertyPath.trimmed().toInt(&ok);
                if (ok && matteIndex >= 0 && matteIndex < static_cast<int>(afterRefs.size())) {
                  auto updated = afterRefs[matteIndex];
                  updated = replaceSource(updated);
                  if (updated.sourceLayerId != beforeRefs[matteIndex].sourceLayerId ||
                      updated.enabled != beforeRefs[matteIndex].enabled) {
                    afterRefs[matteIndex] = updated;
                    changed = true;
                  }
                }
              } else if (targetRow.kind == RowKind::Layer || targetRow.kind == RowKind::MatteStack) {
                if (afterRefs.empty()) {
                  afterRefs.push_back(makeDefaultMatteReference(dragLayerId));
                  changed = true;
                } else {
                  auto updated = replaceSource(afterRefs.front());
                  if (updated.sourceLayerId != beforeRefs.front().sourceLayerId ||
                      updated.enabled != beforeRefs.front().enabled) {
                    afterRefs.front() = updated;
                    changed = true;
                  }
                }
              }

              if (changed) {
                auto* cmd = new ChangeLayerMatteReferencesCommand(targetLayer, std::move(beforeRefs), std::move(afterRefs));
                UndoManager::instance()->push(std::unique_ptr<ChangeLayerMatteReferencesCommand>(cmd));
                updateLayout();
              }
            }
          }
        } else {
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
            int newIndex = static_cast<int>(remainingVisibleLayerIds.size()) -
                           targetVisibleIndex;
            newIndex = std::clamp(newIndex, 0, std::max(0, static_cast<int>(allLayers.size()) - 1));
            if (newIndex != oldIndex) {
              auto layer = comp->layerById(dragLayerId);
              auto* cmd = new MoveLayerIndexCommand(comp, layer, oldIndex, newIndex);
              UndoManager::instance()->push(std::unique_ptr<MoveLayerIndexCommand>(cmd));
              updateLayout();
            }
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
  setToolTip(QString());
  QWidget::mouseReleaseEvent(event);
 }

void ArtifactLayerPanelWidget::focusInEvent(QFocusEvent* event)
{
  if (auto* input = InputOperator::instance()) {
    input->setActiveContext(QString::fromLatin1(kLayerPanelContext));
  }
  QWidget::focusInEvent(event);
}

void ArtifactLayerPanelWidget::focusOutEvent(QFocusEvent* event)
{
  if (auto* input = InputOperator::instance()) {
    if (input->activeContext() == QString::fromLatin1(kLayerPanelContext)) {
      input->setActiveContext(QStringLiteral("Global"));
    }
  }
  QWidget::focusOutEvent(event);
}

void ArtifactLayerPanelWidget::keyPressEvent(QKeyEvent* event)
{
  if (auto* input = InputOperator::instance()) {
    input->setActiveContext(QString::fromLatin1(kLayerPanelContext));
    if (event && input->processKeyPress(this, event->key(), event->modifiers())) {
      event->accept();
      return;
    }
  }

  if ((event->modifiers() & Qt::ControlModifier) &&
      !(event->modifiers() & (Qt::ShiftModifier | Qt::AltModifier | Qt::MetaModifier))) {
    if (event->key() == Qt::Key_Z) {
      if (auto *mgr = UndoManager::instance()) {
        mgr->undo();
        event->accept();
        return;
      }
    } else if (event->key() == Qt::Key_Y) {
      if (auto *mgr = UndoManager::instance()) {
        mgr->redo();
        event->accept();
        return;
      }
    } else if (event->key() == Qt::Key_D) {
      if (auto* svc = ArtifactProjectService::instance()) {
        auto comp = safeCompositionLookup(impl_->compositionId);
        const CompositionID compId = comp ? comp->id() : impl_->compositionId;
        if (!compId.isNil()) {
          const QVector<LayerID> selectedIds =
              selectedLayerIdsInVisibleOrder(impl_->visibleRows);
          if (!selectedIds.isEmpty()) {
            for (const auto& layerId : selectedIds) {
              svc->duplicateLayerInCurrentComposition(layerId);
            }
            updateLayout();
            event->accept();
            return;
          }
        }
      }
    } else if (event->key() == Qt::Key_G) {
      if (auto* svc = ArtifactProjectService::instance()) {
        auto comp = safeCompositionLookup(impl_->compositionId);
        const CompositionID compId = comp ? comp->id() : impl_->compositionId;
        if (!compId.isNil()) {
          if (svc->groupSelectedLayersInCurrentComposition()) {
            updateLayout();
            event->accept();
            return;
          }
        }
      }
    }
  }

  if (event->matches(QKeySequence::SelectAll)) {
    auto* selectionManager = currentLayerSelectionManager();
    auto comp = safeCompositionLookup(impl_->compositionId);
    if (selectionManager && comp) {
      QVector<LayerID> visibleLayerIds;
      visibleLayerIds.reserve(impl_->visibleRows.size());
      for (const auto& row : impl_->visibleRows) {
        if (row.kind == RowKind::Layer && row.layer) {
          visibleLayerIds.push_back(row.layer->id());
        }
      }

      if (!visibleLayerIds.isEmpty()) {
        selectionManager->clearSelection();
        for (const auto& layerId : visibleLayerIds) {
          selectionManager->addToSelection(comp->layerById(layerId));
        }
        impl_->selectedMaskLayerId = LayerID();
        impl_->selectedMaskIndex = -1;
        impl_->selectedLayerId = visibleLayerIds.last();
        impl_->selectionAnchorLayerId = visibleLayerIds.first();
        impl_->currentPropertyPath.clear();
        propertyFocusChanged(impl_->selectedLayerId, impl_->currentPropertyPath);
        update();
        event->accept();
        return;
      }
    }
  }

  auto toggleSelectedSolo = [this]() {
    if (impl_->selectedLayerId.isNil()) return;
    auto* svc = ArtifactProjectService::instance();
    if (!svc) return;
    auto comp = safeCompositionLookup(impl_->compositionId);
    const CompositionID compId = comp ? comp->id() : impl_->compositionId;
    if (compId.isNil()) return;
    auto layer = comp->layerById(impl_->selectedLayerId);
    if (!layer) return;
    svc->setLayerSoloInCurrentComposition(impl_->selectedLayerId, !layer->isSolo());
    updateLayout();
  };
  auto toggleSelectedLock = [this]() {
    if (impl_->selectedLayerId.isNil()) return;
    auto* svc = ArtifactProjectService::instance();
    if (!svc) return;
    auto comp = safeCompositionLookup(impl_->compositionId);
    const CompositionID compId = comp ? comp->id() : impl_->compositionId;
    if (compId.isNil()) return;
    auto layer = comp->layerById(impl_->selectedLayerId);
    if (!layer) return;
    svc->setLayerLockedInCurrentComposition(impl_->selectedLayerId, !layer->isLocked());
    updateLayout();
  };
  auto toggleSelectedShy = [this]() {
    if (impl_->selectedLayerId.isNil()) return;
    auto* svc = ArtifactProjectService::instance();
    if (!svc) return;
    auto comp = safeCompositionLookup(impl_->compositionId);
    const CompositionID compId = comp ? comp->id() : impl_->compositionId;
    if (compId.isNil()) return;
    auto layer = comp->layerById(impl_->selectedLayerId);
    if (!layer) return;
    svc->setLayerShyInCurrentComposition(impl_->selectedLayerId, !layer->isShy());
    updateLayout();
  };

  if (!(event->modifiers() & (Qt::ControlModifier | Qt::AltModifier | Qt::ShiftModifier | Qt::MetaModifier))) {
    if (event->key() == Qt::Key_S) {
      toggleSelectedSolo();
      event->accept();
      return;
    }
    if (event->key() == Qt::Key_L) {
      toggleSelectedLock();
      event->accept();
      return;
    }
    if (event->key() == Qt::Key_P) {
      toggleSelectedShy();
      event->accept();
      return;
    }
  }

  if (event->key() == Qt::Key_U &&
      !(event->modifiers() & (Qt::ControlModifier | Qt::AltModifier | Qt::ShiftModifier | Qt::MetaModifier))) {
    const auto nextMode =
        impl_->displayMode == TimelineLayerDisplayMode::ImportantAndKeyframed
            ? TimelineLayerDisplayMode::AllLayers
            : TimelineLayerDisplayMode::ImportantAndKeyframed;
    setDisplayMode(nextMode);
    event->accept();
    return;
  }

  if (event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace) {
    if (!impl_->selectedMaskLayerId.isNil() &&
        impl_->selectedMaskIndex >= 0 &&
        impl_->selectedMaskLayerId == impl_->selectedLayerId) {
      auto comp = safeCompositionLookup(impl_->compositionId);
      const CompositionID compId = comp ? comp->id() : impl_->compositionId;
      auto layer = comp ? comp->layerById(impl_->selectedLayerId) : ArtifactAbstractLayerPtr{};
      if (!compId.isNil() && layer &&
          impl_->selectedMaskIndex < layer->maskCount()) {
        std::vector<LayerMask> beforeMasks;
        beforeMasks.reserve(static_cast<size_t>(layer->maskCount()));
        for (int i = 0; i < layer->maskCount(); ++i) {
          beforeMasks.push_back(layer->mask(i));
        }

        layer->removeMask(impl_->selectedMaskIndex);

        std::vector<LayerMask> afterMasks;
        afterMasks.reserve(static_cast<size_t>(layer->maskCount()));
        for (int i = 0; i < layer->maskCount(); ++i) {
          afterMasks.push_back(layer->mask(i));
        }

        if (auto *undo = UndoManager::instance()) {
          undo->push(std::make_unique<MaskEditCommand>(layer, std::move(beforeMasks),
                                                       std::move(afterMasks)));
        }
        const int nextMaskCount = layer->maskCount();
        if (nextMaskCount > 0) {
          const int nextMaskIndex =
              std::min(impl_->selectedMaskIndex, nextMaskCount - 1);
          impl_->focusMaskSelection(layer, nextMaskIndex);
          propertyFocusChanged(impl_->selectedLayerId, impl_->currentPropertyPath);
        } else {
          impl_->clearMaskSelection();
          impl_->currentPropertyPath.clear();
          propertyFocusChanged(impl_->selectedLayerId, impl_->currentPropertyPath);
        }
        updateLayout();
        event->accept();
        return;
      }
    }
    QVector<LayerID> selectedIds = selectedLayerIdsSnapshot();
    if (selectedIds.isEmpty() && !impl_->selectedLayerId.isNil()) {
      selectedIds.push_back(impl_->selectedLayerId);
    }
    if (!selectedIds.isEmpty()) {
      if (auto* service = ArtifactProjectService::instance()) {
        auto comp = safeCompositionLookup(impl_->compositionId);
        const CompositionID compId = comp ? comp->id() : impl_->compositionId;
        if (!compId.isNil()) {
          if (auto* selectionManager = currentLayerSelectionManager()) {
            selectionManager->clearSelection();
          }
          impl_->clearMaskSelection();
          impl_->selectedLayerId = LayerID();
          impl_->currentPropertyPath.clear();
          propertyFocusChanged(impl_->selectedLayerId, impl_->currentPropertyPath);
          impl_->clearDragState();
          for (const auto& layerId : selectedIds) {
            service->removeLayerFromComposition(compId, layerId);
          }
          updateLayout();
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
  } else if (event->key() == Qt::Key_Escape) {
   if (auto* selectionManager = currentLayerSelectionManager()) {
    selectionManager->clearSelection();
   }
   impl_->selectedMaskLayerId = LayerID();
   impl_->selectedMaskIndex = -1;
   impl_->selectedLayerId = LayerID();
   impl_->currentPropertyPath.clear();
   propertyFocusChanged(impl_->selectedLayerId, impl_->currentPropertyPath);
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
      p.drawText(rect(), Qt::AlignCenter, "Open a composition to view layers");
      return;
    }
    p.setPen(text.darker(120));
    p.drawText(rect(), Qt::AlignCenter, "No layers yet");
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
    const bool isGroupRow = (row.kind == RowKind::Group ||
                            row.kind == RowKind::MaskStack ||
                            row.kind == RowKind::MatteStack);
    const bool isPropertyRow = (row.kind == RowKind::Property);
    const bool isDisplayLeafRow = (row.kind == RowKind::Mask || row.kind == RowKind::Matte);
    const bool sel = isLayerSelectedInSelectionManager(l->id());
    const bool layerSelected = sel && row.kind == RowKind::Layer;
    const bool selectionAnchor =
        layerSelected && !impl_->selectionAnchorLayerId.isNil() &&
        l->id() == impl_->selectionAnchorLayerId;
    const bool maskSelected = sel && row.kind == RowKind::Mask &&
                              impl_->selectedMaskLayerId == l->id() &&
                              impl_->selectedMaskIndex == row.propertyPath.trimmed().toInt();
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
    } else if (maskSelected) {
      p.fillRect(0, y, width(), rowH, mixColor(background, accent, 0.30));
    } else if (layerSelected) {
      p.fillRect(0, y, width(), rowH, rowSelected); // Modo-like Amber selection
    }
    else if (i == impl_->hoveredLayerIndex) p.fillRect(0, y, width(), rowH, rowHover); // Subtle grey hover
    else p.fillRect(0, y, width(), rowH, rowBase);

    if (impl_->dragMatteLinkMode && i == impl_->dragMatteHoverVisibleRow) {
      const bool validMatteTarget =
          (row.kind == RowKind::Layer || row.kind == RowKind::MatteStack || row.kind == RowKind::Matte);
      if (validMatteTarget) {
        p.fillRect(0, y, width(), rowH, mixColor(background, accent, 0.22));
        p.fillRect(0, y, 4, rowH, accent);
      }
    }

    if (isPropertyRow) {
      if (propertyFocused) {
        QColor focusStrip = accent;
        focusStrip.setAlpha(propertyKeyframed ? 180 : 110);
        p.fillRect(0, y, 3, rowH, focusStrip);
      } else if (propertyKeyframed) {
        QColor keyStrip = accent.lighter(120);
        keyStrip.setAlpha(96);
        p.fillRect(0, y, 2, rowH, keyStrip);
      }
    }

    if (maskSelected) {
      p.fillRect(0, y, 4, rowH, accent);
    } else if (selectionAnchor) {
      QColor anchorColor = mixColor(background, accent, 0.90);
      anchorColor.setAlpha(230);
      p.fillRect(0, y, 3, rowH, anchorColor);
    }

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
      const QString groupAux = row.auxiliaryText.trimmed();
      if (!groupAux.isEmpty()) {
        const QFontMetrics fm(p.font());
        const int badgeW = std::min(96, std::max(54, fm.horizontalAdvance(groupAux) + 16));
        const QRect badgeRect(width() - badgeW - 8, y + 5, badgeW, rowH - 10);
        p.setPen(layerSelected ? accent.darker(180) : border);
        p.setBrush(toneBadgeFill(row.auxiliaryTone, background, surface, accent));
        p.drawRoundedRect(badgeRect, 4, 4);
        p.setPen(toneBadgeText(row.auxiliaryTone, text, accent));
        p.drawText(badgeRect.adjusted(8, 0, -8, 0), Qt::AlignVCenter | Qt::AlignLeft,
                   fm.elidedText(groupAux, Qt::ElideRight, badgeRect.width() - 16));
      }
      p.setPen(maskSelected ? accent.lighter(135) : text);
      const int groupTextWidth = std::max(20, width() - textX - 8 - (groupAux.isEmpty() ? 0 : 100));
      p.drawText(textX, y, groupTextWidth, rowH, Qt::AlignVCenter | Qt::AlignLeft, row.label);
      continue;
    }

    int curX = 0;
    if (!isPropertyRow && !isDisplayLeafRow) {
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
    if ((row.kind == RowKind::Layer || isGroupRow) && row.hasChildren) {
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
    int textX = nameX + ((row.hasChildren && (row.kind == RowKind::Layer || isGroupRow)) ? 16 : 4);
    if (row.kind == RowKind::Layer) {
      const QRect layerTypeIconRect(textX, y + (rowH - kLayerTypeIconSize) / 2,
                                    kLayerTypeIconSize, kLayerTypeIconSize);
      const QIcon& layerTypeIcon = impl_->iconForLayerKind(layerIconKindForLayer(l));
      if (!layerTypeIcon.isNull()) {
        p.setOpacity(layerSelected ? 1.0 : 0.90);
        p.drawPixmap(layerTypeIconRect, layerTypeIcon.pixmap(layerTypeIconRect.size()));
        p.setOpacity(1.0);
      } else {
        p.setPen(QPen(layerSelected ? accent.darker(180) : border, 1.0));
        p.setBrush(mixColor(background, accent, 0.24));
        p.drawRoundedRect(layerTypeIconRect.adjusted(1, 1, -1, -1), 2, 2);
      }
      textX += kLayerTypeIconSize + kLayerTypeIconGap;
      p.setPen(text);
    }
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
    } else if (isDisplayLeafRow) {
     const int leafBadgeW = 84;
     const QRect badgeRect(width() - leafBadgeW - 8, y + 5, leafBadgeW, rowH - 10);
     p.setPen(layerSelected ? accent.darker(180) : border);
     p.setBrush(mixColor(background, surface, 0.30));
     p.drawRoundedRect(badgeRect, 4, 4);
     p.setPen(text.darker(120));
     p.drawText(badgeRect.adjusted(8, 0, -8, 0), Qt::AlignVCenter | Qt::AlignLeft,
                row.auxiliaryText.isEmpty()
                    ? (row.kind == RowKind::Mask ? QStringLiteral("Mask")
                                                 : QStringLiteral("Matte"))
                    : row.auxiliaryText);
      const int labelWidth = std::max(20, badgeRect.left() - textX - 10);
      p.setPen(text);
      p.drawText(textX + 4, y, labelWidth, rowH, Qt::AlignVCenter | Qt::AlignLeft, row.label);
    } else {
     const auto matteRefs = l->matteReferences();
     const bool hasMatteRefs = !matteRefs.empty();
     bool matteBroken = false;
     if (hasMatteRefs) {
      auto comp = safeCompositionLookup(impl_->compositionId);
      for (const auto& ref : matteRefs) {
       if (ref.sourceLayerId.isNil() || ref.sourceLayerId == l->id()) {
        matteBroken = true;
        break;
       }
       auto sourceLayer = comp ? comp->layerById(ref.sourceLayerId) : ArtifactAbstractLayerPtr{};
       if (!sourceLayer || matteDropWouldCreateCycle(comp, sourceLayer, l->id())) {
        matteBroken = true;
        break;
       }
      }
     }
     const auto variants = l->getVariants();
     const int variantChipW = kVariantChipWidth;
     const QFontMetrics fm(p.font());
     const int iconGap = hasMatteRefs ? 18 : 0;
     const QString layerName = l->layerName();
     const QString layerAux = row.auxiliaryText.trimmed();
     const QString matteBadgeText = hasMatteRefs ? matteSourceBadgeLabel(safeCompositionLookup(impl_->compositionId), l)
                                                 : QString();
     const int matteBadgeW = hasMatteRefs
                                 ? std::min(160, std::max(66, fm.horizontalAdvance(matteBadgeText) + 16))
                                 : 0;
     const int matteBadgeX = textX + 4 + 18;
     const QRect matteBadgeRect(matteBadgeX, y + 5, matteBadgeW, rowH - 10);
     const int layerTextX = hasMatteRefs ? (matteBadgeRect.right() + 8) : (textX + 4 + iconGap);
     if (hasMatteRefs) {
      const QRect iconRect(textX + 4, y + 6, 14, 14);
      const QIcon& matteIcon = matteBroken ? impl_->iconLinkOff : impl_->iconLink;
      if (!matteIcon.isNull()) {
        p.setOpacity(matteBroken ? 0.95 : 1.0);
        p.drawPixmap(iconRect, matteIcon.pixmap(iconRect.size()));
        p.setOpacity(1.0);
      } else {
        p.setPen(QPen(matteBroken ? accent.darker(120) : border, 1.2));
        p.setBrush(mixColor(background, accent, matteBroken ? 0.18 : 0.10));
        p.drawEllipse(iconRect);
      }
      p.setPen(matteBroken ? accent.darker(130) : text.darker(118));
      p.setBrush(matteBroken ? mixColor(background, accent, 0.20) : mixColor(background, surface, 0.26));
      p.drawRoundedRect(matteBadgeRect, 4, 4);
      p.setPen(matteBroken ? accent.lighter(120) : text);
      p.drawText(matteBadgeRect.adjusted(8, 0, -8, 0), Qt::AlignVCenter | Qt::AlignLeft,
                 fm.elidedText(matteBadgeText, Qt::ElideRight, matteBadgeRect.width() - 16));
     }
     if (!layerAux.isEmpty()) {
      const int badgeTextWidth = fm.horizontalAdvance(layerAux) + 16;
      const int badgeWidth = std::min(120, std::max(52, badgeTextWidth));
      const int badgeX = std::max(layerTextX, width() - (showInlineCombos ? kInlineComboReserve : 0) - variantChipW - badgeWidth - 10);
      const QRect badgeRect(badgeX, y + 5, badgeWidth, rowH - 10);
      const int nameWidth = std::max(20, badgeRect.left() - (layerTextX + 4));
      const QString elidedName = fm.elidedText(layerName, Qt::ElideRight, nameWidth);
      p.setPen(maskSelected ? accent.lighter(135) : text);
      p.drawText(layerTextX, y, nameWidth, rowH, Qt::AlignVCenter | Qt::AlignLeft, elidedName);
      p.setPen(layerSelected ? accent.darker(180) : border);
      p.setBrush(toneBadgeFill(row.auxiliaryTone, background, surface, accent));
      p.drawRoundedRect(badgeRect, 4, 4);
      p.setPen(toneBadgeText(row.auxiliaryTone, text, accent));
      p.drawText(badgeRect.adjusted(8, 0, -8, 0), Qt::AlignVCenter | Qt::AlignLeft,
                 fm.elidedText(layerAux, Qt::ElideRight, badgeRect.width() - 16));
     } else {
      p.setPen(maskSelected ? accent.lighter(135) : text);
      const int nameWidth = std::max(20, width() - layerTextX - variantChipW - 10);
      p.drawText(layerTextX, y, nameWidth, rowH, Qt::AlignVCenter | Qt::AlignLeft, layerName);
     }

     const QRect chipRect = variantChipRect(QRect(0, y, width(), rowH), l,
                                            showInlineCombos, rowH);
     const QString chipText = QStringLiteral("%1 ▾").arg(variantChipText(l));
     p.save();
     p.setFont(QFont("Inter", 8, QFont::Bold));
     p.setBrush(surface);
     p.setPen(layerSelected ? accent.darker(180) : border);
     p.drawRoundedRect(chipRect, 4, 4);
     p.setPen(text);
     const QFontMetrics chipFm(p.font());
     const QString elidedChip = chipFm.elidedText(chipText, Qt::ElideRight, chipRect.width() - 8);
     p.drawText(chipRect.adjusted(6, 0, -6, 0), Qt::AlignVCenter | Qt::AlignLeft, elidedChip);
     p.restore();
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

  const int selectedCount = static_cast<int>(selectedLayerIdsSnapshot().size());
  if (selectedCount > 0) {
    const QString badgeText = QStringLiteral("%1 selected").arg(selectedCount);
    const QFontMetrics fm(p.font());
    const int badgePadX = 10;
    const int badgePadY = 5;
    const int badgeH = fm.height() + badgePadY * 2;
    const int badgeW = fm.horizontalAdvance(badgeText) + badgePadX * 2;
    const int badgeX = std::max(8, width() - badgeW - 10);
    const QRect badgeRect(badgeX, 8, badgeW, badgeH);
    QColor badgeBg = mixColor(background, accent, 0.52);
    badgeBg.setAlpha(230);
    QColor badgeBorder = mixColor(badgeBg, border, 0.40);
    QColor badgeTextColor = themeColor(theme.textColor, QColor(QStringLiteral("#F5F2E8")));

    p.setPen(QPen(badgeBorder, 1.0));
    p.setBrush(badgeBg);
    p.drawRoundedRect(badgeRect, 9, 9);
    p.setPen(badgeTextColor);
    p.drawText(badgeRect.adjusted(badgePadX, 0, -badgePadX, 0),
               Qt::AlignVCenter | Qt::AlignLeft,
               badgeText);
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
  impl_->dragMatteHoverVisibleRow = -1;
  setToolTip(QString());
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

        int newIndex = static_cast<int>(remainingVisibleLayerIds.size()) -
                       targetVisibleIndex;

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

  QStringList validPaths = collectTimelineDroppedPaths(mime);

  if (validPaths.isEmpty()) {
    event->ignore();
    return;
  }

  importTimelineDroppedPaths(validPaths);

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
  setAcceptDrops(true);
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

void ArtifactLayerTimelinePanelWrapper::dragEnterEvent(QDragEnterEvent* event)
{
  const QStringList validPaths = collectTimelineDroppedPaths(event->mimeData());
  if (!validPaths.isEmpty()) {
    event->acceptProposedAction();
    return;
  }
  event->ignore();
}

void ArtifactLayerTimelinePanelWrapper::dragMoveEvent(QDragMoveEvent* event)
{
  const QStringList validPaths = collectTimelineDroppedPaths(event->mimeData());
  if (!validPaths.isEmpty()) {
    event->acceptProposedAction();
    return;
  }
  event->ignore();
}

void ArtifactLayerTimelinePanelWrapper::dropEvent(QDropEvent* event)
{
  const QStringList validPaths = collectTimelineDroppedPaths(event->mimeData());
  if (validPaths.isEmpty()) {
    event->ignore();
    return;
  }
  importTimelineDroppedPaths(validPaths);
  event->acceptProposedAction();
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
