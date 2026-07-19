module;
#include <utility>
#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QDialog>
#include <QFrame>
#include <QGroupBox>
#include <QHeaderView>
#include <QLineEdit>
#include <QMargins>
#include <QMenu>
#include <QMenuBar>
#include <QPalette>
#include <QPainter>
#include <QPointer>
#include <QPushButton>
#include <QSize>
#include <QSizePolicy>
#include <QScrollArea>
#include <QSlider>
#include <QSpinBox>
#include <QTabBar>
#include <QTableView>
#include <QToolBar>
#include <QToolButton>
#include <QTreeView>
#include <QListView>
#include <QStyleOptionMenuItem>
#include <QStyleOptionToolButton>
#include <QStyleFactory>
#include <QBitmap>
#include <QEvent>
#include <QFontMetrics>
#include <algorithm>
#include <cmath>
#include <vector>
#ifdef _WIN32
#include <qt_windows.h>
#endif

module Widgets.CommonStyle;

import Widgets.Utils.CSS;

namespace Artifact {

class StudioSectionStack::Impl {
public:
  struct Item {
    QPointer<QWidget> widget;
    bool expands = false;
  };

  std::vector<Item> items;
  int spacing = 6;
};

StudioSectionStack::StudioSectionStack(QWidget* parent)
    : QWidget(parent), impl_(new Impl()) {
  setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
}

StudioSectionStack::~StudioSectionStack() { delete impl_; }

void StudioSectionStack::appendWidget(QWidget* widget, const bool expands) {
  if (!impl_ || !widget) return;
  for (auto& item : impl_->items) {
    if (item.widget == widget) {
      item.expands = expands;
      updateGeometry();
      updateChildGeometry();
      return;
    }
  }
  const bool explicitlyHidden =
      widget->testAttribute(Qt::WA_WState_ExplicitShowHide) &&
      widget->isHidden();
  widget->setParent(this);
  widget->installEventFilter(this);
  impl_->items.push_back({widget, expands});
  if (!explicitlyHidden) widget->show();
  updateGeometry();
  updateChildGeometry();
}

void StudioSectionStack::removeWidget(QWidget* widget) {
  if (!impl_ || !widget) return;
  widget->removeEventFilter(this);
  impl_->items.erase(
      std::remove_if(impl_->items.begin(), impl_->items.end(),
                     [widget](const Impl::Item& item) {
                       return !item.widget || item.widget == widget;
                     }),
      impl_->items.end());
  if (widget->parentWidget() == this) widget->setParent(nullptr);
  updateGeometry();
  updateChildGeometry();
}

void StudioSectionStack::setWidgetExpands(QWidget* widget,
                                          const bool expands) {
  if (!impl_ || !widget) return;
  for (auto& item : impl_->items) {
    if (item.widget == widget) {
      item.expands = expands;
      updateGeometry();
      updateChildGeometry();
      return;
    }
  }
}

void StudioSectionStack::setSpacing(const int spacing) {
  if (!impl_) return;
  const int normalized = std::max(0, spacing);
  if (impl_->spacing == normalized) return;
  impl_->spacing = normalized;
  updateGeometry();
  updateChildGeometry();
}

int StudioSectionStack::spacing() const {
  return impl_ ? impl_->spacing : 0;
}

int StudioSectionStack::count() const {
  if (!impl_) return 0;
  return static_cast<int>(std::count_if(
      impl_->items.begin(), impl_->items.end(),
      [](const Impl::Item& item) { return static_cast<bool>(item.widget); }));
}

QWidget* StudioSectionStack::widgetAt(const int index) const {
  if (!impl_ || index < 0) return nullptr;
  int liveIndex = 0;
  for (const auto& item : impl_->items) {
    if (!item.widget) continue;
    if (liveIndex++ == index) return item.widget;
  }
  return nullptr;
}

QSize StudioSectionStack::sizeHint() const {
  if (!impl_) return QWidget::sizeHint();
  const QMargins margins = contentsMargins();
  int width = 0;
  int height = 0;
  int visibleCount = 0;
  for (const auto& item : impl_->items) {
    if (!item.widget || item.widget->isHidden()) continue;
    const QSize hint = item.widget->sizeHint().expandedTo(
        item.widget->minimumSizeHint());
    width = std::max(width, hint.width());
    height += hint.height();
    ++visibleCount;
  }
  if (visibleCount > 1) height += impl_->spacing * (visibleCount - 1);
  return QSize(width + margins.left() + margins.right(),
               height + margins.top() + margins.bottom());
}

QSize StudioSectionStack::minimumSizeHint() const {
  if (!impl_) return QWidget::minimumSizeHint();
  const QMargins margins = contentsMargins();
  int width = 0;
  int height = 0;
  int visibleCount = 0;
  for (const auto& item : impl_->items) {
    if (!item.widget || item.widget->isHidden()) continue;
    const QSize hint = item.widget->minimumSizeHint().expandedTo(
        item.widget->minimumSize());
    width = std::max(width, hint.width());
    height += hint.height();
    ++visibleCount;
  }
  if (visibleCount > 1) height += impl_->spacing * (visibleCount - 1);
  return QSize(width + margins.left() + margins.right(),
               height + margins.top() + margins.bottom());
}

void StudioSectionStack::resizeEvent(QResizeEvent* event) {
  QWidget::resizeEvent(event);
  updateChildGeometry();
}

bool StudioSectionStack::eventFilter(QObject* watched, QEvent* event) {
  if (impl_ && event && event->type() == QEvent::Destroy) {
    impl_->items.erase(
        std::remove_if(impl_->items.begin(), impl_->items.end(),
                       [watched](const Impl::Item& item) {
                         return !item.widget || item.widget.data() == watched;
                       }),
        impl_->items.end());
    updateGeometry();
    updateChildGeometry();
    return QWidget::eventFilter(watched, event);
  }
  if (event && (event->type() == QEvent::Show ||
                event->type() == QEvent::Hide ||
                event->type() == QEvent::LayoutRequest ||
                event->type() == QEvent::FontChange ||
                event->type() == QEvent::StyleChange)) {
    updateGeometry();
    updateChildGeometry();
  }
  return QWidget::eventFilter(watched, event);
}

void StudioSectionStack::updateChildGeometry() {
  if (!impl_) return;
  impl_->items.erase(
      std::remove_if(impl_->items.begin(), impl_->items.end(),
                     [](const Impl::Item& item) { return !item.widget; }),
      impl_->items.end());
  std::vector<Impl::Item*> visible;
  visible.reserve(impl_->items.size());
  int fixedHeight = 0;
  int expandingCount = 0;
  for (auto& item : impl_->items) {
    if (!item.widget || item.widget->isHidden()) continue;
    visible.push_back(&item);
    if (item.expands) {
      ++expandingCount;
    } else {
      const QSize hint = item.widget->sizeHint().expandedTo(
          item.widget->minimumSizeHint());
      fixedHeight += std::clamp(hint.height(), item.widget->minimumHeight(),
                                item.widget->maximumHeight());
    }
  }
  if (visible.empty()) return;
  const QMargins margins = contentsMargins();
  const int totalSpacing = impl_->spacing *
      std::max(0, static_cast<int>(visible.size()) - 1);
  const int availableHeight =
      std::max(0, height() - margins.top() - margins.bottom() - totalSpacing);
  const int expandingHeight = expandingCount > 0
      ? std::max(0, availableHeight - fixedHeight) / expandingCount
      : 0;
  const int availableWidth =
      std::max(0, width() - margins.left() - margins.right());
  int y = margins.top();
  for (auto* item : visible) {
    auto* widget = item->widget.data();
    if (!widget) continue;
    const QSize hint = widget->sizeHint().expandedTo(widget->minimumSizeHint());
    int childHeight = item->expands ? expandingHeight : hint.height();
    childHeight = std::clamp(childHeight, widget->minimumHeight(),
                             widget->maximumHeight());
    const int childWidth = std::clamp(availableWidth, widget->minimumWidth(),
                                      widget->maximumWidth());
    widget->setGeometry(margins.left(), y, childWidth, childHeight);
    y += childHeight + impl_->spacing;
  }
}

ArtifactCommonStyle::ArtifactCommonStyle(QStyle* baseStyle)
    : QProxyStyle(baseStyle ? baseStyle : QStyleFactory::create(QStringLiteral("Fusion"))) {}

ArtifactCommonStyle::~ArtifactCommonStyle() = default;

namespace {
void ensureMenuActionIconsVisible(QMenu* menu)
{
  if (!menu) {
    return;
  }
  for (QAction* action : menu->actions()) {
    if (action && !action->icon().isNull()) {
      action->setIconVisibleInMenu(true);
    }
  }
}

void scaleMenuFont(QWidget* widget)
{
  if (!widget) {
    return;
  }
  if (widget->property("artifactMenuFontScaled").toBool()) {
    return;
  }
  QFont font = widget->font();
  const int pointSize = font.pointSize();
  if (pointSize > 0) {
    font.setPointSize(std::max(11, static_cast<int>(std::lround(static_cast<qreal>(pointSize) * 1.15))));
  } else {
    const qreal pointSizeF = font.pointSizeF();
    if (pointSizeF > 0.0) {
      font.setPointSizeF(std::max<qreal>(11.0, pointSizeF * 1.15));
    }
  }
  widget->setFont(font);
  widget->setProperty("artifactMenuFontScaled", true);
}

void drawFramedToolButtonSurface(const QStyleOption* option, QPainter* painter, const QWidget* widget)
{
  if (!widget || !widget->property("artifactFramedToolButton").toBool()) {
    return;
  }
  if (!option || !painter) {
    return;
  }

  const auto& theme = ArtifactCore::currentDCCTheme();
  QColor border(theme.borderColor);
  QColor fill(theme.secondaryBackgroundColor);
  const bool isPlayButton = widget->property("artifactPlayButton").toBool();
  const bool isSpeedButton = widget->property("artifactSpeedPresetButton").toBool();
  const QRect drawRect = option->rect.adjusted(0, 0, -1, -1);

  if (isPlayButton) {
    fill = QColor(QStringLiteral("#8ADFFF"));
    border = QColor(QStringLiteral("#6BB7DB"));
    if (option->state.testFlag(QStyle::State_Sunken) || option->state.testFlag(QStyle::State_On)) {
      fill = QColor(QStringLiteral("#67C9F0"));
      border = QColor(QStringLiteral("#4F9ECC"));
    } else if (option->state.testFlag(QStyle::State_MouseOver)) {
      fill = QColor(QStringLiteral("#98E6FF"));
      border = QColor(QStringLiteral("#5EABCF"));
    }
  }
  if (isSpeedButton) {
    border = QColor(QStringLiteral("#BFEAFF"));
    fill = QColor(QStringLiteral("#BEEBFF"));
    if (option->state.testFlag(QStyle::State_On)) {
      border = QColor(QStringLiteral("#8FD8FF"));
      fill = QColor(QStringLiteral("#7DC8FF"));
    } else if (option->state.testFlag(QStyle::State_MouseOver)) {
      fill = QColor(QStringLiteral("#CFF4FF"));
    }
  } else if (option->state.testFlag(QStyle::State_Sunken) || option->state.testFlag(QStyle::State_On)) {
    if (!isPlayButton) {
      border = QColor(theme.accentColor).lighter(110);
      fill = QColor(theme.accentColor).darker(165);
    }
  } else if (option->state.testFlag(QStyle::State_MouseOver) && !isPlayButton) {
    border = QColor(theme.accentColor).lighter(110);
    fill = QColor(theme.secondaryBackgroundColor).lighter(106);
  } else if (option->state.testFlag(QStyle::State_MouseOver) && isPlayButton) {
    // play button handles hover above
  }

  painter->save();
  painter->setRenderHint(QPainter::Antialiasing, true);
  if (isPlayButton) {
    painter->setPen(Qt::NoPen);
    painter->setBrush(fill);
    painter->drawEllipse(drawRect.adjusted(1, 1, -1, -1));
    painter->setPen(QPen(border, 1));
    painter->setBrush(Qt::NoBrush);
    painter->drawEllipse(drawRect.adjusted(1, 1, -1, -1));
  } else {
    const qreal radius = isSpeedButton ? 4.0 : 2.0;
    painter->setPen(Qt::NoPen);
    painter->setBrush(fill);
    painter->drawRoundedRect(drawRect.adjusted(1, 1, -1, -1), radius, radius);
    painter->setPen(QPen(border, 1));
    painter->setBrush(Qt::NoBrush);
    painter->drawRoundedRect(drawRect.adjusted(1, 1, -1, -1), radius, radius);
  }
  painter->restore();
}

#ifdef _WIN32
using DwmSetWindowAttributeFn = HRESULT(WINAPI*)(HWND, DWORD, LPCVOID, DWORD);

static bool tryApplyDwmRoundCorners(QWidget* w) {
  static HMODULE dwmModule = ::LoadLibraryW(L"dwmapi.dll");
  if (!dwmModule) return false;
  static const auto setAttr = reinterpret_cast<DwmSetWindowAttributeFn>(
      ::GetProcAddress(dwmModule, "DwmSetWindowAttribute"));
  if (!setAttr) return false;
  HWND hwnd = reinterpret_cast<HWND>(w->winId());
  if (!hwnd) return false;
  // DWMWA_WINDOW_CORNER_PREFERENCE = 33, DWMWCP_ROUNDSMALL = 3
  DWORD pref = 3;
  return SUCCEEDED(setAttr(hwnd, 33, &pref, sizeof(pref)));
}
#endif

class RoundedWindowMaskFilter : public QObject {
  int radius_;
  bool onlyIfFrameless_;
  bool dwmApplied_ = false;
  QSize lastMaskedSize_;
public:
  explicit RoundedWindowMaskFilter(QObject* parent, int r, bool onlyIfFrameless = false)
    : QObject(parent), radius_(r), onlyIfFrameless_(onlyIfFrameless) {}
  bool eventFilter(QObject* watched, QEvent* event) override {
    if (event->type() == QEvent::Show || event->type() == QEvent::Resize) {
      if (auto* w = qobject_cast<QWidget*>(watched)) {
        if (event->type() == QEvent::Show) {
          ensureMenuActionIconsVisible(qobject_cast<QMenu*>(w));
        }
        if (onlyIfFrameless_ && !(w->windowFlags() & Qt::FramelessWindowHint))
          return false;
        if (dwmApplied_) return false;
#ifdef _WIN32
        if (event->type() == QEvent::Show && tryApplyDwmRoundCorners(w)) {
          dwmApplied_ = true;
          w->clearMask();
          return false;
        }
#endif
        const QSize size = w->size();
        if (!size.isEmpty() && size != lastMaskedSize_) {
          lastMaskedSize_ = size;
          QBitmap bm(size);
          bm.fill(Qt::color0);
          QPainter p(&bm);
          p.setRenderHint(QPainter::Antialiasing);
          p.setBrush(Qt::color1);
          p.setPen(Qt::NoPen);
          p.drawRoundedRect(QRectF(w->rect()).adjusted(0.5, 0.5, -0.5, -0.5), radius_, radius_);
          w->setMask(bm);
        }
      }
    }
    return false;
  }
};
} // namespace

void ArtifactCommonStyle::polish(QWidget* widget)
{
  if (!widget) {
    return;
  }

  widget->setAttribute(Qt::WA_StyledBackground, true);
  const auto& theme = ArtifactCore::currentDCCTheme();
  const QColor background(theme.backgroundColor);
  const QColor surface(theme.secondaryBackgroundColor);
  const QColor text(theme.textColor);
  const QColor accent(theme.accentColor);
  const QColor border(theme.borderColor);

  auto applyWindowPalette = [&](QWidget* w) {
    if (!w) return;
    w->setAutoFillBackground(true);
    QPalette pal = w->palette();
    pal.setColor(QPalette::Window, background);
    pal.setColor(QPalette::WindowText, text);
    pal.setColor(QPalette::Base, surface);
    pal.setColor(QPalette::Text, text);
    pal.setColor(QPalette::Button, surface);
    pal.setColor(QPalette::ButtonText, text);
    pal.setColor(QPalette::Highlight, accent);
    pal.setColor(QPalette::HighlightedText, background);
    pal.setColor(QPalette::Mid, border);
    w->setPalette(pal);
  };

  if (qobject_cast<QToolBar*>(widget) ||
      qobject_cast<QMenuBar*>(widget) ||
      qobject_cast<QMenu*>(widget) ||
      qobject_cast<QTabBar*>(widget) ||
      qobject_cast<QGroupBox*>(widget) ||
      qobject_cast<QFrame*>(widget) ||
      qobject_cast<QScrollArea*>(widget) ||
      qobject_cast<QTreeView*>(widget) ||
      qobject_cast<QListView*>(widget) ||
      qobject_cast<QTableView*>(widget) ||
      qobject_cast<QLineEdit*>(widget) ||
      qobject_cast<QComboBox*>(widget) ||
      qobject_cast<QSpinBox*>(widget) ||
      qobject_cast<QPushButton*>(widget) ||
      qobject_cast<QSlider*>(widget)) {
    applyWindowPalette(widget);
  }

  if (qobject_cast<QMenuBar*>(widget) || qobject_cast<QMenu*>(widget)) {
    scaleMenuFont(widget);
    ensureMenuActionIconsVisible(qobject_cast<QMenu*>(widget));
    QPalette pal = widget->palette();
    pal.setColor(QPalette::Highlight, QColor(theme.accentColor).lighter(108));
    pal.setColor(QPalette::HighlightedText, text);
    pal.setColor(QPalette::Button, QColor(theme.secondaryBackgroundColor));
    pal.setColor(QPalette::Window, QColor(theme.secondaryBackgroundColor));
    pal.setColor(QPalette::Base, QColor(theme.backgroundColor));
    widget->setPalette(pal);
  }

  if (qobject_cast<QMenu*>(widget) && !widget->property("artifactMenuMaskInstalled").toBool()) {
    widget->setProperty("artifactMenuMaskInstalled", true);
    widget->installEventFilter(new RoundedWindowMaskFilter(widget, 6));
  }

  if (qobject_cast<QDialog*>(widget) && !widget->property("artifactDialogMaskInstalled").toBool()) {
    widget->setProperty("artifactDialogMaskInstalled", true);
    widget->installEventFilter(new RoundedWindowMaskFilter(widget, 8, true));
  }

  QProxyStyle::polish(widget);
}

void ArtifactCommonStyle::polish(QPalette& palette)
{
  QProxyStyle::polish(palette);
}

int ArtifactCommonStyle::pixelMetric(PixelMetric metric, const QStyleOption* option,
                                     const QWidget* widget) const
{
  switch (metric) {
  case PM_ToolBarIconSize:
    return 18;
  case PM_TabBarTabHSpace:
    return 12;
  case PM_TabBarTabVSpace:
    return 8;
  case PM_DefaultFrameWidth:
    return 1;
  case PM_ButtonMargin:
    return 6;
  case PM_ScrollBarExtent:
    return 14;
  case PM_SliderThickness:
    return 16;
  case PM_SliderLength:
    return 18;
  default:
    break;
  }
  return QProxyStyle::pixelMetric(metric, option, widget);
}

QSize ArtifactCommonStyle::sizeFromContents(ContentsType type,
                                            const QStyleOption* option,
                                            const QSize& contentsSize,
                                            const QWidget* widget) const
{
  if (type == CT_MenuBarItem) {
    if (const auto* menuItem = qstyleoption_cast<const QStyleOptionMenuItem*>(option)) {
      const QFontMetrics& fm = menuItem->fontMetrics;
      const int textWidth = fm.horizontalAdvance(menuItem->text);
      const int textHeight = fm.height();
      const int iconWidth = menuItem->icon.isNull() ? 0 : 19;
      const int spacing = menuItem->icon.isNull() ? 0 : 5;
      return QSize(std::max(contentsSize.width(), textWidth + iconWidth + spacing + 9),
                   std::max(contentsSize.height(), textHeight + 7));
    }
  }

  return QProxyStyle::sizeFromContents(type, option, contentsSize, widget);
}

void ArtifactCommonStyle::drawControl(ControlElement element, const QStyleOption* option,
                                      QPainter* painter, const QWidget* widget) const
{
  if (!option || !painter) {
    return QProxyStyle::drawControl(element, option, painter, widget);
  }

  const auto& theme = ArtifactCore::currentDCCTheme();
  const QColor menuSurface(theme.secondaryBackgroundColor);
  const QColor menuText(theme.textColor);
  const QColor menuHover = QColor(theme.accentColor).lighter(108);
  const QColor menuBorder = QColor(theme.borderColor);

  if (element == CE_MenuItem) {
    if (const auto* menuItem = qstyleoption_cast<const QStyleOptionMenuItem*>(option)) {
      painter->save();
      painter->setRenderHint(QPainter::Antialiasing, true);

      if (menuItem->menuItemType == QStyleOptionMenuItem::Separator) {
        const int y = menuItem->rect.center().y();
        painter->setPen(QPen(menuBorder, 1));
        painter->drawLine(menuItem->rect.left() + 8, y, menuItem->rect.right() - 8, y);
        painter->restore();
        return;
      }

      QRect itemRect = menuItem->rect.adjusted(2, 1, -2, -1);
      if (menuItem->state.testFlag(State_Selected)) {
        painter->setPen(Qt::NoPen);
        painter->setBrush(menuHover);
        painter->drawRoundedRect(itemRect, 4.0, 4.0);
      }

      QStyleOptionMenuItem copy(*menuItem);
      const bool enabled = menuItem->state.testFlag(State_Enabled);
      const QColor disabledText = menuText.darker(155);
      copy.palette.setColor(QPalette::ButtonText, enabled ? menuText : disabledText);
      copy.palette.setColor(QPalette::Text, enabled ? menuText : disabledText);
      copy.palette.setColor(QPalette::WindowText, enabled ? menuText : disabledText);
      copy.palette.setColor(QPalette::HighlightedText, enabled ? menuText : disabledText);
      copy.palette.setColor(QPalette::Highlight, enabled ? menuHover : menuSurface);
      copy.palette.setColor(QPalette::Disabled, QPalette::ButtonText, disabledText);
      copy.palette.setColor(QPalette::Disabled, QPalette::Text, disabledText);
      copy.palette.setColor(QPalette::Disabled, QPalette::WindowText, disabledText);
      painter->restore();
      return QProxyStyle::drawControl(element, &copy, painter, widget);
    }
  }

  if (element == CE_MenuBarItem) {
    if (const auto *menuItem = qstyleoption_cast<const QStyleOptionMenuItem *>(option)) {
      painter->save();
      painter->setRenderHint(QPainter::Antialiasing, true);

      const QRect itemRect = menuItem->rect.adjusted(1, 2, -1, -2);
      const bool enabled = menuItem->state.testFlag(State_Enabled);
      const QColor disabledText = menuText.darker(145);
      if (enabled && (menuItem->state.testFlag(State_Selected) ||
          menuItem->state.testFlag(State_Sunken))) {
        painter->setPen(Qt::NoPen);
        painter->setBrush(menuHover);
        painter->drawRoundedRect(itemRect, 4.0, 4.0);
      }

      const QFontMetrics& fm = menuItem->fontMetrics;
      const int textWidth = fm.horizontalAdvance(menuItem->text);
      const bool hasIcon = !menuItem->icon.isNull();
      const int iconSize = hasIcon ? std::min(19, std::max(13, itemRect.height() - 5)) : 0;
      const int spacing = hasIcon ? 5 : 0;
      const int contentWidth = textWidth + iconSize + spacing;
      int x = itemRect.left() + std::max(0, (itemRect.width() - contentWidth) / 2);

      if (hasIcon) {
        const QRect iconRect(x, itemRect.center().y() - iconSize / 2, iconSize, iconSize);
        const QIcon::Mode mode = enabled ? QIcon::Normal : QIcon::Disabled;
        menuItem->icon.paint(painter, iconRect, Qt::AlignCenter, mode);
        x += iconSize + spacing;
      }

      painter->setPen(enabled ? menuText : disabledText);
      painter->drawText(QRect(x, itemRect.top(), textWidth, itemRect.height()),
                        Qt::AlignVCenter | Qt::TextShowMnemonic, menuItem->text);
      painter->restore();
      return;
    }
  }

  QProxyStyle::drawControl(element, option, painter, widget);
}

void ArtifactCommonStyle::drawPrimitive(PrimitiveElement element, const QStyleOption* option,
                                        QPainter* painter, const QWidget* widget) const
{
  if (element == PE_PanelMenu) {
    if (!option || !painter) {
      return QProxyStyle::drawPrimitive(element, option, painter, widget);
    }
    const auto& theme = ArtifactCore::currentDCCTheme();
    const QColor menuSurface(theme.secondaryBackgroundColor);
    const QColor menuBorder(theme.borderColor);
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setPen(Qt::NoPen);
    painter->setBrush(menuSurface);
    painter->drawRoundedRect(QRectF(option->rect), 6.0, 6.0);
    painter->setPen(QPen(menuBorder, 1.0));
    painter->setBrush(Qt::NoBrush);
    painter->drawRoundedRect(QRectF(option->rect).adjusted(0.5, 0.5, -0.5, -0.5), 6.0, 6.0);
    painter->restore();
    return;
  }
  if (element == PE_FrameMenu) {
    // Background + border already handled in PE_PanelMenu
    return;
  }
  if (element == PE_Widget) {
    if (widget && widget->property("artifactDockTab").toBool()) {
      if (!option || !painter) {
        return QProxyStyle::drawPrimitive(element, option, painter, widget);
      }
      painter->save();
      painter->setRenderHint(QPainter::Antialiasing, true);
      painter->setPen(Qt::NoPen);
      painter->setBrush(option->palette.color(QPalette::Window));
      painter->drawRoundedRect(QRectF(option->rect), 4.0, 4.0);
      painter->restore();
      return;
    }
    if (const auto* dlg = qobject_cast<const QDialog*>(widget)) {
      if (dlg->windowFlags().testFlag(Qt::FramelessWindowHint) && option && painter) {
        const auto& theme = ArtifactCore::currentDCCTheme();
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing, true);
        painter->setPen(Qt::NoPen);
        painter->setBrush(QColor(theme.backgroundColor));
        painter->drawRoundedRect(QRectF(option->rect), 8.0, 8.0);
        painter->setPen(QPen(QColor(theme.borderColor), 1.0));
        painter->setBrush(Qt::NoBrush);
        painter->drawRoundedRect(QRectF(option->rect).adjusted(0.5, 0.5, -0.5, -0.5), 8.0, 8.0);
        painter->restore();
        return;
      }
    }
  }
  if (element == PE_PanelButtonTool) {
    drawFramedToolButtonSurface(option, painter, widget);
    return;
  }
  QProxyStyle::drawPrimitive(element, option, painter, widget);
}

void ArtifactCommonStyle::drawComplexControl(ComplexControl control, const QStyleOptionComplex* option,
                                             QPainter* painter, const QWidget* widget) const
{
  QProxyStyle::drawComplexControl(control, option, painter, widget);
}

}
