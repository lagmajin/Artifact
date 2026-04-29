module;
#include <wobjectdefs.h>
#include <QWidget>
#include <QString>
#include <QKeyEvent>

export module Artifact.Widgets.DebugRenderHarnessWidget;

import Frame.Debug;

export namespace Artifact {

class DebugRenderHarnessWidget : public QWidget {
    W_OBJECT(DebugRenderHarnessWidget)
public:
    explicit DebugRenderHarnessWidget(QWidget* parent = nullptr);
    ~DebugRenderHarnessWidget() override;

    void setFrameDebugSnapshot(const ArtifactCore::FrameDebugSnapshot& snapshot);
    void setScenePreset(const QString& presetName);

protected:
    void keyPressEvent(QKeyEvent* event) override;

private:
    class Impl;
    Impl* impl_;
};

} // namespace Artifact
