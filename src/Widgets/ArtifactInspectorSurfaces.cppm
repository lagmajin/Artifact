module;

#include <QColor>
#include <QFrame>
#include <QFont>
#include <QKeyEvent>
#include <QLabel>
#include <QListWidget>
#include <QModelIndex>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QPalette>
#include <QPushButton>
#include <QRect>
#include <QRectF>
#include <QSize>
#include <QString>
#include <QStyle>
#include <QStyledItemDelegate>
#include <QStyleOptionViewItem>
#include <QVBoxLayout>
#include <QWidget>

#include <functional>
#include <utility>

export module Artifact.Widgets.InspectorSurfaces;

import Artifact.Widgets.InspectorStyle;

export namespace Artifact {

namespace detail {

class InspectorActionButton final : public QPushButton {
public:
  using QPushButton::QPushButton;

  void setOwnerDrawn(bool enabled) {
    ownerDrawn_ = enabled;
    setAttribute(Qt::WA_Hover, enabled);
    update();
  }

  void setAction(std::function<void()> action) {
    action_ = std::move(action);
  }

  void triggerAction() {
    if (action_) {
      action_();
    }
  }

protected:
  void paintEvent(QPaintEvent *event) override {
    if (!ownerDrawn_) {
      QPushButton::paintEvent(event);
      return;
    }

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);
    const QPalette pal = palette();
    const bool hovered = underMouse();
    const bool pressed = isDown();
    const bool active = isCheckable() && isChecked();
    const QColor base = pal.color(isEnabled() ? QPalette::Active
                                              : QPalette::Disabled,
                                  QPalette::Button);
    const QColor accent = pal.color(QPalette::Highlight);
    const QColor fill = pressed
                            ? blendColor(base, accent, 0.46)
                            : active
                                  ? blendColor(base, accent, 0.34)
                                  : hovered
                                        ? blendColor(base, accent, 0.14)
                                        : base;
    const QRectF surface = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    painter.setPen(pal.color(QPalette::Mid));
    painter.setBrush(fill);
    painter.drawRoundedRect(surface, 4.0, 4.0);

    if (active) {
      painter.setPen(Qt::NoPen);
      painter.setBrush(accent);
      painter.drawRoundedRect(QRectF(3.0, 7.0, 3.0,
                                     qMax(4, height() - 14)),
                              1.5, 1.5);
    }

    QFont textFont = font();
    textFont.setWeight(active ? QFont::DemiBold : QFont::Normal);
    painter.setFont(textFont);
    painter.setPen(pal.color(isEnabled() ? QPalette::Active
                                         : QPalette::Disabled,
                             QPalette::ButtonText));
    const QRect textRect = rect().adjusted(active ? 12 : 8, 0, -8, 0);
    painter.drawText(textRect, Qt::AlignVCenter | Qt::AlignLeft, text());

    if (hasFocus()) {
      QPen focusPen(accent);
      focusPen.setStyle(Qt::DashLine);
      painter.setPen(focusPen);
      painter.setBrush(Qt::NoBrush);
      painter.drawRoundedRect(surface.adjusted(2, 2, -2, -2), 3.0, 3.0);
    }
  }

  void mouseReleaseEvent(QMouseEvent *event) override {
    const bool activate =
        event && event->button() == Qt::LeftButton && isEnabled() &&
        isDown() && rect().contains(event->position().toPoint());
    QPushButton::mouseReleaseEvent(event);
    if (activate && action_) {
      action_();
    }
  }

  void keyReleaseEvent(QKeyEvent *event) override {
    const bool activate =
        event && isEnabled() &&
        (event->key() == Qt::Key_Space || event->key() == Qt::Key_Return ||
         event->key() == Qt::Key_Enter);
    QPushButton::keyReleaseEvent(event);
    if (activate && action_) {
      action_();
    }
  }

private:
  std::function<void()> action_;
  bool ownerDrawn_ = false;
};

class ComponentStackItemDelegate final : public QStyledItemDelegate {
 public:
  using QStyledItemDelegate::QStyledItemDelegate;

  QSize sizeHint(const QStyleOptionViewItem &, const QModelIndex &) const override {
    return QSize(0, 30);
  }

  void paint(QPainter *painter, const QStyleOptionViewItem &option,
             const QModelIndex &index) const override {
    if (!painter) {
      return;
    }
    const QPalette pal = option.palette;
    const bool selected = option.state.testFlag(QStyle::State_Selected);
    const bool hovered = option.state.testFlag(QStyle::State_MouseOver);
    const QRectF row = QRectF(option.rect).adjusted(1.5, 1.5, -1.5, -1.5);
    const QColor base = pal.color(QPalette::Base);
    const QColor accent = pal.color(QPalette::Highlight);
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setPen(pal.color(QPalette::Mid));
    painter->setBrush(selected ? blendColor(base, accent, 0.42)
                               : hovered ? blendColor(base, accent, 0.12)
                                         : pal.color(QPalette::AlternateBase));
    painter->drawRoundedRect(row, 3.0, 3.0);
    painter->setPen(selected ? pal.color(QPalette::HighlightedText)
                             : pal.color(QPalette::Text));
    QFont itemFont = option.font;
    itemFont.setWeight(selected ? QFont::DemiBold : QFont::Normal);
    painter->setFont(itemFont);
    painter->drawText(option.rect.adjusted(10, 0, -8, 0),
                      Qt::AlignVCenter | Qt::AlignLeft,
                      index.data(Qt::DisplayRole).toString());
    painter->restore();
  }
};

class ComponentDivider final : public QFrame {
 public:
  using QFrame::QFrame;

 protected:
  void paintEvent(QPaintEvent*) override {
    QPainter painter(this);
    painter.setPen(palette().color(QPalette::Mid));
    painter.drawLine(rect().left(), rect().center().y(),
                     rect().right(), rect().center().y());
  }
};

class InspectorCanvasSurface final : public QWidget {
 public:
  using QWidget::QWidget;

 protected:
  void paintEvent(QPaintEvent*) override {
    QPainter painter(this);
    const QPalette pal = palette();
    painter.fillRect(rect(), pal.color(QPalette::Window));
  }
};

class InspectorChromeLabel final : public QLabel {
 public:
  enum class Role { Section, Active, Summary };

  InspectorChromeLabel(const QString& text, Role role,
                       QWidget* parent = nullptr)
      : QLabel(text, parent), role_(role) {
    setAttribute(Qt::WA_TranslucentBackground, true);
    if (role_ != Role::Summary) {
      QFont labelFont = font();
      labelFont.setWeight(QFont::DemiBold);
      setFont(labelFont);
    }
  }

 protected:
  void paintEvent(QPaintEvent*) override {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);
    const QPalette pal = palette();
    // This label is translucent, so Qt does not erase the previous glyphs
    // when its text changes. Clear the backing area before owner-drawing to
    // prevent the visible text trails during inspector refreshes.
    painter.fillRect(rect(), pal.color(QPalette::Window));
    QRect contentRect = rect();
    if (role_ == Role::Active) {
      const QColor accent = pal.color(QPalette::Highlight);
      painter.setPen(Qt::NoPen);
      painter.setBrush(blendColor(pal.color(QPalette::Window), accent, 0.16));
      painter.drawRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5),
                              4.0, 4.0);
      painter.setPen(accent);
      painter.setBrush(Qt::NoBrush);
      painter.drawLine(2, 4, 2, height() - 5);
      contentRect.adjust(10, 0, -6, 0);
    } else if (role_ == Role::Section) {
      painter.setPen(pal.color(QPalette::Mid));
      painter.drawLine(contentRect.left(), contentRect.bottom(),
                       contentRect.right(), contentRect.bottom());
      contentRect.adjust(0, 0, 0, -3);
    } else {
      contentRect.adjust(2, 0, -2, 0);
    }
    painter.setFont(font());
    painter.setPen(isEnabled() ? pal.color(QPalette::WindowText)
                               : pal.color(QPalette::Disabled,
                                           QPalette::WindowText));
    int flags = Qt::AlignLeft | Qt::AlignVCenter;
    if (wordWrap()) {
      flags |= Qt::TextWordWrap;
    }
    painter.drawText(contentRect, flags, text());
  }

 private:
  Role role_;
};

class InspectorPropertySurface final : public QWidget {
 public:
  explicit InspectorPropertySurface(QWidget* editor, QWidget* parent = nullptr)
      : QWidget(parent) {
    auto* surfaceLayout = new QVBoxLayout(this);
    surfaceLayout->setContentsMargins(6, 6, 6, 6);
    surfaceLayout->setSpacing(0);
    if (editor) {
      surfaceLayout->addWidget(editor, 1);
    }
  }

  void setEditor(QWidget* editor) {
    if (!editor) {
      return;
    }
    if (auto* surfaceLayout = static_cast<QVBoxLayout*>(layout())) {
      surfaceLayout->addWidget(editor, 1);
    }
  }

 protected:
  void paintEvent(QPaintEvent*) override {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    const QPalette pal = palette();
    const QRectF panelRect = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    painter.setPen(pal.color(QPalette::Mid));
    painter.setBrush(pal.color(QPalette::Base));
    painter.drawRoundedRect(panelRect, 4.0, 4.0);
  }
};

class EffectPanelSurface final : public QWidget {
 public:
  enum class Role { Header, Stack, Detail };

  explicit EffectPanelSurface(Role role, QWidget* parent = nullptr)
      : QWidget(parent), role_(role) {}

 protected:
  void paintEvent(QPaintEvent*) override {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    const QPalette pal = palette();
    const QColor base = role_ == Role::Header
        ? pal.color(QPalette::AlternateBase)
        : pal.color(QPalette::Window);
    painter.fillRect(rect(), base);
    painter.setPen(pal.color(QPalette::Mid));
    if (role_ == Role::Header) {
      painter.drawLine(rect().bottomLeft(), rect().bottomRight());
    } else {
      painter.drawRoundedRect(
          QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), 4.0, 4.0);
    }
  }

 private:
  Role role_;
};

class EffectRackSurface final : public QWidget {
 public:
  explicit EffectRackSurface(const QString& title, QWidget* parent = nullptr)
      : QWidget(parent), title_(title) {}

  void setTitle(const QString& title) {
    if (title_ == title) {
      return;
    }
    title_ = title;
    update();
  }

 protected:
  void paintEvent(QPaintEvent*) override {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);
    const QPalette pal = palette();
    const QRectF surfaceRect =
        QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    painter.setPen(pal.color(QPalette::Mid));
    painter.setBrush(pal.color(QPalette::AlternateBase));
    painter.drawRoundedRect(surfaceRect, 4.0, 4.0);
    QFont titleFont = font();
    titleFont.setWeight(QFont::DemiBold);
    painter.setFont(titleFont);
    painter.setPen(pal.color(QPalette::WindowText));
    painter.drawText(QRect(9, 0, qMax(0, width() - 18), 28),
                     Qt::AlignLeft | Qt::AlignVCenter, title_);
    painter.setPen(pal.color(QPalette::Mid));
    painter.drawLine(6, 28, width() - 7, 28);
  }

 private:
  QString title_;
};

} // namespace detail
} // namespace Artifact
