module;
#include <wobjectdefs.h>
#include <QWidget>

export module Artifact.Widgets.ProfilerPanel;

export namespace Artifact {

// Full-featured performance profiler panel.
// A standalone floating window (Qt::Tool) that shows:
//   - Frame-time bar graph (render frames)
//   - Render scope table: last / avg / p95 / %frame / mini-bar
//   - UI timer table (ambient ProfileTimer sources — timeline, paint events)
//   - EventBus dispatch table
//   - Heuristic findings + Copy Report button
//
// Toggle from CompositeEditor with Ctrl+Shift+D.
// Refreshes automatically every 200ms when visible.
class ProfilerPanelWidget : public QWidget {
    W_OBJECT(ProfilerPanelWidget)
public:
    explicit ProfilerPanelWidget(QWidget* parent = nullptr);
    ~ProfilerPanelWidget() override;

    // Number of render frames shown in the bar graph (default: 90).
    void setHistoryFrames(int n);
    [[nodiscard]] int historyFrames() const;

    // Copy the diagnostic report text to the system clipboard.
    void copyReportToClipboard();

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    class Impl;
    Impl* impl_;
};

} // namespace Artifact
