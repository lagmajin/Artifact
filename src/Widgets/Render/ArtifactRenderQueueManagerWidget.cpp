module;
#include <QVector>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QListWidget>
#include <QPushButton>
#include <QLabel>
#include <QLineEdit>
#include <wobjectimpl.h>
module Artifact.Widgets.Render.QueueManager;
import std;
import Widgets.Utils.CSS;


namespace Artifact
{
 using namespace ArtifactCore;
	
 
	

	W_OBJECT_IMPL(RenderQueueManagerWidget)

 class RenderQueueManagerWidget::Impl
 {
 private:
 public:
  Impl();
  ~Impl();
 };

 RenderQueueManagerWidget::Impl::Impl()
 {

 }

 RenderQueueManagerWidget::Impl::~Impl()
 {

 }

 RenderQueueManagerWidget::RenderQueueManagerWidget(QWidget* parent /*= nullptr*/) :QWidget(parent),impl_(new Impl())
 {

 }

 RenderQueueManagerWidget::~RenderQueueManagerWidget()
 {
  delete impl_;
 }

 QSize RenderQueueManagerWidget::sizeHint() const
 {
  return QSize(600, 600);
 }


};