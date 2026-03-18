module;
#include <QObject>
#include <QApplication>
#include <QColor>
#include <QEvent>
#include <QLabel>
#include <QPointer>
#include <QStyle>
#include <QTimer>
#include <QWidget>
#include <wobjectimpl.h>
#include "DockManager.h"
#include "DockWidget.h"
#include "DockWidgetTab.h"
#include "FloatingDockContainer.h"

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

bool isDockRelatedObject(QObject* watched, ads::CDockManager* dockManager) {
    if (!watched || !dockManager) return false;
    if (watched == dockManager) return true;
    if (qobject_cast<ads::CDockWidget*>(watched) || qobject_cast<ads::CDockWidgetTab*>(watched)) {
        return true;
    }

    auto* widget = qobject_cast<QWidget*>(watched);
    if (!widget) return false;
    if (dockManager->isAncestorOf(widget)) return true;

    // Floating dock containers are separate top-level windows that are
    // NOT children of CDockManager, so isAncestorOf() misses them.
    for (auto* w = widget->parentWidget(); w; w = w->parentWidget()) {
        if (qobject_cast<ads::CFloatingDockContainer*>(w)) {
            return true;
        }
    }
    return false;
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

    // rememberedDock はユーザーのクリックまたは focusedDockWidgetChanged シグナルで
    // 設定される。タブクリック直後はまだ isVisible() == false の場合があるため、
    // ここでは可視性チェックを行わない。
    if (rememberedDock) {
        return rememberedDock;
    }

    // フォールバック: QAds が内部的に追跡しているフォーカスドックを使用
    return dockManager->focusedDockWidget();
}

QString tabTextColor(const bool isActiveDock, const bool /*isFloatingTab*/, const bool /*isCurrentTab*/)
{
    if (isActiveDock) {
        return QStringLiteral("#ffffff");
    }
    return QStringLiteral("#BBBBBB");
}

void applyTabLabelColors(ads::CDockWidgetTab* tab, const QString& color, const bool emphasize)
{
    if (!tab) {
        return;
    }

    const QString labelStyle = QStringLiteral("color: %1; background: transparent; font-weight: %2;")
        .arg(color, emphasize ? QStringLiteral("600") : QStringLiteral("500"));
    for (auto* label : tab->findChildren<QLabel*>()) {
        if (label) {
            label->setStyleSheet(labelStyle);
        }
    }
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

    // 高速パス: ドック装飾に無関係なイベント型は isDockRelatedObject の
    // 高コストな isAncestorOf 呼び出しを行わず即座にスキップする。
    // ChildAdded / ChildRemoved / LayoutRequest / Polish はレイアウト処理中に
    // 大量発生するため、アクティブドック状態の更新トリガーから除外する。
    switch (event->type()) {
    case QEvent::FocusIn:
    case QEvent::FocusOut:
    case QEvent::Hide:
    case QEvent::MouseButtonPress:
    case QEvent::MouseButtonRelease:
    case QEvent::Show:
    case QEvent::WindowActivate:
    case QEvent::WindowDeactivate:
    case QEvent::ZOrderChange:
        break;
    default:
        return QObject::eventFilter(watched, event);
    }

    if (isDockRelatedObject(watched, impl_->dockManager_)) {
        if ((event->type() == QEvent::MouseButtonPress || event->type() == QEvent::MouseButtonRelease) &&
            watched) {
            if (auto* dock = dockFromObject(watched)) {
                impl_->focusedDockWidget_ = dock;
            }
        }

        // 常に遅延リフレッシュ。即時リフレッシュは QAds のタブ切替処理前に
        // 実行されるため、古い状態を参照してしまう問題があった。
        scheduleRefresh();
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

    const auto docks = impl_->dockManager_->dockWidgetsMap().values();
    bool anyChanged = false;
    for (auto* dock : docks) {
        if (!dock) continue;

        const bool isActiveDock = (dock == activeDock);
        const bool isFloating = dock->isInFloatingContainer();

        const bool dockActivePrev = dock->property("artifactActiveDock").toBool();
        const bool dockFloatPrev  = dock->property("artifactFloatingDock").toBool();
        const bool dockChanged = (dockActivePrev != isActiveDock) || (dockFloatPrev != isFloating);

        if (dockChanged) {
            dock->setProperty("artifactActiveDock", isActiveDock);
            dock->setProperty("artifactFloatingDock", isFloating);
            repolishWidget(dock);
            anyChanged = true;
        }

        auto* tab = dock->tabWidget();
        if (!tab) continue;

        const bool isCurrentTab = tab->isActiveTab();
        const bool isActiveTab = isActiveDock && isCurrentTab;

        const bool tabActivePrev  = tab->property("artifactActiveTab").toBool();
        const bool tabFloatPrev   = tab->property("artifactFloatingTab").toBool();
        const bool tabCurrentPrev = tab->property("artifactCurrentTab").toBool();
        const bool tabChanged = (tabActivePrev != isActiveTab) || (tabFloatPrev != isFloating)
                                || (tabCurrentPrev != isCurrentTab);

        if (tabChanged) {
            tab->setProperty("artifactActiveTab", isActiveTab);
            tab->setProperty("artifactFloatingTab", isFloating);
            tab->setProperty("artifactCurrentTab", isCurrentTab);
            applyTabLabelColors(tab, tabTextColor(isActiveTab, isFloating, isCurrentTab), isActiveTab);
            repolishWidget(tab);
            anyChanged = true;
        }
    }
    if (anyChanged) {
        impl_->dockManager_->update();
    }
}

}
