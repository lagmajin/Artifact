module;
#include <QProxyStyle>
#include <QPainter>
#include <QStyleOption>
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

    QRect rect = option->rect;
    int w = impl_->glowWidth_;

    QColor glowColor = impl_->glowColor_;
    glowColor.setAlphaF(impl_->glowIntensity_);

    // グラデーションの代わりに、フラットなアクセント枠だけにする。
    painter->fillRect(QRect(rect.left(), rect.top() + w, w, rect.height() - w), glowColor);
    painter->fillRect(QRect(rect.right() - w + 1, rect.top() + w, w, rect.height() - w), glowColor);
    painter->fillRect(QRect(rect.left(), rect.bottom() - w + 1, rect.width(), w), glowColor);

    painter->restore();
}

void DockGlowStyle::drawDockTabGlow(const QStyleOption* option, QPainter* painter,
                                     const QWidget* widget) const {
    if (!painter || !option) return;

    painter->save();

    QRect rect = option->rect;
    int w = impl_->glowWidth_;

    QColor glowColor = impl_->glowColor_;
    glowColor.setAlphaF(impl_->glowIntensity_ * 0.9f);

    // タブ上部だけを少し強調し、それ以外はフラットな枠線にする。
    painter->fillRect(QRect(rect.left(), rect.top(), rect.width(), w), glowColor);
    painter->fillRect(QRect(rect.left(), rect.top(), w, rect.height()), glowColor);
    painter->fillRect(QRect(rect.right() - w + 1, rect.top(), w, rect.height()), glowColor);
    painter->fillRect(QRect(rect.left(), rect.bottom() - w + 1, rect.width(), w), glowColor);

    painter->restore();
}

bool DockGlowStyle::isDockWidgetActive(const QWidget* widget) const {
    if (!widget) return false;
    
    auto dockWidget = qobject_cast<const ads::CDockWidget*>(widget);
    if (!dockWidget) return false;
    
    // artifactActiveDock プロパティ（DockStyleManager が唯一の真のアクティブドックにのみ設定）で判定
    return dockWidget->property("artifactActiveDock").toBool();
}

bool DockGlowStyle::isDockTabActive(const QWidget* widget) const {
    if (!widget) return false;

    auto tab = qobject_cast<const ads::CDockWidgetTab*>(widget);
    if (!tab) return false;

    // artifactActiveTab プロパティ（真にアクティブな1つのタブのみ true）で判定
    return tab->property("artifactActiveTab").toBool();
}

}
