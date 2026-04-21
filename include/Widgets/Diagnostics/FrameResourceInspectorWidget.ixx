module;
#include <wobjectdefs.h>
#include <QWidget>

export module Artifact.Widgets.FrameResourceInspectorWidget;

import Frame.Debug;
import Core.Diagnostics.Trace;

export namespace Artifact {

class FrameResourceInspectorWidget : public QWidget {
    W_OBJECT(FrameResourceInspectorWidget)
public:
    explicit FrameResourceInspectorWidget(QWidget* parent = nullptr);
    ~FrameResourceInspectorWidget() override;

    void setFrameDebugSnapshot(const ArtifactCore::FrameDebugSnapshot& snapshot,
                               const ArtifactCore::TraceSnapshot& trace);

private:
    class Impl;
    Impl* impl_;
};

} // namespace Artifact
