module;
#include <wobjectdefs.h>
#include <QWidget>

export module Artifact.Widgets.EventBusDebugger;

export namespace Artifact {

// EventBus Debugger – 3-tab diagnostic widget.
//
// Tab 1 "Fire Log"    — ring-buffer of recent publish() calls.
//                       "Dupes Only" filter shows rapid re-fires within 150 ms.
// Tab 2 "Subscribers" — cards per registered event type;
//                       red border = never fired since debugger attached.
// Tab 3 "Frequency"   — horizontal bars sorted by fires/sec;
//                       "HIGH" badge when above threshold (default 30/s).
//
// Attaches to ArtifactCore::globalEventBus() on construction.
// Toggle from CompositeEditor with Ctrl+Shift+E.
// Refreshes automatically every 200 ms when visible.
class EventBusDebuggerWidget : public QWidget {
    W_OBJECT(EventBusDebuggerWidget)
public:
    explicit EventBusDebuggerWidget(QWidget* parent = nullptr);
    ~EventBusDebuggerWidget() override;

protected:
    void timerEvent(QTimerEvent* event) override;
    void closeEvent(QCloseEvent* event) override;

private:
    class Impl;
    Impl* impl_;
};

} // namespace Artifact
