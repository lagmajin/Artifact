module;
#include <QWidget>
#include <wobjectdefs.h>
export module Artifact.Timeline.TimeCodeWidget;

export namespace Artifact
{
 class ArtifactTimeCodeWidget :public QWidget {
  W_OBJECT(ArtifactTimeCodeWidget)
 private:
  class Impl;
  Impl* impl_;
 protected:

 public:
  explicit ArtifactTimeCodeWidget(QWidget* parent = nullptr);
  ~ArtifactTimeCodeWidget();

  void updateTimeCode(int frame);
 };
	
};