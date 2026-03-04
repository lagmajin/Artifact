module;
#include <wobjectimpl.h>
#include <QHBoxLayout>
#include <QPushButton>
#include <QVariant>
#include <QStyle>
#include <QToolTip>


module Artifact.Widgets.Timeline.GlobalSwitches;

namespace Artifact {
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

            auto createBtn = [&](const QString& text, const QString& tooltip) {
                auto btn = new QPushButton(text);
                btn->setCheckable(true);
                btn->setFixedSize(24, 24);
                btn->setToolTip(tooltip);
                btn->setStyleSheet(R"(
                    QPushButton {
                        background-color: transparent;
                        border: 1px solid transparent;
                        border-radius: 4px;
                        color: #AAAAAA;
                        font-weight: bold;
                        font-size: 11px;
                    }
                    QPushButton:hover {
                        background-color: #444444;
                    }
                    QPushButton:checked {
                        background-color: #0078D7;
                        color: white;
                        border: 1px solid #005A9E;
                    }
                )");
                layout->addWidget(btn);
                return btn;
            };

            // Using symbols to match AE look (or stylized letters)
            shyBtn = createBtn("S", "Hide Shy Layers");
            motionBlurBtn = createBtn("M", "Enable Motion Blur");
            frameBlendBtn = createBtn("F", "Enable Frame Blending");
            graphEditorBtn = createBtn("G", "Toggle Graph Editor");
            
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
}
