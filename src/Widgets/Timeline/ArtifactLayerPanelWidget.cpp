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
#include <QRegularExpression>
#include <QPolygon>
#include <QIcon>
#include <QtSVG/QSvgRenderer>
#include <QComboBox>
#include <QPointer>
#include <QLineEdit>
#include <QStyledItemDelegate>
#include <QListView>
#include <QKeyEvent>
#include <QWheelEvent>
#include <QInputDialog>
#include <QFileDialog>
#include <QTimer>
#include <QDrag>
#include <optional>
module Artifact.Widgets.LayerPanelWidget;

import std;

import Utils.Path;
import Artifact.Application.Manager;
import Artifact.Service.Project;
import Artifact.Service.Playback;
import Artifact.Layers.Selection.Manager;
import Artifact.Project.Manager;
import Artifact.Composition.Abstract;
import Artifact.Layer.Abstract;
import Artifact.Layer.Image;
import Artifact.Layer.Svg;
import Artifact.Layer.Shape;
import Artifact.Layer.Video;
import Artifact.Layer.Text;
import Artifact.Layer.Solid2D;
import Artifact.Layer.Audio;
import Artifact.Layer.Camera;
import Artifact.Layer.Particle;
import Layer.Blend;
import Artifact.Layer.InitParams;
import File.TypeDetector;

namespace Artifact
{
 using namespace ArtifactCore;
namespace {
  constexpr int kLayerRowHeight = 28;
  constexpr int kLayerHeaderHeight = 26;
  constexpr int kLayerHeaderButtonSize = 24;
  constexpr int kLayerColumnWidth = 28;
  constexpr int kLayerPropertyColumnCount = 5;
  constexpr int kInlineComboHeight = 26;
  constexpr int kInlineBlendWidth = 140;
  constexpr int kInlineParentWidth = 172;
  constexpr int kInlineComboGap = 6;
  constexpr int kInlineComboMarginY = 2;
  constexpr int kInlineComboReserve = kInlineParentWidth + kInlineBlendWidth + kInlineComboGap + 10;
  constexpr int kLayerNameMinWidth = 120;
  constexpr char kLayerReorderMimeType[] = "application/x-artifact-layer-reorder";

 QString makeTimelineButtonStyle(const QWidget* widget, bool checkedAccent = false)
 {
  const QPalette pal = widget ? widget->palette() : QPalette{};
  const QColor bg = pal.button().color();
  const QColor fg = pal.buttonText().color();
  const QColor border = pal.mid().color().darker(120);
  const QColor checkedBg = checkedAccent ? pal.highlight().color() : bg.darker(108);
  const QColor hoverBg = bg.lighter(110);
  const QColor pressedBg = bg.darker(115);
  return QStringLiteral(
      "QPushButton { background:%1; color:%2; border:none; border-right:1px solid %3; font-size:11px; }"
      "QPushButton:hover { background:%4; }"
      "QPushButton:pressed { background:%5; }"
      "QPushButton:checked { background:%6; color:%7; }")
      .arg(bg.name(), fg.name(), border.name(), hoverBg.name(), pressedBg.name(),
           checkedBg.name(), pal.highlightedText().color().name());
 }

 QString makeTimelineLineEditStyle(const QWidget* widget)
 {
  const QPalette pal = widget ? widget->palette() : QPalette{};
  const QColor bg = pal.base().color();
  const QColor fg = pal.text().color();
  const QColor border = pal.mid().color().darker(120);
  return QStringLiteral("QLineEdit { background:%1; color:%2; border:1px solid %3; padding:0 4px; }")
      .arg(bg.name(), fg.name(), border.name());
 }

 QString makeTimelineComboStyle(const QWidget* widget)
 {
  const QPalette pal = widget ? widget->palette() : QPalette{};
  const QColor bg = pal.button().color();
  const QColor fg = pal.buttonText().color();
  const QColor border = pal.mid().color().darker(120);
 return QStringLiteral(
      "QComboBox { background:%1; color:%2; border:1px solid %3; padding:2px 8px; min-height:22px; font-size:11px; }"
      "QComboBox::drop-down { width:20px; border-left:1px solid %3; }"
      "QComboBox QAbstractItemView { font-size:11px; }")
      .arg(bg.name(), fg.name(), border.name());
 }

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

 // レイヤータイプ別の色を取得
 QColor getLayerTypeColor(const ArtifactAbstractLayerPtr& layer)
 {
  if (!layer) return QColor(128, 128, 128);  // Gray - null
  
  if (auto img = std::dynamic_pointer_cast<ArtifactImageLayer>(layer)) {
   return QColor(100, 149, 237);  // CornflowerBlue - 画像
  }
  if (auto video = std::dynamic_pointer_cast<ArtifactVideoLayer>(layer)) {
   return QColor(255, 165, 0);    // Orange - 動画
  }
  if (auto text = std::dynamic_pointer_cast<ArtifactTextLayer>(layer)) {
   return QColor(50, 205, 50);    // LimeGreen - テキスト
  }
  if (auto solid = std::dynamic_pointer_cast<ArtifactSolid2DLayer>(layer)) {
   return QColor(219, 112, 147);  // PaleVioletRed - ソリッド
  }
  if (std::dynamic_pointer_cast<ArtifactShapeLayer>(layer)) {
   return QColor(255, 170, 64);  // Warm orange - シェイプ
  }
  if (auto svg = std::dynamic_pointer_cast<ArtifactSvgLayer>(layer)) {
   return QColor(147, 112, 219);  // MediumPurple - SVG
  }
  if (auto audio = std::dynamic_pointer_cast<ArtifactAudioLayer>(layer)) {
   return QColor(255, 215, 0);    // Gold - オーディオ
  }
  if (auto camera = std::dynamic_pointer_cast<ArtifactCameraLayer>(layer)) {
   return QColor(0, 255, 255);    // Cyan - カメラ
  }
  if (auto particle = std::dynamic_pointer_cast<ArtifactParticleLayer>(layer)) {
   return QColor(255, 105, 180);  // HotPink - パーティクル
  }
  
  return QColor(128, 128, 128);  // Gray - その他
 }

 bool layerCanOutputAudio(const ArtifactAbstractLayerPtr& layer)
 {
  if (!layer || !layer->hasAudio()) {
   return false;
  }
  if (auto audio = std::dynamic_pointer_cast<ArtifactAudioLayer>(layer)) {
   return !audio->isMuted() && audio->volume() > 0.001f;
  }
  if (auto video = std::dynamic_pointer_cast<ArtifactVideoLayer>(layer)) {
   return !video->isAudioMuted() && video->audioVolume() > 0.001;
  }
  return false;
 }

bool layerHasAnimation(const ArtifactAbstractLayerPtr& layer)
{
  if (!layer) {
    return false;
  }
  const auto groups = layer->getLayerPropertyGroups();
  for (const auto& group : groups) {
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

bool layerHasImportantTimelineState(const ArtifactAbstractLayerPtr& layer)
{
  if (!layer) {
    return false;
  }

  const auto& transform = layer->transform3D();
  const bool hasTransformOffset =
      std::abs(transform.positionX()) > 0.0001 ||
      std::abs(transform.positionY()) > 0.0001 ||
      std::abs(transform.scaleX() - 1.0f) > 0.0001 ||
      std::abs(transform.scaleY() - 1.0f) > 0.0001 ||
      std::abs(transform.rotation()) > 0.0001 ||
      std::abs(transform.anchorX()) > 0.0001 ||
      std::abs(transform.anchorY()) > 0.0001;

  const bool hasOpacityOffset = std::abs(layer->opacity() - 1.0f) > 0.0001f;
  const bool hasTimelineOffset =
      layer->inPoint().framePosition() != 0 ||
      layer->startTime().framePosition() != 0;

  return hasTransformOffset || hasOpacityOffset || hasTimelineOffset;
}

bool layerMatchesDisplayMode(const ArtifactAbstractLayerPtr& layer,
                              TimelineLayerDisplayMode mode,
                              const QSet<ArtifactAbstractLayerPtr>& selectedLayers)
 {
  if (!layer) {
   return false;
  }
  switch (mode) {
  case TimelineLayerDisplayMode::AllLayers:
    return true;
  case TimelineLayerDisplayMode::SelectedOnly:
    return selectedLayers.contains(layer);
  case TimelineLayerDisplayMode::AnimatedOnly:
    return layerHasAnimation(layer);
  case TimelineLayerDisplayMode::ImportantAndKeyframed:
    return layerHasAnimation(layer) || layerHasImportantTimelineState(layer);
  case TimelineLayerDisplayMode::AudioOnly:
    return layerCanOutputAudio(layer);
  case TimelineLayerDisplayMode::VideoOnly:
    return std::dynamic_pointer_cast<ArtifactVideoLayer>(layer) != nullptr;
  }
  return true;
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

 struct LayerSearchQuery
 {
  QString rawText;
  QStringList freeTerms;
  QStringList typeTerms;
  QStringList fxTerms;
  QStringList tagTerms;
  QStringList parentTerms;
  QStringList sourceTerms;
  std::optional<bool> visible;
  std::optional<bool> locked;
  std::optional<bool> solo;
  std::optional<bool> shy;

  bool isEmpty() const
  {
   return rawText.trimmed().isEmpty();
  }
 };

 QString normalizeTokenValue(QString value)
 {
  value = value.trimmed();
  if (value.startsWith('"') && value.endsWith('"') && value.size() >= 2) {
   value = value.mid(1, value.size() - 2);
  }
  return value.trimmed();
 }

 std::optional<bool> parseBoolToken(const QString& value)
 {
  const QString lower = value.trimmed().toLower();
  if (lower.isEmpty()) {
   return std::nullopt;
  }
  if (lower == QStringLiteral("1") || lower == QStringLiteral("true") ||
      lower == QStringLiteral("yes") || lower == QStringLiteral("on")) {
   return true;
  }
  if (lower == QStringLiteral("0") || lower == QStringLiteral("false") ||
      lower == QStringLiteral("no") || lower == QStringLiteral("off")) {
   return false;
  }
  return std::nullopt;
 }

 bool containsInsensitive(const QString& haystack, const QString& needle)
 {
  return needle.isEmpty() || haystack.contains(needle, Qt::CaseInsensitive);
 }

 QString layerTypeName(const ArtifactAbstractLayerPtr& layer)
 {
  if (!layer) {
   return {};
  }
  if (layer->isNullLayer()) {
   return QStringLiteral("null");
  }
  if (layer->isAdjustmentLayer()) {
   return QStringLiteral("adjustment");
  }
  if (std::dynamic_pointer_cast<ArtifactTextLayer>(layer)) {
   return QStringLiteral("text");
  }
  if (std::dynamic_pointer_cast<ArtifactImageLayer>(layer)) {
    return QStringLiteral("image");
  }
  if (std::dynamic_pointer_cast<ArtifactShapeLayer>(layer)) {
    return QStringLiteral("shape");
  }
  if (std::dynamic_pointer_cast<ArtifactSvgLayer>(layer)) {
    return QStringLiteral("svg");
  }
  if (std::dynamic_pointer_cast<ArtifactVideoLayer>(layer)) {
   return QStringLiteral("video");
  }
  if (std::dynamic_pointer_cast<ArtifactAudioLayer>(layer)) {
   return QStringLiteral("audio");
  }
  if (std::dynamic_pointer_cast<ArtifactCameraLayer>(layer)) {
   return QStringLiteral("camera");
  }
  if (std::dynamic_pointer_cast<ArtifactParticleLayer>(layer)) {
   return QStringLiteral("particle");
  }
  if (std::dynamic_pointer_cast<ArtifactSolid2DLayer>(layer)) {
   return QStringLiteral("solid");
  }
  return QStringLiteral("layer");
 }

 QString sourceAssetName(const ArtifactAbstractLayerPtr& layer)
 {
  if (!layer) {
   return {};
  }
  if (auto image = std::dynamic_pointer_cast<ArtifactImageLayer>(layer)) {
   return QFileInfo(image->sourcePath()).fileName();
  }
  if (auto video = std::dynamic_pointer_cast<ArtifactVideoLayer>(layer)) {
   return QFileInfo(video->sourcePath()).fileName();
  }
  if (auto audio = std::dynamic_pointer_cast<ArtifactAudioLayer>(layer)) {
   return QFileInfo(audio->sourcePath()).fileName();
  }
  if (auto svg = std::dynamic_pointer_cast<ArtifactSvgLayer>(layer)) {
   return QFileInfo(svg->sourcePath()).fileName();
  }
  return {};
 }

 QString humanizePropertyName(QString value)
 {
  value = value.trimmed();
  if (value.isEmpty()) {
   return value;
  }
  value.replace(QStringLiteral("."), QStringLiteral(" "));
  value.replace(QStringLiteral("_"), QStringLiteral(" "));
  value.replace(QRegularExpression(QStringLiteral("([a-z0-9])([A-Z])")),
                QStringLiteral("\\1 \\2"));
  value.replace(QRegularExpression(QStringLiteral("\\s+")),
                QStringLiteral(" "));
  if (!value.isEmpty()) {
   value[0] = value[0].toUpper();
  }
  return value;
 }

 QString parentLayerName(const ArtifactAbstractLayerPtr& layer, const ArtifactCompositionPtr& comp)
 {
  if (!layer || !comp || !layer->hasParent()) {
   return {};
  }
  const auto parent = comp->layerById(layer->parentLayerId());
  return parent ? parent->layerName() : QString();
 }

 bool matchesSearchToken(const ArtifactAbstractLayerPtr& layer,
                         const ArtifactCompositionPtr& comp,
                         const QString& token,
                         const QString& tokenLower)
 {
  Q_UNUSED(token);
  if (!layer) {
   return false;
  }

  const QString name = layer->layerName();
  const QString type = layerTypeName(layer);
  const QString fxName = [&layer]() {
   QStringList names;
   for (const auto& effect : layer->getEffects()) {
    if (effect) {
     names << effect->displayName().toQString();
    }
   }
   return names.join(u' ');
  }();
  const QString tagBlob = [&layer]() {
   QStringList tags;
   for (const auto& group : layer->getLayerPropertyGroups()) {
    const QString groupName = group.name().trimmed();
    if (!groupName.isEmpty()) {
     tags << groupName;
    }
    for (const auto& property : group.sortedProperties()) {
     if (property) {
      const QString propName = property->getName().trimmed();
      if (!propName.isEmpty()) {
       tags << propName;
      }
     }
    }
   }
   return tags.join(u' ');
  }();
  const QString parentName = parentLayerName(layer, comp);
  const QString sourceName = sourceAssetName(layer);
  const QString stateBlob = QStringLiteral("%1 %2 %3 %4")
                               .arg(layer->isVisible() ? QStringLiteral("visible") : QStringLiteral("hidden"))
                               .arg(layer->isLocked() ? QStringLiteral("locked") : QStringLiteral("unlocked"))
                               .arg(layer->isSolo() ? QStringLiteral("solo") : QStringLiteral("normal"))
                               .arg(layer->isShy() ? QStringLiteral("shy") : QStringLiteral("notshy"));

  return containsInsensitive(name, token) ||
         containsInsensitive(type, tokenLower) ||
         containsInsensitive(fxName, tokenLower) ||
         containsInsensitive(tagBlob, tokenLower) ||
         containsInsensitive(parentName, tokenLower) ||
         containsInsensitive(sourceName, tokenLower) ||
         containsInsensitive(stateBlob, tokenLower);
 }

 LayerSearchQuery parseLayerSearchQuery(const QString& text)
 {
  LayerSearchQuery query;
  query.rawText = text;
  const QStringList tokens = text.split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
  for (const QString& rawToken : tokens) {
   const QString token = normalizeTokenValue(rawToken);
   if (token.isEmpty()) {
    continue;
   }

   const int colon = token.indexOf(u':');
   if (colon <= 0) {
    query.freeTerms.push_back(token);
    continue;
   }

   const QString key = token.left(colon).trimmed().toLower();
   const QString value = normalizeTokenValue(token.mid(colon + 1));
   if (key == QStringLiteral("type")) {
    query.typeTerms.push_back(value);
   } else if (key == QStringLiteral("fx") || key == QStringLiteral("effect")) {
    query.fxTerms.push_back(value);
   } else if (key == QStringLiteral("tag")) {
    query.tagTerms.push_back(value);
   } else if (key == QStringLiteral("parent")) {
    query.parentTerms.push_back(value);
   } else if (key == QStringLiteral("source")) {
    query.sourceTerms.push_back(value);
   } else if (key == QStringLiteral("visible")) {
    query.visible = parseBoolToken(value);
   } else if (key == QStringLiteral("locked") || key == QStringLiteral("lock")) {
    query.locked = parseBoolToken(value);
   } else if (key == QStringLiteral("solo")) {
    query.solo = parseBoolToken(value);
   } else if (key == QStringLiteral("shy")) {
    query.shy = parseBoolToken(value);
   } else {
    query.freeTerms.push_back(token);
   }
  }
  return query;
 }

 bool matchesLayerSearchQuery(const ArtifactAbstractLayerPtr& layer,
                              const ArtifactCompositionPtr& comp,
                              const LayerSearchQuery& query)
 {
  if (!layer) {
   return false;
  }

  if (query.visible.has_value() && layer->isVisible() != query.visible.value()) {
   return false;
  }
  if (query.locked.has_value() && layer->isLocked() != query.locked.value()) {
   return false;
  }
  if (query.solo.has_value() && layer->isSolo() != query.solo.value()) {
   return false;
  }
  if (query.shy.has_value() && layer->isShy() != query.shy.value()) {
   return false;
  }

  const QString name = layer->layerName();
  const QString type = layerTypeName(layer);
  const QString parentName = parentLayerName(layer, comp);
  const QString sourceName = sourceAssetName(layer);

  for (const QString& term : query.freeTerms) {
   if (!matchesSearchToken(layer, comp, term, term.toLower())) {
    return false;
   }
  }
  for (const QString& term : query.typeTerms) {
   if (!containsInsensitive(type, term)) {
    return false;
   }
  }
  for (const QString& term : query.fxTerms) {
   bool matched = false;
   for (const auto& effect : layer->getEffects()) {
    if (effect && containsInsensitive(effect->displayName().toQString(), term)) {
     matched = true;
     break;
    }
   }
   if (!matched) {
    return false;
   }
  }
  for (const QString& term : query.tagTerms) {
   bool matched = false;
   for (const auto& group : layer->getLayerPropertyGroups()) {
    if (containsInsensitive(group.name(), term)) {
     matched = true;
     break;
    }
    for (const auto& property : group.sortedProperties()) {
     if (property && containsInsensitive(property->getName(), term)) {
      matched = true;
      break;
     }
    }
    if (matched) {
     break;
    }
   }
   if (!matched) {
    return false;
   }
  }
  for (const QString& term : query.parentTerms) {
   if (term.compare(QStringLiteral("none"), Qt::CaseInsensitive) == 0 ||
       term.compare(QStringLiteral("null"), Qt::CaseInsensitive) == 0) {
    if (layer->hasParent()) {
     return false;
    }
    continue;
   }
   if (!containsInsensitive(parentName, term)) {
    return false;
   }
  }
  for (const QString& term : query.sourceTerms) {
   if (!containsInsensitive(sourceName, term)) {
    return false;
   }
  }

  return true;
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
    switch (event->type()) {
     case QEvent::MouseButtonPress:
     case QEvent::MouseMove:
     case QEvent::MouseButtonRelease: {
      auto* mouseEvent = static_cast<QMouseEvent*>(event);
      QMouseEvent forwardedEvent(
          event->type(),
          target_->mapFromGlobal(sourceWidget->mapToGlobal(mouseEvent->position().toPoint())),
          mouseEvent->button(),
          mouseEvent->buttons(),
          mouseEvent->modifiers());
      QCoreApplication::sendEvent(target_, &forwardedEvent);
      return forwardedEvent.isAccepted();
     }
     case QEvent::DragEnter: {
      auto* dragEvent = static_cast<QDragEnterEvent*>(event);
      QDragEnterEvent forwardedEvent(
          target_->mapFromGlobal(sourceWidget->mapToGlobal(dragEvent->position().toPoint())),
          dragEvent->possibleActions(),
          dragEvent->mimeData(),
          dragEvent->buttons(),
          dragEvent->modifiers());
      QCoreApplication::sendEvent(target_, &forwardedEvent);
      return forwardedEvent.isAccepted();
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
      return forwardedEvent.isAccepted();
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
      return forwardedEvent.isAccepted();
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

  QColor blendModePopupTint(const LAYER_BLEND_TYPE mode)
  {
    switch (mode) {
    case LAYER_BLEND_TYPE::BLEND_ADD:
    case LAYER_BLEND_TYPE::BLEND_SCREEN:
    case LAYER_BLEND_TYPE::BLEND_LIGHTEN:
    case LAYER_BLEND_TYPE::BLEND_COLOR_DODGE:
    case LAYER_BLEND_TYPE::BLEND_HARD_LIGHT:
      return QColor(76, 100, 66, 86);
    case LAYER_BLEND_TYPE::BLEND_MULTIPLY:
    case LAYER_BLEND_TYPE::BLEND_DARKEN:
    case LAYER_BLEND_TYPE::BLEND_COLOR_BURN:
    case LAYER_BLEND_TYPE::BLEND_SOFT_LIGHT:
      return QColor(44, 48, 58, 112);
    case LAYER_BLEND_TYPE::BLEND_OVERLAY:
    case LAYER_BLEND_TYPE::BLEND_DIFFERENCE:
    case LAYER_BLEND_TYPE::BLEND_EXCLUSION:
      return QColor(60, 56, 78, 90);
    default:
      return QColor(48, 48, 52, 92);
    }
  }

  class BlendModePopupDelegate final : public QStyledItemDelegate
  {
  public:
    explicit BlendModePopupDelegate(QObject* parent = nullptr) : QStyledItemDelegate(parent) {}

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override
    {
      QStyleOptionViewItem opt = option;
      initStyleOption(&opt, index);
      const auto mode = static_cast<LAYER_BLEND_TYPE>(index.data(Qt::UserRole).toInt());
      const QColor tint = blendModePopupTint(mode);

      painter->save();
      painter->setRenderHint(QPainter::Antialiasing, true);

      if (opt.state & QStyle::State_Selected) {
        painter->fillRect(opt.rect, QColor(74, 114, 82, 176));
      } else if (opt.state & QStyle::State_MouseOver) {
        painter->fillRect(opt.rect, tint.lighter(120));
      } else {
        painter->fillRect(opt.rect, tint);
      }

      painter->setPen(QPen(QColor(0, 0, 0, 45), 1));
      painter->drawLine(opt.rect.topLeft(), opt.rect.topRight());
      painter->drawLine(opt.rect.bottomLeft(), opt.rect.bottomRight());

      QStyledItemDelegate::paint(painter, opt, index);
      painter->restore();
    }
  };

  QVector<QString> layerPanelPropertyLabels(const ArtifactAbstractLayerPtr& layer,
                                           const TimelinePropertyDisplayMode mode)
  {
   QVector<QString> labels;
   if (!layer) {
    return labels;
   }

   auto hasLabel = [&labels](const QString& candidate) -> bool {
    return std::any_of(labels.cbegin(), labels.cend(), [&candidate](const QString& existing) {
     return existing.compare(candidate, Qt::CaseInsensitive) == 0;
    });
   };

   if (mode == TimelinePropertyDisplayMode::KeyframesOnly) {
    QSet<QString> seen;
    for (const auto& group : layer->getLayerPropertyGroups()) {
     for (const auto& property : group.sortedProperties()) {
      if (!property || !property->isAnimatable()) {
        continue;
      }
      if (property->getKeyFrames().empty()) {
       continue;
      }
      const auto meta = property->metadata();
      const QString propertyName = property->getName().trimmed();
      QString displayName = meta.displayLabel.trimmed();
      if (displayName.isEmpty()) {
       displayName = humanizePropertyName(propertyName);
      }
      if (displayName.isEmpty() || seen.contains(displayName)) {
       continue;
      }
      seen.insert(displayName);
      labels.push_back(displayName);
     }
    }
    return labels;
   }

   for (const auto& group : layer->getLayerPropertyGroups()) {
    const QString groupName = group.name().trimmed();
    if (groupName.isEmpty()) {
     continue;
    }
    if (groupName.compare(QStringLiteral("Layer"), Qt::CaseInsensitive) == 0) {
     continue;
    }
    if (!hasLabel(groupName)) {
     labels.push_back(groupName);
    }
   }

   // Fallback for visual/null-style layers that should expose timeline transform controls.
   const QString transformLabel = QStringLiteral("Transform");
   if ((layer->isNullLayer() || layer->isAdjustmentLayer() || layer->hasVideo()) && !hasLabel(transformLabel)) {
    labels.prepend(transformLabel);
   }

   return labels;
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
  int propertyColumnWidth = kLayerColumnWidth;
  bool resizingPropertyColumns = false;
  int resizeStartX = 0;
  int resizeStartWidth = kLayerColumnWidth;
  static constexpr int resizeHandleHalfWidth = 4;
  
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
  visButton->setIconSize(QSize(14, 14));
  visButton->setStyleSheet(makeTimelineButtonStyle(this));
  visButton->setFlat(true);
  visButton->setEnabled(false);
  visButton->setAttribute(Qt::WA_TransparentForMouseEvents, true);

  auto lockButton = impl_->lockButton = new QPushButton();
  lockButton->setFixedSize(QSize(kLayerColumnWidth, kLayerHeaderButtonSize));
  if (!impl_->lockIcon.isNull()) lockButton->setIcon(impl_->lockIcon);
  lockButton->setIconSize(QSize(14, 14));
  lockButton->setStyleSheet(makeTimelineButtonStyle(this));
  lockButton->setEnabled(false);
  lockButton->setAttribute(Qt::WA_TransparentForMouseEvents, true);

  auto soloButton = impl_->soloButton = new QPushButton();
  soloButton->setFixedSize(QSize(kLayerColumnWidth, kLayerHeaderButtonSize));
  if (!impl_->soloIcon.isNull()) soloButton->setIcon(impl_->soloIcon);
  soloButton->setIconSize(QSize(14, 14));
  soloButton->setStyleSheet(makeTimelineButtonStyle(this));
  soloButton->setEnabled(false);
  soloButton->setAttribute(Qt::WA_TransparentForMouseEvents, true);

  auto audioButton = impl_->audioButton = new QPushButton();
  audioButton->setFixedSize(QSize(kLayerColumnWidth, kLayerHeaderButtonSize));
  if (!impl_->audioIcon.isNull()) audioButton->setIcon(impl_->audioIcon);
  audioButton->setIconSize(QSize(14, 14));
  audioButton->setStyleSheet(makeTimelineButtonStyle(this));
  audioButton->setEnabled(false);
  audioButton->setAttribute(Qt::WA_TransparentForMouseEvents, true);

  auto shyButton = impl_->shyButton = new QPushButton;
  shyButton->setFixedSize(QSize(kLayerColumnWidth, kLayerHeaderButtonSize));
  shyButton->setCheckable(true);
  if (!impl_->shyIcon.isNull()) shyButton->setIcon(impl_->shyIcon);
  shyButton->setIconSize(QSize(14, 14));
  shyButton->setToolTip("Master Shy Switch");
  shyButton->setStyleSheet(makeTimelineButtonStyle(this, true));
  shyButton->setAttribute(Qt::WA_TransparentForMouseEvents, true);

  auto layerNameButton = impl_->layerNameButton = new QPushButton("Layer Name");
  QString btnStyle = makeTimelineButtonStyle(this);
  btnStyle += QStringLiteral("QPushButton { text-align:left; padding-left:5px; }");
  layerNameButton->setStyleSheet(btnStyle);
  layerNameButton->setEnabled(false);
  layerNameButton->setAttribute(Qt::WA_TransparentForMouseEvents, true);
  
  auto parentHeader = impl_->parentHeaderButton = new QPushButton("Parent");
  parentHeader->setFixedWidth(kInlineParentWidth);
  parentHeader->setStyleSheet(makeTimelineButtonStyle(this));
  parentHeader->setEnabled(false);
  parentHeader->setAttribute(Qt::WA_TransparentForMouseEvents, true);
  
  auto blendHeader = impl_->blendHeaderButton = new QPushButton("Blend");
  blendHeader->setFixedWidth(kInlineBlendWidth);
  blendHeader->setStyleSheet(makeTimelineButtonStyle(this));
  blendHeader->setEnabled(false);
  blendHeader->setAttribute(Qt::WA_TransparentForMouseEvents, true);

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
  setFixedHeight(kLayerHeaderHeight);
  setMouseTracking(true);
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

void ArtifactLayerPanelHeaderWidget::setPropertyColumnWidth(int width)
{
  const int clamped = std::clamp(width, 18, 56);
  if (!impl_ || impl_->propertyColumnWidth == clamped) {
    return;
  }
  impl_->propertyColumnWidth = clamped;
  if (impl_->visibilityButton) impl_->visibilityButton->setFixedWidth(clamped);
  if (impl_->lockButton) impl_->lockButton->setFixedWidth(clamped);
  if (impl_->soloButton) impl_->soloButton->setFixedWidth(clamped);
  if (impl_->audioButton) impl_->audioButton->setFixedWidth(clamped);
  if (impl_->shyButton) impl_->shyButton->setFixedWidth(clamped);
  updateGeometry();
  update();
}

int ArtifactLayerPanelHeaderWidget::propertyColumnWidth() const
{
  return impl_ ? impl_->propertyColumnWidth : kLayerColumnWidth;
}

void ArtifactLayerPanelHeaderWidget::mousePressEvent(QMouseEvent* event)
{
  const int boundaryX = propertyColumnWidth() * kLayerPropertyColumnCount;
  if (event->button() == Qt::LeftButton &&
      std::abs(event->pos().x() - boundaryX) <= Impl::resizeHandleHalfWidth) {
    impl_->resizingPropertyColumns = true;
    impl_->resizeStartX = event->globalPosition().x();
    impl_->resizeStartWidth = propertyColumnWidth();
    setCursor(Qt::SplitHCursor);
    event->accept();
    return;
  }
  QWidget::mousePressEvent(event);
}

void ArtifactLayerPanelHeaderWidget::mouseMoveEvent(QMouseEvent* event)
{
  if (impl_->resizingPropertyColumns) {
    const int dx = static_cast<int>(std::round(event->globalPosition().x() - impl_->resizeStartX));
    setPropertyColumnWidth(impl_->resizeStartWidth + dx / kLayerPropertyColumnCount);
    Q_EMIT propertyColumnWidthChanged(propertyColumnWidth());
    event->accept();
    return;
  }

  const int boundaryX = propertyColumnWidth() * kLayerPropertyColumnCount;
  if (std::abs(event->pos().x() - boundaryX) <= Impl::resizeHandleHalfWidth) {
    setCursor(Qt::SplitHCursor);
  } else {
    unsetCursor();
  }
  QWidget::mouseMoveEvent(event);
}

void ArtifactLayerPanelHeaderWidget::mouseReleaseEvent(QMouseEvent* event)
{
  if (impl_->resizingPropertyColumns && event->button() == Qt::LeftButton) {
    impl_->resizingPropertyColumns = false;
    unsetCursor();
    Q_EMIT propertyColumnWidthChanged(propertyColumnWidth());
    event->accept();
    return;
  }
  QWidget::mouseReleaseEvent(event);
}

void ArtifactLayerPanelHeaderWidget::leaveEvent(QEvent* event)
{
  if (!impl_->resizingPropertyColumns) {
    unsetCursor();
  }
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
   bool searchMatched = false;
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
  SearchMatchMode searchMatchMode = SearchMatchMode::FilterOnly;
  TimelinePropertyDisplayMode propertyDisplayMode = TimelinePropertyDisplayMode::KeyframesOnly;
  int hoveredLayerIndex = -1;
  LayerID selectedLayerId;
  QVector<VisibleRow> visibleRows;
  QHash<QString, bool> expandedByLayerId;
  TimelineLayerDisplayMode displayMode = TimelineLayerDisplayMode::AllLayers;
  int rowHeight = kLayerRowHeight;
  int propertyColumnWidth = kLayerColumnWidth;
  bool resizingPropertyColumns = false;
  int resizeStartX = 0;
  int resizeStartWidth = kLayerColumnWidth;
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
  QTimer* audioPulseTimer = nullptr;
  bool audioPulseVisible = false;
  QHash<QString, QMetaObject::Connection> layerChangedConnections;
  int lastContentHeight = -1;
  
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

  void setSearchMatchMode(SearchMatchMode mode, ArtifactLayerPanelWidget* owner)
  {
   if (searchMatchMode == mode) {
    return;
   }
   searchMatchMode = mode;
   if (owner) {
    owner->updateLayout();
   }
  }

  void setDisplayMode(TimelineLayerDisplayMode mode, ArtifactLayerPanelWidget* owner)
  {
   if (displayMode == mode) {
    return;
   }
   displayMode = mode;
   if (owner) {
    this->expandPathToSelectedLayer();
    owner->updateLayout();
   }
  }

  void setPropertyDisplayMode(TimelinePropertyDisplayMode mode, ArtifactLayerPanelWidget* owner)
  {
   if (propertyDisplayMode == mode) {
    return;
   }
   propertyDisplayMode = mode;
   if (owner) {
    this->expandPathToSelectedLayer();
    owner->updateLayout();
   }
  }

  void setRowHeight(int height, ArtifactLayerPanelWidget* owner)
  {
   const int clamped = std::clamp(height, 20, 48);
   if (rowHeight == clamped) {
    return;
   }
   rowHeight = clamped;
   if (owner) {
    owner->updateLayout();
   }
  }

  void setPropertyColumnWidth(int width, ArtifactLayerPanelWidget* owner)
  {
   const int clamped = std::clamp(width, 18, 44);
   if (propertyColumnWidth == clamped) {
    return;
   }
   propertyColumnWidth = clamped;
   if (owner) {
    owner->update();
    owner->updateGeometry();
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
   const int rowHeightPx = std::max(1, rowHeight);
   return std::clamp<int>(
    (y + (rowHeightPx / 2)) / rowHeightPx,
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

   QSet<ArtifactAbstractLayerPtr> selectedLayers;
   if (displayMode == TimelineLayerDisplayMode::SelectedOnly) {
    if (auto* app = ArtifactApplicationManager::instance()) {
      if (auto* selection = app->layerSelectionManager()) {
        selectedLayers = selection->selectedLayers();
        if (selectedLayers.isEmpty()) {
          if (auto current = selection->currentLayer()) {
            selectedLayers.insert(current);
          }
        }
      }
    }
   }

  QVector<ArtifactAbstractLayerPtr> layers;
  const LayerSearchQuery query = parseLayerSearchQuery(filterText);
  const bool hasQuery = !query.isEmpty();
  for (auto& l : comp->allLayer()) {
     if (!l) continue;
     if (shyHidden && l->isShy()) continue;
     if (!layerMatchesDisplayMode(l, displayMode, selectedLayers)) continue;
     const bool matches = !hasQuery || matchesLayerSearchQuery(l, comp, query);
     if (hasQuery && searchMatchMode == SearchMatchMode::FilterOnly && !matches) continue;
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
     const auto panelGroups = layerPanelPropertyLabels(node, propertyDisplayMode);
     const bool hasChildren = !nodeChildren.isEmpty() || !panelGroups.isEmpty();
     const bool expanded = expandedByLayerId.value(nodeId, true);
     const bool nodeMatched = hasQuery && matchesLayerSearchQuery(node, comp, query);
     visibleRows.push_back(VisibleRow{ node, depth, hasChildren, expanded, RowKind::Layer, QString(), nodeMatched });
     emitted.insert(nodeId);

     if (!hasChildren || !expanded) return;

     for (const auto& groupLabel : panelGroups) {
      visibleRows.push_back(VisibleRow{
       node,
       depth + 1,
       false,
       false,
       RowKind::Group,
       groupLabel,
       nodeMatched
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

  int propertyColumnsWidth() const
  {
   return std::max(1, propertyColumnWidth) * kLayerPropertyColumnCount;
  }

  QRect propertyColumnResizeRect(int rowTop, int rowHeightPx) const
  {
   const int x = propertyColumnsWidth();
   return QRect(x - 4, rowTop, 8, rowHeightPx);
  }

  void expandPathToSelectedLayer()
  {
   auto comp = safeCompositionLookup(compositionId);
   if (!comp) {
    return;
   }

   LayerID targetId = selectedLayerId;
   if (targetId.isNil()) {
    if (auto* app = ArtifactApplicationManager::instance()) {
      if (auto* selection = app->layerSelectionManager()) {
        if (auto current = selection->currentLayer()) {
          targetId = current->id();
        }
      }
    }
   }
   if (targetId.isNil()) {
    return;
   }

   QSet<QString> visited;
   while (!targetId.isNil()) {
    const QString idStr = targetId.toString();
    if (idStr.isEmpty() || visited.contains(idStr)) {
      break;
    }
    visited.insert(idStr);
    expandedByLayerId[idStr] = true;

    const auto layer = comp->layerById(targetId);
    if (!layer || !layer->hasParent()) {
      break;
    }
    targetId = layer->parentLayerId();
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

  impl_->audioPulseTimer = new QTimer(this);
  impl_->audioPulseTimer->setInterval(250);
  impl_->audioPulseTimer->setSingleShot(false);
  QObject::connect(impl_->audioPulseTimer, &QTimer::timeout, this, [this]() {
    impl_->audioPulseVisible = !impl_->audioPulseVisible;
    this->update();
  });

  if (auto* playback = ArtifactPlaybackService::instance()) {
    QObject::connect(playback, &ArtifactPlaybackService::playbackStateChanged, this,
                     [this](PlaybackState state) {
      const bool shouldBlink = (state == PlaybackState::Playing);
      if (shouldBlink) {
        if (!impl_->audioPulseTimer->isActive()) {
          impl_->audioPulseVisible = true;
          impl_->audioPulseTimer->start();
        }
      } else {
        impl_->audioPulseTimer->stop();
        impl_->audioPulseVisible = false;
        this->update();
      }
    });
    if (playback->isPlaying()) {
      impl_->audioPulseVisible = true;
      impl_->audioPulseTimer->start();
    }
  }

  if (auto* service = ArtifactProjectService::instance()) {
    QObject::connect(service, &ArtifactProjectService::layerCreated, this, [this](const CompositionID& compId, const LayerID& layerId) {
      if (impl_->compositionId == compId) {
        this->updateLayout();
        // Start follow-up UI work on the next event loop tick so layer creation
        // itself stays responsive.
       QTimer::singleShot(0, this, [this, layerId]() {
          const auto widgets = QApplication::allWidgets();
          for (QWidget* w : widgets) {
           if (!w) continue;
           const QString className = QString::fromLatin1(w->metaObject()->className());
           if (className.contains("ArtifactInspectorWidget", Qt::CaseInsensitive)) {
            w->show();
            w->raise();
            w->activateWindow();
            break;
           }
          }

          if (impl_->layerNameEditable) {
            this->editLayerName(layerId);
          }
          // Ensure layer is visible in the scroll area wrapper if possible
          Q_EMIT visibleRowsChanged();
        });
      }
    });
    QObject::connect(service, &ArtifactProjectService::layerRemoved, this, [this](const CompositionID& compId, const LayerID&) {
      if (impl_->compositionId == compId) this->updateLayout();
    });
    QObject::connect(service, &ArtifactProjectService::layerSelected, this, [this](const LayerID& layerId) {
      if (impl_->selectedLayerId != layerId) {
        impl_->selectedLayerId = layerId;
        impl_->expandPathToSelectedLayer();
        updateLayout();
      }
    });
    if (auto* app = ArtifactApplicationManager::instance()) {
      if (auto* selection = app->layerSelectionManager()) {
        QObject::connect(selection, &ArtifactLayerSelectionManager::selectionChanged, this,
                         [this, selection]() {
          const auto current = selection ? selection->currentLayer() : ArtifactAbstractLayerPtr{};
          const LayerID nextId = current ? current->id() : LayerID();
          if (impl_->selectedLayerId != nextId) {
            impl_->selectedLayerId = nextId;
          }
          impl_->expandPathToSelectedLayer();
          updateLayout();
        });
      }
    }
    QObject::connect(service, &ArtifactProjectService::compositionCreated, this, [this](const CompositionID& compId) {
      if (impl_->compositionId.isNil()) {
        impl_->compositionId = compId;
      }
      updateLayout();
    });
    QObject::connect(service, &ArtifactProjectService::projectChanged, this, [this]() {
      updateLayout();
    });
  }
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

void ArtifactLayerPanelWidget::setDisplayMode(TimelineLayerDisplayMode mode)
{
  impl_->setDisplayMode(mode, this);
}

TimelineLayerDisplayMode ArtifactLayerPanelWidget::displayMode() const
{
  return impl_->displayMode;
}

void ArtifactLayerPanelWidget::setPropertyDisplayMode(TimelinePropertyDisplayMode mode)
{
  impl_->setPropertyDisplayMode(mode, this);
}

TimelinePropertyDisplayMode ArtifactLayerPanelWidget::propertyDisplayMode() const
{
  return impl_->propertyDisplayMode;
}

void ArtifactLayerPanelWidget::setRowHeight(int rowHeight)
{
  impl_->setRowHeight(rowHeight, this);
}

int ArtifactLayerPanelWidget::rowHeight() const
{
  return impl_->rowHeight;
}

void ArtifactLayerPanelWidget::setPropertyColumnWidth(int width)
{
  impl_->setPropertyColumnWidth(width, this);
}

int ArtifactLayerPanelWidget::propertyColumnWidth() const
{
  return impl_->propertyColumnWidth;
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
  impl_->setSearchMatchMode(mode, this);
}

SearchMatchMode ArtifactLayerPanelWidget::searchMatchMode() const
{
  return impl_->searchMatchMode;
}

QVector<LayerID> ArtifactLayerPanelWidget::matchingTimelineRows() const
{
  QVector<LayerID> rows;
  if (!impl_) {
   return rows;
  }
  rows.reserve(impl_->visibleRows.size());
  for (const auto& row : impl_->visibleRows) {
   if (row.kind == Impl::RowKind::Layer && row.layer && row.searchMatched) {
    rows.append(row.layer->id());
   }
  }
  return rows;
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
  const int previousContentHeight = impl_->lastContentHeight;
  impl_->refreshLayerChangedSubscriptions(this);
  
  impl_->clearInlineEditors();
  impl_->rebuildVisibleRows();
  const bool structureChanged = !rowsEqual(oldRows, impl_->visibleRows);
  const int count = impl_->visibleRows.size();
  const int rowH = std::max(1, impl_->rowHeight);
  const int contentHeight = std::max(rowH, count * rowH);
  if (contentHeight != impl_->lastContentHeight) {
    setMinimumHeight(0);
    setMinimumHeight(contentHeight);
    setMaximumHeight(QWIDGETSIZE_MAX);
    updateGeometry();
    impl_->lastContentHeight = contentHeight;
  }
  update();
  if (structureChanged || contentHeight != previousContentHeight) {
    Q_EMIT visibleRowsChanged();
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
    impl_->inlineNameEditor->setStyleSheet(makeTimelineLineEditStyle(impl_->inlineNameEditor));

    // Position it
    const int rowIndent = impl_->visibleRows[idx].depth * 14;
    const int nameStartX = std::max(1, impl_->propertyColumnWidth) * kLayerPropertyColumnCount;
    const int textX = nameStartX + rowIndent + (impl_->visibleRows[idx].hasChildren ? 16 : 4);
  const int editorWidth = std::max(60, width() - textX - kInlineParentWidth - kInlineBlendWidth - 8);
    const int rowH = std::max(1, impl_->rowHeight);
    impl_->inlineNameEditor->setGeometry(textX, idx * rowH + 2, editorWidth, rowH - 4);

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
  const int rowH = std::max(1, impl_->rowHeight);
  const int colW = std::max(1, impl_->propertyColumnWidth);
  const int clickX = event->pos().x();
  int idx = event->pos().y() / rowH;

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
      combo->setStyleSheet(makeTimelineComboStyle(combo));
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
      combo->setStyleSheet(makeTimelineComboStyle(combo));
      const auto items = blendModeItems();
      for (const auto& [name, mode] : items) {
        combo->addItem(name, static_cast<int>(mode));
      }
      if (auto* popupView = qobject_cast<QListView*>(combo->view())) {
        popupView->setItemDelegate(new BlendModePopupDelegate(popupView));
        popupView->setMouseTracking(true);
        popupView->setAlternatingRowColors(false);
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
        if (service) {
          if (auto project = service->getCurrentProjectSharedPtr()) {
            project->projectChanged();
          }
        }
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
    // 名前エリアまたはスイッチ列での選択操作
    if (auto* app = ArtifactApplicationManager::instance()) {
      if (auto* selection = app->layerSelectionManager()) {
        auto comp = safeCompositionLookup(impl_->compositionId);
        if (comp) {
          selection->setActiveComposition(comp);
          const bool ctrl = event->modifiers() & Qt::ControlModifier;
          const bool shift = event->modifiers() & Qt::ShiftModifier;
          if (ctrl) {
            if (selection->isSelected(layer)) {
              selection->removeFromSelection(layer);
            } else {
              selection->addToSelection(layer);
            }
          } else if (shift) {
            selection->addToSelection(layer);
          } else if (service) {
            service->selectLayer(layer->id());
          } else {
            selection->selectLayer(layer);
          }
          const auto current = selection->currentLayer();
          impl_->selectedLayerId = current ? current->id() : layer->id();
        } else if (service) {
          service->selectLayer(layer->id());
        }
      } else if (service) {
        service->selectLayer(layer->id());
      }
    } else if (service) {
      service->selectLayer(layer->id());
    }
    impl_->dragStartPos = event->pos();
    impl_->dragCandidateLayerId = layer->id();
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

    // Label color submenu
    QMenu* labelColorMenu = menu.addMenu("Label Color");
    static const struct { const char* name; QColor color; } labelColors[] = {
        {"None",   QColor(100, 100, 100)},
        {"Red",    QColor(200, 80, 80)},
        {"Orange", QColor(220, 160, 50)},
        {"Yellow", QColor(210, 200, 60)},
        {"Green",  QColor(80, 180, 80)},
        {"Cyan",   QColor(60, 160, 200)},
        {"Blue",   QColor(80, 120, 220)},
        {"Purple", QColor(180, 80, 200)},
        {"Pink",   QColor(200, 120, 160)},
    };
    const int currentColorIndex = layer->labelColorIndex();
    for (int ci = 0; ci < 9; ++ci) {
        QPixmap colorIcon(16, 16);
        colorIcon.fill(labelColors[ci].color);
        QAction* colorAct = labelColorMenu->addAction(QIcon(colorIcon), labelColors[ci].name);
        colorAct->setCheckable(true);
        colorAct->setChecked(ci == currentColorIndex);
        colorAct->setData(ci);
    }

    QMenu* createMenu = menu.addMenu("Create Layer");
    QAction* createSolidAct  = createMenu->addAction("Solid Layer");
    QAction* createNullAct   = createMenu->addAction("Null Layer");
    QAction* createAdjustAct = createMenu->addAction("Adjustment Layer");
    QAction* createTextAct   = createMenu->addAction("Text Layer");
    QAction* createShapeAct  = createMenu->addAction("Shape Layer");
    QAction* createParticleAct = createMenu->addAction("Particle Layer");
    QAction* createCameraAct = createMenu->addAction("Camera Layer");
    createMenu->setIcon(impl_->iconCreateSolid);
    createSolidAct->setIcon(impl_->iconCreateSolid);
    createNullAct->setIcon(impl_->iconCreateNull);
    createAdjustAct->setIcon(impl_->iconCreateAdjust);
    createTextAct->setIcon(impl_->iconCreateText);
    createShapeAct->setIcon(impl_->iconCreateSolid);
    createCameraAct->setIcon(impl_->iconCreateText);

    menu.addSeparator();
    QAction* swapWithNextAct = menu.addAction("Swap with Layer Below");
    QAction* swapWithPrevAct = menu.addAction("Swap with Layer Above");
    QAction* moveToZeroAct = menu.addAction("Move Start to Frame 0");

    // Align submenu (requires 2+ selected layers)
    auto* selMgr = ArtifactLayerSelectionManager::instance();
    const int selCount = selMgr ? selMgr->selectedLayers().size() : 0;
    QMenu* alignMenu = menu.addMenu("Align");
    alignMenu->setEnabled(selCount >= 2);
    QAction* alignLeftAct   = alignMenu->addAction("Align Left");
    QAction* alignCenterAct = alignMenu->addAction("Align Center Horizontally");
    QAction* alignRightAct  = alignMenu->addAction("Align Right");
    QAction* alignTopAct    = alignMenu->addAction("Align Top");
    QAction* alignMiddleAct = alignMenu->addAction("Align Middle Vertically");
    QAction* alignBottomAct = alignMenu->addAction("Align Bottom");
    alignMenu->addSeparator();
    QAction* distHorizAct   = alignMenu->addAction("Distribute Horizontally");
    QAction* distVertAct    = alignMenu->addAction("Distribute Vertically");

    // Disable if at edge
    auto comp = safeCompositionLookup(impl_->compositionId);
    const int layerCount = comp ? comp->layerCount() : 0;
    swapWithPrevAct->setEnabled(idx > 0);
    swapWithNextAct->setEnabled(idx < layerCount - 1);

    QAction* chosen = menu.exec(event->globalPosition().toPoint());

    // Label color handling
    if (chosen && chosen->parentWidget() == labelColorMenu) {
        int colorIndex = chosen->data().toInt();
        layer->setLabelColorIndex(colorIndex);
        update();
        return;
    }

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
    } else if (chosen == swapWithPrevAct || chosen == swapWithNextAct) {
      if (auto* service = ArtifactProjectService::instance()) {
       const int direction = (chosen == swapWithNextAct) ? 1 : -1;
       service->moveLayerInCurrentComposition(layer->id(), idx + direction);
      }
    } else if (chosen == moveToZeroAct) {
      // Move layer start to frame 0 by adjusting inPoint
      const auto currentInPoint = layer->inPoint();
      if (currentInPoint.framePosition() > 0) {
       const auto currentOutPoint = layer->outPoint();
       layer->setInPoint(FramePosition(0));
       layer->setOutPoint(FramePosition(currentOutPoint.framePosition() - currentInPoint.framePosition()));
       qDebug() << "[LayerPanel] Moved layer" << layer->layerName()
                << "start to frame 0 (offset was" << currentInPoint.framePosition() << ")";
       update();
      }
    } else if (chosen == alignLeftAct || chosen == alignCenterAct || chosen == alignRightAct ||
               chosen == alignTopAct || chosen == alignMiddleAct || chosen == alignBottomAct ||
               chosen == distHorizAct || chosen == distVertAct) {
      // Align / Distribute selected layers
      if (selMgr && comp) {
        const auto selected = selMgr->selectedLayers();
        if (selected.size() >= 2) {
          const float compW = static_cast<float>(comp->settings().compositionSize().width());
          const float compH = static_cast<float>(comp->settings().compositionSize().height());

          // Collect positions
          struct LayerPos { ArtifactAbstractLayerPtr layer; float x, y, w, h; };
          QVector<LayerPos> positions;
          for (const auto& l : selected) {
            if (!l) continue;
            auto bb = l->transformedBoundingBox();
            positions.append({l, static_cast<float>(bb.x()), static_cast<float>(bb.y()),
                              static_cast<float>(bb.width()), static_cast<float>(bb.height())});
          }

          if (chosen == alignLeftAct) {
            float minX = positions[0].x;
            for (const auto& p : positions) minX = std::min(minX, p.x);
            for (auto& p : positions) {
              float dx = minX - p.x;
              auto pos = p.layer->position3D();
              p.layer->setPosition3D(QVector3D(pos.x() + dx, pos.y(), pos.z()));
            }
          } else if (chosen == alignCenterAct) {
            float sumCenterX = 0;
            for (const auto& p : positions) sumCenterX += p.x + p.w * 0.5f;
            float avgCenterX = sumCenterX / positions.size();
            for (auto& p : positions) {
              float dx = avgCenterX - (p.x + p.w * 0.5f);
              auto pos = p.layer->position3D();
              p.layer->setPosition3D(QVector3D(pos.x() + dx, pos.y(), pos.z()));
            }
          } else if (chosen == alignRightAct) {
            float maxX = positions[0].x + positions[0].w;
            for (const auto& p : positions) maxX = std::max(maxX, p.x + p.w);
            for (auto& p : positions) {
              float dx = maxX - (p.x + p.w);
              auto pos = p.layer->position3D();
              p.layer->setPosition3D(QVector3D(pos.x() + dx, pos.y(), pos.z()));
            }
          } else if (chosen == alignTopAct) {
            float minY = positions[0].y;
            for (const auto& p : positions) minY = std::min(minY, p.y);
            for (auto& p : positions) {
              float dy = minY - p.y;
              auto pos = p.layer->position3D();
              p.layer->setPosition3D(QVector3D(pos.x(), pos.y() + dy, pos.z()));
            }
          } else if (chosen == alignMiddleAct) {
            float sumCenterY = 0;
            for (const auto& p : positions) sumCenterY += p.y + p.h * 0.5f;
            float avgCenterY = sumCenterY / positions.size();
            for (auto& p : positions) {
              float dy = avgCenterY - (p.y + p.h * 0.5f);
              auto pos = p.layer->position3D();
              p.layer->setPosition3D(QVector3D(pos.x(), pos.y() + dy, pos.z()));
            }
          } else if (chosen == alignBottomAct) {
            float maxY = positions[0].y + positions[0].h;
            for (const auto& p : positions) maxY = std::max(maxY, p.y + p.h);
            for (auto& p : positions) {
              float dy = maxY - (p.y + p.h);
              auto pos = p.layer->position3D();
              p.layer->setPosition3D(QVector3D(pos.x(), pos.y() + dy, pos.z()));
            }
          } else if (chosen == distHorizAct && positions.size() >= 3) {
            // Sort by X position
            std::sort(positions.begin(), positions.end(), [](const LayerPos& a, const LayerPos& b) { return a.x < b.x; });
            float totalSpace = positions.last().x - positions.first().x;
            float step = totalSpace / (positions.size() - 1);
            for (int i = 1; i < positions.size() - 1; ++i) {
              float targetX = positions.first().x + step * i;
              float dx = targetX - positions[i].x;
              auto pos = positions[i].layer->position3D();
              positions[i].layer->setPosition3D(QVector3D(pos.x() + dx, pos.y(), pos.z()));
            }
          } else if (chosen == distVertAct && positions.size() >= 3) {
            std::sort(positions.begin(), positions.end(), [](const LayerPos& a, const LayerPos& b) { return a.y < b.y; });
            float totalSpace = positions.last().y - positions.first().y;
            float step = totalSpace / (positions.size() - 1);
            for (int i = 1; i < positions.size() - 1; ++i) {
              float targetY = positions.first().y + step * i;
              float dy = targetY - positions[i].y;
              auto pos = positions[i].layer->position3D();
              positions[i].layer->setPosition3D(QVector3D(pos.x(), pos.y() + dy, pos.z()));
            }
          }
          update();
        }
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
      ArtifactSolidLayerInitParams params(QStringLiteral("Adjustment Layer"));
      if (comp) {
       auto sz = comp->settings().compositionSize();
       params.setWidth(sz.width());
       params.setHeight(sz.height());
      }
      params.setColor(FloatColor(0.0f, 0.0f, 0.0f, 1.0f));
      if (service) {
       service->addLayerToCurrentComposition(params);
      }
    } else if (chosen == createTextAct) {
      ArtifactTextLayerInitParams params(QStringLiteral("Text"));
      if (service) {
       service->addLayerToCurrentComposition(params);
      }
    } else if (chosen == createShapeAct) {
      ArtifactLayerInitParams params(QStringLiteral("Shape"), LayerType::Shape);
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
  const int rowH = std::max(1, impl_->rowHeight);
  const int colW = std::max(1, impl_->propertyColumnWidth);
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
  const bool showInlineCombos = width() >= (colW * kLayerPropertyColumnCount + kInlineComboReserve + kLayerNameMinWidth);
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
  editor->setStyleSheet(makeTimelineLineEditStyle(editor));
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
    if (impl_->draggedLayerId.isNil() && !impl_->dragStarted_ && dragDistance >= QApplication::startDragDistance()) {
      impl_->dragStarted_ = true;
      impl_->draggedLayerId = impl_->dragCandidateLayerId;
      qDebug() << "[LayerPanel] Drag starting:" << impl_->draggedLayerId.toString();
      auto* mime = new QMimeData();
      mime->setData(kLayerReorderMimeType, impl_->draggedLayerId.toString().toUtf8());
      mime->setText(impl_->draggedLayerId.toString());
      
      QDrag drag(this);
      drag.setMimeData(mime);
      drag.setHotSpot(event->pos() - impl_->dragStartPos);
      
      const Qt::DropAction dropResult = drag.exec(Qt::MoveAction);
      qDebug() << "[LayerPanel] Drag result:" << dropResult;
      
      impl_->clearDragState();
      unsetCursor();
      update();
      event->accept();
      return;
    }
  }

  const int rowH = std::max(1, impl_->rowHeight);
  const int colW = std::max(1, impl_->propertyColumnWidth);
  const int separatorX = colW * kLayerPropertyColumnCount;
  int idx = event->pos().y() / rowH;
  if (idx != impl_->hoveredLayerIndex) {
    const int previousHoveredIndex = impl_->hoveredLayerIndex;
    impl_->hoveredLayerIndex = idx;
    if (previousHoveredIndex >= 0 && previousHoveredIndex < impl_->visibleRows.size()) {
      update(0, previousHoveredIndex * rowH, width(), rowH);
    }
    if (idx >= 0 && idx < impl_->visibleRows.size()) {
      update(0, idx * rowH, width(), rowH);
    }
  }
  bool pointer = event->pos().x() < separatorX;
  if (!pointer && idx >= 0 && idx < impl_->visibleRows.size()) {
    const auto& row = impl_->visibleRows[idx];
    if (row.hasChildren) {
      const int nameStartX = separatorX;
      const int indent = 14;
      const int toggleSize = 10;
      const int toggleX = nameStartX + row.depth * indent + 2;
      const QRect toggleRect(toggleX, idx * rowH + (rowH - toggleSize) / 2, toggleSize, toggleSize);
      pointer = toggleRect.contains(event->pos());
    }
  }
  setCursor(pointer ? Qt::PointingHandCursor : Qt::ArrowCursor);
}

void ArtifactLayerPanelWidget::mouseReleaseEvent(QMouseEvent* event)
{
  if (event->button() == Qt::LeftButton && !impl_->draggedLayerId.isNil()) {
    impl_->clearDragState();
    unsetCursor();
    update();
    event->accept();
    return;
  }

  impl_->clearDragState();
  unsetCursor();
  QWidget::mouseReleaseEvent(event);
 }

void ArtifactLayerPanelWidget::keyPressEvent(QKeyEvent* event)
{
  if (event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace) {
    auto comp = safeCompositionLookup(impl_->compositionId);
    const CompositionID compId = comp ? comp->id() : impl_->compositionId;
    auto* service = ArtifactProjectService::instance();
    auto* selection = ArtifactApplicationManager::instance()
                          ? ArtifactApplicationManager::instance()->layerSelectionManager()
                          : nullptr;
    const auto selectedLayers = selection ? selection->selectedLayers() : QSet<ArtifactAbstractLayerPtr>{};
    if (service && !compId.isNil()) {
      if (selectedLayers.size() > 1) {
        for (const auto& layer : selectedLayers) {
          if (layer) {
            service->removeLayerFromComposition(compId, layer->id());
          }
        }
        event->accept();
        return;
      }
      if (!impl_->selectedLayerId.isNil()) {
        service->removeLayerFromComposition(compId, impl_->selectedLayerId);
        event->accept();
        return;
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

  // Home キー - 最初のレイヤーへ選択
  if (event->key() == Qt::Key_Home && !impl_->inlineNameEditor) {
    if (!impl_->visibleRows.isEmpty()) {
      for (int i = 0; i < impl_->visibleRows.size(); ++i) {
        if (impl_->visibleRows[i].layer && impl_->visibleRows[i].kind == Impl::RowKind::Layer) {
          impl_->selectedLayerId = impl_->visibleRows[i].layer->id();
          update();
          if (auto* svc = ArtifactProjectService::instance()) {
            svc->selectLayer(impl_->selectedLayerId);
          }
          event->accept();
          return;
        }
      }
    }
  }
  
  // End キー - 最後のレイヤーへ選択
  if (event->key() == Qt::Key_End && !impl_->inlineNameEditor) {
    if (!impl_->visibleRows.isEmpty()) {
      for (int i = impl_->visibleRows.size() - 1; i >= 0; --i) {
        if (impl_->visibleRows[i].layer && impl_->visibleRows[i].kind == Impl::RowKind::Layer) {
          impl_->selectedLayerId = impl_->visibleRows[i].layer->id();
          update();
          if (auto* svc = ArtifactProjectService::instance()) {
            svc->selectLayer(impl_->selectedLayerId);
          }
          event->accept();
          return;
        }
      }
    }
  }
  
  // Ctrl+A - 全選択
  if (event->key() == Qt::Key_A && event->modifiers() & Qt::ControlModifier && !impl_->inlineNameEditor) {
    auto* selection = ArtifactApplicationManager::instance()
                          ? ArtifactApplicationManager::instance()->layerSelectionManager()
                          : nullptr;
    if (selection) {
      selection->clearSelection();
      for (const auto& row : impl_->visibleRows) {
        if (row.layer && row.kind == Impl::RowKind::Layer) {
          selection->addToSelection(row.layer);
        }
      }
      update();
      event->accept();
      return;
    }
  }
  
  // Ctrl+D - レイヤー複製
  if (event->key() == Qt::Key_D && event->modifiers() & Qt::ControlModifier && !impl_->inlineNameEditor) {
    if (!impl_->selectedLayerId.isNil()) {
      auto* svc = ArtifactProjectService::instance();
      if (svc) {
        svc->duplicateLayerInCurrentComposition(impl_->selectedLayerId);
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
    const int rowH = std::max(1, impl_->rowHeight);
    const int y = selectedIdx * rowH + rowH / 2;
    const int x = std::max(1, impl_->propertyColumnWidth) * kLayerPropertyColumnCount + 20;
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
   const int rowH = std::max(1, impl_->rowHeight);
   const int rowIdx = mouseY / rowH;
  const bool showInlineCombos = width() >= (std::max(1, impl_->propertyColumnWidth) * kLayerPropertyColumnCount + kInlineComboReserve + kLayerNameMinWidth);
   const int parentRectX = width() - kInlineComboReserve;
   const QRect parentRect(parentRectX, rowIdx * rowH + kInlineComboMarginY, kInlineParentWidth, kInlineComboHeight);
   const QRect blendRect(parentRect.right() + kInlineComboGap, rowIdx * rowH + kInlineComboMarginY, kInlineBlendWidth, kInlineComboHeight);
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
    const int rowH = std::max(1, impl_->rowHeight);
    update(0, previousHoveredIndex * rowH, width(), rowH);
   }
  }

void ArtifactLayerPanelWidget::paintEvent(QPaintEvent* event)
{
  QPainter p(this);
  p.setRenderHint(QPainter::SmoothPixmapTransform);
  const int rowH = std::max(1, impl_->rowHeight);
  const int colW = std::max(1, impl_->propertyColumnWidth);
  const int iconSize = 16;
  const int offset = (colW - iconSize) / 2;
  const int nameStartX = colW * kLayerPropertyColumnCount;
  const int separatorX = nameStartX;
  const int indent = 14;
  const int toggleSize = 10;
  
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
    const auto selection = ArtifactApplicationManager::instance()
        ? ArtifactApplicationManager::instance()->layerSelectionManager()
        : nullptr;
    const bool sel = (l->id() == impl_->selectedLayerId) ||
                     (selection && selection->isSelected(l));

    // レイヤータイプ別の色を取得
    QColor layerTypeColor = getLayerTypeColor(l);
    
    // 左端にタイプカラーバーを描画（4px）
    p.fillRect(0, y, 4, rowH, layerTypeColor);

    if (impl_->audioPulseVisible && layerCanOutputAudio(l)) {
      const QColor pulseColor(42, 232, 112, 235);
      p.setPen(Qt::NoPen);
      p.setBrush(pulseColor);
      p.drawRoundedRect(QRect(0, y + 5, 4, rowH - 10), 2, 2);
      p.setBrush(QColor(230, 255, 240, 160));
      p.drawRoundedRect(QRect(1, y + 9, 2, rowH - 18), 1, 1);
    }

    if (sel && !isGroupRow) p.fillRect(4, y, width() - 4, rowH, QColor(180, 110, 45)); // Modo-like Amber selection
    else if (i == impl_->hoveredLayerIndex) p.fillRect(4, y, width() - 4, rowH, QColor(60, 60, 60)); // Subtle grey hover
    else p.fillRect(4, y, width() - 4, rowH, (i % 2 == 0) ? QColor(42, 42, 42) : QColor(45, 45, 45));

    if (!impl_->filterText.trimmed().isEmpty() && row.searchMatched && impl_->searchMatchMode == SearchMatchMode::HighlightOnly) {
      p.fillRect(4, y, width() - 4, rowH, QColor(74, 120, 90, 64));
      p.setPen(QColor(120, 220, 160, 180));
      p.drawLine(4, y + rowH - 1, width(), y + rowH - 1);
    }

    p.setPen(QColor(60, 60, 60));
    p.drawLine(0, y + rowH, width(), y + rowH);

    if (isGroupRow) {
      const int textX = nameStartX + row.depth * indent + 4;
      p.setPen(QColor(196, 196, 196));
      p.drawText(textX, y, std::max(20, width() - textX - 8), rowH, Qt::AlignVCenter | Qt::AlignLeft, row.label);
      p.setPen(QPen(QColor(92, 92, 98, 180), 1));
      p.drawLine(separatorX, y, separatorX, y + rowH);
      continue;
    }

    int curX = 0;

    // Label color bar (left edge)
    static const QColor labelColors[] = {
        QColor(100, 100, 100),  // 0: None
        QColor(200, 80, 80),    // 1: Red
        QColor(220, 160, 50),   // 2: Orange
        QColor(210, 200, 60),   // 3: Yellow
        QColor(80, 180, 80),    // 4: Green
        QColor(60, 160, 200),   // 5: Cyan
        QColor(80, 120, 220),   // 6: Blue
        QColor(180, 80, 200),   // 7: Purple
        QColor(200, 120, 160),  // 8: Pink
    };
    p.setPen(Qt::NoPen);
    p.setBrush(labelColors[l->labelColorIndex() % 9]);
    p.drawRect(0, y, 4, rowH);
    curX = 4;

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

    p.setPen(QPen(QColor(92, 92, 98, 180), 1));
    p.drawLine(separatorX, y, separatorX, y + rowH);

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
    const bool showInlineCombos = (width() - (nameX + 8)) >= (kInlineComboReserve + kLayerNameMinWidth);
    const int parentRectX = width() - kInlineComboReserve;
    const QRect parentRect(parentRectX, y + kInlineComboMarginY, kInlineParentWidth, kInlineComboHeight);
    const QRect blendRect(parentRect.right() + kInlineComboGap, y + kInlineComboMarginY, kInlineBlendWidth, kInlineComboHeight);

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

    if (showInlineCombos) {
      drawInlineCombo(parentRect, QStringLiteral("Parent: %1").arg(parentName));
      if (!std::dynamic_pointer_cast<ArtifactAudioLayer>(l)) {
        drawInlineCombo(blendRect, QStringLiteral("Blend: %1").arg(blendModeToText(l->layerBlendType())));
      }
    }
    p.setPen(Qt::white);
    const int textWidth = showInlineCombos ? std::max(20, parentRect.left() - textX - 8) : std::max(20, width() - textX - 8);
    p.drawText(textX + 4, y, textWidth, rowH, Qt::AlignVCenter | Qt::AlignLeft, l->layerName());

    if (auto audio = std::dynamic_pointer_cast<ArtifactAudioLayer>(l)) {
      QString audioState = audio->isMuted()
        ? QStringLiteral("Muted")
        : QStringLiteral("Vol %1%").arg(std::lround(audio->volume() * 100.0));
      const int badgePaddingX = 8;
      const int badgeWidth = std::min(120, p.fontMetrics().horizontalAdvance(audioState) + badgePaddingX * 2);
      int badgeX = showInlineCombos ? (parentRect.left() - badgeWidth - 8) : (width() - badgeWidth - 8);
      if (badgeX > textX + 24) {
        const QRect badgeRect(badgeX, y + kInlineComboMarginY, badgeWidth, kInlineComboHeight);
        p.setPen(QColor(85, 85, 90));
        p.setBrush(QColor(42, 42, 48));
        p.drawRoundedRect(badgeRect, 4, 4);
        p.setPen(QColor(230, 220, 120));
        p.drawText(badgeRect.adjusted(badgePaddingX, 0, -badgePaddingX, 0), Qt::AlignVCenter | Qt::AlignLeft, audioState);
      }
    }
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
  qDebug() << "[LayerPanel] dragEnterEvent called, has reorder MIME:" << (mime && mime->hasFormat(kLayerReorderMimeType));
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
  qDebug() << "[LayerPanel] dropEvent called, has reorder MIME:" << (mime && mime->hasFormat(kLayerReorderMimeType));
  if (!mime) {
    event->ignore();
    return;
  }

  if (mime->hasFormat(kLayerReorderMimeType)) {
    auto* svc = ArtifactProjectService::instance();
    auto comp = safeCompositionLookup(impl_->compositionId);

    // MIME からドラッグ中のレイヤーIDを取得（impl_->draggedLayerId は別インスタンスでは無効）
    const LayerID dragLayerId = LayerID(QString::fromUtf8(mime->data(kLayerReorderMimeType)));
    qDebug() << "[LayerPanel] Drop layer:" << dragLayerId.toString() << "at row:" << impl_->dragInsertVisibleRow;
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
        // visibleRows は allLayer() の逆順で描画されるため、
        // 「先頭から targetVisibleIndex 個だけ前にある visible row」の位置を
        // そのまま storage index に落とし込む。
        const int remainingVisibleCount = static_cast<int>(remainingVisibleLayerIds.size());
        const int lastIndex = std::max(0, static_cast<int>(allLayers.size()) - 1);
        const int newIndex = std::clamp(remainingVisibleCount - targetVisibleIndex, 0, lastIndex);
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
      } else if (type == LayerType::Video) {
        ArtifactLayerInitParams params(QFileInfo(path).baseName(), type);
        svc->addLayerToCurrentComposition(params);

        if (auto* app = ArtifactApplicationManager::instance()) {
          if (auto* selectionManager = app->layerSelectionManager()) {
            if (auto currentLayer = selectionManager->currentLayer()) {
              if (!svc->replaceLayerSourceInCurrentComposition(currentLayer->id(), path)) {
                qWarning() << "[LayerPanel] Failed to bind video source to new layer"
                           << currentLayer->id().toString() << path;
              }
            }
          }
        }
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
  impl_->scroll->viewport()->setMouseTracking(true);
   impl_->panel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

   auto* wheelFilter = new LayerPanelWheelFilter(impl_->scroll, this);
   this->installEventFilter(wheelFilter);
   impl_->header->installEventFilter(wheelFilter);
   impl_->panel->installEventFilter(wheelFilter);
   impl_->scroll->viewport()->installEventFilter(wheelFilter);

   auto* dragFilter = new LayerPanelDragForwardFilter(impl_->panel, this);
   impl_->scroll->viewport()->installEventFilter(dragFilter);

  layout->addWidget(impl_->header);
  layout->addWidget(impl_->scroll, 1);

  QObject::connect(impl_->header, &ArtifactLayerPanelHeaderWidget::shyToggled,
                   impl_->panel, &ArtifactLayerPanelWidget::setShyHidden);
  QObject::connect(impl_->header, &ArtifactLayerPanelHeaderWidget::propertyColumnWidthChanged,
                   this, [this](int width) {
                    if (impl_ && impl_->panel) {
                      impl_->panel->setPropertyColumnWidth(width);
                    }
                   });
  QObject::connect(impl_->panel, &ArtifactLayerPanelWidget::visibleRowsChanged,
                   this, [this]() {
                    const bool isEmpty = impl_->panel->visibleTimelineRows().isEmpty();
                    impl_->scroll->setAlignment(isEmpty ? (Qt::AlignHCenter | Qt::AlignVCenter)
                                                        : (Qt::AlignLeft | Qt::AlignTop));
                    Q_EMIT visibleRowsChanged();
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

void ArtifactLayerTimelinePanelWrapper::setPropertyDisplayMode(TimelinePropertyDisplayMode mode)
{
  if (impl_ && impl_->panel) {
    impl_->panel->setPropertyDisplayMode(mode);
  }
}

TimelinePropertyDisplayMode ArtifactLayerTimelinePanelWrapper::propertyDisplayMode() const
{
  return impl_ && impl_->panel ? impl_->panel->propertyDisplayMode() : TimelinePropertyDisplayMode::KeyframesOnly;
}

void ArtifactLayerTimelinePanelWrapper::setRowHeight(int rowHeight)
{
  if (impl_ && impl_->panel) {
    impl_->panel->setRowHeight(rowHeight);
  }
}

int ArtifactLayerTimelinePanelWrapper::rowHeight() const
{
  return impl_ && impl_->panel ? impl_->panel->rowHeight() : kLayerRowHeight;
}

void ArtifactLayerTimelinePanelWrapper::setPropertyColumnWidth(int width)
{
  if (impl_ && impl_->panel) {
    if (impl_->header) {
      impl_->header->setPropertyColumnWidth(width);
    }
    impl_->panel->setPropertyColumnWidth(width);
  }
}

int ArtifactLayerTimelinePanelWrapper::propertyColumnWidth() const
{
  return impl_ && impl_->panel ? impl_->panel->propertyColumnWidth() : kLayerColumnWidth;
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
   return impl_ && impl_->panel ? impl_->panel->searchMatchMode() : SearchMatchMode::FilterOnly;
  }

  QVector<LayerID> ArtifactLayerTimelinePanelWrapper::matchingTimelineRows() const
  {
   return impl_ && impl_->panel ? impl_->panel->matchingTimelineRows() : QVector<LayerID>();
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

} // namespace Artifact
