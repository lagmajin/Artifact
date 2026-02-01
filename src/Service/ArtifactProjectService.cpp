module;
#include <QList>
#include <wobjectimpl.h>
#include <glm/ext/matrix_projection.hpp>
module Artifact.Service.Project;

import std;
import Utils.String.UniString;
import Artifact.Project.Manager;
import Artifact.Layer.Factory;
import Artifact.Composition.Abstract;

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



  ArtifactCompositionWeakPtr currentComposition();
  FindCompositionResult findComposition(const CompositionID& id);
  ChangeCompositionResult changeCurrentComposition(const CompositionID& id);

  void removeAllAssets();
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
  
  projectManager().currentComposition()->appendLayerTop(layer);
 	
 	
 }

 void ArtifactProjectService::Impl::addAssetFromPath(const UniString& path)
 {
    auto& manager = projectManager();
 	
    //manager->

    manager.addAssetFromFilePath(path);
 	
 }

 UniString ArtifactProjectService::Impl::projectName() const
 {
  
 	
  return UniString();
 }

 void ArtifactProjectService::Impl::changeProjectName(const UniString& name)
 {
 
  

 }

 ChangeCompositionResult ArtifactProjectService::Impl::changeCurrentComposition(const CompositionID& id)
 {

  return ChangeCompositionResult();
 }

 void ArtifactProjectService::Impl::removeAllAssets()
 {

 }

 FindCompositionResult ArtifactProjectService::Impl::findComposition(const CompositionID& id)
 {
  return ArtifactProjectManager::getInstance().findComposition(id);
 }

 ArtifactCompositionWeakPtr ArtifactProjectService::Impl::currentComposition()
 {
  return projectManager().currentComposition();
 }

 W_OBJECT_IMPL(ArtifactProjectService)
	
 ArtifactProjectService::ArtifactProjectService(QObject*parent):QObject(parent),impl_(new Impl())
 {
  connect(&impl_->projectManager(),&ArtifactProjectManager::projectCreated,this,&ArtifactProjectService::projectCreated);
  connect(&impl_->projectManager(), &ArtifactProjectManager::compositionCreated, this, &ArtifactProjectService::compositionCreated);
  connect(&impl_->projectManager(), &ArtifactProjectManager::layerCreated, this, &ArtifactProjectService::layerCreated);
  connect(&impl_->projectManager(), &ArtifactProjectManager::projectChanged, this, &ArtifactProjectService::projectChanged);
  
  
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
 	

 }

 void ArtifactProjectService::addLayerToCurrentComposition(const ArtifactLayerInitParams& params)
 {
  impl_->addLayerToCurrentComposition(params);
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

 ArtifactCompositionWeakPtr ArtifactProjectService::currentComposition()
 {

  return impl_->currentComposition();
 }

 ChangeCompositionResult ArtifactProjectService::changeCurrentComposition(const CompositionID& id)
 {

  return impl_->changeCurrentComposition(id);
 }

FindCompositionResult ArtifactProjectService::findComposition(const CompositionID& id)
 {
 return impl_->findComposition(id);
 }

QVector<ProjectItem*> ArtifactProjectService::projectItems() const
{
 return impl_->projectManager().getCurrentProjectSharedPtr() ? impl_->projectManager().getCurrentProjectSharedPtr()->projectItems() : QVector<ProjectItem*>();
}

void ArtifactProjectService::createComposition(const ArtifactCompositionInitParams& params)
{

}

void ArtifactProjectService::removeAllAssets()
{
 //removeall assets via projectmanager instance

 impl_->projectManager().removeAllAssets();
 
 
}

};

//W_REGISTER_ARGTYPE(ArtifactCore::CompositionID)