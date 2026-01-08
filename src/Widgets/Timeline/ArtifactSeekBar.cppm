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
  resize(6, 800);
 	
  setAttribute(Qt::WA_NoSystemBackground);
  setAttribute(Qt::WA_OpaquePaintEvent, true);
  setAttribute(Qt::WA_TranslucentBackground);
 }

 ArtifactSeekBar::~ArtifactSeekBar()
 {
  delete impl_;
 }

 void ArtifactSeekBar::paintEvent(QPaintEvent* event)
 {
  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing, true);
 	
  p.fillRect(rect(), QColor(250, 20, 30));
 	
  //double ratio = impl_->position_ / impl_->duration_;
  //int progressWidth = static_cast<int>(ratio * width());
  //p.fillRect(0, 0, progressWidth, height(), QColor(100, 180, 255));
 	
 	
 }

};

