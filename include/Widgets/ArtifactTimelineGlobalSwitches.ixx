module;
#include <wobjectimpl.h>
#include <QPushButton>
#include <QWidget>
export module Artifact.Widgets.Timeline.GlobalSwitches;

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

        // Verdigris Signals
        void shyChanged(bool active) W_SIGNAL(shyChanged, active);
        void motionBlurChanged(bool active) W_SIGNAL(motionBlurChanged, active);
        void frameBlendingChanged(bool active) W_SIGNAL(frameBlendingChanged, active);
        void graphEditorToggled(bool active) W_SIGNAL(graphEditorToggled, active);
    };
}
