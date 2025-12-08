module;

#include <QObject>
#include <QThread>
module Artifact.Preview.Worker;


namespace Artifact
{

 class ArtifactPreviewWorker::Impl
 {
 private:

 public:
  Impl();

 };

 ArtifactPreviewWorker::Impl::Impl()
 {

 }

 ArtifactPreviewWorker::ArtifactPreviewWorker(QObject* parent /*= nullptr*/):QObject(parent),impl_(new Impl())
 {

 }

 ArtifactPreviewWorker::~ArtifactPreviewWorker()
 {
  delete impl_;
 }

};
