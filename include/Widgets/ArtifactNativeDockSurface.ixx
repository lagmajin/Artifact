module;

#include <QHash>
#include <QSplitter>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QWidget>

export module Artifact.NativeDockSurface;

import Artifact.DockManager;

export namespace Artifact {

// Minimal backend-independent dock surface.  It intentionally omits floating
// and drag/drop until the registry and persistence paths are proven against
// the current QADS adapter.
class NativeDockSurface final : public QWidget {
public:
  explicit NativeDockSurface(QWidget *parent = nullptr) : QWidget(parent) {
    auto *rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);

    auto *splitter = new QSplitter(Qt::Horizontal, this);
    leftTabs_ = createTabSurface(splitter);
    centerTabs_ = createTabSurface(splitter);
    rightTabs_ = createTabSurface(splitter);
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 3);
    splitter->setStretchFactor(2, 1);
    rootLayout->addWidget(splitter);
  }

  bool addDockWidget(const QString &dockId, const QString &title,
                    QWidget *widget, DockArea area) {
    if (dockId.trimmed().isEmpty() || !widget || docks_.contains(dockId)) {
      return false;
    }
    auto *tabs = tabsForArea(area);
    if (!tabs) {
      return false;
    }
    widget->setParent(tabs);
    tabs->addTab(widget, title);
    docks_.insert(dockId, widget);
    areas_.insert(dockId, area);
    return true;
  }

  bool restoreLayout(const QList<DockLayoutEntry> &entries) {
    bool restoredAny = false;
    for (const auto &entry : entries) {
      auto *widget = docks_.value(entry.dockId);
      if (!widget) {
        continue;
      }
      QString title = entry.dockId;
      if (auto *tabs = qobject_cast<QTabWidget *>(widget->parentWidget())) {
        const int index = tabs->indexOf(widget);
        if (index >= 0) {
          title = tabs->tabText(index);
        }
      }
      const bool removed = removeDockWidget(entry.dockId);
      if (!removed ||
          !addDockWidget(entry.dockId, title, widget, entry.area)) {
        continue;
      }
      widget->setVisible(entry.visible);
      restoredAny = true;
    }
    return restoredAny;
  }

  bool removeDockWidget(const QString &dockId) {
    auto *widget = docks_.take(dockId);
    if (!widget) {
      return false;
    }
    if (auto *tabs = qobject_cast<QTabWidget *>(widget->parentWidget())) {
      const int index = tabs->indexOf(widget);
      if (index >= 0) {
        tabs->removeTab(index);
      }
    }
    widget->setParent(nullptr);
    areas_.remove(dockId);
    return true;
  }

  bool setDockVisible(const QString &dockId, bool visible) {
    auto *widget = docks_.value(dockId);
    if (!widget) {
      return false;
    }
    widget->setVisible(visible);
    return true;
  }

  QWidget *dockWidget(const QString &dockId) const {
    return docks_.value(dockId, nullptr);
  }

  bool supportsFloating() const { return false; }
  bool supportsDragDrop() const { return false; }

private:
  static QTabWidget *createTabSurface(QWidget *parent) {
    auto *tabs = new QTabWidget(parent);
    tabs->setDocumentMode(true);
    tabs->setMovable(true);
    return tabs;
  }

  QTabWidget *tabsForArea(DockArea area) const {
    switch (area) {
    case DockArea::Left:
      return leftTabs_;
    case DockArea::Right:
      return rightTabs_;
    case DockArea::Top:
    case DockArea::Bottom:
    case DockArea::Center:
      return centerTabs_;
    }
    return centerTabs_;
  }

  QTabWidget *leftTabs_ = nullptr;
  QTabWidget *centerTabs_ = nullptr;
  QTabWidget *rightTabs_ = nullptr;
  QHash<QString, QWidget *> docks_;
  QHash<QString, DockArea> areas_;
};

} // namespace Artifact
