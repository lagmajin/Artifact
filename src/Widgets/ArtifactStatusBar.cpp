module;

#include <QAction>
#include <QLabel>
#include <QMenu>
#include <QString>

module ArtifactStatusBar;

namespace Artifact
{
 namespace
 {
  QString defaultTextForItem(const ArtifactStatusBar::Item item)
  {
   switch (item)
   {
   case ArtifactStatusBar::Item::Zoom:
    return QStringLiteral("Zoom: 100%");
   case ArtifactStatusBar::Item::Coordinates:
    return QStringLiteral("Pos: -, -");
   case ArtifactStatusBar::Item::Frame:
    return QStringLiteral("Frame: -");
   case ArtifactStatusBar::Item::FPS:
    return QStringLiteral("FPS: -");
   case ArtifactStatusBar::Item::Memory:
   return QStringLiteral("RAM: - MB");
  case ArtifactStatusBar::Item::Project:
   return QStringLiteral("Project: Ready");
  case ArtifactStatusBar::Item::Drops:
   return QStringLiteral("Drops: -");
  }
  return QString();
 }
 }

 ArtifactStatusBar::ArtifactStatusBar(QWidget* parent)
  : QStatusBar(parent)
 {
  setSizeGripEnabled(true);

  labels_[itemIndex(Item::Project)] = new QLabel(defaultTextForItem(Item::Project), this);
  labels_[itemIndex(Item::Coordinates)] = new QLabel(defaultTextForItem(Item::Coordinates), this);
  labels_[itemIndex(Item::Frame)] = new QLabel(defaultTextForItem(Item::Frame), this);
  labels_[itemIndex(Item::Zoom)] = new QLabel(defaultTextForItem(Item::Zoom), this);
  labels_[itemIndex(Item::FPS)] = new QLabel(defaultTextForItem(Item::FPS), this);
  labels_[itemIndex(Item::Memory)] = new QLabel(defaultTextForItem(Item::Memory), this);
  labels_[itemIndex(Item::Drops)] = new QLabel(defaultTextForItem(Item::Drops), this);

  addWidget(labels_[itemIndex(Item::Project)], 1);
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
   label->setText(QStringLiteral("Zoom: %1%").arg(QString::number(zoomPercent, 'f', 1)));
  }
 }

 void ArtifactStatusBar::setCoordinates(const int x, const int y)
 {
  if (auto* label = itemLabel(Item::Coordinates))
  {
   label->setText(QStringLiteral("Pos: %1, %2").arg(x).arg(y));
  }
 }

 void ArtifactStatusBar::setFrame(const int64_t frame)
 {
  if (auto* label = itemLabel(Item::Frame))
  {
   label->setText(QStringLiteral("Frame: %1").arg(frame));
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
   label->setText(QStringLiteral("RAM: %1 MB").arg(memoryMB));
  }
 }

 void ArtifactStatusBar::setProjectText(const QString& text)
 {
  if (auto* label = itemLabel(Item::Project))
  {
   label->setText(QStringLiteral("Project: %1").arg(text));
  }
 }

 void ArtifactStatusBar::setDropSummaryText(const QString& text)
 {
  if (auto* label = itemLabel(Item::Drops))
  {
   label->setText(QStringLiteral("Drops: %1").arg(text));
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
  for (const auto item : { Item::Zoom, Item::Coordinates, Item::Frame, Item::FPS, Item::Memory, Item::Project, Item::Drops })
  {
   setItemVisible(item, visible);
  }
 }

 void ArtifactStatusBar::showReadyMessage()
 {
  showMessage(QStringLiteral("Ready"), 1500);
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
  case Item::Drops: return 6;
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
  case Item::Drops: return QStringLiteral("Drops");
  }
  return QStringLiteral("Unknown");
 }

 void ArtifactStatusBar::rebuildVisibilityMenu(QMenu& menu)
 {
  for (const auto item : { Item::Project, Item::Coordinates, Item::Frame, Item::Zoom, Item::FPS, Item::Memory, Item::Drops })
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
