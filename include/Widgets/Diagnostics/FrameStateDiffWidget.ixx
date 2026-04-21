module;
#include <wobjectdefs.h>
#include <QWidget>

export module Artifact.Widgets.FrameStateDiffWidget;

import Frame.Debug;
import Core.Diagnostics.Trace;

export namespace Artifact {

class FrameStateDiffWidget : public QWidget {
    W_OBJECT(FrameStateDiffWidget)
public:
    explicit FrameStateDiffWidget(QWidget* parent = nullptr);
    ~FrameStateDiffWidget() override;

    void setFrameDebugSnapshot(const ArtifactCore::FrameDebugSnapshot& snapshot,
                               const ArtifactCore::TraceSnapshot& trace);

private:
    class Impl;
    Impl* impl_;
};

} // namespace Artifact
