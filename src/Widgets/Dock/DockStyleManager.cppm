module;
#include <QObject>
#include <QApplication>
#include <QColor>
#include <QGraphicsDropShadowEffect>
#include <QPointer>
#include <wobjectimpl.h>
#include "DockManager.h"
#include "DockWidget.h"
#include "DockWidgetTab.h"

module Widgets.Dock.StyleManager;

import Widgets.Dock.GlowStyle;

namespace Artifact {

W_OBJECT_IMPL(DockStyleManager)

class DockStyleManager::Impl {
public:
    ads::CDockManager* dockManager_ = nullptr;
    DockGlowStyle* glowStyle_ = nullptr;
    QPointer<ads::CDockWidget> focusedDockWidget_;
};

DockStyleManager::DockStyleManager(ads::CDockManager* dockManager, QObject* parent)
    : QObject(parent), impl_(new Impl()) {
    impl_->dockManager_ = dockManager;
    
    // カスタムスタイルを作成
    impl_->glowStyle_ = new DockGlowStyle(QApplication::style());
    
    // DockManagerに適用
    impl_->dockManager_->setStyle(impl_->glowStyle_);
    
    auto applyTabGlow = [](ads::CDockWidget* dockWidget) {
        if (!dockWidget) return;
        auto* tab = dockWidget->tabWidget();
        if (!tab) return;

        tab->setProperty("artifactActiveTab", true);
        auto* fx = new QGraphicsDropShadowEffect(tab);
        fx->setBlurRadius(18.0);
        fx->setOffset(0.0, 0.0);
        fx->setColor(QColor(86, 156, 214, 210));
        tab->setGraphicsEffect(fx);
        tab->update();
    };

    auto clearTabGlow = [](ads::CDockWidget* dockWidget) {
        if (!dockWidget) return;
        auto* tab = dockWidget->tabWidget();
        if (!tab) return;

        tab->setProperty("artifactActiveTab", false);
        tab->setGraphicsEffect(nullptr);
        tab->update();
    };

    // フォーカス変更時に再描画とタブ発光更新
    connect(dockManager, &ads::CDockManager::focusedDockWidgetChanged,
            [this, applyTabGlow, clearTabGlow](ads::CDockWidget* old, ads::CDockWidget* now) {
        if (old) {
            clearTabGlow(old);
            old->setProperty("artifactActiveDock", false);
            old->update();
        }
        if (now) {
            applyTabGlow(now);
            now->setProperty("artifactActiveDock", true);
            now->update();
        }
        impl_->focusedDockWidget_ = now;
    });

    if (auto* focused = dockManager->focusedDockWidget()) {
        applyTabGlow(focused);
        focused->setProperty("artifactActiveDock", true);
        impl_->focusedDockWidget_ = focused;
    }
}

DockStyleManager::~DockStyleManager() {
    delete impl_;
}

void DockStyleManager::setGlowEnabled(bool enabled) {
    if (impl_->glowStyle_) {
        impl_->glowStyle_->setGlowEnabled(enabled);
        impl_->dockManager_->update();
    }
}

void DockStyleManager::setGlowColor(const QColor& color) {
    if (impl_->glowStyle_) {
        impl_->glowStyle_->setGlowColor(color);
        impl_->dockManager_->update();
    }
}

void DockStyleManager::setGlowWidth(int width) {
    if (impl_->glowStyle_) {
        impl_->glowStyle_->setGlowWidth(width);
        impl_->dockManager_->update();
    }
}

void DockStyleManager::setGlowIntensity(float intensity) {
    if (impl_->glowStyle_) {
        impl_->glowStyle_->setGlowIntensity(intensity);
        impl_->dockManager_->update();
    }
}

void DockStyleManager::applyStyle() {
    if (impl_->dockManager_) {
        impl_->dockManager_->update();
    }
}

}
