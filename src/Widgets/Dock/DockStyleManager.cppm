module;
#include <QObject>
#include <QApplication>
#include <QColor>
#include <QEvent>
#include <QGraphicsDropShadowEffect>
#include <QPointer>
#include <QStyle>
#include <QTimer>
#include <QWidget>
#include <algorithm>
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
    bool glowEnabled_ = true;
    QColor glowColor_ = QColor(86, 156, 214);
    int glowWidth_ = 3;
    float glowIntensity_ = 0.82f;
    bool refreshScheduled_ = false;
};

namespace {

void repolishWidget(QWidget* widget) {
    if (!widget) return;
    widget->setAttribute(Qt::WA_StyledBackground, true);
    if (auto* style = widget->style()) {
        style->unpolish(widget);
        style->polish(widget);
    }
    widget->update();
}

QGraphicsDropShadowEffect* ensureTabGlow(QWidget* tab) {
    if (!tab) return nullptr;
    auto* effect = qobject_cast<QGraphicsDropShadowEffect*>(tab->graphicsEffect());
    if (!effect) {
        effect = new QGraphicsDropShadowEffect(tab);
        tab->setGraphicsEffect(effect);
    }
    return effect;
}

QGraphicsDropShadowEffect* ensureDockGlow(QWidget* dock) {
    if (!dock) return nullptr;
    auto* effect = qobject_cast<QGraphicsDropShadowEffect*>(dock->graphicsEffect());
    if (!effect) {
        effect = new QGraphicsDropShadowEffect(dock);
        dock->setGraphicsEffect(effect);
    }
    return effect;
}

void clearTabGlow(QWidget* tab) {
    if (!tab) return;
    if (tab->graphicsEffect()) {
        tab->setGraphicsEffect(nullptr);
    }
}

void clearDockGlow(QWidget* dock) {
    if (!dock) return;
    if (dock->graphicsEffect()) {
        dock->setGraphicsEffect(nullptr);
    }
}

bool isDockRelatedObject(QObject* watched, ads::CDockManager* dockManager) {
    if (!watched || !dockManager) return false;
    if (watched == dockManager) return true;
    if (qobject_cast<ads::CDockWidget*>(watched) || qobject_cast<ads::CDockWidgetTab*>(watched)) {
        return true;
    }

    auto* widget = qobject_cast<QWidget*>(watched);
    return widget && dockManager->isAncestorOf(widget);
}

ads::CDockWidget* dockFromObject(QObject* object) {
    QObject* cursor = object;
    while (cursor) {
        if (auto* tab = qobject_cast<ads::CDockWidgetTab*>(cursor)) {
            return tab->dockWidget();
        }
        if (auto* dock = qobject_cast<ads::CDockWidget*>(cursor)) {
            return dock;
        }
        cursor = cursor->parent();
    }
    return nullptr;
}

ads::CDockWidget* resolveActiveDock(ads::CDockManager* dockManager, ads::CDockWidget* rememberedDock) {
    if (!dockManager) return nullptr;

    if (rememberedDock && rememberedDock->isVisible()) {
        return rememberedDock;
    }

    if (auto* focusedDock = dockManager->focusedDockWidget()) {
        if (focusedDock->isVisible()) {
            return focusedDock;
        }
    }

    if (auto* focusedWidget = QApplication::focusWidget()) {
        QObject* cursor = focusedWidget;
        while (cursor) {
            if (auto* dock = qobject_cast<ads::CDockWidget*>(cursor)) {
                if (dock->isVisible()) {
                    return dock;
                }
            }
            cursor = cursor->parent();
        }
    }

    const auto docks = dockManager->findChildren<ads::CDockWidget*>();
    for (auto* dock : docks) {
        if (dock && dock->isVisible() && dock->isCurrentTab()) {
            return dock;
        }
    }

    return nullptr;
}

}

DockStyleManager::DockStyleManager(ads::CDockManager* dockManager, QObject* parent)
    : QObject(parent), impl_(new Impl()) {
    impl_->dockManager_ = dockManager;

    impl_->glowStyle_ = new DockGlowStyle(QApplication::style());
    impl_->dockManager_->setStyle(impl_->glowStyle_);

    qApp->installEventFilter(this);

    connect(dockManager, &ads::CDockManager::focusedDockWidgetChanged,
            this, [this](ads::CDockWidget*, ads::CDockWidget* now) {
        impl_->focusedDockWidget_ = now;
        scheduleRefresh();
    });

    connect(qApp, &QApplication::focusChanged,
            this, [this](QWidget*, QWidget*) {
        scheduleRefresh();
    });

    impl_->focusedDockWidget_ = dockManager->focusedDockWidget();
    scheduleRefresh();
}

DockStyleManager::~DockStyleManager() {
    if (qApp) {
        qApp->removeEventFilter(this);
    }
    delete impl_;
}

void DockStyleManager::setGlowEnabled(bool enabled) {
    if (impl_->glowStyle_) {
        impl_->glowEnabled_ = enabled;
        impl_->glowStyle_->setGlowEnabled(enabled);
        scheduleRefresh();
    }
}

void DockStyleManager::setGlowColor(const QColor& color) {
    if (impl_->glowStyle_) {
        impl_->glowColor_ = color;
        impl_->glowStyle_->setGlowColor(color);
        scheduleRefresh();
    }
}

void DockStyleManager::setGlowWidth(int width) {
    if (impl_->glowStyle_) {
        impl_->glowWidth_ = width;
        impl_->glowStyle_->setGlowWidth(width);
        scheduleRefresh();
    }
}

void DockStyleManager::setGlowIntensity(float intensity) {
    if (impl_->glowStyle_) {
        impl_->glowIntensity_ = intensity;
        impl_->glowStyle_->setGlowIntensity(intensity);
        scheduleRefresh();
    }
}

void DockStyleManager::applyStyle() {
    if (impl_->dockManager_) {
        scheduleRefresh();
    }
}

bool DockStyleManager::eventFilter(QObject* watched, QEvent* event) {
    if (!impl_ || !impl_->dockManager_ || !event) {
        return QObject::eventFilter(watched, event);
    }

    if (isDockRelatedObject(watched, impl_->dockManager_)) {
        bool refreshImmediately = false;
        if ((event->type() == QEvent::MouseButtonPress || event->type() == QEvent::MouseButtonRelease) &&
            watched) {
            if (auto* dock = dockFromObject(watched)) {
                impl_->focusedDockWidget_ = dock;
                refreshImmediately = (event->type() == QEvent::MouseButtonPress);
            }
        }

        if (refreshImmediately) {
            impl_->refreshScheduled_ = false;
            refreshDockDecorations();
            return QObject::eventFilter(watched, event);
        }

        switch (event->type()) {
        case QEvent::ChildAdded:
        case QEvent::ChildRemoved:
        case QEvent::FocusIn:
        case QEvent::FocusOut:
        case QEvent::Hide:
        case QEvent::LayoutRequest:
        case QEvent::MouseButtonPress:
        case QEvent::MouseButtonRelease:
        case QEvent::Polish:
        case QEvent::Show:
        case QEvent::WindowActivate:
        case QEvent::WindowDeactivate:
        case QEvent::ZOrderChange:
            scheduleRefresh();
            break;
        default:
            break;
        }
    }

    return QObject::eventFilter(watched, event);
}

void DockStyleManager::scheduleRefresh() {
    if (!impl_ || !impl_->dockManager_ || impl_->refreshScheduled_) {
        return;
    }

    impl_->refreshScheduled_ = true;
    QTimer::singleShot(0, this, [this]() {
        if (!impl_) return;
        impl_->refreshScheduled_ = false;
        refreshDockDecorations();
    });
}

void DockStyleManager::refreshDockDecorations() {
    if (!impl_ || !impl_->dockManager_) {
        return;
    }

    auto* activeDock = resolveActiveDock(impl_->dockManager_, impl_->focusedDockWidget_);
    impl_->focusedDockWidget_ = activeDock;

    const auto docks = impl_->dockManager_->findChildren<ads::CDockWidget*>();
    for (auto* dock : docks) {
        if (!dock) continue;

        const bool isActive = (dock == activeDock);
        dock->setProperty("artifactActiveDock", isActive);
        clearDockGlow(dock);
        repolishWidget(dock);

        auto* tab = dock->tabWidget();
        if (!tab) continue;

        tab->setProperty("artifactActiveTab", isActive);
        if (isActive && impl_->glowEnabled_) {
            auto* effect = ensureTabGlow(tab);
            effect->setBlurRadius(18.0 + static_cast<qreal>(impl_->glowWidth_) * 2.0);
            effect->setOffset(0.0, 0.0);
            QColor glowColor = impl_->glowColor_;
            glowColor.setAlphaF(std::clamp(impl_->glowIntensity_, 0.0f, 1.0f));
            effect->setColor(glowColor);
        } else {
            clearTabGlow(tab);
        }
        repolishWidget(tab);
    }
    impl_->dockManager_->update();
}

}
