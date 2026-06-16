module;
#include <utility>
#include <wobjectimpl.h>
#include <QHBoxLayout>
#include <QIcon>
#include <QPushButton>
#include <QString>
#include <QSize>
#include <QSignalBlocker>
#include <QVariant>
#include <QStyle>
#include <QToolTip>
#include <QWidget>

module Artifact.Widgets.Timeline.GlobalSwitches;

import Utils.Path;
import Artifact.Event.Types;
import Event.Bus;
import Application.AppSettings;

namespace Artifact {
namespace {
QIcon loadIconWithFallback(const QString& resourceRelativePath, const QString& fallbackFileName = {})
{
    QIcon icon(ArtifactCore::resolveIconResourcePath(resourceRelativePath));
    if (!icon.isNull()) {
        return icon;
    }
    if (!fallbackFileName.isEmpty()) {
        return QIcon(ArtifactCore::resolveIconPath(fallbackFileName));
    }
    return QIcon();
}
} // namespace

W_OBJECT_IMPL(ArtifactTimelineGlobalSwitches)

class ArtifactTimelineGlobalSwitches::Impl {
public:
    QPushButton* shyBtn = nullptr;
    QPushButton* motionBlurBtn = nullptr;
    QPushButton* frameBlendBtn = nullptr;
    QPushButton* graphEditorBtn = nullptr;
    QPushButton* motionPathBtn = nullptr;
    QPushButton* overscrollBtn = nullptr;
    ArtifactCore::EventBus eventBus_ = ArtifactCore::globalEventBus();

    void setupUi(QWidget* parent) {
        auto layout = new QHBoxLayout(parent);
        layout->setContentsMargins(4, 0, 4, 0);
        layout->setSpacing(2);

        auto createBtn = [&](const QString& tooltip, const QIcon& icon) {
            auto btn = new QPushButton();
            btn->setCheckable(true);
            btn->setFixedSize(28, 28);
            btn->setToolTip(tooltip);
            btn->setIcon(icon);
            btn->setIconSize(QSize(19, 19));
            btn->setFlat(true);
            layout->addWidget(btn);
            return btn;
        };

        shyBtn = createBtn(
            "Hide Shy Layers",
            loadIconWithFallback(
                QStringLiteral("Studio/timeline_switch_shy.svg"),
                QStringLiteral("Studio/layermenu_shy.svg")));
        motionBlurBtn = createBtn(
            "Enable Motion Blur",
            loadIconWithFallback(
                QStringLiteral("Studio/timeline_switch_motion_blur.svg"),
                QStringLiteral("Studio/timemenu_timer.svg")));
        frameBlendBtn = createBtn(
            "Enable Frame Blending",
            loadIconWithFallback(
                QStringLiteral("Studio/timeline_switch_frame_blend.svg"),
                QStringLiteral("Studio/tune.svg")));
        graphEditorBtn = createBtn(
            "Toggle Curve Editor / カーブエディタ切り替え (Ctrl+G)",
            loadIconWithFallback(
                QStringLiteral("Studio/timeline_switch_curve_editor.svg"),
                QStringLiteral("Studio/graphic_eq.svg")));
        motionPathBtn = createBtn(
            "Show Motion Path Overlay",
            loadIconWithFallback(
                QStringLiteral("Studio/timeline_switch_motion_path.svg"),
                QStringLiteral("Studio/timeline.svg")));
        overscrollBtn = createBtn(
            "Allow Timeline Overscroll",
            loadIconWithFallback(
                QStringLiteral("Studio/timeline_switch_overscroll.svg"),
                QStringLiteral("Studio/swap_horiz.svg")));

        layout->addStretch();
    }
};

ArtifactTimelineGlobalSwitches::ArtifactTimelineGlobalSwitches(QWidget* parent)
    : QWidget(parent), impl_(new Impl()) {
    impl_->setupUi(this);

    connect(impl_->shyBtn, &QPushButton::toggled, this, [this](bool v){
        Q_EMIT shyChanged(v);
        impl_->eventBus_.post<TimelineShyChangedEvent>(TimelineShyChangedEvent{v});
        if (auto* settings = ArtifactCore::ArtifactAppSettings::instance()) {
            settings->setTimelineShyActive(v);
        }
    });
    connect(impl_->motionBlurBtn, &QPushButton::toggled, this, [this](bool v){
        Q_EMIT motionBlurChanged(v);
        impl_->eventBus_.post<TimelineMotionBlurChangedEvent>(TimelineMotionBlurChangedEvent{v});
        if (auto* settings = ArtifactCore::ArtifactAppSettings::instance()) {
            settings->setTimelineMotionBlurActive(v);
        }
    });
    connect(impl_->frameBlendBtn, &QPushButton::toggled, this, [this](bool v){
        Q_EMIT frameBlendingChanged(v);
        impl_->eventBus_.post<TimelineFrameBlendingChangedEvent>(TimelineFrameBlendingChangedEvent{v});
        if (auto* settings = ArtifactCore::ArtifactAppSettings::instance()) {
            settings->setTimelineFrameBlendingActive(v);
        }
    });
    connect(impl_->graphEditorBtn, &QPushButton::toggled, this, [this](bool v){
        Q_EMIT graphEditorToggled(v);
        impl_->eventBus_.post<TimelineGraphEditorToggledEvent>(TimelineGraphEditorToggledEvent{v});
        if (auto* settings = ArtifactCore::ArtifactAppSettings::instance()) {
            settings->setTimelineGraphEditorActive(v);
        }
    });
    connect(impl_->motionPathBtn, &QPushButton::toggled, this, [this](bool v){
        if (auto* settings = ArtifactCore::ArtifactAppSettings::instance()) {
            settings->setCompositionShowMotionPathOverlay(v);
        }
    });
    connect(impl_->overscrollBtn, &QPushButton::toggled, this, [this](bool v){
        if (auto* settings = ArtifactCore::ArtifactAppSettings::instance()) {
            settings->setTimelineAllowOverscroll(v);
        }
    });

    if (auto* settings = ArtifactCore::ArtifactAppSettings::instance()) {
        connect(settings, &ArtifactCore::ArtifactAppSettings::settingsChanged, this, [this]() {
            if (!impl_ || !impl_->overscrollBtn) {
                return;
            }
            if (auto* settings = ArtifactCore::ArtifactAppSettings::instance()) {
                const QSignalBlocker blockerShy(impl_->shyBtn);
                impl_->shyBtn->setChecked(settings->timelineShyActive());
                const QSignalBlocker blockerMotionBlur(impl_->motionBlurBtn);
                impl_->motionBlurBtn->setChecked(settings->timelineMotionBlurActive());
                const QSignalBlocker blockerFrameBlend(impl_->frameBlendBtn);
                impl_->frameBlendBtn->setChecked(settings->timelineFrameBlendingActive());
                const QSignalBlocker blockerGraph(impl_->graphEditorBtn);
                impl_->graphEditorBtn->setChecked(settings->timelineGraphEditorActive());
                const QSignalBlocker blocker1(impl_->overscrollBtn);
                impl_->overscrollBtn->setChecked(settings->timelineAllowOverscroll());
                const QSignalBlocker blocker2(impl_->motionPathBtn);
                impl_->motionPathBtn->setChecked(settings->compositionShowMotionPathOverlay());
            }
        });
        const QSignalBlocker blockerShy(impl_->shyBtn);
        const QSignalBlocker blockerMotionBlur(impl_->motionBlurBtn);
        const QSignalBlocker blockerFrameBlend(impl_->frameBlendBtn);
        const QSignalBlocker blockerGraph(impl_->graphEditorBtn);
        const QSignalBlocker blocker1(impl_->overscrollBtn);
        const QSignalBlocker blocker2(impl_->motionPathBtn);
        impl_->shyBtn->setChecked(settings->timelineShyActive());
        impl_->motionBlurBtn->setChecked(settings->timelineMotionBlurActive());
        impl_->frameBlendBtn->setChecked(settings->timelineFrameBlendingActive());
        impl_->graphEditorBtn->setChecked(settings->timelineGraphEditorActive());
        impl_->overscrollBtn->setChecked(settings->timelineAllowOverscroll());
        impl_->motionPathBtn->setChecked(settings->compositionShowMotionPathOverlay());
    }
}

ArtifactTimelineGlobalSwitches::~ArtifactTimelineGlobalSwitches() {
    delete impl_;
}

void ArtifactTimelineGlobalSwitches::setShyActive(bool active) {
    impl_->shyBtn->setChecked(active);
}

void ArtifactTimelineGlobalSwitches::setMotionBlurActive(bool active) {
    impl_->motionBlurBtn->setChecked(active);
}

void ArtifactTimelineGlobalSwitches::setFrameBlendingActive(bool active) {
    impl_->frameBlendBtn->setChecked(active);
}

void ArtifactTimelineGlobalSwitches::setGraphEditorActive(bool active) {
    if (impl_->graphEditorBtn->isChecked() == active) {
        return;
    }
    impl_->graphEditorBtn->setChecked(active);
}

void ArtifactTimelineGlobalSwitches::setMotionPathActive(bool active) {
    if (!impl_ || !impl_->motionPathBtn) {
        return;
    }
    if (impl_->motionPathBtn->isChecked() == active) {
        return;
    }
    impl_->motionPathBtn->setChecked(active);
}

void ArtifactTimelineGlobalSwitches::setTimelineOverscrollActive(bool active) {
    if (!impl_ || !impl_->overscrollBtn) {
        return;
    }
    if (impl_->overscrollBtn->isChecked() == active) {
        return;
    }
    impl_->overscrollBtn->setChecked(active);
}
} // namespace Artifact
