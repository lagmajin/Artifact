module;
#include <QDebug>
#include <memory>
#include <vector>
#include <typeindex>
#include <wobjectimpl.h>
#include <wobjectdefs.h>
#include <QFileInfo>

#include <QHash>
#include <QJsonArray>
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
   bool isDirty_; // ダーティ状態フラグ

   // AI向けメタデータ
   QString aiDescription_;
   QStringList aiTags_;
   QString aiNotes_;

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
  bool addImportedComposition(ArtifactCompositionPtr comp, const QString& name);
  void setProjectName(const QString& name);
  void setAuthor(const QString& author);

  // AI向けメタデータ
  void setAIDescription(const QString& description);
  QString aiDescription() const;
  void setAITags(const QStringList& tags);
  QStringList aiTags() const;
  void setAINotes(const QString& notes);
  QString aiNotes() const;

   // Layer management
   ArtifactLayerResult createLayerAndAddToComposition(const CompositionID& compositionId, ArtifactLayerInitParams& params);
   AppendLayerToCompositionResult addLayerToComposition(const CompositionID& compositionId, ArtifactAbstractLayerPtr layer);
   bool removeLayerFromComposition(const CompositionID& compositionId, const LayerID& layerId);
   ArtifactLayerResult duplicateLayerInComposition(const CompositionID& compositionId, const LayerID& layerId);
   CreateCompositionResult duplicateComposition(const CompositionID& compositionId);

   // ダーティ状態管理
   bool isDirty() const;
   void setDirty(bool dirty);

   QJsonObject toJson() const;
  AssetMultiIndexContainer assetContainer_;
  ArtifactCompositionMultiIndexContainer container_;
  std::vector<std::unique_ptr<ProjectItem>> ownedItems_; // owns all allocated items
 };


 ArtifactProject::Impl::Impl() : isDirty_(false)
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
  if (string.isEmpty()) return;

  // Create a FootageItem and add it under the project root
  auto fi = QFileInfo(string);
  auto footageUp = std::make_unique<FootageItem>();
  footageUp->filePath = string;
  footageUp->name.setQString(fi.fileName());

  // attach to project root if exists
  ProjectItem* projectRoot = nullptr;
  if (!ownedItems_.empty()) projectRoot = ownedItems_.front().get();
  if (!projectRoot) {
    // create a root placeholder if none
    auto rootUp = std::make_unique<FolderItem>();
    rootUp->name.setQString("Project Root");
    projectRoot = rootUp.get();
    ownedItems_.push_back(std::move(rootUp));
  }
  footageUp->parent = projectRoot;
  projectRoot->children.append(footageUp.get());

  // keep ownership
  ownedItems_.push_back(std::move(footageUp));

  // Optionally register into asset container if implemented
  // assetContainer_.addSafe(asset->assetID(), asset);

  // notify listeners - emit signal on the owning ArtifactProject instance
  // We are in Impl, so call the containing ArtifactProject's signal via a helper
  // Emit projectChanged from the public ArtifactProject that owns this Impl
  // (the public object will be the 'this' pointer's outer class; use outer emit helper)
  // For simplicity, we will call a free helper that emits the signal on a target project
  // Find the parent ArtifactProject instance: we cannot from Impl; instead rely on callers
  // to call projectChanged() after calling this method.

  // ダーティ状態に設定
  setDirty(true);
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

 // ダーティ状態に設定
 setDirty(true);

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
  auto ptr = container_.findById(id);
  FindCompositionResult result;
  if (ptr) {
    result.success = true;
    result.ptr = ptr;
  } else {
    result.success = false;
  }
  return result;
}

 void ArtifactProject::Impl::removeAllCompositions()
 {
  // Clear all compositions from container
  container_.clear();

  // Remove all composition items from project tree (while preserving root folder)
  auto it = ownedItems_.begin();
  while (it != ownedItems_.end()) {
    if (*it && (*it)->type() == eProjectItemType::Composition) {
      // Also remove from parent's children list
      CompositionItem* compItem = static_cast<CompositionItem*>(it->get());
      if (compItem->parent) {
        compItem->parent->children.removeOne(compItem);
      }
      it = ownedItems_.erase(it);
    } else {
      ++it;
    }
  }

  qDebug() << "removeAllCompositions succeeded";
 }

 bool ArtifactProject::Impl::addImportedComposition(ArtifactCompositionPtr comp, const QString& name)
 {
  if (!comp) return false;
  CompositionID id = comp->id();
  container_.add(comp, id, std::type_index(typeid(ArtifactAbstractComposition)));

  auto compItemUp = std::make_unique<CompositionItem>();
  compItemUp->compositionId = id;
  compItemUp->name.setQString(name.isEmpty() ? QStringLiteral("Composition") : name);
  ProjectItem* raw = compItemUp.get();
  if (!ownedItems_.empty()) {
   ProjectItem* projectRoot = ownedItems_.front().get();
   compItemUp->parent = projectRoot;
   projectRoot->children.append(raw);
  }
  ownedItems_.push_back(std::move(compItemUp));
  return true;
 }

 void ArtifactProject::Impl::setProjectName(const QString& name)
 {
  projectSettings_.setProjectName(name);
 }

 void ArtifactProject::Impl::setAuthor(const QString& author)
 {
  projectSettings_.setAuthor(author);
 }


  // bool ArtifactProject::Impl::removeById(const CompositionID& id) - TODO: container_.remove() not implemented
  // Commented out - use alternative removal method
  /*
  bool ArtifactProject::Impl::removeById(const CompositionID& id)
  {
   // Remove composition from container
   if (!container_.remove(id)) {
     qDebug() << "removeById failed: composition not in container";
     return false;
   }

   // Remove composition item from project tree
   auto it = std::remove_if(ownedItems_.begin(), ownedItems_.end(),
     [id](const std::unique_ptr<ProjectItem>& item) {
       if (!item || item->type() != eProjectItemType::Composition) {
         return false;
       }
       CompositionItem* compItem = static_cast<CompositionItem*>(item.get());
       return compItem->compositionId == id;
     });

   if (it != ownedItems_.end()) {
     // Also remove from parent's children list
     CompositionItem* removedItem = static_cast<CompositionItem*>(it->get());
     if (removedItem->parent) {
       removedItem->parent->children.removeOne(removedItem);
     }
     ownedItems_.erase(it);
   }

   qDebug() << "removeById succeeded: id=" << id.toString();
   return true;
  }
  */

 bool ArtifactProject::Impl::isDirty() const
 {
  return isDirty_;
 }

 void ArtifactProject::Impl::setDirty(bool dirty)
 {
  isDirty_ = dirty;
  // TODO: ダーティ状態が変更されたときの通知を実装する
 }

 void ArtifactProject::Impl::setAIDescription(const QString& description)
 {
  aiDescription_ = description;
  setDirty(true);
 }

 QString ArtifactProject::Impl::aiDescription() const
 {
  return aiDescription_;
 }

 void ArtifactProject::Impl::setAITags(const QStringList& tags)
 {
  aiTags_ = tags;
  setDirty(true);
 }

 QStringList ArtifactProject::Impl::aiTags() const
 {
  return aiTags_;
 }

 void ArtifactProject::Impl::setAINotes(const QString& notes)
 {
  aiNotes_ = notes;
  setDirty(true);
 }

 QString ArtifactProject::Impl::aiNotes() const
 {
  return aiNotes_;
 }

 static QString compositionNameFromItems(const std::vector<std::unique_ptr<ProjectItem>>& ownedItems, const CompositionID& id)
 {
  for (const auto& up : ownedItems) {
   if (!up || up->type() != eProjectItemType::Composition) continue;
   auto* ci = static_cast<CompositionItem*>(up.get());
   if (ci->compositionId == id) return ci->name.toQString();
  }
  return QStringLiteral("Composition");
 }

 QJsonObject ArtifactProject::Impl::toJson() const
 {
  QJsonObject result;
  result["name"] = projectSettings_.projectName();
  result["author"] = projectSettings_.author().toQString();
  result["version"] = "1.0";

  // AI向けメタデータを追加
  if (!aiDescription_.isEmpty()) {
   result["ai_description"] = aiDescription_;
  }
  if (!aiTags_.isEmpty()) {
   QJsonArray tagsArray;
   for (const auto& tag : aiTags_) {
    tagsArray.append(tag);
   }
   result["ai_tags"] = tagsArray;
  }
  if (!aiNotes_.isEmpty()) {
   result["ai_notes"] = aiNotes_;
  }

  QJsonArray compsArray;
  for (const auto& comp : container_.all()) {
   if (comp) {
    QJsonObject compObj = comp->toJson().object();
    compObj["name"] = compositionNameFromItems(ownedItems_, comp->id());
    compsArray.append(compObj);
   }
  }
  result["compositions"] = compsArray;
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

 bool ArtifactProject::addImportedComposition(ArtifactCompositionPtr comp, const QString& name)
 {
  if (!impl_->addImportedComposition(comp, name)) return false;
  compositionCreated(comp->id());
  projectChanged();
  return true;
 }

 void ArtifactProject::setProjectName(const QString& name)
 {
  impl_->setProjectName(name);
 }

 void ArtifactProject::setAuthor(const QString& author)
 {
  impl_->setAuthor(author);
 }

 void ArtifactProject::setAIDescription(const QString& description)
 {
  impl_->setAIDescription(description);
 }

 QString ArtifactProject::aiDescription() const
 {
  return impl_->aiDescription();
 }

 void ArtifactProject::setAITags(const QStringList& tags)
 {
  impl_->setAITags(tags);
 }

 QStringList ArtifactProject::aiTags() const
 {
  return impl_->aiTags();
 }

 void ArtifactProject::setAINotes(const QString& notes)
 {
  impl_->setAINotes(notes);
 }

 QString ArtifactProject::aiNotes() const
 {
  return impl_->aiNotes();
 }

 void ArtifactProject::removeAllCompositions()
 {
  impl_->removeAllCompositions();
 }

 bool ArtifactProject::hasComposition(const CompositionID& id) const
 {
  if (!impl_) return false;
  auto findResult = impl_->findComposition(id);
  return findResult.success && !findResult.ptr.expired();
 }

 void ArtifactProject::addAssetFile()
 {
  // Open file dialog would be handled by UI layer
  // For now, just log that this method was called
  qDebug() << "ArtifactProject::addAssetFile called - UI should handle file selection";
  
  // In a real implementation, this would:
  // 1. Open file dialog (handled by UI)
  // 2. Get selected file path
  // 3. Call addAssetFromPath(filePath)
 }
 bool ArtifactProject::isDirty() const
 {
  return impl_->isDirty();
 }

 // void ArtifactProject::setDirty(bool dirty) - TODO: impl_->setDirty not available
 // {
 //  impl_->setDirty(dirty);
 // }

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

   // ダーティ状態に設定
   setDirty(true);

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
    
    // ダーティ状態に設定
    setDirty(true);
    
    return true;
  }

  ArtifactLayerResult ArtifactProject::Impl::duplicateLayerInComposition(const CompositionID& compositionId, const LayerID& layerId)
  {
    ArtifactLayerResult result;
    
    // Find the composition
    auto findResult = findComposition(compositionId);
    auto compositionPtr = findResult.ptr.lock();
    if (!findResult.success || !compositionPtr) {
      result.success = false;
      return result;
    }
    
    // Find the layer to duplicate
    if (!compositionPtr->containsLayerById(layerId)) {
      result.success = false;
      return result;
    }
    
    // Get the layer to duplicate
    auto layerToDuplicate = compositionPtr->layerById(layerId);
    if (!layerToDuplicate) {
      result.success = false;
      return result;
    }
    
     // Create a copy of the layer
     // TODO: レイヤーの複製方法を実装する
     // 現在のレイヤーの種類に応じたコピーを作成する必要がある
     
     // ここでは簡単な例として、レイヤーのInitParamsを取得して新しいレイヤーを作成する方法を示す
     // TODO: ArtifactLayerInitParams has no default constructor
     // ArtifactLayerInitParams params;
     // TODO: レイヤーのプロパティをparamsにコピーする
     
     // 新しいレイヤーを作成して追加する
     // result = createLayerAndAddToComposition(compositionId, params);
     result.success = false;
     // result.message.setQString("Layer duplication not yet implemented");  // ArtifactLayerResult has no message field
     
     return result;
  }

  CreateCompositionResult ArtifactProject::Impl::duplicateComposition(const CompositionID& compositionId)
  {
    CreateCompositionResult result;
    
    // Find the composition to duplicate
    auto findResult = findComposition(compositionId);
    auto compositionPtr = findResult.ptr.lock();
    if (!findResult.success || !compositionPtr) {
      result.success = false;
      result.message.setQString("Composition not found");
      qDebug() << "Impl::duplicateComposition failed: composition not found";
      return result;
    }
    
    // Create a copy of the composition's settings
    ArtifactCompositionInitParams params;
    // TODO: コンポジションのプロパティをparamsにコピーする
    
    // Create a new composition with the copied settings
    auto newCompResult = createComposition(params);
    if (!newCompResult.success) {
      qDebug() << "Impl::duplicateComposition failed: could not create new composition";
      return newCompResult;
    }
    
    // Copy all layers from the original composition to the new one
    auto newCompPtr = findComposition(newCompResult.id).ptr.lock();
    if (!newCompPtr) {
      result.success = false;
      result.message.setQString("Could not find newly created composition");
      qDebug() << "Impl::duplicateComposition failed: could not find new composition";
      return result;
    }
    
    // TODO: レイヤーのコピー処理を実装する
    
    // Return the result
    result.success = true;
    result.id = newCompResult.id;
    result.message.setQString("Composition duplicated successfully");
    qDebug() << "Impl::duplicateComposition succeeded: new id=" << newCompResult.id.toString();
    
    return result;
  }

  ArtifactLayerResult ArtifactProject::createLayerAndAddToComposition(const CompositionID& compositionId, ArtifactLayerInitParams& params)
  {
   return impl_->createLayerAndAddToComposition(compositionId, params);
  }


   // ArtifactLayerResult ArtifactProject::duplicateLayerInComposition - TODO: impl_->duplicateLayerInComposition not available
   // {
   //   return impl_->duplicateLayerInComposition(compositionId, layerId);
   // }


   // CreateCompositionResult ArtifactProject::duplicateComposition - TODO: impl_->duplicateComposition not available
   // {
   //   auto result = impl_->duplicateComposition(compositionId);
   //   if (result.success) {
   //     // compositionCreated(result.id);  // Signal not available
   //     // projectChanged();                // Signal not available
   //   }
   //   return result;
   // }

  FindCompositionResult ArtifactProject::findComposition(const CompositionID& id)
  {
   return impl_->findComposition(id);
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