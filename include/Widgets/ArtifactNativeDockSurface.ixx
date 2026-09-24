module;

#include <algorithm>
#include <functional>
#include <utility>
#include <limits>
#include <QHash>
#include <QSet>
#include <QByteArray>
#include <QColor>
#include <QApplication>
#include <QContextMenuEvent>
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
#include <QPainterPath>
#include <QPaintEvent>
#include <QPen>
#include <QPointer>
#include <QVariant>
#include <QPalette>
#include <QLabel>
#include <QMenu>
#include <QAction>
#include <QSize>
#include <QSizePolicy>
#include <QJsonDocument>
#include <QSplitter>
#include <QSplitterHandle>
#include <QTabWidget>
#include <QTabBar>
#include <QEvent>
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
  class DockTabSurface final : public QTabWidget {
    class DockTabBar final : public QTabBar {
    public:
      explicit DockTabBar(DockTabSurface *tabs)
          : QTabBar(tabs), tabs_(tabs) {
      }

    protected:
      void paintEvent(QPaintEvent *event) override {
        QTabBar::paintEvent(event);
        if (!tabs_ ||
            !tabs_->property("artifactNativeActivePanel").toBool()) {
          return;
        }
        const int selectedIndex = currentIndex();
        if (selectedIndex < 0) {
          return;
        }

        QStyleOptionTab option;
        initStyleOption(&option, selectedIndex);
        const QRectF rect(option.rect.adjusted(0, 0, -1, 1));
        const bool tabsAtBottom =
            tabs_->tabPosition() == QTabWidget::South;
        constexpr qreal radius = 5.0;
        QPainterPath contour;
        if (tabsAtBottom) {
          contour.moveTo(rect.left(), rect.top());
          contour.lineTo(rect.left(), rect.bottom() - radius);
          contour.quadTo(rect.left(), rect.bottom(), rect.left() + radius,
                         rect.bottom());
          contour.lineTo(rect.right() - radius, rect.bottom());
          contour.quadTo(rect.right(), rect.bottom(), rect.right(),
                         rect.bottom() - radius);
          contour.lineTo(rect.right(), rect.top());
        } else {
          contour.moveTo(rect.left(), rect.bottom());
          contour.lineTo(rect.left(), rect.top() + radius);
          contour.quadTo(rect.left(), rect.top(), rect.left() + radius,
                         rect.top());
          contour.lineTo(rect.right() - radius, rect.top());
          contour.quadTo(rect.right(), rect.top(), rect.right(),
                         rect.top() + radius);
          contour.lineTo(rect.right(), rect.bottom());
        }

        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);
        QPen pen(QColor(145, 132, 238), 2.0);
        pen.setJoinStyle(Qt::RoundJoin);
        pen.setCapStyle(Qt::RoundCap);
        painter.setPen(pen);
        painter.setBrush(Qt::NoBrush);
        painter.drawPath(contour);

        const QRect titleRect =
            style()->subElementRect(QStyle::SE_TabBarTabText, &option, this)
                .intersected(option.rect);
        if (titleRect.width() > 0) {
          // The underline belongs to the title, not to the tab/pane seam.
          // Keep it close to the text and use the configurable theme accent;
          // the fixed violet contour remains the panel-focus cue.
          const int underlineY = tabsAtBottom
                                     ? std::max(titleRect.top() - 4,
                                                option.rect.top() + 2)
                                     : std::min(titleRect.bottom() + 2,
                                                option.rect.bottom() - 2);
          QColor titleAccent = palette().color(QPalette::Highlight);
          titleAccent.setAlpha(224);
          painter.fillRect(
              QRect(titleRect.left(), underlineY, titleRect.width(), 2),
              titleAccent);
        }
      }

    private:
      DockTabSurface *tabs_ = nullptr;
    };

    class DockSurfaceStyle final : public QProxyStyle {
    public:
      explicit DockSurfaceStyle(DockTabSurface *tabs)
          : QProxyStyle(tabs ? tabs->style() : nullptr), tabs_(tabs) {
      }

      void drawPrimitive(QStyle::PrimitiveElement element, const QStyleOption *option,
                         QPainter *painter,
                         const QWidget *widget = nullptr) const override {
        // DockTabSurface paints the active-panel frame itself.  Leaving the
        // base QTabWidget pane frame enabled draws a second horizontal rule
        // immediately beside the violet focus rule.
        if (element == QStyle::PE_FrameTabWidget && widget == tabs_) {
          return;
        }
        QProxyStyle::drawPrimitive(element, option, painter, widget);
      }

      QRect subElementRect(SubElement element, const QStyleOption *option,
                           const QWidget *widget = nullptr) const override {
        QRect result = QProxyStyle::subElementRect(element, option, widget);
        if (element == SE_TabWidgetTabContents && widget == tabs_ &&
            result.width() > 4 && result.height() > 4) {
          // Reserve a stable owner-drawn gutter before native render children
          // initialize. Never resize the live page stack on focus/show events.
          result.adjust(2, 2, -2, -2);
        }
        return result;
      }

    private:
      DockTabSurface *tabs_ = nullptr;
    };

  public:
    explicit DockTabSurface(QWidget *parent) : QTabWidget(parent) {
      setTabBar(new DockTabBar(this));
      auto *surfaceStyle = new DockSurfaceStyle(this);
      surfaceStyle->setParent(this);
      setStyle(surfaceStyle);
    }

    void setTabListButton(QToolButton *button) {
      tabListButton_ = button;
      syncTabListButtonCorner();
    }

    void refreshFocusChrome() {
      update();
      tabBar()->update();
    }

  protected:
    bool event(QEvent *event) override {
      const bool handled = QTabWidget::event(event);
      if (event->type() == QEvent::LayoutRequest ||
          event->type() == QEvent::Resize || event->type() == QEvent::Show) {
        syncTabListButtonCorner();
      }
      return handled;
    }

    void paintEvent(QPaintEvent *event) override {
      QTabWidget::paintEvent(event);
      if (!property("artifactNativeActivePanel").toBool() ||
          currentIndex() < 0) {
        return;
      }

      const QRectF surface(rect().adjusted(1, 1, -2, -2));
      const QRect selectedRect =
          tabBar()->tabRect(currentIndex()).translated(tabBar()->pos())
              .intersected(surface.toAlignedRect());
      if (selectedRect.isEmpty() || surface.width() <= 8.0 ||
          surface.height() <= 8.0) {
        return;
      }

      const QRectF tab(selectedRect.adjusted(0, 0, -1, 0));
      constexpr qreal outerRadius = 3.0;
      const bool tabsAtBottom = tabPosition() == QTabWidget::South;
      const qreal contentEdge = tabsAtBottom
                                    ? std::clamp(
                                          static_cast<qreal>(
                                              tabBar()->geometry().top() - 1),
                                          surface.top() + outerRadius,
                                          tab.top())
                                    : std::clamp(
                                          static_cast<qreal>(
                                              tabBar()->geometry().bottom() + 1),
                                          tab.bottom(),
                                          surface.bottom() - outerRadius);

      // Draw only the dock chrome owned by QTabWidget. The selected tab's
      // open contour edge is painted by DockTabBar after the tab itself, so
      // no additional QWidget needs to overlap a native viewport child.
      QPainterPath outline;
      if (tabsAtBottom) {
        outline.moveTo(tab.right(), contentEdge);
        outline.lineTo(surface.right() - outerRadius, contentEdge);
        outline.quadTo(surface.right(), contentEdge, surface.right(),
                       contentEdge - outerRadius);
        outline.lineTo(surface.right(), surface.top() + outerRadius);
        outline.quadTo(surface.right(), surface.top(),
                       surface.right() - outerRadius, surface.top());
        outline.lineTo(surface.left() + outerRadius, surface.top());
        outline.quadTo(surface.left(), surface.top(), surface.left(),
                       surface.top() + outerRadius);
        outline.lineTo(surface.left(), contentEdge - outerRadius);
        outline.quadTo(surface.left(), contentEdge,
                       surface.left() + outerRadius, contentEdge);
        outline.lineTo(tab.left(), contentEdge);
      } else {
        outline.moveTo(tab.right(), contentEdge);
        outline.lineTo(surface.right() - outerRadius, contentEdge);
        outline.quadTo(surface.right(), contentEdge, surface.right(),
                       contentEdge + outerRadius);
        outline.lineTo(surface.right(), surface.bottom() - outerRadius);
        outline.quadTo(surface.right(), surface.bottom(),
                       surface.right() - outerRadius, surface.bottom());
        outline.lineTo(surface.left() + outerRadius, surface.bottom());
        outline.quadTo(surface.left(), surface.bottom(), surface.left(),
                       surface.bottom() - outerRadius);
        outline.lineTo(surface.left(), contentEdge + outerRadius);
        outline.quadTo(surface.left(), contentEdge,
                       surface.left() + outerRadius, contentEdge);
        outline.lineTo(tab.left(), contentEdge);
      }

      QPainter painter(this);
      painter.setRenderHint(QPainter::Antialiasing, true);
      QPen pen(QColor(145, 132, 238), 2.0);
      pen.setJoinStyle(Qt::RoundJoin);
      pen.setCapStyle(Qt::RoundCap);
      painter.setPen(pen);
      painter.setBrush(Qt::NoBrush);
      painter.drawPath(outline);
    }

  private:
    void syncTabListButtonCorner() {
      if (!tabListButton_) {
        return;
      }
      const Corner desiredCorner = tabPosition() == QTabWidget::South
                                       ? Qt::BottomRightCorner
                                       : Qt::TopRightCorner;
      if (cornerWidget(desiredCorner) != tabListButton_) {
        setCornerWidget(tabListButton_, desiredCorner);
      }
    }

    QToolButton *tabListButton_ = nullptr;
  };

  class DockSplitter final : public QSplitter {
    class Handle final : public QSplitterHandle {
    public:
      Handle(Qt::Orientation orientation, DockSplitter *owner)
          : QSplitterHandle(orientation, owner), owner_(owner) {
        setCursor(orientation == Qt::Horizontal ? Qt::SplitHCursor
                                                : Qt::SplitVCursor);
      }

    protected:
      void mousePressEvent(QMouseEvent *event) override {
        if (event->button() != Qt::LeftButton) {
          QSplitterHandle::mousePressEvent(event);
          return;
        }
        dragging_ = true;
        pressOffset_ = orientation() == Qt::Horizontal
                           ? event->position().toPoint().x()
                           : event->position().toPoint().y();
        grabMouse();
        event->accept();
      }

      void mouseMoveEvent(QMouseEvent *event) override {
        if (!dragging_ || !(event->buttons() & Qt::LeftButton) || !owner_) {
          QSplitterHandle::mouseMoveEvent(event);
          return;
        }
        int handleIndex = -1;
        for (int index = 1; index < owner_->count(); ++index) {
          if (owner_->handle(index) == this) {
            handleIndex = index;
            break;
          }
        }
        if (handleIndex < 1) {
          return;
        }
        const QPoint local = owner_->mapFromGlobal(
            event->globalPosition().toPoint());
        const int position = orientation() == Qt::Horizontal
                                 ? local.x() - pressOffset_
                                 : local.y() - pressOffset_;
        owner_->moveSplitter(position, handleIndex);
        event->accept();
      }

      void mouseReleaseEvent(QMouseEvent *event) override {
        if (dragging_ && event->button() == Qt::LeftButton) {
          dragging_ = false;
          releaseMouse();
          event->accept();
          return;
        }
        QSplitterHandle::mouseReleaseEvent(event);
      }

    private:
      DockSplitter *owner_ = nullptr;
      int pressOffset_ = 0;
      bool dragging_ = false;
    };

  public:
    explicit DockSplitter(Qt::Orientation orientation, QWidget *parent)
        : QSplitter(orientation, parent) {
      setChildrenCollapsible(false);
      setHandleWidth(8);
      setOpaqueResize(true);
    }

  protected:
    QSplitterHandle *createHandle() override {
      return new Handle(orientation(), this);
    }
  };

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
public:
  using LayoutMutationCallback =
      std::function<void(const QByteArray &, const QString &)>;

  explicit NativeDockSurface(QWidget *parent = nullptr) : QWidget(parent) {
    setAcceptDrops(true);
    qApp->installEventFilter(this);
    dropPreview_ = new DockDropPreview(this);
    auto *rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);

    verticalSplitter_ = new DockSplitter(Qt::Vertical, this);
    // Keep the docking splitters visibly draggable and update the panel sizes
    // while dragging.  The application-wide style uses deferred resizing for
    // heavyweight editor splitters, which is inappropriate for dock layout.
    rootLayout->addWidget(verticalSplitter_);
    topTabs_ = createTabSurface(verticalSplitter_);
    topTabs_->hide();
    verticalSplitter_->addWidget(topTabs_);

    auto *splitter = new DockSplitter(Qt::Horizontal, verticalSplitter_);
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
      if (auto *tabListButton = qobject_cast<QToolButton *>(
              tabs->cornerWidget(Qt::TopRightCorner))) {
        tabListButton->installEventFilter(this);
      }
    }
  }

  ~NativeDockSurface() override {
    if (qApp) {
      qApp->removeEventFilter(this);
    }
    QSet<QDialog *> dialogs;
    for (auto *dialog : floatingDialogs_) {
      dialogs.insert(dialog);
    }
    for (auto *dialog : dialogs) {
      if (!dialog) {
        continue;
      }
      dialog->hide();
      delete dialog;
    }
    floatingDialogs_.clear();
    floatingWidgets_.clear();
    floatingTabSurfaces_.clear();
  }

  void setLayoutMutationCallback(LayoutMutationCallback callback) {
    layoutMutationCallback_ = std::move(callback);
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
    if (!activeTabs_) {
      setActiveTabSurface(tabs);
    }
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
    setActiveTabSurface(tabs);
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
    installFloatingHeader(dialog, layout, title);
    auto *tabs = createFloatingTabSurface(dialog, layout);
    widget->setParent(tabs);
    tabs->addTab(widget, title);
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
    const QByteArray beforeState = beginLayoutMutation();
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
    installFloatingHeader(dialog, layout, title);
    auto *floatingTabs = createFloatingTabSurface(dialog, layout);
    widget->setParent(floatingTabs);
    floatingTabs->addTab(widget, title);
    const QRect fallback(dialog->mapToGlobal(QPoint(0, 0)), QSize(560, 400));
    dialog->setGeometry(geometry.isValid() ? geometry : fallback);
    floatingDialogs_.insert(resolvedId, dialog);
    floatingWidgets_.insert(resolvedId, widget);
    if (visible) dialog->show();
    finishLayoutMutation(beforeState, tr("Float panel"));
    return true;
  }

  bool floatDockTabGroup(const QString &dockId, const QRect &geometry = {}) {
    const QString resolvedId = resolveDockId(dockId);
    auto *source = docks_.value(resolvedId, nullptr);
    auto *sourceTabs = source ? tabsForWidget(source) : nullptr;
    if (!source || !sourceTabs || sourceTabs->count() < 2) {
      return floatDockWidget(resolvedId, geometry);
    }

    QStringList dockIds;
    for (int index = 0; index < sourceTabs->count(); ++index) {
      const QString id = dockIdForWidget(sourceTabs->widget(index));
      if (!id.isEmpty()) {
        dockIds.push_back(id);
      }
    }
    if (dockIds.size() < 2) {
      return floatDockWidget(resolvedId, geometry);
    }

    const QByteArray beforeState = beginLayoutMutation();
    auto *dialog = new QDialog(window());
    dialog->setWindowTitle(titles_.value(resolvedId, resolvedId));
    dialog->setObjectName(QStringLiteral("ArtifactNativeFloatingDockGroup_%1")
                              .arg(resolvedId));
    dialog->setAttribute(Qt::WA_DeleteOnClose, false);
    auto *layout = new QVBoxLayout(dialog);
    layout->setContentsMargins(0, 0, 0, 0);
    installFloatingHeader(dialog, layout, dialog->windowTitle());
    auto *floatingTabs = createFloatingTabSurface(dialog, layout);
    const bool visible = sourceTabs->isVisible();
    for (const QString &id : dockIds) {
      auto *widget = docks_.take(id);
      if (!widget) {
        continue;
      }
      const int index = sourceTabs->indexOf(widget);
      if (index >= 0) {
        sourceTabs->removeTab(index);
      }
      widget->setParent(floatingTabs);
      floatingTabs->addTab(widget, titles_.value(id, id));
      floatingDialogs_.insert(id, dialog);
      floatingWidgets_.insert(id, widget);
    }
    if (sourceTabs->count() == 0) {
      sourceTabs->hide();
    }
    const QRect fallback(dialog->mapToGlobal(QPoint(0, 0)), QSize(680, 460));
    dialog->setGeometry(geometry.isValid() ? geometry : fallback);
    if (visible) {
      dialog->show();
    }
    const bool floated = floatingTabs->count() > 0;
    if (floated) {
      finishLayoutMutation(beforeState, tr("Float tab group"));
    }
    return floated;
  }

  bool floatDockWidgetToGroup(const QString &dockId, QDialog *dialog) {
    const QString resolvedId = resolveDockId(dockId);
    auto *widget = docks_.value(resolvedId, nullptr);
    auto *floatingTabs = floatingTabSurfaces_.value(dialog, nullptr);
    if (!widget || !floatingTabs || floatingDialogs_.contains(resolvedId)) {
      return false;
    }
    if (auto *sourceTabs = tabsForWidget(widget)) {
      const int index = sourceTabs->indexOf(widget);
      if (index >= 0) {
        sourceTabs->removeTab(index);
      }
      if (sourceTabs->count() == 0) {
        sourceTabs->hide();
      }
    }
    docks_.remove(resolvedId);
    widget->setParent(floatingTabs);
    floatingTabs->addTab(widget, titles_.value(resolvedId, resolvedId));
    floatingDialogs_.insert(resolvedId, dialog);
    floatingWidgets_.insert(resolvedId, widget);
    return true;
  }

  bool dockFloatingWidget(const QString &dockId, DockArea area = DockArea::Center) {
    const QByteArray beforeState = beginLayoutMutation();
    const QString resolvedId = resolveDockId(dockId);
    auto *dialog = floatingDialogs_.value(resolvedId, nullptr);
    auto *widget = floatingWidgets_.value(resolvedId, nullptr);
    if (!dialog || !widget) return false;
    const QString title = titles_.value(resolvedId, resolvedId);
    const bool visible = dialog->isVisible();
    const bool pinned = pinned_.value(resolvedId, false);
    auto *floatingTabs = floatingTabSurfaces_.value(dialog, nullptr);
    if (floatingTabs) {
      const int index = floatingTabs->indexOf(widget);
      if (index >= 0) {
        floatingTabs->removeTab(index);
      }
    }
    floatingDialogs_.remove(resolvedId);
    floatingWidgets_.remove(resolvedId);
    widget->setParent(nullptr);
    if (!floatingTabs || floatingTabs->count() == 0) {
      floatingTabSurfaces_.remove(dialog);
      dialog->hide();
      dialog->deleteLater();
    }
    if (!addDockWidget(resolvedId, title, widget, area)) return false;
    pinned_.insert(resolvedId, pinned);
    setDockVisible(resolvedId, visible);
    finishLayoutMutation(beforeState, tr("Dock panel"));
    return true;
  }

  bool restoreLayout(const QList<DockLayoutEntry> &entries) {
    const bool wasRestoringLayout = restoringLayout_;
    restoringLayout_ = true;
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
    const auto savedTabOrder = [](const DockLayoutEntry &entry) -> qsizetype {
      QString ids;
      if (entry.tabGroup.startsWith(QStringLiteral("tabs:"))) {
        ids = entry.tabGroup.mid(5);
      } else if (entry.tabGroup.startsWith(QStringLiteral("floating-tabs:"))) {
        ids = entry.tabGroup.mid(14);
      } else {
        return std::numeric_limits<qsizetype>::max();
      }
      return ids.split(QLatin1Char('|')).indexOf(entry.dockId);
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

    QHash<QString, QDialog *> restoredFloatingGroups;
    bool restoredAny = false;
    for (const auto &entry : normalized) {
      if (entry.floating) {
        QDialog *groupDialog = nullptr;
        if (entry.tabGroup.startsWith(QStringLiteral("floating-tabs:"))) {
          groupDialog = restoredFloatingGroups.value(entry.tabGroup, nullptr);
        }
        if (!floatingDialogs_.contains(entry.dockId) &&
            docks_.contains(entry.dockId)) {
          if (groupDialog) {
            floatDockWidgetToGroup(entry.dockId, groupDialog);
          } else {
            floatDockWidget(entry.dockId, entry.floatingGeometry);
          }
        }
        if (auto *dialog = floatingDialogs_.value(entry.dockId, nullptr)) {
          if (entry.tabGroup.startsWith(QStringLiteral("floating-tabs:"))) {
            restoredFloatingGroups.insert(entry.tabGroup, dialog);
          }
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
    for (const auto &entry : normalized) {
      if (entry.active && entry.visible) {
        activateDock(entry.dockId);
      }
    }
    restoringLayout_ = wasRestoringLayout;
    return restoredAny;
  }

  QByteArray saveLayoutState() const {
    DockLayoutDocument document;
    document.areaTabsAtBottom = areaTabsAtBottom_;
    QHash<int, QStringList> areaIds;
    QHash<QDialog *, QStringList> floatingTabIds;
    QHash<QTabWidget *, QString> activeTabIds;
    const auto appendTabOrder = [this, &areaIds, &activeTabIds](
                                    QTabWidget *tabs, DockArea area) {
      if (!tabs) return;
      auto &ids = areaIds[static_cast<int>(area)];
      for (int i = 0; i < tabs->count(); ++i) {
        const QString id = dockIdForWidget(tabs->widget(i));
        if (!id.isEmpty()) ids.push_back(id);
        if (i == tabs->currentIndex() && !id.isEmpty()) {
          activeTabIds.insert(tabs, id);
        }
      }
    };
    appendTabOrder(leftTabs_, DockArea::Left);
    appendTabOrder(centerTabs_, DockArea::Center);
    appendTabOrder(rightTabs_, DockArea::Right);
    appendTabOrder(topTabs_, DockArea::Top);
    appendTabOrder(bottomTabs_, DockArea::Bottom);
    for (auto it = floatingTabSurfaces_.cbegin();
         it != floatingTabSurfaces_.cend(); ++it) {
      auto *tabs = it.value();
      if (!tabs) {
        continue;
      }
      auto &ids = floatingTabIds[it.key()];
      for (int index = 0; index < tabs->count(); ++index) {
        const QString id = dockIdForWidget(tabs->widget(index));
        if (!id.isEmpty()) {
          ids.push_back(id);
        }
        if (index == tabs->currentIndex() && !id.isEmpty()) {
          activeTabIds.insert(tabs, id);
        }
      }
    }
    for (auto it = docks_.cbegin(); it != docks_.cend(); ++it) {
      DockLayoutEntry entry;
      entry.dockId = it.key();
      entry.area = areas_.value(it.key(), DockArea::Center);
      auto *tabs = tabsForWidget(it.value());
      const int index = tabs ? tabs->indexOf(it.value()) : -1;
      entry.visible = tabs && index >= 0 ? tabs->isTabVisible(index)
                                         : it.value() && it.value()->isVisible();
      entry.active = activeTabIds.value(tabs) == it.key();
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
        auto *tabs = floatingTabSurfaces_.value(dialog, nullptr);
        const int index = tabs ? tabs->indexOf(it.value()) : -1;
        if (tabs && index >= 0) {
          entry.visible = tabs->isTabVisible(index);
        }
        entry.floatingGeometry = dialog->geometry();
        entry.active = activeTabIds.value(tabs) == it.key();
        auto ids = floatingTabIds.value(dialog);
        ids.removeDuplicates();
        entry.tabGroup = QStringLiteral("floating-tabs:") +
                         ids.join(QStringLiteral("|"));
      }
      document.entries.push_back(entry);
    }
    for (auto &entry : document.entries) {
      if (entry.floating) {
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
      const bool restored = restoreLayout(document.entries);
      if (restored) {
        applyAreaTabPositions(document.areaTabsAtBottom);
      }
      return restored;
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
      const bool restored = restoreLayout(entries);
      if (restored) {
        applyAreaTabPositions({});
      }
      return restored;
    }
    return false;
  }

  bool removeDockWidget(const QString &dockId) {
    const QString resolvedId = resolveDockId(dockId);
    if (auto *dialog = floatingDialogs_.value(resolvedId, nullptr)) {
      auto *widget = floatingWidgets_.value(resolvedId, nullptr);
      if (auto *floatingTabs = floatingTabSurfaces_.value(dialog, nullptr)) {
        const int index = widget ? floatingTabs->indexOf(widget) : -1;
        if (index >= 0) {
          floatingTabs->removeTab(index);
        }
      }
      floatingDialogs_.remove(resolvedId);
      floatingWidgets_.remove(resolvedId);
      if (floatingTabSurfaces_.value(dialog, nullptr) &&
          floatingTabSurfaces_.value(dialog)->count() == 0) {
        floatingTabSurfaces_.remove(dialog);
        dialog->hide();
        dialog->deleteLater();
      }
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
    const QByteArray beforeState = beginLayoutMutation();
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
    finishLayoutMutation(beforeState, tr("Move panel"));
    return true;
  }

  bool moveDockWidgetToTab(const QString &dockId,
                           const QString &targetDockId) {
    const QByteArray beforeState = beginLayoutMutation();
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
    finishLayoutMutation(beforeState, tr("Move panel to tab group"));
    return true;
  }

  bool moveDockWidgetToTabAt(const QString &dockId, QTabWidget *targetTabs,
                             int insertionIndex) {
    const QByteArray beforeState = beginLayoutMutation();
    const QString resolvedId = resolveDockId(dockId);
    auto *widget = docks_.value(resolvedId, nullptr);
    if (!widget || !targetTabs || tabsForWidget(widget) == targetTabs) {
      return false;
    }
    const QString title = titles_.value(resolvedId, resolvedId);
    auto *sourceTabs = tabsForWidget(widget);
    const int sourceIndex = sourceTabs ? sourceTabs->indexOf(widget) : -1;
    const bool visible = sourceTabs && sourceIndex >= 0
        ? sourceTabs->isTabVisible(sourceIndex)
        : widget->isVisible();
    const bool pinned = pinned_.value(resolvedId, false);
    const DockArea previousArea = areas_.value(resolvedId, DockArea::Center);
    const DockArea targetArea = areaForTabs(targetTabs);
    auto *floatingDialog = floatingDialogForTabs(targetTabs);
    if (!removeDockWidget(resolvedId)) {
      return false;
    }
    widget->setParent(targetTabs);
    insertionIndex = std::clamp(insertionIndex, 0, targetTabs->count());
    installCloseButton(targetTabs,
                       targetTabs->insertTab(insertionIndex, widget, title),
                       resolvedId);
    targetTabs->setCurrentWidget(widget);
    targetTabs->show();
    if (floatingDialog) {
      floatingDialogs_.insert(resolvedId, floatingDialog);
      floatingWidgets_.insert(resolvedId, widget);
      areas_.insert(resolvedId, previousArea);
    } else {
      docks_.insert(resolvedId, widget);
      areas_.insert(resolvedId, targetArea);
    }
    titles_.insert(resolvedId, title);
    pinned_.insert(resolvedId, pinned);
    syncPinButton(resolvedId, pinned);
    widget->setVisible(visible);
    setActiveTabSurface(targetTabs);
    finishLayoutMutation(beforeState, tr("Move panel to tab group"));
    return true;
  }

  bool setDockVisible(const QString &dockId, bool visible) {
    const QString resolvedId = resolveDockId(dockId);
    if (auto *dialog = floatingDialogs_.value(resolvedId, nullptr)) {
      auto *widget = floatingWidgets_.value(resolvedId, nullptr);
      auto *tabs = floatingTabSurfaces_.value(dialog, nullptr);
      if (tabs && widget) {
        const int index = tabs->indexOf(widget);
        if (index >= 0) {
          tabs->setTabVisible(index, visible);
          if (visible) {
            tabs->setCurrentIndex(index);
          }
        }
        bool anyVisible = false;
        for (int index = 0; index < tabs->count(); ++index) {
          anyVisible |= tabs->isTabVisible(index);
        }
        dialog->setVisible(anyVisible);
      } else {
        dialog->setVisible(visible);
      }
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
    syncPinButton(resolvedId, pinned);
    return true;
  }

  bool activateDock(const QString &dockId) {
    const QString resolvedId = resolveDockId(dockId);
    if (auto *dialog = floatingDialogs_.value(resolvedId, nullptr)) {
      if (auto *tabs = floatingTabSurfaces_.value(dialog, nullptr)) {
        if (auto *widget = floatingWidgets_.value(resolvedId, nullptr)) {
          const int index = tabs->indexOf(widget);
          if (index >= 0) {
            tabs->setTabVisible(index, true);
            tabs->setCurrentIndex(index);
          }
        }
      }
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
    setActiveTabSurface(tabs);
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
      if (auto *tabs = floatingTabSurfaces_.value(dialog, nullptr)) {
        if (auto *widget = floatingWidgets_.value(resolvedId, nullptr)) {
          const int index = tabs->indexOf(widget);
          return index >= 0 && tabs->isTabVisible(index) && dialog->isVisible();
        }
      }
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
    if (!dragSourceId_.isEmpty() && event &&
        event->type() == QEvent::KeyPress &&
        static_cast<QKeyEvent *>(event)->key() == Qt::Key_Escape) {
      // Let QDrag process Escape as usual, but retain the reason so an ignored
      // drag is not mistaken for an intentional detach-to-floating operation.
      dockDragCancelled_ = true;
    }
    if (event && (event->type() == QEvent::FocusIn ||
                  event->type() == QEvent::MouseButtonPress)) {
      if (auto *widget = qobject_cast<QWidget *>(watched)) {
        if (auto *tabs = tabSurfaceForObject(widget)) {
          setActiveTabSurface(tabs);
        }
      }
    }

    if (auto *button = qobject_cast<QToolButton *>(watched)) {
      const bool mouseRelease = event->type() == QEvent::MouseButtonRelease &&
          static_cast<QMouseEvent *>(event)->button() == Qt::LeftButton &&
          button->rect().contains(static_cast<QMouseEvent *>(event)->position().toPoint());
      const bool keyRelease = event->type() == QEvent::KeyRelease &&
          (static_cast<QKeyEvent *>(event)->key() == Qt::Key_Space ||
           static_cast<QKeyEvent *>(event)->key() == Qt::Key_Return ||
           static_cast<QKeyEvent *>(event)->key() == Qt::Key_Enter) &&
          !static_cast<QKeyEvent *>(event)->isAutoRepeat();
      if (button->property("artifactDockTabList").toBool() &&
          button->isDown() && (mouseRelease || keyRelease)) {
        button->setDown(false);
        showTabListMenu(qobject_cast<QTabWidget *>(button->parentWidget()),
                        button->mapToGlobal(button->rect().bottomLeft()));
        return true;
      }
      const QString id = button->property("artifactDockCloseId").toString();
      if (!id.isEmpty()) {
        if (button->isDown() && (mouseRelease || keyRelease)) {
          button->setDown(false);
          dragSourceId_.clear();
          const QByteArray beforeState = beginLayoutMutation();
          setDockVisible(id, false);
          finishLayoutMutation(beforeState, tr("Close panel"));
          return true;
        }
      }
      const QString dockBackId = button->property("artifactDockBackId").toString();
      if (button->property("artifactDockBackGroup").toBool()) {
        if (button->isDown() && (mouseRelease || keyRelease)) {
          button->setDown(false);
          dockFloatingGroup(qobject_cast<QDialog *>(button->window()));
          return true;
        }
      }
      if (!dockBackId.isEmpty()) {
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
    } else if (event->type() == QEvent::ContextMenu && tabBar) {
      const auto *contextEvent = static_cast<QContextMenuEvent *>(event);
      const int index = tabBar->tabAt(contextEvent->pos());
      const QString dockId = index >= 0
          ? dockIdForWidget(tabs->widget(index))
          : QString{};
      if (!dockId.isEmpty()) {
        showDockTabContextMenu(dockId, contextEvent->globalPos());
        return true;
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
      dockDragCancelled_ = false;
      const Qt::DropAction result = drag->exec(Qt::MoveAction);
      hideDropPreview();
      if (result == Qt::IgnoreAction && !dockDragCancelled_ &&
          docks_.contains(dragSourceId_)) {
        const QPoint globalPosition = tabBar->mapToGlobal(mouseEvent->position().toPoint());
        floatDockWidget(dragSourceId_, QRect(globalPosition, QSize(560, 400)));
      }
      dockDragCancelled_ = false;
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
        const int insertionIndex = tabInsertionIndex(tabs, tabPosition);
        if (tabs->tabBar()->tabAt(tabPosition) >= 0) {
          showTabInsertionPreview(tabs, insertionIndex);
        } else {
          showDropPreview(QRect(tabs->mapTo(this, tabs->rect().topLeft()),
                               tabs->size()));
        }
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
        const int insertionIndex = tabInsertionIndex(tabs, tabPosition);
        auto *source = docks_.value(sourceId, nullptr);
        auto *sourceTabs = source ? tabsForWidget(source) : nullptr;
        if (source && sourceTabs == tabs) {
          const int sourceIndex = tabs->indexOf(source);
          if (sourceIndex >= 0) {
            const QByteArray beforeState = beginLayoutMutation();
            int adjustedInsertionIndex = insertionIndex;
            const QString title = titles_.value(sourceId, sourceId);
            tabs->removeTab(sourceIndex);
            if (sourceIndex < adjustedInsertionIndex) --adjustedInsertionIndex;
            adjustedInsertionIndex = std::clamp(adjustedInsertionIndex, 0, tabs->count());
            installCloseButton(tabs, tabs->insertTab(adjustedInsertionIndex, source, title), sourceId);
            tabs->setCurrentWidget(source);
            finishLayoutMutation(beforeState, tr("Reorder tab"));
            dropEvent->acceptProposedAction();
            return true;
          }
        }
        if (targetIndex >= 0) {
          if (moveDockWidgetToTabAt(sourceId, tabs, insertionIndex)) {
            dropEvent->acceptProposedAction();
            return true;
          }
        } else if (floatingDialogForTabs(tabs) &&
                   moveDockWidgetToTabAt(sourceId, tabs, tabs->count())) {
          dropEvent->acceptProposedAction();
          return true;
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
  QByteArray beginLayoutMutation() const {
    if (!layoutMutationCallback_ || restoringLayout_ ||
        layoutMutationBatchDepth_ > 0) {
      return {};
    }
    return saveLayoutState();
  }

  void finishLayoutMutation(const QByteArray &beforeState,
                            const QString &label) {
    if (beforeState.isEmpty() || !layoutMutationCallback_ ||
        restoringLayout_ || layoutMutationBatchDepth_ > 0) {
      return;
    }
    layoutMutationCallback_(beforeState, label);
  }

  void beginLayoutMutationBatch(const QString &label) {
    if (layoutMutationBatchDepth_ == 0) {
      layoutMutationBatchBeforeState_ = beginLayoutMutation();
      layoutMutationBatchLabel_ = label;
    }
    ++layoutMutationBatchDepth_;
  }

  void endLayoutMutationBatch() {
    if (layoutMutationBatchDepth_ == 0 || --layoutMutationBatchDepth_ != 0) {
      return;
    }
    const QByteArray beforeState = std::move(layoutMutationBatchBeforeState_);
    const QString label = std::move(layoutMutationBatchLabel_);
    finishLayoutMutation(beforeState, label);
  }

  DockArea areaForPosition(const QPoint &position) const {
    if (topTabs_ && topTabs_->isVisible() &&
        topTabs_->rect().contains(topTabs_->mapFrom(this, position))) {
      return DockArea::Top;
    }
    if (bottomTabs_ && bottomTabs_->isVisible() &&
        bottomTabs_->rect().contains(bottomTabs_->mapFrom(this, position))) {
      return DockArea::Bottom;
    }
    const QRect surface = rect();
    const int verticalEdge = std::min(
        std::max(24, surface.height() / 5), std::max(1, surface.height() / 2));
    const int horizontalEdge = std::min(
        std::max(32, surface.width() / 5), std::max(1, surface.width() / 2));
    if (position.y() < surface.top() + verticalEdge) {
      return DockArea::Top;
    }
    if (position.y() > surface.bottom() - verticalEdge) {
      return DockArea::Bottom;
    }
    if (position.x() < surface.left() + horizontalEdge) {
      return DockArea::Left;
    }
    if (position.x() > surface.right() - horizontalEdge) {
      return DockArea::Right;
    }
    return DockArea::Center;
  }

  QRect dropPreviewRectForArea(DockArea area) const {
    auto *tabs = tabsForArea(area);
    if (tabs && tabs->isVisible() && !tabs->size().isEmpty()) {
      return QRect(tabs->mapTo(const_cast<NativeDockSurface *>(this),
                               tabs->rect().topLeft()),
                   tabs->size());
    }
    const QRect surface = rect();
    const int verticalEdge = std::min(
        std::max(24, surface.height() / 5), std::max(1, surface.height() / 2));
    const int horizontalEdge = std::min(
        std::max(32, surface.width() / 5), std::max(1, surface.width() / 2));
    switch (area) {
    case DockArea::Top:
      return QRect(surface.left(), surface.top(), surface.width(), verticalEdge);
    case DockArea::Bottom:
      return QRect(surface.left(), surface.bottom() - verticalEdge + 1,
                   surface.width(), verticalEdge);
    case DockArea::Left:
      return QRect(surface.left(), surface.top(), horizontalEdge, surface.height());
    case DockArea::Right:
      return QRect(surface.right() - horizontalEdge + 1, surface.top(),
                   horizontalEdge, surface.height());
    case DockArea::Center:
      return surface.adjusted(horizontalEdge, verticalEdge,
                              -horizontalEdge, -verticalEdge);
    }
    return surface;
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

  int tabInsertionIndex(const QTabWidget *tabs,
                        const QPoint &tabBarPosition) const {
    if (!tabs || !tabs->tabBar()) {
      return 0;
    }
    const int targetIndex = tabs->tabBar()->tabAt(tabBarPosition);
    if (targetIndex < 0) {
      return tabs->count();
    }
    return tabBarPosition.x() > tabs->tabBar()->tabRect(targetIndex).center().x()
        ? targetIndex + 1
        : targetIndex;
  }

  void showTabInsertionPreview(QTabWidget *tabs, int insertionIndex) {
    if (!tabs || !tabs->tabBar() || !dropPreview_) {
      return;
    }
    auto *tabBar = tabs->tabBar();
    const int boundedIndex = std::clamp(insertionIndex, 0, tabs->count());
    int x = tabBar->contentsRect().right();
    if (boundedIndex < tabs->count()) {
      x = tabBar->tabRect(boundedIndex).left();
    }
    const QRect marker(tabBar->mapTo(this, QPoint(x - 2, 1)),
                       QSize(4, std::max(1, tabBar->height() - 2)));
    showDropPreview(marker);
  }

  bool isFloatingTabSurface(const QTabWidget *tabs) const {
    for (auto it = floatingTabSurfaces_.cbegin();
         it != floatingTabSurfaces_.cend(); ++it) {
      if (it.value() == tabs) {
        return true;
      }
    }
    return false;
  }

  void applyAreaTabPositions(const QHash<int, bool> &positions) {
    areaTabsAtBottom_.clear();
    for (const DockArea area : {DockArea::Left, DockArea::Right,
                                DockArea::Top, DockArea::Bottom,
                                DockArea::Center}) {
      const bool atBottom = positions.value(static_cast<int>(area), false);
      if (auto *tabs = tabsForArea(area)) {
        tabs->setTabPosition(atBottom ? QTabWidget::South
                                      : QTabWidget::North);
      }
      if (atBottom) {
        areaTabsAtBottom_.insert(static_cast<int>(area), true);
      }
    }
  }

  void setAreaTabPosition(DockArea area, bool atBottom) {
    auto *tabs = tabsForArea(area);
    if (!tabs || isFloatingTabSurface(tabs) ||
        (tabs->tabPosition() == QTabWidget::South) == atBottom) {
      return;
    }
    const QByteArray beforeState = beginLayoutMutation();
    tabs->setTabPosition(atBottom ? QTabWidget::South : QTabWidget::North);
    if (atBottom) {
      areaTabsAtBottom_.insert(static_cast<int>(area), true);
    } else {
      areaTabsAtBottom_.remove(static_cast<int>(area));
    }
    finishLayoutMutation(beforeState, tr("Change tab bar position"));
  }

  void addTabPositionActions(QMenu *menu, QTabWidget *tabs,
                             QAction **topAction,
                             QAction **bottomAction) {
    if (!menu || !tabs || isFloatingTabSurface(tabs)) {
      return;
    }
    const DockArea area = areaForTabs(tabs);
    QMenu *positionMenu = menu->addMenu(
        tr("Tab bar position (%1 area)").arg(dockAreaToString(area)));
    if (topAction) {
      *topAction = positionMenu->addAction(tr("Tabs at top"));
      (*topAction)->setCheckable(true);
      (*topAction)->setChecked(tabs->tabPosition() == QTabWidget::North);
    }
    if (bottomAction) {
      *bottomAction = positionMenu->addAction(tr("Tabs at bottom"));
      (*bottomAction)->setCheckable(true);
      (*bottomAction)->setChecked(tabs->tabPosition() == QTabWidget::South);
    }
  }

  void showTabListMenu(QTabWidget *tabs, const QPoint &globalPosition) {
    if (!tabs || tabs->count() == 0) {
      return;
    }
    QMenu menu(tabs);
    for (int index = 0; index < tabs->count(); ++index) {
      const QString dockId = dockIdForWidget(tabs->widget(index));
      if (dockId.isEmpty()) {
        continue;
      }
      QAction *action = menu.addAction(tabs->tabText(index));
      action->setCheckable(true);
      action->setChecked(index == tabs->currentIndex());
      action->setEnabled(tabs->isTabVisible(index));
      action->setData(dockId);
    }
    QAction *topPositionAction = nullptr;
    QAction *bottomPositionAction = nullptr;
    addTabPositionActions(&menu, tabs, &topPositionAction,
                          &bottomPositionAction);
    if (QAction *selected = menu.exec(globalPosition)) {
      if (selected == topPositionAction) {
        setAreaTabPosition(areaForTabs(tabs), false);
      } else if (selected == bottomPositionAction) {
        setAreaTabPosition(areaForTabs(tabs), true);
      } else {
        activateDock(selected->data().toString());
      }
    }
  }

  void showDockTabContextMenu(const QString &dockId,
                              const QPoint &globalPosition) {
    const QString resolvedId = resolveDockId(dockId);
    const bool docked = docks_.contains(resolvedId);
    const bool floating = floatingDialogs_.contains(resolvedId);
    if (resolvedId.isEmpty() || (!docked && !floating)) {
      return;
    }
    QMenu menu(this);
    QAction *placementAction = nullptr;
    QAction *floatGroupAction = nullptr;
    QAction *topPositionAction = nullptr;
    QAction *bottomPositionAction = nullptr;
    if (docked) {
      placementAction = menu.addAction(tr("Float panel"));
      floatGroupAction = menu.addAction(tr("Float tab group"));
      auto *tabs = tabsForWidget(docks_.value(resolvedId, nullptr));
      floatGroupAction->setEnabled(tabs && tabs->count() > 1);
      addTabPositionActions(&menu, tabs, &topPositionAction,
                            &bottomPositionAction);
    } else {
      placementAction = menu.addAction(tr("Dock panel back"));
    }
    QAction *pinAction = menu.addAction(
        dockPinned(resolvedId) ? tr("Unpin panel") : tr("Pin panel"));
    QAction *closeAction = menu.addAction(tr("Close panel"));
    closeAction->setEnabled(!dockPinned(resolvedId));
    QAction *selected = menu.exec(globalPosition);
    if (selected == placementAction) {
      if (docked) {
        floatDockWidget(resolvedId);
      } else {
        dockFloatingWidget(resolvedId,
                           areas_.value(resolvedId, DockArea::Center));
      }
    } else if (selected == floatGroupAction) {
      floatDockTabGroup(resolvedId);
    } else if (selected == topPositionAction) {
      setAreaTabPosition(areas_.value(resolvedId, DockArea::Center), false);
    } else if (selected == bottomPositionAction) {
      setAreaTabPosition(areas_.value(resolvedId, DockArea::Center), true);
    } else if (selected == pinAction) {
      const QByteArray beforeState = beginLayoutMutation();
      setDockPinned(resolvedId, !dockPinned(resolvedId));
      finishLayoutMutation(beforeState, tr("Pin panel"));
    } else if (selected == closeAction) {
      const QByteArray beforeState = beginLayoutMutation();
      setDockVisible(resolvedId, false);
      finishLayoutMutation(beforeState, tr("Close panel"));
    }
  }

  void installCloseButton(QTabWidget *tabs, int index, const QString &id) {
    auto *buttons = new QWidget(tabs->tabBar());
    auto *buttonLayout = new QHBoxLayout(buttons);
    buttonLayout->setContentsMargins(0, 0, 0, 0);
    buttonLayout->setSpacing(1);

    auto *closeButton = new QToolButton(buttons);
    closeButton->setAutoRaise(true);
    closeButton->setIcon(closeButton->style()->standardIcon(QStyle::SP_TitleBarCloseButton));
    closeButton->setToolTip(tr("Close panel"));
    closeButton->setAccessibleName(tr("Close panel"));
    closeButton->setProperty("artifactDockCloseId", id);
    closeButton->setProperty("artifactDockCloseButton", true);
    closeButton->setFixedSize(20, 20);
    closeButton->installEventFilter(this);
    buttonLayout->addWidget(closeButton);

    buttons->setFixedSize(20, 20);
    tabs->tabBar()->setTabButton(index, QTabBar::RightSide, buttons);
    syncPinButton(id, pinned_.value(id, false));
  }

  void syncPinButton(const QString &dockId, bool pinned) {
    auto *widget = dockWidget(dockId);
    if (!widget) return;
    auto *tabs = tabsForWidget(widget);
    if (!tabs) return;
    const int index = tabs->indexOf(widget);
    if (index < 0) return;
    auto *side = tabs->tabBar()->tabButton(index, QTabBar::RightSide);
    if (!side) return;
    const auto buttons = side->findChildren<QToolButton *>();
    for (auto *button : buttons) {
      if (!button) {
        continue;
      }
      if (button->property("artifactDockCloseId").toString() == dockId) {
        button->setEnabled(!pinned);
        button->setToolTip(pinned ? tr("Unpin panel before closing")
                                  : tr("Close panel"));
      }
    }
  }

  void dockFloatingGroup(QDialog *dialog) {
    if (!dialog) {
      return;
    }
    QStringList dockIds;
    if (auto *tabs = floatingTabSurfaces_.value(dialog, nullptr)) {
      for (int index = 0; index < tabs->count(); ++index) {
        const QString dockId = dockIdForWidget(tabs->widget(index));
        if (!dockId.isEmpty()) {
          dockIds.push_back(dockId);
        }
      }
    }
    if (dockIds.isEmpty()) {
      for (auto it = floatingDialogs_.cbegin(); it != floatingDialogs_.cend();
           ++it) {
        if (it.value() == dialog) {
          dockIds.push_back(it.key());
        }
      }
      dockIds.sort();
    }
    beginLayoutMutationBatch(tr("Dock tab group"));
    for (const QString &dockId : dockIds) {
      dockFloatingWidget(dockId, areas_.value(dockId, DockArea::Center));
    }
    endLayoutMutationBatch();
  }

  void installFloatingHeader(QDialog *dialog, QVBoxLayout *layout,
                             const QString &title) {
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
    dockBack->setToolTip(tr("Dock tab group back"));
    dockBack->setAccessibleName(tr("Dock tab group back"));
    dockBack->setProperty("artifactDockBackGroup", true);
    dockBack->installEventFilter(this);
    headerLayout->addWidget(dockBack);
    layout->addWidget(header);
  }

  QTabWidget *createFloatingTabSurface(QDialog *dialog,
                                       QVBoxLayout *layout) {
    if (!dialog || !layout) {
      return nullptr;
    }
    auto *tabs = createTabSurface(dialog);
    tabs->installEventFilter(this);
    tabs->tabBar()->installEventFilter(this);
    if (auto *tabListButton = qobject_cast<QToolButton *>(
            tabs->cornerWidget(Qt::TopRightCorner))) {
      tabListButton->installEventFilter(this);
    }
    layout->addWidget(tabs);
    floatingTabSurfaces_.insert(dialog, tabs);
    return tabs;
  }

  static QTabWidget *createTabSurface(QWidget *parent) {
    auto *tabs = new DockTabSurface(parent);
    tabs->setDocumentMode(true);
    tabs->setMovable(true);
    tabs->tabBar()->setExpanding(false);
    tabs->tabBar()->setUsesScrollButtons(true);
    tabs->tabBar()->setElideMode(Qt::ElideRight);
    auto *tabListButton = new QToolButton(tabs);
    tabListButton->setAutoRaise(true);
    tabListButton->setArrowType(Qt::DownArrow);
    tabListButton->setToolTip(tr("Show panel list"));
    tabListButton->setAccessibleName(tr("Show panel list"));
    tabListButton->setProperty("artifactDockTabList", true);
    tabListButton->setFixedSize(22, 22);
    tabs->setTabListButton(tabListButton);
    tabs->setCornerWidget(tabListButton, Qt::TopRightCorner);
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
    for (auto it = floatingTabSurfaces_.cbegin();
         it != floatingTabSurfaces_.cend(); ++it) {
      if (it.value() && it.value()->indexOf(widget) >= 0) {
        return it.value();
      }
    }
    return nullptr;
  }

  QDialog *floatingDialogForTabs(const QTabWidget *tabs) const {
    for (auto it = floatingTabSurfaces_.cbegin();
         it != floatingTabSurfaces_.cend(); ++it) {
      if (it.value() == tabs) {
        return it.key();
      }
    }
    return nullptr;
  }

  QTabWidget *tabSurfaceForObject(const QWidget *widget) const {
    if (!widget) {
      return nullptr;
    }
    for (auto *tabs : {leftTabs_, centerTabs_, rightTabs_, topTabs_,
                       bottomTabs_}) {
      if (tabs && (tabs == widget || tabs->isAncestorOf(widget))) {
        return tabs;
      }
    }
    for (auto it = floatingTabSurfaces_.cbegin();
         it != floatingTabSurfaces_.cend(); ++it) {
      if (it.value() && (it.value() == widget ||
                         it.value()->isAncestorOf(widget))) {
        return it.value();
      }
    }
    return nullptr;
  }

  void setActiveTabSurface(QTabWidget *tabs) {
    if (!tabs) {
      return;
    }
    if (activeTabs_ == tabs) {
      static_cast<DockTabSurface *>(tabs)->refreshFocusChrome();
      return;
    }
    if (activeTabs_) {
      activeTabs_->setProperty("artifactNativeActivePanel", false);
      static_cast<DockTabSurface *>(activeTabs_.data())->refreshFocusChrome();
    }
    activeTabs_ = tabs;
    activeTabs_->setProperty("artifactNativeActivePanel", true);
    static_cast<DockTabSurface *>(activeTabs_.data())->refreshFocusChrome();
  }

  QTabWidget *topTabs_ = nullptr;
  QTabWidget *leftTabs_ = nullptr;
  QTabWidget *centerTabs_ = nullptr;
  QTabWidget *rightTabs_ = nullptr;
  QTabWidget *bottomTabs_ = nullptr;
  QSplitter *splitter_ = nullptr;
  QSplitter *verticalSplitter_ = nullptr;
  DockDropPreview *dropPreview_ = nullptr;
  QPointer<QTabWidget> activeTabs_;
  QPoint dragStartPosition_;
  QString dragSourceId_;
  bool dockDragCancelled_ = false;
  LayoutMutationCallback layoutMutationCallback_;
  QByteArray layoutMutationBatchBeforeState_;
  QString layoutMutationBatchLabel_;
  int layoutMutationBatchDepth_ = 0;
  bool restoringLayout_ = false;
  QHash<QString, QWidget *> docks_;
  QHash<QString, QDialog *> floatingDialogs_;
  QHash<QString, QWidget *> floatingWidgets_;
  QHash<QDialog *, QTabWidget *> floatingTabSurfaces_;
  QHash<QString, DockArea> areas_;
  QHash<int, bool> areaTabsAtBottom_;
  QHash<QString, QString> titles_;
  QHash<QString, bool> pinned_;
};

} // namespace Artifact
