module;

#include <QAction>
#include <QColor>
#include <QFont>
#include <QLabel>
#include <QMenu>
#include <QPalette>
#include <QString>

module ArtifactStatusBar;

import Widgets.Utils.CSS;

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
  case ArtifactStatusBar::Item::Drops:
   return QStringLiteral("DROP: -");
  case ArtifactStatusBar::Item::TimelineDebug:
   return QStringLiteral("READY");
  case ArtifactStatusBar::Item::Console:
   return QStringLiteral("LOGS: 0E 0W");
  }
  return QString();
 }
 }

 ArtifactStatusBar::ArtifactStatusBar(QWidget* parent)
  : QStatusBar(parent)
 {
  const QColor backgroundColor = QColor(ArtifactCore::currentDCCTheme().backgroundColor);
  const QColor surfaceColor = QColor(ArtifactCore::currentDCCTheme().secondaryBackgroundColor);
  const QColor textColor = QColor(ArtifactCore::currentDCCTheme().textColor);
  const QColor mutedTextColor = textColor.darker(130);
  const QColor accentColor = QColor(ArtifactCore::currentDCCTheme().accentColor);
  const QColor dangerColor = QColor(QStringLiteral("#E91E63"));
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
      labels_[i]->setAlignment(Qt::AlignCenter);
      labels_[i]->setAutoFillBackground(false);
      QPalette labelPalette = labels_[i]->palette();
      labelPalette.setColor(QPalette::WindowText, mutedTextColor);
      labels_[i]->setPalette(labelPalette);
  }

  QFont boldFont = font();
  boldFont.setBold(true);
  labels_[itemIndex(Item::TimelineDebug)]->setFont(boldFont);
  labels_[itemIndex(Item::Memory)]->setMinimumWidth(100);
  labels_[itemIndex(Item::FPS)]->setMinimumWidth(80);
  labels_[itemIndex(Item::Zoom)]->setMinimumWidth(80);
  labels_[itemIndex(Item::Coordinates)]->setMinimumWidth(110);
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
  addPermanentWidget(labels_[itemIndex(Item::Zoom)]);
  addPermanentWidget(labels_[itemIndex(Item::FPS)]);
  addPermanentWidget(labels_[itemIndex(Item::Memory)]);
  addPermanentWidget(labels_[itemIndex(Item::Drops)]);
 }

 ArtifactStatusBar::~ArtifactStatusBar() = default;

 void ArtifactStatusBar::setZoomPercent(const float zoomPercent)
 {
  if (auto* label = itemLabel(Item::Zoom))
  {
   label->setText(QStringLiteral("ZOOM: %1%").arg(static_cast<int>(zoomPercent)));
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
  if (auto* label = itemLabel(Item::FPS))
  {
   label->setText(QStringLiteral("FPS: %1").arg(QString::number(fps, 'f', 1)));
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
   label->setText(text.toUpper());
  }
 }

 void ArtifactStatusBar::setConsoleSummary(const int errors, const int warnings)
 {
  if (auto* label = itemLabel(Item::Console))
  {
   label->setText(QStringLiteral("LOGS: %1E %2W").arg(errors).arg(warnings));
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
  for (const auto item : { Item::Zoom, Item::Coordinates, Item::Frame, Item::FPS, Item::Memory, Item::Project, Item::Layer, Item::Drops, Item::TimelineDebug, Item::Console })
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
  menu.exec(event->globalPos());
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
  case Item::Drops: return 7;
  case Item::TimelineDebug: return 8;
  case Item::Console: return 9;
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
  case Item::Drops: return QStringLiteral("Drops");
  case Item::TimelineDebug: return QStringLiteral("Timeline Debug");
  case Item::Console: return QStringLiteral("Console");
  }
  return QStringLiteral("Unknown");
 }

 void ArtifactStatusBar::rebuildVisibilityMenu(QMenu& menu)
 {
  for (const auto item : { Item::Project, Item::Layer, Item::Coordinates, Item::Frame, Item::Zoom, Item::FPS, Item::Memory, Item::Drops, Item::TimelineDebug, Item::Console })
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
