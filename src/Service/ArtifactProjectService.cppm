module;
#include <wobjectimpl.h>
#include <glm/ext/matrix_projection.hpp>
module Artifact.Service.Project;

import std;
import Artifact.Project.Manager;
import Artifact.Layer.Factory;

namespace Artifact
{
	
 class ArtifactProjectService::Impl
 {
 private:
 	
 	
 public:
  Impl();
  ~Impl();
  static ArtifactProjectManager& projectManager();
  void addLayerToCurrentComposition(const ArtifactLayerInitParams& params);
  void addAssetFromPath(const UniString& path);
  UniString projectName() const;
  void changeProjectName(const UniString& name);
 };

 ArtifactProjectService::Impl::Impl()
 {

 }

 ArtifactProjectService::Impl::~Impl()
 {

 }

 ArtifactProjectManager& ArtifactProjectService::Impl::projectManager()
 {
  return ArtifactProjectManager::getInstance();
 }

 void ArtifactProjectService::Impl::addLayerToCurrentComposition(const ArtifactLayerInitParams& params)
 {
 	
  auto factory = ArtifactLayerFactory();

  auto layer = factory.createNewLayer(params);
  
  projectManager().currentComposition()->addLayer(layer);
 	
 	
 }

 void ArtifactProjectService::Impl::addAssetFromPath(const UniString& path)
 {
    auto& manager = projectManager();
 	
    manager.addAssetFromFilePath(path);
 	
 }

 UniString ArtifactProjectService::Impl::projectName() const
 {

  return UniString();
 }

 void ArtifactProjectService::Impl::changeProjectName(const UniString& name)
 {
  //projectManager().set
 	
 }

 W_OBJECT_IMPL(ArtifactProjectService)
	
 ArtifactProjectService::ArtifactProjectService(QObject*parent):QObject(parent),impl_(new Impl())
 {
  connect(&impl_->projectManager(),&ArtifactProjectManager::newProjectCreated,this,&ArtifactProjectService::projectCreated);

 }

 ArtifactProjectService::~ArtifactProjectService()
 {
  delete impl_;
 }

 ArtifactProjectService* ArtifactProjectService::instance()
 {
  static ArtifactProjectService service;
  return&service;

 }

 void ArtifactProjectService::projectSettingChanged(const ArtifactProjectSettings& setting)
 {

 }

 void ArtifactProjectService::addLayer(const CompositionID& id, const ArtifactLayerInitParams& param)
 {
 	//ArtifactProjectManager::getInstance()->find

 }

 void ArtifactProjectService::addLayerToCurrentComposition(const ArtifactLayerInitParams& params)
 {

 	
 	
 	
 }

 UniString ArtifactProjectService::projectName() const
 {
 	
  return impl_->projectName();
 }

 void ArtifactProjectService::changeProjectName(const UniString& string)
 {
  impl_->changeProjectName(string);
 }

 void ArtifactProjectService::addAssetFromPath(const UniString& path)
 {
  impl_->addAssetFromPath(path);
 }
	
	
};