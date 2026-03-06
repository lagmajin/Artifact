module;
#include <QList>
#include <wobjectimpl.h>
#include <glm/ext/matrix_projection.hpp>
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QImage>
module Artifact.Service.Project;

import std;
import Utils.String.UniString;
import Artifact.Project.Manager;
import Artifact.Layer.Factory;
import Artifact.Composition.Abstract;
import Artifact.Project.Items;
import File.TypeDetector;
import Artifact.Layers.Selection.Manager;
import Artifact.Application.Manager;
//import Artifact.Render.FrameCache;

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
  QStringList importAssetsFromPaths(const QStringList& sourcePaths);
  void setPreviewQualityPreset(PreviewQualityPreset preset);
  PreviewQualityPreset previewQualityPreset() const;
  UniString projectName() const;
  void changeProjectName(const UniString& name);



  ArtifactCompositionWeakPtr currentComposition();
  FindCompositionResult findComposition(const CompositionID& id);
  ChangeCompositionResult changeCurrentComposition(const CompositionID& id);

  void removeAllAssets();
  PreviewQualityPreset qualityPreset_ = PreviewQualityPreset::Preview;
  //ProgressiveRenderer progressiveRenderer_;

  void checkImportedAssetCompatibility(const QStringList& importedPaths);
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
    QStringList input;
    input.append(path.toQString());
    importAssetsFromPaths(input);
 }

 QStringList ArtifactProjectService::Impl::importAssetsFromPaths(const QStringList& sourcePaths)
 {
  QStringList importedPaths;
  if (sourcePaths.isEmpty()) {
   return importedPaths;
  }

  auto& manager = projectManager();
  QString assetsRoot = manager.currentProjectAssetsPath();
  QStringList toCopy;
  QStringList alreadyInProject;

  for (const auto& src : sourcePaths) {
   if (src.isEmpty()) continue;
   QFileInfo info(src);
   if (!info.exists() || !info.isFile()) continue;

   QString abs = info.absoluteFilePath();
   if (!assetsRoot.isEmpty() && abs.startsWith(assetsRoot, Qt::CaseInsensitive)) {
    alreadyInProject.append(abs);
   } else {
    toCopy.append(abs);
   }
  }

  QStringList copied = manager.copyFilesToProjectAssets(toCopy);
  importedPaths.append(copied);
  importedPaths.append(alreadyInProject);

  if (!importedPaths.isEmpty()) {
   manager.addAssetsFromFilePaths(importedPaths);
   checkImportedAssetCompatibility(importedPaths);
  }

  return importedPaths;
 }

 void ArtifactProjectService::Impl::checkImportedAssetCompatibility(const QStringList& importedPaths)
 {
  if (importedPaths.isEmpty()) return;

  QSize compSize;
  if (auto comp = currentComposition().lock()) {
   compSize = comp->settings().compositionSize();
  }

  ArtifactCore::FileTypeDetector detector;
  for (const auto& path : importedPaths) {
   if (path.isEmpty()) continue;

   auto type = detector.detect(path);
   if (type == ArtifactCore::FileType::Unknown) {
    qWarning() << "[CompatibilityGuard] Unknown/unsupported file type:" << path;
    continue;
   }

   if (type == ArtifactCore::FileType::Image) {
    QImage img(path);
    if (img.isNull()) {
      qWarning() << "[CompatibilityGuard] Image decode failed:" << path;
      continue;
    }
    if (compSize.width() > 0 && compSize.height() > 0 &&
        (img.width() != compSize.width() || img.height() != compSize.height())) {
      qWarning() << "[CompatibilityGuard] Image resolution differs from composition. image="
                 << img.width() << "x" << img.height()
                 << " comp=" << compSize.width() << "x" << compSize.height()
                 << " path=" << path;
    }
   }
  }
 }

 void ArtifactProjectService::Impl::setPreviewQualityPreset(PreviewQualityPreset preset)
 {
  qualityPreset_ = preset;
  switch (preset) {
  case PreviewQualityPreset::Draft:
   //progressiveRenderer_.setQuality(RenderQuality::Draft);
   //progressiveRenderer_.setDraftQuality(4);
   //progressiveRenderer_.setPreviewQuality(2);
   break;
  case PreviewQualityPreset::Preview:
   //progressiveRenderer_.setQuality(RenderQuality::Preview);
   //progressiveRenderer_.setDraftQuality(4);
   //progressiveRenderer_.setPreviewQuality(2);
   break;
  case PreviewQualityPreset::Final:
   //progressiveRenderer_.setQuality(RenderQuality::Final);
   //progressiveRenderer_.setDraftQuality(2);
   //progressiveRenderer_.setPreviewQuality(1);
   break;
  }
 }

 PreviewQualityPreset ArtifactProjectService::Impl::previewQualityPreset() const
 {
  return qualityPreset_;
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

 void ArtifactProjectService::selectLayer(const LayerID& id)
 {
  layerSelected(id);
 }

 void ArtifactProjectService::addLayer(const CompositionID& id, const ArtifactLayerInitParams& param)
 {
 	

 }

void ArtifactProjectService::addLayerToCurrentComposition(const ArtifactLayerInitParams& params)
{
 impl_->addLayerToCurrentComposition(params);
}

bool ArtifactProjectService::removeLayerFromComposition(const CompositionID& compositionId, const LayerID& layerId)
{
    bool ok = impl_->projectManager().removeLayerFromComposition(compositionId, layerId);
    if (ok) layerRemoved(compositionId, layerId);
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

 QStringList ArtifactProjectService::importAssetsFromPaths(const QStringList& sourcePaths)
 {
  return impl_->importAssetsFromPaths(sourcePaths);
 }

 ArtifactCompositionWeakPtr ArtifactProjectService::currentComposition()
 {

  return impl_->currentComposition();
 }

std::shared_ptr<ArtifactProject> ArtifactProjectService::getCurrentProjectSharedPtr() const
{
    return impl_->projectManager().getCurrentProjectSharedPtr();
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

void ArtifactProjectService::createComposition(const UniString& name)
{
 auto& manager = impl_->projectManager();
 auto result = manager.createComposition(name);
 if (result.success) {
  qDebug() << "[ArtifactProjectService::createComposition(UniString)] succeeded, id:" << result.id.toString();
 } else {
  qDebug() << "[ArtifactProjectService::createComposition(UniString)] failed";
 }
}

void ArtifactProjectService::createComposition(const ArtifactCompositionInitParams& params)
{
 auto& manager = impl_->projectManager();
 auto result = manager.createComposition(params);
 if (result.success) {
  qDebug() << "[ArtifactProjectService::createComposition] succeeded, id:" << result.id.toString();
 } else {
  qDebug() << "[ArtifactProjectService::createComposition] failed";
 }
}

void ArtifactProjectService::createProject(const ArtifactProjectSettings& setting)
{
 auto& manager = impl_->projectManager();
 manager.createProject(setting.projectName());
}

void ArtifactProjectService::removeAllAssets()
{
 //removeall assets via projectmanager instance

 impl_->projectManager().removeAllAssets();
 
 
}

void ArtifactProjectService::setPreviewQualityPreset(PreviewQualityPreset preset)
{
 impl_->setPreviewQualityPreset(preset);
 previewQualityPresetChanged(preset);
}

PreviewQualityPreset ArtifactProjectService::previewQualityPreset() const
{
 return impl_->previewQualityPreset();
}

};

//W_REGISTER_ARGTYPE(ArtifactCore::CompositionID)
