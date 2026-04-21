module;
#include <utility>

#include <wobjectdefs.h>
#include <QWidget>
export module Artifact.Widgets.DebugConsoleWidget;

import Frame.Debug;

export namespace Artifact {

class ArtifactDebugConsoleWidget : public QWidget {
    W_OBJECT(ArtifactDebugConsoleWidget)
public:
    explicit ArtifactDebugConsoleWidget(QWidget* parent = nullptr);
    ~ArtifactDebugConsoleWidget() override;
    int debugConsoleFontPointSize() const;
    void setDebugConsoleFontPointSize(int pointSize);
    void setFrameDebugSnapshot(const ArtifactCore::FrameDebugSnapshot& snapshot);

private:
    class Impl;
    Impl* impl_;
};

} // namespace Artifact
