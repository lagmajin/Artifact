module;
#include <QApplication>
#include <QComboBox>
#include <QFrame>
#include <QGroupBox>
#include <QHeaderView>
#include <QLineEdit>
#include <QMenuBar>
#include <QPalette>
#include <QPushButton>
#include <QScrollArea>
#include <QSlider>
#include <QSpinBox>
#include <QTabBar>
#include <QTableView>
#include <QToolBar>
#include <QTreeView>
#include <QListView>
#include <QStyleFactory>

module Widgets.CommonStyle;

import Widgets.Utils.CSS;

namespace Artifact {

ArtifactCommonStyle::ArtifactCommonStyle(QStyle* baseStyle)
    : QProxyStyle(baseStyle ? baseStyle : QStyleFactory::create(QStringLiteral("Fusion"))) {}

ArtifactCommonStyle::~ArtifactCommonStyle() = default;

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

}
