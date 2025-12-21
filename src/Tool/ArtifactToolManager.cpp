module;
#include <QSpinBox>
#include <wobjectimpl.h>
module Artifact.Tool.Manager;


namespace Artifact
{
 class ArtifactToolManager::Impl
 {
 private:
 public:
  Impl();
  ~Impl();
 };

 ArtifactToolManager::Impl::Impl()
 {

 }

 ArtifactToolManager::Impl::~Impl()
 {

 }


	//W_OBJECT_IMPL(ArtifactToolManager)

 ArtifactToolManager::ArtifactToolManager():impl_(new Impl())
 {

 }

 ArtifactToolManager::~ArtifactToolManager()
 {
  delete impl_;
 }

};