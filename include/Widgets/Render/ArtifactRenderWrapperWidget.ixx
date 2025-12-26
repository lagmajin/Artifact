module;
#include <QWidget>
export module Artifact.Render.WrapperWidget;


export namespace Artifact
{
 class ArtifactRenderWrapperWidget:public QWidget
 {
 private:
  class Impl;
  Impl* impl_;
 public:
  explicit ArtifactRenderWrapperWidget(QWidget* parent = nullptr);
  ~ArtifactRenderWrapperWidget();
 };
	
};