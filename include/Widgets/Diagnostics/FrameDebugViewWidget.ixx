module;
#include <wobjectdefs.h>
#include <QWidget>
export module Artifact.Widgets.FrameDebugViewWidget;

import Frame.Debug;

export namespace Artifact {

class FrameDebugViewWidget : public QWidget {
    W_OBJECT(FrameDebugViewWidget)
public:
    explicit FrameDebugViewWidget(QWidget* parent = nullptr);
    ~FrameDebugViewWidget() override;
    void setFrameDebugSnapshot(const ArtifactCore::FrameDebugSnapshot& snapshot);

private:
    class Impl;
    Impl* impl_;
};

} // namespace Artifact
