module;
#include <wobjectdefs.h>
#include <QWidget>
export module Artifact.Widgets.LayerPanelWidget;

import Utils.Id;

export namespace Artifact
{
 using namespace ArtifactCore;
	
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
   void mouseMoveEvent(QMouseEvent* event) override;
   void leaveEvent(QEvent* event) override;
   void paintEvent(QPaintEvent* event) override;
   void dragEnterEvent(class QDragEnterEvent* event) override;
   void dragMoveEvent(class QDragMoveEvent* event) override;
   void dropEvent(class QDropEvent* event) override;
   void dragLeaveEvent(class QDragLeaveEvent* event) override;
  public:
  explicit ArtifactLayerPanelWidget(QWidget* parent = nullptr);

  ~ArtifactLayerPanelWidget();
 
  // Set the composition to display layers for
  void setComposition(const CompositionID& id);

 };

 class ArtifactLayerTimelinePanelWrapper :public QWidget
 {
 private:
  class Impl;
  Impl* impl_;
 protected:

 public:
  explicit ArtifactLayerTimelinePanelWrapper(QWidget* parent = nullptr);
  ArtifactLayerTimelinePanelWrapper(const CompositionID& id, QWidget* parent = nullptr);
  ~ArtifactLayerTimelinePanelWrapper();
  void setComposition(const CompositionID& id);
 	
 };




};