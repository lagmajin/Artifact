module;
#include <QWidget>
#include <wobjectdefs.h>
export module Artifact.Widgets.SeekBar;

export namespace Artifact
{
	
	
	
 class ArtifactSeekBar:public QWidget
 {
 	W_OBJECT(ArtifactSeekBar)
 private:
  class Impl;
  Impl* impl_;
 protected:
 	void paintEvent(QPaintEvent* event) override;
 public:
  explicit ArtifactSeekBar(QWidget*parent=nullptr);
  ~ArtifactSeekBar();
 };
	
	

};
