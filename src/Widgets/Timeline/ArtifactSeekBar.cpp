module;

#include <wobjectimpl.h>
#include <QWidget>
module Artifact.Widgets.SeekBar;


namespace Artifact
{
	W_OBJECT_IMPL(ArtifactSeekBar)

 class ArtifactSeekBar::Impl
 {
 public:
  Impl();
  ~Impl();
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

};

