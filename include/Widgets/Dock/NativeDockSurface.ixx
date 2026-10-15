module;

#include <algorithm>
#include <functional>
#include <utility>
#include <limits>
#include <QHash>
#include <QSet>
#include <QByteArray>
#include <QBasicTimer>
#include <QColor>
#include <QApplication>
#include <QContextMenuEvent>
#include <QDebug>
#include <QCursor>
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
#include <QCommonStyle>
#include <QProxyStyle>
#include <QStyleOptionTab>
#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>
#include <QPen>
#include <QResizeEvent>
#include <QTimerEvent>
#include <QPointer>
#include <QVariant>
#include <QPalette>
#include <QLabel>
#include <QFrame>
#include <QFont>
#include <QFontMetrics>
#include <QMenu>
#include <QAction>
#include <QSize>
#include <QSizePolicy>
#include <QString>
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

      QSize tabSizeHint(int index) const override {
        QSize size = QTabBar::tabSizeHint(index);
        size.setHeight((size.height() * 6 + 4) / 5);
        return size;
      }

      void setDropInsertionIndex(int index) {
        if (dropInsertionIndex_ == index) {
          return;
        }
        dropInsertionIndex_ = index;
        update();
      }

      void clearDropInsertionPreview() {
        setDropInsertionIndex(-1);
      }

      bool hasElidedVisibleTabTitle() const {
        if (!tabs_ || count() <= 1) return false;
        QStyleOptionTab option;
        const QFontMetrics metrics(font());
        for (int index = 0; index < count(); ++index) {
          if (!isTabVisible(index)) continue;
          initStyleOption(&option, index);
          const QRect textRect = style()->subElementRect(
              QStyle::SE_TabBarTabText, &option, this);
          if (textRect.width() > 0 &&
              metrics.horizontalAdvance(option.text) > textRect.width()) {
            return true;
          }
        }
        return false;
      }

      bool hasOverflowingVisibleTab() const {
        if (!tabs_ || count() <= 1) return false;
        const QRect available = contentsRect();
        for (int index = 0; index < count(); ++index) {
          if (!isTabVisible(index)) continue;
          const QRect tab = tabRect(index);
          if (tab.left() < available.left() ||
              tab.right() > available.right()) {
            return true;
          }
        }
        return false;
      }

    protected:
      void resizeEvent(QResizeEvent *event) override {
        QTabBar::resizeEvent(event);
        if (tabs_) tabs_->refreshTabListButtonVisibility();
      }

      void paintEvent(QPaintEvent *event) override {
        QTabBar::paintEvent(event);
        if (dropInsertionIndex_ >= 0) {
          const int insertionIndex = std::clamp(dropInsertionIndex_, 0, count());
          const int markerX = insertionIndex < count()
                                  ? tabRect(insertionIndex).left()
                                  : contentsRect().right();
          QPainter markerPainter(this);
          QPen markerPen(palette().color(QPalette::Highlight), 3.0);
          markerPen.setCapStyle(Qt::SquareCap);
          markerPainter.setPen(markerPen);
          markerPainter.drawLine(markerX, contentsRect().top() + 2,
                                 markerX, contentsRect().bottom() - 2);
        }
        if (!tabs_ ||
            !tabs_->property("artifactNativeActivePanel").toBool() ||
            !tabs_->currentPanelFocusable()) {
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
      int dropInsertionIndex_ = -1;
    };

    class DockSurfaceStyle final : public QProxyStyle {
    public:
      explicit DockSurfaceStyle(DockTabSurface *tabs)
          : QProxyStyle(new QCommonStyle()), tabs_(tabs) {
        // QProxyStyle owns its base; never transfer the shared application style.
      }

      int pixelMetric(PixelMetric metric, const QStyleOption *option = nullptr,
                      const QWidget *widget = nullptr) const override {
        // Preserve the shared tab spacing without depending on app modules.
        switch (metric) {
        case PM_TabBarTabHSpace: return 12;
        case PM_TabBarTabVSpace: return 8;
        case PM_DefaultFrameWidth: return 1;
        default: return QProxyStyle::pixelMetric(metric, option, widget);
        }
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
          if (tabs_->hasSeparateDockAreaTitleBar()) {
            const int maximumTop = std::max(result.top(), result.bottom() - 4);
            result.setTop(std::min(
                result.top() + tabs_->dockAreaTitleBarHeight(), maximumTop));
          }
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
      cornerWidget_ = new QWidget(this);
      cornerLayout_ = new QHBoxLayout(cornerWidget_);
      cornerLayout_->setContentsMargins(2, 0, 2, 0);
      cornerLayout_->setSpacing(1);
      dockAreaTitleLabel_ = new QLabel(cornerWidget_);
      dockAreaTitleLabel_->setSizePolicy(QSizePolicy::Ignored,
                                         QSizePolicy::Preferred);
      dockAreaTitleLabel_->setProperty("artifactDockAreaTitleDrag", true);
      cornerLayout_->addWidget(dockAreaTitleLabel_, 1);
      dockAreaTitleLabel_->hide();
    }

    void setTabListButton(QToolButton *button) {
      tabListButton_ = button;
      if (button && cornerLayout_ && cornerLayout_->indexOf(button) < 0) {
        cornerLayout_->addWidget(button);
      }
      syncTabListButtonCorner();
    }

    void setCloseGroupButton(QToolButton *button) {
      closeGroupButton_ = button;
      if (button && cornerLayout_ && cornerLayout_->indexOf(button) < 0) {
        cornerLayout_->insertWidget(
            cornerLayout_->indexOf(dockAreaTitleLabel_) + 1, button);
      }
      if (button) button->hide();
      syncTabListButtonCorner();
    }

    void setAutoHideButton(QToolButton *button) {
      autoHideButton_ = button;
      if (button && cornerLayout_ && cornerLayout_->indexOf(button) < 0) {
        cornerLayout_->insertWidget(
            cornerLayout_->indexOf(dockAreaTitleLabel_) + 1, button);
      }
      if (button) button->hide();
      syncTabListButtonCorner();
    }

    void setFloatGroupButton(QToolButton *button) {
      floatGroupButton_ = button;
      if (button && cornerLayout_ && cornerLayout_->indexOf(button) < 0) {
        const int closeIndex = cornerLayout_->indexOf(closeGroupButton_);
        cornerLayout_->insertWidget(closeIndex >= 0 ? closeIndex + 1 : 0,
                                    button);
      }
      if (button) button->hide();
      syncTabListButtonCorner();
    }

    QToolButton *closeGroupButton() const { return closeGroupButton_; }
    QToolButton *autoHideButton() const { return autoHideButton_; }
    QToolButton *floatGroupButton() const { return floatGroupButton_; }
    QToolButton *tabListButton() const { return tabListButton_; }
    QLabel *dockAreaTitleLabel() const { return dockAreaTitleLabel_; }

    void setDockAreaTitleBarEnabled(bool enabled) {
      dockAreaTitleBarEnabled_ = enabled;
      syncTabListButtonCorner();
    }

    void refreshDockAreaTitleBarPlacement() {
      syncTabListButtonCorner();
    }

    bool hasSeparateDockAreaTitleBar() const {
      return !dockAreaTitleBarSuppressed_ && dockAreaTitleBarEnabled_ &&
             tabPosition() == QTabWidget::South;
    }

    void setDockAreaTitleBarSuppressed(bool suppressed) {
      if (dockAreaTitleBarSuppressed_ == suppressed) return;
      dockAreaTitleBarSuppressed_ = suppressed;
      syncTabListButtonCorner();
    }

    int dockAreaTitleBarHeight() const {
      return cornerWidget_ ? cornerWidget_->sizeHint().height() : 0;
    }

    void setDockAreaTitle(const QString &title) {
      if (dockAreaTitle_ != title) {
        dockAreaTitle_ = title;
        updateDockAreaTitleLabel();
      }
    }

    void setDockAreaTitleActions(const QList<QAction *> &actions) {
      QList<QAction *> normalizedActions;
      for (QAction *action : actions) {
        if (action && !normalizedActions.contains(action)) {
          normalizedActions.push_back(action);
        }
      }
      if (normalizedActions.size() == titleBarActionButtons_.size()) {
        bool unchanged = true;
        for (qsizetype index = 0; index < normalizedActions.size(); ++index) {
          if (titleBarActionButtons_.at(index)->defaultAction() !=
              normalizedActions.at(index)) {
            unchanged = false;
            break;
          }
        }
        if (unchanged) return;
      }

      for (QToolButton *button : titleBarActionButtons_) {
        if (cornerLayout_) cornerLayout_->removeWidget(button);
        delete button;
      }
      titleBarActionButtons_.clear();
      if (!cornerLayout_) return;

      int insertionIndex = cornerLayout_->count();
      for (QToolButton *button : {autoHideButton_, floatGroupButton_,
                                  closeGroupButton_, tabListButton_}) {
        const int index = cornerLayout_->indexOf(button);
        if (index >= 0) insertionIndex = std::min(insertionIndex, index);
      }
      for (QAction *action : normalizedActions) {
        auto *button = new QToolButton(cornerWidget_);
        button->setAutoRaise(true);
        button->setDefaultAction(action);
        cornerLayout_->insertWidget(insertionIndex++, button);
        titleBarActionButtons_.push_back(button);
      }
      syncTabListButtonCorner();
    }

    void setDynamicTabListButtonVisibility(bool enabled) {
      dynamicTabListButtonVisibility_ = enabled;
      refreshTabListButtonVisibility();
    }

    void setTabListButtonEnabled(bool enabled) {
      tabListButtonEnabled_ = enabled;
      refreshTabListButtonVisibility();
    }

    void refreshTabListButtonVisibility() {
      if (!tabListButton_) return;
      const auto *dockTabBar = static_cast<DockTabBar *>(tabBar());
      const bool listNeeded = tabBar()->elideMode() == Qt::ElideNone
          ? dockTabBar->hasOverflowingVisibleTab()
          : dockTabBar->hasElidedVisibleTabTitle();
      const bool visible = tabListButtonEnabled_ &&
          (!dynamicTabListButtonVisibility_ || listNeeded);
      tabListButton_->setVisible(visible);
    }

    void refreshFocusChrome() {
      update();
      tabBar()->update();
    }

    bool currentPanelFocusable() const {
      const QWidget *panel = currentWidget();
      return !panel || !panel->property("artifactDockFocusable").isValid() ||
             panel->property("artifactDockFocusable").toBool();
    }

    void setDropInsertionIndex(int index) {
      static_cast<DockTabBar *>(tabBar())->setDropInsertionIndex(index);
    }

    void clearDropInsertionPreview() {
      static_cast<DockTabBar *>(tabBar())->clearDropInsertionPreview();
    }

  protected:
    bool event(QEvent *event) override {
      const bool handled = QTabWidget::event(event);
      if (event->type() == QEvent::LayoutRequest ||
          event->type() == QEvent::Resize || event->type() == QEvent::Show) {
        syncTabListButtonCorner();
        refreshTabListButtonVisibility();
      }
      return handled;
    }

    void paintEvent(QPaintEvent *event) override {
      QTabWidget::paintEvent(event);
      if (!property("artifactNativeActivePanel").toBool() ||
          !currentPanelFocusable() ||
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
      if (!cornerWidget_ || (!tabListButton_ && !closeGroupButton_ &&
                             !floatGroupButton_ && !autoHideButton_ &&
                             titleBarActionButtons_.isEmpty())) {
        return;
      }
      if (dockAreaTitleBarSuppressed_) {
        if (cornerWidget(Qt::TopRightCorner) == cornerWidget_) {
          setCornerWidget(nullptr, Qt::TopRightCorner);
        }
        if (cornerWidget(Qt::BottomRightCorner) == cornerWidget_) {
          setCornerWidget(nullptr, Qt::BottomRightCorner);
        }
        dockAreaTitleLabel_->hide();
        cornerWidget_->hide();
        return;
      }
      if (hasSeparateDockAreaTitleBar()) {
        dockAreaTitleLabel_->setVisible(true);
        if (cornerWidget(Qt::TopRightCorner) == cornerWidget_) {
          setCornerWidget(nullptr, Qt::TopRightCorner);
        }
        if (cornerWidget(Qt::BottomRightCorner) == cornerWidget_) {
          setCornerWidget(nullptr, Qt::BottomRightCorner);
        }
        if (cornerWidget_->parentWidget() != this) {
          cornerWidget_->setParent(this);
        }
        const int height = dockAreaTitleBarHeight();
        cornerWidget_->setGeometry(1, 1, std::max(0, width() - 2), height);
        if (cornerLayout_) cornerLayout_->activate();
        updateDockAreaTitleLabel();
        cornerWidget_->raise();
        cornerWidget_->show();
      } else {
        dockAreaTitleLabel_->hide();
        if (cornerWidget_->parentWidget() != this) {
          cornerWidget_->setParent(this);
        }
        if (cornerWidget(Qt::TopRightCorner) != cornerWidget_) {
          setCornerWidget(cornerWidget_, Qt::TopRightCorner);
        }
        cornerWidget_->show();
      }
    }

    void updateDockAreaTitleLabel() {
      if (!dockAreaTitleLabel_) return;
      dockAreaTitleLabel_->setToolTip(dockAreaTitle_);
      dockAreaTitleLabel_->setAccessibleName(dockAreaTitle_);
      dockAreaTitleLabel_->setText(QFontMetrics(dockAreaTitleLabel_->font())
          .elidedText(dockAreaTitle_, Qt::ElideRight,
                      std::max(0, dockAreaTitleLabel_->contentsRect().width())));
    }

    QWidget *cornerWidget_ = nullptr;
    QHBoxLayout *cornerLayout_ = nullptr;
    QToolButton *tabListButton_ = nullptr;
    QToolButton *closeGroupButton_ = nullptr;
    QToolButton *floatGroupButton_ = nullptr;
    QToolButton *autoHideButton_ = nullptr;
    QList<QToolButton *> titleBarActionButtons_;
    QLabel *dockAreaTitleLabel_ = nullptr;
    QString dockAreaTitle_;
    bool dockAreaTitleBarEnabled_ = false;
    bool dynamicTabListButtonVisibility_ = false;
    bool tabListButtonEnabled_ = true;
    bool dockAreaTitleBarSuppressed_ = false;
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
        pendingHandleIndex_ = -1;
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
        if (owner_->opaqueResize()) {
          owner_->moveSplitter(position, handleIndex);
        } else {
          owner_->previewHandlePosition(position);
          pendingPosition_ = position;
          pendingHandleIndex_ = handleIndex;
        }
        event->accept();
      }

      void mouseReleaseEvent(QMouseEvent *event) override {
        if (dragging_ && event->button() == Qt::LeftButton) {
          if (owner_ && pendingHandleIndex_ >= 1) {
            owner_->commitHandlePosition(pendingPosition_,
                                         pendingHandleIndex_);
          } else if (owner_ && !owner_->opaqueResize()) {
            owner_->previewHandlePosition(-1);
          }
          dragging_ = false;
          pendingHandleIndex_ = -1;
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
      int pendingPosition_ = -1;
      int pendingHandleIndex_ = -1;
    };

  public:
    explicit DockSplitter(Qt::Orientation orientation, QWidget *parent)
        : QSplitter(orientation, parent) {
      setChildrenCollapsible(false);
      setHandleWidth(8);
      setOpaqueResize(false);
    }

    void previewHandlePosition(int position) {
      setRubberBand(position);
    }

    void commitHandlePosition(int position, int handleIndex) {
      moveSplitter(position, handleIndex);
      setRubberBand(-1);
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

    void showAreaTargets(const QRect &previewRect, const QPoint &targetCenter,
                         DockArea centerTargetArea,
                         DockArea selectedButton,
                         int allowedAreas = kAllDockAreaDropTargets) {
      areaTargetMode_ = true;
      containerTargetMode_ = false;
      autoHideInsertionMode_ = false;
      allowedAreas_ = allowedAreas & kAllDockAreaDropTargets;
      centerTargetArea_ = centerTargetArea;
      selectedButton_ = selectedButton;
      previewRect_ = previewRect;
      targetCenter_ = targetCenter;
      update();
    }

    void showContainerTargets(const QRect &previewRect,
                              DockArea selectedButton) {
      areaTargetMode_ = true;
      containerTargetMode_ = true;
      autoHideInsertionMode_ = false;
      allowedAreas_ = kAllDockAreaDropTargets;
      centerTargetArea_ = DockArea::Center;
      selectedButton_ = selectedButton;
      previewRect_ = previewRect;
      targetCenter_ = rect().center();
      update();
    }

    void showAutoHideInsertion(const QRect &barRect,
                               const QRect &markerRect) {
      areaTargetMode_ = false;
      containerTargetMode_ = false;
      autoHideInsertionMode_ = true;
      allowedAreas_ = kAllDockAreaDropTargets;
      previewRect_ = barRect;
      insertionMarkerRect_ = markerRect;
      update();
    }

    void showRectPreview() {
      areaTargetMode_ = false;
      containerTargetMode_ = false;
      autoHideInsertionMode_ = false;
      allowedAreas_ = kAllDockAreaDropTargets;
      update();
    }

    bool showsAreaTargets() const { return areaTargetMode_; }
    bool showsContainerTargets() const { return containerTargetMode_; }

    bool isDisallowedAreaTargetAt(const QPoint &position) const {
      if (!areaTargetMode_ || containerTargetMode_) return false;
      for (const DockArea area : {DockArea::Center, DockArea::Top,
                                  DockArea::Right, DockArea::Bottom,
                                  DockArea::Left}) {
        if (!(allowedAreas_ & dockAreaDropTargetBit(area)) &&
            targetRect(area).contains(position)) {
          return true;
        }
      }
      return false;
    }

    bool containsAreaTarget(const QPoint &position) const {
      if (!areaTargetMode_) {
        return false;
      }
      for (const DockArea area : {DockArea::Center, DockArea::Top,
                                  DockArea::Right, DockArea::Bottom,
                                  DockArea::Left}) {
        if (containerTargetMode_ && area == DockArea::Center) {
          continue;
        }
        if (!(allowedAreas_ & dockAreaDropTargetBit(area))) {
          continue;
        }
        if (targetRect(area).contains(position)) {
          return true;
        }
      }
      return false;
    }

    bool areaTargetAt(const QPoint &targetCenter, const QPoint &position,
                      DockArea &area,
                      int allowedAreas = kAllDockAreaDropTargets) const {
      for (const DockArea candidate : {DockArea::Center, DockArea::Top,
                                       DockArea::Right, DockArea::Bottom,
                                       DockArea::Left}) {
        if (!(allowedAreas & dockAreaDropTargetBit(candidate))) {
          continue;
        }
        if (targetRect(candidate, targetCenter, false).contains(position)) {
          area = candidate;
          return true;
        }
      }
      return false;
    }

    DockArea targetAreaAt(const QPoint &position,
                          DockArea fallback) const {
      if (containsAreaTarget(position)) {
        const DockArea target = targetButtonAt(position);
        return target == DockArea::Center ? centerTargetArea_ : target;
      }
      return fallback;
    }

    DockArea targetButtonAt(const QPoint &position) const {
      for (const DockArea area : {DockArea::Center, DockArea::Top,
                                  DockArea::Right, DockArea::Bottom,
                                  DockArea::Left}) {
        if (!(allowedAreas_ & dockAreaDropTargetBit(area))) {
          continue;
        }
        if (targetRect(area).contains(position)) {
          return area;
        }
      }
      return DockArea::Center;
    }

  protected:
    void paintEvent(QPaintEvent *) override {
      QPainter painter(this);
      const QColor accent = palette().color(QPalette::Highlight);
      if (autoHideInsertionMode_) {
        QColor fill = accent;
        fill.setAlpha(48);
        painter.fillRect(previewRect_, fill);
        painter.setPen(QPen(accent, 1.0));
        painter.setBrush(Qt::NoBrush);
        painter.drawRect(previewRect_.adjusted(0, 0, -1, -1));
        painter.fillRect(insertionMarkerRect_, accent);
        return;
      }
      if (areaTargetMode_) {
        painter.setRenderHint(QPainter::Antialiasing, true);
        if (!previewRect_.isEmpty()) {
          QColor fill = accent;
          fill.setAlpha(42);
          painter.fillRect(previewRect_, fill);
          painter.setPen(QPen(accent, 2.0));
          painter.setBrush(Qt::NoBrush);
          painter.drawRect(previewRect_.adjusted(1, 1, -2, -2));
        }

        for (const DockArea area : {DockArea::Center, DockArea::Top,
                                    DockArea::Right, DockArea::Bottom,
                                    DockArea::Left}) {
          if (containerTargetMode_ && area == DockArea::Center) {
            continue;
          }
          if (!(allowedAreas_ & dockAreaDropTargetBit(area))) {
            continue;
          }
          const QRect target = targetRect(area);
          QColor fill = palette().color(QPalette::Window);
          fill.setAlpha(238);
          if (area == selectedButton_) {
            fill = accent;
            fill.setAlpha(224);
          }
          painter.setPen(QPen(accent, area == selectedButton_ ? 2.0 : 1.0));
          painter.setBrush(fill);
          painter.drawRoundedRect(target, 5.0, 5.0);
          QFont glyphFont = painter.font();
          glyphFont.setPixelSize(std::max(14, target.height() * 3 / 5));
          painter.setFont(glyphFont);
          painter.setPen(palette().color(
              area == selectedButton_ ? QPalette::HighlightedText
                                      : QPalette::WindowText));
          painter.drawText(target, Qt::AlignCenter, targetGlyph(area));
        }
        return;
      }

      QColor fill = accent;
      fill.setAlpha(52);
      painter.fillRect(rect(), fill);
      QPen pen(accent, 2.0);
      painter.setPen(pen);
      painter.drawRect(rect().adjusted(1, 1, -2, -2));
    }

  private:
    QRect targetRect(DockArea area) const {
      return targetRect(area, targetCenter_, containerTargetMode_);
    }

    QRect targetRect(DockArea area, const QPoint &targetCenter,
                     bool containerTarget) const {
      const int extent = std::min(
          48, std::max(24, std::min(width(), height()) / 8));
      const int gap = std::max(5, extent / 6);
      if (containerTarget) {
        const int inset = extent / 2 + gap + 2;
        switch (area) {
        case DockArea::Top:
          return QRect(targetCenter.x() - extent / 2, inset - extent / 2,
                       extent, extent);
        case DockArea::Right:
          return QRect(width() - inset - extent / 2,
                       targetCenter.y() - extent / 2, extent, extent);
        case DockArea::Bottom:
          return QRect(targetCenter.x() - extent / 2,
                       height() - inset - extent / 2, extent, extent);
        case DockArea::Left:
          return QRect(inset - extent / 2,
                       targetCenter.y() - extent / 2, extent, extent);
        case DockArea::Center:
          return QRect(targetCenter.x() - extent / 2,
                       targetCenter.y() - extent / 2, extent, extent);
        }
        return {};
      }
      const QPoint center = targetCenter;
      switch (area) {
      case DockArea::Top:
        return QRect(center.x() - extent / 2,
                     center.y() - extent - gap, extent, extent);
      case DockArea::Right:
        return QRect(center.x() + gap, center.y() - extent / 2,
                     extent, extent);
      case DockArea::Bottom:
        return QRect(center.x() - extent / 2, center.y() + gap,
                     extent, extent);
      case DockArea::Left:
        return QRect(center.x() - extent - gap,
                     center.y() - extent / 2, extent, extent);
      case DockArea::Center:
        return QRect(center.x() - extent / 2,
                     center.y() - extent / 2, extent, extent);
      }
      return {};
    }

    static QString targetGlyph(DockArea area) {
      switch (area) {
      case DockArea::Top: return QStringLiteral("↑");
      case DockArea::Right: return QStringLiteral("→");
      case DockArea::Bottom: return QStringLiteral("↓");
      case DockArea::Left: return QStringLiteral("←");
      case DockArea::Center: return QStringLiteral("▣");
      }
      return {};
    }

    bool areaTargetMode_ = false;
    bool containerTargetMode_ = false;
    bool autoHideInsertionMode_ = false;
    int allowedAreas_ = kAllDockAreaDropTargets;
    DockArea centerTargetArea_ = DockArea::Center;
    DockArea selectedButton_ = DockArea::Center;
    QRect previewRect_;
    QRect insertionMarkerRect_;
    QPoint targetCenter_;
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
    rootLayout->setSpacing(0);

    autoHideTopBar_ = new QTabBar(this);
    configureAutoHideBar(autoHideTopBar_, DockArea::Top);
    rootLayout->addWidget(autoHideTopBar_);

    auto *dockBody = new QWidget(this);
    auto *dockBodyLayout = new QHBoxLayout(dockBody);
    dockBodyLayout->setContentsMargins(0, 0, 0, 0);
    dockBodyLayout->setSpacing(0);
    autoHideLeftBar_ = new QTabBar(dockBody);
    configureAutoHideBar(autoHideLeftBar_, DockArea::Left);
    dockBodyLayout->addWidget(autoHideLeftBar_);

    containerRootHost_ = new QWidget(dockBody);
    containerRootLayout_ = new QVBoxLayout(containerRootHost_);
    containerRootLayout_->setContentsMargins(0, 0, 0, 0);
    containerRootLayout_->setSpacing(0);
    verticalSplitter_ = createConfiguredDockSplitter(
        Qt::Vertical, containerRootHost_);
    containerRootWidget_ = verticalSplitter_;
    containerGroupRoots_.insert(QStringLiteral("base"), verticalSplitter_);
    topTabs_ = createTabSurface(verticalSplitter_);
    topTabs_->hide();
    verticalSplitter_->addWidget(topTabs_);

    auto *splitter = createConfiguredDockSplitter(
        Qt::Horizontal, verticalSplitter_);
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

    containerRootLayout_->addWidget(verticalSplitter_);
    dockBodyLayout->addWidget(containerRootHost_, 1);
    autoHideRightBar_ = new QTabBar(dockBody);
    configureAutoHideBar(autoHideRightBar_, DockArea::Right);
    dockBodyLayout->addWidget(autoHideRightBar_);
    rootLayout->addWidget(dockBody, 1);

    autoHideBottomBar_ = new QTabBar(this);
    configureAutoHideBar(autoHideBottomBar_, DockArea::Bottom);
    rootLayout->addWidget(autoHideBottomBar_);

    autoHideFlyout_ = new QFrame(this);
    autoHideFlyout_->setFrameShape(QFrame::StyledPanel);
    autoHideFlyout_->setAutoFillBackground(true);
    autoHideFlyout_->hide();
    autoHideFlyoutLayout_ = new QVBoxLayout(autoHideFlyout_);
    autoHideFlyoutLayout_->setContentsMargins(0, 0, 0, 0);
    autoHideFlyoutLayout_->setSpacing(0);
    autoHideFlyoutHeader_ = new QWidget(autoHideFlyout_);
    autoHideFlyoutHeaderLayout_ = new QHBoxLayout(autoHideFlyoutHeader_);
    autoHideFlyoutHeaderLayout_->setContentsMargins(6, 3, 3, 3);
    autoHideFlyoutTitle_ = new QLabel(autoHideFlyoutHeader_);
    autoHideFlyoutTitle_->setSizePolicy(QSizePolicy::Expanding,
                                        QSizePolicy::Preferred);
    autoHideFlyoutHeaderLayout_->addWidget(autoHideFlyoutTitle_);
    autoHideDockBackButton_ = new QToolButton(autoHideFlyoutHeader_);
    autoHideDockBackButton_->setAutoRaise(true);
    autoHideDockBackButton_->setIcon(autoHideDockBackButton_->style()->standardIcon(
        QStyle::SP_ArrowBack));
    autoHideDockBackButton_->setToolTip(tr("Dock panel back"));
    autoHideDockBackButton_->setAccessibleName(tr("Dock panel back"));
    autoHideDockBackButton_->setProperty("artifactAutoHideDockBack", true);
    autoHideDockBackButton_->installEventFilter(this);
    autoHideFlyoutHeaderLayout_->addWidget(autoHideDockBackButton_);
    autoHideFlyoutCloseButton_ = new QToolButton(autoHideFlyoutHeader_);
    autoHideFlyoutCloseButton_->setAutoRaise(true);
    autoHideFlyoutCloseButton_->setIcon(autoHideFlyoutCloseButton_->style()->standardIcon(
        QStyle::SP_TitleBarCloseButton));
    autoHideFlyoutCloseButton_->setToolTip(tr("Close panel"));
    autoHideFlyoutCloseButton_->setAccessibleName(tr("Close panel"));
    autoHideFlyoutCloseButton_->setProperty("artifactAutoHideClose", true);
    autoHideFlyoutCloseButton_->installEventFilter(this);
    autoHideFlyoutHeaderLayout_->addWidget(autoHideFlyoutCloseButton_);
    autoHideFlyoutLayout_->addWidget(autoHideFlyoutHeader_);

    registerDockTabSurface(leftTabs_, DockArea::Left);
    registerDockTabSurface(rightTabs_, DockArea::Right);
    registerDockTabSurface(topTabs_, DockArea::Top);
    registerDockTabSurface(bottomTabs_, DockArea::Bottom);
    registerDockTabSurface(centerTabs_, DockArea::Center);
  }

  ~NativeDockSurface() override {
    autoHideHoverTimer_.stop();
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
    floatingDropPreviews_.clear();
  }

  DockSplitter *createConfiguredDockSplitter(Qt::Orientation orientation,
                                             QWidget *parent) {
    auto *splitter = new DockSplitter(orientation, parent);
    splitter->setOpaqueResize(opaqueSplitterResize_);
    return splitter;
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
    if (!widget->property("artifactDockClosable").isValid()) {
      widget->setProperty("artifactDockClosable", true);
    }
    if (!widget->property("artifactDockMovable").isValid()) {
      widget->setProperty("artifactDockMovable", true);
    }
    if (!widget->property("artifactDockFloatable").isValid()) {
      widget->setProperty("artifactDockFloatable", true);
    }
    if (!widget->property("artifactDockAutoHideable").isValid()) {
      widget->setProperty("artifactDockAutoHideable", true);
    }
    if (!widget->property("artifactDockOpen").isValid()) {
      widget->setProperty("artifactDockOpen", true);
    }
    widget->setParent(tabs);
    installCloseButton(tabs, tabs->addTab(widget, title), dockId);
    tabs->show();
    docks_.insert(dockId, widget);
    areas_.insert(dockId, area);
    titles_.insert(dockId, title);
    pinned_.insert(dockId, false);
    syncPinButton(dockId, false);
    if (!activeTabs_) {
      setActiveTabSurface(tabs);
    }
    return true;
  }

  // Add a dock to the same tab surface as an existing dock.  The native MVP
  // uses the dock ID as the stable routing key; callers do not need to know
  // which QTabWidget currently owns either panel.
  bool addDockWidgetToTab(const QString &dockId, const QString &title,
                          QWidget *widget, const QString &targetDockId,
                          int insertionIndex = -1) {
    if (dockId.trimmed().isEmpty() || !widget || docks_.contains(dockId) ||
        targetDockId.trimmed().isEmpty()) {
      return false;
    }
    auto *target = docks_.value(targetDockId, nullptr);
    auto *tabs = target ? tabsForWidget(target) : nullptr;
    if (!tabs) {
      return false;
    }
    if (!widget->property("artifactDockClosable").isValid()) {
      widget->setProperty("artifactDockClosable", true);
    }
    if (!widget->property("artifactDockMovable").isValid()) {
      widget->setProperty("artifactDockMovable", true);
    }
    if (!widget->property("artifactDockFloatable").isValid()) {
      widget->setProperty("artifactDockFloatable", true);
    }
    if (!widget->property("artifactDockAutoHideable").isValid()) {
      widget->setProperty("artifactDockAutoHideable", true);
    }
    if (!widget->property("artifactDockOpen").isValid()) {
      widget->setProperty("artifactDockOpen", true);
    }
    widget->setParent(tabs);
    insertionIndex = insertionIndex < 0
        ? tabs->count()
        : std::clamp(insertionIndex, 0, tabs->count());
    installCloseButton(
        tabs, tabs->insertTab(insertionIndex, widget, title), dockId);
    tabs->setCurrentWidget(widget);
    tabs->show();
    docks_.insert(dockId, widget);
    areas_.insert(dockId, areas_.value(targetDockId, DockArea::Center));
    titles_.insert(dockId, title);
    pinned_.insert(dockId, false);
    syncPinButton(dockId, false);
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
    if (!widget->property("artifactDockClosable").isValid()) {
      widget->setProperty("artifactDockClosable", true);
    }
    if (!widget->property("artifactDockMovable").isValid()) {
      widget->setProperty("artifactDockMovable", true);
    }
    if (!widget->property("artifactDockFloatable").isValid()) {
      widget->setProperty("artifactDockFloatable", true);
    }
    if (!widget->property("artifactDockAutoHideable").isValid()) {
      widget->setProperty("artifactDockAutoHideable", true);
    }
    if (!widget->property("artifactDockOpen").isValid()) {
      widget->setProperty("artifactDockOpen", visible);
    }
    widget->setParent(tabs);
    const int tabIndex = tabs->addTab(widget, title);
    if (geometry.isValid()) {
      dialog->setGeometry(geometry);
    }
    floatingDialogs_.insert(dockId, dialog);
    floatingWidgets_.insert(dockId, widget);
    titles_.insert(dockId, title);
    areas_.insert(dockId, DockArea::Center);
    pinned_.insert(dockId, false);
    installCloseButton(tabs, tabIndex, dockId);
    refreshFloatingDialogTitle(tabs);
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
    if (!widget || floatingDialogs_.contains(resolvedId) ||
        !canFloatDock(resolvedId)) return false;
    const QByteArray beforeState = beginLayoutMutation();
    const QString title = titles_.value(resolvedId, resolvedId);
    const bool visible = dockOpen(resolvedId);
    auto *sourceTabs = tabsForWidget(widget);
    const DockArea sourceArea = autoHideAreas_.contains(resolvedId)
        ? areas_.value(resolvedId, DockArea::Center)
        : sourceTabs ? areaForTabs(sourceTabs)
                     : areas_.value(resolvedId, DockArea::Center);
    if (autoHideAreas_.contains(resolvedId)) {
      removeAutoHidePlacement(resolvedId, widget);
    } else if (sourceTabs) {
      const int index = sourceTabs->indexOf(widget);
      if (index >= 0) {
        sourceTabs->removeTab(index);
        widget->setParent(nullptr);
      }
      if (sourceTabs->count() == 0) {
        sourceTabs->hide();
        collapseEmptyDockSurface(sourceTabs, sourceArea);
      }
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
    const int tabIndex = floatingTabs->addTab(widget, title);
    const QRect fallback(dialog->mapToGlobal(QPoint(0, 0)), QSize(560, 400));
    dialog->setGeometry(geometry.isValid() ? geometry : fallback);
    floatingDialogs_.insert(resolvedId, dialog);
    floatingWidgets_.insert(resolvedId, widget);
    installCloseButton(floatingTabs, tabIndex, resolvedId);
    refreshFloatingDialogTitle(floatingTabs);
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
    for (const QString &id : dockIds) {
      if (!canFloatDock(id)) return false;
    }

    const QByteArray beforeState = beginLayoutMutation();
    const DockArea sourceArea = areaForTabs(sourceTabs);
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
        widget->setParent(nullptr);
      }
      widget->setParent(floatingTabs);
      const int tabIndex = floatingTabs->addTab(widget, titles_.value(id, id));
      floatingDialogs_.insert(id, dialog);
      floatingWidgets_.insert(id, widget);
      installCloseButton(floatingTabs, tabIndex, id);
      floatingTabs->setTabVisible(tabIndex,
                                  dockOpen(id) && !dockNoTab(id));
    }
    refreshFloatingDialogTitle(floatingTabs);
    if (sourceTabs->count() == 0) {
      sourceTabs->hide();
      collapseEmptyDockSurface(sourceTabs, sourceArea);
    }
    const QRect fallback(dialog->mapToGlobal(QPoint(0, 0)), QSize(680, 460));
    dialog->setGeometry(geometry.isValid() ? geometry : fallback);
    if (floatingDialogHasVisibleTabs(dialog)) {
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
    auto *widget = dockWidget(resolvedId);
    auto *floatingTabs = floatingTabSurfaces_.value(dialog, nullptr);
    if (!widget || !floatingTabs ||
        floatingDialogs_.value(resolvedId, nullptr) == dialog ||
        !canFloatDock(resolvedId)) {
      return false;
    }
    auto *sourceDialog = floatingDialogs_.value(resolvedId, nullptr);
    auto *sourceTabs = tabsForWidget(widget);
    if (sourceTabs == floatingTabs) {
      return false;
    }
    const int sourceIndex = sourceTabs ? sourceTabs->indexOf(widget) : -1;
    const bool visible = dockOpen(resolvedId);
    if (sourceTabs) {
      const DockArea sourceArea = areaForTabs(sourceTabs);
      const int index = sourceTabs->indexOf(widget);
      if (index >= 0) {
        sourceTabs->removeTab(index);
        widget->setParent(nullptr);
      }
      if (sourceTabs->count() == 0) {
        if (sourceDialog) {
          collapseEmptyFloatingDockSurface(sourceTabs, sourceDialog);
        } else {
          sourceTabs->hide();
          collapseEmptyDockSurface(sourceTabs, sourceArea);
        }
      }
    }
    if (sourceDialog) {
      floatingDialogs_.remove(resolvedId);
      floatingWidgets_.remove(resolvedId);
      destroyFloatingDialogIfEmpty(sourceDialog);
    } else {
      docks_.remove(resolvedId);
    }
    widget->setParent(floatingTabs);
    const int targetIndex = floatingTabs->addTab(
        widget, titles_.value(resolvedId, resolvedId));
    floatingTabs->setTabVisible(targetIndex,
                                visible && !dockNoTab(resolvedId));
    floatingDialogs_.insert(resolvedId, dialog);
    floatingWidgets_.insert(resolvedId, widget);
    installCloseButton(floatingTabs, targetIndex, resolvedId);
    ensureFloatingGroupId(dialog);
    floatingTabs->show();
    setActiveTabSurface(floatingTabs);
    return true;
  }

  bool dockFloatingWidget(const QString &dockId, DockArea area = DockArea::Center) {
    const QString resolvedId = resolveDockId(dockId);
    auto *dialog = floatingDialogs_.value(resolvedId, nullptr);
    auto *widget = floatingWidgets_.value(resolvedId, nullptr);
    if (!dialog || !widget || !canMoveDock(resolvedId)) return false;
    const QByteArray beforeState = beginLayoutMutation();
    const QString title = titles_.value(resolvedId, resolvedId);
    const bool visible = dockOpen(resolvedId);
    const bool pinned = pinned_.value(resolvedId, false);
    auto *floatingTabs = tabsForWidget(widget);
    if (floatingTabs) {
      const int index = floatingTabs->indexOf(widget);
      if (index >= 0) {
        floatingTabs->removeTab(index);
        widget->setParent(nullptr);
      }
      if (floatingTabs->count() == 0) {
        collapseEmptyFloatingDockSurface(floatingTabs, dialog);
      }
    }
    floatingDialogs_.remove(resolvedId);
    floatingWidgets_.remove(resolvedId);
    widget->setParent(nullptr);
    destroyFloatingDialogIfEmpty(dialog);
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
      if (!docks_.contains(entry.dockId) &&
          !floatingDialogs_.contains(entry.dockId)) {
        continue;
      }
      seen.insert(entry.dockId, true);
      normalized.push_back(entry);
    }
    if (normalized.isEmpty()) {
      restoringLayout_ = wasRestoringLayout;
      return false;
    }
    const auto currentLayout = QJsonDocument::fromJson(saveLayoutState());
    if (currentLayout.isObject()) {
      const auto currentDocument =
          DockLayoutDocument::fromJson(currentLayout.object());
      for (const auto &entry : currentDocument.entries) {
        if (seen.contains(entry.dockId) ||
            (!entry.floating && !docks_.contains(entry.dockId)) ||
            (entry.floating && !floatingWidgets_.contains(entry.dockId))) {
          continue;
        }
        seen.insert(entry.dockId, true);
        normalized.push_back(entry);
      }
    }
    normalizeDockSplitPaths(normalized);
    resetDockSplitLayout();
    resetFloatingSplitLayouts();
    const auto savedTabOrder = [](const DockLayoutEntry &entry) -> qsizetype {
      QString ids;
      if (entry.tabGroup.startsWith(QStringLiteral("tabs:"))) {
        ids = entry.tabGroup.mid(5);
      } else if (entry.tabGroup.startsWith(QStringLiteral("floating-tabs:"))) {
        ids = entry.tabGroup.mid(14);
      } else {
        return std::numeric_limits<qsizetype>::max();
      }
      const qsizetype index = ids.split(QLatin1Char('|')).indexOf(entry.dockId);
      return index < 0 ? std::numeric_limits<qsizetype>::max() : index;
    };
    std::sort(normalized.begin(), normalized.end(),
              [&savedTabOrder](const DockLayoutEntry &left,
                               const DockLayoutEntry &right) {
                if (left.floating != right.floating) return !left.floating;
                if (left.area != right.area)
                  return static_cast<int>(left.area) < static_cast<int>(right.area);
                if (left.splitPath != right.splitPath) {
                  return left.splitPath < right.splitPath;
                }
                const int leftOrder = savedTabOrder(left);
                const int rightOrder = savedTabOrder(right);
                if (leftOrder != rightOrder) return leftOrder < rightOrder;
                return left.dockId < right.dockId;
              });

    QHash<QString, QDialog *> restoredFloatingGroups;
    bool restoredAny = false;
    for (const auto &entry : normalized) {
      if (entry.floating) {
        const QString groupKey = floatingGroupRestoreKey(entry);
        QDialog *groupDialog = nullptr;
        if (!groupKey.isEmpty()) {
          groupDialog = restoredFloatingGroups.value(groupKey, nullptr);
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
          if (!groupKey.isEmpty()) {
            if (groupDialog && groupDialog != dialog) {
              if (!floatDockWidgetToGroup(entry.dockId, groupDialog)) {
                continue;
              }
              dialog = floatingDialogs_.value(entry.dockId, nullptr);
              if (!dialog) continue;
            } else if (entry.floatingGroupId.isEmpty()) {
              restoredFloatingGroups.insert(groupKey, dialog);
            } else {
              floatingGroupIds_.insert(dialog, entry.floatingGroupId);
              restoredFloatingGroups.insert(groupKey, dialog);
            }
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
    applyDockSplitLayout(normalized);
    applyFloatingSplitLayouts(normalized);
    for (const auto &entry : normalized) {
      if (!entry.autoHidden || !docks_.contains(entry.dockId)) continue;
      setDockAutoHidden(entry.dockId, entry.autoHideArea);
      setDockVisible(entry.dockId, entry.visible);
    }
    for (const auto &entry : normalized) {
      if (entry.active && entry.visible) {
        activateDock(entry.dockId);
      }
    }
    for (const auto &entry : normalized) {
      setDockLocked(entry.dockId, entry.locked);
      setDockClosable(entry.dockId, entry.closable);
      setDockMovable(entry.dockId, entry.movable);
      setDockFloatable(entry.dockId, entry.floatable);
      setDockFocusable(entry.dockId, entry.focusable);
      setDockNoTab(entry.dockId, entry.noTab);
      setDockAutoHideable(entry.dockId, entry.autoHideable);
      setDockAreaHideSingleWidgetTitleBar(
          entry.dockId, entry.hideSingleWidgetTitleBar);
      setDockAreaAllowedDropAreas(
          entry.dockId, dockAreasFromDropMask(entry.allowedDropAreas));
    }
    restoringLayout_ = wasRestoringLayout;
    return restoredAny;
  }

  QByteArray saveLayoutState() const {
    DockLayoutDocument document;
    document.areaTabsAtBottom = areaTabsAtBottom_;
    document.middleMouseButtonClosesTab = middleMouseButtonClosesTab_;
    document.autoHideButtonTogglesArea = autoHideButtonTogglesArea_;
    const auto verticalRatios = normalizedSplitterRatios(
        verticalSplitter_ ? verticalSplitter_->sizes() : QList<int>{});
    const auto horizontalRatios = normalizedSplitterRatios(
        splitter_ ? splitter_->sizes() : QList<int>{});
    if (verticalRatios.size() == 3 && horizontalRatios.size() == 3) {
      document.rootSplitterRatios = verticalRatios;
      for (const int ratio : horizontalRatios) {
        document.rootSplitterRatios.push_back(ratio);
      }
    }
    QHash<QTabWidget *, QStringList> tabSurfaceIds;
    QHash<QDialog *, QStringList> floatingTabIds;
    QHash<QTabWidget *, QString> activeTabIds;
    const auto appendTabOrder = [this, &tabSurfaceIds, &activeTabIds](
                                    QTabWidget *tabs) {
      if (!tabs) return;
      auto &ids = tabSurfaceIds[tabs];
      for (int i = 0; i < tabs->count(); ++i) {
        const QString id = dockIdForWidget(tabs->widget(i));
        if (!id.isEmpty()) ids.push_back(id);
        if (i == tabs->currentIndex() && !id.isEmpty()) {
          activeTabIds.insert(tabs, id);
        }
      }
    };
    for (auto *tabs : dockedTabSurfaces_) {
      appendTabOrder(tabs);
    }
    for (auto it = floatingTabSurfaces_.cbegin();
         it != floatingTabSurfaces_.cend(); ++it) {
      for (auto *tabs : floatingTabSurfacesForDialog(it.key())) {
        appendTabOrder(tabs);
        auto &ids = floatingTabIds[it.key()];
        ids.append(tabSurfaceIds.value(tabs));
      }
    }
    for (auto it = docks_.cbegin(); it != docks_.cend(); ++it) {
      DockLayoutEntry entry;
      entry.dockId = it.key();
      entry.area = areas_.value(it.key(), DockArea::Center);
      auto *tabs = tabsForWidget(it.value());
      entry.hideSingleWidgetTitleBar = tabs
          ? areaHideSingleWidgetTitleBar_.value(
                tabs, it.value()->property(
                          "artifactDockHideSingleWidgetTitleBar").toBool())
          : it.value()->property(
                "artifactDockHideSingleWidgetTitleBar").toBool();
      entry.autoHidden = autoHideAreas_.contains(it.key());
      if (entry.autoHidden) {
        entry.autoHideArea = autoHideAreas_.value(it.key(), DockArea::Left);
        entry.visible = autoHideVisible_.value(it.key(), false);
        entry.splitPath = autoHideReturnSplitPaths_.value(it.key());
        entry.splitRatios = autoHideReturnSplitRatios_.value(it.key());
        entry.containerSplitPath =
            autoHideReturnContainerPaths_.value(it.key());
        entry.containerSplitRatios =
            autoHideReturnContainerRatios_.value(it.key());
      } else {
        entry.visible = dockOpen(it.key());
      }
      if (tabs && !isFloatingTabSurface(tabs) &&
          !isAutoHideTabSurface(tabs)) {
        splitPathForTabs(tabs, entry.splitPath, entry.splitRatios);
        const QString groupId = dockedContainerGroupIds_.value(
            tabs, QStringLiteral("base"));
        containerSplitPathForGroup(groupId, entry.containerSplitPath,
                                   entry.containerSplitRatios);
      }
      entry.active = entry.autoHidden
          ? autoHideActiveDockId_ == it.key()
          : activeTabIds.value(tabs) == it.key();
      entry.pinned = pinned_.value(it.key(), false);
      entry.closable = dockClosable(it.key());
      entry.movable = dockMovable(it.key());
      entry.floatable = dockFloatable(it.key());
      entry.focusable = dockFocusable(it.key());
      entry.noTab = dockNoTab(it.key());
      entry.autoHideable = dockAutoHideable(it.key());
      entry.locked = dockLocked(it.key());
      entry.allowedDropAreas = dockAreaAllowedDropAreasMask(it.key());
      document.entries.push_back(entry);
    }
    for (auto it = floatingWidgets_.cbegin(); it != floatingWidgets_.cend();
         ++it) {
      DockLayoutEntry entry;
      entry.dockId = it.key();
      entry.area = DockArea::Center;
      entry.visible = dockOpen(it.key());
      entry.pinned = pinned_.value(it.key(), false);
      entry.closable = dockClosable(it.key());
      entry.movable = dockMovable(it.key());
      entry.floatable = dockFloatable(it.key());
      entry.focusable = dockFocusable(it.key());
      entry.noTab = dockNoTab(it.key());
      entry.autoHideable = dockAutoHideable(it.key());
      entry.floating = true;
      entry.locked = dockLocked(it.key());
      entry.allowedDropAreas = dockAreaAllowedDropAreasMask(it.key());
      if (auto *dialog = floatingDialogs_.value(it.key(), nullptr)) {
        entry.floatingGroupId = floatingGroupIds_.value(dialog);
        auto *tabs = tabsForWidget(it.value());
        entry.hideSingleWidgetTitleBar = tabs
            ? areaHideSingleWidgetTitleBar_.value(
                  tabs, it.value()->property(
                            "artifactDockHideSingleWidgetTitleBar").toBool())
            : it.value()->property(
                  "artifactDockHideSingleWidgetTitleBar").toBool();
        const int index = tabs ? tabs->indexOf(it.value()) : -1;
        if (tabs && index >= 0) entry.visible = dockOpen(it.key());
        splitPathForTabs(tabs, entry.splitPath, entry.splitRatios);
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
      if (entry.autoHidden) {
        entry.tabGroup = autoHideReturnTabGroups_.value(entry.dockId);
      } else {
        auto *tabs = tabsForWidget(docks_.value(entry.dockId, nullptr));
        auto ids = tabSurfaceIds.value(tabs);
        ids.removeDuplicates();
        entry.tabGroup = QStringLiteral("tabs:") +
                         ids.join(QStringLiteral("|"));
      }
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
      middleMouseButtonClosesTab_ = document.middleMouseButtonClosesTab;
      setAutoHideButtonTogglesArea(document.autoHideButtonTogglesArea);
      if (restored) {
        applyRootSplitterRatios(document.rootSplitterRatios);
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
      if (auto *floatingTabs = tabsForWidget(widget)) {
        const int index = widget ? floatingTabs->indexOf(widget) : -1;
        if (index >= 0) {
          floatingTabs->removeTab(index);
          widget->setParent(nullptr);
        }
        if (floatingTabs->count() == 0) {
          collapseEmptyFloatingDockSurface(floatingTabs, dialog);
        }
      }
      floatingDialogs_.remove(resolvedId);
      floatingWidgets_.remove(resolvedId);
      areas_.remove(resolvedId);
      titles_.remove(resolvedId);
      pinned_.remove(resolvedId);
      destroyFloatingDialogIfEmpty(dialog);
      return true;
    }
    if (autoHideAreas_.contains(resolvedId)) {
      removeAutoHidePlacement(resolvedId, docks_.value(resolvedId, nullptr));
    }
    auto *widget = docks_.take(resolvedId);
    if (!widget) {
      return false;
    }
    auto *sourceTabs = tabsForWidget(widget);
    const DockArea sourceArea = sourceTabs
        ? areaForTabs(sourceTabs)
        : areas_.value(resolvedId, DockArea::Center);
    if (sourceTabs) {
      const int index = sourceTabs->indexOf(widget);
      if (index >= 0) {
        sourceTabs->removeTab(index);
      }
      refreshDockTabGroupCloseButton(sourceTabs);
    }
    widget->setParent(nullptr);
    areas_.remove(resolvedId);
    titles_.remove(resolvedId);
    pinned_.remove(resolvedId);
    if (sourceTabs && sourceTabs->count() == 0) {
      sourceTabs->hide();
      collapseEmptyDockSurface(sourceTabs, sourceArea);
    }
    return true;
  }

  bool moveDockWidget(const QString &dockId, DockArea area) {
    const QString resolvedId = resolveDockId(dockId);
    if (!canMoveDock(resolvedId)) return false;
    if (floatingDialogs_.contains(resolvedId)) {
      return dockFloatingWidget(resolvedId, area);
    }
    const QByteArray beforeState = beginLayoutMutation();
    auto *widget = docks_.value(resolvedId);
    if (!widget || (areas_.value(resolvedId, DockArea::Center) == area &&
                    !autoHideAreas_.contains(resolvedId))) {
      return widget != nullptr;
    }
    const QString title = titles_.value(resolvedId, resolvedId);
    const bool visible = dockOpen(resolvedId);
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
    const QString resolvedId = resolveDockId(dockId);
    const QString resolvedTargetId = resolveDockId(targetDockId);
    if (!canMoveDock(resolvedId)) return false;
    const QByteArray beforeState = beginLayoutMutation();
    auto *widget = docks_.value(resolvedId, nullptr);
    auto *target = docks_.value(resolvedTargetId, nullptr);
    auto *targetTabs = target ? tabsForWidget(target) : nullptr;
    if (!widget || !target || !targetTabs ||
        resolvedId == resolvedTargetId) {
      return false;
    }
    const QString title = titles_.value(resolvedId, resolvedId);
    const bool visible = dockOpen(resolvedId);
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
    const QString resolvedId = resolveDockId(dockId);
    if (!targetTabs ||
        !(isFloatingTabSurface(targetTabs) ? canFloatDock(resolvedId)
                                          : canMoveDock(resolvedId))) {
      return false;
    }
    const QByteArray beforeState = beginLayoutMutation();
    auto *widget = docks_.value(resolvedId, nullptr);
    if (!widget) {
      widget = floatingWidgets_.value(resolvedId, nullptr);
    }
    if (!widget || !targetTabs || tabsForWidget(widget) == targetTabs) {
      return false;
    }
    const QString title = titles_.value(resolvedId, resolvedId);
    auto *sourceTabs = tabsForWidget(widget);
    const int sourceIndex = sourceTabs ? sourceTabs->indexOf(widget) : -1;
    const bool visible = dockOpen(resolvedId);
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
      ensureFloatingGroupId(floatingDialog);
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

  bool splitDockWidget(const QString &dockId, const QString &targetDockId,
                       DockArea side) {
    const QString resolvedId = resolveDockId(dockId);
    if (side == DockArea::Center) {
      return false;
    }
    QString resolvedTargetId = resolveDockId(targetDockId);
    auto *sourceWidget = dockWidget(resolvedId);
    auto *targetWidget = dockWidget(resolvedTargetId);
    if (!sourceWidget || !targetWidget || resolvedId.isEmpty() ||
        resolvedTargetId.isEmpty()) {
      return false;
    }

    auto *targetTabs = tabsForWidget(targetWidget);
    if (!targetTabs) {
      return false;
    }
    if (isFloatingTabSurface(targetTabs)) {
      return splitFloatingDockWidget(resolvedId, resolvedTargetId,
                                     targetTabs, side);
    }
    if (!canMoveDock(resolvedId)) {
      return false;
    }
    if (resolvedId == resolvedTargetId) {
      resolvedTargetId.clear();
      for (int index = 0; index < targetTabs->count(); ++index) {
        auto *candidate = targetTabs->widget(index);
        const QString candidateId = dockIdForWidget(candidate);
        if (!candidateId.isEmpty() && candidateId != resolvedId) {
          resolvedTargetId = candidateId;
          break;
        }
      }
      if (resolvedTargetId.isEmpty()) {
        return false;
      }
      targetWidget = dockWidget(resolvedTargetId);
      targetTabs = tabsForWidget(targetWidget);
      if (!targetTabs) {
        return false;
      }
    }

    const DockArea targetArea = areaForTabs(targetTabs);
    const QString targetContainerGroup = dockedContainerGroupIds_.value(
        targetTabs, QStringLiteral("base"));
    const bool placeFirst = side == DockArea::Left || side == DockArea::Top;
    const bool horizontal = side == DockArea::Left || side == DockArea::Right;
    const QString title = titles_.value(resolvedId, resolvedId);
    const bool visible = dockOpen(resolvedId);
    const bool pinned = pinned_.value(resolvedId, false);
    const DockArea previousArea = areas_.value(resolvedId, DockArea::Center);
    const QByteArray beforeState = beginLayoutMutation();
    if (!removeDockWidget(resolvedId)) {
      return false;
    }

    auto *parentSplitter = qobject_cast<QSplitter *>(targetTabs->parentWidget());
    const int parentIndex = parentSplitter ? parentSplitter->indexOf(targetTabs)
                                          : -1;
    const QList<int> parentSizes = parentSplitter
        ? parentSplitter->sizes()
        : QList<int>{};
    QString targetPath;
    QList<int> targetRatios;
    if (parentIndex < 0 ||
        !splitPathForTabs(targetTabs, targetPath, targetRatios)) {
      if (addDockWidget(resolvedId, title, sourceWidget, previousArea)) {
        pinned_.insert(resolvedId, pinned);
        setDockVisible(resolvedId, visible);
      }
      finishLayoutMutation(beforeState, tr("Split dock area"));
      return false;
    }
    const QString orientation = horizontal ? QStringLiteral("H")
                                           : QStringLiteral("V");
    const QString newPath = targetPath + orientation +
                            (placeFirst ? QStringLiteral("0")
                                        : QStringLiteral("1"));
    const QString retainedPath = targetPath + orientation +
                                 (placeFirst ? QStringLiteral("1")
                                             : QStringLiteral("0"));
    QList<int> childRatios = targetRatios;
    childRatios.push_back(5000);

    auto *localSplitter = createConfiguredDockSplitter(
        horizontal ? Qt::Horizontal : Qt::Vertical,
        nullptr);
    QWidget *replaced = parentSplitter->replaceWidget(parentIndex, localSplitter);
    if (replaced != targetTabs) {
      delete localSplitter;
      if (addDockWidget(resolvedId, title, sourceWidget, previousArea)) {
        pinned_.insert(resolvedId, pinned);
        setDockVisible(resolvedId, visible);
      }
      finishLayoutMutation(beforeState, tr("Split dock area"));
      return false;
    }
    localSplitters_.push_back(localSplitter);
    auto *newTabs = createDockTabSurface(targetArea, newPath, childRatios,
                                         targetContainerGroup);
    dockedSplitPaths_.insert(
        targetTabs, retainedPath);
    dockedSplitRatios_.insert(
        targetTabs, childRatios);
    if (placeFirst) {
      localSplitter->addWidget(newTabs);
      localSplitter->addWidget(targetTabs);
    } else {
      localSplitter->addWidget(targetTabs);
      localSplitter->addWidget(newTabs);
    }
    if (containerGroupRoots_.value(targetContainerGroup) == targetTabs) {
      containerGroupRoots_.insert(targetContainerGroup, localSplitter);
    }
    localSplitter->setStretchFactor(0, 1);
    localSplitter->setStretchFactor(1, 1);
    localSplitter->show();
    if (equalSplitOnInsertion_) {
      if (parentSplitter &&
          parentSplitter->orientation() == localSplitter->orientation()) {
        equalizeSameOrientationSplitters(parentSplitter,
                                         localSplitter->orientation());
      } else {
        if (parentSplitter && parentSizes.size() == parentSplitter->count()) {
          parentSplitter->setSizes(parentSizes);
        }
        equalizeSameOrientationSplitters(localSplitter,
                                         localSplitter->orientation());
      }
    }

    sourceWidget->setParent(newTabs);
    installCloseButton(newTabs, newTabs->addTab(sourceWidget, title), resolvedId);
    newTabs->setCurrentWidget(sourceWidget);
    newTabs->show();
    docks_.insert(resolvedId, sourceWidget);
    areas_.insert(resolvedId, targetArea);
    titles_.insert(resolvedId, title);
    pinned_.insert(resolvedId, pinned);
    syncPinButton(resolvedId, pinned);
    newTabs->setTabVisible(newTabs->indexOf(sourceWidget),
                           visible && !dockNoTab(resolvedId));
    newTabs->setVisible(visible);
    setActiveTabSurface(newTabs);
    refreshDockSplitMetadata(targetArea);
    finishLayoutMutation(beforeState, tr("Split dock area"));
    return true;
  }

  bool splitFloatingDockWidget(const QString &dockId,
                               const QString &targetDockId,
                               QTabWidget *targetTabs, DockArea side) {
    const QString resolvedId = resolveDockId(dockId);
    if (!targetTabs || side == DockArea::Center ||
        !(tabsForWidget(dockWidget(resolvedId)) == targetTabs
              ? canMoveDock(resolvedId)
              : canFloatDock(resolvedId))) {
      return false;
    }
    QString resolvedTargetId = resolveDockId(targetDockId);
    auto *dialog = floatingDialogForTabs(targetTabs);
    auto *sourceWidget = dockWidget(resolvedId);
    if (!dialog || !sourceWidget || resolvedId.isEmpty()) {
      return false;
    }
    if (resolvedId == resolvedTargetId) {
      resolvedTargetId.clear();
      for (int index = 0; index < targetTabs->count(); ++index) {
        const QString candidate = dockIdForWidget(targetTabs->widget(index));
        if (!candidate.isEmpty() && candidate != resolvedId) {
          resolvedTargetId = candidate;
          break;
        }
      }
    }
    auto *targetWidget = dockWidget(resolvedTargetId);
    if (!targetWidget || tabsForWidget(targetWidget) != targetTabs) {
      return false;
    }

    const QString title = titles_.value(resolvedId, resolvedId);
    const bool visible = dockOpen(resolvedId);
    const bool pinned = pinned_.value(resolvedId, false);
    const DockArea previousArea = areas_.value(resolvedId, DockArea::Center);
    const bool placeFirst = side == DockArea::Left || side == DockArea::Top;
    const bool horizontal = side == DockArea::Left || side == DockArea::Right;
    const QByteArray beforeState = beginLayoutMutation();
    if (!removeDockWidget(resolvedId)) {
      return false;
    }

    QString targetPath;
    QList<int> targetRatios;
    auto *parentSplitter = qobject_cast<QSplitter *>(targetTabs->parentWidget());
    const int parentIndex = parentSplitter ? parentSplitter->indexOf(targetTabs)
                                          : -1;
    const QList<int> parentSizes = parentSplitter
        ? parentSplitter->sizes()
        : QList<int>{};
    if (!splitPathForTabs(targetTabs, targetPath, targetRatios)) {
      if (addDockWidget(resolvedId, title, sourceWidget, previousArea)) {
        pinned_.insert(resolvedId, pinned);
        setDockVisible(resolvedId, visible);
      }
      finishLayoutMutation(beforeState, tr("Split floating dock area"));
      return false;
    }
    const QString orientation = horizontal ? QStringLiteral("H")
                                           : QStringLiteral("V");
    const QString newPath = targetPath + orientation +
                            (placeFirst ? QStringLiteral("0")
                                        : QStringLiteral("1"));
    const QString retainedPath = targetPath + orientation +
                                 (placeFirst ? QStringLiteral("1")
                                             : QStringLiteral("0"));
    QList<int> childRatios = targetRatios;
    childRatios.push_back(5000);

    auto *localSplitter = createConfiguredDockSplitter(
        horizontal ? Qt::Horizontal : Qt::Vertical, nullptr);
    if (parentSplitter && parentIndex >= 0) {
      if (parentSplitter->replaceWidget(parentIndex, localSplitter) !=
          targetTabs) {
        delete localSplitter;
        if (addDockWidget(resolvedId, title, sourceWidget, previousArea)) {
          pinned_.insert(resolvedId, pinned);
          setDockVisible(resolvedId, visible);
        }
        finishLayoutMutation(beforeState, tr("Split floating dock area"));
        return false;
      }
    } else {
      auto *layout = qobject_cast<QVBoxLayout *>(dialog->layout());
      const int layoutIndex = layout ? layout->indexOf(targetTabs) : -1;
      if (!layout || layoutIndex < 0 ||
          floatingDockRoots_.value(dialog) != targetTabs) {
        delete localSplitter;
        if (addDockWidget(resolvedId, title, sourceWidget, previousArea)) {
          pinned_.insert(resolvedId, pinned);
          setDockVisible(resolvedId, visible);
        }
        finishLayoutMutation(beforeState, tr("Split floating dock area"));
        return false;
      }
      layout->removeWidget(targetTabs);
      targetTabs->setParent(nullptr);
      localSplitter->setParent(dialog);
      layout->insertWidget(layoutIndex, localSplitter);
      floatingDockRoots_.insert(dialog, localSplitter);
    }

    floatingSplitters_.push_back(localSplitter);
    dockedSplitPaths_.insert(targetTabs, retainedPath);
    dockedSplitRatios_.insert(targetTabs, childRatios);
    auto *newTabs = createFloatingDockTabSurface(dialog, newPath,
                                                 childRatios);
    if (placeFirst) {
      localSplitter->addWidget(newTabs);
      localSplitter->addWidget(targetTabs);
    } else {
      localSplitter->addWidget(targetTabs);
      localSplitter->addWidget(newTabs);
    }
    localSplitter->setStretchFactor(0, 1);
    localSplitter->setStretchFactor(1, 1);
    localSplitter->show();
    if (equalSplitOnInsertion_) {
      if (parentSplitter &&
          parentSplitter->orientation() == localSplitter->orientation()) {
        equalizeSameOrientationSplitters(parentSplitter,
                                         localSplitter->orientation());
      } else {
        if (parentSplitter && parentSizes.size() == parentSplitter->count()) {
          parentSplitter->setSizes(parentSizes);
        }
        equalizeSameOrientationSplitters(localSplitter,
                                         localSplitter->orientation());
      }
    }

    sourceWidget->setParent(newTabs);
    installCloseButton(newTabs, newTabs->addTab(sourceWidget, title), resolvedId);
    newTabs->setCurrentWidget(sourceWidget);
    newTabs->setVisible(visible);
    docks_.remove(resolvedId);
    floatingDialogs_.insert(resolvedId, dialog);
    floatingWidgets_.insert(resolvedId, sourceWidget);
    areas_.insert(resolvedId, previousArea);
    titles_.insert(resolvedId, title);
    pinned_.insert(resolvedId, pinned);
    syncPinButton(resolvedId, pinned);
    setActiveTabSurface(newTabs);
    refreshDockSplitMetadataNode(floatingDockRoots_.value(dialog),
                                 DockArea::Center, {}, {});
    finishLayoutMutation(beforeState, tr("Split floating dock area"));
    return true;
  }

  bool setDockVisible(const QString &dockId, bool visible) {
    const QString resolvedId = resolveDockId(dockId);
    const bool wasVisible = dockOpen(resolvedId);
    if (!visible && wasVisible && !restoringLayout_) {
      recentlyClosedDockId_ = resolvedId;
    }
    if (auto *dialog = floatingDialogs_.value(resolvedId, nullptr)) {
      auto *widget = floatingWidgets_.value(resolvedId, nullptr);
      auto *tabs = tabsForWidget(widget);
      if (widget) widget->setProperty("artifactDockOpen", visible);
      if (tabs && widget) {
        const int index = tabs->indexOf(widget);
        if (index >= 0) {
          tabs->setTabVisible(index, visible && !dockNoTab(resolvedId));
          refreshDockTabBarVisibility(tabs);
          if (visible) {
            tabs->setCurrentIndex(index);
            widget->show();
            refreshFloatingDialogTitle(tabs);
          } else {
            widget->hide();
          }
        }
        dialog->setVisible(floatingDialogHasVisibleTabs(dialog));
      } else {
        dialog->setVisible(visible);
      }
      return true;
    }
    if (autoHideAreas_.contains(resolvedId)) {
      autoHideVisible_.insert(resolvedId, visible);
      if (auto *widget = docks_.value(resolvedId, nullptr)) {
        widget->setProperty("artifactDockOpen", visible);
      }
      if (auto *tabs = autoHidePanelSurfaces_.value(resolvedId, nullptr)) {
        const int index = tabs->indexOf(docks_.value(resolvedId, nullptr));
        if (index >= 0) {
          tabs->setTabVisible(index, visible && !dockNoTab(resolvedId));
        }
      }
      const DockArea area = autoHideAreas_.value(resolvedId);
      if (auto *bar = autoHideBarForArea(area)) {
        for (int index = 0; index < bar->count(); ++index) {
          if (bar->tabData(index).toString() == resolvedId) {
            bar->setTabVisible(index, visible);
            break;
          }
        }
      }
      if (!visible && autoHideActiveDockId_ == resolvedId) {
        closeAutoHideFlyout();
      }
      syncAutoHideBarVisibility(area);
      return true;
    }
    auto *widget = docks_.value(resolvedId);
    if (!widget) {
      return false;
    }
    widget->setProperty("artifactDockOpen", visible);
    if (auto *tabs = tabsForWidget(widget)) {
      const int index = tabs->indexOf(widget);
      if (index >= 0) {
        tabs->setTabVisible(index, visible && !dockNoTab(resolvedId));
        refreshDockTabBarVisibility(tabs);
      }
      if (visible) {
        tabs->setCurrentWidget(widget);
        widget->show();
      } else {
        widget->hide();
      }
      bool anyOpen = false;
      for (int i = 0; i < tabs->count(); ++i) {
        anyOpen |= dockOpen(dockIdForWidget(tabs->widget(i)));
      }
      tabs->setVisible(anyOpen);
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

  bool setDockTitle(const QString &dockId, const QString &title) {
    const QString resolvedId = resolveDockId(dockId);
    auto *widget = dockWidget(resolvedId);
    if (resolvedId.isEmpty() || !widget) return false;
    titles_.insert(resolvedId, title);
    widget->setWindowTitle(title);
    if (auto *tabs = tabsForWidget(widget)) {
      const int index = tabs->indexOf(widget);
      if (index >= 0) refreshDockTabLabels(tabs);
      if (auto *dialog = floatingDialogForTabs(tabs);
          dialog && tabs->currentWidget() == widget) {
        refreshFloatingDialogTitle(tabs);
      }
    }
    if (autoHideAreas_.contains(resolvedId)) {
      if (auto *bar = autoHideBarForArea(autoHideAreas_.value(resolvedId))) {
        for (int index = 0; index < bar->count(); ++index) {
          if (bar->tabData(index).toString() == resolvedId) {
            refreshAutoHideBarTab(bar, index);
            break;
          }
        }
      }
      if (autoHideActiveDockId_ == resolvedId && autoHideFlyoutTitle_) {
        autoHideFlyoutTitle_->setText(title);
      }
    }
    return true;
  }

  QList<QAction *> dockTitleBarActions(const QString &dockId) const {
    if (!dockWidget(dockId)) return {};
    QList<QAction *> actions;
    for (const QPointer<QAction> &action :
         dockTitleBarActions_.value(resolveDockId(dockId))) {
      if (action) actions.push_back(action.data());
    }
    return actions;
  }

  bool setDockTitleBarActions(const QString &dockId,
                             const QList<QAction *> &actions) {
    const QString resolvedId = resolveDockId(dockId);
    auto *widget = dockWidget(resolvedId);
    if (resolvedId.isEmpty() || !widget) return false;
    QList<QPointer<QAction>> guardedActions;
    for (QAction *action : actions) {
      bool alreadyAdded = false;
      for (const QPointer<QAction> &existing : guardedActions) {
        alreadyAdded |= existing.data() == action;
      }
      if (action && !alreadyAdded) {
        guardedActions.push_back(action);
      }
    }
    dockTitleBarActions_.insert(resolvedId, guardedActions);
    if (auto *tabs = tabsForWidget(widget)) {
      refreshDockTabBarVisibility(tabs);
    }
    return true;
  }

  QList<DockArea> dockAreaAllowedDropAreas(const QString &dockId) const {
    if (!dockWidget(dockId)) return {};
    QList<DockArea> areas;
    const int mask = dockAreaAllowedDropAreasMask(dockId);
    for (const DockArea area : {DockArea::Left, DockArea::Right,
                                DockArea::Top, DockArea::Bottom,
                                DockArea::Center}) {
      if (mask & dockAreaDropTargetBit(area)) areas.push_back(area);
    }
    return areas;
  }

  bool setDockAreaAllowedDropAreas(const QString &dockId,
                                  const QList<DockArea> &areas) {
    const QString resolvedId = resolveDockId(dockId);
    auto *widget = dockWidget(resolvedId);
    if (resolvedId.isEmpty() || !widget) return false;
    const bool autoHidden = autoHideAreas_.contains(resolvedId);
    auto *tabs = autoHidden ? nullptr : tabsForWidget(widget);
    if (!autoHidden && !tabs) return false;
    int mask = 0;
    for (const DockArea area : areas) {
      const int value = static_cast<int>(area);
      if (value < static_cast<int>(DockArea::Left) ||
          value > static_cast<int>(DockArea::Center)) {
        return false;
      }
      mask |= dockAreaDropTargetBit(area);
    }
    widget->setProperty("artifactDockAreaAllowedDropAreas", mask);
    if (autoHidden) return true;
    tabs->setProperty("artifactDockAreaAllowedDropAreas", mask);
    for (int index = 0; index < tabs->count(); ++index) {
      if (auto *member = tabs->widget(index)) {
        member->setProperty("artifactDockAreaAllowedDropAreas", mask);
      }
    }
    return true;
  }

  QIcon dockIcon(const QString &dockId) const {
    const auto *widget = dockWidget(dockId);
    return widget ? widget->property("artifactDockIcon").value<QIcon>()
                  : QIcon{};
  }

  bool setDockIcon(const QString &dockId, const QIcon &icon) {
    const QString resolvedId = resolveDockId(dockId);
    auto *widget = dockWidget(resolvedId);
    if (!widget) return false;
    widget->setProperty("artifactDockIcon", QVariant::fromValue(icon));
    if (auto *tabs = tabsForWidget(widget)) {
      const int index = tabs->indexOf(widget);
      if (index >= 0) tabs->setTabIcon(index, icon);
      refreshDockTabLabels(tabs);
      refreshFloatingDialogTitle(tabs);
    }
    if (autoHideAreas_.contains(resolvedId)) {
      if (auto *bar = autoHideBarForArea(autoHideAreas_.value(resolvedId))) {
        for (int index = 0; index < bar->count(); ++index) {
          if (bar->tabData(index).toString() == resolvedId) {
            refreshAutoHideBarTab(bar, index);
            break;
          }
        }
      }
    }
    return true;
  }

  QString dockTabToolTip(const QString &dockId) const {
    const auto *widget = dockWidget(dockId);
    return widget ? widget->toolTip() : QString{};
  }

  bool setDockTabToolTip(const QString &dockId, const QString &toolTip) {
    const QString resolvedId = resolveDockId(dockId);
    auto *widget = dockWidget(resolvedId);
    if (!widget) return false;
    widget->setToolTip(toolTip);
    if (auto *tabs = tabsForWidget(widget)) {
      const int index = tabs->indexOf(widget);
      if (index >= 0) tabs->setTabToolTip(index, toolTip);
    }
    if (autoHideAreas_.contains(resolvedId)) {
      if (auto *bar = autoHideBarForArea(autoHideAreas_.value(resolvedId))) {
        for (int index = 0; index < bar->count(); ++index) {
          if (bar->tabData(index).toString() == resolvedId) {
            refreshAutoHideBarTab(bar, index);
            break;
          }
        }
      }
    }
    return true;
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
    if (autoHideAreas_.contains(resolvedId)) {
      if (!autoHideVisible_.value(resolvedId, false)) {
        setDockVisible(resolvedId, true);
      }
      return openAutoHideDock(resolvedId);
    }
    if (auto *dialog = floatingDialogs_.value(resolvedId, nullptr)) {
      auto *tabs = tabsForWidget(floatingWidgets_.value(resolvedId, nullptr));
      if (auto *widget = floatingWidgets_.value(resolvedId, nullptr)) {
        widget->setProperty("artifactDockOpen", true);
        const int index = tabs ? tabs->indexOf(widget) : -1;
        if (index >= 0) {
          tabs->setTabVisible(index, !dockNoTab(resolvedId));
          tabs->setCurrentIndex(index);
          setActiveTabSurface(tabs);
        }
        widget->show();
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
    widget->setProperty("artifactDockOpen", true);
    tabs->setTabVisible(index, !dockNoTab(resolvedId));
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
    if (autoHideAreas_.contains(resolvedId)) {
      return autoHideVisible_.value(resolvedId, false);
    }
    if (auto *dialog = floatingDialogs_.value(resolvedId, nullptr)) {
      return dockOpen(resolvedId) && dialog->isVisible();
    }
    return dockOpen(resolvedId);
  }

  bool dockOpen(const QString &dockId) const {
    const QString resolvedId = resolveDockId(dockId);
    if (autoHideAreas_.contains(resolvedId)) {
      return autoHideVisible_.value(resolvedId, false);
    }
    const auto *widget = dockWidget(resolvedId);
    if (!widget) return false;
    const QVariant openState = widget->property("artifactDockOpen");
    if (openState.isValid()) return openState.toBool();
    auto *tabs = tabsForWidget(const_cast<QWidget *>(widget));
    const int index = tabs ? tabs->indexOf(const_cast<QWidget *>(widget)) : -1;
    return index >= 0 ? tabs->isTabVisible(index) : widget->isVisible();
  }

  bool panelTabVisible(const QString &dockId) const {
    const QString resolvedId = resolveDockId(dockId);
    if (autoHideAreas_.contains(resolvedId)) {
      return autoHideVisible_.value(resolvedId, false);
    }
    if (auto *dialog = floatingDialogs_.value(resolvedId, nullptr)) {
      auto *widget = floatingWidgets_.value(resolvedId, nullptr);
      auto *tabs = tabsForWidget(widget);
      const int index = tabs && widget ? tabs->indexOf(widget) : -1;
      return index >= 0 && tabs->isTabVisible(index);
    }
    auto *widget = docks_.value(resolvedId, nullptr);
    auto *tabs = widget ? tabsForWidget(widget) : nullptr;
    const int index = tabs && widget ? tabs->indexOf(widget) : -1;
    return index >= 0 ? tabs->isTabVisible(index)
                      : widget && widget->isVisible();
  }

  bool dockPinned(const QString &dockId) const {
    return pinned_.value(resolveDockId(dockId), false);
  }

  bool dockClosable(const QString &dockId) const {
    const auto *widget = dockWidget(dockId);
    return !widget || !widget->property("artifactDockClosable").isValid() ||
           widget->property("artifactDockClosable").toBool();
  }

  bool setDockClosable(const QString &dockId, bool closable) {
    auto *widget = dockWidget(dockId);
    if (!widget) return false;
    widget->setProperty("artifactDockClosable", closable);
    syncPinButton(dockId, dockPinned(dockId));
    return true;
  }

  bool dockMovable(const QString &dockId) const {
    const auto *widget = dockWidget(dockId);
    return !widget || !widget->property("artifactDockMovable").isValid() ||
           widget->property("artifactDockMovable").toBool();
  }

  bool setDockMovable(const QString &dockId, bool movable) {
    const QString resolvedId = resolveDockId(dockId);
    auto *widget = dockWidget(dockId);
    if (!widget) return false;
    widget->setProperty("artifactDockMovable", movable);
    refreshDockTabGroupCloseButton(tabsForWidget(widget));
    if (auto *dialog = floatingDialogs_.value(resolvedId, nullptr)) {
      syncFloatingDockBackButton(dialog);
    }
    if (autoHideActiveDockId_ == resolvedId && autoHideDockBackButton_) {
      autoHideDockBackButton_->setEnabled(canMoveDock(resolvedId));
    }
    return true;
  }

  bool dockFloatable(const QString &dockId) const {
    const auto *widget = dockWidget(dockId);
    return !widget || !widget->property("artifactDockFloatable").isValid() ||
           widget->property("artifactDockFloatable").toBool();
  }

  bool setDockFloatable(const QString &dockId, bool floatable) {
    auto *widget = dockWidget(dockId);
    if (!widget) return false;
    widget->setProperty("artifactDockFloatable", floatable);
    refreshDockTabGroupCloseButton(tabsForWidget(widget));
    return true;
  }

  bool dockFocusable(const QString &dockId) const {
    const auto *widget = dockWidget(dockId);
    return !widget || !widget->property("artifactDockFocusable").isValid() ||
           widget->property("artifactDockFocusable").toBool();
  }

  bool setDockFocusable(const QString &dockId, bool focusable) {
    auto *widget = dockWidget(dockId);
    if (!widget) return false;
    widget->setProperty("artifactDockFocusable", focusable);
    if (auto *tabs = tabsForWidget(widget)) {
      static_cast<DockTabSurface *>(tabs)->refreshFocusChrome();
    }
    return true;
  }

  bool dockNoTab(const QString &dockId) const {
    const auto *widget = dockWidget(dockId);
    return widget && widget->property("artifactDockNoTab").toBool();
  }

  bool setDockNoTab(const QString &dockId, bool noTab) {
    const QString resolvedId = resolveDockId(dockId);
    auto *widget = dockWidget(resolvedId);
    if (!widget) return false;
    widget->setProperty("artifactDockNoTab", noTab);
    if (auto *tabs = tabsForWidget(widget)) {
      const int index = tabs->indexOf(widget);
      if (index >= 0) {
        tabs->setTabVisible(index, dockOpen(resolvedId) && !noTab);
        refreshDockTabBarVisibility(tabs);
      }
    }
    return true;
  }

  bool dockAutoHideable(const QString &dockId) const {
    const auto *widget = dockWidget(dockId);
    return !widget || !widget->property("artifactDockAutoHideable").isValid() ||
           widget->property("artifactDockAutoHideable").toBool();
  }

  bool setDockAutoHideable(const QString &dockId, bool autoHideable) {
    auto *widget = dockWidget(dockId);
    if (!widget) return false;
    widget->setProperty("artifactDockAutoHideable", autoHideable);
    refreshDockTabGroupCloseButton(tabsForWidget(widget));
    return true;
  }

  bool dockLocked(const QString &dockId) const {
    const auto *widget = dockWidget(dockId);
    return widget && widget->property("artifactDockLocked").toBool();
  }

  bool setDockLocked(const QString &dockId, bool locked) {
    const QString resolvedId = resolveDockId(dockId);
    auto *widget = dockWidget(dockId);
    if (!widget) return false;
    widget->setProperty("artifactDockLocked", locked);
    refreshDockTabGroupCloseButton(tabsForWidget(widget));
    if (auto *dialog = floatingDialogs_.value(resolvedId, nullptr)) {
      syncFloatingDockBackButton(dialog);
    }
    if (autoHideActiveDockId_ == resolvedId && autoHideDockBackButton_) {
      autoHideDockBackButton_->setEnabled(canMoveDock(resolvedId));
    }
    return true;
  }

  bool canMoveDock(const QString &dockId) const {
    return restoringLayout_ ||
           (!dockLocked(dockId) && dockMovable(dockId));
  }

  bool canFloatDock(const QString &dockId) const {
    return restoringLayout_ ||
           (!dockLocked(dockId) && dockFloatable(dockId));
  }

  bool canDragDock(const QString &dockId) const {
    return restoringLayout_ ||
           (!dockLocked(dockId) &&
            (dockMovable(dockId) || dockFloatable(dockId)));
  }

  bool canAutoHideDock(const QString &dockId) const {
    return restoringLayout_ ||
           (canMoveDock(dockId) &&
            (autoHideAreas_.contains(resolveDockId(dockId)) ||
             dockAutoHideable(dockId)));
  }

  DockBackendKind backendKind() const { return DockBackendKind::Native; }

  DockBackendCapabilities capabilities() const {
    return DockBackendCapabilities{true, true, true};
  }

  void setAreaTabPosition(DockArea area, bool atBottom) {
    setAreaTabPositionImpl(area, atBottom);
  }

  bool areaTabPositionAtBottom(DockArea area) const {
    return areaTabsAtBottom_.value(static_cast<int>(area), false);
  }

  void setDockAreaAlwaysShowTabs(bool enabled) {
    if (alwaysShowTabs_ == enabled) return;
    alwaysShowTabs_ = enabled;
    for (auto *tabs : dockedTabSurfaces_) {
      refreshDockTabBarVisibility(tabs);
    }
    for (auto it = floatingTabOwners_.cbegin();
         it != floatingTabOwners_.cend(); ++it) {
      refreshDockTabBarVisibility(it.key());
    }
  }

  bool dockAreaAlwaysShowTabs() const { return alwaysShowTabs_; }

  bool setDockAreaHideSingleWidgetTitleBar(const QString &dockId,
                                          bool enabled) {
    auto *widget = dockWidget(resolveDockId(dockId));
    auto *tabs = widget ? tabsForWidget(widget) : nullptr;
    if (!widget || !tabs) return false;
    areaHideSingleWidgetTitleBar_.insert(tabs, enabled);
    for (int index = 0; index < tabs->count(); ++index) {
      if (auto *member = tabs->widget(index)) {
        member->setProperty("artifactDockHideSingleWidgetTitleBar", enabled);
      }
    }
    refreshDockTabBarVisibility(tabs);
    return true;
  }

  bool dockAreaHideSingleWidgetTitleBar(const QString &dockId) const {
    auto *widget = dockWidget(resolveDockId(dockId));
    auto *tabs = widget ? tabsForWidget(widget) : nullptr;
    if (!widget || !tabs) return false;
    return areaHideSingleWidgetTitleBar_.value(
        tabs, widget->property("artifactDockHideSingleWidgetTitleBar")
                  .toBool());
  }

  void setDockAreaHideSingleCentralWidgetTitleBar(bool enabled) {
    if (hideSingleCentralWidgetTitleBar_ == enabled) return;
    hideSingleCentralWidgetTitleBar_ = enabled;
    refreshAllDockTabBars();
  }

  bool dockAreaHideSingleCentralWidgetTitleBar() const {
    return hideSingleCentralWidgetTitleBar_;
  }

  void setDockEqualSplitOnInsertion(bool enabled) {
    equalSplitOnInsertion_ = enabled;
  }

  bool dockEqualSplitOnInsertion() const { return equalSplitOnInsertion_; }

  void setDockOpaqueSplitterResize(bool enabled) {
    if (opaqueSplitterResize_ == enabled) return;
    opaqueSplitterResize_ = enabled;
    if (verticalSplitter_) verticalSplitter_->setOpaqueResize(enabled);
    if (splitter_) splitter_->setOpaqueResize(enabled);
    for (auto *splitter : containerSplitters_) {
      if (splitter) splitter->setOpaqueResize(enabled);
    }
    for (auto *splitter : localSplitters_) {
      if (splitter) splitter->setOpaqueResize(enabled);
    }
    for (auto *splitter : floatingSplitters_) {
      if (splitter) splitter->setOpaqueResize(enabled);
    }
  }

  bool dockOpaqueSplitterResize() const { return opaqueSplitterResize_; }

  void setDockAreaHasTabsMenuButton(bool enabled) {
    if (dockAreaHasTabsMenuButton_ == enabled) return;
    dockAreaHasTabsMenuButton_ = enabled;
    for (auto *tabs : dockedTabSurfaces_) {
      if (auto *surface = static_cast<DockTabSurface *>(tabs)) {
        surface->setTabListButtonEnabled(enabled);
      }
    }
    for (auto it = floatingTabOwners_.cbegin();
         it != floatingTabOwners_.cend(); ++it) {
      if (auto *surface = static_cast<DockTabSurface *>(it.key())) {
        surface->setTabListButtonEnabled(enabled);
      }
    }
    for (auto it = autoHidePanelSurfaces_.cbegin();
         it != autoHidePanelSurfaces_.cend(); ++it) {
      if (auto *surface = static_cast<DockTabSurface *>(it.value())) {
        surface->setTabListButtonEnabled(enabled);
      }
    }
  }

  bool dockAreaHasTabsMenuButton() const {
    return dockAreaHasTabsMenuButton_;
  }

  void setDockAreaHasCloseButton(bool enabled) {
    if (dockAreaHasCloseButton_ == enabled) return;
    dockAreaHasCloseButton_ = enabled;
    refreshAllDockAreaButtons();
  }

  bool dockAreaHasCloseButton() const { return dockAreaHasCloseButton_; }

  void setDockAreaCloseButtonClosesTab(bool enabled) {
    if (dockAreaCloseButtonClosesTab_ == enabled) return;
    dockAreaCloseButtonClosesTab_ = enabled;
    refreshAllDockAreaButtons();
  }

  bool dockAreaCloseButtonClosesTab() const {
    return dockAreaCloseButtonClosesTab_;
  }

  void setDockAreaHasUndockButton(bool enabled) {
    if (dockAreaHasUndockButton_ == enabled) return;
    dockAreaHasUndockButton_ = enabled;
    refreshAllDockAreaButtons();
  }

  bool dockAreaHasUndockButton() const { return dockAreaHasUndockButton_; }

  void setDockAreaDoubleClickUndocksWidget(bool enabled) {
    doubleClickUndocksWidget_ = enabled;
  }

  bool dockAreaDoubleClickUndocksWidget() const {
    return doubleClickUndocksWidget_;
  }

  bool middleMouseButtonClosesTab() const {
    return middleMouseButtonClosesTab_;
  }

  void setDockAreaDynamicTabsMenuButtonVisibility(bool enabled) {
    if (dynamicTabListButtonVisibility_ == enabled) return;
    dynamicTabListButtonVisibility_ = enabled;
    for (auto *tabs : dockedTabSurfaces_) {
      if (auto *surface = static_cast<DockTabSurface *>(tabs)) {
        surface->setDynamicTabListButtonVisibility(enabled);
      }
    }
    for (auto *tabs : floatingTabSurfaces_) {
      if (auto *surface = static_cast<DockTabSurface *>(tabs)) {
        surface->setDynamicTabListButtonVisibility(enabled);
      }
    }
    for (auto *tabs : autoHidePanelSurfaces_) {
      if (auto *surface = static_cast<DockTabSurface *>(tabs)) {
        surface->setDynamicTabListButtonVisibility(enabled);
      }
    }
  }

  bool dockAreaDynamicTabsMenuButtonVisibility() const {
    return dynamicTabListButtonVisibility_;
  }

  void setDockAreaHideDisabledButtons(bool enabled) {
    if (dockAreaHideDisabledButtons_ == enabled) return;
    dockAreaHideDisabledButtons_ = enabled;
    refreshAllDockAreaButtons();
  }

  bool dockAreaHideDisabledButtons() const {
    return dockAreaHideDisabledButtons_;
  }

  void setFloatingContainerHasWidgetIcon(bool enabled) {
    if (floatingContainerHasWidgetIcon_ == enabled) return;
    floatingContainerHasWidgetIcon_ = enabled;
    for (auto it = floatingTabSurfaces_.cbegin();
         it != floatingTabSurfaces_.cend(); ++it) {
      refreshFloatingDialogTitle(it.value());
    }
  }

  bool floatingContainerHasWidgetIcon() const {
    return floatingContainerHasWidgetIcon_;
  }

  void setDockAreaActiveTabHasCloseButton(bool enabled) {
    if (activeTabHasCloseButton_ == enabled) return;
    activeTabHasCloseButton_ = enabled;
    refreshAllDockTabCloseButtons();
  }

  bool dockAreaActiveTabHasCloseButton() const {
    return activeTabHasCloseButton_;
  }

  void setDockAreaAllTabsHaveCloseButton(bool enabled) {
    if (allTabsHaveCloseButton_ == enabled) return;
    allTabsHaveCloseButton_ = enabled;
    refreshAllDockTabCloseButtons();
  }

  bool dockAreaAllTabsHaveCloseButton() const {
    return allTabsHaveCloseButton_;
  }

  void setDockAreaRetainTabSizeWhenCloseButtonHidden(bool enabled) {
    if (retainTabSizeWhenCloseButtonHidden_ == enabled) return;
    retainTabSizeWhenCloseButtonHidden_ = enabled;
    refreshAllDockTabCloseButtons();
  }

  bool dockAreaRetainTabSizeWhenCloseButtonHidden() const {
    return retainTabSizeWhenCloseButtonHidden_;
  }

  void setDockAreaDisableTabTextEliding(bool enabled) {
    if (disableTabTextEliding_ == enabled) return;
    disableTabTextEliding_ = enabled;
    const Qt::TextElideMode mode =
        enabled ? Qt::ElideNone : Qt::ElideRight;
    for (auto *tabs : dockedTabSurfaces_) {
      if (tabs && tabs->tabBar()) tabs->tabBar()->setElideMode(mode);
    }
    for (auto *tabs : floatingTabSurfaces_) {
      if (tabs && tabs->tabBar()) tabs->tabBar()->setElideMode(mode);
    }
    for (auto *tabs : autoHidePanelSurfaces_) {
      if (tabs && tabs->tabBar()) tabs->tabBar()->setElideMode(mode);
    }
    refreshAllDockTabListButtons();
  }

  bool dockAreaDisableTabTextEliding() const {
    return disableTabTextEliding_;
  }

  void setDockAreaShowTabTextOnlyForActiveTab(bool enabled) {
    if (showTabTextOnlyForActiveTab_ == enabled) return;
    showTabTextOnlyForActiveTab_ = enabled;
    refreshAllDockTabLabels();
  }

  bool dockAreaShowTabTextOnlyForActiveTab() const {
    return showTabTextOnlyForActiveTab_;
  }

  void setMiddleMouseButtonClosesTab(bool enabled) {
    middleMouseButtonClosesTab_ = enabled;
  }

  bool autoHideButtonTogglesArea() const {
    return autoHideButtonTogglesArea_;
  }

  void setAutoHideButtonCheckable(bool enabled) {
    if (autoHideButtonCheckable_ == enabled) return;
    autoHideButtonCheckable_ = enabled;
    for (auto *tabs : dockedTabSurfaces_) {
      auto *surface = tabs ? static_cast<DockTabSurface *>(tabs) : nullptr;
      auto *button = surface ? surface->autoHideButton() : nullptr;
      if (!button) continue;
      button->setCheckable(enabled);
      if (!enabled) button->setChecked(false);
    }
  }

  bool autoHideButtonCheckable() const {
    return autoHideButtonCheckable_;
  }

  void setAutoHideSideBarsIconOnly(bool enabled) {
    if (autoHideSideBarsIconOnly_ == enabled) return;
    autoHideSideBarsIconOnly_ = enabled;
    for (const DockArea area : {DockArea::Left, DockArea::Right,
                                DockArea::Top, DockArea::Bottom}) {
      auto *bar = autoHideBarForArea(area);
      if (!bar) continue;
      for (int index = 0; index < bar->count(); ++index) {
        refreshAutoHideBarTab(bar, index);
      }
    }
  }

  bool autoHideSideBarsIconOnly() const {
    return autoHideSideBarsIconOnly_;
  }

  void setAutoHideShowOnMouseOver(bool enabled) {
    if (autoHideShowOnMouseOver_ == enabled) return;
    autoHideShowOnMouseOver_ = enabled;
    autoHideHoverTimer_.stop();
    autoHideHoverDockId_.clear();
  }

  bool autoHideShowOnMouseOver() const {
    return autoHideShowOnMouseOver_;
  }

  void setAutoHideButtonTogglesArea(bool enabled) {
    if (autoHideButtonTogglesArea_ == enabled) return;
    autoHideButtonTogglesArea_ = enabled;
    for (auto *tabs : dockedTabSurfaces_) {
      updateAutoHideButtonPresentation(tabs);
      refreshDockTabGroupCloseButton(tabs);
    }
  }

protected:
  void dragEnterEvent(QDragEnterEvent *event) override {
    if (event->mimeData()->hasFormat(
            QStringLiteral("application/x-artifact-dock-id"))) {
      const QString sourceId = QString::fromUtf8(
          event->mimeData()->data(
              QStringLiteral("application/x-artifact-dock-id")));
      if (dockDragWholeGroup_) {
        const QPoint position = event->position().toPoint();
        auto *targetTabs = tabSurfaceAt(position);
        if (!targetTabs) {
          targetTabs = tabsForArea(areaForDockDropPosition(position));
        }
        showDockTargetOverlay(position, areaForDockDropPosition(position));
        const bool overTabBar = targetTabs &&
            tabBarContainsPosition(targetTabs, position) &&
            !containsActiveDockTargetAt(position);
        const bool containerTarget = dropPreview_ &&
            dropPreview_->showsContainerTargets() &&
            containerDropAreaAt(position) != DockArea::Center;
        const DockArea targetButton = containsActiveDockTargetAt(position)
            ? dropPreview_->targetButtonAt(position)
            : areaForDockDropPosition(position);
        const bool canDrop = overTabBar
            ? canMoveDockTabGroupToTabs(sourceId, targetTabs)
            : (containerTarget
                   ? canDockTabGroupAtContainerEdge(sourceId)
                   : (targetButton == DockArea::Center
                          ? canMoveDockTabGroupToTabs(sourceId, targetTabs)
                          : canMoveDockTabGroupToSplit(
                                sourceId, targetTabs, targetButton)));
        if (!canDrop) {
          dockDragRejected_ = true;
          hideDropPreview();
          event->ignore();
          return;
        }
        dockDragRejected_ = false;
        if (overTabBar) {
          const int insertionIndex = tabInsertionIndex(
                  targetTabs,
                  targetTabs->tabBar()->mapFrom(this, position));
          showTabInsertionPreview(targetTabs, insertionIndex);
        }
        event->acceptProposedAction();
        return;
      }
      if (!canMoveDock(sourceId)) {
        dockDragRejected_ = true;
        hideDropPreview();
        event->ignore();
        return;
      }
      dockDragRejected_ = false;
      const QPoint position = event->position().toPoint();
      showDockTargetOverlay(position, areaForDockDropPosition(position));
      auto *tabs = tabSurfaceAt(position);
      const bool overAreaTarget = containsActiveDockTargetAt(position);
      if (tabs && tabBarContainsPosition(tabs, position) &&
          !overAreaTarget) {
        const QPoint tabPosition = tabs->tabBar()->mapFrom(this, position);
        showTabInsertionPreview(tabs, tabInsertionIndex(tabs, tabPosition));
        event->acceptProposedAction();
        return;
      }
      event->acceptProposedAction();
      return;
    }
    QWidget::dragEnterEvent(event);
  }

  void dragMoveEvent(QDragMoveEvent *event) override {
    if (event->mimeData()->hasFormat(QStringLiteral("application/x-artifact-dock-id"))) {
      const QString sourceId = QString::fromUtf8(
          event->mimeData()->data(
              QStringLiteral("application/x-artifact-dock-id")));
      if (dockDragWholeGroup_) {
        const QPoint position = event->position().toPoint();
        auto *targetTabs = tabSurfaceAt(position);
        if (!targetTabs) {
          targetTabs = tabsForArea(areaForDockDropPosition(position));
        }
        showDockTargetOverlay(position, areaForDockDropPosition(position));
        const bool overTabBar = targetTabs &&
            tabBarContainsPosition(targetTabs, position) &&
            !containsActiveDockTargetAt(position);
        const bool containerTarget = dropPreview_ &&
            dropPreview_->showsContainerTargets() &&
            containerDropAreaAt(position) != DockArea::Center;
        const DockArea targetButton = containsActiveDockTargetAt(position)
            ? dropPreview_->targetButtonAt(position)
            : areaForDockDropPosition(position);
        const bool canDrop = overTabBar
            ? canMoveDockTabGroupToTabs(sourceId, targetTabs)
            : (containerTarget
                   ? canDockTabGroupAtContainerEdge(sourceId)
                   : (targetButton == DockArea::Center
                          ? canMoveDockTabGroupToTabs(sourceId, targetTabs)
                          : canMoveDockTabGroupToSplit(
                                sourceId, targetTabs, targetButton)));
        if (!canDrop) {
          dockDragRejected_ = true;
          hideDropPreview();
          event->ignore();
          return;
        }
        dockDragRejected_ = false;
        if (overTabBar) {
          const int insertionIndex = tabInsertionIndex(
                  targetTabs,
                  targetTabs->tabBar()->mapFrom(this, position));
          showTabInsertionPreview(targetTabs, insertionIndex);
        }
        event->acceptProposedAction();
        return;
      }
      if (!canMoveDock(sourceId)) {
        dockDragRejected_ = true;
        hideDropPreview();
        event->ignore();
        return;
      }
      dockDragRejected_ = false;
      const QPoint position = event->position().toPoint();
      showDockTargetOverlay(position, areaForDockDropPosition(position));
      auto *tabs = tabSurfaceAt(position);
      const bool overAreaTarget = containsActiveDockTargetAt(position);
      if (tabs && tabBarContainsPosition(tabs, position) && !overAreaTarget) {
        const QPoint tabPosition = tabs->tabBar()->mapFrom(this, position);
        showTabInsertionPreview(tabs, tabInsertionIndex(tabs, tabPosition));
      }
      event->acceptProposedAction();
      return;
    }
    QWidget::dragMoveEvent(event);
  }

  void dragLeaveEvent(QDragLeaveEvent *event) override {
    hideDropPreview();
    dockDragRejected_ = false;
    QWidget::dragLeaveEvent(event);
  }

  void dropEvent(QDropEvent *event) override {
    if (event->mimeData()->hasFormat(
            QStringLiteral("application/x-artifact-dock-id"))) {
      dockDragRejected_ = true;
    }
    const QPoint position = event->position().toPoint();
    auto *tabs = tabSurfaceAt(position);
    const bool overAreaTarget = containsActiveDockTargetAt(position);
    const bool overTabBar = tabs && tabBarContainsPosition(tabs, position) &&
                            !overAreaTarget;
    const int insertionIndex = overTabBar
        ? tabInsertionIndex(tabs, tabs->tabBar()->mapFrom(this, position))
        : 0;
    const DockArea targetArea = areaAtDockTargetOverlay(
        position, areaForDockDropPosition(position));
    const bool containerTarget = dropPreview_ &&
        dropPreview_->showsContainerTargets() &&
        containerDropAreaAt(position) != DockArea::Center;
    const DockArea targetButton = overAreaTarget && dropPreview_
        ? dropPreview_->targetButtonAt(position)
        : DockArea::Center;
    const bool disallowedAreaTarget = dropPreview_ &&
        dropPreview_->isDisallowedAreaTargetAt(position);
    hideDropPreview();
    const QByteArray payload = event->mimeData()->data(
        QStringLiteral("application/x-artifact-dock-id"));
    if (!payload.isEmpty()) {
      const QString sourceId = QString::fromUtf8(payload);
      if (disallowedAreaTarget) {
        dockDragRejected_ = true;
        event->ignore();
        return;
      }
      if (dockDragWholeGroup_) {
        if (!tabs) {
          tabs = tabsForArea(targetArea);
        }
        bool moved = false;
        if (containerTarget) {
          moved = dockTabGroupToContainerEdge(sourceId, targetArea);
        } else if (overAreaTarget && targetButton != DockArea::Center) {
          const QString targetDockId = targetDockIdForSplit(tabs, sourceId);
          moved = !targetDockId.isEmpty() &&
              splitDockTabGroup(sourceId, targetDockId, targetButton, tabs);
        } else if (overTabBar || overAreaTarget) {
          moved = moveDockTabGroupToTabs(
              sourceId, tabs, overTabBar ? insertionIndex : 0);
        }
        if (moved) {
          event->acceptProposedAction();
        } else {
          event->ignore();
        }
        return;
      }
      if (overTabBar && placeDockWidgetAtTab(sourceId, tabs,
                                             insertionIndex)) {
        event->acceptProposedAction();
        return;
      }
      if (containerTarget) {
        if (dockToContainerEdge(sourceId, targetArea)) {
          event->acceptProposedAction();
        } else {
          event->ignore();
        }
        return;
      }
      if (overAreaTarget && tabs) {
        bool moved = false;
        if (targetButton == DockArea::Center) {
          auto *sourceWidget = dockWidget(sourceId);
          if (sourceWidget && tabsForWidget(sourceWidget) != tabs) {
            moved = placeDockWidgetAtTab(sourceId, tabs, 0);
          }
        } else {
          const QString targetDockId =
              targetDockIdForSplit(tabs, sourceId);
          if (!targetDockId.isEmpty()) {
            moved = splitDockWidget(sourceId, targetDockId, targetButton);
          } else if (tabs->count() == 0) {
            moved = moveDockWidget(sourceId, targetArea);
          }
        }
        if (moved) event->acceptProposedAction();
        else event->ignore();
        return;
      }
      if (moveDockWidget(sourceId, targetArea)) {
        event->acceptProposedAction();
        return;
      }
    }
    QWidget::dropEvent(event);
  }

  void timerEvent(QTimerEvent *event) override {
    if (!event || event->timerId() != autoHideHoverTimer_.timerId()) {
      QWidget::timerEvent(event);
      return;
    }
    autoHideHoverTimer_.stop();
    const QString dockId = autoHideHoverDockId_;
    const bool shouldOpen = autoHideHoverOpen_;
    autoHideHoverDockId_.clear();
    if (!autoHideShowOnMouseOver_ || dockId.isEmpty()) return;
    if (shouldOpen) {
      const DockArea area = autoHideAreas_.value(dockId, DockArea::Center);
      auto *bar = autoHideBarForArea(area);
      if (!bar || autoHideDockIdAt(
                      bar, bar->mapFromGlobal(QCursor::pos())) != dockId) {
        return;
      }
      openAutoHideDock(dockId);
      return;
    }
    if (autoHideActiveDockId_ != dockId || !autoHideFlyout_ ||
        !autoHideFlyout_->isVisible()) {
      return;
    }
    const QPoint flyoutPosition = autoHideFlyout_->mapFromGlobal(QCursor::pos());
    if (autoHideFlyout_->rect().contains(flyoutPosition)) return;
    closeAutoHideFlyout();
  }

  bool eventFilter(QObject *watched, QEvent *event) override {
    if (!dragSourceId_.isEmpty() && event &&
        event->type() == QEvent::KeyPress &&
        static_cast<QKeyEvent *>(event)->key() == Qt::Key_Escape) {
      // Let QDrag process Escape as usual, but retain the reason so an ignored
      // drag is not mistaken for an intentional detach-to-floating operation.
      dockDragCancelled_ = true;
    }
    if (autoHideShowOnMouseOver_ && event) {
      auto *widget = qobject_cast<QWidget *>(watched);
      const bool withinFlyout = widget && autoHideFlyout_ &&
          (widget == autoHideFlyout_ || autoHideFlyout_->isAncestorOf(widget));
      if (withinFlyout && event->type() == QEvent::Enter &&
          !autoHideHoverOpen_) {
        autoHideHoverTimer_.stop();
        autoHideHoverDockId_.clear();
      } else if (withinFlyout && event->type() == QEvent::Leave &&
                 !autoHideActiveDockId_.isEmpty()) {
        scheduleAutoHideHover(autoHideActiveDockId_, false);
      }
    }
    if (auto *header = qobject_cast<QWidget *>(watched);
        header && header->property("artifactFloatingDockHeaderDrag").toBool()) {
      if (event->type() == QEvent::MouseButtonPress) {
        auto *mouseEvent = static_cast<QMouseEvent *>(event);
        if (mouseEvent->button() == Qt::LeftButton) {
          auto *dialog = qobject_cast<QDialog *>(header->window());
          const auto surfaces = floatingTabSurfacesForDialog(dialog);
          if (surfaces.size() == 1 && surfaces.front() &&
              surfaces.front()->currentWidget()) {
            const QString activeId =
                dockIdForWidget(surfaces.front()->currentWidget());
            if (canDragDockTabGroup(activeId)) {
              dockDragWholeGroup_ = true;
              dockDragBlocked_ = false;
              dragSourceId_ = activeId;
              dragStartPosition_ =
                  mouseEvent->globalPosition().toPoint();
              dockDragWindowOffset_ = dragStartPosition_ -
                  dialog->frameGeometry().topLeft();
              return true;
            }
          }
        }
      } else if (event->type() == QEvent::MouseMove &&
                 dockDragWholeGroup_ && !dragSourceId_.isEmpty()) {
        auto *mouseEvent = static_cast<QMouseEvent *>(event);
        const QPoint globalPosition = mouseEvent->globalPosition().toPoint();
        if (!(mouseEvent->buttons() & Qt::LeftButton) ||
            (globalPosition - dragStartPosition_).manhattanLength() <
                QApplication::startDragDistance()) {
          return true;
        }
        auto *mime = new QMimeData();
        mime->setData(QStringLiteral("application/x-artifact-dock-id"),
                      dragSourceId_.toUtf8());
        auto *drag = new QDrag(header);
        drag->setMimeData(mime);
        dockDragCancelled_ = false;
        dockDragRejected_ = false;
        const Qt::DropAction result = drag->exec(Qt::MoveAction);
        hideDropPreview();
        if (result == Qt::IgnoreAction && !dockDragCancelled_ &&
            !dockDragRejected_) {
          if (auto *sourceTabs = tabsForWidget(dockWidget(dragSourceId_));
              sourceTabs && isFloatingTabSurface(sourceTabs)) {
            if (auto *dialog = floatingDialogForTabs(sourceTabs)) {
              dialog->move(QCursor::pos() - dockDragWindowOffset_);
            }
          }
        }
        dockDragCancelled_ = false;
        dockDragRejected_ = false;
        dockDragWholeGroup_ = false;
        dockDragBlocked_ = false;
        dragSourceId_.clear();
        dockDragWindowOffset_ = {};
        return true;
      } else if (event->type() == QEvent::MouseButtonRelease &&
                 static_cast<QMouseEvent *>(event)->button() ==
                     Qt::LeftButton) {
        dockDragWholeGroup_ = false;
        dockDragBlocked_ = false;
        dragSourceId_.clear();
        dockDragWindowOffset_ = {};
        return true;
      }
    }
    if (event && (event->type() == QEvent::FocusIn ||
                  event->type() == QEvent::MouseButtonPress ||
                  event->type() == QEvent::MouseButtonRelease ||
                  event->type() == QEvent::KeyRelease)) {
      if (auto *widget = qobject_cast<QWidget *>(watched)) {
        if (auto *tabs = tabSurfaceForObject(widget)) {
          if (event->type() == QEvent::FocusIn ||
              event->type() == QEvent::MouseButtonPress) {
            setActiveTabSurface(tabs);
          } else {
            refreshDockTabGroupCloseButton(tabs);
          }
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
      if (button->property("artifactDockFloatGroup").toBool() &&
          button->isDown() && (mouseRelease || keyRelease)) {
        button->setDown(false);
        auto *tabs = tabSurfaceForObject(button);
        if (canFloatDockTabGroup(tabs)) {
          QString dockId = tabs ? dockIdForWidget(tabs->currentWidget())
                                : QString{};
          if (dockId.isEmpty() && tabs) {
            for (int index = 0; index < tabs->count(); ++index) {
              dockId = dockIdForWidget(tabs->widget(index));
              if (!dockId.isEmpty()) break;
            }
          }
          if (!dockId.isEmpty()) floatDockTabGroup(dockId);
        }
        return true;
      }
      if (button->property("artifactDockAutoHideActive").toBool() &&
          button->isDown() && (mouseRelease || keyRelease)) {
        button->setDown(false);
        auto *tabs = tabSurfaceForObject(button);
        const QString dockId =
            tabs ? dockIdForWidget(tabs->currentWidget()) : QString{};
        const DockArea area = dockedTabAreas_.value(tabs, DockArea::Center);
        const bool toggleArea = autoHideButtonTogglesArea_ ||
            QApplication::keyboardModifiers().testFlag(Qt::ControlModifier);
        if (area != DockArea::Center) {
          if (toggleArea) {
            autoHideDockSurface(tabs, area);
          } else if (!dockId.isEmpty() && dockOpen(dockId) &&
                     canAutoHideDock(dockId)) {
            setDockAutoHidden(dockId, area);
          }
        }
        return true;
      }
      if (button->property("artifactDockCloseGroup").toBool() &&
          button->isDown() && (mouseRelease || keyRelease)) {
        button->setDown(false);
        auto *tabs = tabSurfaceForObject(button);
        if (dockAreaCloseButtonClosesTab_) {
          closeActiveDockTab(tabs);
        } else {
          closeDockTabGroup(tabs);
        }
        return true;
      }
      if (button->property("artifactDockTabList").toBool() &&
          button->isDown() && (mouseRelease || keyRelease)) {
        button->setDown(false);
        showTabListMenu(tabSurfaceForObject(button),
                        button->mapToGlobal(button->rect().bottomLeft()));
        return true;
      }
      const QString id = button->property("artifactDockCloseId").toString();
      if (!id.isEmpty()) {
        if (button->isDown() && (mouseRelease || keyRelease)) {
          button->setDown(false);
          if (!dockClosable(id) || dockPinned(id)) return true;
          dragSourceId_.clear();
          const QByteArray beforeState = beginLayoutMutation();
          setDockVisible(id, false);
          finishLayoutMutation(beforeState, tr("Close panel"));
          return true;
        }
      }
      const QString dockBackId = button->property("artifactDockBackId").toString();
      const QString autoHideDockBackId =
          button->property("artifactAutoHideDockBackId").toString();
      if (!autoHideDockBackId.isEmpty() && button->isDown() &&
          (mouseRelease || keyRelease)) {
        button->setDown(false);
        dockAutoHiddenWidget(
            autoHideDockBackId,
            areas_.value(autoHideDockBackId, DockArea::Center));
        return true;
      }
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
    if (auto *autoHideBar = qobject_cast<QTabBar *>(watched);
        autoHideBar &&
        autoHideBar->property("artifactAutoHideSideBar").toBool()) {
      const DockArea side = autoHideAreaForBar(autoHideBar);
      if (autoHideShowOnMouseOver_ &&
          event->type() == QEvent::MouseMove) {
        auto *mouseEvent = static_cast<QMouseEvent *>(event);
        if (!(mouseEvent->buttons() & Qt::LeftButton)) {
          const QString dockId = autoHideDockIdAt(
              autoHideBar, mouseEvent->position().toPoint());
          if (!dockId.isEmpty()) scheduleAutoHideHover(dockId, true);
        }
      }
      if (autoHideShowOnMouseOver_ && event->type() == QEvent::Enter) {
        const QString dockId = autoHideDockIdAt(
            autoHideBar,
            autoHideBar->mapFromGlobal(QCursor::pos()));
        if (!dockId.isEmpty()) scheduleAutoHideHover(dockId, true);
      } else if (autoHideShowOnMouseOver_ &&
                 event->type() == QEvent::Leave) {
        if (!autoHideActiveDockId_.isEmpty()) {
          scheduleAutoHideHover(autoHideActiveDockId_, false);
        } else {
          autoHideHoverTimer_.stop();
          autoHideHoverDockId_.clear();
        }
      } else if (event->type() == QEvent::MouseButtonRelease) {
        auto *mouseEvent = static_cast<QMouseEvent *>(event);
        if (mouseEvent->button() == Qt::LeftButton) {
          const QString id = autoHideDockIdAt(
              autoHideBar, mouseEvent->position().toPoint());
          if (!id.isEmpty()) {
            openAutoHideDock(id, true);
            return true;
          }
        }
      } else if (event->type() == QEvent::MouseButtonPress) {
        autoHideHoverTimer_.stop();
        autoHideHoverDockId_.clear();
        auto *mouseEvent = static_cast<QMouseEvent *>(event);
        if (mouseEvent->button() == Qt::LeftButton) {
          const QString dockId = autoHideDockIdAt(
              autoHideBar, mouseEvent->position().toPoint());
          dragSourceId_ = canDragDock(dockId) ? dockId : QString{};
          dragStartPosition_ = mouseEvent->position().toPoint();
        }
      } else if (event->type() == QEvent::MouseMove &&
                 !dragSourceId_.isEmpty()) {
        auto *mouseEvent = static_cast<QMouseEvent *>(event);
        if (!(mouseEvent->buttons() & Qt::LeftButton) ||
            (mouseEvent->position().toPoint() - dragStartPosition_)
                    .manhattanLength() < QApplication::startDragDistance()) {
          return QWidget::eventFilter(watched, event);
        }
        auto *mime = new QMimeData();
        mime->setData(QStringLiteral("application/x-artifact-dock-id"),
                      dragSourceId_.toUtf8());
        auto *drag = new QDrag(autoHideBar);
        drag->setMimeData(mime);
        dockDragCancelled_ = false;
        dockDragRejected_ = false;
        const QString sourceId = dragSourceId_;
        const Qt::DropAction result = drag->exec(Qt::MoveAction);
        hideDropPreview();
        if (result == Qt::IgnoreAction && !dockDragCancelled_ &&
            !dockDragRejected_ &&
            docks_.contains(sourceId)) {
          const QPoint globalPosition = autoHideBar->mapToGlobal(
              mouseEvent->position().toPoint());
          floatDockWidget(sourceId,
                          QRect(globalPosition, QSize(560, 400)));
        }
        dockDragCancelled_ = false;
        dockDragRejected_ = false;
        dragSourceId_.clear();
        return true;
      } else if (event->type() == QEvent::KeyRelease) {
        const auto *keyEvent = static_cast<QKeyEvent *>(event);
        if ((keyEvent->key() == Qt::Key_Space ||
             keyEvent->key() == Qt::Key_Return ||
             keyEvent->key() == Qt::Key_Enter) &&
            !keyEvent->isAutoRepeat()) {
          const int index = autoHideBar->currentIndex();
          const QString id = index >= 0
              ? autoHideBar->tabData(index).toString()
              : QString{};
          if (!id.isEmpty()) {
            openAutoHideDock(id, true);
            return true;
          }
        }
      } else if (event->type() == QEvent::ContextMenu) {
        const auto *contextEvent = static_cast<QContextMenuEvent *>(event);
        const QString id = autoHideDockIdAt(autoHideBar, contextEvent->pos());
        if (!id.isEmpty()) {
          showDockTabContextMenu(id, contextEvent->globalPos());
          return true;
        }
      } else if (event->type() == QEvent::DragEnter ||
                 event->type() == QEvent::DragMove) {
        auto *dragEvent = static_cast<QDragMoveEvent *>(event);
        if (dragEvent->mimeData()->hasFormat(
                QStringLiteral("application/x-artifact-dock-id"))) {
          const QString sourceId = QString::fromUtf8(
              dragEvent->mimeData()->data(
                  QStringLiteral("application/x-artifact-dock-id")));
          auto *sourceTabs = tabsForWidget(dockWidget(sourceId));
          const bool canAutoHideGroup = dockDragWholeGroup_ && sourceTabs &&
                  sourceTabs->count() > 1
              ? canAutoHideDockTabGroup(sourceTabs)
              : canAutoHideDock(sourceId);
          if (!canAutoHideGroup) {
            dockDragRejected_ = true;
            hideDropPreview();
            dragEvent->ignore();
            return true;
          }
          dockDragRejected_ = false;
          const int insertionIndex = autoHideInsertionIndex(
              autoHideBar, dragEvent->position().toPoint());
          showAutoHideInsertionPreview(autoHideBar, insertionIndex);
          dragEvent->acceptProposedAction();
          return true;
        }
      } else if (event->type() == QEvent::DragLeave) {
        hideDropPreview();
        dockDragRejected_ = false;
      } else if (event->type() == QEvent::Drop) {
        auto *dropEvent = static_cast<QDropEvent *>(event);
        if (dropEvent->mimeData()->hasFormat(
                QStringLiteral("application/x-artifact-dock-id"))) {
          dockDragRejected_ = true;
        }
        const QByteArray payload = dropEvent->mimeData()->data(
            QStringLiteral("application/x-artifact-dock-id"));
        hideDropPreview();
        const QString sourceId = QString::fromUtf8(payload);
        auto *sourceTabs = tabsForWidget(dockWidget(sourceId));
        const int insertionIndex = autoHideInsertionIndex(
            autoHideBar, dropEvent->position().toPoint());
        const bool moved = !sourceId.isEmpty() &&
            (dockDragWholeGroup_ && sourceTabs && sourceTabs->count() > 1
                 ? autoHideDockTabGroup(sourceTabs, side, insertionIndex)
                 : autoHideDockFromDrag(sourceId, side, insertionIndex));
        if (moved) {
          dropEvent->acceptProposedAction();
        } else {
          dropEvent->ignore();
        }
        return true;
      }
    }
    if (auto *dialog = qobject_cast<QDialog *>(watched)) {
      QTabWidget *tabs = nullptr;
      if (event->type() == QEvent::DragEnter ||
          event->type() == QEvent::DragMove) {
        tabs = floatingTabAt(
            dialog, static_cast<QDragMoveEvent *>(event)->position().toPoint());
      } else if (event->type() == QEvent::Drop) {
        tabs = floatingTabAt(
            dialog, static_cast<QDropEvent *>(event)->position().toPoint());
      }
      if (event->type() == QEvent::DragLeave) {
        hideDropPreview();
        dockDragRejected_ = false;
        return QWidget::eventFilter(watched, event);
      }
      if (tabs && (event->type() == QEvent::DragEnter ||
                   event->type() == QEvent::DragMove)) {
        auto *dragEvent = static_cast<QDragMoveEvent *>(event);
        if (dragEvent->mimeData()->hasFormat(
                QStringLiteral("application/x-artifact-dock-id"))) {
          const QString sourceId = QString::fromUtf8(
              dragEvent->mimeData()->data(
                  QStringLiteral("application/x-artifact-dock-id")));
          const QPoint positionInDialog = dragEvent->position().toPoint();
          DockArea containerArea = DockArea::Center;
          const bool containerTarget = floatingContainerTargetWins(
              dialog, positionInDialog, containerArea);
          if (containerTarget && !canDockAtFloatingContainerEdge(
                                     sourceId, dialog, dockDragWholeGroup_)) {
            auto *sourceTabs = tabsForWidget(dockWidget(sourceId));
            dockDragRejected_ = floatingDialogForTabs(sourceTabs) != dialog;
            hideDropPreview();
            dragEvent->ignore();
            return true;
          }
          if (dockDragWholeGroup_) {
            if (!canMoveDockTabGroupToTabs(sourceId, tabs)) {
              dockDragRejected_ =
                  tabsForWidget(dockWidget(sourceId)) != tabs;
              hideDropPreview();
              dragEvent->ignore();
              return true;
            }
            dockDragRejected_ = false;
            const QPoint positionInTabs = tabs->mapFrom(dialog,
                                                         positionInDialog);
            showFloatingDockPreview(tabs, positionInTabs);
            dragEvent->acceptProposedAction();
            return true;
          }
          if (!canDropDockOnTabSurface(sourceId, tabs)) {
            dockDragRejected_ = true;
            hideDropPreview();
            dragEvent->ignore();
            return true;
          }
          dockDragRejected_ = false;
          showFloatingDockPreview(
              tabs, tabs->mapFrom(dialog, positionInDialog));
          dragEvent->acceptProposedAction();
          return true;
        }
      } else if (tabs && event->type() == QEvent::Drop) {
        auto *dropEvent = static_cast<QDropEvent *>(event);
        if (dropEvent->mimeData()->hasFormat(
                QStringLiteral("application/x-artifact-dock-id"))) {
          dockDragRejected_ = true;
        }
        const QByteArray payload = dropEvent->mimeData()->data(
            QStringLiteral("application/x-artifact-dock-id"));
        if (!payload.isEmpty()) {
          const QPoint positionInDialog = dropEvent->position().toPoint();
          if (dockDragWholeGroup_) {
            const QPoint positionInTabs = tabs->mapFrom(dialog,
                                                         positionInDialog);
            const QPoint tabBarPosition =
                tabs->tabBar()->mapFrom(tabs, positionInTabs);
            auto *preview = floatingDropPreviews_.value(dialog, nullptr);
            DockArea containerArea = DockArea::Center;
            const bool containerTarget = floatingContainerTargetWins(
                dialog, positionInDialog, containerArea);
            if (preview &&
                preview->isDisallowedAreaTargetAt(positionInDialog)) {
              hideDropPreview();
              dockDragRejected_ = true;
              dropEvent->ignore();
              return true;
            }
            const bool overAreaTarget = preview &&
                preview->showsAreaTargets() &&
                preview->containsAreaTarget(positionInDialog);
            const bool overTabBar = isTabBarPosition(tabs, tabBarPosition) &&
                                    !overAreaTarget;
            const int insertionIndex = tabInsertionIndex(tabs, tabBarPosition);
            const DockArea targetButton = overAreaTarget
                ? preview->targetButtonAt(positionInDialog)
                : DockArea::Center;
            hideDropPreview();
            const QString sourceId = QString::fromUtf8(payload);
            bool moved = false;
            if (containerTarget) {
              moved = dockAtFloatingContainerEdge(
                  sourceId, dialog, containerArea, true);
            } else if (overAreaTarget && targetButton != DockArea::Center) {
              const QString targetDockId =
                  targetDockIdForSplit(tabs, sourceId);
              moved = !targetDockId.isEmpty() &&
                  splitFloatingDockTabGroup(sourceId, targetDockId, tabs,
                                            targetButton);
            } else if (overTabBar || !overAreaTarget) {
              moved = moveDockTabGroupToTabs(
                  sourceId, tabs, overTabBar ? insertionIndex : 0);
            }
            if (!moved &&
                floatingDialogForTabs(tabsForWidget(dockWidget(sourceId))) ==
                    dialog) {
              dockDragRejected_ = false;
            }
            if (moved) dropEvent->acceptProposedAction();
            else dropEvent->ignore();
            return true;
          }
          const QPoint positionInTabs = tabs->mapFrom(dialog, positionInDialog);
          const QPoint tabBarPosition =
              tabs->tabBar()->mapFrom(tabs, positionInTabs);
          auto *preview = floatingDropPreviews_.value(dialog, nullptr);
          DockArea containerArea = DockArea::Center;
          const bool containerTarget = floatingContainerTargetWins(
              dialog, positionInDialog, containerArea);
          const bool overAreaTarget = preview &&
              preview->showsAreaTargets() &&
              preview->containsAreaTarget(positionInDialog);
          const bool overTabBar = isTabBarPosition(tabs, tabBarPosition) &&
                                  !overAreaTarget;
          const int insertionIndex =
              tabInsertionIndex(tabs, tabBarPosition);
          const DockArea targetButton = overAreaTarget
              ? preview->targetButtonAt(positionInDialog)
              : DockArea::Center;
          hideDropPreview();
          const QString sourceId = QString::fromUtf8(payload);
          bool moved = false;
          if (containerTarget) {
            moved = dockAtFloatingContainerEdge(
                sourceId, dialog, containerArea, false);
          } else if (overAreaTarget && targetButton != DockArea::Center) {
            const QString targetDockId =
                targetDockIdForSplit(tabs, sourceId);
            if (!targetDockId.isEmpty()) {
              moved = splitFloatingDockWidget(
                  sourceId, targetDockId, tabs, targetButton);
            }
          } else if (!overAreaTarget ||
                     tabsForWidget(dockWidget(sourceId)) != tabs) {
            moved = placeDockWidgetAtTab(
                sourceId, tabs,
                overTabBar ? insertionIndex : 0);
          }
          if (moved) {
            dropEvent->acceptProposedAction();
          } else {
            dropEvent->ignore();
          }
          return true;
        }
      } else if (!tabs &&
                 (event->type() == QEvent::DragEnter ||
                  event->type() == QEvent::DragMove)) {
        auto *dragEvent = static_cast<QDragMoveEvent *>(event);
        if (dragEvent->mimeData()->hasFormat(
                QStringLiteral("application/x-artifact-dock-id"))) {
          const QString sourceId = QString::fromUtf8(
              dragEvent->mimeData()->data(
                  QStringLiteral("application/x-artifact-dock-id")));
          DockArea containerArea = DockArea::Center;
          const bool containerTarget = floatingContainerTargetWins(
              dialog, dragEvent->position().toPoint(), containerArea);
          const bool canDock = containerTarget &&
              canDockAtFloatingContainerEdge(
                  sourceId, dialog, dockDragWholeGroup_);
          if (!canDock) {
            auto *sourceTabs = tabsForWidget(dockWidget(sourceId));
            const bool sameSourceDialog =
                floatingDialogForTabs(sourceTabs) == dialog;
            dockDragRejected_ = !sameSourceDialog;
            hideDropPreview();
            dragEvent->ignore();
            return true;
          }
          dockDragRejected_ = false;
          showFloatingContainerOverlay(dialog, containerArea);
          dragEvent->acceptProposedAction();
          return true;
        }
      } else if (!tabs && event->type() == QEvent::Drop) {
        auto *dropEvent = static_cast<QDropEvent *>(event);
        const QByteArray payload = dropEvent->mimeData()->data(
            QStringLiteral("application/x-artifact-dock-id"));
        DockArea containerArea = DockArea::Center;
        const QPoint position = dropEvent->position().toPoint();
        const bool containerTarget = floatingContainerTargetWins(
            dialog, position, containerArea);
        hideDropPreview();
        const QString sourceId = QString::fromUtf8(payload);
        const bool moved = !payload.isEmpty() && containerTarget &&
            dockAtFloatingContainerEdge(
                sourceId, dialog, containerArea, dockDragWholeGroup_);
        if (moved) dropEvent->acceptProposedAction();
        else dropEvent->ignore();
        return true;
      }
    }
    auto *tabs = qobject_cast<QTabWidget *>(watched);
    auto *tabBar = qobject_cast<QTabBar *>(watched);
    auto *areaTitleDragWidget = qobject_cast<QWidget *>(watched);
    if (!areaTitleDragWidget ||
        !areaTitleDragWidget->property("artifactDockAreaTitleDrag").toBool() ||
        !areaTitleDragWidget->isVisible()) {
      areaTitleDragWidget = nullptr;
    }
    if (!tabs && tabBar) {
      tabs = qobject_cast<QTabWidget *>(tabBar->parentWidget());
    }
    if (!tabs && areaTitleDragWidget) {
      tabs = tabSurfaceForObject(areaTitleDragWidget);
    }
    if (!tabs) {
      return QWidget::eventFilter(watched, event);
    }

    if (event->type() == QEvent::MouseButtonDblClick && tabBar &&
        doubleClickUndocksWidget_) {
      auto *mouseEvent = static_cast<QMouseEvent *>(event);
      if (mouseEvent->button() == Qt::LeftButton) {
        const int index = tabBar->tabAt(mouseEvent->position().toPoint());
        const QString dockId = index >= 0
            ? dockIdForWidget(tabs->widget(index))
            : QString{};
        if (!dockId.isEmpty() && canFloatDock(dockId)) {
          const QRect geometry(
              tabBar->mapToGlobal(tabBar->tabRect(index).topLeft()),
              QSize(560, 400));
          dragSourceId_.clear();
          if (floatDockWidget(dockId, geometry)) return true;
        } else if (index < 0 && dockedTabAreas_.contains(tabs) &&
                   canFloatDockTabGroup(tabs)) {
          const QPoint globalPosition =
              tabBar->mapToGlobal(mouseEvent->position().toPoint());
          const QRect geometry(globalPosition - QPoint(40, 12),
                               QSize(560, 400));
          const QString activeDockId =
              dockIdForWidget(tabs->currentWidget());
          if (!activeDockId.isEmpty() &&
              floatDockTabGroup(activeDockId, geometry)) {
            return true;
          }
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
    } else if (event->type() == QEvent::MouseButtonPress &&
               (tabBar || areaTitleDragWidget)) {
      auto *mouseEvent = static_cast<QMouseEvent *>(event);
      if (mouseEvent->button() == Qt::LeftButton) {
        const int index = tabBar
            ? tabBar->tabAt(mouseEvent->position().toPoint())
            : -1;
        const QString dockId = index >= 0
            ? dockIdForWidget(tabs->widget(index))
            : QString{};
        dockDragWholeGroup_ = index < 0 &&
            (dockedTabAreas_.contains(tabs) ||
             floatingTabOwners_.contains(tabs)) &&
            canDragDockTabGroup(dockIdForWidget(tabs->currentWidget()));
        const QString dragId = dockDragWholeGroup_
            ? dockIdForWidget(tabs->currentWidget())
            : dockId;
        dockDragBlocked_ = !dockDragWholeGroup_ && !dockId.isEmpty() &&
            !canMoveDock(dockId);
        dragSourceId_ = dockDragWholeGroup_
            ? dragId
            : (canDragDock(dragId) ? dragId : QString{});
        dockDragWindowOffset_ = {};
        if (dockDragWholeGroup_ && isFloatingTabSurface(tabs)) {
          if (auto *dialog = floatingDialogForTabs(tabs)) {
            dockDragWindowOffset_ =
                mouseEvent->globalPosition().toPoint() -
                dialog->frameGeometry().topLeft();
          }
        }
        dragStartPosition_ = mouseEvent->position().toPoint();
      }
    } else if (event->type() == QEvent::MouseMove &&
               (tabBar || areaTitleDragWidget) &&
               dockDragBlocked_ && dragSourceId_.isEmpty()) {
      return true;
    } else if (event->type() == QEvent::MouseMove &&
               (tabBar || areaTitleDragWidget) &&
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
      QWidget *dragWidget = tabBar ? static_cast<QWidget *>(tabBar)
                                   : areaTitleDragWidget;
      auto *drag = new QDrag(dragWidget);
      drag->setMimeData(mime);
      dockDragCancelled_ = false;
      dockDragRejected_ = false;
      const Qt::DropAction result = drag->exec(Qt::MoveAction);
      hideDropPreview();
      if (result == Qt::IgnoreAction && !dockDragCancelled_ &&
          !dockDragRejected_ && dockWidget(dragSourceId_)) {
        const QPoint globalPosition =
            dragWidget->mapToGlobal(mouseEvent->position().toPoint());
        const QRect geometry(globalPosition, QSize(560, 400));
        if (dockDragWholeGroup_) {
          auto *sourceTabs = tabsForWidget(dockWidget(dragSourceId_));
          if (isFloatingTabSurface(sourceTabs)) {
            if (auto *dialog = floatingDialogForTabs(sourceTabs)) {
              dialog->move(QCursor::pos() - dockDragWindowOffset_);
            }
          } else if (canFloatDockTabGroup(sourceTabs)) {
            floatDockTabGroup(dragSourceId_, geometry);
          }
        } else {
          floatDockWidget(dragSourceId_, geometry);
        }
      }
      dockDragCancelled_ = false;
      dockDragRejected_ = false;
      dockDragWholeGroup_ = false;
      dockDragBlocked_ = false;
      dragSourceId_.clear();
      return true;
    } else if (event->type() == QEvent::MouseButtonRelease &&
               (tabBar || areaTitleDragWidget)) {
      auto *mouseEvent = static_cast<QMouseEvent *>(event);
      if (tabBar && mouseEvent->button() == Qt::MiddleButton &&
          middleMouseButtonClosesTab_) {
        const int index = tabBar->tabAt(mouseEvent->position().toPoint());
        const QString dockId = index >= 0
            ? dockIdForWidget(tabs->widget(index))
            : QString{};
        if (!dockId.isEmpty() && dockClosable(dockId) &&
            !dockPinned(dockId)) {
          const QByteArray beforeState = beginLayoutMutation();
          setDockVisible(dockId, false);
          finishLayoutMutation(beforeState, tr("Close panel"));
          dockDragBlocked_ = false;
          dragSourceId_.clear();
          return true;
        }
      }
      dockDragBlocked_ = false;
      dockDragWholeGroup_ = false;
      dragSourceId_.clear();
    } else if (event->type() == QEvent::DragEnter ||
               event->type() == QEvent::DragMove) {
      auto *dragEvent = static_cast<QDragMoveEvent *>(event);
      if (dragEvent->mimeData()->hasFormat(
              QStringLiteral("application/x-artifact-dock-id"))) {
        const QString sourceId = QString::fromUtf8(
            dragEvent->mimeData()->data(
                QStringLiteral("application/x-artifact-dock-id")));
        if (dockDragWholeGroup_) {
          if (!canMoveDockTabGroupToTabs(sourceId, tabs)) {
            dockDragRejected_ =
                tabsForWidget(dockWidget(sourceId)) != tabs;
            hideDropPreview();
            dragEvent->ignore();
            return true;
          }
          dockDragRejected_ = false;
          const QPoint position = dragEvent->position().toPoint();
          const QPoint tabPosition = tabBar
              ? position
              : tabs->tabBar()->mapFrom(tabs, position);
          if (isFloatingTabSurface(tabs)) {
            auto *dialog = floatingDialogForTabs(tabs);
            const QPoint dialogPosition = tabBar
                ? tabBar->mapTo(dialog, position)
                : tabs->mapTo(dialog, position);
            DockArea containerArea = DockArea::Center;
            if (floatingContainerTargetWins(
                    dialog, dialogPosition, containerArea) &&
                !canDockAtFloatingContainerEdge(
                    sourceId, dialog, true)) {
              auto *sourceTabs = tabsForWidget(dockWidget(sourceId));
              dockDragRejected_ = floatingDialogForTabs(sourceTabs) != dialog;
              hideDropPreview();
              dragEvent->ignore();
              return true;
            }
            const QPoint positionInTabs = tabBar
                ? tabBar->mapTo(tabs, position)
                : position;
            showFloatingDockPreview(tabs, positionInTabs);
          } else {
            const QPoint surfacePosition = tabBar
                ? tabBar->mapTo(this, position)
                : tabs->mapTo(this, position);
            const bool overAreaTarget =
                containsActiveDockTargetAt(surfacePosition);
            const bool overTabBar = tabBar && tabBar->rect().contains(position) &&
                                    !overAreaTarget;
            if (overAreaTarget) {
              showDockTargetOverlay(surfacePosition, areaForTabs(tabs));
            } else if (overTabBar) {
              showTabInsertionPreview(
                  tabs, tabInsertionIndex(tabs, tabPosition));
            } else {
              showDockTargetOverlay(surfacePosition, areaForTabs(tabs));
            }
          }
          dragEvent->acceptProposedAction();
          return true;
        }
        if (!canDropDockOnTabSurface(sourceId, tabs)) {
          dockDragRejected_ = true;
          hideDropPreview();
          dragEvent->ignore();
          return true;
        }
        dockDragRejected_ = false;
        const QPoint position = dragEvent->position().toPoint();
        const QPoint tabPosition = tabBar
                                       ? position
                                       : tabs->tabBar()->mapFrom(tabs, position);
        const int insertionIndex = tabInsertionIndex(tabs, tabPosition);
        if (isFloatingTabSurface(tabs)) {
          auto *dialog = floatingDialogForTabs(tabs);
          const QPoint dialogPosition = tabBar
              ? tabBar->mapTo(dialog, position)
              : tabs->mapTo(dialog, position);
          DockArea containerArea = DockArea::Center;
          if (floatingContainerTargetWins(
                  dialog, dialogPosition, containerArea) &&
              !canDockAtFloatingContainerEdge(
                  sourceId, dialog, false)) {
            auto *sourceTabs = tabsForWidget(dockWidget(sourceId));
            dockDragRejected_ = floatingDialogForTabs(sourceTabs) != dialog;
            hideDropPreview();
            dragEvent->ignore();
            return true;
          }
          const QPoint positionInTabs =
              tabBar ? tabBar->mapTo(tabs, position) : position;
          showFloatingDockPreview(tabs, positionInTabs);
          dragEvent->acceptProposedAction();
          return true;
        }
        const QPoint surfacePosition = tabBar
            ? tabBar->mapTo(this, position)
            : tabs->mapTo(this, position);
        const bool overAreaTarget =
            containsActiveDockTargetAt(surfacePosition);
        const bool overTabBar = tabBar && tabBar->rect().contains(position) &&
                                !overAreaTarget;
        if (overAreaTarget) {
          showDockTargetOverlay(surfacePosition, areaForTabs(tabs));
        } else if (overTabBar) {
          showTabInsertionPreview(tabs, insertionIndex);
        } else {
          const DockArea fallbackArea = areaForTabs(tabs);
          showDockTargetOverlay(surfacePosition, fallbackArea);
        }
        dragEvent->acceptProposedAction();
        return true;
      }
    } else if (event->type() == QEvent::DragLeave) {
      hideDropPreview();
      dockDragRejected_ = false;
    } else if (event->type() == QEvent::Drop) {
      auto *dropEvent = static_cast<QDropEvent *>(event);
      if (dropEvent->mimeData()->hasFormat(
              QStringLiteral("application/x-artifact-dock-id"))) {
        dockDragRejected_ = true;
      }
      const QByteArray payload = dropEvent->mimeData()->data(
          QStringLiteral("application/x-artifact-dock-id"));
      if (!payload.isEmpty()) {
        const QString sourceId = QString::fromUtf8(payload);
        const QPoint position = dropEvent->position().toPoint();
        if (dockDragWholeGroup_) {
          const QPoint tabPosition = tabBar
              ? position
              : tabs->tabBar()->mapFrom(tabs, position);
          const int insertionIndex = tabInsertionIndex(tabs, tabPosition);
          bool moved = false;
          if (isFloatingTabSurface(tabs)) {
            auto *dialog = floatingDialogForTabs(tabs);
            auto *preview = floatingDropPreviews_.value(dialog, nullptr);
            const QPoint dialogPosition = tabBar
                ? tabBar->mapTo(dialog, position)
                : tabs->mapTo(dialog, position);
            DockArea containerArea = DockArea::Center;
            const bool containerTarget = floatingContainerTargetWins(
                dialog, dialogPosition, containerArea);
            if (preview &&
                preview->isDisallowedAreaTargetAt(dialogPosition)) {
              hideDropPreview();
              dockDragRejected_ = true;
              dropEvent->ignore();
              return true;
            }
            const bool overAreaTarget = preview &&
                preview->showsAreaTargets() &&
                preview->containsAreaTarget(dialogPosition);
            const bool overTabBar = isTabBarPosition(tabs, tabPosition) &&
                                    !overAreaTarget;
            const DockArea targetButton = overAreaTarget
                ? preview->targetButtonAt(dialogPosition)
                : DockArea::Center;
            hideDropPreview();
            if (containerTarget) {
              moved = dockAtFloatingContainerEdge(
                  sourceId, dialog, containerArea, true);
            } else if (overAreaTarget && targetButton != DockArea::Center) {
              const QString targetDockId =
                  targetDockIdForSplit(tabs, sourceId);
              moved = !targetDockId.isEmpty() &&
                  splitFloatingDockTabGroup(sourceId, targetDockId, tabs,
                                            targetButton);
            } else if (overTabBar || !overAreaTarget) {
              moved = moveDockTabGroupToTabs(
                  sourceId, tabs, overTabBar ? insertionIndex : 0);
            }
            if (!moved &&
                floatingDialogForTabs(tabsForWidget(dockWidget(sourceId))) ==
                    floatingDialogForTabs(tabs)) {
              dockDragRejected_ = false;
            }
          } else {
            const QPoint surfacePosition = tabBar
                ? tabBar->mapTo(this, position)
                : tabs->mapTo(this, position);
            const bool overAreaTarget = dropPreview_ &&
                dropPreview_->showsAreaTargets() &&
                containsActiveDockTargetAt(surfacePosition);
            const bool overTabBar = tabBar && tabBar->rect().contains(position) &&
                                    !overAreaTarget;
            const bool containerTarget = dropPreview_ &&
                dropPreview_->showsContainerTargets() &&
                containerDropAreaAt(surfacePosition) != DockArea::Center;
            if (dropPreview_ &&
                dropPreview_->isDisallowedAreaTargetAt(surfacePosition)) {
              hideDropPreview();
              dockDragRejected_ = true;
              dropEvent->ignore();
              return true;
            }
            const DockArea targetArea = areaAtDockTargetOverlay(
                surfacePosition, areaForDockDropPosition(surfacePosition));
            const DockArea targetButton = overAreaTarget && dropPreview_
                ? dropPreview_->targetButtonAt(surfacePosition)
                : DockArea::Center;
            hideDropPreview();
            if (containerTarget) {
              moved = dockTabGroupToContainerEdge(sourceId, targetArea);
            } else if (overAreaTarget &&
                       targetButton != DockArea::Center) {
              const QString targetDockId =
                  targetDockIdForSplit(tabs, sourceId);
              moved = !targetDockId.isEmpty() &&
                  splitDockTabGroup(sourceId, targetDockId, targetButton,
                                    tabs);
            } else if (overTabBar || overAreaTarget) {
              moved = moveDockTabGroupToTabs(
                  sourceId, tabs, overTabBar ? insertionIndex : 0);
            }
          }
          if (moved) dropEvent->acceptProposedAction();
          else dropEvent->ignore();
          return true;
        }
        if (isFloatingTabSurface(tabs)) {
          const QPoint tabPosition = tabBar
              ? position
              : tabs->tabBar()->mapFrom(tabs, position);
          auto *dialog = floatingDialogForTabs(tabs);
          auto *preview = floatingDropPreviews_.value(dialog, nullptr);
          const QPoint dialogPosition = tabBar
              ? tabBar->mapTo(dialog, position)
              : tabs->mapTo(dialog, position);
          DockArea containerArea = DockArea::Center;
          const bool containerTarget = floatingContainerTargetWins(
              dialog, dialogPosition, containerArea);
          if (preview &&
              preview->isDisallowedAreaTargetAt(dialogPosition)) {
            hideDropPreview();
            dockDragRejected_ = true;
            dropEvent->ignore();
            return true;
          }
          const bool overAreaTarget = preview &&
              preview->showsAreaTargets() &&
              preview->containsAreaTarget(dialogPosition);
          const bool overTabBar = tabBar && tabBar->rect().contains(position) &&
                                  !overAreaTarget;
          const int insertionIndex = tabInsertionIndex(tabs, tabPosition);
          hideDropPreview();
          const DockArea targetButton = overAreaTarget
              ? preview->targetButtonAt(dialogPosition)
              : DockArea::Center;
          const bool moved = containerTarget
              ? dockAtFloatingContainerEdge(
                    sourceId, dialog, containerArea, false)
              : overAreaTarget &&
                  targetButton != DockArea::Center
              ? splitFloatingDockWidget(
                    sourceId, targetDockIdForSplit(tabs, sourceId), tabs,
                    targetButton)
              : placeDockWidgetAtTab(
                    sourceId, tabs,
                    overTabBar ? insertionIndex : 0);
          if (moved) {
            dropEvent->acceptProposedAction();
            return true;
          }
          return false;
        }
        const QPoint tabPosition = tabBar
                                       ? position
                                       : tabs->tabBar()->mapFrom(tabs, position);
        const int insertionIndex = tabInsertionIndex(tabs, tabPosition);
        const bool areaTargetOverlay = dropPreview_ &&
                                       dropPreview_->showsAreaTargets();
        const QPoint surfacePosition = tabBar
            ? tabBar->mapTo(this, position)
            : tabs->mapTo(this, position);
        const bool overAreaTarget = areaTargetOverlay &&
            containsActiveDockTargetAt(surfacePosition);
        const bool overTabBar = tabBar && tabBar->rect().contains(position) &&
                                !overAreaTarget;
        const DockArea targetArea = areaAtDockTargetOverlay(
            surfacePosition, areaForDockDropPosition(surfacePosition));
        const DockArea targetButton = overAreaTarget && dropPreview_
            ? dropPreview_->targetButtonAt(surfacePosition)
            : DockArea::Center;
        const bool containerTarget = dropPreview_ &&
            dropPreview_->showsContainerTargets() &&
            containerDropAreaAt(surfacePosition) != DockArea::Center;
        if (dropPreview_ &&
            dropPreview_->isDisallowedAreaTargetAt(surfacePosition)) {
          hideDropPreview();
          dockDragRejected_ = true;
          dropEvent->ignore();
          return true;
        }
        hideDropPreview();
        if ((overTabBar ||
             (isFloatingTabSurface(tabs) && !overAreaTarget)) &&
            placeDockWidgetAtTab(sourceId, tabs,
                                 overTabBar ? insertionIndex : 0)) {
          dropEvent->acceptProposedAction();
          return true;
        } else if ((overAreaTarget || containerTarget) &&
                   !isFloatingTabSurface(tabs)) {
          bool moved = false;
          if (containerTarget) {
            moved = dockToContainerEdge(sourceId, targetArea);
          } else if (targetButton == DockArea::Center) {
            auto *sourceWidget = dockWidget(sourceId);
            if (sourceWidget && tabsForWidget(sourceWidget) != tabs) {
              moved = placeDockWidgetAtTab(sourceId, tabs, 0);
            }
          } else {
            const QString targetDockId =
                targetDockIdForSplit(tabs, sourceId);
            if (!targetDockId.isEmpty()) {
              moved = splitDockWidget(sourceId, targetDockId, targetButton);
            } else if (tabs->count() == 0) {
              moved = moveDockWidget(sourceId, targetArea);
            }
          }
          if (moved) dropEvent->acceptProposedAction();
          else dropEvent->ignore();
          return true;
        } else if (moveDockWidget(sourceId, targetArea)) {
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
  static QList<int> normalizedSplitterRatios(const QList<int> &sizes) {
    if (sizes.size() != 3) return {};
    qint64 total = 0;
    for (const int size : sizes) {
      if (size < 0) return {};
      total += size;
    }
    if (total <= 0) return {};
    QList<int> ratios;
    int assigned = 0;
    qsizetype largestIndex = 0;
    for (qsizetype index = 0; index < sizes.size(); ++index) {
      const int ratio = static_cast<int>(
          static_cast<qint64>(sizes.at(index)) * 10000 / total);
      ratios.push_back(ratio);
      assigned += ratio;
      if (sizes.at(index) > sizes.at(largestIndex)) largestIndex = index;
    }
    ratios[largestIndex] += 10000 - assigned;
    return ratios;
  }

  void applyRootSplitterRatios(const QList<int> &ratios) {
    if (ratios.size() != 6 || !verticalSplitter_ || !splitter_ ||
        !std::all_of(ratios.cbegin(), ratios.cend(),
                     [](int ratio) { return ratio >= 0 && ratio <= 10000; }) ||
        ratios.at(0) + ratios.at(1) + ratios.at(2) != 10000 ||
        ratios.at(3) + ratios.at(4) + ratios.at(5) != 10000) {
      return;
    }
    verticalSplitter_->setSizes(
        {ratios.at(0), ratios.at(1), ratios.at(2)});
    splitter_->setSizes({ratios.at(3), ratios.at(4), ratios.at(5)});
  }

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

  DockArea containerDropAreaAt(const QPoint &position) const {
    const QRect surface = rect();
    if (!surface.contains(position)) {
      return DockArea::Center;
    }
    if (dropPreview_ && dropPreview_->showsContainerTargets() &&
        dropPreview_->containsAreaTarget(position)) {
      const DockArea button = dropPreview_->targetButtonAt(position);
      if (button != DockArea::Center) {
        return button;
      }
    }
    if (auto *tabs = tabSurfaceAt(position);
        tabs && tabBarContainsPosition(tabs, position)) {
      return DockArea::Center;
    }
    const int targetExtent = std::min(
        48, std::max(24, std::min(surface.width(), surface.height()) / 8));
    const int targetGap = std::max(5, targetExtent / 6);
    const int targetInset = targetExtent / 2 + targetGap + 2;
    const int edgeBand = std::clamp(
        std::max(std::min(surface.width(), surface.height()) / 16,
                 targetInset),
        24, 48);
    if (position.y() <= surface.top() + edgeBand) {
      return DockArea::Top;
    }
    if (position.y() >= surface.bottom() - edgeBand) {
      return DockArea::Bottom;
    }
    if (position.x() <= surface.left() + edgeBand) {
      return DockArea::Left;
    }
    if (position.x() >= surface.right() - edgeBand) {
      return DockArea::Right;
    }
    return DockArea::Center;
  }

  DockArea floatingContainerDropAreaAt(QDialog *dialog,
                                       const QPoint &position) const {
    if (!dialog || !dialog->rect().contains(position)) {
      return DockArea::Center;
    }
    if (auto *tabs = floatingTabAt(dialog, position);
        tabs && tabs->rect().contains(tabs->mapFrom(dialog, position))) {
      const QPoint tabBarPosition =
          tabs->tabBar()->mapFrom(dialog, position);
      if (isTabBarPosition(tabs, tabBarPosition)) {
        return DockArea::Center;
      }
    }
    auto *preview = floatingDropPreviews_.value(dialog, nullptr);
    if (preview && preview->showsContainerTargets() &&
        preview->containsAreaTarget(position)) {
      return preview->targetButtonAt(position);
    }
    const QRect surface = dialog->rect();
    const int targetExtent = std::min(
        48, std::max(24, std::min(surface.width(), surface.height()) / 8));
    const int targetGap = std::max(5, targetExtent / 6);
    const int targetInset = targetExtent / 2 + targetGap + 2;
    const int edgeBand = std::clamp(
        std::max(std::min(surface.width(), surface.height()) / 16,
                 targetInset),
        24, 48);
    if (position.y() <= surface.top() + edgeBand) {
      return DockArea::Top;
    }
    if (position.y() >= surface.bottom() - edgeBand) {
      return DockArea::Bottom;
    }
    if (position.x() <= surface.left() + edgeBand) {
      return DockArea::Left;
    }
    if (position.x() >= surface.right() - edgeBand) {
      return DockArea::Right;
    }
    return DockArea::Center;
  }

  bool floatingContainerTargetWins(QDialog *dialog,
                                   const QPoint &position,
                                   DockArea &containerArea) const {
    if (auto *preview = floatingDropPreviews_.value(dialog, nullptr)) {
      preview->setGeometry(dialog->rect());
    }
    containerArea = floatingContainerDropAreaAt(dialog, position);
    if (containerArea == DockArea::Center) return false;
    auto *tabs = floatingTabAt(dialog, position);
    auto *preview = floatingDropPreviews_.value(dialog, nullptr);
    if (!tabs || !preview ||
        !tabs->rect().contains(tabs->mapFrom(dialog, position))) {
      return true;
    }
    const QPoint tabBarPosition = tabs->tabBar()->mapFrom(dialog, position);
    if (isTabBarPosition(tabs, tabBarPosition)) return false;
    const QRect targetRect(tabs->mapTo(dialog, tabs->rect().topLeft()),
                           tabs->size());
    DockArea areaButton = DockArea::Center;
    const bool overAreaButton = preview->areaTargetAt(
        targetRect.center(), position, areaButton,
        dockAreaAllowedDropAreasMask(tabs));
    return !overAreaButton || areaButton != containerArea;
  }

  bool containsActiveDockTargetAt(const QPoint &position) const {
    if (!dropPreview_ || !dropPreview_->showsAreaTargets()) {
      return false;
    }
    if (dropPreview_->showsContainerTargets() &&
        containerDropAreaAt(position) == DockArea::Center) {
      return false;
    }
    return dropPreview_->containsAreaTarget(position);
  }

  QTabWidget *tabSurfaceAt(const QPoint &position) const {
    for (auto *tabs : dockedTabSurfaces_) {
      if (tabs && tabs->isVisible() &&
          tabs->rect().contains(tabs->mapFrom(
              const_cast<NativeDockSurface *>(this), position))) {
        return tabs;
      }
    }
    return nullptr;
  }

  bool canDropDockOnTabSurface(const QString &dockId,
                              const QTabWidget *targetTabs) const {
    auto *sourceWidget = dockWidget(dockId);
    if (!sourceWidget || !targetTabs) return false;
    if (tabsForWidget(sourceWidget) == targetTabs) {
      return canMoveDock(dockId);
    }
    return isFloatingTabSurface(targetTabs) ? canFloatDock(dockId)
                                            : canMoveDock(dockId);
  }

  bool tabBarContainsPosition(const QTabWidget *tabs,
                              const QPoint &position) const {
    if (!tabs || !tabs->tabBar() || !tabs->tabBar()->isVisible()) {
      return false;
    }
    return tabs->tabBar()->rect().contains(
        tabs->tabBar()->mapFrom(const_cast<NativeDockSurface *>(this),
                                position));
  }

  void registerDockTabSurface(QTabWidget *tabs, DockArea area,
                              const QString &splitPath = {},
                              const QList<int> &splitRatios = {},
                              const QString &containerGroupId =
                                  QStringLiteral("base")) {
    if (!tabs || dockedTabAreas_.contains(tabs)) {
      return;
    }
    static_cast<DockTabSurface *>(tabs)->setDockAreaTitleBarEnabled(true);
    if (auto *titleLabel =
            static_cast<DockTabSurface *>(tabs)->dockAreaTitleLabel()) {
      titleLabel->installEventFilter(this);
    }
    tabs->setAcceptDrops(true);
    tabs->tabBar()->setAcceptDrops(true);
    tabs->installEventFilter(this);
    tabs->tabBar()->installEventFilter(this);
    if (auto *surface = static_cast<DockTabSurface *>(tabs);
        surface->closeGroupButton()) {
      surface->closeGroupButton()->show();
      surface->closeGroupButton()->installEventFilter(this);
    }
    if (auto *surface = static_cast<DockTabSurface *>(tabs);
        surface->floatGroupButton()) {
      surface->floatGroupButton()->show();
      surface->floatGroupButton()->installEventFilter(this);
    }
    if (auto *surface = static_cast<DockTabSurface *>(tabs);
        surface->autoHideButton()) {
      surface->autoHideButton()->show();
      surface->autoHideButton()->installEventFilter(this);
    }
    if (auto *tabListButton =
            static_cast<DockTabSurface *>(tabs)->tabListButton()) {
      tabListButton->installEventFilter(this);
    }
    dockedTabSurfaces_.push_back(tabs);
    dockedTabAreas_.insert(tabs, area);
    areaHideSingleWidgetTitleBar_.insert(tabs, false);
    dockedSplitPaths_.insert(tabs, splitPath);
    dockedSplitRatios_.insert(tabs, splitRatios);
    dockedContainerGroupIds_.insert(tabs, containerGroupId);
    if (containerGroupId != QStringLiteral("base")) {
      containerGroupAreas_.insert(containerGroupId, area);
    }
    refreshDockTabGroupCloseButton(tabs);
  }

  QTabWidget *createDockTabSurface(DockArea area, const QString &splitPath,
                                   const QList<int> &splitRatios,
                                   const QString &containerGroupId =
                                       QStringLiteral("base")) {
    auto *tabs = createTabSurface(nullptr);
    tabs->setTabPosition(areaTabPositionAtBottom(area)
                             ? QTabWidget::South
                             : QTabWidget::North);
    static_cast<DockTabSurface *>(tabs)->refreshDockAreaTitleBarPlacement();
    registerDockTabSurface(tabs, area, splitPath, splitRatios,
                           containerGroupId);
    dynamicTabSurfaces_.push_back(tabs);
    return tabs;
  }

  void configureAutoHideBar(QTabBar *bar, DockArea area) {
    if (!bar || area == DockArea::Center) return;
    bar->setMouseTracking(true);
    switch (area) {
    case DockArea::Left: bar->setShape(QTabBar::RoundedWest); break;
    case DockArea::Right: bar->setShape(QTabBar::RoundedEast); break;
    case DockArea::Top: bar->setShape(QTabBar::RoundedNorth); break;
    case DockArea::Bottom: bar->setShape(QTabBar::RoundedSouth); break;
    case DockArea::Center: return;
    }
    bar->setProperty("artifactAutoHideSideBar", true);
    bar->setProperty("artifactAutoHideArea", static_cast<int>(area));
    bar->setDocumentMode(true);
    bar->setDrawBase(false);
    bar->setExpanding(false);
    bar->setUsesScrollButtons(true);
    bar->setMovable(false);
    bar->setAcceptDrops(true);
    bar->installEventFilter(this);
    if (area == DockArea::Left || area == DockArea::Right) {
      bar->setFixedWidth(26);
    } else {
      bar->setFixedHeight(24);
    }
    bar->hide();
  }

  QTabBar *autoHideBarForArea(DockArea area) const {
    switch (area) {
    case DockArea::Left: return autoHideLeftBar_;
    case DockArea::Right: return autoHideRightBar_;
    case DockArea::Top: return autoHideTopBar_;
    case DockArea::Bottom: return autoHideBottomBar_;
    case DockArea::Center: return nullptr;
    }
    return nullptr;
  }

  DockArea autoHideAreaForBar(const QTabBar *bar) const {
    return bar ? static_cast<DockArea>(
                     bar->property("artifactAutoHideArea").toInt())
               : DockArea::Center;
  }

  QString autoHideDockIdAt(QTabBar *bar, const QPoint &position) const {
    if (!bar) return {};
    const int index = bar->tabAt(position);
    return index >= 0 ? bar->tabData(index).toString() : QString{};
  }

  int autoHideInsertionIndex(const QTabBar *bar,
                             const QPoint &position) const {
    if (!bar) return 0;
    const int count = bar->count();
    const DockArea side = autoHideAreaForBar(bar);
    const bool vertical = side == DockArea::Left || side == DockArea::Right;
    const int coordinate = vertical ? position.y() : position.x();
    for (int index = 0; index < count; ++index) {
      const QRect tab = bar->tabRect(index);
      const int center = vertical ? tab.center().y() : tab.center().x();
      if (coordinate < center) return index;
    }
    return count;
  }

  void showAutoHideInsertionPreview(QTabBar *bar, int insertionIndex) {
    if (!bar || !dropPreview_) return;
    const QRect globalBar(bar->mapToGlobal(bar->rect().topLeft()),
                          bar->size());
    const QRect barRect(mapFromGlobal(globalBar.topLeft()), globalBar.size());
    QRect markerGlobal;
    const DockArea side = autoHideAreaForBar(bar);
    const bool vertical = side == DockArea::Left || side == DockArea::Right;
    const int count = bar->count();
    if (vertical) {
      int y = bar->rect().top();
      if (count > 0 && insertionIndex < count) {
        y = bar->tabRect(insertionIndex).top();
      } else if (count > 0) {
        y = bar->tabRect(count - 1).bottom() + 1;
      }
      const QPoint global = bar->mapToGlobal(QPoint(0, y));
      markerGlobal = QRect(global.x(), global.y() - 1, bar->width(), 3);
    } else {
      int x = bar->rect().left();
      if (count > 0 && insertionIndex < count) {
        x = bar->tabRect(insertionIndex).left();
      } else if (count > 0) {
        x = bar->tabRect(count - 1).right() + 1;
      }
      const QPoint global = bar->mapToGlobal(QPoint(x, 0));
      markerGlobal = QRect(global.x() - 1, global.y(), 3, bar->height());
    }
    const QRect markerRect(mapFromGlobal(markerGlobal.topLeft()),
                           markerGlobal.size());
    hideFloatingDropPreviews();
    clearTabInsertionPreview();
    dropPreview_->setGeometry(rect());
    dropPreview_->showAutoHideInsertion(barRect, markerRect);
    dropPreview_->show();
    dropPreview_->raise();
  }

  bool isAutoHideTabSurface(const QTabWidget *tabs) const {
    if (!tabs) return false;
    for (auto it = autoHidePanelSurfaces_.cbegin();
         it != autoHidePanelSurfaces_.cend(); ++it) {
      if (it.value() == tabs) return true;
    }
    return false;
  }

  void refreshDockTabBarVisibility(QTabWidget *tabs) {
    if (!tabs || !tabs->tabBar() || isAutoHideTabSurface(tabs)) return;
    int visibleTabCount = 0;
    int openDockCount = 0;
    for (int index = 0; index < tabs->count(); ++index) {
      if (tabs->isTabVisible(index)) ++visibleTabCount;
      openDockCount += dockOpen(dockIdForWidget(tabs->widget(index))) ? 1 : 0;
      if (QWidget *member = tabs->widget(index)) {
        member->setProperty(
            "artifactDockHideSingleWidgetTitleBar",
            areaHideSingleWidgetTitleBar_.value(tabs, false));
      }
    }
    bool showTabBar = visibleTabCount > 0;
    const QString activeDockId = dockIdForWidget(tabs->currentWidget());
    bool hasTitleBarActions = false;
    for (const QPointer<QAction> &action :
         dockTitleBarActions_.value(activeDockId)) {
      hasTitleBarActions |= !action.isNull();
    }
    bool singleFloatingSurface = false;
    if (isFloatingTabSurface(tabs) && openDockCount == 1) {
      auto *dialog = floatingDialogForTabs(tabs);
      singleFloatingSurface =
          dialog && floatingTabSurfacesForDialog(dialog).size() == 1;
    }
    const bool hideSingleTitleBar = !alwaysShowTabs_ &&
        openDockCount == 1 && !hasTitleBarActions &&
        (singleFloatingSurface ||
         (hideSingleCentralWidgetTitleBar_ &&
          !isFloatingTabSurface(tabs) && mainDockOpenWidgetCount() == 1) ||
         areaHideSingleWidgetTitleBar_.value(tabs, false));
    if (auto *surface = static_cast<DockTabSurface *>(tabs)) {
      surface->setDockAreaTitleBarSuppressed(hideSingleTitleBar);
    }
    if (!alwaysShowTabs_ && tabs->tabPosition() == QTabWidget::South) {
      showTabBar = openDockCount > 1;
    }
    showTabBar &= !hideSingleTitleBar;
    tabs->tabBar()->setVisible(showTabBar);
    refreshDockTabGroupCloseButton(tabs);
    if (hideSingleCentralWidgetTitleBar_ &&
        !refreshingDockTabBarVisibility_) {
      refreshAllDockTabBars();
    }
  }

  void refreshAllDockTabBars() {
    if (refreshingDockTabBarVisibility_) return;
    refreshingDockTabBarVisibility_ = true;
    for (auto *tabs : dockedTabSurfaces_) {
      refreshDockTabBarVisibility(tabs);
    }
    for (auto it = floatingTabOwners_.cbegin();
         it != floatingTabOwners_.cend(); ++it) {
      refreshDockTabBarVisibility(it.key());
    }
    refreshingDockTabBarVisibility_ = false;
  }

  int mainDockOpenWidgetCount() const {
    int count = 0;
    for (auto *tabs : dockedTabSurfaces_) {
      if (!tabs || isAutoHideTabSurface(tabs) || isFloatingTabSurface(tabs)) {
        continue;
      }
      for (int index = 0; index < tabs->count(); ++index) {
        if (dockOpen(dockIdForWidget(tabs->widget(index)))) ++count;
      }
    }
    return count;
  }

  bool canCloseDockTabGroup(const QTabWidget *tabs) const {
    if (!tabs) return false;
    int openCount = 0;
    for (int index = 0; index < tabs->count(); ++index) {
      const QString memberId = dockIdForWidget(tabs->widget(index));
      if (memberId.isEmpty() || !dockOpen(memberId)) continue;
      ++openCount;
      const QWidget *member = tabs->widget(index);
      if (!dockClosable(memberId) || dockPinned(memberId) ||
          (member && member->property("artifactDockCloseProtected").toBool())) {
        return false;
      }
    }
    return openCount > 0;
  }

  bool canFloatDockTabGroup(const QTabWidget *tabs) const {
    if (!tabs || tabs->count() == 0) return false;
    bool foundPanel = false;
    for (int index = 0; index < tabs->count(); ++index) {
      const QString memberId = dockIdForWidget(tabs->widget(index));
      if (memberId.isEmpty()) continue;
      foundPanel = true;
      if (!canFloatDock(memberId)) return false;
    }
    return foundPanel;
  }

  bool canDragDockTabGroup(const QString &dockId) const {
    auto *widget = dockWidget(dockId);
    auto *tabs = widget ? tabsForWidget(widget) : nullptr;
    if (!tabs || (!dockedTabAreas_.contains(tabs) &&
                  !floatingTabOwners_.contains(tabs))) {
      return false;
    }
    bool foundPanel = false;
    for (int index = 0; index < tabs->count(); ++index) {
      const QString memberId = dockIdForWidget(tabs->widget(index));
      if (memberId.isEmpty()) continue;
      foundPanel = true;
      if (!canDragDock(memberId)) return false;
    }
    return foundPanel;
  }

  bool canMoveDockTabGroupToTabs(const QString &dockId,
                                 const QTabWidget *targetTabs) const {
    auto *sourceWidget = dockWidget(dockId);
    auto *sourceTabs = sourceWidget ? tabsForWidget(sourceWidget) : nullptr;
    if (!sourceTabs || sourceTabs == targetTabs || !targetTabs ||
        (!dockedTabAreas_.contains(sourceTabs) &&
         !floatingTabOwners_.contains(sourceTabs)) ||
        (!dockedTabAreas_.contains(const_cast<QTabWidget *>(targetTabs)) &&
         !floatingTabOwners_.contains(const_cast<QTabWidget *>(targetTabs)))) {
      return false;
    }
    const bool targetFloating = isFloatingTabSurface(targetTabs);
    bool foundPanel = false;
    for (int index = 0; index < sourceTabs->count(); ++index) {
      const QString memberId = dockIdForWidget(sourceTabs->widget(index));
      if (memberId.isEmpty()) continue;
      foundPanel = true;
      if (targetFloating ? !canFloatDock(memberId)
                         : !canMoveDock(memberId)) {
        return false;
      }
    }
    return foundPanel;
  }

  bool canMoveDockTabGroupToSplit(const QString &dockId,
                                 const QTabWidget *targetTabs,
                                 DockArea side) const {
    if (side == DockArea::Center ||
        !canMoveDockTabGroupToTabs(dockId, targetTabs)) {
      return false;
    }
    return !targetDockIdForSplit(targetTabs, dockId).isEmpty();
  }

  bool canDockTabGroupAtContainerEdge(const QString &dockId) const {
    auto *widget = dockWidget(dockId);
    auto *tabs = widget ? tabsForWidget(widget) : nullptr;
    if (!tabs || (!dockedTabAreas_.contains(tabs) &&
                  !floatingTabOwners_.contains(tabs)) ||
        !containerRootWidget_ ||
        !containerRootLayout_) {
      return false;
    }
    bool foundPanel = false;
    for (int index = 0; index < tabs->count(); ++index) {
      const QString memberId = dockIdForWidget(tabs->widget(index));
      if (memberId.isEmpty()) continue;
      foundPanel = true;
      if (!canMoveDock(memberId)) return false;
    }
    return foundPanel;
  }

  bool canDockAtFloatingContainerEdge(const QString &dockId,
                                      QDialog *targetDialog,
                                      bool wholeGroup) const {
    auto *sourceWidget = dockWidget(dockId);
    auto *sourceTabs = sourceWidget ? tabsForWidget(sourceWidget) : nullptr;
    if (!sourceWidget || !sourceTabs || !targetDialog ||
        !floatingDockRoots_.value(targetDialog) ||
        floatingDialogForTabs(sourceTabs) == targetDialog) {
      return false;
    }
    return wholeGroup ? canFloatDockTabGroup(sourceTabs)
                      : canFloatDock(dockId);
  }

  bool moveDockTabGroupToTabs(const QString &dockId, QTabWidget *targetTabs,
                              int insertionIndex) {
    if (!canMoveDockTabGroupToTabs(dockId, targetTabs)) return false;
    auto *sourceWidget = dockWidget(dockId);
    auto *sourceTabs = sourceWidget ? tabsForWidget(sourceWidget) : nullptr;
    if (!sourceTabs) return false;
    const QString activeDockId = dockIdForWidget(sourceTabs->currentWidget());
    QStringList memberIds;
    for (int index = 0; index < sourceTabs->count(); ++index) {
      const QString memberId = dockIdForWidget(sourceTabs->widget(index));
      if (!memberId.isEmpty()) memberIds.push_back(memberId);
    }
    if (memberIds.isEmpty()) return false;

    beginLayoutMutationBatch(tr("Move dock tab group"));
    bool movedAll = true;
    int nextIndex = std::clamp(insertionIndex, 0, targetTabs->count());
    for (const QString &memberId : memberIds) {
      if (!moveDockWidgetToTabAt(memberId, targetTabs, nextIndex)) {
        movedAll = false;
        break;
      }
      ++nextIndex;
    }
    if (movedAll && !activeDockId.isEmpty()) {
      targetTabs->setCurrentWidget(dockWidget(activeDockId));
      refreshFloatingDialogTitle(targetTabs);
    }
    endLayoutMutationBatch();
    return movedAll;
  }

  bool splitDockTabGroup(const QString &dockId, const QString &targetDockId,
                        DockArea side, QTabWidget *targetTabs) {
    if (side == DockArea::Center || !targetTabs ||
        !canMoveDockTabGroupToTabs(dockId, targetTabs)) {
      return false;
    }
    auto *sourceWidget = dockWidget(dockId);
    auto *sourceTabs = sourceWidget ? tabsForWidget(sourceWidget) : nullptr;
    if (!sourceTabs) return false;
    const QString activeDockId = dockIdForWidget(sourceTabs->currentWidget());
    QStringList memberIds;
    for (int index = 0; index < sourceTabs->count(); ++index) {
      const QString memberId = dockIdForWidget(sourceTabs->widget(index));
      if (!memberId.isEmpty()) memberIds.push_back(memberId);
    }
    if (memberIds.isEmpty()) return false;

    beginLayoutMutationBatch(tr("Split dock tab group"));
    bool movedAll = splitDockWidget(memberIds.front(), targetDockId, side);
    auto *groupTabs = movedAll ? tabsForWidget(dockWidget(memberIds.front()))
                               : nullptr;
    if (!groupTabs) movedAll = false;
    for (qsizetype index = 1; movedAll && index < memberIds.size(); ++index) {
      movedAll = moveDockWidgetToTabAt(memberIds.at(index), groupTabs,
                                       groupTabs->count());
    }
    if (movedAll && !activeDockId.isEmpty()) {
      groupTabs->setCurrentWidget(dockWidget(activeDockId));
      refreshFloatingDialogTitle(groupTabs);
    }
    endLayoutMutationBatch();
    return movedAll;
  }

  bool splitFloatingDockTabGroup(const QString &dockId,
                                 const QString &targetDockId,
                                 QTabWidget *targetTabs, DockArea side) {
    if (side == DockArea::Center || !targetTabs ||
        !canMoveDockTabGroupToTabs(dockId, targetTabs)) {
      return false;
    }
    auto *sourceWidget = dockWidget(dockId);
    auto *sourceTabs = sourceWidget ? tabsForWidget(sourceWidget) : nullptr;
    if (!sourceTabs) return false;
    const QString activeDockId = dockIdForWidget(sourceTabs->currentWidget());
    QStringList memberIds;
    for (int index = 0; index < sourceTabs->count(); ++index) {
      const QString memberId = dockIdForWidget(sourceTabs->widget(index));
      if (!memberId.isEmpty()) memberIds.push_back(memberId);
    }
    if (memberIds.isEmpty()) return false;

    beginLayoutMutationBatch(tr("Split dock tab group"));
    bool movedAll = splitFloatingDockWidget(
        memberIds.front(), targetDockId, targetTabs, side);
    auto *groupTabs = movedAll ? tabsForWidget(dockWidget(memberIds.front()))
                               : nullptr;
    if (!groupTabs) movedAll = false;
    for (qsizetype index = 1; movedAll && index < memberIds.size(); ++index) {
      movedAll = moveDockWidgetToTabAt(memberIds.at(index), groupTabs,
                                       groupTabs->count());
    }
    if (movedAll && !activeDockId.isEmpty()) {
      groupTabs->setCurrentWidget(dockWidget(activeDockId));
      refreshFloatingDialogTitle(groupTabs);
    }
    endLayoutMutationBatch();
    return movedAll;
  }

  bool dockTabGroupToContainerEdge(const QString &dockId, DockArea side) {
    if (side == DockArea::Center || !containerRootWidget_ ||
        !containerRootLayout_) {
      return false;
    }
    auto *sourceWidget = dockWidget(dockId);
    auto *sourceTabs = sourceWidget ? tabsForWidget(sourceWidget) : nullptr;
    if (!sourceTabs || (!dockedTabAreas_.contains(sourceTabs) &&
                        !floatingTabOwners_.contains(sourceTabs))) {
      return false;
    }
    const QString activeDockId = dockIdForWidget(sourceTabs->currentWidget());
    QStringList memberIds;
    for (int index = 0; index < sourceTabs->count(); ++index) {
      const QString memberId = dockIdForWidget(sourceTabs->widget(index));
      if (memberId.isEmpty()) continue;
      if (!canMoveDock(memberId)) return false;
      memberIds.push_back(memberId);
    }
    if (memberIds.isEmpty()) return false;

    beginLayoutMutationBatch(tr("Dock tab group at container edge"));
    bool movedAll = dockToContainerEdge(memberIds.front(), side);
    auto *groupTabs = movedAll ? tabsForWidget(dockWidget(memberIds.front()))
                               : nullptr;
    if (!groupTabs) movedAll = false;
    for (qsizetype index = 1; movedAll && index < memberIds.size(); ++index) {
      movedAll = moveDockWidgetToTabAt(memberIds.at(index), groupTabs,
                                       groupTabs->count());
    }
    if (movedAll && !activeDockId.isEmpty()) {
      groupTabs->setCurrentWidget(dockWidget(activeDockId));
    }
    endLayoutMutationBatch();
    return movedAll;
  }

  void updateAutoHideButtonPresentation(QTabWidget *tabs) {
    auto *surface = tabs ? static_cast<DockTabSurface *>(tabs) : nullptr;
    auto *button = surface ? surface->autoHideButton() : nullptr;
    if (!button) return;
    const QString accessibleName = autoHideButtonTogglesArea_
        ? tr("Auto-hide tab group")
        : tr("Auto-hide active panel");
    const QString toolTip = autoHideButtonTogglesArea_
        ? accessibleName
        : tr("Auto-hide active panel (Ctrl-click to auto-hide the tab group)");
    button->setAccessibleName(accessibleName);
    button->setToolTip(toolTip);
  }

  void refreshDockTabGroupCloseButton(QTabWidget *tabs) {
    auto *surface = tabs ? static_cast<DockTabSurface *>(tabs) : nullptr;
    refreshDockTabLabels(tabs);
    refreshDockTabCloseButtons(tabs);
    refreshFloatingDialogTitle(tabs);
    if (surface && tabs) {
      const QString activeDockId =
          dockIdForWidget(tabs->currentWidget());
      surface->setDockAreaTitle(
          titles_.value(activeDockId, activeDockId));
      QList<QAction *> actions;
      for (const QPointer<QAction> &action :
           dockTitleBarActions_.value(activeDockId)) {
        if (action) actions.push_back(action.data());
      }
      surface->setDockAreaTitleActions(actions);
    }
    const bool docked = tabs && dockedTabAreas_.contains(tabs);
    const bool floating = isFloatingTabSurface(tabs);
    auto *button = surface ? surface->closeGroupButton() : nullptr;
    if (button) {
      bool hasOpenPanel = false;
      for (int index = 0; index < tabs->count(); ++index) {
        hasOpenPanel |= dockOpen(dockIdForWidget(tabs->widget(index)));
      }
      const QString activeDockId =
          dockIdForWidget(tabs ? tabs->currentWidget() : nullptr);
      const bool canClose = dockAreaCloseButtonClosesTab_
          ? canCloseDockTab(activeDockId)
          : (docked || floating) && canCloseDockTabGroup(tabs);
      button->setToolTip(dockAreaCloseButtonClosesTab_
                             ? tr("Close panel")
                             : tr("Close tab group"));
      button->setAccessibleName(button->toolTip());
      button->setVisible((docked || floating) && dockAreaHasCloseButton_ &&
                         hasOpenPanel &&
                         (!dockAreaHideDisabledButtons_ || canClose));
      button->setEnabled(canClose);
    }
    if (surface && surface->floatGroupButton()) {
      bool hasPanel = false;
      if (tabs) {
        for (int index = 0; index < tabs->count(); ++index) {
          hasPanel |= !dockIdForWidget(tabs->widget(index)).isEmpty();
        }
      }
      const bool canFloatGroup = docked && canFloatDockTabGroup(tabs);
      surface->floatGroupButton()->setVisible(
          docked && dockAreaHasUndockButton_ && hasPanel &&
          (!dockAreaHideDisabledButtons_ || canFloatGroup));
      surface->floatGroupButton()->setEnabled(canFloatGroup);
    }
    if (surface && surface->autoHideButton()) {
      updateAutoHideButtonPresentation(tabs);
      const DockArea area = dockedTabAreas_.value(tabs, DockArea::Center);
      const QString activeDockId =
          tabs ? dockIdForWidget(tabs->currentWidget()) : QString{};
      bool hasOpenPanel = false;
      if (tabs) {
        for (int index = 0; index < tabs->count(); ++index) {
          hasOpenPanel |= dockOpen(dockIdForWidget(tabs->widget(index)));
        }
      }
      const bool canAutoHideGroup = docked && area != DockArea::Center &&
          hasOpenPanel && (autoHideButtonTogglesArea_
              ? canAutoHideDockSurface(tabs)
              : (!activeDockId.isEmpty() && dockOpen(activeDockId) &&
                 canAutoHideDock(activeDockId)));
      const bool autoHideAvailable =
          docked && area != DockArea::Center && hasOpenPanel;
      surface->autoHideButton()->setVisible(
          autoHideAvailable &&
          (!dockAreaHideDisabledButtons_ || canAutoHideGroup));
      surface->autoHideButton()->setEnabled(canAutoHideGroup);
    }
  }

  void closeDockTabGroup(QTabWidget *tabs) {
    if (!canCloseDockTabGroup(tabs)) return;
    const QByteArray beforeState = beginLayoutMutation();
    for (int index = 0; index < tabs->count(); ++index) {
      const QString memberId = dockIdForWidget(tabs->widget(index));
      if (!memberId.isEmpty() && dockOpen(memberId) &&
          dockClosable(memberId) && !dockPinned(memberId)) {
        const QWidget *member = tabs->widget(index);
        if (!(member &&
              member->property("artifactDockCloseProtected").toBool())) {
          setDockVisible(memberId, false);
        }
      }
    }
    finishLayoutMutation(beforeState, tr("Close tab group"));
  }

  bool canCloseDockTab(const QString &dockId) const {
    if (dockId.isEmpty() || !dockOpen(dockId) || !dockClosable(dockId) ||
        dockPinned(dockId)) {
      return false;
    }
    const QWidget *member = dockWidget(dockId);
    return !member ||
           !member->property("artifactDockCloseProtected").toBool();
  }

  void closeActiveDockTab(QTabWidget *tabs) {
    const QString dockId = tabs ? dockIdForWidget(tabs->currentWidget())
                                : QString{};
    if (!canCloseDockTab(dockId)) return;
    const QByteArray beforeState = beginLayoutMutation();
    setDockVisible(dockId, false);
    finishLayoutMutation(beforeState, tr("Close panel"));
  }

  void refreshAllDockAreaButtons() {
    for (auto *tabs : dockedTabSurfaces_) {
      refreshDockTabGroupCloseButton(tabs);
    }
    for (auto it = floatingTabOwners_.cbegin();
         it != floatingTabOwners_.cend(); ++it) {
      refreshDockTabGroupCloseButton(it.key());
    }
  }

  void syncAutoHideBarVisibility(DockArea area) {
    auto *bar = autoHideBarForArea(area);
    if (!bar) return;
    bool visible = false;
    for (int index = 0; index < bar->count(); ++index) {
      visible |= bar->isTabVisible(index);
    }
    bar->setVisible(visible);
  }

  void addAutoHideBarTab(const QString &dockId, DockArea area,
                         int insertionIndex = -1) {
    auto *bar = autoHideBarForArea(area);
    if (!bar) return;
    const int index = insertionIndex < 0
        ? bar->count()
        : std::clamp(insertionIndex, 0, bar->count());
    const int inserted = bar->insertTab(
        index, titles_.value(dockId, dockId));
    bar->setTabData(inserted, dockId);
    refreshAutoHideBarTab(bar, inserted);
    bar->setTabVisible(inserted, autoHideVisible_.value(dockId, true));
    syncAutoHideBarVisibility(area);
  }

  void scheduleAutoHideHover(const QString &dockId, bool open) {
    if (!autoHideShowOnMouseOver_) return;
    const QString resolvedId = open ? resolveDockId(dockId)
                                    : autoHideActiveDockId_;
    if (resolvedId.isEmpty() ||
        (open && !autoHideAreas_.contains(resolvedId)) ||
        (!open && (autoHideActiveDockId_ != resolvedId ||
                   !autoHideFlyout_ || !autoHideFlyout_->isVisible()))) {
      return;
    }
    if (open && autoHideActiveDockId_ == resolvedId &&
        autoHideFlyout_ && autoHideFlyout_->isVisible()) {
      autoHideHoverTimer_.stop();
      autoHideHoverDockId_.clear();
      return;
    }
    if (autoHideHoverTimer_.isActive() &&
        autoHideHoverDockId_ == resolvedId &&
        autoHideHoverOpen_ == open) {
      return;
    }
    autoHideHoverDockId_ = resolvedId;
    autoHideHoverOpen_ = open;
    autoHideHoverTimer_.start(500, this);
  }

  void refreshAutoHideBarTab(QTabBar *bar, int index) {
    if (!bar || index < 0 || index >= bar->count()) return;
    const QString dockId = bar->tabData(index).toString();
    if (dockId.isEmpty()) return;
    const QIcon icon = dockIcon(dockId);
    const QString title = titles_.value(dockId, dockId);
    bar->setTabIcon(index, icon);
    bar->setTabText(index,
                    autoHideSideBarsIconOnly_ && !icon.isNull()
                        ? QString{}
                        : title);
    const QString toolTip = dockTabToolTip(dockId);
    bar->setTabToolTip(index, toolTip.isEmpty() ? title : toolTip);
  }

  void closeAutoHideFlyout() {
    autoHideHoverTimer_.stop();
    autoHideHoverDockId_.clear();
    if (!autoHideFlyout_) return;
    if (autoHideActiveTabs_) {
      autoHideFlyoutLayout_->removeWidget(autoHideActiveTabs_);
      autoHideActiveTabs_->hide();
      autoHideActiveTabs_ = nullptr;
    }
    autoHideActiveDockId_.clear();
    autoHideFlyout_->hide();
  }

  void positionAutoHideFlyout() {
    if (!autoHideFlyout_ || !autoHideFlyout_->isVisible()) return;
    const int width = std::min(420, std::max(260, this->width() * 2 / 5));
    const int height = std::min(360, std::max(220, this->height() * 2 / 3));
    const DockArea side = autoHideAreas_.value(autoHideActiveDockId_,
                                               DockArea::Left);
    QRect geometry;
    switch (side) {
    case DockArea::Left:
      geometry = QRect(autoHideLeftBar_->geometry().right() + 1,
                       autoHideTopBar_->height(), width,
                       std::max(1, this->height() - autoHideTopBar_->height() -
                                      autoHideBottomBar_->height()));
      break;
    case DockArea::Right:
      geometry = QRect(std::max(0, this->width() - width -
                                      autoHideRightBar_->width()),
                       autoHideTopBar_->height(), width,
                       std::max(1, this->height() - autoHideTopBar_->height() -
                                      autoHideBottomBar_->height()));
      break;
    case DockArea::Top:
      geometry = QRect(autoHideLeftBar_->width(), autoHideTopBar_->height(),
                       std::max(1, this->width() - autoHideLeftBar_->width() -
                                      autoHideRightBar_->width()), height);
      break;
    case DockArea::Bottom:
      geometry = QRect(autoHideLeftBar_->width(),
                       std::max(0, this->height() - height -
                                      autoHideBottomBar_->height()),
                       std::max(1, this->width() - autoHideLeftBar_->width() -
                                      autoHideRightBar_->width()), height);
      break;
    case DockArea::Center: return;
    }
    autoHideFlyout_->setGeometry(geometry);
    autoHideFlyout_->raise();
  }

  bool openAutoHideDock(const QString &dockId, bool toggleIfOpen = false) {
    const QString resolvedId = resolveDockId(dockId);
    auto *tabs = autoHidePanelSurfaces_.value(resolvedId, nullptr);
    if (!tabs || !autoHideVisible_.value(resolvedId, true)) return false;
    if (toggleIfOpen && autoHideActiveDockId_ == resolvedId &&
        autoHideFlyout_->isVisible()) {
      closeAutoHideFlyout();
      return true;
    }
    closeAutoHideFlyout();
    autoHideActiveDockId_ = resolvedId;
    autoHideActiveTabs_ = tabs;
    autoHideFlyoutTitle_->setText(titles_.value(resolvedId, resolvedId));
    autoHideFlyoutCloseButton_->setProperty("artifactDockCloseId", resolvedId);
    syncPinButton(resolvedId, dockPinned(resolvedId));
    autoHideDockBackButton_->setProperty("artifactAutoHideDockBackId",
                                         resolvedId);
    autoHideDockBackButton_->setEnabled(canMoveDock(resolvedId));
    tabs->show();
    autoHideFlyoutLayout_->addWidget(tabs, 1);
    autoHideFlyout_->show();
    positionAutoHideFlyout();
    setActiveTabSurface(tabs);
    return true;
  }

  bool removeAutoHidePlacement(const QString &dockId, QWidget *widget,
                               bool preserveReturnLocation = false) {
    if (!autoHideAreas_.contains(dockId)) return false;
    const DockArea area = autoHideAreas_.take(dockId);
    if (autoHideActiveDockId_ == dockId) closeAutoHideFlyout();
    if (auto *tabs = autoHidePanelSurfaces_.take(dockId)) {
      const int index = widget ? tabs->indexOf(widget) : -1;
      if (index >= 0) tabs->removeTab(index);
      if (widget) widget->setParent(nullptr);
      tabs->hide();
      tabs->setParent(nullptr);
      delete tabs;
    }
    autoHideVisible_.remove(dockId);
    if (!preserveReturnLocation) {
      autoHideReturnTabGroups_.remove(dockId);
      autoHideReturnTabIndexes_.remove(dockId);
      autoHideReturnSplitPaths_.remove(dockId);
      autoHideReturnSplitRatios_.remove(dockId);
      autoHideReturnContainerGroupIds_.remove(dockId);
      autoHideReturnContainerPaths_.remove(dockId);
      autoHideReturnContainerRatios_.remove(dockId);
    }
    if (auto *bar = autoHideBarForArea(area)) {
      for (int index = bar->count() - 1; index >= 0; --index) {
        if (bar->tabData(index).toString() == dockId) {
          bar->removeTab(index);
        }
      }
    }
    syncAutoHideBarVisibility(area);
    return true;
  }

  bool setDockAutoHidden(const QString &dockId, DockArea area,
                         int insertionIndex = -1) {
    const QString resolvedId = resolveDockId(dockId);
    if (resolvedId.isEmpty() || area == DockArea::Center ||
        !canAutoHideDock(resolvedId) ||
        !docks_.contains(resolvedId) || floatingDialogs_.contains(resolvedId)) {
      return false;
    }
    const QByteArray beforeState = beginLayoutMutation();
    auto *widget = docks_.value(resolvedId, nullptr);
    const bool visible = dockOpen(resolvedId);
    if (autoHideAreas_.contains(resolvedId)) {
      removeAutoHidePlacement(resolvedId, widget, true);
    } else if (auto *sourceTabs = tabsForWidget(widget)) {
      widget->setProperty("artifactDockAreaAllowedDropAreas",
                          dockAreaAllowedDropAreasMask(resolvedId));
      const DockArea sourceArea = areas_.value(resolvedId,
                                                areaForTabs(sourceTabs));
      const int index = sourceTabs->indexOf(widget);
      QStringList returnTabIds;
      for (int tabIndex = 0; tabIndex < sourceTabs->count(); ++tabIndex) {
        const QString tabId = dockIdForWidget(sourceTabs->widget(tabIndex));
        if (!tabId.isEmpty()) returnTabIds.push_back(tabId);
      }
      QString returnTabGroup = QStringLiteral("tabs:") +
                               returnTabIds.join(QStringLiteral("|"));
      int returnTabIndex = std::max(0, index);
      bool adoptedPriorGroup = false;
      const QString containerGroupId = dockedContainerGroupIds_.value(
          sourceTabs, QStringLiteral("base"));
      QString returnContainerPath;
      QList<int> returnContainerRatios;
      containerSplitPathForGroup(containerGroupId, returnContainerPath,
                                 returnContainerRatios);
      autoHideReturnContainerGroupIds_.insert(resolvedId,
                                              containerGroupId);
      autoHideReturnContainerPaths_.insert(resolvedId,
                                           returnContainerPath);
      autoHideReturnContainerRatios_.insert(resolvedId,
                                            returnContainerRatios);
      for (auto it = autoHideReturnTabGroups_.cbegin();
           it != autoHideReturnTabGroups_.cend(); ++it) {
        if (returnTabIds.size() != 1 || it.key() == resolvedId ||
            !it.value().startsWith(QStringLiteral("tabs:"))) {
          continue;
        }
        const auto priorIds = it.value().mid(5).split(
            QLatin1Char('|'), Qt::SkipEmptyParts);
        const int priorIndex = priorIds.indexOf(resolvedId);
        const int currentIndex = returnTabGroup.mid(5).split(
            QLatin1Char('|'), Qt::SkipEmptyParts).size();
        if (priorIndex >= 0 && priorIds.size() > currentIndex) {
          returnTabGroup = it.value();
          returnTabIndex = priorIndex;
          adoptedPriorGroup = true;
          autoHideReturnSplitPaths_.insert(
              resolvedId, autoHideReturnSplitPaths_.value(it.key()));
          autoHideReturnSplitRatios_.insert(
              resolvedId, autoHideReturnSplitRatios_.value(it.key()));
          autoHideReturnContainerGroupIds_.insert(
              resolvedId,
              autoHideReturnContainerGroupIds_.value(it.key()));
          autoHideReturnContainerPaths_.insert(
              resolvedId,
              autoHideReturnContainerPaths_.value(it.key()));
          autoHideReturnContainerRatios_.insert(
              resolvedId,
              autoHideReturnContainerRatios_.value(it.key()));
        }
      }
      autoHideReturnTabGroups_.insert(resolvedId, returnTabGroup);
      autoHideReturnTabIndexes_.insert(resolvedId, returnTabIndex);
      QString returnPath;
      QList<int> returnRatios;
      if (!adoptedPriorGroup &&
          splitPathForTabs(sourceTabs, returnPath, returnRatios)) {
        autoHideReturnSplitPaths_.insert(resolvedId, returnPath);
        autoHideReturnSplitRatios_.insert(resolvedId, returnRatios);
      }
      if (index >= 0) {
        sourceTabs->removeTab(index);
        widget->setParent(nullptr);
      }
      refreshDockTabBarVisibility(sourceTabs);
      bool anyOpen = false;
      for (int tabIndex = 0; tabIndex < sourceTabs->count(); ++tabIndex) {
        anyOpen |= dockOpen(dockIdForWidget(sourceTabs->widget(tabIndex)));
      }
      sourceTabs->setVisible(anyOpen);
      if (sourceTabs->count() == 0 &&
          dynamicTabSurfaces_.contains(sourceTabs)) {
        collapseEmptyDockSurface(sourceTabs, sourceArea);
      }
    }
    auto *tabs = createTabSurface(autoHideFlyout_);
    tabs->tabBar()->hide();
    tabs->installEventFilter(this);
    tabs->tabBar()->installEventFilter(this);
    widget->setParent(tabs);
    const int tabIndex = tabs->addTab(widget,
                                      titles_.value(resolvedId, resolvedId));
    installCloseButton(tabs, tabIndex, resolvedId);
    tabs->setTabVisible(tabIndex, visible && !dockNoTab(resolvedId));
    tabs->hide();
    autoHidePanelSurfaces_.insert(resolvedId, tabs);
    autoHideAreas_.insert(resolvedId, area);
    autoHideVisible_.insert(resolvedId, visible);
    addAutoHideBarTab(resolvedId, area, insertionIndex);
    finishLayoutMutation(beforeState, tr("Auto-hide panel"));
    return true;
  }

  bool canAutoHideDockTabGroup(const QTabWidget *tabs) const {
    if (!tabs ||
        (!dockedTabAreas_.contains(const_cast<QTabWidget *>(tabs)) &&
         !floatingTabOwners_.contains(const_cast<QTabWidget *>(tabs))) ||
        tabs->count() < 2) {
      return false;
    }
    int openCount = 0;
    for (int index = 0; index < tabs->count(); ++index) {
      const QString memberId = dockIdForWidget(tabs->widget(index));
      if (memberId.isEmpty() || !dockOpen(memberId)) continue;
      ++openCount;
      if ((!docks_.contains(memberId) &&
           !floatingWidgets_.contains(memberId)) ||
          !canAutoHideDock(memberId)) {
        return false;
      }
    }
    return openCount > 0;
  }

  bool canAutoHideDockSurface(const QTabWidget *tabs) const {
    if (!tabs || !dockedTabAreas_.contains(const_cast<QTabWidget *>(tabs))) {
      return false;
    }
    bool foundOpenPanel = false;
    for (int index = 0; index < tabs->count(); ++index) {
      const QString memberId = dockIdForWidget(tabs->widget(index));
      if (memberId.isEmpty() || !dockOpen(memberId)) continue;
      foundOpenPanel = true;
      if (!canAutoHideDock(memberId)) return false;
    }
    return foundOpenPanel;
  }

  bool autoHideDockSurface(QTabWidget *tabs, DockArea area) {
    if (area == DockArea::Center || !canAutoHideDockSurface(tabs)) {
      return false;
    }
    if (tabs->count() > 1) {
      return autoHideDockTabGroup(tabs, area);
    }
    const QString dockId = dockIdForWidget(tabs->currentWidget());
    return !dockId.isEmpty() && setDockAutoHidden(dockId, area);
  }

  bool autoHideDockTabGroup(QTabWidget *tabs, DockArea area,
                            int insertionIndex = -1) {
    if (area == DockArea::Center || !canAutoHideDockTabGroup(tabs)) {
      return false;
    }
    QStringList originalTabIds;
    QStringList openDockIds;
    for (int index = 0; index < tabs->count(); ++index) {
      const QString memberId = dockIdForWidget(tabs->widget(index));
      if (memberId.isEmpty()) continue;
      originalTabIds.push_back(memberId);
      if (dockOpen(memberId)) openDockIds.push_back(memberId);
    }
    if (openDockIds.isEmpty()) return false;

    const bool floatingSource = isFloatingTabSurface(tabs);
    const QStringList returnIds = floatingSource ? openDockIds
                                                  : originalTabIds;
    const QString returnTabGroup = QStringLiteral("tabs:") +
                                   returnIds.join(QLatin1Char('|'));
    QString returnSplitPath;
    QList<int> returnSplitRatios;
    const bool hasReturnSplitPath =
        splitPathForTabs(tabs, returnSplitPath, returnSplitRatios);
    const QString returnContainerGroupId = floatingSource
        ? QStringLiteral("base")
        : dockedContainerGroupIds_.value(tabs, QStringLiteral("base"));
    QString returnContainerPath;
    QList<int> returnContainerRatios;
    containerSplitPathForGroup(returnContainerGroupId, returnContainerPath,
                               returnContainerRatios);

    beginLayoutMutationBatch(tr("Auto-hide tab group"));
    bool movedAll = true;
    for (const QString &memberId : openDockIds) {
      const bool moved = floatingSource
          ? autoHideDockFromDrag(memberId, area, insertionIndex)
          : setDockAutoHidden(memberId, area, insertionIndex);
      if (!moved) {
        movedAll = false;
        break;
      }
      if (insertionIndex >= 0) ++insertionIndex;
      autoHideReturnTabGroups_.insert(memberId, returnTabGroup);
      autoHideReturnTabIndexes_.insert(memberId,
                                       returnIds.indexOf(memberId));
      if (hasReturnSplitPath) {
        autoHideReturnSplitPaths_.insert(memberId, returnSplitPath);
        autoHideReturnSplitRatios_.insert(memberId, returnSplitRatios);
      }
      autoHideReturnContainerGroupIds_.insert(memberId,
                                              returnContainerGroupId);
      autoHideReturnContainerPaths_.insert(memberId,
                                           returnContainerPath);
      autoHideReturnContainerRatios_.insert(memberId,
                                            returnContainerRatios);
      if (floatingSource) {
        autoHideReturnSplitPaths_.remove(memberId);
        autoHideReturnSplitRatios_.remove(memberId);
        autoHideReturnContainerPaths_.remove(memberId);
        autoHideReturnContainerRatios_.remove(memberId);
      }
    }
    endLayoutMutationBatch();
    return movedAll;
  }

  bool autoHideDockFromDrag(const QString &dockId, DockArea area,
                            int insertionIndex) {
    const QString resolvedId = resolveDockId(dockId);
    if (resolvedId.isEmpty() || area == DockArea::Center ||
        !canAutoHideDock(resolvedId)) return false;
    if (!floatingDialogs_.contains(resolvedId)) {
      int adjustedInsertionIndex = insertionIndex;
      if (autoHideAreas_.value(resolvedId, DockArea::Center) == area) {
        if (auto *bar = autoHideBarForArea(area)) {
          for (int index = 0; index < bar->count(); ++index) {
            if (bar->tabData(index).toString() == resolvedId) {
              if (index < adjustedInsertionIndex) {
                --adjustedInsertionIndex;
              }
              break;
            }
          }
        }
      }
      return setDockAutoHidden(resolvedId, area, adjustedInsertionIndex);
    }
    auto *widget = floatingWidgets_.value(resolvedId, nullptr);
    auto *dialog = floatingDialogs_.value(resolvedId, nullptr);
    if (!widget || !dialog) return false;
    const QString title = titles_.value(resolvedId, resolvedId);
    const bool visible = dockOpen(resolvedId);
    const bool pinned = pinned_.value(resolvedId, false);
    const QRect geometry = dialog->geometry();
    beginLayoutMutationBatch(tr("Auto-hide panel"));
    bool moved = removeDockWidget(resolvedId) &&
        addDockWidget(resolvedId, title, widget, DockArea::Center) &&
        setDockAutoHidden(resolvedId, area, insertionIndex);
    if (moved) {
      pinned_.insert(resolvedId, pinned);
      setDockVisible(resolvedId, visible);
    } else if (!docks_.contains(resolvedId) &&
               !floatingDialogs_.contains(resolvedId)) {
      addFloatingDockWidget(resolvedId, title, widget, geometry, visible);
      pinned_.insert(resolvedId, pinned);
    }
    endLayoutMutationBatch();
    return moved;
  }

  bool dockAutoHiddenWidget(const QString &dockId, DockArea targetArea) {
    const QString resolvedId = resolveDockId(dockId);
    if (!autoHideAreas_.contains(resolvedId) || !canMoveDock(resolvedId)) {
      return false;
    }
    const QByteArray beforeState = beginLayoutMutation();
    auto *widget = docks_.value(resolvedId, nullptr);
    const QString title = titles_.value(resolvedId, resolvedId);
    const bool visible = autoHideVisible_.value(resolvedId, true);
    const bool pinned = pinned_.value(resolvedId, false);
    const int returnAllowedDropAreas =
        dockAreaAllowedDropAreasMask(resolvedId);
    const QString returnTabGroup =
        autoHideReturnTabGroups_.value(resolvedId);
    const int returnTabIndex = autoHideReturnTabIndexes_.value(resolvedId, -1);
    const QString returnSplitPath =
        autoHideReturnSplitPaths_.value(resolvedId);
    const QList<int> returnSplitRatios =
        autoHideReturnSplitRatios_.value(resolvedId);
    const QString returnContainerGroupId =
        autoHideReturnContainerGroupIds_.value(
            resolvedId, QStringLiteral("base"));
    const QString returnContainerPath =
        autoHideReturnContainerPaths_.value(resolvedId);
    const QList<int> returnContainerRatios =
        autoHideReturnContainerRatios_.value(resolvedId);
    QString returnTargetId;
    bool hadReturnTabPeers = false;
    bool allReturnTabPeersAutoHidden = true;
    if (returnTabGroup.startsWith(QStringLiteral("tabs:"))) {
      const auto returnIds = returnTabGroup.mid(5).split(
          QLatin1Char('|'), Qt::SkipEmptyParts);
      for (const QString &candidateId : returnIds) {
        if (candidateId == resolvedId) continue;
        hadReturnTabPeers = true;
        if (docks_.contains(candidateId) &&
            !autoHideAreas_.contains(candidateId)) {
          allReturnTabPeersAutoHidden = false;
          returnTargetId = candidateId;
          break;
        }
        if (!autoHideAreas_.contains(candidateId)) {
          allReturnTabPeersAutoHidden = false;
        }
      }
    }
    const bool removed = removeDockWidget(resolvedId);
    bool added = false;
    bool restoredSavedPath = false;
    if (removed && !returnTargetId.isEmpty()) {
      added = addDockWidgetToTab(resolvedId, title, widget,
                                 returnTargetId, returnTabIndex);
    }
    if (removed && !added && returnTargetId.isEmpty() &&
        (!hadReturnTabPeers || allReturnTabPeersAutoHidden)) {
      if (returnContainerGroupId != QStringLiteral("base") &&
          !returnContainerPath.isEmpty()) {
        added = restoreDockAtContainerPath(
            resolvedId, title, widget, targetArea, returnContainerPath,
            returnContainerRatios, visible, pinned,
            returnContainerGroupId);
        restoredSavedPath = added;
      }
      if (!added) {
        added = restoreDockAtSplitPath(
            resolvedId, title, widget, targetArea, returnSplitPath,
            returnSplitRatios, visible, pinned, returnContainerGroupId);
        restoredSavedPath = added;
      }
    }
    if (removed && !added) {
      added = addDockWidget(resolvedId, title, widget, targetArea);
    }
    if (!removed || !added) {
      if (widget && !docks_.contains(resolvedId)) {
        addDockWidget(resolvedId, title, widget, targetArea);
        pinned_.insert(resolvedId, pinned);
        setDockVisible(resolvedId, visible);
      }
      return false;
    }
    pinned_.insert(resolvedId, pinned);
    setDockVisible(resolvedId, visible);
    if (restoredSavedPath) {
      if (auto *tabs = tabsForWidget(widget)) {
        tabs->setProperty("artifactDockAreaAllowedDropAreas",
                          returnAllowedDropAreas);
        for (int index = 0; index < tabs->count(); ++index) {
          if (auto *member = tabs->widget(index)) {
            member->setProperty("artifactDockAreaAllowedDropAreas",
                                returnAllowedDropAreas);
          }
        }
      }
    }
    finishLayoutMutation(beforeState, tr("Dock panel back"));
    return true;
  }

  DockArea areaForDockDropPosition(const QPoint &position) const {
    const DockArea containerArea = containerDropAreaAt(position);
    if (containerArea != DockArea::Center) {
      return containerArea;
    }
    if (auto *tabs = tabSurfaceAt(position)) {
      return areaForTabs(tabs);
    }
    return areaForPosition(position);
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

  QRect splitPreviewRect(const QRect &targetRect, DockArea side) const {
    QRect preview = targetRect;
    if (side == DockArea::Left) {
      preview.setWidth(std::max(1, targetRect.width() / 2));
    } else if (side == DockArea::Right) {
      const int width = std::max(1, targetRect.width() / 2);
      preview.setX(targetRect.right() - width + 1);
      preview.setWidth(width);
    } else if (side == DockArea::Top) {
      preview.setHeight(std::max(1, targetRect.height() / 2));
    } else if (side == DockArea::Bottom) {
      const int height = std::max(1, targetRect.height() / 2);
      preview.setY(targetRect.bottom() - height + 1);
      preview.setHeight(height);
    }
    return preview;
  }

  QString targetDockIdForSplit(const QTabWidget *tabs,
                               const QString &sourceDockId) const {
    if (!tabs) {
      return {};
    }
    const int currentIndex = tabs->currentIndex();
    if (currentIndex >= 0) {
      const QString currentId = dockIdForWidget(tabs->widget(currentIndex));
      if (!currentId.isEmpty() && currentId != sourceDockId) {
        return currentId;
      }
    }
    for (int index = 0; index < tabs->count(); ++index) {
      const QString candidate = dockIdForWidget(tabs->widget(index));
      if (!candidate.isEmpty() && candidate != sourceDockId) {
        return candidate;
      }
    }
    return {};
  }

  QTabWidget *containerEdgeDockTabs(DockArea area,
                                    const QString &sourceDockId) const {
    QTabWidget *nearestTabs = nullptr;
    int nearestCoordinate = 0;
    for (auto *tabs : dockedTabSurfaces_) {
      if (!tabs || dockedTabAreas_.value(tabs, DockArea::Center) != area ||
          tabs->count() == 0 || !tabs->isVisible() ||
          targetDockIdForSplit(tabs, sourceDockId).isEmpty()) {
        continue;
      }
      const QPoint topLeft = tabs->mapTo(
          const_cast<NativeDockSurface *>(this), tabs->rect().topLeft());
      const QRect surface(topLeft, tabs->size());
      const int coordinate =
          area == DockArea::Left ? surface.left()
          : area == DockArea::Right ? -surface.right()
          : area == DockArea::Top ? surface.top()
                                  : -surface.bottom();
      if (!nearestTabs || coordinate < nearestCoordinate) {
        nearestTabs = tabs;
        nearestCoordinate = coordinate;
      }
    }
    return nearestTabs;
  }

  QRect containerEdgePreviewRect(DockArea area,
                                 const QString &sourceDockId) const {
    static_cast<void>(sourceDockId);
    if (!containerRootWidget_) {
      return dropPreviewRectForArea(area);
    }
    const QPoint topLeft = containerRootWidget_->mapTo(
        const_cast<NativeDockSurface *>(this),
        containerRootWidget_->rect().topLeft());
    return splitPreviewRect(
        QRect(topLeft, containerRootWidget_->size()), area);
  }

  QRect floatingContainerEdgePreviewRect(QDialog *dialog,
                                         DockArea area) const {
    auto *root = dialog ? floatingDockRoots_.value(dialog, nullptr) : nullptr;
    if (!dialog || !root) return dialog ? dialog->rect() : QRect{};
    const QPoint topLeft = root->mapTo(dialog, root->rect().topLeft());
    return splitPreviewRect(QRect(topLeft, root->size()), area);
  }

  bool dockToContainerEdge(const QString &dockId, DockArea area) {
    if (area == DockArea::Center || !containerRootWidget_ ||
        !containerRootLayout_) {
      return false;
    }
    const QString resolvedId = resolveDockId(dockId);
    if (!canMoveDock(resolvedId)) return false;
    auto *widget = dockWidget(resolvedId);
    if (resolvedId.isEmpty() || !widget) return false;
    const QString title = titles_.value(resolvedId, resolvedId);
    const bool visible = dockOpen(resolvedId);
    const bool pinned = pinned_.value(resolvedId, false);
    const QByteArray beforeState = beginLayoutMutation();
    if (!removeDockWidget(resolvedId)) return false;

    QWidget *previousRoot = containerRootWidget_;
    if (!previousRoot) {
      addDockWidget(resolvedId, title, widget, area);
      return false;
    }
    const QString groupId = QStringLiteral("container-root-%1")
                                .arg(++containerGroupSequence_);
    auto *tabs = createDockTabSurface(area, {}, {}, groupId);
    widget->setParent(tabs);
    installCloseButton(tabs, tabs->addTab(widget, title), resolvedId);
    tabs->setTabVisible(tabs->indexOf(widget),
                        visible && !dockNoTab(resolvedId));
    tabs->setVisible(visible);

    const bool horizontal = area == DockArea::Left || area == DockArea::Right;
    const bool placeFirst = area == DockArea::Left || area == DockArea::Top;
    auto *rootSplitter = createConfiguredDockSplitter(
        horizontal ? Qt::Horizontal : Qt::Vertical, nullptr);
    containerSplitters_.push_back(rootSplitter);
    containerRootLayout_->removeWidget(previousRoot);
    previousRoot->setParent(nullptr);
    if (placeFirst) {
      rootSplitter->addWidget(tabs);
      rootSplitter->addWidget(previousRoot);
    } else {
      rootSplitter->addWidget(previousRoot);
      rootSplitter->addWidget(tabs);
    }
    rootSplitter->setStretchFactor(0, 1);
    rootSplitter->setStretchFactor(1, 1);
    containerRootWidget_ = rootSplitter;
    containerRootLayout_->addWidget(rootSplitter);
    rootSplitter->show();
    if (equalSplitOnInsertion_) {
      equalizeSameOrientationSplitters(
          rootSplitter, rootSplitter->orientation());
    }

    containerGroupRoots_.insert(groupId, tabs);
    docks_.insert(resolvedId, widget);
    areas_.insert(resolvedId, area);
    titles_.insert(resolvedId, title);
    pinned_.insert(resolvedId, pinned);
    syncPinButton(resolvedId, pinned);
    setActiveTabSurface(tabs);
    finishLayoutMutation(beforeState, tr("Dock at container edge"));
    return true;
  }

  bool dockAtFloatingContainerEdge(const QString &dockId,
                                   QDialog *targetDialog, DockArea area,
                                   bool wholeGroup) {
    if (area == DockArea::Center ||
        !canDockAtFloatingContainerEdge(dockId, targetDialog, wholeGroup)) {
      return false;
    }
    auto *layout = qobject_cast<QVBoxLayout *>(targetDialog->layout());
    QWidget *previousRoot = floatingDockRoots_.value(targetDialog, nullptr);
    const int layoutIndex = layout ? layout->indexOf(previousRoot) : -1;
    if (!layout || !previousRoot || layoutIndex < 0) return false;

    beginLayoutMutationBatch(tr("Dock at floating container edge"));
    auto *newTabs = createFloatingDockTabSurface(targetDialog, {}, {});
    if (!newTabs) {
      endLayoutMutationBatch();
      return false;
    }
    newTabs->setParent(targetDialog);
    const bool movedAll = wholeGroup
        ? moveDockTabGroupToTabs(dockId, newTabs, 0)
        : moveDockWidgetToTabAt(dockId, newTabs, 0);
    if (newTabs->count() == 0) {
      newTabs->removeEventFilter(this);
      newTabs->tabBar()->removeEventFilter(this);
      floatingTabOwners_.remove(newTabs);
      areaHideSingleWidgetTitleBar_.remove(newTabs);
      floatingDynamicTabSurfaces_.removeAll(newTabs);
      dockedSplitPaths_.remove(newTabs);
      dockedSplitRatios_.remove(newTabs);
      delete newTabs;
      endLayoutMutationBatch();
      return false;
    }

    const bool horizontal = area == DockArea::Left || area == DockArea::Right;
    const bool placeFirst = area == DockArea::Left || area == DockArea::Top;
    auto *rootSplitter = createConfiguredDockSplitter(
        horizontal ? Qt::Horizontal : Qt::Vertical, targetDialog);
    layout->removeWidget(previousRoot);
    previousRoot->setParent(nullptr);
    layout->insertWidget(layoutIndex, rootSplitter);
    if (placeFirst) {
      rootSplitter->addWidget(newTabs);
      rootSplitter->addWidget(previousRoot);
    } else {
      rootSplitter->addWidget(previousRoot);
      rootSplitter->addWidget(newTabs);
    }
    rootSplitter->setStretchFactor(0, 1);
    rootSplitter->setStretchFactor(1, 1);
    rootSplitter->show();
    if (equalSplitOnInsertion_) {
      equalizeSameOrientationSplitters(
          rootSplitter, rootSplitter->orientation());
    }
    floatingSplitters_.push_back(rootSplitter);
    floatingDockRoots_.insert(targetDialog, rootSplitter);
    refreshDockSplitMetadataNode(rootSplitter, DockArea::Center, {}, {});
    setActiveTabSurface(newTabs);
    endLayoutMutationBatch();
    return movedAll || newTabs->count() > 0;
  }

  void showDockTargetOverlay(const QPoint &position, DockArea fallbackArea) {
    if (!dropPreview_) return;
    hideFloatingDropPreviews();
    clearTabInsertionPreview();
    dropPreview_->setGeometry(rect());
    const DockArea containerArea = containerDropAreaAt(position);
    auto *anchorTabs = tabSurfaceAt(position);
    if (!anchorTabs) {
      anchorTabs = tabsForArea(fallbackArea);
    }
    QRect anchorRect = dropPreviewRectForArea(fallbackArea);
    if (anchorTabs && anchorTabs->isVisible() && !anchorTabs->size().isEmpty()) {
      anchorRect = QRect(anchorTabs->mapTo(this, anchorTabs->rect().topLeft()),
                         anchorTabs->size());
    }
    DockArea areaButton = DockArea::Center;
    const int allowedDropAreas = dockAreaAllowedDropAreasMask(anchorTabs);
    const bool overAreaButton = anchorTabs && dropPreview_->areaTargetAt(
        anchorRect.center(), position, areaButton, allowedDropAreas);
    if (containerArea != DockArea::Center &&
        (!overAreaButton || containerArea != areaButton)) {
      dropPreview_->showContainerTargets(
          containerEdgePreviewRect(containerArea, dragSourceId_),
          containerArea);
      dropPreview_->show();
      dropPreview_->raise();
      return;
    }
    const DockArea anchorArea = anchorTabs
        ? areaForTabs(anchorTabs)
        : fallbackArea;
    const DockArea area = overAreaButton
        ? (areaButton == DockArea::Center ? anchorArea : areaButton)
        : fallbackArea;
    const DockArea selectedButton = overAreaButton
        ? areaButton
        : (area == fallbackArea ? DockArea::Center : area);
    QRect previewRect = dropPreviewRectForArea(area);
    if (selectedButton != DockArea::Center && anchorTabs &&
        anchorTabs->count() > 0) {
      previewRect = splitPreviewRect(anchorRect, selectedButton);
    }
    dropPreview_->showAreaTargets(previewRect,
                                  anchorRect.center(), anchorArea,
                                  selectedButton, allowedDropAreas);
    dropPreview_->show();
    dropPreview_->raise();
  }

  DockArea areaAtDockTargetOverlay(const QPoint &position,
                                   DockArea fallback) const {
    if (!dropPreview_ || !dropPreview_->showsAreaTargets()) {
      return fallback;
    }
    if (dropPreview_->showsContainerTargets() &&
        containerDropAreaAt(position) == DockArea::Center) {
      return fallback;
    }
    return dropPreview_ ? dropPreview_->targetAreaAt(position, fallback)
                        : fallback;
  }

  void hideDropPreview() {
    if (dropPreview_) {
      dropPreview_->hide();
      dropPreview_->showRectPreview();
    }
    hideFloatingDropPreviews();
    clearTabInsertionPreview();
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

  bool placeDockWidgetAtTab(const QString &dockId, QTabWidget *targetTabs,
                            int insertionIndex) {
    const QString resolvedId = resolveDockId(dockId);
    auto *source = dockWidget(resolvedId);
    if (!source || !targetTabs) {
      return false;
    }
    auto *sourceTabs = tabsForWidget(source);
    if (sourceTabs == targetTabs) {
      if (!canMoveDock(resolvedId)) return false;
    } else if (isFloatingTabSurface(targetTabs)) {
      if (!canFloatDock(resolvedId)) return false;
    } else if (!canMoveDock(resolvedId)) {
      return false;
    }
    if (sourceTabs != targetTabs) {
      return moveDockWidgetToTabAt(resolvedId, targetTabs, insertionIndex);
    }

    const int sourceIndex = targetTabs->indexOf(source);
    if (sourceIndex < 0) {
      return false;
    }
    int adjustedInsertionIndex =
        std::clamp(insertionIndex, 0, targetTabs->count());
    if (sourceIndex < adjustedInsertionIndex) {
      --adjustedInsertionIndex;
    }
    adjustedInsertionIndex = std::clamp(
        adjustedInsertionIndex, 0, targetTabs->count() - 1);
    if (adjustedInsertionIndex == sourceIndex) {
      targetTabs->setCurrentWidget(source);
      return true;
    }

    const QByteArray beforeState = beginLayoutMutation();
    const QString title = titles_.value(resolvedId, resolvedId);
    targetTabs->removeTab(sourceIndex);
    installCloseButton(
        targetTabs,
        targetTabs->insertTab(adjustedInsertionIndex, source, title),
        resolvedId);
    targetTabs->setCurrentWidget(source);
    finishLayoutMutation(beforeState, tr("Reorder tab"));
    return true;
  }

  void showTabInsertionPreview(QTabWidget *tabs, int insertionIndex) {
    if (!tabs || !tabs->tabBar()) {
      return;
    }
    if (dropPreview_) {
      dropPreview_->hide();
      dropPreview_->showRectPreview();
    }
    hideFloatingDropPreviews();
    if (dropInsertionPreviewTabs_ && dropInsertionPreviewTabs_ != tabs) {
      static_cast<DockTabSurface *>(dropInsertionPreviewTabs_.data())
          ->clearDropInsertionPreview();
    }
    dropInsertionPreviewTabs_ = tabs;
    static_cast<DockTabSurface *>(tabs)->setDropInsertionIndex(
        std::clamp(insertionIndex, 0, tabs->count()));
  }

  void clearTabInsertionPreview() {
    if (!dropInsertionPreviewTabs_) {
      return;
    }
    static_cast<DockTabSurface *>(dropInsertionPreviewTabs_.data())
        ->clearDropInsertionPreview();
    dropInsertionPreviewTabs_ = nullptr;
  }

  void hideFloatingDropPreviews(QDialog *except = nullptr) {
    for (auto it = floatingDropPreviews_.cbegin();
         it != floatingDropPreviews_.cend(); ++it) {
      if (it.key() != except && it.value()) {
        it.value()->hide();
        it.value()->showRectPreview();
      }
    }
  }

  bool isTabBarPosition(const QTabWidget *tabs,
                        const QPoint &tabBarPosition) const {
    return tabs && tabs->tabBar() && tabs->tabBar()->isVisible() &&
           tabs->tabBar()->rect().contains(tabBarPosition);
  }

  void showFloatingDockPreview(QTabWidget *tabs,
                               const QPoint &positionInTabs) {
    if (!tabs) {
      return;
    }
    const QPoint tabBarPosition =
        tabs->tabBar()->mapFrom(tabs, positionInTabs);
    if (isTabBarPosition(tabs, tabBarPosition)) {
      showTabInsertionPreview(
          tabs, tabInsertionIndex(tabs, tabBarPosition));
    } else {
      auto *dialog = floatingDialogForTabs(tabs);
      auto *preview = floatingDropPreviews_.value(dialog, nullptr);
      if (!dialog || !preview) {
        return;
      }
      if (dropPreview_) {
        dropPreview_->hide();
        dropPreview_->showRectPreview();
      }
      clearTabInsertionPreview();
      hideFloatingDropPreviews(dialog);
      const QRect targetRect(
          tabs->mapTo(dialog, tabs->rect().topLeft()), tabs->size());
      const QPoint dialogPosition = tabs->mapTo(
          dialog, positionInTabs);
      preview->setGeometry(dialog->rect());
      DockArea containerArea = DockArea::Center;
      const bool containerTarget = floatingContainerTargetWins(
          dialog, dialogPosition, containerArea);
      DockArea areaButton = DockArea::Center;
      const bool overAreaButton = preview->areaTargetAt(
          targetRect.center(), dialogPosition, areaButton,
          dockAreaAllowedDropAreasMask(tabs));
      if (containerTarget) {
        showFloatingContainerOverlay(dialog, containerArea);
        return;
      }
      const DockArea anchorArea = areaForTabs(tabs);
      const DockArea selectedButton = overAreaButton
          ? areaButton
          : DockArea::Center;
      const QRect previewRect = selectedButton == DockArea::Center
          ? targetRect
          : splitPreviewRect(targetRect, selectedButton);
      preview->showAreaTargets(previewRect, targetRect.center(),
                               anchorArea, selectedButton,
                               dockAreaAllowedDropAreasMask(tabs));
      preview->show();
      preview->raise();
    }
  }

  void showFloatingContainerOverlay(QDialog *dialog, DockArea area) {
    if (!dialog || area == DockArea::Center) return;
    if (dropPreview_) {
      dropPreview_->hide();
      dropPreview_->showRectPreview();
    }
    clearTabInsertionPreview();
    hideFloatingDropPreviews(dialog);
    auto *preview = floatingDropPreviews_.value(dialog, nullptr);
    if (!preview) return;
    preview->setGeometry(dialog->rect());
    preview->showContainerTargets(
        floatingContainerEdgePreviewRect(dialog, area), area);
    preview->show();
    preview->raise();
  }

  bool isFloatingTabSurface(const QTabWidget *tabs) const {
    return tabs &&
           floatingTabOwners_.contains(const_cast<QTabWidget *>(tabs));
  }

  QTabWidget *floatingTabAt(QDialog *dialog, const QPoint &position) const {
    if (!dialog) {
      return nullptr;
    }
    for (auto it = floatingTabOwners_.cbegin();
         it != floatingTabOwners_.cend(); ++it) {
      auto *tabs = it.key();
      if (it.value() == dialog && tabs && tabs->isVisible() &&
          tabs->rect().contains(tabs->mapFrom(dialog, position))) {
        return tabs;
      }
    }
    return nullptr;
  }

  QList<QTabWidget *> floatingTabSurfacesForDialog(QDialog *dialog) const {
    QList<QTabWidget *> surfaces;
    if (!dialog) {
      return surfaces;
    }
    for (auto it = floatingTabOwners_.cbegin();
         it != floatingTabOwners_.cend(); ++it) {
      if (it.value() == dialog && it.key()) {
        surfaces.push_back(it.key());
      }
    }
    std::sort(surfaces.begin(), surfaces.end(),
              [this](QTabWidget *left, QTabWidget *right) {
                QString leftPath;
                QString rightPath;
                QList<int> leftRatios;
                QList<int> rightRatios;
                splitPathForTabs(left, leftPath, leftRatios);
                splitPathForTabs(right, rightPath, rightRatios);
                return leftPath < rightPath;
              });
    return surfaces;
  }

  QTabWidget *createFloatingDockTabSurface(QDialog *dialog,
                                           const QString &splitPath,
                                           const QList<int> &splitRatios) {
    if (!dialog) {
      return nullptr;
    }
    auto *tabs = createTabSurface(nullptr);
    tabs->setAcceptDrops(true);
    tabs->tabBar()->setAcceptDrops(true);
    tabs->installEventFilter(this);
    tabs->tabBar()->installEventFilter(this);
    if (auto *tabListButton =
            static_cast<DockTabSurface *>(tabs)->tabListButton()) {
      tabListButton->installEventFilter(this);
    }
    floatingTabOwners_.insert(tabs, dialog);
    areaHideSingleWidgetTitleBar_.insert(tabs, false);
    dockedSplitPaths_.insert(tabs, splitPath);
    dockedSplitRatios_.insert(tabs, splitRatios);
    floatingDynamicTabSurfaces_.push_back(tabs);
    return tabs;
  }

  bool dockSplitPathsValid(const QList<DockLayoutEntry> &entries,
                           int depth) const {
    if (entries.isEmpty()) {
      return false;
    }
    const qsizetype offset = static_cast<qsizetype>(depth) * 2;
    bool hasTerminal = false;
    bool hasChild = false;
    for (const auto &entry : entries) {
      if (entry.splitPath.size() == offset) {
        hasTerminal = true;
      } else if (entry.splitPath.size() > offset) {
        hasChild = true;
      } else {
        return false;
      }
    }
    if (hasTerminal) {
      return !hasChild;
    }

    const QChar orientation = entries.front().splitPath.at(offset);
    const int ratio = entries.front().splitRatios.value(depth, 0);
    QList<DockLayoutEntry> firstChild;
    QList<DockLayoutEntry> secondChild;
    for (const auto &entry : entries) {
      if (entry.splitPath.size() <= offset + 1 ||
          entry.splitPath.at(offset) != orientation ||
          entry.splitRatios.value(depth, 0) != ratio) {
        return false;
      }
      if (entry.splitPath.at(offset + 1) == QLatin1Char('0')) {
        firstChild.push_back(entry);
      } else {
        secondChild.push_back(entry);
      }
    }
    return !firstChild.isEmpty() && !secondChild.isEmpty() &&
           dockSplitPathsValid(firstChild, depth + 1) &&
           dockSplitPathsValid(secondChild, depth + 1);
  }

  QWidget *dockAreaRootWidget(DockArea area) const {
    QSplitter *parent = nullptr;
    int index = -1;
    switch (area) {
    case DockArea::Left:
      parent = splitter_;
      index = 0;
      break;
    case DockArea::Center:
      parent = splitter_;
      index = 1;
      break;
    case DockArea::Right:
      parent = splitter_;
      index = 2;
      break;
    case DockArea::Top:
      parent = verticalSplitter_;
      index = 0;
      break;
    case DockArea::Bottom:
      parent = verticalSplitter_;
      index = 2;
      break;
    }
    return parent && index < parent->count() ? parent->widget(index) : nullptr;
  }

  int firstSplitShare(const QSplitter *splitter,
                      int fallback = 5000) const {
    if (!splitter || splitter->count() != 2) {
      return fallback;
    }
    const QList<int> sizes = splitter->sizes();
    const int total = sizes.value(0) + sizes.value(1);
    if (total <= 0) {
      return std::clamp(fallback, 1, 9999);
    }
    const long long share =
        (static_cast<long long>(sizes.value(0)) * 10000 + total / 2) / total;
    return static_cast<int>(std::clamp<long long>(share, 1, 9999));
  }

  bool findDockSplitPath(QWidget *node, QTabWidget *targetTabs,
                         const QString &path, const QList<int> &ratios,
                         QString &resultPath,
                         QList<int> &resultRatios) const {
    if (!node || !targetTabs) {
      return false;
    }
    if (node == targetTabs) {
      resultPath = path;
      resultRatios = ratios;
      return true;
    }
    auto *splitter = qobject_cast<QSplitter *>(node);
    if (!splitter || splitter->count() != 2) {
      return false;
    }
    const QChar orientation = splitter->orientation() == Qt::Horizontal
        ? QLatin1Char('H')
        : QLatin1Char('V');
    const int depth = ratios.size();
    auto *firstTabs = firstDockTabSurface(splitter->widget(0));
    const int fallback = firstTabs
        ? dockedSplitRatios_.value(firstTabs).value(depth, 5000)
        : 5000;
    const int ratio = firstSplitShare(splitter, fallback);
    for (int childIndex = 0; childIndex < 2; ++childIndex) {
      QString childPath = path;
      childPath += orientation;
      childPath += childIndex == 0 ? QLatin1Char('0') : QLatin1Char('1');
      QList<int> childRatios = ratios;
      childRatios.push_back(ratio);
      if (findDockSplitPath(splitter->widget(childIndex), targetTabs,
                            childPath, childRatios, resultPath,
                            resultRatios)) {
        return true;
      }
    }
    return false;
  }

  bool splitPathForTabs(QTabWidget *tabs, QString &path,
                        QList<int> &ratios) const {
    if (!tabs) {
      return false;
    }
    QWidget *root = nullptr;
    if (auto *dialog = floatingTabOwners_.value(tabs, nullptr)) {
      root = floatingDockRoots_.value(dialog, nullptr);
    } else {
      const QString groupId = dockedContainerGroupIds_.value(
          tabs, QStringLiteral("base"));
      root = groupId == QStringLiteral("base")
          ? dockAreaRootWidget(areaForTabs(tabs))
          : containerGroupRoots_.value(groupId, nullptr);
    }
    return findDockSplitPath(root, tabs, {}, {}, path, ratios);
  }

  bool findContainerGroupPath(QWidget *node, QWidget *target,
                              const QString &path,
                              const QList<int> &ratios,
                              QString &resultPath,
                              QList<int> &resultRatios) const {
    if (!node || !target) return false;
    if (node == target) {
      resultPath = path;
      resultRatios = ratios;
      return true;
    }
    auto *splitter = qobject_cast<QSplitter *>(node);
    if (!splitter || !containerSplitters_.contains(splitter) ||
        splitter->count() != 2) {
      return false;
    }
    const QChar orientation = splitter->orientation() == Qt::Horizontal
        ? QLatin1Char('H')
        : QLatin1Char('V');
    const int ratio = firstSplitShare(splitter);
    for (int childIndex = 0; childIndex < 2; ++childIndex) {
      QString childPath = path;
      childPath += orientation;
      childPath += childIndex == 0 ? QLatin1Char('0') : QLatin1Char('1');
      QList<int> childRatios = ratios;
      childRatios.push_back(ratio);
      if (findContainerGroupPath(splitter->widget(childIndex), target,
                                  childPath, childRatios, resultPath,
                                  resultRatios)) {
        return true;
      }
    }
    return false;
  }

  bool containerSplitPathForGroup(const QString &groupId, QString &path,
                                  QList<int> &ratios) const {
    auto *root = containerGroupRoots_.value(groupId, nullptr);
    if (!root) return false;
    return findContainerGroupPath(containerRootWidget_, root, {}, {}, path,
                                  ratios);
  }

  QWidget *containerSplitPathNode(const QString &path,
                                  QSplitter *&parentSplitter,
                                  int &parentIndex) const {
    QWidget *node = containerRootWidget_;
    parentSplitter = nullptr;
    parentIndex = -1;
    for (qsizetype offset = 0; offset < path.size(); offset += 2) {
      auto *splitter = qobject_cast<QSplitter *>(node);
      if (!splitter || !containerSplitters_.contains(splitter) ||
          splitter->count() != 2) {
        return nullptr;
      }
      const int childIndex = path.at(offset + 1) == QLatin1Char('0') ? 0 : 1;
      parentSplitter = splitter;
      parentIndex = childIndex;
      node = splitter->widget(childIndex);
      if (!node) return nullptr;
    }
    return node;
  }

  bool restoreDockAtContainerPath(const QString &dockId,
                                  const QString &title, QWidget *widget,
                                  DockArea area, const QString &path,
                                  const QList<int> &ratios, bool visible,
                                  bool pinned, const QString &groupId) {
    if (path.size() < 2 || path.size() % 2 != 0 ||
        ratios.size() != path.size() / 2 || groupId.isEmpty() ||
        groupId == QStringLiteral("base")) {
      return false;
    }
    const QString parentPath = path.left(path.size() - 2);
    QSplitter *parentSplitter = nullptr;
    int parentIndex = -1;
    QWidget *parentNode = containerSplitPathNode(
        parentPath, parentSplitter, parentIndex);
    if (!parentNode) return false;
    if (parentSplitter && parentIndex < 0) return false;

    const bool horizontal = path.at(path.size() - 2) == QLatin1Char('H');
    const int groupSide = path.at(path.size() - 1) == QLatin1Char('0') ? 0 : 1;
    const int share = std::clamp(ratios.back(), 1, 9999);
    const QList<int> parentSizes = parentSplitter
        ? parentSplitter->sizes()
        : QList<int>{};
    auto *splitter = createConfiguredDockSplitter(
        horizontal ? Qt::Horizontal : Qt::Vertical, nullptr);
    containerSplitters_.push_back(splitter);
    if (parentSplitter) {
      QWidget *replaced = parentSplitter->replaceWidget(parentIndex, splitter);
      if (replaced != parentNode) {
        containerSplitters_.removeAll(splitter);
        delete splitter;
        return false;
      }
    } else {
      if (containerRootWidget_ != parentNode || !containerRootLayout_ ||
          containerRootLayout_->indexOf(parentNode) < 0) {
        containerSplitters_.removeAll(splitter);
        delete splitter;
        return false;
      }
      containerRootLayout_->removeWidget(parentNode);
      parentNode->setParent(nullptr);
    }
    auto *tabs = createDockTabSurface(area, {}, {}, groupId);
    widget->setParent(tabs);
    const int tabIndex = tabs->addTab(widget, title);
    installCloseButton(tabs, tabIndex, dockId);
    tabs->setTabVisible(tabIndex, visible && !dockNoTab(dockId));
    tabs->setVisible(visible);
    if (groupSide == 0) {
      splitter->addWidget(tabs);
      splitter->addWidget(parentNode);
    } else {
      splitter->addWidget(parentNode);
      splitter->addWidget(tabs);
    }
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 1);
    splitter->setSizes(groupSide == 0
                           ? QList<int>{share, 10000 - share}
                           : QList<int>{10000 - share, share});
    if (parentSplitter && parentSizes.size() == parentSplitter->count()) {
      parentSplitter->setSizes(parentSizes);
    }
    if (!parentSplitter) {
      containerRootWidget_ = splitter;
      containerRootLayout_->addWidget(splitter);
    }
    splitter->show();
    containerGroupRoots_.insert(groupId, tabs);
    containerGroupAreas_.insert(groupId, area);
    docks_.insert(dockId, widget);
    areas_.insert(dockId, area);
    titles_.insert(dockId, title);
    pinned_.insert(dockId, pinned);
    syncPinButton(dockId, pinned);
    setActiveTabSurface(tabs);
    return true;
  }

  QWidget *dockSplitPathNode(DockArea area, const QString &path,
                            QSplitter *&parentSplitter,
                            int &parentIndex) const {
    QWidget *node = dockAreaRootWidget(area);
    parentSplitter = area == DockArea::Top || area == DockArea::Bottom
        ? verticalSplitter_
        : splitter_;
    parentIndex = node && parentSplitter
        ? parentSplitter->indexOf(node)
        : -1;
    if (!node || parentIndex < 0) return nullptr;
    for (qsizetype offset = 0; offset < path.size(); offset += 2) {
      auto *splitter = qobject_cast<QSplitter *>(node);
      if (!splitter || splitter->count() != 2) return nullptr;
      const int childIndex = path.at(offset + 1) == QLatin1Char('0') ? 0 : 1;
      parentSplitter = splitter;
      parentIndex = childIndex;
      node = splitter->widget(childIndex);
      if (!node) return nullptr;
    }
    return node;
  }

  QWidget *dockSplitPathNode(const QString &groupId, DockArea area,
                             const QString &path,
                             QSplitter *&parentSplitter,
                             int &parentIndex) const {
    if (groupId == QStringLiteral("base")) {
      return dockSplitPathNode(area, path, parentSplitter, parentIndex);
    }
    QWidget *node = containerGroupRoots_.value(groupId, nullptr);
    parentSplitter = node ? qobject_cast<QSplitter *>(node->parentWidget())
                          : nullptr;
    parentIndex = parentSplitter ? parentSplitter->indexOf(node) : -1;
    if (!node || !parentSplitter || parentIndex < 0) return nullptr;
    for (qsizetype offset = 0; offset < path.size(); offset += 2) {
      auto *splitter = qobject_cast<QSplitter *>(node);
      if (!splitter || !localSplitters_.contains(splitter) ||
          splitter->count() != 2) {
        return nullptr;
      }
      const int childIndex = path.at(offset + 1) == QLatin1Char('0') ? 0 : 1;
      parentSplitter = splitter;
      parentIndex = childIndex;
      node = splitter->widget(childIndex);
      if (!node) return nullptr;
    }
    return node;
  }

  bool restoreDockAtSplitPath(const QString &dockId, const QString &title,
                              QWidget *widget, DockArea area,
                              const QString &path,
                              const QList<int> &ratios, bool visible,
                              bool pinned,
                              const QString &containerGroupId =
                                  QStringLiteral("base")) {
    if (path.size() < 2 || path.size() % 2 != 0 ||
        ratios.size() != path.size() / 2) {
      return false;
    }
    const QString parentPath = path.left(path.size() - 2);
    QSplitter *parentSplitter = nullptr;
    int parentIndex = -1;
    QWidget *parentNode = dockSplitPathNode(
        containerGroupId, area, parentPath, parentSplitter, parentIndex);
    if (!parentNode || !parentSplitter || parentIndex < 0) return false;

    const QChar orientationCode = path.at(path.size() - 2);
    const bool horizontal = orientationCode == QLatin1Char('H');
    const int sourceSide =
        path.at(path.size() - 1) == QLatin1Char('0') ? 0 : 1;
    const int share = std::clamp(ratios.back(), 1, 9999);
    const QList<int> parentSizes = parentSplitter->sizes();
    auto *splitter = createConfiguredDockSplitter(
        horizontal ? Qt::Horizontal : Qt::Vertical, nullptr);
    QWidget *replaced = parentSplitter->replaceWidget(parentIndex, splitter);
    if (replaced != parentNode) {
      delete splitter;
      return false;
    }
    localSplitters_.push_back(splitter);
    const bool replacesGroupRoot =
        containerGroupRoots_.value(containerGroupId) == parentNode;
    auto *tabs = createDockTabSurface(area, path, ratios);
    if (sourceSide == 0) {
      splitter->addWidget(tabs);
      splitter->addWidget(parentNode);
    } else {
      splitter->addWidget(parentNode);
      splitter->addWidget(tabs);
    }
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 1);
    splitter->setSizes(sourceSide == 0
                           ? QList<int>{share, 10000 - share}
                           : QList<int>{10000 - share, share});
    splitter->show();
    if (replacesGroupRoot) {
      containerGroupRoots_.insert(containerGroupId, splitter);
    }
    if (parentSizes.size() == parentSplitter->count()) {
      parentSplitter->setSizes(parentSizes);
    }
    widget->setParent(tabs);
    const int index = tabs->addTab(widget, title);
    installCloseButton(tabs, index, dockId);
    tabs->setTabVisible(index, visible && !dockNoTab(dockId));
    tabs->show();
    docks_.insert(dockId, widget);
    areas_.insert(dockId, area);
    titles_.insert(dockId, title);
    pinned_.insert(dockId, pinned);
    syncPinButton(dockId, pinned);
    setActiveTabSurface(tabs);
    refreshDockSplitMetadata(area);
    return true;
  }

  void refreshDockSplitMetadataNode(QWidget *node, DockArea area,
                                    const QString &path,
                                    const QList<int> &ratios) {
    if (!node) {
      return;
    }
    if (auto *tabs = qobject_cast<QTabWidget *>(node)) {
      dockedTabAreas_.insert(tabs, area);
      dockedSplitPaths_.insert(tabs, path);
      dockedSplitRatios_.insert(tabs, ratios);
      return;
    }
    auto *splitter = qobject_cast<QSplitter *>(node);
    if (!splitter || splitter->count() != 2) {
      return;
    }
    const QChar orientation = splitter->orientation() == Qt::Horizontal
        ? QLatin1Char('H')
        : QLatin1Char('V');
    const int depth = ratios.size();
    auto *firstTabs = firstDockTabSurface(splitter->widget(0));
    const int fallback = firstTabs
        ? dockedSplitRatios_.value(firstTabs).value(depth, 5000)
        : 5000;
    const int ratio = firstSplitShare(splitter, fallback);
    for (int childIndex = 0; childIndex < 2; ++childIndex) {
      QString childPath = path;
      childPath += orientation;
      childPath += childIndex == 0 ? QLatin1Char('0') : QLatin1Char('1');
      QList<int> childRatios = ratios;
      childRatios.push_back(ratio);
      refreshDockSplitMetadataNode(splitter->widget(childIndex), area,
                                   childPath, childRatios);
    }
  }

  int sameOrientationDockSplitLeafCount(
      const QWidget *node, Qt::Orientation orientation) const {
    auto *splitter = qobject_cast<const QSplitter *>(node);
    if (!splitter || splitter->orientation() != orientation) return 1;
    int count = 0;
    for (int index = 0; index < splitter->count(); ++index) {
      count += sameOrientationDockSplitLeafCount(
          splitter->widget(index), orientation);
    }
    return std::max(1, count);
  }

  void equalizeSameOrientationSplitters(QSplitter *splitter,
                                        Qt::Orientation orientation) {
    if (!splitter || splitter->orientation() != orientation ||
        splitter->count() < 2) {
      return;
    }
    QList<int> sizes;
    for (int index = 0; index < splitter->count(); ++index) {
      sizes.push_back(sameOrientationDockSplitLeafCount(
          splitter->widget(index), orientation));
    }
    splitter->setSizes(sizes);
    for (int index = 0; index < splitter->count(); ++index) {
      equalizeSameOrientationSplitters(
          qobject_cast<QSplitter *>(splitter->widget(index)), orientation);
    }
  }

  void refreshDockSplitMetadata(DockArea area) {
    refreshDockSplitMetadataNode(dockAreaRootWidget(area), area, {}, {});
    for (auto it = containerGroupRoots_.cbegin();
         it != containerGroupRoots_.cend(); ++it) {
      if (it.key() == QStringLiteral("base") ||
          containerGroupAreas_.value(it.key(), DockArea::Center) != area) {
        continue;
      }
      refreshDockSplitMetadataNode(it.value(), area, {}, {});
    }
  }

  QTabWidget *firstDockTabSurface(QWidget *node) const {
    if (!node) {
      return nullptr;
    }
    if (auto *tabs = qobject_cast<QTabWidget *>(node)) {
      return dockedTabAreas_.contains(tabs) ||
                     floatingTabOwners_.contains(tabs)
          ? tabs
          : nullptr;
    }
    auto *splitter = qobject_cast<QSplitter *>(node);
    if (!splitter) {
      return nullptr;
    }
    for (int index = 0; index < splitter->count(); ++index) {
      if (auto *tabs = firstDockTabSurface(splitter->widget(index))) {
        return tabs;
      }
    }
    return nullptr;
  }

  bool floatingDialogHasVisibleTabs(QDialog *dialog) const {
    if (!dialog) {
      return false;
    }
    for (auto it = floatingTabOwners_.cbegin();
         it != floatingTabOwners_.cend(); ++it) {
      auto *tabs = it.key();
      if (it.value() != dialog || !tabs) {
        continue;
      }
      for (int index = 0; index < tabs->count(); ++index) {
        const QString dockId = dockIdForWidget(tabs->widget(index));
        if (!dockId.isEmpty() && dockOpen(dockId)) {
          return true;
        }
      }
    }
    return false;
  }

  void destroyFloatingDialogIfEmpty(QDialog *dialog) {
    if (!dialog) {
      return;
    }
    for (auto it = floatingDialogs_.cbegin();
         it != floatingDialogs_.cend(); ++it) {
      if (it.value() == dialog) {
        return;
      }
    }
    floatingGroupIds_.remove(dialog);
    floatingTabSurfaces_.remove(dialog);
    floatingDockRoots_.remove(dialog);
    floatingDropPreviews_.remove(dialog);
    for (auto it = floatingTabOwners_.begin();
         it != floatingTabOwners_.end();) {
      if (it.value() == dialog) {
        dockedSplitPaths_.remove(it.key());
        dockedSplitRatios_.remove(it.key());
        dockedTabAreas_.remove(it.key());
        areaHideSingleWidgetTitleBar_.remove(it.key());
        floatingDynamicTabSurfaces_.removeAll(it.key());
        it = floatingTabOwners_.erase(it);
      } else {
        ++it;
      }
    }
    for (auto it = floatingSplitters_.begin();
         it != floatingSplitters_.end();) {
      if (*it && dialog->isAncestorOf(*it)) {
        it = floatingSplitters_.erase(it);
      } else {
        ++it;
      }
    }
    dialog->hide();
    dialog->deleteLater();
  }

  QString ensureFloatingGroupId(QDialog *dialog) {
    if (!dialog) return {};
    const QString existing = floatingGroupIds_.value(dialog);
    if (!existing.isEmpty()) return existing;
    QString groupId;
    do {
      groupId = QStringLiteral("native-floating-group-%1")
                    .arg(++floatingGroupSequence_);
    } while (floatingGroupIds_.values().contains(groupId));
    floatingGroupIds_.insert(dialog, groupId);
    return groupId;
  }

  static QString floatingGroupRestoreKey(const DockLayoutEntry &entry) {
    if (!entry.floatingGroupId.isEmpty()) {
      return QStringLiteral("id:") + entry.floatingGroupId;
    }
    if (entry.tabGroup.startsWith(QStringLiteral("floating-tabs:"))) {
      return QStringLiteral("legacy:") + entry.tabGroup;
    }
    return {};
  }

  void collapseEmptyFloatingDockSurface(QTabWidget *tabs,
                                       QDialog *dialog) {
    if (!tabs || !dialog || tabs->count() != 0 ||
        floatingTabOwners_.value(tabs) != dialog) {
      return;
    }
    auto *parentSplitter = qobject_cast<QSplitter *>(tabs->parentWidget());
    if (!parentSplitter || !floatingSplitters_.contains(parentSplitter) ||
        parentSplitter->count() != 2) {
      return;
    }
    const int tabIndex = parentSplitter->indexOf(tabs);
    QWidget *sibling = tabIndex == 0 ? parentSplitter->widget(1)
                                    : parentSplitter->widget(0);
    if (tabIndex < 0 || !sibling) {
      return;
    }
    auto *grandparent = qobject_cast<QSplitter *>(parentSplitter->parentWidget());
    const bool replacesRoot = floatingDockRoots_.value(dialog) == parentSplitter;
    const bool wasActive = activeTabs_ == tabs;
    QList<int> grandparentSizes;
    int parentIndex = -1;
    QVBoxLayout *dialogLayout = nullptr;
    int layoutIndex = -1;
    if (grandparent) {
      parentIndex = grandparent->indexOf(parentSplitter);
      grandparentSizes = grandparent->sizes();
      if (parentIndex < 0) {
        return;
      }
    } else if (replacesRoot && parentSplitter->parentWidget() == dialog) {
      dialogLayout = qobject_cast<QVBoxLayout *>(dialog->layout());
      layoutIndex = dialogLayout ? dialogLayout->indexOf(parentSplitter) : -1;
      if (!dialogLayout || layoutIndex < 0) {
        return;
      }
    } else {
      return;
    }

    tabs->setParent(nullptr);
    sibling->setParent(nullptr);
    if (grandparent) {
      grandparent->insertWidget(parentIndex, sibling);
      parentSplitter->setParent(nullptr);
      if (grandparentSizes.size() == grandparent->count()) {
        grandparent->setSizes(grandparentSizes);
      }
    } else {
      dialogLayout->removeWidget(parentSplitter);
      parentSplitter->setParent(nullptr);
      sibling->setParent(dialog);
      dialogLayout->insertWidget(layoutIndex, sibling);
      floatingDockRoots_.insert(dialog, sibling);
    }
    floatingSplitters_.removeAll(parentSplitter);
    delete parentSplitter;

    tabs->removeEventFilter(this);
    tabs->tabBar()->removeEventFilter(this);
    floatingTabOwners_.remove(tabs);
    areaHideSingleWidgetTitleBar_.remove(tabs);
    floatingDynamicTabSurfaces_.removeAll(tabs);
    dockedSplitPaths_.remove(tabs);
    dockedSplitRatios_.remove(tabs);
    dockedTabAreas_.remove(tabs);
    const bool wasPrimary = floatingTabSurfaces_.value(dialog) == tabs;
    delete tabs;
    if (wasPrimary) {
      floatingTabSurfaces_.insert(
          dialog, firstDockTabSurface(floatingDockRoots_.value(dialog)));
    }
    if (replacesRoot && grandparent) {
      floatingDockRoots_.insert(dialog, sibling);
    }
    refreshDockSplitMetadataNode(floatingDockRoots_.value(dialog),
                                 DockArea::Center, {}, {});
    if (wasActive) {
      if (auto *fallbackTabs = firstDockTabSurface(sibling)) {
        setActiveTabSurface(fallbackTabs);
      }
    }
  }

  void collapseEmptyDockSurface(QTabWidget *tabs, DockArea area) {
    if (!tabs || !dynamicTabSurfaces_.contains(tabs)) {
      return;
    }
    const QString groupId = dockedContainerGroupIds_.value(
        tabs, QStringLiteral("base"));
    auto *parentSplitter = qobject_cast<QSplitter *>(tabs->parentWidget());
    if (!parentSplitter || parentSplitter->count() != 2) return;
    const int tabIndex = parentSplitter->indexOf(tabs);
    QWidget *sibling = tabIndex == 0 ? parentSplitter->widget(1)
                                    : parentSplitter->widget(0);
    const bool wasActive = activeTabs_ == tabs;

    if (containerSplitters_.contains(parentSplitter)) {
      if (tabIndex < 0 || !sibling) return;
      auto *grandparent = qobject_cast<QSplitter *>(
          parentSplitter->parentWidget());
      const int parentIndex = grandparent
          ? grandparent->indexOf(parentSplitter)
          : -1;
      const bool replacesRoot = containerRootWidget_ == parentSplitter;
      const QList<int> parentSizes = grandparent
          ? grandparent->sizes()
          : QList<int>{};
      if (grandparent && !containerSplitters_.contains(grandparent)) return;
      if (!grandparent &&
          (!replacesRoot || parentSplitter->parentWidget() !=
                                containerRootHost_ ||
           !containerRootLayout_ ||
           containerRootLayout_->indexOf(parentSplitter) < 0)) {
        return;
      }

      tabs->setParent(nullptr);
      sibling->setParent(nullptr);
      if (grandparent) {
        grandparent->insertWidget(parentIndex, sibling);
        parentSplitter->setParent(nullptr);
        if (parentSizes.size() == grandparent->count()) {
          grandparent->setSizes(parentSizes);
        }
      } else {
        containerRootLayout_->removeWidget(parentSplitter);
        parentSplitter->setParent(nullptr);
        containerRootLayout_->addWidget(sibling);
      }
      containerSplitters_.removeAll(parentSplitter);
      delete parentSplitter;
      if (replacesRoot) containerRootWidget_ = sibling;

      tabs->removeEventFilter(this);
      tabs->tabBar()->removeEventFilter(this);
      dockedTabSurfaces_.removeAll(tabs);
      dynamicTabSurfaces_.removeAll(tabs);
      dockedTabAreas_.remove(tabs);
      areaHideSingleWidgetTitleBar_.remove(tabs);
      dockedSplitPaths_.remove(tabs);
      dockedSplitRatios_.remove(tabs);
      dockedContainerGroupIds_.remove(tabs);
      containerGroupRoots_.remove(groupId);
      containerGroupAreas_.remove(groupId);
      delete tabs;
      if (wasActive) {
        if (auto *fallbackTabs = firstDockTabSurface(sibling)) {
          setActiveTabSurface(fallbackTabs);
        }
      }
      return;
    }

    if (!localSplitters_.contains(parentSplitter) || tabIndex < 0 ||
        !sibling) {
      return;
    }
    auto *grandparent = qobject_cast<QSplitter *>(parentSplitter->parentWidget());
    const int parentIndex = grandparent ? grandparent->indexOf(parentSplitter) : -1;
    if (!grandparent || parentIndex < 0) return;
    const QList<int> parentSizes = grandparent->sizes();

    tabs->setParent(nullptr);
    sibling->setParent(nullptr);
    grandparent->insertWidget(parentIndex, sibling);
    parentSplitter->setParent(nullptr);
    localSplitters_.removeAll(parentSplitter);
    delete parentSplitter;

    tabs->removeEventFilter(this);
    tabs->tabBar()->removeEventFilter(this);
    dockedTabSurfaces_.removeAll(tabs);
    dynamicTabSurfaces_.removeAll(tabs);
    dockedTabAreas_.remove(tabs);
    areaHideSingleWidgetTitleBar_.remove(tabs);
    dockedSplitPaths_.remove(tabs);
    dockedSplitRatios_.remove(tabs);
    dockedContainerGroupIds_.remove(tabs);
    if (containerGroupRoots_.value(groupId) == parentSplitter) {
      containerGroupRoots_.insert(groupId, sibling);
    }
    delete tabs;
    if (parentSizes.size() == grandparent->count()) {
      grandparent->setSizes(parentSizes);
    }
    refreshDockSplitMetadata(area);
    if (wasActive) {
      if (auto *fallbackTabs = firstDockTabSurface(sibling)) {
        setActiveTabSurface(fallbackTabs);
      }
    }
  }

  void normalizeDockSplitPaths(QList<DockLayoutEntry> &entries) const {
    QSet<QString> dockedGroups;
    const auto groupKey = [](const DockLayoutEntry &entry) {
      return QString::number(static_cast<int>(entry.area)) +
             QLatin1Char('\n') + entry.containerSplitPath;
    };
    for (const auto &entry : entries) {
      if (!entry.floating) dockedGroups.insert(groupKey(entry));
    }
    for (const QString &key : dockedGroups) {
      QList<DockLayoutEntry> groupEntries;
      for (const auto &entry : entries) {
        if (!entry.floating && groupKey(entry) == key) {
          groupEntries.push_back(entry);
        }
      }
      const auto topology = std::find_if(
          groupEntries.cbegin(), groupEntries.cend(),
          [](const DockLayoutEntry &entry) {
            return !entry.splitPath.isEmpty();
          });
      if (topology != groupEntries.cend()) {
        for (auto &entry : entries) {
          if (!entry.floating && groupKey(entry) == key &&
              entry.splitPath.isEmpty()) {
            entry.splitPath = topology->splitPath;
            entry.splitRatios = topology->splitRatios;
            entry.tabGroup = topology->tabGroup;
          }
        }
        groupEntries.clear();
        for (const auto &entry : entries) {
          if (!entry.floating && groupKey(entry) == key) {
            groupEntries.push_back(entry);
          }
        }
      }
      if (dockSplitPathsValid(groupEntries, 0)) continue;
      for (auto &entry : entries) {
        if (!entry.floating && groupKey(entry) == key) {
          entry.splitPath.clear();
          entry.splitRatios.clear();
        }
      }
    }

    QSet<QString> containerGroups;
    for (const auto &entry : entries) {
      if (!entry.floating) containerGroups.insert(entry.containerSplitPath);
    }
    QList<DockLayoutEntry> containerEntries;
    bool containerTopologyConsistent = true;
    for (const QString &path : containerGroups) {
      const auto it = std::find_if(
          entries.cbegin(), entries.cend(), [&path](const DockLayoutEntry &entry) {
            return !entry.floating && entry.containerSplitPath == path;
          });
      if (it == entries.cend()) continue;
      DockLayoutEntry containerEntry = *it;
      containerEntry.splitPath = path;
      containerEntry.splitRatios = it->containerSplitRatios;
      for (const auto &entry : entries) {
        if (!entry.floating && entry.containerSplitPath == path &&
            (entry.containerSplitRatios != it->containerSplitRatios ||
             (!path.isEmpty() && entry.area != it->area))) {
          containerTopologyConsistent = false;
        }
      }
      containerEntries.push_back(containerEntry);
    }
    if (!containerTopologyConsistent ||
        (!containerEntries.isEmpty() &&
         !dockSplitPathsValid(containerEntries, 0))) {
      for (auto &entry : entries) {
        if (!entry.floating) {
          entry.containerSplitPath.clear();
          entry.containerSplitRatios.clear();
        }
      }
    }
    QHash<QString, QList<DockLayoutEntry>> floatingGroups;
    for (const auto &entry : entries) {
      if (entry.floating) {
        const QString group = entry.tabGroup.isEmpty()
            ? QStringLiteral("floating-single:") + entry.dockId
            : entry.tabGroup;
        floatingGroups[group].push_back(entry);
      }
    }
    for (auto it = floatingGroups.cbegin(); it != floatingGroups.cend(); ++it) {
      auto groupEntries = it.value();
      const auto topology = std::find_if(
          groupEntries.cbegin(), groupEntries.cend(),
          [](const DockLayoutEntry &entry) {
            return !entry.splitPath.isEmpty();
          });
      if (topology != groupEntries.cend()) {
        for (auto &entry : entries) {
          if (entry.floating &&
              (entry.tabGroup.isEmpty()
                   ? QStringLiteral("floating-single:") + entry.dockId
                   : entry.tabGroup) == it.key() &&
              entry.splitPath.isEmpty()) {
            entry.splitPath = topology->splitPath;
            entry.splitRatios = topology->splitRatios;
          }
        }
        groupEntries.clear();
        for (const auto &entry : entries) {
          const QString group = entry.tabGroup.isEmpty()
              ? QStringLiteral("floating-single:") + entry.dockId
              : entry.tabGroup;
          if (entry.floating && group == it.key()) {
            groupEntries.push_back(entry);
          }
        }
      }
      if (dockSplitPathsValid(groupEntries, 0)) {
        continue;
      }
      for (auto &entry : entries) {
        const QString group = entry.tabGroup.isEmpty()
            ? QStringLiteral("floating-single:") + entry.dockId
            : entry.tabGroup;
        if (entry.floating && group == it.key()) {
          entry.splitPath.clear();
          entry.splitRatios.clear();
        }
      }
    }
  }

  void resetDockSplitLayout() {
    hideDropPreview();
    for (auto *tabs : dockedTabSurfaces_) {
      if (!tabs) continue;
      while (tabs->count() > 0) {
        QWidget *widget = tabs->widget(tabs->count() - 1);
        tabs->removeTab(tabs->count() - 1);
        if (widget) widget->setParent(nullptr);
      }
      tabs->removeEventFilter(this);
      tabs->tabBar()->removeEventFilter(this);
      tabs->setParent(nullptr);
    }
    splitter_->setParent(nullptr);
    if (containerRootLayout_) {
      containerRootLayout_->removeWidget(containerRootWidget_);
    }
    verticalSplitter_->setParent(nullptr);
    for (auto *splitter : localSplitters_) {
      if (splitter) splitter->setParent(nullptr);
    }
    for (auto *splitter : containerSplitters_) {
      if (splitter) splitter->setParent(nullptr);
    }
    for (auto *tabs : dynamicTabSurfaces_) {
      if (tabs) tabs->setParent(nullptr);
    }
    for (auto *splitter : localSplitters_) {
      delete splitter;
    }
    for (auto *splitter : containerSplitters_) {
      delete splitter;
    }
    for (auto *tabs : dynamicTabSurfaces_) {
      delete tabs;
    }
    localSplitters_.clear();
    containerSplitters_.clear();
    dynamicTabSurfaces_.clear();
    dockedTabSurfaces_.clear();
    dockedTabAreas_.clear();
    areaHideSingleWidgetTitleBar_.clear();
    dockedSplitPaths_.clear();
    dockedSplitRatios_.clear();
    dockedContainerGroupIds_.clear();
    containerGroupRoots_.clear();
    containerGroupAreas_.clear();
    containerRootWidget_ = verticalSplitter_;
    containerGroupRoots_.insert(QStringLiteral("base"), verticalSplitter_);
    if (containerRootLayout_) {
      containerRootLayout_->addWidget(verticalSplitter_);
    }

    registerDockTabSurface(leftTabs_, DockArea::Left);
    registerDockTabSurface(rightTabs_, DockArea::Right);
    registerDockTabSurface(topTabs_, DockArea::Top);
    registerDockTabSurface(bottomTabs_, DockArea::Bottom);
    registerDockTabSurface(centerTabs_, DockArea::Center);
    for (const DockArea area : {DockArea::Left, DockArea::Right,
                                DockArea::Top, DockArea::Bottom,
                                DockArea::Center}) {
      if (auto *tabs = tabsForArea(area)) {
        tabs->setTabPosition(areaTabPositionAtBottom(area)
                                 ? QTabWidget::South
                                 : QTabWidget::North);
      }
    }

    splitter_->addWidget(leftTabs_);
    splitter_->addWidget(centerTabs_);
    splitter_->addWidget(rightTabs_);
    splitter_->setStretchFactor(0, 1);
    splitter_->setStretchFactor(1, 3);
    splitter_->setStretchFactor(2, 1);
    verticalSplitter_->addWidget(topTabs_);
    verticalSplitter_->addWidget(splitter_);
    verticalSplitter_->addWidget(bottomTabs_);
    verticalSplitter_->setStretchFactor(0, 1);
    verticalSplitter_->setStretchFactor(1, 3);
    verticalSplitter_->setStretchFactor(2, 1);
    topTabs_->hide();
    bottomTabs_->hide();
  }

  QWidget *buildDockSplitNode(DockArea area,
                              const QList<DockLayoutEntry> &entries,
                              int depth, QTabWidget *rootTabs,
                              int &leafIndex,
                              QDialog *floatingDialog = nullptr,
                              const QString &containerGroupId =
                                  QStringLiteral("base")) {
    if (entries.isEmpty()) return nullptr;
    const qsizetype offset = static_cast<qsizetype>(depth) * 2;
    if (entries.front().splitPath.size() == offset) {
      const auto &firstEntry = entries.front();
      auto *tabs = leafIndex++ == 0
          ? rootTabs
          : floatingDialog
                ? createFloatingDockTabSurface(floatingDialog,
                                               firstEntry.splitPath,
                                               firstEntry.splitRatios)
                : createDockTabSurface(area, firstEntry.splitPath,
                                       firstEntry.splitRatios,
                                       containerGroupId);
      dockedSplitPaths_.insert(tabs, firstEntry.splitPath);
      dockedSplitRatios_.insert(tabs, firstEntry.splitRatios);
      bool anyVisible = false;
      QWidget *activeWidget = nullptr;
      for (const auto &entry : entries) {
        auto *widget = floatingDialog
            ? floatingWidgets_.value(entry.dockId, nullptr)
            : docks_.value(entry.dockId, nullptr);
        if (!widget) continue;
        widget->setParent(tabs);
        const int index = tabs->addTab(
            widget, titles_.value(entry.dockId, entry.dockId));
        installCloseButton(tabs, index, entry.dockId);
        tabs->setTabVisible(index, entry.visible && !entry.noTab);
        pinned_.insert(entry.dockId, entry.pinned);
        syncPinButton(entry.dockId, entry.pinned);
        areas_.insert(entry.dockId, area);
        anyVisible |= entry.visible;
        if (entry.active && entry.visible) activeWidget = widget;
      }
      if (activeWidget) {
        tabs->setCurrentWidget(activeWidget);
        setActiveTabSurface(tabs);
      }
      tabs->setVisible(anyVisible);
      return tabs;
    }

    const QChar orientation = entries.front().splitPath.at(offset);
    const int ratio = entries.front().splitRatios.value(depth, 5000);
    QList<DockLayoutEntry> firstChild;
    QList<DockLayoutEntry> secondChild;
    for (const auto &entry : entries) {
      if (entry.splitPath.at(offset + 1) == QLatin1Char('0')) {
        firstChild.push_back(entry);
      } else {
        secondChild.push_back(entry);
      }
    }
    auto *splitter = createConfiguredDockSplitter(
        orientation == QLatin1Char('H') ? Qt::Horizontal : Qt::Vertical,
        nullptr);
    if (floatingDialog) {
      floatingSplitters_.push_back(splitter);
    } else {
      localSplitters_.push_back(splitter);
    }
    QWidget *firstWidget = buildDockSplitNode(
        area, firstChild, depth + 1, rootTabs, leafIndex, floatingDialog,
        containerGroupId);
    QWidget *secondWidget = buildDockSplitNode(
        area, secondChild, depth + 1, rootTabs, leafIndex, floatingDialog,
        containerGroupId);
    if (!firstWidget || !secondWidget) {
      if (floatingDialog) {
        floatingSplitters_.removeAll(splitter);
      } else {
        localSplitters_.removeAll(splitter);
      }
      delete splitter;
      return nullptr;
    }
    splitter->addWidget(firstWidget);
    splitter->addWidget(secondWidget);
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 1);
    splitter->setSizes({ratio, 10000 - ratio});
    return splitter;
  }

  static QString containerGroupIdForPath(const QString &path) {
    return QStringLiteral("layout-container:") + path;
  }

  QWidget *buildContainerSplitNode(
      const QList<DockLayoutEntry> &entries, int depth) {
    if (entries.isEmpty()) return nullptr;
    const qsizetype offset = static_cast<qsizetype>(depth) * 2;
    if (entries.front().containerSplitPath.size() == offset) {
      const QString path = entries.front().containerSplitPath;
      return path.isEmpty()
          ? verticalSplitter_
          : containerGroupRoots_.value(containerGroupIdForPath(path), nullptr);
    }
    const QChar orientation = entries.front().containerSplitPath.at(offset);
    const int ratio = entries.front().containerSplitRatios.value(depth, 5000);
    QList<DockLayoutEntry> firstChild;
    QList<DockLayoutEntry> secondChild;
    for (const auto &entry : entries) {
      if (entry.containerSplitPath.at(offset + 1) == QLatin1Char('0')) {
        firstChild.push_back(entry);
      } else {
        secondChild.push_back(entry);
      }
    }
    auto *splitter = createConfiguredDockSplitter(
        orientation == QLatin1Char('H') ? Qt::Horizontal : Qt::Vertical,
        nullptr);
    containerSplitters_.push_back(splitter);
    QWidget *firstWidget = buildContainerSplitNode(firstChild, depth + 1);
    QWidget *secondWidget = buildContainerSplitNode(secondChild, depth + 1);
    if (!firstWidget || !secondWidget) {
      containerSplitters_.removeAll(splitter);
      delete splitter;
      return nullptr;
    }
    splitter->addWidget(firstWidget);
    splitter->addWidget(secondWidget);
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 1);
    splitter->setSizes({ratio, 10000 - ratio});
    return splitter;
  }

  void applyDockSplitLayout(const QList<DockLayoutEntry> &entries) {
    for (const DockArea area : {DockArea::Left, DockArea::Right,
                                DockArea::Top, DockArea::Bottom,
                                DockArea::Center}) {
      QList<DockLayoutEntry> areaEntries;
      for (const auto &entry : entries) {
        if (!entry.floating && entry.containerSplitPath.isEmpty() &&
            entry.area == area &&
            docks_.contains(entry.dockId)) {
          areaEntries.push_back(entry);
        }
      }
      bool hasSplit = false;
      for (const auto &entry : areaEntries) {
        hasSplit |= !entry.splitPath.isEmpty();
      }
      if (!hasSplit || areaEntries.isEmpty()) continue;
      auto *rootTabs = tabsForArea(area);
      auto *parent = area == DockArea::Top || area == DockArea::Bottom
          ? verticalSplitter_
          : splitter_;
      const int rootIndex = rootTabs && parent
          ? parent->indexOf(rootTabs)
          : -1;
      if (rootIndex < 0) continue;
      const QList<int> parentSizes = parent->sizes();
      while (rootTabs->count() > 0) {
        QWidget *widget = rootTabs->widget(rootTabs->count() - 1);
        rootTabs->removeTab(rootTabs->count() - 1);
        if (widget) widget->setParent(nullptr);
      }
      int leafIndex = 0;
      QWidget *areaRoot = buildDockSplitNode(
          area, areaEntries, 0, rootTabs, leafIndex);
      if (!areaRoot) continue;
      parent->insertWidget(rootIndex, areaRoot);
      if (parentSizes.size() == parent->count()) {
        parent->setSizes(parentSizes);
      }
      areaRoot->show();
    }

    QSet<QString> dynamicPaths;
    for (const auto &entry : entries) {
      if (!entry.floating && !entry.containerSplitPath.isEmpty() &&
          docks_.contains(entry.dockId)) {
        dynamicPaths.insert(entry.containerSplitPath);
      }
    }
    for (const QString &path : dynamicPaths) {
      QList<DockLayoutEntry> groupEntries;
      DockArea area = DockArea::Center;
      for (const auto &entry : entries) {
        if (!entry.floating && entry.containerSplitPath == path &&
            docks_.contains(entry.dockId)) {
          area = entry.area;
          groupEntries.push_back(entry);
        }
      }
      if (groupEntries.isEmpty()) continue;
      const QString groupId = containerGroupIdForPath(path);
      auto *rootTabs = createDockTabSurface(area, {}, {}, groupId);
      int leafIndex = 0;
      QWidget *groupRoot = buildDockSplitNode(
          area, groupEntries, 0, rootTabs, leafIndex, nullptr, groupId);
      if (!groupRoot) continue;
      containerGroupRoots_.insert(groupId, groupRoot);
      containerGroupAreas_.insert(groupId, area);
    }

    QSet<QString> containerPaths;
    for (const auto &entry : entries) {
      if (!entry.floating && docks_.contains(entry.dockId)) {
        containerPaths.insert(entry.containerSplitPath);
      }
    }
    QList<DockLayoutEntry> containerEntries;
    for (const QString &path : containerPaths) {
      const auto it = std::find_if(
          entries.cbegin(), entries.cend(), [&path](const DockLayoutEntry &entry) {
            return !entry.floating && entry.containerSplitPath == path;
          });
      if (it == entries.cend()) continue;
      DockLayoutEntry containerEntry = *it;
      containerEntry.splitPath = path;
      containerEntry.splitRatios = it->containerSplitRatios;
      containerEntries.push_back(containerEntry);
    }
    if (containerEntries.size() > 1 &&
        dockSplitPathsValid(containerEntries, 0)) {
      containerRootLayout_->removeWidget(containerRootWidget_);
      if (containerRootWidget_) containerRootWidget_->setParent(nullptr);
      QWidget *root = buildContainerSplitNode(containerEntries, 0);
      if (root) {
        containerRootWidget_ = root;
        containerRootLayout_->addWidget(root);
        root->show();
      } else {
        containerRootWidget_ = verticalSplitter_;
        containerRootLayout_->addWidget(verticalSplitter_);
      }
    }
  }

  void resetFloatingSplitLayouts() {
    QSet<QDialog *> dialogs;
    for (auto it = floatingDialogs_.cbegin(); it != floatingDialogs_.cend();
         ++it) {
      if (it.value()) dialogs.insert(it.value());
    }
    for (auto *dialog : dialogs) {
      auto *rootTabs = floatingTabSurfaces_.value(dialog, nullptr);
      auto *layout = qobject_cast<QVBoxLayout *>(dialog->layout());
      if (!rootTabs || !layout) continue;

      QList<QWidget *> widgets;
      const auto surfaces = floatingTabSurfacesForDialog(dialog);
      for (auto *tabs : surfaces) {
        if (!tabs) continue;
        while (tabs->count() > 0) {
          auto *widget = tabs->widget(0);
          tabs->removeTab(0);
          if (widget) {
            widget->setParent(nullptr);
            widgets.push_back(widget);
          }
        }
      }

      QWidget *oldRoot = floatingDockRoots_.value(dialog, rootTabs);
      const int rootIndex = layout->indexOf(oldRoot);
      QSet<QTabWidget *> removedSurfaces;
      for (auto *tabs : surfaces) {
        if (tabs && tabs != rootTabs) removedSurfaces.insert(tabs);
      }
      if (rootIndex >= 0) layout->removeWidget(oldRoot);
      if (rootTabs != oldRoot) rootTabs->setParent(nullptr);
      if (oldRoot) oldRoot->setParent(nullptr);
      for (auto *tabs : floatingTabSurfacesForDialog(dialog)) {
        if (tabs == rootTabs) continue;
        tabs->removeEventFilter(this);
        tabs->tabBar()->removeEventFilter(this);
        floatingTabOwners_.remove(tabs);
        areaHideSingleWidgetTitleBar_.remove(tabs);
        dockedSplitPaths_.remove(tabs);
        dockedSplitRatios_.remove(tabs);
      }
      floatingSplitters_.erase(
          std::remove_if(floatingSplitters_.begin(), floatingSplitters_.end(),
                         [oldRoot](QSplitter *splitter) {
                           return !splitter || splitter == oldRoot ||
                                  (oldRoot && oldRoot->isAncestorOf(splitter)) ||
                                  splitter->parentWidget() == nullptr;
                         }),
          floatingSplitters_.end());
      floatingDynamicTabSurfaces_.erase(
          std::remove_if(floatingDynamicTabSurfaces_.begin(),
                         floatingDynamicTabSurfaces_.end(),
                         [&removedSurfaces](QTabWidget *tabs) {
                           return !tabs || removedSurfaces.contains(tabs);
                         }),
          floatingDynamicTabSurfaces_.end());

      if (oldRoot && oldRoot != rootTabs) delete oldRoot;
      rootTabs->setParent(dialog);
      layout->insertWidget(rootIndex >= 0 ? rootIndex : 1, rootTabs);
      floatingDockRoots_.insert(dialog, rootTabs);
      floatingTabOwners_.insert(rootTabs, dialog);
      areaHideSingleWidgetTitleBar_.insert(rootTabs, false);
      floatingDynamicTabSurfaces_.removeAll(rootTabs);
      dockedSplitPaths_.insert(rootTabs, {});
      dockedSplitRatios_.insert(rootTabs, {});
      for (auto *widget : widgets) {
        const QString id = dockIdForWidget(widget);
        const int index = rootTabs->addTab(
            widget, titles_.value(id, id));
        if (!id.isEmpty()) {
          installCloseButton(rootTabs, index, id);
          rootTabs->setTabVisible(index, dockOpen(id) && !dockNoTab(id));
        }
      }
    }
  }

  void applyFloatingSplitLayouts(const QList<DockLayoutEntry> &entries) {
    QHash<QDialog *, QList<DockLayoutEntry>> groups;
    for (const auto &entry : entries) {
      if (!entry.floating) continue;
      auto *dialog = floatingDialogs_.value(entry.dockId, nullptr);
      if (dialog) groups[dialog].push_back(entry);
    }
    for (auto it = groups.begin(); it != groups.end(); ++it) {
      auto &groupEntries = it.value();
      const bool hasSplit = std::any_of(
          groupEntries.cbegin(), groupEntries.cend(),
          [](const DockLayoutEntry &entry) {
            return !entry.splitPath.isEmpty();
          });
      if (!hasSplit) continue;
      auto *dialog = it.key();
      auto *rootTabs = floatingTabSurfaces_.value(dialog, nullptr);
      auto *layout = qobject_cast<QVBoxLayout *>(dialog->layout());
      if (!rootTabs || !layout) continue;
      QWidget *oldRoot = floatingDockRoots_.value(dialog, rootTabs);
      const int rootIndex = layout->indexOf(oldRoot);
      if (rootIndex < 0) continue;
      layout->removeWidget(oldRoot);
      oldRoot->setParent(nullptr);
      while (rootTabs->count() > 0) {
        QWidget *widget = rootTabs->widget(0);
        rootTabs->removeTab(0);
        if (widget) widget->setParent(nullptr);
      }
      int leafIndex = 0;
      QWidget *treeRoot = buildDockSplitNode(
          DockArea::Center, groupEntries, 0, rootTabs, leafIndex, dialog);
      if (!treeRoot) {
        rootTabs->setParent(dialog);
        layout->insertWidget(rootIndex, rootTabs);
        floatingDockRoots_.insert(dialog, rootTabs);
        continue;
      }
      treeRoot->setParent(dialog);
      layout->insertWidget(rootIndex, treeRoot);
      treeRoot->show();
      floatingDockRoots_.insert(dialog, treeRoot);
      floatingTabSurfaces_.insert(dialog,
                                  firstDockTabSurface(treeRoot));
      refreshDockSplitMetadataNode(treeRoot, DockArea::Center, {}, {});
    }
  }

  void applyAreaTabPositions(const QHash<int, bool> &positions) {
    areaTabsAtBottom_.clear();
    for (const DockArea area : {DockArea::Left, DockArea::Right,
                                DockArea::Top, DockArea::Bottom,
                                DockArea::Center}) {
      const bool atBottom = positions.value(static_cast<int>(area), false);
      for (auto *tabs : dockedTabSurfaces_) {
        if (tabs && areaForTabs(tabs) == area) {
          tabs->setTabPosition(atBottom ? QTabWidget::South
                                        : QTabWidget::North);
          static_cast<DockTabSurface *>(tabs)
              ->refreshDockAreaTitleBarPlacement();
        }
      }
      if (atBottom) {
        areaTabsAtBottom_.insert(static_cast<int>(area), true);
      }
    }
  }

  void setAreaTabPositionImpl(DockArea area, bool atBottom) {
    if (!tabsForArea(area)) {
      return;
    }
    bool changed = false;
    for (auto *tabs : dockedTabSurfaces_) {
      if (!tabs || areaForTabs(tabs) != area ||
          (tabs->tabPosition() == QTabWidget::South) == atBottom) {
        continue;
      }
      changed = true;
    }
    if (!changed) return;
    const QByteArray beforeState = beginLayoutMutation();
    for (auto *tabs : dockedTabSurfaces_) {
      if (tabs && areaForTabs(tabs) == area) {
        tabs->setTabPosition(atBottom ? QTabWidget::South
                                      : QTabWidget::North);
        static_cast<DockTabSurface *>(tabs)
            ->refreshDockAreaTitleBarPlacement();
      }
    }
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

  void restoreDockFromTabList(const QString &dockId) {
    const QString resolvedId = resolveDockId(dockId);
    if (resolvedId.isEmpty() || !dockWidget(resolvedId)) {
      return;
    }
    if (!dockOpen(resolvedId)) {
      const QByteArray beforeState = beginLayoutMutation();
      setDockVisible(resolvedId, true);
      activateDock(resolvedId);
      finishLayoutMutation(beforeState, tr("Restore panel"));
    } else {
      activateDock(resolvedId);
    }
    if (recentlyClosedDockId_ == resolvedId) {
      recentlyClosedDockId_.clear();
    }
  }

  void showTabListMenu(QTabWidget *tabs, const QPoint &globalPosition) {
    if (!tabs || tabs->count() == 0) {
      return;
    }
    QMenu menu(tabs);
    for (int index = 0; index < tabs->count(); ++index) {
      const QString dockId = dockIdForWidget(tabs->widget(index));
      if (dockId.isEmpty() || dockNoTab(dockId)) {
        continue;
      }
      const bool visible = tabs->isTabVisible(index);
      QAction *action = menu.addAction(
          dockIcon(dockId), tabs->tabText(index));
      action->setCheckable(true);
      action->setChecked(index == tabs->currentIndex());
      QFont itemFont = action->font();
      itemFont.setItalic(!visible);
      action->setFont(itemFont);
      action->setData(dockId);
    }
    QAction *restoreLastClosedAction = nullptr;
    if (!recentlyClosedDockId_.isEmpty() &&
        dockWidget(recentlyClosedDockId_)) {
      menu.addSeparator();
      restoreLastClosedAction = menu.addAction(
          tr("Restore last closed panel: %1")
              .arg(titles_.value(recentlyClosedDockId_,
                                 recentlyClosedDockId_)));
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
      } else if (selected == restoreLastClosedAction) {
        restoreDockFromTabList(recentlyClosedDockId_);
      } else {
        restoreDockFromTabList(selected->data().toString());
      }
    }
  }

  void showDockTabContextMenu(const QString &dockId,
                              const QPoint &globalPosition) {
    const QString resolvedId = resolveDockId(dockId);
    const bool docked = docks_.contains(resolvedId);
    const bool floating = floatingDialogs_.contains(resolvedId);
    const bool autoHidden = autoHideAreas_.contains(resolvedId);
    if (resolvedId.isEmpty() || (!docked && !floating && !autoHidden)) {
      return;
    }
    QMenu menu(this);
    QAction *placementAction = nullptr;
    QAction *floatGroupAction = nullptr;
    QAction *closeOthersAction = nullptr;
    QAction *closeGroupAction = nullptr;
    QAction *topPositionAction = nullptr;
    QAction *bottomPositionAction = nullptr;
    QMenu *autoHideMenu = nullptr;
    QMenu *autoHideGroupMenu = nullptr;
    const auto addCloseOthersAction = [this, &menu, &resolvedId](
        QTabWidget *tabs) -> QAction * {
      if (!tabs) return nullptr;
      int openCount = 0;
      for (int index = 0; index < tabs->count(); ++index) {
        openCount += dockOpen(dockIdForWidget(tabs->widget(index))) ? 1 : 0;
      }
      if (openCount <= 1) return nullptr;
      QAction *action = menu.addAction(tr("Close other panels"));
      bool hasCloseableOther = false;
      for (int index = 0; index < tabs->count(); ++index) {
        const QString otherId = dockIdForWidget(tabs->widget(index));
        if (otherId != resolvedId && !otherId.isEmpty() &&
            dockOpen(otherId) && dockClosable(otherId) &&
            !dockPinned(otherId)) {
          hasCloseableOther = true;
          break;
        }
      }
      action->setEnabled(hasCloseableOther);
      return action;
    };
    if (autoHidden) {
      placementAction = menu.addAction(tr("Dock panel back"));
    } else if (docked) {
      placementAction = menu.addAction(tr("Float panel"));
      floatGroupAction = menu.addAction(tr("Float tab group"));
      auto *tabs = tabsForWidget(docks_.value(resolvedId, nullptr));
      floatGroupAction->setEnabled(tabs && tabs->count() > 1);
      closeOthersAction = addCloseOthersAction(tabs);
      closeGroupAction = menu.addAction(tr("Close tab group"));
      closeGroupAction->setEnabled(canCloseDockTabGroup(tabs));
      addTabPositionActions(&menu, tabs, &topPositionAction,
                            &bottomPositionAction);
      menu.addSeparator();
      autoHideMenu = menu.addMenu(tr("Auto-hide to"));
      for (const DockArea side : {DockArea::Left, DockArea::Right,
                                  DockArea::Top, DockArea::Bottom}) {
        QString label;
        switch (side) {
        case DockArea::Left: label = tr("Left"); break;
        case DockArea::Right: label = tr("Right"); break;
        case DockArea::Top: label = tr("Top"); break;
        case DockArea::Bottom: label = tr("Bottom"); break;
        case DockArea::Center: break;
        }
        QAction *action = autoHideMenu->addAction(label);
        action->setData(static_cast<int>(side));
      }
      autoHideGroupMenu = menu.addMenu(tr("Auto-hide tab group to"));
      for (const DockArea side : {DockArea::Left, DockArea::Right,
                                  DockArea::Top, DockArea::Bottom}) {
        QString label;
        switch (side) {
        case DockArea::Left: label = tr("Left"); break;
        case DockArea::Right: label = tr("Right"); break;
        case DockArea::Top: label = tr("Top"); break;
        case DockArea::Bottom: label = tr("Bottom"); break;
        case DockArea::Center: break;
        }
        QAction *action = autoHideGroupMenu->addAction(label);
        action->setData(static_cast<int>(side));
      }
    } else {
      placementAction = menu.addAction(tr("Dock panel back"));
      auto *tabs = tabsForWidget(floatingWidgets_.value(resolvedId, nullptr));
      closeOthersAction = addCloseOthersAction(tabs);
    }
    if (placementAction) {
      placementAction->setEnabled(
          autoHidden ? canMoveDock(resolvedId)
                     : docked ? canFloatDock(resolvedId)
                              : canMoveDock(resolvedId));
    }
    if (floatGroupAction) {
      auto *tabs = tabsForWidget(docks_.value(resolvedId, nullptr));
      floatGroupAction->setEnabled(canFloatDockTabGroup(tabs));
    }
    if (autoHideMenu) {
      autoHideMenu->setEnabled(canAutoHideDock(resolvedId));
    }
    if (autoHideGroupMenu) {
      auto *tabs = tabsForWidget(docks_.value(resolvedId, nullptr));
      autoHideGroupMenu->setEnabled(canAutoHideDockTabGroup(tabs));
    }
    QAction *pinAction = menu.addAction(
        dockPinned(resolvedId) ? tr("Unpin panel") : tr("Pin panel"));
    QAction *closableAction = menu.addAction(tr("Allow closing"));
    closableAction->setCheckable(true);
    closableAction->setChecked(dockClosable(resolvedId));
    QAction *movableAction = menu.addAction(tr("Allow moving"));
    movableAction->setCheckable(true);
    movableAction->setChecked(dockMovable(resolvedId));
    QAction *floatableAction = menu.addAction(tr("Allow floating"));
    floatableAction->setCheckable(true);
    floatableAction->setChecked(dockFloatable(resolvedId));
    QAction *focusableAction = menu.addAction(tr("Show focus highlight"));
    focusableAction->setCheckable(true);
    focusableAction->setChecked(dockFocusable(resolvedId));
    QAction *autoHideableAction = menu.addAction(tr("Allow Auto-hide"));
    autoHideableAction->setCheckable(true);
    autoHideableAction->setChecked(dockAutoHideable(resolvedId));
    QAction *middleClickCloseAction =
        menu.addAction(tr("Close tab on middle-click"));
    middleClickCloseAction->setCheckable(true);
    middleClickCloseAction->setChecked(middleMouseButtonClosesTab());
    QAction *autoHideButtonTogglesAreaAction =
        menu.addAction(tr("Auto-hide button toggles area"));
    autoHideButtonTogglesAreaAction->setCheckable(true);
    autoHideButtonTogglesAreaAction->setChecked(
        autoHideButtonTogglesArea());
    QAction *lockAction = menu.addAction(tr("Lock panel position"));
    lockAction->setCheckable(true);
    lockAction->setChecked(dockLocked(resolvedId));
    QAction *closeAction = menu.addAction(tr("Close panel"));
    closeAction->setEnabled(!dockPinned(resolvedId) &&
                            dockClosable(resolvedId));
    QAction *selected = menu.exec(globalPosition);
    if (selected == placementAction) {
      if (autoHidden) {
        dockAutoHiddenWidget(
            resolvedId, areas_.value(resolvedId, DockArea::Center));
      } else if (docked) {
        floatDockWidget(resolvedId);
      } else {
        dockFloatingWidget(resolvedId,
                           areas_.value(resolvedId, DockArea::Center));
      }
    } else if (selected == floatGroupAction) {
      floatDockTabGroup(resolvedId);
    } else if (selected == closeOthersAction) {
      auto *tabs = tabsForWidget(dockWidget(resolvedId));
      if (tabs) {
        const QByteArray beforeState = beginLayoutMutation();
        for (int index = 0; index < tabs->count(); ++index) {
          const QString otherId = dockIdForWidget(tabs->widget(index));
          if (!otherId.isEmpty() && otherId != resolvedId &&
              dockOpen(otherId) &&
              dockClosable(otherId) && !dockPinned(otherId)) {
            setDockVisible(otherId, false);
          }
        }
        finishLayoutMutation(beforeState, tr("Close other panels"));
      }
    } else if (selected == closeGroupAction) {
      closeDockTabGroup(tabsForWidget(dockWidget(resolvedId)));
    } else if (autoHideGroupMenu && selected &&
               selected->parent() == autoHideGroupMenu) {
      autoHideDockTabGroup(tabsForWidget(docks_.value(resolvedId, nullptr)),
                           static_cast<DockArea>(selected->data().toInt()));
    } else if (autoHideMenu && selected &&
               selected->parent() == autoHideMenu) {
      setDockAutoHidden(resolvedId,
                        static_cast<DockArea>(selected->data().toInt()));
    } else if (selected == topPositionAction) {
      setAreaTabPosition(areas_.value(resolvedId, DockArea::Center), false);
    } else if (selected == bottomPositionAction) {
      setAreaTabPosition(areas_.value(resolvedId, DockArea::Center), true);
    } else if (selected == pinAction) {
      const QByteArray beforeState = beginLayoutMutation();
      setDockPinned(resolvedId, !dockPinned(resolvedId));
      finishLayoutMutation(beforeState, tr("Pin panel"));
    } else if (selected == closableAction) {
      const QByteArray beforeState = beginLayoutMutation();
      setDockClosable(resolvedId, !dockClosable(resolvedId));
      finishLayoutMutation(beforeState, tr("Change panel closing"));
    } else if (selected == movableAction) {
      const QByteArray beforeState = beginLayoutMutation();
      setDockMovable(resolvedId, !dockMovable(resolvedId));
      finishLayoutMutation(beforeState, tr("Change panel movement"));
    } else if (selected == floatableAction) {
      const QByteArray beforeState = beginLayoutMutation();
      setDockFloatable(resolvedId, !dockFloatable(resolvedId));
      finishLayoutMutation(beforeState, tr("Change panel floating"));
    } else if (selected == focusableAction) {
      const QByteArray beforeState = beginLayoutMutation();
      setDockFocusable(resolvedId, !dockFocusable(resolvedId));
      finishLayoutMutation(beforeState, tr("Change focus highlight"));
    } else if (selected == autoHideableAction) {
      const QByteArray beforeState = beginLayoutMutation();
      setDockAutoHideable(resolvedId, !dockAutoHideable(resolvedId));
      finishLayoutMutation(beforeState, tr("Change panel Auto-hide"));
    } else if (selected == middleClickCloseAction) {
      const QByteArray beforeState = beginLayoutMutation();
      setMiddleMouseButtonClosesTab(!middleMouseButtonClosesTab());
      finishLayoutMutation(beforeState, tr("Change middle-click close"));
    } else if (selected == autoHideButtonTogglesAreaAction) {
      const QByteArray beforeState = beginLayoutMutation();
      setAutoHideButtonTogglesArea(!autoHideButtonTogglesArea());
      finishLayoutMutation(beforeState, tr("Change Auto-hide button behavior"));
    } else if (selected == lockAction) {
      const QByteArray beforeState = beginLayoutMutation();
      setDockLocked(resolvedId, !dockLocked(resolvedId));
      finishLayoutMutation(beforeState, tr("Lock panel position"));
    } else if (selected == closeAction) {
      const QByteArray beforeState = beginLayoutMutation();
      setDockVisible(resolvedId, false);
      finishLayoutMutation(beforeState, tr("Close panel"));
    }
  }

  void installCloseButton(QTabWidget *tabs, int index, const QString &id) {
    if (tabs && index >= 0 && index < tabs->count()) {
      tabs->setTabIcon(index, dockIcon(id));
      tabs->setTabToolTip(index, dockTabToolTip(id));
      auto *panel = tabs->widget(index);
      const QVariant openState = panel ? panel->property("artifactDockOpen")
                                       : QVariant{};
      const bool open = !openState.isValid() || openState.toBool();
      const bool noTab = panel &&
          panel->property("artifactDockNoTab").toBool();
      tabs->setTabVisible(index, open && !noTab);
      refreshDockTabLabels(tabs);
      refreshDockTabBarVisibility(tabs);
    }
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
    refreshDockTabCloseButtons(tabs);
    syncPinButton(id, pinned_.value(id, false));
  }

  void refreshDockTabCloseButtons(QTabWidget *tabs) {
    if (!tabs || !tabs->tabBar()) return;
    for (int index = 0; index < tabs->count(); ++index) {
      const QString dockId = dockIdForWidget(tabs->widget(index));
      auto *side = tabs->tabBar()->tabButton(index, QTabBar::RightSide);
      if (!side) continue;
      auto *closeButton = side->findChild<QToolButton *>();
      if (!closeButton ||
          closeButton->property("artifactDockCloseId").toString() != dockId) {
        continue;
      }
      const bool closable = !dockId.isEmpty() && dockClosable(dockId);
      const bool active = tabs->currentIndex() == index;
      const bool showButton = closable &&
          (allTabsHaveCloseButton_ ||
           (activeTabHasCloseButton_ && active));
      const bool pinned = !dockId.isEmpty() && dockPinned(dockId);
      closeButton->setEnabled(closable && !pinned);
      closeButton->setToolTip(!closable ? tr("Panel closing is disabled")
                           : pinned ? tr("Unpin panel before closing")
                                    : tr("Close panel"));
      QSizePolicy policy = side->sizePolicy();
      policy.setRetainSizeWhenHidden(
          closable && retainTabSizeWhenCloseButtonHidden_);
      side->setSizePolicy(policy);
      side->setVisible(showButton);
    }
  }

  void refreshAllDockTabCloseButtons() {
    for (auto *tabs : dockedTabSurfaces_) {
      refreshDockTabCloseButtons(tabs);
    }
    for (auto *tabs : floatingTabSurfaces_) {
      refreshDockTabCloseButtons(tabs);
    }
  }

  void refreshAllDockTabListButtons() {
    for (auto *tabs : dockedTabSurfaces_) {
      if (auto *surface = static_cast<DockTabSurface *>(tabs)) {
        surface->refreshTabListButtonVisibility();
      }
    }
    for (auto *tabs : floatingTabSurfaces_) {
      if (auto *surface = static_cast<DockTabSurface *>(tabs)) {
        surface->refreshTabListButtonVisibility();
      }
    }
    for (auto *tabs : autoHidePanelSurfaces_) {
      if (auto *surface = static_cast<DockTabSurface *>(tabs)) {
        surface->refreshTabListButtonVisibility();
      }
    }
  }

  void refreshDockTabLabels(QTabWidget *tabs) {
    if (!tabs) return;
    for (int index = 0; index < tabs->count(); ++index) {
      const QString dockId = dockIdForWidget(tabs->widget(index));
      if (dockId.isEmpty()) continue;
      const QString title = titles_.value(dockId, dockId);
      const bool hideTitle = showTabTextOnlyForActiveTab_ &&
          !dockIcon(dockId).isNull() && tabs->currentIndex() != index;
      const QString displayTitle = hideTitle ? QString{} : title;
      if (tabs->tabText(index) != displayTitle) {
        tabs->setTabText(index, displayTitle);
      }
    }
  }

  void refreshAllDockTabLabels() {
    for (auto *tabs : dockedTabSurfaces_) refreshDockTabLabels(tabs);
    for (auto *tabs : floatingTabSurfaces_) refreshDockTabLabels(tabs);
    for (auto *tabs : autoHidePanelSurfaces_) refreshDockTabLabels(tabs);
  }

  void syncPinButton(const QString &dockId, bool pinned) {
    const QString resolvedId = resolveDockId(dockId);
    const bool closable = dockClosable(resolvedId);
    if (auto *widget = dockWidget(resolvedId)) {
      refreshDockTabGroupCloseButton(tabsForWidget(widget));
    }
    if (autoHideActiveDockId_ == resolvedId && autoHideFlyoutCloseButton_) {
      autoHideFlyoutCloseButton_->setVisible(closable);
      autoHideFlyoutCloseButton_->setEnabled(closable && !pinned);
      autoHideFlyoutCloseButton_->setToolTip(
          !closable ? tr("Panel closing is disabled")
          : pinned ? tr("Unpin panel before closing")
                   : tr("Close panel"));
    }
    auto *widget = dockWidget(dockId);
    if (!widget) return;
    auto *tabs = tabsForWidget(widget);
    if (!tabs) return;
    const int index = tabs->indexOf(widget);
    if (index < 0) return;
    refreshDockTabCloseButtons(tabs);
  }

  bool floatingGroupMovable(QDialog *dialog) const {
    bool foundPanel = false;
    for (auto *tabs : floatingTabSurfacesForDialog(dialog)) {
      for (int index = 0; index < tabs->count(); ++index) {
        const QString dockId = dockIdForWidget(tabs->widget(index));
        if (dockId.isEmpty()) continue;
        foundPanel = true;
        if (!canMoveDock(dockId)) return false;
      }
    }
    return foundPanel;
  }

  void syncFloatingDockBackButton(QDialog *dialog) {
    if (!dialog) return;
    const bool enabled = floatingGroupMovable(dialog);
    for (auto *button : dialog->findChildren<QToolButton *>()) {
      if (button && button->property("artifactDockBackGroup").toBool()) {
        button->setEnabled(enabled);
      }
    }
  }

  void dockFloatingGroup(QDialog *dialog) {
    if (!dialog || !floatingGroupMovable(dialog)) {
      return;
    }
    QStringList dockIds;
    for (auto *tabs : floatingTabSurfacesForDialog(dialog)) {
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
    for (const QString &dockId : dockIds) {
      if (!canMoveDock(dockId)) return;
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
    header->setProperty("artifactFloatingDockHeaderDrag", true);
    header->installEventFilter(this);
    auto *headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(6, 3, 3, 3);
    auto *label = new QLabel(title, header);
    label->setObjectName(
        QStringLiteral("artifactNativeFloatingDockTitle"));
    label->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    label->setProperty("artifactFloatingDockHeaderDrag", true);
    label->installEventFilter(this);
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
    dialog->setAcceptDrops(true);
    dialog->installEventFilter(this);
    auto *tabs = createTabSurface(dialog);
    tabs->setAcceptDrops(true);
    tabs->tabBar()->setAcceptDrops(true);
    tabs->installEventFilter(this);
    tabs->tabBar()->installEventFilter(this);
    if (auto *tabListButton =
            static_cast<DockTabSurface *>(tabs)->tabListButton()) {
      tabListButton->installEventFilter(this);
    }
    if (auto *closeButton =
            static_cast<DockTabSurface *>(tabs)->closeGroupButton()) {
      closeButton->installEventFilter(this);
    }
    layout->addWidget(tabs);
    floatingTabSurfaces_.insert(dialog, tabs);
    floatingDockRoots_.insert(dialog, tabs);
    floatingTabOwners_.insert(tabs, dialog);
    areaHideSingleWidgetTitleBar_.insert(tabs, false);
    ensureFloatingGroupId(dialog);
    dockedSplitPaths_.insert(tabs, {});
    dockedSplitRatios_.insert(tabs, {});
    auto *dropPreview = new DockDropPreview(dialog);
    floatingDropPreviews_.insert(dialog, dropPreview);
    return tabs;
  }

  QTabWidget *createTabSurface(QWidget *parent) {
    auto *tabs = new DockTabSurface(parent);
    tabs->setDynamicTabListButtonVisibility(
        dynamicTabListButtonVisibility_);
    tabs->setDocumentMode(true);
    tabs->setMovable(true);
    QFont tabFont = tabs->tabBar()->font();
    if (tabFont.pointSizeF() > 0.0) {
      tabFont.setPointSizeF(tabFont.pointSizeF() * 1.2);
    } else if (tabFont.pixelSize() > 0) {
      tabFont.setPixelSize((tabFont.pixelSize() * 6 + 4) / 5);
    }
    tabs->tabBar()->setFont(tabFont);
    tabs->tabBar()->setExpanding(false);
    tabs->tabBar()->setUsesScrollButtons(true);
    tabs->tabBar()->setElideMode(disableTabTextEliding_
                                     ? Qt::ElideNone
                                     : Qt::ElideRight);
    auto *tabListButton = new QToolButton(tabs);
    tabListButton->setAutoRaise(true);
    tabListButton->setArrowType(Qt::DownArrow);
    tabListButton->setToolTip(tr("Show panel list"));
    tabListButton->setAccessibleName(tr("Show panel list"));
    tabListButton->setProperty("artifactDockTabList", true);
    tabListButton->setFixedSize(22, 22);
    tabs->setTabListButton(tabListButton);
    tabs->setTabListButtonEnabled(dockAreaHasTabsMenuButton_);
    auto *closeGroupButton = new QToolButton(tabs);
    closeGroupButton->setAutoRaise(true);
    closeGroupButton->setIcon(closeGroupButton->style()->standardIcon(
        QStyle::SP_TitleBarCloseButton));
    closeGroupButton->setToolTip(tr("Close tab group"));
    closeGroupButton->setAccessibleName(tr("Close tab group"));
    closeGroupButton->setProperty("artifactDockCloseGroup", true);
    closeGroupButton->setFixedSize(22, 22);
    tabs->setCloseGroupButton(closeGroupButton);
    auto *floatGroupButton = new QToolButton(tabs);
    floatGroupButton->setAutoRaise(true);
    floatGroupButton->setIcon(floatGroupButton->style()->standardIcon(
        QStyle::SP_TitleBarNormalButton));
    floatGroupButton->setToolTip(tr("Float tab group"));
    floatGroupButton->setAccessibleName(tr("Float tab group"));
    floatGroupButton->setProperty("artifactDockFloatGroup", true);
    floatGroupButton->setFixedSize(22, 22);
    tabs->setFloatGroupButton(floatGroupButton);
    auto *autoHideButton = new QToolButton(tabs);
    autoHideButton->setAutoRaise(true);
    autoHideButton->setIcon(autoHideButton->style()->standardIcon(
        QStyle::SP_DialogOkButton));
    autoHideButton->setCheckable(autoHideButtonCheckable_);
    autoHideButton->setToolTip(tr("Auto-hide active panel"));
    autoHideButton->setAccessibleName(tr("Auto-hide active panel"));
    autoHideButton->setProperty("artifactDockAutoHideActive", true);
    autoHideButton->setFixedSize(22, 22);
    tabs->setAutoHideButton(autoHideButton);
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
    return dockedTabAreas_.value(const_cast<QTabWidget *>(tabs),
                                 DockArea::Center);
  }

  QTabWidget *tabsForWidget(QWidget *widget) const {
    for (auto *tabs : dockedTabSurfaces_) {
      if (tabs && tabs->indexOf(widget) >= 0) {
        return tabs;
      }
    }
    for (auto it = floatingTabOwners_.cbegin();
         it != floatingTabOwners_.cend(); ++it) {
      if (it.key() && it.key()->indexOf(widget) >= 0) {
        return it.key();
      }
    }
    for (auto it = autoHidePanelSurfaces_.cbegin();
         it != autoHidePanelSurfaces_.cend(); ++it) {
      if (it.value() && it.value()->indexOf(widget) >= 0) {
        return it.value();
      }
    }
    return nullptr;
  }

  QDialog *floatingDialogForTabs(const QTabWidget *tabs) const {
    return floatingTabOwners_.value(const_cast<QTabWidget *>(tabs), nullptr);
  }

  void refreshFloatingDialogTitle(QTabWidget *tabs) {
    auto *dialog = floatingDialogForTabs(tabs);
    auto *widget = tabs ? tabs->currentWidget() : nullptr;
    const QString dockId = dockIdForWidget(widget);
    if (!dialog || dockId.isEmpty()) return;
    const QString title = titles_.value(dockId, dockId);
    if (dialog->windowTitle() != title) dialog->setWindowTitle(title);
    const QIcon panelIcon = floatingContainerHasWidgetIcon_
        ? dockIcon(dockId)
        : QIcon{};
    const QIcon effectiveWindowIcon = panelIcon.isNull()
        ? QApplication::windowIcon()
        : panelIcon;
    if (dialog->windowIcon().cacheKey() !=
        effectiveWindowIcon.cacheKey()) {
      dialog->setWindowIcon(effectiveWindowIcon);
    }
    for (auto *label : dialog->findChildren<QLabel *>()) {
      if (label->objectName() ==
              QStringLiteral("artifactNativeFloatingDockTitle") &&
          label->text() != title) {
        label->setText(title);
      }
    }
  }

  QTabWidget *tabSurfaceForObject(const QWidget *widget) const {
    if (!widget) {
      return nullptr;
    }
    for (auto *tabs : dockedTabSurfaces_) {
      if (tabs && (tabs == widget || tabs->isAncestorOf(widget))) {
        return tabs;
      }
    }
    for (auto it = floatingTabOwners_.cbegin();
         it != floatingTabOwners_.cend(); ++it) {
      if (it.key() && (it.key() == widget ||
                       it.key()->isAncestorOf(widget))) {
        return it.key();
      }
    }
    for (auto it = autoHidePanelSurfaces_.cbegin();
         it != autoHidePanelSurfaces_.cend(); ++it) {
      if (it.value() && (it.value() == widget ||
                         it.value()->isAncestorOf(widget))) {
        return it.value();
      }
    }
    return nullptr;
  }

  int dockAreaAllowedDropAreasMask(const QString &dockId) const {
    const QString resolvedId = resolveDockId(dockId);
    auto *widget = dockWidget(resolvedId);
    if (!widget) return kAllDockAreaDropTargets;
    if (!autoHideAreas_.contains(resolvedId)) {
      return dockAreaAllowedDropAreasMask(tabsForWidget(widget));
    }
    const QVariant stored =
        widget->property("artifactDockAreaAllowedDropAreas");
    if (!stored.isValid()) return kAllDockAreaDropTargets;
    const int mask = stored.toInt();
    return mask >= 0 && (mask & ~kAllDockAreaDropTargets) == 0
        ? mask
        : kAllDockAreaDropTargets;
  }

  int dockAreaAllowedDropAreasMask(const QTabWidget *tabs) const {
    if (!tabs) return kAllDockAreaDropTargets;
    const QVariant stored =
        tabs->property("artifactDockAreaAllowedDropAreas");
    if (!stored.isValid()) return kAllDockAreaDropTargets;
    const int mask = stored.toInt();
    return mask >= 0 && (mask & ~kAllDockAreaDropTargets) == 0
        ? mask
        : kAllDockAreaDropTargets;
  }

  static QList<DockArea> dockAreasFromDropMask(int mask) {
    QList<DockArea> areas;
    mask &= kAllDockAreaDropTargets;
    for (const DockArea area : {DockArea::Left, DockArea::Right,
                                DockArea::Top, DockArea::Bottom,
                                DockArea::Center}) {
      if (mask & dockAreaDropTargetBit(area)) areas.push_back(area);
    }
    return areas;
  }

  void setActiveTabSurface(QTabWidget *tabs) {
    if (!tabs) {
      return;
    }
    if (activeTabs_ == tabs) {
      static_cast<DockTabSurface *>(tabs)->refreshFocusChrome();
      refreshDockTabGroupCloseButton(tabs);
      return;
    }
    if (activeTabs_) {
      activeTabs_->setProperty("artifactNativeActivePanel", false);
      static_cast<DockTabSurface *>(activeTabs_.data())->refreshFocusChrome();
    }
    activeTabs_ = tabs;
    activeTabs_->setProperty("artifactNativeActivePanel", true);
    static_cast<DockTabSurface *>(activeTabs_.data())->refreshFocusChrome();
    refreshDockTabGroupCloseButton(tabs);
  }

  QTabWidget *topTabs_ = nullptr;
  QTabWidget *leftTabs_ = nullptr;
  QTabWidget *centerTabs_ = nullptr;
  QTabWidget *rightTabs_ = nullptr;
  QTabWidget *bottomTabs_ = nullptr;
  QTabBar *autoHideTopBar_ = nullptr;
  QTabBar *autoHideLeftBar_ = nullptr;
  QTabBar *autoHideRightBar_ = nullptr;
  QTabBar *autoHideBottomBar_ = nullptr;
  QFrame *autoHideFlyout_ = nullptr;
  QVBoxLayout *autoHideFlyoutLayout_ = nullptr;
  QWidget *autoHideFlyoutHeader_ = nullptr;
  QHBoxLayout *autoHideFlyoutHeaderLayout_ = nullptr;
  QLabel *autoHideFlyoutTitle_ = nullptr;
  QToolButton *autoHideDockBackButton_ = nullptr;
  QToolButton *autoHideFlyoutCloseButton_ = nullptr;
  QPointer<QTabWidget> autoHideActiveTabs_;
  QHash<QString, QTabWidget *> autoHidePanelSurfaces_;
  QHash<QString, DockArea> autoHideAreas_;
  QHash<QString, bool> autoHideVisible_;
  QString autoHideActiveDockId_;
  QSplitter *splitter_ = nullptr;
  QSplitter *verticalSplitter_ = nullptr;
  QWidget *containerRootHost_ = nullptr;
  QVBoxLayout *containerRootLayout_ = nullptr;
  QWidget *containerRootWidget_ = nullptr;
  DockDropPreview *dropPreview_ = nullptr;
  QPointer<QTabWidget> activeTabs_;
  QPointer<QTabWidget> dropInsertionPreviewTabs_;
  QPoint dragStartPosition_;
  QPoint dockDragWindowOffset_;
  QString dragSourceId_;
  bool dockDragCancelled_ = false;
  bool dockDragRejected_ = false;
  bool dockDragBlocked_ = false;
  bool dockDragWholeGroup_ = false;
  bool middleMouseButtonClosesTab_ = false;
  bool dynamicTabListButtonVisibility_ = false;
  bool dockAreaHideDisabledButtons_ = false;
  bool floatingContainerHasWidgetIcon_ = false;
  bool activeTabHasCloseButton_ = true;
  bool allTabsHaveCloseButton_ = false;
  bool retainTabSizeWhenCloseButtonHidden_ = false;
  bool disableTabTextEliding_ = false;
  bool showTabTextOnlyForActiveTab_ = false;
  bool alwaysShowTabs_ = false;
  bool hideSingleCentralWidgetTitleBar_ = false;
  bool equalSplitOnInsertion_ = false;
  bool opaqueSplitterResize_ = false;
  bool refreshingDockTabBarVisibility_ = false;
  bool doubleClickUndocksWidget_ = true;
  bool dockAreaHasTabsMenuButton_ = true;
  bool dockAreaHasCloseButton_ = true;
  bool dockAreaCloseButtonClosesTab_ = false;
  bool dockAreaHasUndockButton_ = true;
  bool autoHideButtonTogglesArea_ = false;
  bool autoHideButtonCheckable_ = false;
  bool autoHideSideBarsIconOnly_ = false;
  bool autoHideShowOnMouseOver_ = false;
  bool autoHideHoverOpen_ = false;
  QString autoHideHoverDockId_;
  QBasicTimer autoHideHoverTimer_;
  LayoutMutationCallback layoutMutationCallback_;
  QByteArray layoutMutationBatchBeforeState_;
  QString layoutMutationBatchLabel_;
  int layoutMutationBatchDepth_ = 0;
  bool restoringLayout_ = false;
  QHash<QString, QWidget *> docks_;
  QHash<QString, QDialog *> floatingDialogs_;
  QHash<QString, QWidget *> floatingWidgets_;
  QHash<QDialog *, QTabWidget *> floatingTabSurfaces_;
  QHash<QDialog *, QString> floatingGroupIds_;
  QHash<QDialog *, DockDropPreview *> floatingDropPreviews_;
  QHash<QTabWidget *, QDialog *> floatingTabOwners_;
  QHash<QDialog *, QWidget *> floatingDockRoots_;
  QList<QTabWidget *> dockedTabSurfaces_;
  QHash<QTabWidget *, DockArea> dockedTabAreas_;
  QHash<QTabWidget *, bool> areaHideSingleWidgetTitleBar_;
  QHash<QTabWidget *, QString> dockedSplitPaths_;
  QHash<QTabWidget *, QList<int>> dockedSplitRatios_;
  QHash<QTabWidget *, QString> dockedContainerGroupIds_;
  QHash<QString, QWidget *> containerGroupRoots_;
  QHash<QString, DockArea> containerGroupAreas_;
  QList<QTabWidget *> dynamicTabSurfaces_;
  QList<QSplitter *> localSplitters_;
  QList<QSplitter *> containerSplitters_;
  int containerGroupSequence_ = 0;
  int floatingGroupSequence_ = 0;
  QList<QTabWidget *> floatingDynamicTabSurfaces_;
  QList<QSplitter *> floatingSplitters_;
  QHash<QString, DockArea> areas_;
  QHash<int, bool> areaTabsAtBottom_;
  QHash<QString, QString> titles_;
  QHash<QString, QList<QPointer<QAction>>> dockTitleBarActions_;
  QHash<QString, bool> pinned_;
  QHash<QString, QString> autoHideReturnTabGroups_;
  QHash<QString, int> autoHideReturnTabIndexes_;
  QHash<QString, QString> autoHideReturnSplitPaths_;
  QHash<QString, QList<int>> autoHideReturnSplitRatios_;
  QHash<QString, QString> autoHideReturnContainerGroupIds_;
  QHash<QString, QString> autoHideReturnContainerPaths_;
  QHash<QString, QList<int>> autoHideReturnContainerRatios_;
  QString recentlyClosedDockId_;
};

} // namespace Artifact
