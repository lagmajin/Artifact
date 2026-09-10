module;
#include <QEvent>
#include <QWidget>
#include <wobjectdefs.h>

export module Artifact.Widgets.HistoryTimelineWidget;

export namespace Artifact {

class ArtifactHistoryTimelineWidget : public QWidget {
  W_OBJECT(ArtifactHistoryTimelineWidget)
public:
  explicit ArtifactHistoryTimelineWidget(QWidget* parent = nullptr);
  ~ArtifactHistoryTimelineWidget() override;

  void refreshHistory();

protected:
  bool eventFilter(QObject* watched, QEvent* event) override;

private:
  class Impl;
  Impl* impl_ = nullptr;
  void updateInspector();
  void restoreSelectedState();
};

} // namespace Artifact
