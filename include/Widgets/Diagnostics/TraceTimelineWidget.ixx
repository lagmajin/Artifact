module;
#include <wobjectdefs.h>
#include <QWidget>

export module Artifact.Widgets.TraceTimelineWidget;

import Core.Diagnostics.Trace;

export namespace Artifact {

class TraceTimelineWidget : public QWidget {
    W_OBJECT(TraceTimelineWidget)
public:
    explicit TraceTimelineWidget(QWidget* parent = nullptr);
    ~TraceTimelineWidget() override;

    void setTraceSnapshot(const ArtifactCore::TraceSnapshot& snapshot);
    void setFocusedThreadId(std::uint64_t threadId);
    void setFocusedMutexName(const QString& mutexName);

private:
    class Impl;
    Impl* impl_;
};

} // namespace Artifact
