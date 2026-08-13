module;

#include <algorithm>
#include <QHash>
#include <QByteArray>
#include <QDebug>
#include <QJsonDocument>
#include <QSplitter>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QStringList>
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

    topTabs_ = createTabSurface(this);
    topTabs_->hide();
    rootLayout->addWidget(topTabs_);

    auto *splitter = new QSplitter(Qt::Horizontal, this);
    leftTabs_ = createTabSurface(splitter);
    centerTabs_ = createTabSurface(splitter);
    rightTabs_ = createTabSurface(splitter);
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 3);
    splitter->setStretchFactor(2, 1);
    rootLayout->addWidget(splitter);

    bottomTabs_ = createTabSurface(this);
    bottomTabs_->hide();
    rootLayout->addWidget(bottomTabs_);
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
    tabs->show();
    docks_.insert(dockId, widget);
    areas_.insert(dockId, area);
    titles_.insert(dockId, title);
    pinned_.insert(dockId, false);
    return true;
  }

  bool restoreLayout(const QList<DockLayoutEntry> &entries) {
    QList<DockLayoutEntry> normalized;
    QHash<QString, bool> seen;
    for (const auto &entry : entries) {
      if (!isValidDockLayoutEntry(entry) || seen.contains(entry.dockId)) {
        continue;
      }
      if (entry.floating && !capabilities().supportsFloating) {
        qWarning() << "[NativeDockSurface] skipping unsupported floating dock"
                   << entry.dockId;
        continue;
      }
      seen.insert(entry.dockId, true);
      normalized.push_back(entry);
    }
    std::sort(normalized.begin(), normalized.end(),
              [](const DockLayoutEntry &left, const DockLayoutEntry &right) {
                return left.dockId < right.dockId;
              });

    bool restoredAny = false;
    for (const auto &entry : normalized) {
      auto *widget = docks_.value(entry.dockId);
      if (!widget) {
        continue;
      }
      const QString title = titles_.value(entry.dockId, entry.dockId);
      const DockArea previousArea =
          areas_.value(entry.dockId, DockArea::Center);
      const bool previousPinned = pinned_.value(entry.dockId, false);
      const bool removed = removeDockWidget(entry.dockId);
      if (!removed ||
          !addDockWidget(entry.dockId, title, widget, entry.area)) {
        if (removed) {
          if (addDockWidget(entry.dockId, title, widget, previousArea)) {
            pinned_.insert(entry.dockId, previousPinned);
          }
        }
        continue;
      }
      widget->setVisible(entry.visible);
      pinned_.insert(entry.dockId, entry.pinned);
      restoredAny = true;
    }
    return restoredAny;
  }

  QByteArray saveLayoutState() const {
    DockLayoutDocument document;
    QHash<int, QStringList> areaIds;
    for (auto it = docks_.cbegin(); it != docks_.cend(); ++it) {
      DockLayoutEntry entry;
      entry.dockId = it.key();
      entry.area = areas_.value(it.key(), DockArea::Center);
      entry.visible = it.value() && it.value()->isVisible();
      entry.pinned = pinned_.value(it.key(), false);
      areaIds[static_cast<int>(entry.area)].push_back(entry.dockId);
      document.entries.push_back(entry);
    }
    for (auto &entry : document.entries) {
      auto ids = areaIds.value(static_cast<int>(entry.area));
      ids.removeDuplicates();
      ids.sort();
      entry.tabGroup = QStringLiteral("tabs:") + ids.join(QStringLiteral("|"));
    }
    return QJsonDocument(document.toJson()).toJson(QJsonDocument::Compact);
  }

  bool restoreLayoutState(const QByteArray &state) {
    const auto json = QJsonDocument::fromJson(state);
    if (json.isObject()) {
      const auto document = DockLayoutDocument::fromJson(json.object());
      if (document.version != kDockLayoutDocumentVersion) {
        return false;
      }
      return restoreLayout(document.entries);
    }
    if (json.isArray()) {
      QList<DockLayoutEntry> entries;
      for (const auto &value : json.array()) {
        if (!value.isObject()) {
          continue;
        }
        const auto entry = dockLayoutEntryFromJson(value.toObject());
        if (isValidDockLayoutEntry(entry)) {
          entries.push_back(entry);
        }
      }
      return restoreLayout(entries);
    }
    return false;
  }

  bool removeDockWidget(const QString &dockId) {
    auto *widget = docks_.take(dockId);
    if (!widget) {
      return false;
    }
    if (auto *tabs = tabsForWidget(widget)) {
      const int index = tabs->indexOf(widget);
      if (index >= 0) {
        tabs->removeTab(index);
        if (tabs->count() == 0) {
          tabs->hide();
        }
      }
    }
    widget->setParent(nullptr);
    areas_.remove(dockId);
    titles_.remove(dockId);
    pinned_.remove(dockId);
    return true;
  }

  bool moveDockWidget(const QString &dockId, DockArea area) {
    auto *widget = docks_.value(dockId);
    if (!widget || areas_.value(dockId, DockArea::Center) == area) {
      return widget != nullptr;
    }
    const QString title = titles_.value(dockId, dockId);
    const bool visible = widget->isVisible();
    const bool pinned = pinned_.value(dockId, false);
    const DockArea previousArea = areas_.value(dockId, DockArea::Center);
    if (!removeDockWidget(dockId) ||
        !addDockWidget(dockId, title, widget, area)) {
      if (!docks_.contains(dockId)) {
        addDockWidget(dockId, title, widget, previousArea);
        pinned_.insert(dockId, pinned);
        widget->setVisible(visible);
      }
      return false;
    }
    pinned_.insert(dockId, pinned);
    widget->setVisible(visible);
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

  bool setDockPinned(const QString &dockId, bool pinned) {
    if (!docks_.contains(dockId)) {
      return false;
    }
    pinned_.insert(dockId, pinned);
    return true;
  }

  bool activateDock(const QString &dockId) {
    auto *widget = docks_.value(dockId);
    if (!widget) {
      return false;
    }
    auto *tabs = tabsForWidget(widget);
    if (!tabs) {
      return false;
    }
    const int index = tabs->indexOf(widget);
    if (index < 0) {
      return false;
    }
    tabs->setCurrentIndex(index);
    widget->show();
    return true;
  }

  QWidget *dockWidget(const QString &dockId) const {
    return docks_.value(dockId, nullptr);
  }

  QStringList dockIds() const { return docks_.keys(); }

  DockArea dockArea(const QString &dockId) const {
    return areas_.value(dockId, DockArea::Center);
  }

  DockBackendKind backendKind() const { return DockBackendKind::Native; }

  DockBackendCapabilities capabilities() const {
    return DockBackendCapabilities{};
  }

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
      return topTabs_;
    case DockArea::Bottom:
      return bottomTabs_;
    case DockArea::Center:
      return centerTabs_;
    }
    return centerTabs_;
  }

  QTabWidget *tabsForWidget(QWidget *widget) const {
    for (auto *tabs : {leftTabs_, centerTabs_, rightTabs_, topTabs_,
                       bottomTabs_}) {
      if (tabs && tabs->indexOf(widget) >= 0) {
        return tabs;
      }
    }
    return nullptr;
  }

  QTabWidget *topTabs_ = nullptr;
  QTabWidget *leftTabs_ = nullptr;
  QTabWidget *centerTabs_ = nullptr;
  QTabWidget *rightTabs_ = nullptr;
  QTabWidget *bottomTabs_ = nullptr;
  QHash<QString, QWidget *> docks_;
  QHash<QString, DockArea> areas_;
  QHash<QString, QString> titles_;
  QHash<QString, bool> pinned_;
};

} // namespace Artifact
