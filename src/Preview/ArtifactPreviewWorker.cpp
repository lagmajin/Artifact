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

 ArtifactPreviewWorker::ArtifactPreviewWorker(const QObject* parent /*= nullptr*/):QObject(parent)
 {

 }

 ArtifactPreviewWorker::~ArtifactPreviewWorker()
 {

 }

};
