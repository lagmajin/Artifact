module;
#include <QSpinBox>
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

 ArtifactToolManager::ArtifactToolManager():impl_(new Impl())
 {

 }

 ArtifactToolManager::~ArtifactToolManager()
 {
  delete impl_;
 }

};