module;
#include <wobjectdefs.h>
#include <QWidget>

export module Artifact.Widgets.FramePipelineViewWidget;

import Frame.Debug;
import Core.Diagnostics.Trace;

export namespace Artifact {

class FramePipelineViewWidget : public QWidget {
    W_OBJECT(FramePipelineViewWidget)
public:
    explicit FramePipelineViewWidget(QWidget* parent = nullptr);
    ~FramePipelineViewWidget() override;

    void setFrameDebugSnapshot(const ArtifactCore::FrameDebugSnapshot& snapshot,
                               const ArtifactCore::TraceSnapshot& trace);

private:
    class Impl;
    Impl* impl_;
};

} // namespace Artifact
