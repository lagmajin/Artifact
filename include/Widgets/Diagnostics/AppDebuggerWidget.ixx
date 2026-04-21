module;
#include <wobjectdefs.h>
#include <QWidget>
#include <QTimerEvent>

export module Artifact.Widgets.AppDebuggerWidget;

import Artifact.Widgets.CompositionRenderController;
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

private:
    class Impl;
    Impl* impl_;
};

} // namespace Artifact
