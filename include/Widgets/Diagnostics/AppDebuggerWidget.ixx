module;
#include <wobjectdefs.h>
#include <QWidget>
#include <QTimerEvent>
#include <QResizeEvent>
#include <QShowEvent>
#include <QPaintEvent>

export module Artifact.Widgets.AppDebuggerWidget;

import Artifact.Widgets.CompositionRenderController;
import Artifact.Widgets.FramePipelineViewWidget;
import Artifact.Widgets.FrameDebugViewWidget;
import Artifact.Widgets.FrameResourceInspectorWidget;
import Artifact.Widgets.TraceTimelineWidget;

export namespace Artifact {

class AppDebuggerWidget : public QWidget {
    W_OBJECT(AppDebuggerWidget)
public:
    explicit AppDebuggerWidget(CompositionRenderController* controller = nullptr,
                               QWidget* parent = nullptr);
    ~AppDebuggerWidget() override;

protected:
    void timerEvent(QTimerEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    class Impl;
    Impl* impl_;
};

} // namespace Artifact
