module;
#include <utility>
#include <QProxyStyle>
#include <QPainter>
#include <QLabel>
#include <QPoint>
#include <QRect>
#include <QStyleOption>
#include <wobjectimpl.h>
#if defined(ARTIFACT_QADS_COMPAT)
#include "DockWidget.h"
#include "DockAreaWidget.h"
#include "DockContainerWidget.h"
#include "DockWidgetTab.h"
#endif

module Widgets.Dock.GlowStyle;

namespace Artifact {

W_OBJECT_IMPL(DockGlowStyle)

#if defined(ARTIFACT_QADS_COMPAT)

class DockGlowStyle::Impl {
public:
    bool glowEnabled_ = true;
    QColor glowColor_ = QColor(86, 156, 214);
    int glowWidth_ = 1;
    float glowIntensity_ = 0.58f;
};

DockGlowStyle::DockGlowStyle(QStyle* baseStyle)
    : QProxyStyle(baseStyle), impl_(new Impl()) {
}

DockGlowStyle::~DockGlowStyle() {
    delete impl_;
}

void DockGlowStyle::setGlowEnabled(bool enabled) {
    impl_->glowEnabled_ = enabled;
}

void DockGlowStyle::setGlowColor(const QColor& color) {
    impl_->glowColor_ = color;
}

void DockGlowStyle::setGlowWidth(int width) {
    impl_->glowWidth_ = qBound(1, width, 20);
}

void DockGlowStyle::setGlowIntensity(float intensity) {
    impl_->glowIntensity_ = qBound(0.0f, intensity, 1.0f);
}

void DockGlowStyle::drawControl(ControlElement element, const QStyleOption* option,
                                 QPainter* painter, const QWidget* widget) const {
    const bool focusedDockSurface = widget &&
        (qobject_cast<const ads::CDockWidget*>(widget) ||
         qobject_cast<const ads::CDockAreaWidget*>(widget));
    const bool adsFrame = widget &&
        (focusedDockSurface ||
         qobject_cast<const ads::CDockContainerWidget*>(widget) ||
         qobject_cast<const ads::CDockWidgetTab*>(widget));
    const bool dockTab = widget &&
        qobject_cast<const ads::CDockWidgetTab*>(widget);

    // QFrame paints its visible border through CE_ShapedFrame rather than the
    // primitive frame path on some platform styles.  Suppress that path too so
    // the old white QADS frame cannot remain behind the focus treatment.
    // Some QADS versions route the selected-tab outline through the tab-bar
    // controls instead of QFrame.  Suppress both paths: the selected-tab
    // underline and dock focus outline are drawn by this style below.
    const bool tabFrameControl = dockTab &&
        (element == CE_TabBarTab || element == CE_TabBarTabShape);
    if (!(adsFrame && element == CE_ShapedFrame) && !tabFrameControl) {
        QProxyStyle::drawControl(element, option, painter, widget);
    }
    if (impl_->glowEnabled_ && focusedDockSurface && element == CE_ShapedFrame &&
        isDockWidgetActive(widget)) {
        drawDockWidgetGlow(option, painter, widget);
    }
}

void DockGlowStyle::drawPrimitive(PrimitiveElement element, const QStyleOption* option,
                                   QPainter* painter, const QWidget* widget) const {
    const bool focusedDockSurface = widget &&
        (qobject_cast<const ads::CDockWidget*>(widget) ||
         qobject_cast<const ads::CDockAreaWidget*>(widget));
    const bool adsFrame = widget &&
        (focusedDockSurface ||
         qobject_cast<const ads::CDockContainerWidget*>(widget) ||
         qobject_cast<const ads::CDockWidgetTab*>(widget));
    const bool dockTab = widget &&
        qobject_cast<const ads::CDockWidgetTab*>(widget);
    const bool dockFrame = adsFrame &&
        (element == PE_FrameDockWidget || element == PE_Frame);
    const bool tabFrame = dockTab &&
        (element == PE_PanelButtonTool || element == PE_PanelButtonCommand);

    // Remove QADS/base-style dock frames for both focused and unfocused docks.
    // Focus is represented exclusively by the subtle full-surface blue frame.
    if (!dockFrame && !tabFrame) {
        QProxyStyle::drawPrimitive(element, option, painter, widget);
    }

    // The current-tab underline is a selection indicator, not a dock-focus
    // glow. Keep it available while the outer dock glow is disabled.
    if (element == PE_Widget && qobject_cast<const ads::CDockWidgetTab*>(widget) &&
        isDockTabActive(widget)) {
        drawDockTabGlow(option, painter, widget);
    }

    if (impl_->glowEnabled_ &&
        ((dockFrame && focusedDockSurface) ||
         (element == PE_Widget && focusedDockSurface)) &&
        isDockWidgetActive(widget)) {
        drawDockWidgetGlow(option, painter, widget);
    }
}

void DockGlowStyle::drawComplexControl(ComplexControl control, const QStyleOptionComplex* option,
                                        QPainter* painter, const QWidget* widget) const {
    QProxyStyle::drawComplexControl(control, option, painter, widget);
}

void DockGlowStyle::drawDockWidgetGlow(const QStyleOption* option, QPainter* painter,
                                        const QWidget* widget) const {
    if (!painter || !option) return;

    painter->save();

    QRect rect = option->rect;
    int w = impl_->glowWidth_;

    QColor glowColor = impl_->glowColor_;
    glowColor.setAlphaF(impl_->glowIntensity_);

    // Focus belongs to the entire dock surface, rather than only its tab.
    // Keep this deliberately narrow so it remains a context cue, not chrome.
    painter->fillRect(QRect(rect.left(), rect.top(), rect.width(), w), glowColor);
    painter->fillRect(QRect(rect.left(), rect.top() + w, w, rect.height() - w), glowColor);
    painter->fillRect(QRect(rect.right() - w + 1, rect.top() + w, w, rect.height() - w), glowColor);
    painter->fillRect(QRect(rect.left(), rect.bottom() - w + 1, rect.width(), w), glowColor);

    painter->restore();
}

void DockGlowStyle::drawDockTabGlow(const QStyleOption* option, QPainter* painter,
                                     const QWidget* widget) const {
    if (!painter || !option) return;

    const auto labels = widget->findChildren<QLabel*>();
    const QLabel* titleLabel = nullptr;
    for (const auto* label : labels) {
        if (label && label->isVisible() && !label->text().isEmpty()) {
            titleLabel = label;
            break;
        }
    }
    if (!titleLabel) return;

    const QRect titleRect(
        titleLabel->mapTo(widget, QPoint(0, 0)), titleLabel->size());
    const int underlineHeight = 2;
    const int underlineY = qMin(titleRect.bottom() + 1,
                                option->rect.bottom() - underlineHeight + 1);
    const QRect underline(titleRect.left(), underlineY,
                          titleRect.width(), underlineHeight);

    QColor underlineColor = impl_->glowColor_;
    underlineColor.setAlphaF(0.88f);

    painter->save();
    painter->fillRect(underline, underlineColor);
    painter->restore();
}

bool DockGlowStyle::isDockWidgetActive(const QWidget* widget) const {
    if (!widget) return false;

    if (const auto* dockWidget = qobject_cast<const ads::CDockWidget*>(widget)) {
        return dockWidget->property("artifactActiveDock").toBool();
    }
    if (const auto* dockArea = qobject_cast<const ads::CDockAreaWidget*>(widget)) {
        const auto* currentDock = dockArea->currentDockWidget();
        return currentDock && currentDock->property("artifactActiveDock").toBool();
    }
    return false;
}

bool DockGlowStyle::isDockTabActive(const QWidget* widget) const {
    if (!widget) return false;

    auto tab = qobject_cast<const ads::CDockWidgetTab*>(widget);
    if (!tab) return false;

    return tab->property("artifactCurrentTab").toBool();
}

#else

class DockGlowStyle::Impl {
public:
    bool glowEnabled_ = false;
    QColor glowColor_ = QColor(86, 156, 214);
    int glowWidth_ = 1;
    float glowIntensity_ = 0.58f;
};

DockGlowStyle::DockGlowStyle(QStyle* baseStyle)
    : QProxyStyle(baseStyle), impl_(new Impl()) {}

DockGlowStyle::~DockGlowStyle() {
    delete impl_;
}

void DockGlowStyle::setGlowEnabled(bool enabled) {
    if (impl_) impl_->glowEnabled_ = enabled;
}

void DockGlowStyle::setGlowColor(const QColor& color) {
    if (impl_) impl_->glowColor_ = color;
}

void DockGlowStyle::setGlowWidth(int width) {
    if (impl_) impl_->glowWidth_ = width;
}

void DockGlowStyle::setGlowIntensity(float intensity) {
    if (impl_) impl_->glowIntensity_ = intensity;
}

void DockGlowStyle::drawControl(ControlElement element,
                                const QStyleOption* option,
                                QPainter* painter,
                                const QWidget* widget) const {
    QProxyStyle::drawControl(element, option, painter, widget);
}

void DockGlowStyle::drawPrimitive(PrimitiveElement element,
                                  const QStyleOption* option,
                                  QPainter* painter,
                                  const QWidget* widget) const {
    QProxyStyle::drawPrimitive(element, option, painter, widget);
}

void DockGlowStyle::drawComplexControl(ComplexControl control,
                                       const QStyleOptionComplex* option,
                                       QPainter* painter,
                                       const QWidget* widget) const {
    QProxyStyle::drawComplexControl(control, option, painter, widget);
}

void DockGlowStyle::drawDockWidgetGlow(const QStyleOption*, QPainter*,
                                       const QWidget*) const {}
void DockGlowStyle::drawDockTabGlow(const QStyleOption*, QPainter*,
                                    const QWidget*) const {}
bool DockGlowStyle::isDockWidgetActive(const QWidget*) const { return false; }
bool DockGlowStyle::isDockTabActive(const QWidget*) const { return false; }

#endif

}
