module;
#include <wobjectdefs.h>
#include <QWidget>
export module Artifact.Widgets.LayerPanelWidget;

export namespace Artifact
{
 class ArtifactLayerPanelHeaderWidget :public QWidget
 {
  W_OBJECT(ArtifactLayerPanelHeaderWidget)
 private:
  class Impl;
  Impl* impl_;
 protected:

 public:
  explicit ArtifactLayerPanelHeaderWidget(QWidget* parent = nullptr);
  ~ArtifactLayerPanelHeaderWidget();
 public /*signals*/:
  void lockClicked() W_SIGNAL(lockClicked)
void soloClicked() W_SIGNAL(soloClicked) 	
 };


 class ArtifactLayerPanelWidget :public QWidget
 {
  W_OBJECT(ArtifactLayerPanelWidget)
 private:
  class Impl;
  Impl* impl_;
 protected:
  void mousePressEvent(QMouseEvent* event) override;
  void paintEvent(QPaintEvent* event) override;
 public:
  explicit ArtifactLayerPanelWidget(QWidget* parent = nullptr);
  ~ArtifactLayerPanelWidget();

 };

 class ArtifactLayerTimelinePanelWrapper :public QWidget
 {
 private:
  class Impl;
  Impl* impl_;
 protected:

 public:
  explicit ArtifactLayerTimelinePanelWrapper(QWidget* parent = nullptr);
  ~ArtifactLayerTimelinePanelWrapper();
 };




};