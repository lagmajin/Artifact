module;
#include <wobjectimpl.h>
#include <QHBoxLayout>
#include <QIcon>
#include <QPushButton>
#include <QString>
#include <QSize>
#include <QVariant>
#include <QStyle>
#include <QToolTip>
#include <QWidget>

module Artifact.Widgets.Timeline.GlobalSwitches;

import Utils.Path;

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

    void setupUi(QWidget* parent) {
        auto layout = new QHBoxLayout(parent);
        layout->setContentsMargins(4, 0, 4, 0);
        layout->setSpacing(2);

        auto createBtn = [&](const QString& tooltip, const QIcon& icon) {
            auto btn = new QPushButton();
            btn->setCheckable(true);
            btn->setFixedSize(24, 24);
            btn->setToolTip(tooltip);
            btn->setIcon(icon);
            btn->setIconSize(QSize(16, 16));
            btn->setStyleSheet(R"(
                QPushButton {
                    background-color: transparent;
                    border: 1px solid transparent;
                    border-radius: 4px;
                }
                QPushButton:hover {
                    background-color: #444444;
                }
                QPushButton:checked {
                    background-color: #0078D7;
                    border: 1px solid #005A9E;
                }
            )");
            layout->addWidget(btn);
            return btn;
        };

        shyBtn = createBtn(
            "Hide Shy Layers",
            loadIconWithFallback(
                QStringLiteral("MaterialVS/neutral/visibility_off.svg"),
                QStringLiteral("visibility_off.png")));
        motionBlurBtn = createBtn(
            "Enable Motion Blur",
            loadIconWithFallback(
                QStringLiteral("MaterialVS/colored/E3E3E3/stopwatch.svg"),
                QStringLiteral("stopwatch.png")));
        frameBlendBtn = createBtn(
            "Enable Frame Blending",
            loadIconWithFallback(
                QStringLiteral("MaterialVS/neutral/tune.svg"),
                QStringLiteral("tune.png")));
        graphEditorBtn = createBtn(
            "Toggle Graph Editor",
            loadIconWithFallback(
                QStringLiteral("MaterialVS/neutral/view_sidebar.svg"),
                QStringLiteral("view_sidebar.png")));

        layout->addStretch();
    }
};

ArtifactTimelineGlobalSwitches::ArtifactTimelineGlobalSwitches(QWidget* parent)
    : QWidget(parent), impl_(new Impl()) {
    impl_->setupUi(this);

    connect(impl_->shyBtn, &QPushButton::toggled, this, [this](bool v){ Q_EMIT shyChanged(v); });
    connect(impl_->motionBlurBtn, &QPushButton::toggled, this, [this](bool v){ Q_EMIT motionBlurChanged(v); });
    connect(impl_->frameBlendBtn, &QPushButton::toggled, this, [this](bool v){ Q_EMIT frameBlendingChanged(v); });
    connect(impl_->graphEditorBtn, &QPushButton::toggled, this, [this](bool v){ Q_EMIT graphEditorToggled(v); });
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
} // namespace Artifact
