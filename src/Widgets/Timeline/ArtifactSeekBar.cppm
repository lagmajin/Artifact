module;

#include <wobjectimpl.h>
#include <QRect>
#include <QWidget>
#include <QPainter>
module Artifact.Widgets.SeekBar;

import std;
import Frame.Position;

namespace Artifact
{
	W_OBJECT_IMPL(ArtifactSeekBar)

 class ArtifactSeekBar::Impl
 {private:
		
 public:
  Impl();
  ~Impl();
  double duration_;   // ‘ŽžŠÔ
  bool dragging_=false;     // ƒhƒ‰ƒbƒO’†‚©
  QRect handleRect_;
 };

 ArtifactSeekBar::Impl::Impl()
 {

 }

 ArtifactSeekBar::Impl::~Impl()
 {

 }

 ArtifactSeekBar::ArtifactSeekBar(QWidget*parent):QWidget(parent),impl_(new Impl)
 {

 }

 ArtifactSeekBar::~ArtifactSeekBar()
 {
  delete impl_;
 }

 void ArtifactSeekBar::paintEvent(QPaintEvent* event)
 {
  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing, true);
 	
  p.fillRect(rect(), QColor(50, 50, 50));
 	
  //double ratio = impl_->position_ / impl_->duration_;
  //int progressWidth = static_cast<int>(ratio * width());
  //p.fillRect(0, 0, progressWidth, height(), QColor(100, 180, 255));
 	
 	
 }

};

