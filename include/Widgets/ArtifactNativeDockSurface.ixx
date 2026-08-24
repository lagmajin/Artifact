module;

#include <algorithm>
#include <QHash>
#include <QByteArray>
#include <QColor>
#include <QApplication>
#include <QDebug>
#include <QDrag>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDialog>
#include <QDropEvent>
#include <QEvent>
#include <QMimeData>
#include <QMouseEvent>
#include <QPalette>
#include <QJsonDocument>
#include <QSplitter>
#include <QTabWidget>
#include <QTabBar>
#include <QVBoxLayout>
#include <QStringList>
#include <QWidget>

export module Artifact.NativeDockSurface;

import Artifact.DockManager;
import Widgets.Utils.CSS;

export namespace Artifact {

// Backend-independent dock surface.  Floating docks are represented by owned
// top-level dialogs; the registry remains the stable routing and persistence
// boundary used by both embedded and floating surfaces.
class NativeDockSurface final : public QWidget {
public:
  explicit NativeDockSurface(QWidget *parent = nullptr) : QWidget(parent) {
    setAcceptDrops(true);
    auto *rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);

    topTabs_ = createTabSurface(this);
    topTabs_->hide();
    rootLayout->addWidget(topTabs_);

    auto *splitter = new QSplitter(Qt::Horizontal, this);
    splitter_ = splitter;
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

    for (auto *tabs : {leftTabs_, centerTabs_, rightTabs_, topTabs_,
                       bottomTabs_}) {
      if (!tabs) {
        continue;
      }
      tabs->setAcceptDrops(true);
      tabs->installEventFilter(this);
      tabs->tabBar()->installEventFilter(this);
    }
  }

  ~NativeDockSurface() override {
    const auto dialogs = floatingDialogs_.values();
    for (auto *dialog : dialogs) {
      if (!dialog) {
        continue;
      }
      dialog->hide();
      delete dialog;
    }
    floatingDialogs_.clear();
    floatingWidgets_.clear();
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

  // Add a dock to the same tab surface as an existing dock.  The native MVP
  // uses the dock ID as the stable routing key; callers do not need to know
  // which QTabWidget currently owns either panel.
  bool addDockWidgetToTab(const QString &dockId, const QString &title,
                          QWidget *widget, const QString &targetDockId) {
    if (dockId.trimmed().isEmpty() || !widget || docks_.contains(dockId) ||
        targetDockId.trimmed().isEmpty()) {
      return false;
    }
    auto *target = docks_.value(targetDockId, nullptr);
    auto *tabs = target ? tabsForWidget(target) : nullptr;
    if (!tabs) {
      return false;
    }
    widget->setParent(tabs);
    tabs->addTab(widget, title);
    tabs->setCurrentWidget(widget);
    tabs->show();
    docks_.insert(dockId, widget);
    areas_.insert(dockId, areas_.value(targetDockId, DockArea::Center));
    titles_.insert(dockId, title);
    pinned_.insert(dockId, false);
    return true;
  }

  bool addFloatingDockWidget(const QString &dockId, const QString &title,
                             QWidget *widget, const QRect &geometry,
                             bool visible = true) {
    if (dockId.trimmed().isEmpty() || !widget ||
        docks_.contains(dockId) || floatingDialogs_.contains(dockId)) {
      return false;
    }
    auto *dialog = new QDialog(window());
    dialog->setWindowTitle(title);
    dialog->setObjectName(QStringLiteral("ArtifactNativeFloatingDock_%1")
                              .arg(dockId));
    dialog->setAttribute(Qt::WA_DeleteOnClose, false);
    auto *layout = new QVBoxLayout(dialog);
    layout->setContentsMargins(0, 0, 0, 0);
    widget->setParent(dialog);
    layout->addWidget(widget);
    if (geometry.isValid()) {
      dialog->setGeometry(geometry);
    }
    floatingDialogs_.insert(dockId, dialog);
    floatingWidgets_.insert(dockId, widget);
    titles_.insert(dockId, title);
    areas_.insert(dockId, DockArea::Center);
    pinned_.insert(dockId, false);
    widget->setVisible(visible);
    if (visible) {
      dialog->show();
    }
    return true;
  }

  bool restoreLayout(const QList<DockLayoutEntry> &entries) {
    QList<DockLayoutEntry> normalized;
    QHash<QString, bool> seen;
    for (const auto &entry : entries) {
      if (!isValidDockLayoutEntry(entry) || seen.contains(entry.dockId)) {
        continue;
      }
      if (!canApplyDockLayoutEntry(capabilities(), entry)) {
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
      if (entry.floating) {
        if (auto *dialog = floatingDialogs_.value(entry.dockId, nullptr)) {
          if (entry.floatingGeometry.isValid()) {
            dialog->setGeometry(entry.floatingGeometry);
          }
          dialog->setVisible(entry.visible);
          pinned_.insert(entry.dockId, entry.pinned);
          restoredAny = true;
        }
        continue;
      }
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
    for (auto it = floatingWidgets_.cbegin(); it != floatingWidgets_.cend();
         ++it) {
      DockLayoutEntry entry;
      entry.dockId = it.key();
      entry.area = DockArea::Center;
      entry.visible = it.value() && it.value()->isVisible();
      entry.pinned = pinned_.value(it.key(), false);
      entry.floating = true;
      if (auto *dialog = floatingDialogs_.value(it.key(), nullptr)) {
        entry.floatingGeometry = dialog->geometry();
      }
      document.entries.push_back(entry);
    }
    for (auto &entry : document.entries) {
      if (entry.floating) {
        entry.tabGroup.clear();
        continue;
      }
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
    const QString resolvedId = resolveDockId(dockId);
    if (auto *dialog = floatingDialogs_.take(resolvedId)) {
      floatingWidgets_.remove(resolvedId);
      dialog->hide();
      dialog->deleteLater();
      areas_.remove(resolvedId);
      titles_.remove(resolvedId);
      pinned_.remove(resolvedId);
      return true;
    }
    auto *widget = docks_.take(resolvedId);
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
    areas_.remove(resolvedId);
    titles_.remove(resolvedId);
    pinned_.remove(resolvedId);
    return true;
  }

  bool moveDockWidget(const QString &dockId, DockArea area) {
    const QString resolvedId = resolveDockId(dockId);
    auto *widget = docks_.value(resolvedId);
    if (!widget || areas_.value(resolvedId, DockArea::Center) == area) {
      return widget != nullptr;
    }
    const QString title = titles_.value(resolvedId, resolvedId);
    const bool visible = widget->isVisible();
    const bool pinned = pinned_.value(resolvedId, false);
    const DockArea previousArea = areas_.value(resolvedId, DockArea::Center);
    if (!removeDockWidget(resolvedId) ||
        !addDockWidget(resolvedId, title, widget, area)) {
      if (!docks_.contains(resolvedId)) {
        addDockWidget(resolvedId, title, widget, previousArea);
        pinned_.insert(resolvedId, pinned);
        widget->setVisible(visible);
      }
      return false;
    }
    pinned_.insert(resolvedId, pinned);
    widget->setVisible(visible);
    return true;
  }

  bool moveDockWidgetToTab(const QString &dockId,
                           const QString &targetDockId) {
    const QString resolvedId = resolveDockId(dockId);
    const QString resolvedTargetId = resolveDockId(targetDockId);
    auto *widget = docks_.value(resolvedId, nullptr);
    auto *target = docks_.value(resolvedTargetId, nullptr);
    auto *targetTabs = target ? tabsForWidget(target) : nullptr;
    if (!widget || !target || !targetTabs ||
        resolvedId == resolvedTargetId) {
      return false;
    }
    const QString title = titles_.value(resolvedId, resolvedId);
    const bool visible = widget->isVisible();
    const bool pinned = pinned_.value(resolvedId, false);
    const DockArea previousArea = areas_.value(resolvedId, DockArea::Center);
    const DockArea targetArea = areas_.value(resolvedTargetId, DockArea::Center);
    if (!removeDockWidget(resolvedId) ||
        !addDockWidget(resolvedId, title, widget, targetArea)) {
      if (!docks_.contains(resolvedId)) {
        addDockWidget(resolvedId, title, widget, previousArea);
        pinned_.insert(resolvedId, pinned);
        widget->setVisible(visible);
      }
      return false;
    }
    auto *newTabs = tabsForWidget(widget);
    if (!newTabs || newTabs == targetTabs) {
      widget->setVisible(visible);
      pinned_.insert(resolvedId, pinned);
      return newTabs == targetTabs;
    }
    const int index = newTabs->indexOf(widget);
    if (index >= 0) {
      newTabs->removeTab(index);
    }
    widget->setParent(targetTabs);
    targetTabs->addTab(widget, title);
    targetTabs->setCurrentWidget(widget);
    areas_.insert(resolvedId, targetArea);
    pinned_.insert(resolvedId, pinned);
    widget->setVisible(visible);
    if (newTabs->count() == 0) {
      newTabs->hide();
    }
    return true;
  }

  bool setDockVisible(const QString &dockId, bool visible) {
    const QString resolvedId = resolveDockId(dockId);
    if (auto *dialog = floatingDialogs_.value(resolvedId, nullptr)) {
      dialog->setVisible(visible);
      return true;
    }
    auto *widget = docks_.value(resolvedId);
    if (!widget) {
      return false;
    }
    widget->setVisible(visible);
    return true;
  }

  bool setSplitterSizes(DockArea area, const QList<int> &sizes) {
    if (!splitter_ || sizes.isEmpty()) {
      return false;
    }
    if (area == DockArea::Left || area == DockArea::Center ||
        area == DockArea::Right) {
      splitter_->setSizes(sizes);
      return true;
    }
    return false;
  }

  bool containsDock(const QString &dockId) const {
    return !resolveDockId(dockId).isEmpty();
  }

  QString resolveDockId(const QString &idOrTitle) const {
    if (docks_.contains(idOrTitle) || floatingDialogs_.contains(idOrTitle)) {
      return idOrTitle;
    }
    for (auto it = titles_.cbegin(); it != titles_.cend(); ++it) {
      if (it.value() == idOrTitle) {
        return it.key();
      }
    }
    return {};
  }

  bool containsDockPrefix(const QString &prefix) const {
    if (prefix.trimmed().isEmpty()) {
      return false;
    }
    for (auto it = docks_.cbegin(); it != docks_.cend(); ++it) {
      if (it.key().startsWith(prefix)) {
        return true;
      }
    }
    return false;
  }

  QString dockIdWithPrefix(const QString &prefix) const {
    if (prefix.trimmed().isEmpty()) {
      return {};
    }
    auto ids = docks_.keys();
    std::sort(ids.begin(), ids.end());
    for (const auto &id : ids) {
      if (id.startsWith(prefix)) {
        return id;
      }
    }
    return {};
  }

  QString dockTitle(const QString &dockId) const {
    return titles_.value(resolveDockId(dockId));
  }

  bool setDockPinned(const QString &dockId, bool pinned) {
    const QString resolvedId = resolveDockId(dockId);
    if (resolvedId.isEmpty()) {
      return false;
    }
    pinned_.insert(resolvedId, pinned);
    return true;
  }

  bool activateDock(const QString &dockId) {
    const QString resolvedId = resolveDockId(dockId);
    if (auto *dialog = floatingDialogs_.value(resolvedId, nullptr)) {
      dialog->show();
      dialog->raise();
      dialog->activateWindow();
      return true;
    }
    auto *widget = docks_.value(resolvedId);
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
    const QString resolvedId = resolveDockId(dockId);
    return docks_.value(resolvedId,
                        floatingWidgets_.value(resolvedId, nullptr));
  }

  QString dockIdForWidget(const QWidget *widget) const {
    if (!widget) {
      return {};
    }
    for (auto it = docks_.cbegin(); it != docks_.cend(); ++it) {
      if (it.value() == widget ||
          (it.value() && it.value()->isAncestorOf(widget))) {
        return it.key();
      }
    }
    for (auto it = floatingWidgets_.cbegin();
         it != floatingWidgets_.cend(); ++it) {
      if (it.value() == widget ||
          (it.value() && it.value()->isAncestorOf(widget))) {
        return it.key();
      }
    }
    return {};
  }

  QStringList dockIds() const {
    auto ids = docks_.keys();
    ids.append(floatingWidgets_.keys());
    ids.removeDuplicates();
    ids.sort();
    return ids;
  }

  DockArea dockArea(const QString &dockId) const {
    return areas_.value(resolveDockId(dockId), DockArea::Center);
  }

  bool dockVisible(const QString &dockId) const {
    const QString resolvedId = resolveDockId(dockId);
    if (auto *dialog = floatingDialogs_.value(resolvedId, nullptr)) {
      return dialog->isVisible();
    }
    const auto *widget = docks_.value(resolvedId,
                                      floatingWidgets_.value(resolvedId, nullptr));
    return widget && widget->isVisible();
  }

  bool dockPinned(const QString &dockId) const {
    return pinned_.value(resolveDockId(dockId), false);
  }

  DockBackendKind backendKind() const { return DockBackendKind::Native; }

  DockBackendCapabilities capabilities() const {
    return DockBackendCapabilities{true, true, true};
  }

protected:
  void dragEnterEvent(QDragEnterEvent *event) override {
    if (event->mimeData()->hasFormat(
            QStringLiteral("application/x-artifact-dock-id"))) {
      event->acceptProposedAction();
      return;
    }
    QWidget::dragEnterEvent(event);
  }

  void dropEvent(QDropEvent *event) override {
    const QByteArray payload = event->mimeData()->data(
        QStringLiteral("application/x-artifact-dock-id"));
    if (!payload.isEmpty()) {
      const QString sourceId = QString::fromUtf8(payload);
      if (moveDockWidget(sourceId, areaForPosition(event->position().toPoint()))) {
        event->acceptProposedAction();
        return;
      }
    }
    QWidget::dropEvent(event);
  }

  bool eventFilter(QObject *watched, QEvent *event) override {
    auto *tabs = qobject_cast<QTabWidget *>(watched);
    auto *tabBar = qobject_cast<QTabBar *>(watched);
    if (!tabs && tabBar) {
      tabs = qobject_cast<QTabWidget *>(tabBar->parentWidget());
    }
    if (!tabs) {
      return QWidget::eventFilter(watched, event);
    }

    if (event->type() == QEvent::MouseButtonPress && tabBar) {
      auto *mouseEvent = static_cast<QMouseEvent *>(event);
      if (mouseEvent->button() == Qt::LeftButton) {
        const int index = tabBar->tabAt(mouseEvent->position().toPoint());
        dragSourceId_ = index >= 0
                            ? dockIdForWidget(tabs->widget(index))
                            : QString{};
        dragStartPosition_ = mouseEvent->position().toPoint();
      }
    } else if (event->type() == QEvent::MouseMove && tabBar &&
               !dragSourceId_.isEmpty()) {
      auto *mouseEvent = static_cast<QMouseEvent *>(event);
      if (!(mouseEvent->buttons() & Qt::LeftButton) ||
          (mouseEvent->position().toPoint() - dragStartPosition_).manhattanLength() <
              QApplication::startDragDistance()) {
        return QWidget::eventFilter(watched, event);
      }
      auto *mime = new QMimeData();
      mime->setData(QStringLiteral("application/x-artifact-dock-id"),
                    dragSourceId_.toUtf8());
      auto *drag = new QDrag(tabBar);
      drag->setMimeData(mime);
      drag->exec(Qt::MoveAction);
      dragSourceId_.clear();
      return true;
    } else if (event->type() == QEvent::DragEnter ||
               event->type() == QEvent::DragMove) {
      auto *dragEvent = static_cast<QDragMoveEvent *>(event);
      if (dragEvent->mimeData()->hasFormat(
              QStringLiteral("application/x-artifact-dock-id"))) {
        dragEvent->acceptProposedAction();
        return true;
      }
    } else if (event->type() == QEvent::Drop) {
      auto *dropEvent = static_cast<QDropEvent *>(event);
      const QByteArray payload = dropEvent->mimeData()->data(
          QStringLiteral("application/x-artifact-dock-id"));
      if (!payload.isEmpty()) {
        const QString sourceId = QString::fromUtf8(payload);
        const QPoint position = dropEvent->position().toPoint();
        const QPoint tabPosition = tabBar
                                       ? position
                                       : tabs->tabBar()->mapFrom(tabs, position);
        const int targetIndex = tabs->tabBar()->tabAt(tabPosition);
        if (targetIndex >= 0) {
          const QString targetId = dockIdForWidget(tabs->widget(targetIndex));
          if (!targetId.isEmpty() && sourceId != targetId &&
              moveDockWidgetToTab(sourceId, targetId)) {
            dropEvent->acceptProposedAction();
            return true;
          }
        }
      }
    }
    return QWidget::eventFilter(watched, event);
  }

private:
  DockArea areaForPosition(const QPoint &position) const {
    if (topTabs_ && topTabs_->isVisible() &&
        topTabs_->geometry().contains(position)) {
      return DockArea::Top;
    }
    if (bottomTabs_ && bottomTabs_->isVisible() &&
        bottomTabs_->geometry().contains(position)) {
      return DockArea::Bottom;
    }
    if (splitter_ && splitter_->geometry().contains(position)) {
      const int center = splitter_->geometry().center().x();
      if (position.x() < center - splitter_->width() / 6) {
        return DockArea::Left;
      }
      if (position.x() > center + splitter_->width() / 6) {
        return DockArea::Right;
      }
    }
    return DockArea::Center;
  }

  static QTabWidget *createTabSurface(QWidget *parent) {
    auto *tabs = new QTabWidget(parent);
    tabs->setDocumentMode(true);
    tabs->setMovable(true);
    tabs->tabBar()->setExpanding(false);
    tabs->tabBar()->setUsesScrollButtons(true);
    tabs->tabBar()->setElideMode(Qt::ElideRight);
    const auto &theme = ArtifactCore::currentDCCTheme();
    QPalette palette = tabs->palette();
    palette.setColor(QPalette::Window,
                     QColor(theme.secondaryBackgroundColor));
    palette.setColor(QPalette::Base, QColor(theme.backgroundColor));
    palette.setColor(QPalette::Button,
                     QColor(theme.secondaryBackgroundColor));
    palette.setColor(QPalette::ButtonText, QColor(theme.textColor));
    palette.setColor(QPalette::Text, QColor(theme.textColor));
    palette.setColor(QPalette::Highlight, QColor(theme.accentColor));
    palette.setColor(QPalette::HighlightedText, QColor(theme.textColor));
    tabs->setPalette(palette);
    tabs->tabBar()->setPalette(palette);
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
  QSplitter *splitter_ = nullptr;
  QPoint dragStartPosition_;
  QString dragSourceId_;
  QHash<QString, QWidget *> docks_;
  QHash<QString, QDialog *> floatingDialogs_;
  QHash<QString, QWidget *> floatingWidgets_;
  QHash<QString, DockArea> areas_;
  QHash<QString, QString> titles_;
  QHash<QString, bool> pinned_;
};

} // namespace Artifact
