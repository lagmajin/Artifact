module;
#include <wobjectimpl.h>
#include <QElapsedTimer>
module Artifact.Preview.Controller;



namespace Artifact
{
	W_OBJECT_IMPL(ArtifactPreviewController)
	
 class ArtifactPreviewController::Impl
 {
 public:
  Impl();
  ~Impl();
  void tick();
  QElapsedTimer clock;
 };

 ArtifactPreviewController::Impl::Impl()
 {

 }

 void ArtifactPreviewController::Impl::tick()
 {

 }

 ArtifactPreviewController::ArtifactPreviewController(QObject* parent/*=nullptr*/) :QObject(parent),impl_(new Impl())
 {

 }

 ArtifactPreviewController::~ArtifactPreviewController()
 {
  delete impl_;
 }

};
