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
  explicit ArtifactLayerPanelHeaderWidget(const QWidget* parent = nullptr);
  ~ArtifactLayerPanelHeaderWidget();
 };


 class ArtifactLayerPanelWidget :public QWidget
 {
  W_OBJECT(ArtifactLayerPanelWidget)
 private:
  class Impl;
  Impl* impl_;
 protected:
  void paintEvent(QPaintEvent* event) override;
 public:
  explicit ArtifactLayerPanelWidget(QWidget* parent = nullptr);
  ~ArtifactLayerPanelWidget();

 };

 class ArtifactLayerPanelWrapper :public QWidget
 {
 private:
  class Impl;
  Impl* impl_;
 protected:

 public:
  explicit ArtifactLayerPanelWrapper(QWidget* parent = nullptr);
  ~ArtifactLayerPanelWrapper();
 };




};