module;
#include <utility>
#include <QApplication>
#include <QComboBox>
#include <QFrame>
#include <QGroupBox>
#include <QHeaderView>
#include <QLineEdit>
#include <QMenu>
#include <QMenuBar>
#include <QPalette>
#include <QPainter>
#include <QPushButton>
#include <QScrollArea>
#include <QSlider>
#include <QSpinBox>
#include <QTabBar>
#include <QTableView>
#include <QToolBar>
#include <QToolButton>
#include <QTreeView>
#include <QListView>
#include <QStyleOptionToolButton>
#include <QStyleFactory>

module Widgets.CommonStyle;

import Widgets.Utils.CSS;

namespace Artifact {

ArtifactCommonStyle::ArtifactCommonStyle(QStyle* baseStyle)
    : QProxyStyle(baseStyle ? baseStyle : QStyleFactory::create(QStringLiteral("Fusion"))) {}

ArtifactCommonStyle::~ArtifactCommonStyle() = default;

namespace {
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
    font.setPointSizeF(static_cast<qreal>(pointSize) * 1.2);
  } else {
    const qreal pointSizeF = font.pointSizeF();
    if (pointSizeF > 0.0) {
      font.setPointSizeF(pointSizeF * 1.2);
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
    border = QColor(QStringLiteral("#BFEAFF"));
    fill = QColor(QStringLiteral("#8ADFFF"));
    if (option->state.testFlag(QStyle::State_Sunken) || option->state.testFlag(QStyle::State_On)) {
      fill = QColor(QStringLiteral("#67C9F0"));
    } else if (option->state.testFlag(QStyle::State_MouseOver)) {
      fill = QColor(QStringLiteral("#98E6FF"));
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

void ArtifactCommonStyle::drawPrimitive(PrimitiveElement element, const QStyleOption* option,
                                        QPainter* painter, const QWidget* widget) const
{
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
