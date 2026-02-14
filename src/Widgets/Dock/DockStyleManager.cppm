module;
#include <QObject>
#include <QApplication>
#include <wobjectimpl.h>
#include "DockManager.h"

module Widgets.Dock.StyleManager;

import Widgets.Dock.GlowStyle;

namespace Artifact {

W_OBJECT_IMPL(DockStyleManager)

class DockStyleManager::Impl {
public:
    ads::CDockManager* dockManager_ = nullptr;
    DockGlowStyle* glowStyle_ = nullptr;
};

DockStyleManager::DockStyleManager(ads::CDockManager* dockManager, QObject* parent)
    : QObject(parent), impl_(new Impl()) {
    impl_->dockManager_ = dockManager;
    
    // カスタムスタイルを作成
    impl_->glowStyle_ = new DockGlowStyle(QApplication::style());
    
    // DockManagerに適用
    impl_->dockManager_->setStyle(impl_->glowStyle_);
    
    // フォーカス変更時に再描画を促す
    connect(dockManager, &ads::CDockManager::focusedDockWidgetChanged,
            [this](ads::CDockWidget* old, ads::CDockWidget* now) {
        if (old) old->update();
        if (now) now->update();
    });
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
