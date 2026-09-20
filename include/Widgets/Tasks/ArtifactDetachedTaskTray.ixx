module;

#include <vector>

#include <wobjectdefs.h>
#include <QtWidgets/QWidget>
#include <QString>

export module Artifact.Widgets.DetachedTaskTray;

import Event.Bus;

export namespace Artifact {

/// Non-activating floating tray for Detached Task state.
///
/// Shown without activation so a task that appears mid-drag never steals focus
/// from the composition view.  Collapses to a compact badge once every task has
/// finished.
class ArtifactDetachedTaskTray : public QWidget {
  W_OBJECT(ArtifactDetachedTaskTray)
public:
  explicit ArtifactDetachedTaskTray(QWidget* parent = nullptr);
  ~ArtifactDetachedTaskTray() override;

  void showTray();
  void hideTray();
  void toggleTray();
  void setExpanded(bool expanded);
  bool isExpanded() const;

  /// Re-reads the service snapshot and rebuilds the rows.
  void refresh();

protected:
  void paintEvent(QPaintEvent* event) override;
  void mousePressEvent(QMouseEvent* event) override;
  void mouseMoveEvent(QMouseEvent* event) override;
  void mouseReleaseEvent(QMouseEvent* event) override;

private:
  class Impl;
  Impl* impl_ = nullptr;

  ArtifactCore::EventBus eventBus_ = ArtifactCore::globalEventBus();
  std::vector<ArtifactCore::EventBus::Subscription> eventBusSubscriptions_;
};

} // namespace Artifact
