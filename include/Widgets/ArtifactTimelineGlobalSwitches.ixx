module;
#include <utility>
#include <wobjectimpl.h>
#include <QPushButton>
#include <QWidget>
export module Artifact.Widgets.Timeline.GlobalSwitches;

import Event.Bus;

export namespace Artifact {
    class ArtifactTimelineGlobalSwitches final : public QWidget {
        W_OBJECT(ArtifactTimelineGlobalSwitches)
    private:
        class Impl;
        Impl* impl_;
    public:
        explicit ArtifactTimelineGlobalSwitches(QWidget* parent = nullptr);
        ~ArtifactTimelineGlobalSwitches() override;

        void setShyActive(bool active);
        void setMotionBlurActive(bool active);
        void setFrameBlendingActive(bool active);
        void setGraphEditorActive(bool active);
        void setEventBus(ArtifactCore::EventBus* eventBus);
    };
}
