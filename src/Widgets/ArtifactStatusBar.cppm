module;

#include <algorithm>
#include <cmath>
#include <QAction>
#include <QColor>
#include <QFont>
#include <QLabel>
#include <QMenu>
#include <QPalette>
#include <QSizePolicy>
#include <QString>
#include <QMetaObject>
#include <QThread>

module ArtifactStatusBar;

import Widgets.Utils.CSS;
import Settings.Accessibility;
import Artifact.Event.Types;

namespace Artifact
{
 namespace
 {
  QString defaultTextForItem(const ArtifactStatusBar::Item item)
  {
   switch (item)
   {
   case ArtifactStatusBar::Item::Zoom:
    return QStringLiteral("ZOOM: 100%");
   case ArtifactStatusBar::Item::Coordinates:
    return QStringLiteral("X: - | Y: -");
   case ArtifactStatusBar::Item::Frame:
    return QStringLiteral("FRM: -");
   case ArtifactStatusBar::Item::FPS:
    return QStringLiteral("FPS: -");
   case ArtifactStatusBar::Item::Memory:
   return QStringLiteral("MEM: - MB");
  case ArtifactStatusBar::Item::Project:
   return QStringLiteral("PROJECT: READY");
  case ArtifactStatusBar::Item::Layer:
   return QStringLiteral("LAYER: -");
  case ArtifactStatusBar::Item::Selection:
   return QStringLiteral("SEL: 0");
  case ArtifactStatusBar::Item::Drops:
   return QStringLiteral("DROP: -");
  case ArtifactStatusBar::Item::TimelineDebug:
   return QStringLiteral("READY");
  case ArtifactStatusBar::Item::Console:
   return QStringLiteral("LOGS: 0E 0W");
  case ArtifactStatusBar::Item::Accessibility:
   return QStringLiteral("A11Y: OFF");
  }
  return QString();
 }

 QString accessibleNameForItem(const ArtifactStatusBar::Item item)
 {
  switch (item)
  {
  case ArtifactStatusBar::Item::Zoom: return QStringLiteral("Zoom status");
  case ArtifactStatusBar::Item::Coordinates: return QStringLiteral("Coordinates status");
  case ArtifactStatusBar::Item::Frame: return QStringLiteral("Current frame status");
  case ArtifactStatusBar::Item::FPS: return QStringLiteral("Frame rate status");
  case ArtifactStatusBar::Item::Memory: return QStringLiteral("Memory status");
  case ArtifactStatusBar::Item::Project: return QStringLiteral("Project status");
  case ArtifactStatusBar::Item::Layer: return QStringLiteral("Layer status");
  case ArtifactStatusBar::Item::Selection: return QStringLiteral("Selection count status");
  case ArtifactStatusBar::Item::Drops: return QStringLiteral("Dropped frames status");
  case ArtifactStatusBar::Item::TimelineDebug: return QStringLiteral("Timeline debug status");
  case ArtifactStatusBar::Item::Console: return QStringLiteral("Console status");
  case ArtifactStatusBar::Item::Accessibility: return QStringLiteral("Accessibility status");
  }
  return QStringLiteral("Application status");
 }

 QString objectNameForItem(const ArtifactStatusBar::Item item)
 {
  switch (item)
  {
  case ArtifactStatusBar::Item::Zoom: return QStringLiteral("ZoomStatusLabel");
  case ArtifactStatusBar::Item::Coordinates: return QStringLiteral("CoordinatesStatusLabel");
  case ArtifactStatusBar::Item::Frame: return QStringLiteral("FrameStatusLabel");
  case ArtifactStatusBar::Item::FPS: return QStringLiteral("FpsStatusLabel");
  case ArtifactStatusBar::Item::Memory: return QStringLiteral("MemoryStatusLabel");
  case ArtifactStatusBar::Item::Project: return QStringLiteral("ProjectStatusLabel");
  case ArtifactStatusBar::Item::Layer: return QStringLiteral("LayerStatusLabel");
  case ArtifactStatusBar::Item::Selection: return QStringLiteral("SelectionStatusLabel");
  case ArtifactStatusBar::Item::Drops: return QStringLiteral("DropsStatusLabel");
  case ArtifactStatusBar::Item::TimelineDebug: return QStringLiteral("TimelineDebugStatusLabel");
  case ArtifactStatusBar::Item::Console: return QStringLiteral("ConsoleStatusLabel");
  case ArtifactStatusBar::Item::Accessibility: return QStringLiteral("AccessibilityStatusLabel");
  }
  return QStringLiteral("StatusLabel");
 }
 }

 ArtifactStatusBar::ArtifactStatusBar(QWidget* parent)
  : QStatusBar(parent)
 {
  setAccessibleName(QStringLiteral("Status Bar"));
  setAccessibleDescription(
      QStringLiteral("Shows project, layer, playback, display, and diagnostic status."));
  const QColor backgroundColor = QColor(ArtifactCore::currentDCCTheme().backgroundColor);
  const QColor surfaceColor = QColor(ArtifactCore::currentDCCTheme().secondaryBackgroundColor);
  const QColor textColor = QColor(ArtifactCore::currentDCCTheme().textColor);
  const QColor mutedTextColor = Accessibility::adjustColorForDeficiency(
      textColor.darker(130));
  const QColor accentColor = Accessibility::adjustColorForDeficiency(
      QColor(ArtifactCore::currentDCCTheme().accentColor));
  const QColor dangerColor = Accessibility::adjustColorForDeficiency(
      QColor(QStringLiteral("#E91E63")));
  const QColor borderColor = QColor(ArtifactCore::currentDCCTheme().borderColor);

  setSizeGripEnabled(true);
  setAutoFillBackground(true);
  QPalette statusPalette = palette();
  statusPalette.setColor(QPalette::Window, backgroundColor);
  statusPalette.setColor(QPalette::Base, surfaceColor);
  statusPalette.setColor(QPalette::WindowText, textColor);
  statusPalette.setColor(QPalette::Mid, borderColor);
  setPalette(statusPalette);

  for (int i = 0; i < kItemCount; ++i) {
      labels_[i] = new QLabel(defaultTextForItem(static_cast<Item>(i)), this);
      labels_[i]->setObjectName(
          objectNameForItem(static_cast<Item>(i)));
      labels_[i]->setAccessibleName(
          accessibleNameForItem(static_cast<Item>(i)));
      labels_[i]->setAccessibleDescription(
          QStringLiteral("Live application status information."));
      labels_[i]->setAlignment(Qt::AlignCenter);
      labels_[i]->setAutoFillBackground(false);
      QPalette labelPalette = labels_[i]->palette();
      labelPalette.setColor(QPalette::WindowText, mutedTextColor);
      labels_[i]->setPalette(labelPalette);
  }

  QFont boldFont = font();
  boldFont.setBold(true);
  labels_[itemIndex(Item::TimelineDebug)]->setFont(boldFont);
  labels_[itemIndex(Item::TimelineDebug)]->setFixedWidth(220);
  labels_[itemIndex(Item::TimelineDebug)]->setSizePolicy(
      QSizePolicy::Fixed, QSizePolicy::Preferred);
  labels_[itemIndex(Item::Memory)]->setMinimumWidth(100);
  labels_[itemIndex(Item::FPS)]->setMinimumWidth(80);
  labels_[itemIndex(Item::Zoom)]->setMinimumWidth(80);
  labels_[itemIndex(Item::Coordinates)]->setMinimumWidth(110);
  labels_[itemIndex(Item::Selection)]->setMinimumWidth(60);
  labels_[itemIndex(Item::Console)]->setFont(boldFont);
  {
      QPalette p = labels_[itemIndex(Item::TimelineDebug)]->palette();
      p.setColor(QPalette::WindowText, accentColor);
      labels_[itemIndex(Item::TimelineDebug)]->setPalette(p);
  }
  {
      QPalette p = labels_[itemIndex(Item::Console)]->palette();
      p.setColor(QPalette::WindowText, dangerColor);
      labels_[itemIndex(Item::Console)]->setPalette(p);
  }

  addWidget(labels_[itemIndex(Item::TimelineDebug)]);
  addWidget(labels_[itemIndex(Item::Project)], 1);
  addWidget(labels_[itemIndex(Item::Layer)], 1);
  
  addPermanentWidget(labels_[itemIndex(Item::Console)]);
  addPermanentWidget(labels_[itemIndex(Item::Coordinates)]);
  addPermanentWidget(labels_[itemIndex(Item::Frame)]);
  addPermanentWidget(labels_[itemIndex(Item::Selection)]);
  addPermanentWidget(labels_[itemIndex(Item::Zoom)]);
  addPermanentWidget(labels_[itemIndex(Item::FPS)]);
  addPermanentWidget(labels_[itemIndex(Item::Memory)]);
  addPermanentWidget(labels_[itemIndex(Item::Drops)]);
  addPermanentWidget(labels_[itemIndex(Item::Accessibility)]);

  eventBusSubscriptions_.push_back(
      eventBus_.subscribe<TimelineZoomLevelChangedEvent>(
          [this](const TimelineZoomLevelChangedEvent& event) {
            const auto dispatch = [this, zoomPercent = event.zoomPercent]() {
              setZoomPercent(static_cast<float>(zoomPercent));
            };
            if (QThread::currentThread() == thread()) {
              dispatch();
            } else {
              QMetaObject::invokeMethod(this, dispatch, Qt::QueuedConnection);
            }
          }));
  eventBusSubscriptions_.push_back(
      eventBus_.subscribe<TimelineDebugMessageEvent>(
          [this](const TimelineDebugMessageEvent& event) {
            const auto dispatch = [this, message = event.message]() {
              setTimelineDebugText(message);
            };
            if (QThread::currentThread() == thread()) {
              dispatch();
            } else {
              QMetaObject::invokeMethod(this, dispatch, Qt::QueuedConnection);
            }
          }));
 }

 ArtifactStatusBar::~ArtifactStatusBar() = default;

 void ArtifactStatusBar::setZoomPercent(const float zoomPercent)
 {
  const float normalized = std::isfinite(zoomPercent)
      ? std::clamp(zoomPercent, 0.0f, 100000.0f)
      : 100.0f;
  if (auto* label = itemLabel(Item::Zoom))
  {
   label->setText(QStringLiteral("ZOOM: %1%").arg(static_cast<int>(normalized)));
  }
 }

 void ArtifactStatusBar::setCoordinates(const int x, const int y)
 {
  if (auto* label = itemLabel(Item::Coordinates))
  {
   label->setText(QStringLiteral("X: %1 | Y: %2").arg(x).arg(y));
  }
 }

 void ArtifactStatusBar::setFrame(const int64_t frame)
 {
  if (auto* label = itemLabel(Item::Frame))
  {
   label->setText(QStringLiteral("FRM: %1").arg(frame));
  }
 }

 void ArtifactStatusBar::setFPS(const double fps)
 {
  const double normalized = std::isfinite(fps) ? std::max(0.0, fps) : 0.0;
  if (auto* label = itemLabel(Item::FPS))
  {
   label->setText(QStringLiteral("FPS: %1").arg(QString::number(normalized, 'f', 1)));
  }
 }

 void ArtifactStatusBar::setMemoryMB(const quint64 memoryMB)
 {
  if (auto* label = itemLabel(Item::Memory))
  {
   label->setText(QStringLiteral("MEM: %1 MB").arg(memoryMB));
  }
 }

 void ArtifactStatusBar::setProjectText(const QString& text)
 {
  if (auto* label = itemLabel(Item::Project))
  {
   label->setText(QStringLiteral("PROJECT: %1").arg(text.toUpper()));
  }
 }

 void ArtifactStatusBar::setLayerText(const QString& text)
 {
  if (auto* label = itemLabel(Item::Layer))
  {
   label->setText(QStringLiteral("LAYER: %1").arg(text.isEmpty() ? "-" : text.toUpper()));
  }
 }

 void ArtifactStatusBar::setSelectionCount(const int count)
 {
  if (auto* label = itemLabel(Item::Selection))
  {
   label->setText(QStringLiteral("SEL: %1").arg(std::max(0, count)));
  }
 }

 void ArtifactStatusBar::setDropSummaryText(const QString& text)
 {
  if (auto* label = itemLabel(Item::Drops))
  {
   label->setText(QStringLiteral("DROP: %1").arg(text.toUpper()));
  }
 }

 void ArtifactStatusBar::setTimelineDebugText(const QString& text)
 {
  if (auto* label = itemLabel(Item::TimelineDebug))
  {
   const QString fullText = text.toUpper();
   label->setText(label->fontMetrics().elidedText(
       fullText, Qt::ElideRight,
       label->contentsRect().width() > 0 ? label->contentsRect().width() : 1));
   label->setToolTip(fullText);
  }
 }

 void ArtifactStatusBar::setConsoleSummary(const int errors, const int warnings)
 {
  if (auto* label = itemLabel(Item::Console))
  {
   label->setText(QStringLiteral("LOGS: %1E %2W").arg(errors).arg(warnings));
  }
 }

 void ArtifactStatusBar::setAccessibilityText(const QString& text)
 {
  if (auto* label = itemLabel(Item::Accessibility))
  {
   label->setText(QStringLiteral("A11Y: %1").arg(text));
   label->setToolTip(QStringLiteral("Accessibility modifier state: %1").arg(text));
  }
 }

 void ArtifactStatusBar::setCompositionInfo(const QString& name, const int width, const int height, const double fps)
 {
  if (auto* label = itemLabel(Item::Project))
  {
   // フォーマット：名前 (解像度，fps)
   label->setText(QStringLiteral("PROJECT: %1 (%2x%3, %4fps)")
    .arg(name.isEmpty() ? QStringLiteral("NO NAME") : name)
    .arg(width)
    .arg(height)
    .arg(fps, 0, 'f', 0));
  }
 }

 void ArtifactStatusBar::setItemVisible(const Item item, const bool visible)
 {
  if (auto* label = itemLabel(item))
  {
   label->setVisible(visible);
  }
 }

 bool ArtifactStatusBar::isItemVisible(const Item item) const
 {
  if (auto* label = itemLabel(item))
  {
   return label->isVisible();
  }
  return false;
 }

 void ArtifactStatusBar::setAllItemsVisible(const bool visible)
 {
  for (const auto item : { Item::Zoom, Item::Coordinates, Item::Frame, Item::FPS, Item::Memory, Item::Project, Item::Layer, Item::Selection, Item::Drops, Item::TimelineDebug, Item::Console, Item::Accessibility })
  {
   setItemVisible(item, visible);
  }
 }

 void ArtifactStatusBar::showReadyMessage()
 {
  showMessage(QStringLiteral("READY"), 1500);
 }

 void ArtifactStatusBar::contextMenuEvent(QContextMenuEvent* event)
 {
  QMenu menu(this);
  rebuildVisibilityMenu(menu);
  int menuX = event->globalPos().x();
  int menuY = event->globalPos().y();
  Accessibility::adjustContextMenuPosition(menuX, menuY,
                                            menu.sizeHint().width());
  menu.exec(QPoint(menuX, menuY));
 }

 QLabel* ArtifactStatusBar::itemLabel(const Item item) const
 {
  const int idx = itemIndex(item);
  if (idx < 0 || idx >= kItemCount)
  {
   return nullptr;
  }
  return labels_[idx];
 }

 int ArtifactStatusBar::itemIndex(const Item item)
 {
  switch (item)
  {
  case Item::Zoom: return 0;
  case Item::Coordinates: return 1;
  case Item::Frame: return 2;
  case Item::FPS: return 3;
  case Item::Memory: return 4;
  case Item::Project: return 5;
  case Item::Layer: return 6;
  case Item::Selection: return 7;
  case Item::Drops: return 8;
  case Item::TimelineDebug: return 9;
  case Item::Console: return 10;
  case Item::Accessibility: return 11;
  }
  return -1;
 }

 QString ArtifactStatusBar::itemTitle(const Item item) const
 {
  switch (item)
  {
  case Item::Zoom: return QStringLiteral("Zoom");
  case Item::Coordinates: return QStringLiteral("Coordinates");
  case Item::Frame: return QStringLiteral("Frame");
  case Item::FPS: return QStringLiteral("FPS");
  case Item::Memory: return QStringLiteral("Memory");
  case Item::Project: return QStringLiteral("Project");
  case Item::Layer: return QStringLiteral("Layer");
  case Item::Selection: return QStringLiteral("Selection");
  case Item::Drops: return QStringLiteral("Drops");
  case Item::TimelineDebug: return QStringLiteral("Timeline Debug");
  case Item::Console: return QStringLiteral("Console");
  case Item::Accessibility: return QStringLiteral("Accessibility");
  }
  return QStringLiteral("Unknown");
 }

 void ArtifactStatusBar::rebuildVisibilityMenu(QMenu& menu)
 {
  for (const auto item : { Item::Project, Item::Layer, Item::Selection, Item::Coordinates, Item::Frame, Item::Zoom, Item::FPS, Item::Memory, Item::Drops, Item::TimelineDebug, Item::Console })
  {
   QAction* action = menu.addAction(itemTitle(item));
   action->setCheckable(true);
   action->setChecked(isItemVisible(item));
   QObject::connect(action, &QAction::toggled, this, [this, item](const bool checked) {
    setItemVisible(item, checked);
   });
  }

  menu.addSeparator();
  QAction* showAll = menu.addAction(QStringLiteral("Show All"));
  QObject::connect(showAll, &QAction::triggered, this, [this]() { setAllItemsVisible(true); });
 }
}
