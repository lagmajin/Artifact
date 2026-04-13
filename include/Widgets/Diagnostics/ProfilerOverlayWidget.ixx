module;
#include <wobjectdefs.h>
#include <QWidget>

export module Artifact.Widgets.ProfilerOverlay;

export namespace Artifact {

// Semi-transparent performance overlay for the composition editor.
// Shows a frame-time bar graph, top-N scope table, and EventBus event heat map.
// Toggle visibility from outside; the widget self-refreshes every 150 ms.
class ProfilerOverlayWidget : public QWidget {
    W_OBJECT(ProfilerOverlayWidget)
public:
    explicit ProfilerOverlayWidget(QWidget* parent = nullptr);
    ~ProfilerOverlayWidget() override;

    // Number of latest frames shown in the bar graph (default: 60).
    void setBarGraphFrameCount(int n);
    [[nodiscard]] int barGraphFrameCount() const;

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    class Impl;
    Impl* impl_;
};

} // namespace Artifact
