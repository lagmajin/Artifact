module;
#include <QDebug>
#include <memory>
#include <vector>
#include <typeindex>
#include <wobjectimpl.h>
#include <wobjectdefs.h>

#include <QHash>
#include <QVector>
#include <QtTest/QtTest>
//#include <QtCore/QString>


module Artifact.Project;

import Utils;
import Utils.String.Like;
import Utils.String.UniString;

import Composition.Settings;
import Container;
import Asset.File;

import Artifact.Composition.Abstract;
import Artifact.Composition._2D;
import Artifact.Composition.InitParams;

import Artifact.Layer.Factory;
import Artifact.Layer.InitParams;
import Artifact.Layer.Result;

import Artifact.Project.Items;

namespace Artifact {
 using namespace ArtifactCore;

 //W_REGISTER_ARGTYPE(Id)
 W_OBJECT_IMPL(ArtifactProject)

  ArtifactProjectSignalHelper::ArtifactProjectSignalHelper()
 {

 }

 ArtifactProjectSignalHelper::~ArtifactProjectSignalHelper()
 {

 }

 struct ArtifactProjectNode
 {

 };

 class ArtifactProject::Impl {
  private:
   ArtifactProjectSettings projectSettings_;
   ArtifactLayerFactory layerFactory_;
  
  
 public:
  Impl();
  ~Impl();
  void addAssetFromPath(const QString& string);
  CreateCompositionResult createComposition(const UniString& str);
  CreateCompositionResult createComposition(const ArtifactCompositionInitParams& settings);
  //CreateCompositionResult createComposition(const Composition)
 	
  void createCompositions(const QStringList& names);
  FindCompositionResult findComposition(const CompositionID& id);
  bool removeById(const CompositionID& id);
   void removeAllCompositions();

   // Layer management
   ArtifactLayerResult createLayerAndAddToComposition(const CompositionID& compositionId, ArtifactLayerInitParams& params);
   AppendLayerToCompositionResult addLayerToComposition(const CompositionID& compositionId, ArtifactAbstractLayerPtr layer);
  bool removeLayerFromComposition(const CompositionID& compositionId, const LayerID& layerId);

   QJsonObject toJson() const;
  AssetMultiIndexContainer assetContainer_;
  ArtifactCompositionMultiIndexContainer container_;
  std::vector<std::unique_ptr<ProjectItem>> ownedItems_; // owns all allocated items
 };


 ArtifactProject::Impl::Impl()
 {

 }

QVector<ProjectItem*> ArtifactProject::projectItems() const
{
 // Build list of top-level project items from owned items.
 QVector<ProjectItem*> roots;
 for (const auto& up : impl_->ownedItems_) {
   if (!up) continue;
   ProjectItem* p = up.get();
   if (p->parent == nullptr) roots.push_back(p);
 }
 return roots;
}

 ArtifactProject::Impl::~Impl()
 {

 }

 void ArtifactProject::Impl::addAssetFromPath(const QString& string)
 {
  auto asset = new AbstractAssetFile();

  //assetContainer_.addSafe(asset->assetID(),asset);
 }


 CreateCompositionResult ArtifactProject::Impl::createComposition(const UniString& str)
 {
  auto id = CompositionID();
 	
  ArtifactCompositionInitParams params;
 	
 	
  auto newComposition = new ArtifactComposition(id,params);

  CreateCompositionResult result;


  return result;
 }

CreateCompositionResult ArtifactProject::Impl::createComposition(const ArtifactCompositionInitParams& settings)
{
 auto id = CompositionID();
 // If this is a default/unnamed creation and we already have at least one
 // composition, avoid creating another implicit default composition. This
 // prevents duplicate creations when project creation triggers a default
 // creation and the caller also requests a named composition immediately.
 if (settings.compositionName().toQString().isEmpty() && !container_.all().isEmpty()) {
   CreateCompositionResult skipped;
   skipped.success = false;
   skipped.message.setQString("Default composition creation suppressed: composition(s) already exist");
   qDebug() << "Impl::createComposition: suppressed default creation (existing count):" << container_.all().size();
   return skipped;
 }

 // create a shared_ptr for the new composition and insert into the multi-index
 auto newCompPtr = std::make_shared<ArtifactAbstractComposition>(id, settings);
 container_.add(newCompPtr, id, std::type_index(typeid(ArtifactAbstractComposition)));

 CreateCompositionResult result;
 result.id = id;
 result.success = true;
 qDebug() << "Impl::createComposition: created composition id=" << id.toString();
 return result;
 }

void notifyProjectCompositionCreated(ArtifactProject* proj, const CompositionID& id) {
    // helper - emit signal
    proj->compositionCreated(id);
}

FindCompositionResult ArtifactProject::Impl::findComposition(const CompositionID& id)
{
 auto ptr=container_.findById(id);
 	
 FindCompositionResult result;
 result.success = true;
 result.ptr = ptr;

 return result;
}

FindCompositionResult ArtifactProject::findComposition(const CompositionID& id)
{
 	
 	
 return impl_->findComposition(id);
}

 void ArtifactProject::Impl::removeAllCompositions()
 {

 }

 bool ArtifactProject::Impl::removeById(const CompositionID& id)
 {


  return false;
 }

 QJsonObject ArtifactProject::Impl::toJson() const
 {
  QJsonObject result;
  result["name"] = projectSettings_.projectName();
  result["author"];
  result["version"] = "";
  auto allComposition = container_.all();



  return result;
 }
	
 ArtifactProject::ArtifactProject() :impl_(new Impl())
 {
  // create a root folder for project items and own it
  auto rootUp = std::make_unique<FolderItem>();
  rootUp->name.setQString("Project Root");
  ProjectItem* root = rootUp.get();
  impl_->ownedItems_.push_back(std::move(rootUp));

  Q_EMIT projectCreated();

 }

 ArtifactProject::ArtifactProject(const QString& name) :impl_(new Impl())
 {

  Q_EMIT projectCreated();
 }

 ArtifactProject::ArtifactProject(const ArtifactProjectSettings& setting) :impl_(new Impl())
 {
  Q_EMIT projectCreated();

 }

 ArtifactProject::~ArtifactProject()
 {
  delete impl_;
 }

 void ArtifactProject::createComposition(const QString& name)
 {
 // Create composition using the standard params API, then update the created item's name
 ArtifactCompositionInitParams params;
 CreateCompositionResult res = createComposition(params);
 if (!res.success) {
  qDebug() << "Failed to create composition with name:" << name;
  return;
 }
 // find created composition item under project root and set its name
 ProjectItem* projectRoot = nullptr;
 if (!impl_->ownedItems_.empty()) projectRoot = impl_->ownedItems_.front().get();
 if (projectRoot) {
   for (auto child : projectRoot->children) {
     if (!child) continue;
     if (child->type() == eProjectItemType::Composition) {
       CompositionItem* ci = static_cast<CompositionItem*>(child);
       if (ci->compositionId == res.id) {
         ci->name.setQString(name);
         qDebug() << "Composition created:" << name << "(ID:" << res.id.toString() << ")";
         return;
       }
     }
   }
 }
 // fallback log if item not found
 qDebug() << "Composition created (but item not found):" << name << "(ID:" << res.id.toString() << ")";
 }

 CreateCompositionResult ArtifactProject::createComposition(const ArtifactCompositionInitParams& param)
 {
 // If a name was provided and a default/unamed composition already exists,
 // prefer renaming that existing composition instead of creating a new one.
 QString requestedName = param.compositionName().toQString();
 if (!requestedName.isEmpty()) {
   ProjectItem* projectRoot = nullptr;
   if (!impl_->ownedItems_.empty()) projectRoot = impl_->ownedItems_.front().get();
   if (projectRoot) {
     for (auto child : projectRoot->children) {
       if (!child) continue;
       if (child->type() == eProjectItemType::Composition) {
         CompositionItem* ci = static_cast<CompositionItem*>(child);
         QString existingName = ci->name.toQString();
         if (existingName.isEmpty() || existingName == QStringLiteral("Composition")) {
           ci->name.setQString(requestedName);
           qDebug() << "Renamed existing composition to:" << requestedName << "(ID:" << ci->compositionId.toString() << ")";
           CreateCompositionResult result;
           result.success = true;
           result.id = ci->compositionId;
           // notify listeners that project data changed
           projectChanged();
           return result;
         }
       }
     }
   }
 }

 auto res = impl_->createComposition(param);
 if (res.success) {
  // avoid adding duplicate project items for the same composition id
  for (const auto& up : impl_->ownedItems_) {
    if (!up) continue;
    if (up->type() == eProjectItemType::Composition) {
      CompositionItem* existing = static_cast<CompositionItem*>(up.get());
      if (existing->compositionId == res.id) {
        // already have an item for this composition
        qDebug() << "Composition item for ID already exists, skipping add:" << res.id.toString();
        // notify listeners that project data changed
        projectChanged();
        return res;
      }
    }
  }

  // add composition item to project tree
  auto compItemUp = std::make_unique<CompositionItem>();
  compItemUp->compositionId = res.id;
  // set name for composition: prefer param-provided name, otherwise default
  QString createdName;
  try {
    createdName = param.compositionName().toQString();
  } catch (...) {
    createdName = QStringLiteral("Composition");
  }
  if (createdName.isEmpty()) createdName = QStringLiteral("Composition");
  compItemUp->name.setQString(createdName);
  // capture name before moving the unique_ptr
  QString capturedName = compItemUp->name.toQString();
   ProjectItem* raw = compItemUp.get();
   // set parent to project root and append to its children
   if (!impl_->ownedItems_.empty()) {
     ProjectItem* projectRoot = impl_->ownedItems_.front().get();
     compItemUp->parent = projectRoot;
     projectRoot->children.append(raw);
   }
   impl_->ownedItems_.push_back(std::move(compItemUp));
  // emit signal
  compositionCreated(res.id);
  // log using captured name (compItemUp is null after move)
  QString idStr = res.id.toString();
  qDebug() << "Composition created:" << capturedName << "(ID:" << idStr << ")";
 }
  // notify listeners that project data changed
  projectChanged();
 return res;
 }

 bool ArtifactProject::isNull() const
 {
  return false;
 }

 void ArtifactProject::addAssetFromPath(const QString& filepath)
 {
  impl_->addAssetFromPath(filepath);

 }


 bool ArtifactProject::removeCompositionById(const CompositionID& id)
 {
  return impl_->removeById(id);
 }

 void ArtifactProject::removeAllCompositions()
 {
  impl_->removeAllCompositions();
 }

 bool ArtifactProject::hasComposition(const CompositionID& id) const
 {
  return true;
 }

 void ArtifactProject::addAssetFile()
 {

 }
 bool ArtifactProject::isDirty() const
 {
  return false;
 }

  QJsonObject ArtifactProject::toJson() const
  {

   return  impl_->toJson();
  }

  ArtifactLayerResult ArtifactProject::Impl::createLayerAndAddToComposition(const CompositionID& compositionId, ArtifactLayerInitParams& params)
  {
   ArtifactLayerResult result;
   
   // Create layer using factory
   result = layerFactory_.createLayer(params);
   
   if (!result.success || !result.layer) {
    return result;
   }

   // Find the composition
   auto findResult = findComposition(compositionId);
   auto compositionPtr = findResult.ptr.lock();
   if (!findResult.success || !compositionPtr) {
    result.success = false;
    return result;
   }

   // Add layer to composition
   auto appendResult = compositionPtr->appendLayerTop(result.layer);
   result.success = appendResult.success;

   return result;
  }

  AppendLayerToCompositionResult ArtifactProject::Impl::addLayerToComposition(const CompositionID& compositionId, ArtifactAbstractLayerPtr layer)
  {
   AppendLayerToCompositionResult result;

   if (!layer) {
    result.success = false;
    result.error = AppendLayerToCompositionError::LayerNotFound;
    result.message = QString("Layer is null");
    return result;
   }

   // Find the composition
   auto findResult = findComposition(compositionId);
   auto compositionPtr = findResult.ptr.lock();
   if (!findResult.success || !compositionPtr) {
    result.success = false;
    result.error = AppendLayerToCompositionError::CompositionNotFound;
    result.message = QString("Composition not found");
    return result;
   }

   // Add layer to composition
   result = compositionPtr->appendLayerTop(layer);

   return result;
  }

  bool ArtifactProject::Impl::removeLayerFromComposition(const CompositionID& compositionId, const LayerID& layerId)
  {
    // find composition
    auto findResult = findComposition(compositionId);
    auto compPtr = findResult.ptr.lock();
    if (!findResult.success || !compPtr) return false;
    if (!compPtr->containsLayerById(layerId)) return false;
    compPtr->removeLayer(layerId);
    return true;
  }

  ArtifactLayerResult ArtifactProject::createLayerAndAddToComposition(const CompositionID& compositionId, ArtifactLayerInitParams& params)
  {
   return impl_->createLayerAndAddToComposition(compositionId, params);
  }

  AppendLayerToCompositionResult ArtifactProject::addLayerToComposition(const CompositionID& compositionId, ArtifactAbstractLayerPtr layer)
  {
   return impl_->addLayerToComposition(compositionId, layer);
  }

  bool ArtifactProject::removeLayerFromComposition(const CompositionID& compositionId, const LayerID& layerId)
  {
    bool ok = impl_->removeLayerFromComposition(compositionId, layerId);
    if (ok) layerRemoved(layerId);
    return ok;
  }




};