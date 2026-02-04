module;
#include <QList>
#include <wobjectimpl.h>
#include <glm/ext/matrix_projection.hpp>
#include <QDebug>
module Artifact.Service.Project;

import std;
import Utils.String.UniString;
import Artifact.Project.Manager;
import Artifact.Layer.Factory;
import Artifact.Composition.Abstract;
import Artifact.Project.Items;

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

// Impl::removeLayerFromComposition was removed; use manager call in service wrapper

 ArtifactProjectService::Impl::~Impl()
 {

 }

 ArtifactProjectManager& ArtifactProjectService::Impl::projectManager()
 {
  return ArtifactProjectManager::getInstance();
 }

 void ArtifactProjectService::Impl::addLayerToCurrentComposition(const ArtifactLayerInitParams& params)
 {
 	
  	
    // Delegate to ArtifactProjectManager - this is the proper flow
    auto& manager = projectManager();
    auto result = manager.addLayerToCurrentComposition(const_cast<ArtifactLayerInitParams&>(params));
    qDebug() << "[ArtifactProjectService::Impl::addLayerToCurrentComposition] delegated to manager, result=" << result.success;
 	
 	
 }

 void ArtifactProjectService::Impl::addAssetFromPath(const UniString& path)
 {
    auto& manager = projectManager();

    manager.addAssetFromFilePath(path);

    // Notify listeners that project changed so UI updates immediately
    // Notify manager/service that project changed so UI updates immediately
    manager.projectChanged();
 	
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
}

bool ArtifactProjectService::removeLayerFromComposition(const CompositionID& compositionId, const LayerID& layerId)
{
    bool ok = impl_->projectManager().removeLayerFromComposition(compositionId, layerId);
    if (ok) layerRemoved(layerId);
    return ok;
}

 bool ArtifactProjectService::removeComposition(const CompositionID& id)
 {
   auto& pm = impl_->projectManager();
   auto projectShared = pm.getCurrentProjectSharedPtr();
   if (!projectShared) return false;
   bool ok = projectShared->removeCompositionById(id);
   if (ok) projectShared->projectChanged();
   return ok;
 }

 bool ArtifactProjectService::renameComposition(const CompositionID& id, const UniString& name)
 {
   auto& pm = impl_->projectManager();
   auto projectShared = pm.getCurrentProjectSharedPtr();
   if (!projectShared) return false;
   auto items = projectShared->projectItems();
   for (auto root : items) {
     if (!root) continue;
     for (auto c : root->children) {
       if (!c) continue;
       if (c->type() == eProjectItemType::Composition) {
         CompositionItem* ci = static_cast<CompositionItem*>(c);
         if (ci->compositionId == id) {
           ci->name = name;
           projectShared->projectChanged();
           return true;
         }
       }
     }
   }
   return false;
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