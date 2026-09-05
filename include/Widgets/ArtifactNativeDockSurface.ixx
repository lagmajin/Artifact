module;

#include <algorithm>
#include <limits>
#include <QHash>
#include <QByteArray>
#include <QColor>
#include <QApplication>
#include <QDebug>
#include <QDrag>
#include <QDragEnterEvent>
#include <QDragLeaveEvent>
#include <QDragMoveEvent>
#include <QDialog>
#include <QDropEvent>
#include <QEvent>
#include <QMimeData>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QIcon>
#include <QToolButton>
#include <QStyle>
#include <QProxyStyle>
#include <QStyleOptionTab>
#include <QPainter>
#include <QPaintEvent>
#include <QPen>
#include <QVariant>
#include <QPalette>
#include <QLabel>
#include <QSizePolicy>
#include <QJsonDocument>
#include <QSplitter>
#include <QTabWidget>
#include <QTabBar>
#include <QHBoxLayout>
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
  class DockDropPreview final : public QWidget {
  public:
    explicit DockDropPreview(QWidget *parent) : QWidget(parent) {
      setAttribute(Qt::WA_TransparentForMouseEvents, true);
      hide();
    }
  protected:
    void paintEvent(QPaintEvent *) override {
      QPainter painter(this);
      const QColor accent = palette().color(QPalette::Highlight);
      QColor fill = accent;
      fill.setAlpha(52);
      painter.fillRect(rect(), fill);
      QPen pen(accent, 2.0);
      painter.setPen(pen);
      painter.drawRect(rect().adjusted(1, 1, -2, -2));
    }
  };
  class TabAccentStyle final : public QProxyStyle {
  public:
    void drawControl(ControlElement element, const QStyleOption *option,
                     QPainter *painter, const QWidget *widget = nullptr) const override {
      QProxyStyle::drawControl(element, option, painter, widget);
      if (element != CE_TabBarTabLabel || !painter) return;
      const auto *tab = qstyleoption_cast<const QStyleOptionTab *>(option);
      if (!tab || !(tab->state & State_Selected)) return;
      // The style's text sub-element excludes the tab's close button.
      const QRect titleRect = subElementRect(SE_TabBarTabText, tab, widget)
                                  .intersected(tab->rect);
      if (titleRect.width() <= 0) return;
      painter->save();
      painter->setClipRect(tab->rect, Qt::IntersectClip);
      painter->fillRect(QRect(titleRect.left(), tab->rect.bottom() - 2,
                              titleRect.width(), 2),
                        tab->palette.color(QPalette::Highlight));
      painter->restore();
    }
  };
public:
  explicit NativeDockSurface(QWidget *parent = nullptr) : QWidget(parent) {
    setAcceptDrops(true);
    dropPreview_ = new DockDropPreview(this);
    auto *rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);

    verticalSplitter_ = new QSplitter(Qt::Vertical, this);
    verticalSplitter_->setChildrenCollapsible(false);
    verticalSplitter_->setOpaqueResize(false);
    rootLayout->addWidget(verticalSplitter_);
    topTabs_ = createTabSurface(verticalSplitter_);
    topTabs_->hide();
    verticalSplitter_->addWidget(topTabs_);

    auto *splitter = new QSplitter(Qt::Horizontal, verticalSplitter_);
    splitter->setChildrenCollapsible(false);
    splitter->setOpaqueResize(false);
    splitter_ = splitter;
    leftTabs_ = createTabSurface(splitter);
    centerTabs_ = createTabSurface(splitter);
    rightTabs_ = createTabSurface(splitter);
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 3);
    splitter->setStretchFactor(2, 1);
    verticalSplitter_->addWidget(splitter);

    bottomTabs_ = createTabSurface(verticalSplitter_);
    bottomTabs_->hide();
    verticalSplitter_->addWidget(bottomTabs_);
    verticalSplitter_->setStretchFactor(0, 1);
    verticalSplitter_->setStretchFactor(1, 3);
    verticalSplitter_->setStretchFactor(2, 1);

    for (auto *tabs : {leftTabs_, centerTabs_, rightTabs_, topTabs_,
                       bottomTabs_}) {
      if (!tabs) {
        continue;
      }
      tabs->setAcceptDrops(true);
      tabs->tabBar()->setAcceptDrops(true);
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
    installCloseButton(tabs, tabs->addTab(widget, title), dockId);
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
    installCloseButton(tabs, tabs->addTab(widget, title), dockId);
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
    installFloatingHeader(dialog, layout, dockId, title);
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

  // QADS-style detach: a tab released outside a dock surface becomes an owned
  // floating window. The panel object itself is preserved for a later re-dock.
  bool floatDockWidget(const QString &dockId, const QRect &geometry = {}) {
    const QString resolvedId = resolveDockId(dockId);
    auto *widget = docks_.value(resolvedId, nullptr);
    if (!widget || floatingDialogs_.contains(resolvedId)) return false;
    const QString title = titles_.value(resolvedId, resolvedId);
    const bool visible = widget->isVisible();
    if (auto *tabs = tabsForWidget(widget)) {
      const int index = tabs->indexOf(widget);
      if (index >= 0) tabs->removeTab(index);
      if (tabs->count() == 0) tabs->hide();
    }
    docks_.remove(resolvedId);
    auto *dialog = new QDialog(window());
    dialog->setWindowTitle(title);
    dialog->setObjectName(QStringLiteral("ArtifactNativeFloatingDock_%1").arg(resolvedId));
    dialog->setAttribute(Qt::WA_DeleteOnClose, false);
    auto *layout = new QVBoxLayout(dialog);
    layout->setContentsMargins(0, 0, 0, 0);
    installFloatingHeader(dialog, layout, resolvedId, title);
    widget->setParent(dialog);
    layout->addWidget(widget);
    const QRect fallback(dialog->mapToGlobal(QPoint(0, 0)), QSize(560, 400));
    dialog->setGeometry(geometry.isValid() ? geometry : fallback);
    floatingDialogs_.insert(resolvedId, dialog);
    floatingWidgets_.insert(resolvedId, widget);
    if (visible) dialog->show();
    return true;
  }

  bool dockFloatingWidget(const QString &dockId, DockArea area = DockArea::Center) {
    const QString resolvedId = resolveDockId(dockId);
    auto *dialog = floatingDialogs_.take(resolvedId);
    auto *widget = floatingWidgets_.take(resolvedId);
    if (!dialog || !widget) return false;
    const QString title = titles_.value(resolvedId, resolvedId);
    const bool visible = dialog->isVisible();
    const bool pinned = pinned_.value(resolvedId, false);
    widget->setParent(nullptr);
    dialog->hide();
    dialog->deleteLater();
    if (!addDockWidget(resolvedId, title, widget, area)) return false;
    pinned_.insert(resolvedId, pinned);
    setDockVisible(resolvedId, visible);
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
    const auto savedTabOrder = [](const DockLayoutEntry &entry) {
      if (!entry.tabGroup.startsWith(QStringLiteral("tabs:"))) {
        return std::numeric_limits<int>::max();
      }
      return entry.tabGroup.mid(5).split(QLatin1Char('|')).indexOf(entry.dockId);
    };
    std::sort(normalized.begin(), normalized.end(),
              [&savedTabOrder](const DockLayoutEntry &left,
                               const DockLayoutEntry &right) {
                if (left.floating != right.floating) return !left.floating;
                if (left.area != right.area)
                  return static_cast<int>(left.area) < static_cast<int>(right.area);
                const int leftOrder = savedTabOrder(left);
                const int rightOrder = savedTabOrder(right);
                if (leftOrder != rightOrder) return leftOrder < rightOrder;
                return left.dockId < right.dockId;
              });

    bool restoredAny = false;
    for (const auto &entry : normalized) {
      if (entry.floating) {
        if (!floatingDialogs_.contains(entry.dockId) &&
            docks_.contains(entry.dockId)) {
          floatDockWidget(entry.dockId, entry.floatingGeometry);
        }
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
      if (floatingDialogs_.contains(entry.dockId)) {
        if (!dockFloatingWidget(entry.dockId, entry.area)) {
          continue;
        }
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
      setDockVisible(entry.dockId, entry.visible);
      pinned_.insert(entry.dockId, entry.pinned);
      restoredAny = true;
    }
    return restoredAny;
  }

  QByteArray saveLayoutState() const {
    DockLayoutDocument document;
    QHash<int, QStringList> areaIds;
    const auto appendTabOrder = [this, &areaIds](QTabWidget *tabs,
                                                  DockArea area) {
      if (!tabs) return;
      auto &ids = areaIds[static_cast<int>(area)];
      for (int i = 0; i < tabs->count(); ++i) {
        const QString id = dockIdForWidget(tabs->widget(i));
        if (!id.isEmpty()) ids.push_back(id);
      }
    };
    appendTabOrder(leftTabs_, DockArea::Left);
    appendTabOrder(centerTabs_, DockArea::Center);
    appendTabOrder(rightTabs_, DockArea::Right);
    appendTabOrder(topTabs_, DockArea::Top);
    appendTabOrder(bottomTabs_, DockArea::Bottom);
    for (auto it = docks_.cbegin(); it != docks_.cend(); ++it) {
      DockLayoutEntry entry;
      entry.dockId = it.key();
      entry.area = areas_.value(it.key(), DockArea::Center);
      entry.visible = it.value() && it.value()->isVisible();
      entry.pinned = pinned_.value(it.key(), false);
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
    installCloseButton(targetTabs, targetTabs->addTab(widget, title), dockId);
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
    if (auto *tabs = tabsForWidget(widget)) {
      tabs->setTabVisible(tabs->indexOf(widget), visible);
      if (visible) tabs->setCurrentWidget(widget);
      bool anyVisible = false;
      for (int i = 0; i < tabs->count(); ++i) anyVisible |= tabs->isTabVisible(i);
      tabs->setVisible(anyVisible);
    } else {
      widget->setVisible(visible);
    }
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
    if (verticalSplitter_ && (area == DockArea::Top || area == DockArea::Bottom)) {
      verticalSplitter_->setSizes(sizes);
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
      showDropPreview(rect());
      return;
    }
    QWidget::dragEnterEvent(event);
  }

  void dragMoveEvent(QDragMoveEvent *event) override {
    if (event->mimeData()->hasFormat(QStringLiteral("application/x-artifact-dock-id"))) {
      showDropPreview(dropPreviewRectForArea(areaForPosition(event->position().toPoint())));
      event->acceptProposedAction();
      return;
    }
    QWidget::dragMoveEvent(event);
  }

  void dragLeaveEvent(QDragLeaveEvent *event) override {
    hideDropPreview();
    QWidget::dragLeaveEvent(event);
  }

  void dropEvent(QDropEvent *event) override {
    hideDropPreview();
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
    if (auto *button = qobject_cast<QToolButton *>(watched)) {
      const QString id = button->property("artifactDockCloseId").toString();
      if (!id.isEmpty()) {
        const bool mouseRelease = event->type() == QEvent::MouseButtonRelease &&
            static_cast<QMouseEvent *>(event)->button() == Qt::LeftButton &&
            button->rect().contains(static_cast<QMouseEvent *>(event)->position().toPoint());
        const bool keyRelease = event->type() == QEvent::KeyRelease &&
            static_cast<QKeyEvent *>(event)->key() == Qt::Key_Space &&
            !static_cast<QKeyEvent *>(event)->isAutoRepeat();
        if (button->isDown() && (mouseRelease || keyRelease)) {
          button->setDown(false);
          dragSourceId_.clear();
          setDockVisible(id, false);
          return true;
        }
      }
      const QString dockBackId = button->property("artifactDockBackId").toString();
      if (!dockBackId.isEmpty()) {
        const bool mouseRelease = event->type() == QEvent::MouseButtonRelease &&
            static_cast<QMouseEvent *>(event)->button() == Qt::LeftButton &&
            button->rect().contains(static_cast<QMouseEvent *>(event)->position().toPoint());
        const bool keyRelease = event->type() == QEvent::KeyRelease &&
            static_cast<QKeyEvent *>(event)->key() == Qt::Key_Space &&
            !static_cast<QKeyEvent *>(event)->isAutoRepeat();
        if (button->isDown() && (mouseRelease || keyRelease)) {
          button->setDown(false);
          dockFloatingWidget(dockBackId, areas_.value(dockBackId, DockArea::Center));
          return true;
        }
      }
    }
    auto *tabs = qobject_cast<QTabWidget *>(watched);
    auto *tabBar = qobject_cast<QTabBar *>(watched);
    if (!tabs && tabBar) {
      tabs = qobject_cast<QTabWidget *>(tabBar->parentWidget());
    }
    if (!tabs) {
      return QWidget::eventFilter(watched, event);
    }

    if (event->type() == QEvent::MouseButtonDblClick && tabBar) {
      auto *mouseEvent = static_cast<QMouseEvent *>(event);
      if (mouseEvent->button() == Qt::LeftButton) {
        const int index = tabBar->tabAt(mouseEvent->position().toPoint());
        const QString dockId = index >= 0
            ? dockIdForWidget(tabs->widget(index))
            : QString{};
        if (!dockId.isEmpty()) {
          const QRect geometry(
              tabBar->mapToGlobal(tabBar->tabRect(index).topLeft()),
              QSize(560, 400));
          dragSourceId_.clear();
          if (floatDockWidget(dockId, geometry)) return true;
        }
      }
    } else if (event->type() == QEvent::MouseButtonPress && tabBar) {
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
      const Qt::DropAction result = drag->exec(Qt::MoveAction);
      hideDropPreview();
      if (result == Qt::IgnoreAction && docks_.contains(dragSourceId_)) {
        const QPoint globalPosition = tabBar->mapToGlobal(mouseEvent->position().toPoint());
        floatDockWidget(dragSourceId_, QRect(globalPosition, QSize(560, 400)));
      }
      dragSourceId_.clear();
      return true;
    } else if (event->type() == QEvent::DragEnter ||
               event->type() == QEvent::DragMove) {
      auto *dragEvent = static_cast<QDragMoveEvent *>(event);
      if (dragEvent->mimeData()->hasFormat(
              QStringLiteral("application/x-artifact-dock-id"))) {
        const QPoint position = dragEvent->position().toPoint();
        const QPoint tabPosition = tabBar
                                       ? position
                                       : tabs->tabBar()->mapFrom(tabs, position);
        const int targetIndex = tabs->tabBar()->tabAt(tabPosition);
        const QRect targetRect = targetIndex >= 0
            ? QRect(tabs->tabBar()->mapTo(this, tabs->tabBar()->tabRect(targetIndex).topLeft()),
                    tabs->tabBar()->tabRect(targetIndex).size())
            : QRect(tabs->mapTo(this, tabs->rect().topLeft()), tabs->size());
        showDropPreview(targetRect);
        dragEvent->acceptProposedAction();
        return true;
      }
    } else if (event->type() == QEvent::DragLeave) {
      hideDropPreview();
    } else if (event->type() == QEvent::Drop) {
      hideDropPreview();
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
        auto *source = docks_.value(sourceId, nullptr);
        auto *sourceTabs = source ? tabsForWidget(source) : nullptr;
        if (source && sourceTabs == tabs) {
          const int sourceIndex = tabs->indexOf(source);
          if (sourceIndex >= 0) {
            int insertionIndex = targetIndex < 0 ? tabs->count() : targetIndex;
            const QString title = titles_.value(sourceId, sourceId);
            tabs->removeTab(sourceIndex);
            if (sourceIndex < insertionIndex) --insertionIndex;
            insertionIndex = std::clamp(insertionIndex, 0, tabs->count());
            installCloseButton(tabs, tabs->insertTab(insertionIndex, source, title), sourceId);
            tabs->setCurrentWidget(source);
            dropEvent->acceptProposedAction();
            return true;
          }
        }
        if (targetIndex >= 0) {
          const QString targetId = dockIdForWidget(tabs->widget(targetIndex));
          if (!targetId.isEmpty() && sourceId != targetId &&
              moveDockWidgetToTab(sourceId, targetId)) {
            dropEvent->acceptProposedAction();
            return true;
          }
        } else if (moveDockWidget(sourceId, areaForTabs(tabs))) {
          // The empty content/tab-strip portion is an area drop, not a
          // request to merge with whichever tab happens to be current.
          dropEvent->acceptProposedAction();
          return true;
        }
      }
    }
    return QWidget::eventFilter(watched, event);
  }

private:
  DockArea areaForPosition(const QPoint &position) const {
    if (topTabs_ && topTabs_->isVisible() &&
        topTabs_->rect().contains(topTabs_->mapFrom(this, position))) {
      return DockArea::Top;
    }
    if (bottomTabs_ && bottomTabs_->isVisible() &&
        bottomTabs_->rect().contains(bottomTabs_->mapFrom(this, position))) {
      return DockArea::Bottom;
    }
    if (splitter_ && splitter_->rect().contains(splitter_->mapFrom(this, position))) {
      const int center = splitter_->width() / 2;
      const int localX = splitter_->mapFrom(this, position).x();
      if (localX < center - splitter_->width() / 6) {
        return DockArea::Left;
      }
      if (localX > center + splitter_->width() / 6) {
        return DockArea::Right;
      }
    }
    return DockArea::Center;
  }

  QRect dropPreviewRectForArea(DockArea area) const {
    auto *tabs = tabsForArea(area);
    if (!tabs) return rect();
    return QRect(tabs->mapTo(const_cast<NativeDockSurface *>(this), tabs->rect().topLeft()),
                 tabs->size());
  }

  void showDropPreview(const QRect &targetRect) {
    if (!dropPreview_) return;
    dropPreview_->setGeometry(targetRect.adjusted(2, 2, -2, -2));
    dropPreview_->show();
    dropPreview_->raise();
  }

  void hideDropPreview() {
    if (dropPreview_) dropPreview_->hide();
  }

  void installCloseButton(QTabWidget *tabs, int index, const QString &id) {
    auto *button = new QToolButton(tabs->tabBar());
    button->setAutoRaise(true);
    button->setIcon(button->style()->standardIcon(QStyle::SP_TitleBarCloseButton));
    button->setToolTip(tr("Close panel"));
    button->setAccessibleName(tr("Close panel"));
    button->setProperty("artifactDockCloseId", id);
    button->installEventFilter(this);
    tabs->tabBar()->setTabButton(index, QTabBar::RightSide, button);
  }

  void installFloatingHeader(QDialog *dialog, QVBoxLayout *layout,
                             const QString &dockId, const QString &title) {
    if (!dialog || !layout) return;
    auto *header = new QWidget(dialog);
    auto *headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(6, 3, 3, 3);
    auto *label = new QLabel(title, header);
    label->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    headerLayout->addWidget(label);
    auto *dockBack = new QToolButton(header);
    dockBack->setAutoRaise(true);
    dockBack->setIcon(dockBack->style()->standardIcon(QStyle::SP_ArrowBack));
    dockBack->setToolTip(tr("Dock panel back to the center"));
    dockBack->setAccessibleName(tr("Dock panel back"));
    dockBack->setProperty("artifactDockBackId", dockId);
    dockBack->installEventFilter(this);
    headerLayout->addWidget(dockBack);
    layout->addWidget(header);
  }

  static QTabWidget *createTabSurface(QWidget *parent) {
    auto *tabs = new QTabWidget(parent);
    tabs->setDocumentMode(true);
    tabs->setMovable(true);
    tabs->tabBar()->setExpanding(false);
    tabs->tabBar()->setUsesScrollButtons(true);
    tabs->tabBar()->setElideMode(Qt::ElideRight);
    auto *accentStyle = new TabAccentStyle;
    accentStyle->setParent(tabs->tabBar());
    tabs->tabBar()->setStyle(accentStyle);
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

  DockArea areaForTabs(const QTabWidget *tabs) const {
    if (tabs == leftTabs_) return DockArea::Left;
    if (tabs == rightTabs_) return DockArea::Right;
    if (tabs == topTabs_) return DockArea::Top;
    if (tabs == bottomTabs_) return DockArea::Bottom;
    return DockArea::Center;
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
  QSplitter *verticalSplitter_ = nullptr;
  DockDropPreview *dropPreview_ = nullptr;
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
