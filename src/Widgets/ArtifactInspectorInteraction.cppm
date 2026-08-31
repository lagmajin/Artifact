module;

#include <QAction>
#include <QCursor>
#include <QDropEvent>
#include <QFocusEvent>
#include <QListWidget>
#include <QMenu>
#include <QModelIndex>
#include <QPaintEvent>
#include <QPainter>
#include <QPalette>
#include <QRect>
#include <QRectF>
#include <QString>
#include <QStringList>
#include <QStyle>
#include <QStyleOptionViewItem>
#include <QWidget>

#include <functional>
#include <utility>

export module Artifact.Widgets.InspectorInteraction;

import Artifact.Widgets.InspectorStyle;

export namespace Artifact {

namespace detail {
class EffectRackList final : public QListWidget {
 public:
  using ReorderHandler = std::function<void(const QStringList&, int)>;

  explicit EffectRackList(QWidget* parent = nullptr) : QListWidget(parent) {
    setAttribute(Qt::WA_Hover, true);
    if (viewport()) {
      viewport()->setAttribute(Qt::WA_Hover, true);
    }
  }

  void setReorderHandler(ReorderHandler handler) {
    reorderHandler_ = std::move(handler);
  }

 protected:
  void dropEvent(QDropEvent* event) override {
    if (!event || event->source() != this || !reorderHandler_) {
      QListWidget::dropEvent(event);
      return;
    }
    const int sourceRow = currentRow();
    const int targetRow = qBound(
        0, indexAt(event->position().toPoint()).row(), qMax(0, count() - 1));
    auto* sourceItem = currentItem();
    if (!sourceItem || sourceRow == targetRow) {
      event->ignore();
      return;
    }
    QStringList selectedEffectIds;
    const auto selected = selectedItems();
    for (auto *item : selected) {
      if (!item) {
        continue;
      }
      const QString effectId = item->data(Qt::UserRole).toString().trimmed();
      if (!effectId.isEmpty()) {
        selectedEffectIds.push_back(effectId);
      }
    }
    if (selectedEffectIds.isEmpty()) {
      selectedEffectIds.push_back(
          sourceItem->data(Qt::UserRole).toString().trimmed());
    }
    reorderHandler_(selectedEffectIds, targetRow - sourceRow);
    event->acceptProposedAction();
  }

  void paintEvent(QPaintEvent* event) override {
    QPainter painter(viewport());
    painter.setRenderHint(QPainter::Antialiasing, true);
    const QPalette pal = palette();
    if (event) {
      painter.setClipRegion(event->region());
    }
    painter.fillRect(viewport()->rect(), pal.color(QPalette::Base));
    const QModelIndex hoveredIndex =
        viewport()->underMouse()
            ? indexAt(viewport()->mapFromGlobal(QCursor::pos()))
            : QModelIndex{};
    auto* delegate = itemDelegate();
    if (delegate) {
      for (int row = 0; row < count(); ++row) {
        auto* listItem = item(row);
        if (!listItem) {
          continue;
        }
        const QRect itemRect = visualItemRect(listItem);
        if (!itemRect.isValid() || !itemRect.intersects(viewport()->rect())) {
          continue;
        }
        const QModelIndex index = model()->index(row, 0);
        QStyleOptionViewItem option;
        option.initFrom(this);
        option.rect = itemRect;
        option.palette = pal;
        option.font = font();
        option.state = isEnabled() ? QStyle::State_Enabled
                                   : QStyle::State_None;
        if (isActiveWindow()) option.state |= QStyle::State_Active;
        if (listItem->isSelected()) option.state |= QStyle::State_Selected;
        if (hasFocus() && currentItem() == listItem)
          option.state |= QStyle::State_HasFocus;
        if (hoveredIndex == index) option.state |= QStyle::State_MouseOver;
        delegate->paint(&painter, option, index);
      }
    }
    painter.setClipping(false);
    painter.setPen(hasFocus() ? pal.color(QPalette::Highlight)
                              : pal.color(QPalette::Mid));
    painter.setBrush(Qt::NoBrush);
    painter.drawRoundedRect(
        QRectF(viewport()->rect()).adjusted(0.5, 0.5, -0.5, -0.5),
        3.0, 3.0);
  }

 private:
  ReorderHandler reorderHandler_;
};

class InspectorActionMenu final : public QMenu {
 public:
  using QMenu::QMenu;

 protected:
  void paintEvent(QPaintEvent* event) override {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);
    if (event) {
      painter.setClipRegion(event->region());
    }
    const QPalette pal = palette();
    painter.fillRect(rect(), pal.color(QPalette::Window));

    for (auto* action : actions()) {
      if (!action || !action->isVisible()) {
        continue;
      }
      const QRect actionRect = actionGeometry(action);
      if (!actionRect.isValid()) {
        continue;
      }
      if (action->isSeparator()) {
        painter.setPen(pal.color(QPalette::Mid));
        painter.drawLine(actionRect.left() + 8, actionRect.center().y(),
                         actionRect.right() - 8, actionRect.center().y());
        continue;
      }

      const bool hovered = activeAction() == action;
      if (hovered && action->isEnabled()) {
        painter.setPen(Qt::NoPen);
        painter.setBrush(
            blendColor(pal.color(QPalette::Window),
                       pal.color(QPalette::Highlight), 0.22));
        painter.drawRoundedRect(
            QRectF(actionRect).adjusted(3.0, 1.0, -3.0, -1.0), 3.0, 3.0);
      }

      int textLeft = actionRect.left() + 10;
      if (action->isCheckable()) {
        const QRectF stateRect(actionRect.left() + 9,
                               actionRect.center().y() - 5, 10, 10);
        painter.setPen(action->isChecked()
                           ? pal.color(QPalette::Highlight)
                           : pal.color(QPalette::Mid));
        if (action->isChecked()) {
          painter.setBrush(pal.color(QPalette::Highlight));
        } else {
          painter.setBrush(Qt::NoBrush);
        }
        painter.drawRoundedRect(stateRect, 2.0, 2.0);
        textLeft = actionRect.left() + 27;
      }
      painter.setPen(action->isEnabled()
                         ? pal.color(QPalette::WindowText)
                         : pal.color(QPalette::Disabled,
                                     QPalette::WindowText));
      painter.drawText(actionRect.adjusted(textLeft - actionRect.left(), 0,
                                           -10, 0),
                       Qt::AlignLeft | Qt::AlignVCenter, action->text());
    }

    painter.setClipping(false);
    painter.setPen(pal.color(QPalette::Mid));
    painter.setBrush(Qt::NoBrush);
    painter.drawRoundedRect(
        QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), 4.0, 4.0);
  }
};

class InspectorSelectionList final : public QListWidget {
public:
  explicit InspectorSelectionList(QWidget* parent = nullptr)
      : QListWidget(parent) {
    setAttribute(Qt::WA_Hover, true);
    if (viewport()) {
      viewport()->setAttribute(Qt::WA_Hover, true);
    }
  }

  void setSelectionAction(
      std::function<void(QListWidgetItem *)> action) {
    action_ = std::move(action);
  }

  void setSelectionActionEnabled(bool enabled) {
    selectionActionEnabled_ = enabled;
  }

protected:
  void paintEvent(QPaintEvent* event) override {
    QPainter painter(viewport());
    painter.setRenderHint(QPainter::Antialiasing, true);
    const QPalette pal = palette();
    if (event) {
      painter.setClipRegion(event->region());
    }
    painter.fillRect(viewport()->rect(), pal.color(QPalette::Base));

    const QModelIndex hoveredIndex =
        viewport()->underMouse()
            ? indexAt(viewport()->mapFromGlobal(QCursor::pos()))
            : QModelIndex{};
    auto* delegate = itemDelegate();
    if (delegate) {
      for (int row = 0; row < count(); ++row) {
        auto* listItem = item(row);
        if (!listItem) {
          continue;
        }
        const QRect itemRect = visualItemRect(listItem);
        if (!itemRect.isValid() || !itemRect.intersects(viewport()->rect())) {
          continue;
        }
        const QModelIndex index = model()->index(row, 0);
        QStyleOptionViewItem option;
        option.initFrom(this);
        option.rect = itemRect;
        option.palette = pal;
        option.font = font();
        option.state = isEnabled() ? QStyle::State_Enabled
                                   : QStyle::State_None;
        if (isActiveWindow()) {
          option.state |= QStyle::State_Active;
        }
        if (listItem->isSelected()) {
          option.state |= QStyle::State_Selected;
        }
        if (hasFocus() && currentItem() == listItem) {
          option.state |= QStyle::State_HasFocus;
        }
        if (hoveredIndex == index) {
          option.state |= QStyle::State_MouseOver;
        }
        delegate->paint(&painter, option, index);
      }
    }

    painter.setClipping(false);
    const QColor border = hasFocus()
        ? pal.color(QPalette::Highlight)
        : pal.color(QPalette::Mid);
    painter.setPen(border);
    painter.setBrush(Qt::NoBrush);
    painter.drawRoundedRect(
        QRectF(viewport()->rect()).adjusted(0.5, 0.5, -0.5, -0.5),
        3.0, 3.0);
  }

  void focusInEvent(QFocusEvent* event) override {
    QListWidget::focusInEvent(event);
    if (viewport()) {
      viewport()->update();
    }
  }

  void focusOutEvent(QFocusEvent* event) override {
    QListWidget::focusOutEvent(event);
    if (viewport()) {
      viewport()->update();
    }
  }

  void currentChanged(const QModelIndex &current,
                      const QModelIndex &previous) override {
    QListWidget::currentChanged(current, previous);
    if (selectionActionEnabled_ && action_) {
      action_(currentItem());
    }
  }

private:
  bool selectionActionEnabled_ = true;
  std::function<void(QListWidgetItem *)> action_;
};

class SelectionActionBlocker final {
public:
  explicit SelectionActionBlocker(InspectorSelectionList *list)
      : list_(list) {
    if (list_) {
      list_->setSelectionActionEnabled(false);
    }
  }

  ~SelectionActionBlocker() {
    if (list_) {
      list_->setSelectionActionEnabled(true);
    }
  }

private:
  InspectorSelectionList *list_ = nullptr;
};
} // namespace detail
} // namespace Artifact

