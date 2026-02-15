module;
#include <QProxyStyle>
#include <QPainter>
#include <QStyleOption>
#include <QLinearGradient>
#include <QRadialGradient>
#include <wobjectimpl.h>
#include "DockWidget.h"
#include "DockAreaWidget.h"
#include "DockWidgetTab.h"

module Widgets.Dock.GlowStyle;

namespace Artifact {

W_OBJECT_IMPL(DockGlowStyle)

class DockGlowStyle::Impl {
public:
    bool glowEnabled_ = true;
    QColor glowColor_ = QColor(41, 121, 255);
    int glowWidth_ = 4;
    float glowIntensity_ = 0.8f;
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
    // 標準描画を先に実行
    QProxyStyle::drawControl(element, option, painter, widget);
    
    // CDockWidget関連の描画ならグローを追加
    if (!impl_->glowEnabled_ || !widget) return;
    
    // DockWidgetの判定
    if (qobject_cast<const ads::CDockWidget*>(widget)) {
        if (isDockWidgetActive(widget)) {
            drawDockWidgetGlow(option, painter, widget);
        }
    }
}

void DockGlowStyle::drawPrimitive(PrimitiveElement element, const QStyleOption* option,
                                   QPainter* painter, const QWidget* widget) const {
    // 標準描画を先に実行
    QProxyStyle::drawPrimitive(element, option, painter, widget);
    
    if (!impl_->glowEnabled_ || !widget) return;
    
    // フレーム描画時にグローを追加
    if (element == PE_FrameDockWidget) {
        if (isDockWidgetActive(widget)) {
            drawDockWidgetGlow(option, painter, widget);
        }
    }
}

void DockGlowStyle::drawComplexControl(ComplexControl control, const QStyleOptionComplex* option,
                                        QPainter* painter, const QWidget* widget) const {
    // 標準描画を先に実行
    QProxyStyle::drawComplexControl(control, option, painter, widget);
    
    if (!impl_->glowEnabled_ || !widget) return;
    
    // タブバー関連の描画
    if (qobject_cast<const ads::CDockWidgetTab*>(widget)) {
        if (isDockTabActive(widget)) {
            drawDockTabGlow(option, painter, widget);
        }
    }
}

void DockGlowStyle::drawDockWidgetGlow(const QStyleOption* option, QPainter* painter,
                                        const QWidget* widget) const {
    if (!painter || !option) return;
    
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);
    
    QRect rect = option->rect;
    int w = impl_->glowWidth_;
    
    QColor glowColor = impl_->glowColor_;
    glowColor.setAlphaF(impl_->glowIntensity_);
    QColor transparentColor = impl_->glowColor_;
    transparentColor.setAlphaF(0);
    
    // ウィジェット本体は上側（タブ接触部分）を除いてグロー
    
    // 左辺のグロー
    QLinearGradient leftGradient(rect.left(), 0, rect.left() + w, 0);
    leftGradient.setColorAt(0, glowColor);
    leftGradient.setColorAt(1, transparentColor);
    painter->fillRect(rect.left(), rect.top() + w, w, rect.height() - w, leftGradient);
    
    // 右辺のグロー
    QLinearGradient rightGradient(rect.right() - w, 0, rect.right(), 0);
    rightGradient.setColorAt(0, transparentColor);
    rightGradient.setColorAt(1, glowColor);
    painter->fillRect(rect.right() - w, rect.top() + w, w, rect.height() - w, rightGradient);
    
    // 下辺のグロー
    QLinearGradient bottomGradient(0, rect.bottom() - w, 0, rect.bottom());
    bottomGradient.setColorAt(0, transparentColor);
    bottomGradient.setColorAt(1, glowColor);
    painter->fillRect(rect.left(), rect.bottom() - w, rect.width(), w, bottomGradient);
    
    // 左下コーナー
    QRadialGradient blCorner(rect.left() + w, rect.bottom() - w, w);
    blCorner.setColorAt(0, glowColor);
    blCorner.setColorAt(1, transparentColor);
    painter->fillRect(rect.left(), rect.bottom() - w, w, w, blCorner);
    
    // 右下コーナー
    QRadialGradient brCorner(rect.right() - w, rect.bottom() - w, w);
    brCorner.setColorAt(0, glowColor);
    brCorner.setColorAt(1, transparentColor);
    painter->fillRect(rect.right() - w, rect.bottom() - w, w, w, brCorner);
    
    painter->restore();
}

void DockGlowStyle::drawDockTabGlow(const QStyleOption* option, QPainter* painter,
                                     const QWidget* widget) const {
    if (!painter || !option) return;
    
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);
    
    QRect rect = option->rect;
    int w = impl_->glowWidth_;
    
    QColor glowColor = impl_->glowColor_;
    glowColor.setAlphaF(impl_->glowIntensity_ * 0.9f);
    QColor transparentColor = impl_->glowColor_;
    transparentColor.setAlphaF(0);
    
    // タブは下側（ウィジェット接触部分）を除いてグロー
    
    // 上辺の強いグロー
    QLinearGradient topGradient(0, rect.top(), 0, rect.top() + w);
    topGradient.setColorAt(0, glowColor);
    topGradient.setColorAt(1, transparentColor);
    painter->fillRect(rect.left(), rect.top(), rect.width(), w, topGradient);
    
    // 左辺のグロー
    QLinearGradient leftGradient(rect.left(), 0, rect.left() + w, 0);
    leftGradient.setColorAt(0, glowColor);
    leftGradient.setColorAt(1, transparentColor);
    painter->fillRect(rect.left(), rect.top(), w, rect.height() - w, leftGradient);
    
    // 右辺のグロー
    QLinearGradient rightGradient(rect.right() - w, 0, rect.right(), 0);
    rightGradient.setColorAt(0, transparentColor);
    rightGradient.setColorAt(1, glowColor);
    painter->fillRect(rect.right() - w, rect.top(), w, rect.height() - w, rightGradient);
    
    // 左上コーナー
    QRadialGradient tlCorner(rect.left() + w, rect.top() + w, w);
    tlCorner.setColorAt(0, glowColor);
    tlCorner.setColorAt(1, transparentColor);
    painter->fillRect(rect.left(), rect.top(), w, w, tlCorner);
    
    // 右上コーナー
    QRadialGradient trCorner(rect.right() - w, rect.top() + w, w);
    trCorner.setColorAt(0, glowColor);
    trCorner.setColorAt(1, transparentColor);
    painter->fillRect(rect.right() - w, rect.top(), w, w, trCorner);
    
    painter->restore();
}

bool DockGlowStyle::isDockWidgetActive(const QWidget* widget) const {
    if (!widget) return false;
    
    auto dockWidget = qobject_cast<const ads::CDockWidget*>(widget);
    if (!dockWidget) return false;
    
    // Use ADS API to determine whether the dock widget is the current tab
    // in its dock area.
    return dockWidget->isCurrentTab();
}

bool DockGlowStyle::isDockTabActive(const QWidget* widget) const {
    if (!widget) return false;
    
    auto tab = qobject_cast<const ads::CDockWidgetTab*>(widget);
    if (!tab) return false;
    
    return tab->isActiveTab();
}

}
