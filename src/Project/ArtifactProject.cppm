module;
#include <utility>
#include <QDebug>
#include <wobjectimpl.h>
#include <wobjectdefs.h>
#include <QFileInfo>
#include <QDateTime>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QVector>
#include <QDir>
#include <QSet>
#include <QtTest/QtTest>
module Artifact.Project;

//#include <QtCore/QString>

import std;

import Artifact.Event.Types;
import Event.Bus;
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
import Artifact.Layer.Svg;
import Artifact.Project.CreationDefaults;
import Application.AppSettings;

import Artifact.Project.Items;

namespace Artifact {
 using namespace ArtifactCore;

 namespace {
  inline void publishProjectChangedEvent()
  {
    ArtifactCore::globalEventBus().publish<ProjectChangedEvent>(ProjectChangedEvent{QString(), QString()});
  }

  inline void publishCompositionCreatedEvent(const CompositionID& id)
  {
    ArtifactCore::globalEventBus().publish<CompositionCreatedEvent>(
        CompositionCreatedEvent{id.toString(), QString()});
  }

  inline void publishLayerChangedEvent(const CompositionID& compId, const LayerID& layerId,
                                       LayerChangedEvent::ChangeType changeType)
  {
    ArtifactCore::globalEventBus().publish<LayerChangedEvent>(LayerChangedEvent{
        compId.toString(), layerId.toString(), changeType});
  }
 } // namespace

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
   CompositionID currentCompositionId_;

   // AI向けメタデータ
   QString aiDescription_;
   QStringList aiTags_;
   QString aiNotes_;
   CreationDefaultsState creationDefaultsState_;
   GuideSet guideSet_;
   QJsonObject extensionData_;

 public:
  Impl();
  ~Impl();
  void addAssetFromPath(const QString& string);
  void addAssetFromPath(const QString& string, const QStringList& sequencePaths, double frameRate);
  CreateCompositionResult createComposition(const UniString& str);
  CreateCompositionResult createComposition(const ArtifactCompositionInitParams& settings);
  //CreateCompositionResult createComposition(const Composition)

   void createCompositions(const QStringList& names);
   FindCompositionResult findComposition(const CompositionID& id);
   bool removeById(CompositionID id);
   void removeAllCompositions();
  bool addImportedComposition(ArtifactCompositionPtr comp, const QString& name);
  bool addProjectItemsFromJson(const QJsonArray& items, ProjectItem* parent = nullptr);
   void setProjectName(const QString& name);
   void setAuthor(const QString& author);
  ArtifactProjectSettings projectSettings() const { return projectSettings_; }
  GuideSet guideSet() const { return guideSet_; }
  void setGuideSet(const GuideSet& guideSet) { guideSet_ = guideSet; }

  // AI向けメタデータ
  void setAIDescription(const QString& description);
  QString aiDescription() const;
  void setAITags(const QStringList& tags);
  QStringList aiTags() const;
  void setAINotes(const QString& notes);
  QString aiNotes() const;

  // 拡張データ(JSON dict)。コマンドパレット MRU 等、周辺機能の
  // 保存受け口。既存サービス・メニュー経路には接続しない。
  QJsonObject extensionData() const { return extensionData_; }
  void setExtensionData(const QJsonObject& data) { extensionData_ = data; }

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
   CreationDefaultsState creationDefaultsState() const { return creationDefaultsState_; }
   void setCreationDefaultsState(const CreationDefaultsState& state) { creationDefaultsState_ = state; }
   CompositionID currentCompositionId() const;
   void setCurrentCompositionId(const CompositionID& id, bool markDirty = true);
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
  addAssetFromPath(string, {}, 0.0);
 }

 void ArtifactProject::Impl::addAssetFromPath(const QString& string, const QStringList& sequencePaths, double frameRate)
 {
  if (string.isEmpty()) return;
  QFileInfo fiInput(string);
  QString canonicalPath = fiInput.canonicalFilePath();
  if (canonicalPath.isEmpty()) {
    canonicalPath = fiInput.absoluteFilePath();
  }
  if (canonicalPath.isEmpty()) return;

  // Avoid duplicate footage entries by canonical path.
  for (const auto& up : ownedItems_) {
    if (!up || up->type() != eProjectItemType::Footage) continue;
    auto* existing = static_cast<FootageItem*>(up.get());
    QFileInfo exInfo(existing->filePath);
    QString exCanonical = exInfo.canonicalFilePath();
    if (exCanonical.isEmpty()) exCanonical = exInfo.absoluteFilePath();
    if (!exCanonical.isEmpty() && exCanonical.compare(canonicalPath, Qt::CaseInsensitive) == 0) {
      setDirty(true);
      return;
    }
  }

  // Create a FootageItem and add it under the project root
  auto fi = QFileInfo(canonicalPath);
  auto footageUp = std::make_unique<FootageItem>();
  footageUp->filePath = canonicalPath;
  footageUp->name.setQString(fi.fileName());
  footageUp->sequencePaths = sequencePaths;
  footageUp->frameRate = frameRate > 0.0 ? frameRate : 0.0;
  footageUp->isSequence = sequencePaths.size() > 1;

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
  // publish ProjectChangedEvent after calling this method.

  // Mark project state as dirty
  setDirty(true);
 }


 CreateCompositionResult ArtifactProject::Impl::createComposition(const UniString& str)
 {
  ArtifactCompositionInitParams params;
  params.setCompositionName(str);
  return createComposition(params);
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

 // Mark project state as dirty
 setDirty(true);

 CreateCompositionResult result;
 result.id = id;
 result.success = true;
 currentCompositionId_ = id;
 qDebug() << "Impl::createComposition: created composition id=" << id.toString();
 return result;
 }

void ArtifactProject::Impl::createCompositions(const QStringList& names) {}

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
  if (currentCompositionId_.isNil()) {
    currentCompositionId_ = id;
  }
  return true;
 }

 bool ArtifactProject::Impl::addProjectItemsFromJson(const QJsonArray& items, ProjectItem* parent)
 {
  if (ownedItems_.empty()) {
   return false;
  }

  ProjectItem* targetParent = parent ? parent : ownedItems_.front().get();
  if (!targetParent || targetParent->type() != eProjectItemType::Folder) {
   return false;
  }

  bool changed = false;

  std::function<bool(const QJsonObject&, ProjectItem*)> appendItem =
      [&](const QJsonObject& obj, ProjectItem* currentParent) -> bool {
    if (!currentParent || currentParent->type() != eProjectItemType::Folder) {
      return false;
    }

    const QString type = obj.value(QStringLiteral("type")).toString();
    const QString name = obj.value(QStringLiteral("name")).toString();
    const QString idStr = obj.value(QStringLiteral("id")).toString();

    auto appendChild = [&](auto&& uniquePtr) -> bool {
      uniquePtr->parent = currentParent;
      currentParent->children.append(uniquePtr.get());
      ownedItems_.push_back(std::move(uniquePtr));
      changed = true;
      return true;
    };

    if (type == QStringLiteral("folder")) {
      auto folderUp = std::make_unique<FolderItem>();
      folderUp->name.setQString(name);
      if (!idStr.isEmpty()) {
        folderUp->id = Id(idStr);
      }
      auto* rawFolder = folderUp.get();
      if (!appendChild(std::move(folderUp))) {
        return false;
      }

      const QJsonArray children = obj.value(QStringLiteral("children")).toArray();
      for (const auto& childVal : children) {
        if (!childVal.isObject()) {
          continue;
        }
        appendItem(childVal.toObject(), rawFolder);
      }
      return true;
    }

    if (type == QStringLiteral("footage")) {
      auto footageUp = std::make_unique<FootageItem>();
      footageUp->name.setQString(name);
      if (!idStr.isEmpty()) {
        footageUp->id = Id(idStr);
      }
      footageUp->filePath = obj.value(QStringLiteral("filePath")).toString();
      footageUp->isSequence = obj.value(QStringLiteral("isSequence")).toBool(false);
      footageUp->frameRate = obj.value(QStringLiteral("frameRate")).toDouble(0.0);
      const QJsonArray sequenceArray = obj.value(QStringLiteral("sequencePaths")).toArray();
      if (!sequenceArray.isEmpty()) {
        QStringList sequencePaths;
        sequencePaths.reserve(sequenceArray.size());
        for (const auto& value : sequenceArray) {
          if (value.isString()) {
            sequencePaths.append(value.toString());
          }
        }
        footageUp->sequencePaths = sequencePaths;
        if (sequencePaths.size() > 1) {
          footageUp->isSequence = true;
        }
      }
      return appendChild(std::move(footageUp));
    }

    if (type == QStringLiteral("solid")) {
      auto solidUp = std::make_unique<SolidItem>();
      solidUp->name.setQString(name);
      if (!idStr.isEmpty()) {
        solidUp->id = Id(idStr);
      }
      const QString colorStr = obj.value(QStringLiteral("color")).toString();
      if (!colorStr.isEmpty()) {
        solidUp->color = QColor(colorStr);
      }
      return appendChild(std::move(solidUp));
    }

    if (type == QStringLiteral("composition")) {
      ArtifactCompositionPtr comp;
      const QJsonObject compJsonObj = obj.value(QStringLiteral("compositionJson")).toObject();
      if (!compJsonObj.isEmpty()) {
        comp = ArtifactAbstractComposition::fromJson(QJsonDocument(compJsonObj));
      }
      if (!comp) {
        // Fallback to a placeholder composition item if the payload does not
        // contain a full composition snapshot.
        auto compItemUp = std::make_unique<CompositionItem>();
        compItemUp->name.setQString(name.isEmpty() ? QStringLiteral("Composition") : name);
        if (!idStr.isEmpty()) {
          compItemUp->id = Id(idStr);
        }
        const QString compIdStr = obj.value(QStringLiteral("compositionId")).toString();
        if (!compIdStr.isEmpty()) {
          compItemUp->compositionId = CompositionID(compIdStr);
        }
        return appendChild(std::move(compItemUp));
      }

      const QString importedName = name.isEmpty()
                                       ? QStringLiteral("Composition")
                                       : name;
      const CompositionID compId = comp->id();
      container_.add(comp, compId, std::type_index(typeid(ArtifactAbstractComposition)));

      auto compItemUp = std::make_unique<CompositionItem>();
      compItemUp->compositionId = compId;
      compItemUp->name.setQString(importedName);
      if (!idStr.isEmpty()) {
        compItemUp->id = Id(idStr);
      }
      compItemUp->parent = currentParent;
      currentParent->children.append(compItemUp.get());
      ownedItems_.push_back(std::move(compItemUp));
      changed = true;
      return true;
    }

    return false;
  };

  for (const auto& val : items) {
    if (!val.isObject()) {
      continue;
    }
    appendItem(val.toObject(), targetParent);
  }

  if (changed) {
    setDirty(true);
  }
  return changed;
 }

 void ArtifactProject::Impl::setProjectName(const QString& name)
 {
  projectSettings_.setProjectName(name);
 }

 void ArtifactProject::Impl::setAuthor(const QString& author)
 {
  projectSettings_.setAuthor(author);
 }


  bool ArtifactProject::Impl::removeById(CompositionID id)
  {
   // Check existence
   if (!container_.containsId(id)) {
     qDebug() << "removeById failed: composition not in container";
     return false;
   }

   // Remove from container
   container_.removeById(id);

   // Remove composition item from project tree
   for (auto it = ownedItems_.begin(); it != ownedItems_.end(); ) {
     if (!(*it) || (*it)->type() != eProjectItemType::Composition) { ++it; continue; }
     CompositionItem* compItem = static_cast<CompositionItem*>((*it).get());
     if (compItem->compositionId == id) {
       if (compItem->parent) {
         compItem->parent->children.removeOne(compItem);
       }
       it = ownedItems_.erase(it);
       qDebug() << "removeById succeeded: id=" << id.toString();
       if (currentCompositionId_ == id) {
         currentCompositionId_ = CompositionID();
       }
       setDirty(true);
       return true;
     } else {
       ++it;
     }
   }

  qDebug() << "removeById: composition removed from container but item not found in ownedItems: id=" << id.toString();
   if (currentCompositionId_ == id) {
     currentCompositionId_ = CompositionID();
   }
  setDirty(true);
  return true;
  }

 bool ArtifactProject::Impl::isDirty() const
 {
  return isDirty_;
 }

 void ArtifactProject::Impl::setDirty(bool dirty)
 {
  if (isDirty_ != dirty) {
   isDirty_ = dirty;
  }
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

 static bool setTreeError(QString* errorMessage, const QString& message)
 {
  if (errorMessage) {
   *errorMessage = message;
  }
  return false;
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

 static LayerType inferLayerTypeFromRuntimeName(const ArtifactAbstractLayerPtr& layer)
 {
  if (!layer) {
   return LayerType::Unknown;
  }

  std::string typeName = typeid(*layer).name();
  std::transform(typeName.begin(), typeName.end(), typeName.begin(), [](unsigned char c) {
   return static_cast<char>(std::tolower(c));
  });

  if (typeName.find("null") != std::string::npos) return LayerType::Null;
  if (typeName.find("solid") != std::string::npos) return LayerType::Solid;
  if (typeName.find("image") != std::string::npos) return LayerType::Image;
  if (typeName.find("shape") != std::string::npos) return LayerType::Shape;
  if (typeName.find("svg") != std::string::npos) return LayerType::Shape;
  if (typeName.find("formparticle") != std::string::npos) return LayerType::FormParticle;
  if (typeName.find("procedural3d") != std::string::npos) return LayerType::Procedural3D;
  if (typeName.find("particle") != std::string::npos) return LayerType::Particle;
  if (typeName.find("adjust") != std::string::npos) return LayerType::Adjustment;
  if (typeName.find("text") != std::string::npos) return LayerType::Text;
  if (typeName.find("camera") != std::string::npos) return LayerType::Camera;
  if (typeName.find("audio") != std::string::npos) return LayerType::Audio;
  if (typeName.find("video") != std::string::npos) return LayerType::Video;
  if (typeName.find("construction") != std::string::npos) return LayerType::Construction;
  if (typeName.find("compositionbackground") != std::string::npos) return LayerType::CompositionBackground;
  if (typeName.find("backgroundlayer") != std::string::npos) return LayerType::CompositionBackground;
  if (typeName.find("3dlayer") != std::string::npos) return LayerType::Model3D;
  if (typeName.find("model3d") != std::string::npos) return LayerType::Model3D;
  if (typeName.find("3dmodel") != std::string::npos) return LayerType::Model3D;

  return LayerType::Solid;
 }

 static void copyLayerProperties(const ArtifactAbstractLayerPtr& sourceLayer, const ArtifactAbstractLayerPtr& duplicatedLayer, const QString& duplicatedName)
 {
  if (!sourceLayer || !duplicatedLayer) {
   return;
  }

  for (const auto& group : sourceLayer->getLayerPropertyGroups()) {
   for (const auto& property : group.allProperties()) {
    if (!property) {
     continue;
    }

    const QString propertyName = property->getName();
    if (propertyName == QStringLiteral("layer.name")) {
     continue;
    }

    QVariant value = property->getValue();
    if (property->getType() == ArtifactCore::PropertyType::Color) {
      value = property->getColorValue();
    }

    duplicatedLayer->setLayerPropertyValue(propertyName, value);
   }
  }

  duplicatedLayer->setLayerName(duplicatedName);
  duplicatedLayer->setBlendMode(sourceLayer->layerBlendType());
  if (sourceLayer->hasParent()) {
   duplicatedLayer->setParentById(sourceLayer->parentLayerId());
  } else {
   duplicatedLayer->clearParent();
  }
 }

	 QJsonObject ArtifactProject::Impl::toJson() const
	 {
  QJsonObject result;
  result["name"] = projectSettings_.projectName();
  result["author"] = projectSettings_.author().toQString();
  result["version"] = "1.1";
  result["minVersion"] = "1.0";
  result["savedAt"] = QDateTime::currentDateTime().toString(Qt::ISODate);
  if (!currentCompositionId_.isNil()) {
   result["currentCompositionId"] = currentCompositionId_.toString();
  }

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
  // 拡張データ(コマンドパレット MRU 等)
  if (!extensionData_.isEmpty()) {
   result["extension_data"] = extensionData_;
  }
  result["creationDefaults"] = creationDefaultsState_.toJson();
  result["guideSet"] = guideSet_.toJson();

QJsonArray compsArray;
   for (const auto& comp : container_.all()) {
    if (comp) {
     QJsonObject compObj = comp->toJson().object();
     compObj["name"] = compositionNameFromItems(ownedItems_, comp->id());
     compsArray.append(compObj);
    }
   }
   result["compositions"] = compsArray;

   // Project items (Footage, Folder, Solid) を再帰的に保存
   std::function<QJsonObject(const ProjectItem*)> projectItemToJson = [&](const ProjectItem* item) -> QJsonObject {
     QJsonObject obj;
     if (!item) return obj;
     obj["name"] = item->name.toQString();
     obj["id"] = item->id.toString();
     switch (item->type()) {
      case eProjectItemType::Folder: {
       obj["type"] = "folder";
       QJsonArray children;
       for (const auto* child : item->children) {
         children.append(projectItemToJson(child));
       }
       obj["children"] = children;
       break;
      }
      case eProjectItemType::Footage: {
       obj["type"] = "footage";
       const auto* footage = static_cast<const FootageItem*>(item);
       obj["filePath"] = footage->filePath;
       obj["filePathExists"] = QFileInfo(footage->filePath).exists();
       if (footage->isSequence) {
        obj["isSequence"] = true;
       }
       if (!footage->sequencePaths.isEmpty()) {
        QJsonArray sequenceArray;
        for (const QString& sequencePath : footage->sequencePaths) {
         sequenceArray.append(sequencePath);
        }
        obj["sequencePaths"] = sequenceArray;
       }
       if (footage->frameRate > 0.0) {
        obj["frameRate"] = footage->frameRate;
       }
       break;
      }
      case eProjectItemType::Solid: {
       obj["type"] = "solid";
       const auto* solid = static_cast<const SolidItem*>(item);
       obj["color"] = solid->color.name(QColor::HexArgb);
       break;
      }
      case eProjectItemType::Composition: {
       obj["type"] = "composition";
       const auto* compItem = static_cast<const CompositionItem*>(item);
       obj["compositionId"] = compItem->compositionId.toString();
       break;
      }
      default:
       obj["type"] = "unknown";
       break;
     }
     return obj;
   };

   // Skip the root "Project Root" folder and save its children directly
   QJsonArray projectItemsArray;
   if (!ownedItems_.empty()) {
     const ProjectItem* root = ownedItems_.front().get();
     if (root && root->type() == eProjectItemType::Folder) {
       for (const auto* child : root->children) {
         projectItemsArray.append(projectItemToJson(child));
       }
     }
   }
   result["projectItems"] = projectItemsArray;

   return result;
  }

 CompositionID ArtifactProject::Impl::currentCompositionId() const
 {
  return currentCompositionId_;
 }

 void ArtifactProject::Impl::setCurrentCompositionId(const CompositionID& id, bool markDirty)
 {
  currentCompositionId_ = id;
  if (markDirty) {
   setDirty(true);
  }
 }

 bool ArtifactProject::validateProjectTree(QString* errorMessage) const
 {
  if (!impl_) {
   return setTreeError(errorMessage, QStringLiteral("Project impl is null."));
  }
  if (impl_->ownedItems_.empty()) {
   return setTreeError(errorMessage, QStringLiteral("Owned item list is empty."));
  }

  ProjectItem* const root = impl_->ownedItems_.front().get();
  if (!root) {
   return setTreeError(errorMessage, QStringLiteral("Project root item is null."));
  }
  if (root->type() != eProjectItemType::Folder) {
   return setTreeError(errorMessage, QStringLiteral("Project root item must be a folder."));
  }
  if (root->parent != nullptr) {
   return setTreeError(errorMessage, QStringLiteral("Project root parent must be null."));
  }

  QSet<const ProjectItem*> ownedSet;
  for (const auto& up : impl_->ownedItems_) {
   if (!up) {
    return setTreeError(errorMessage, QStringLiteral("Owned item contains null pointer."));
   }
   const ProjectItem* raw = up.get();
   if (ownedSet.contains(raw)) {
    return setTreeError(errorMessage, QStringLiteral("Duplicate project item pointer detected."));
   }
   ownedSet.insert(raw);
  }

  QSet<const ProjectItem*> activePath;
  QSet<const ProjectItem*> visited;
  std::function<bool(const ProjectItem*)> walk = [&](const ProjectItem* node) -> bool {
   if (!node) {
    return setTreeError(errorMessage, QStringLiteral("Encountered null node during walk."));
   }
   if (!ownedSet.contains(node)) {
    return setTreeError(errorMessage, QStringLiteral("Encountered node not owned by project."));
   }
   if (activePath.contains(node)) {
    return setTreeError(errorMessage, QStringLiteral("Cycle detected in project tree."));
   }
   if (visited.contains(node)) {
    return true;
   }

   activePath.insert(node);
   visited.insert(node);
   for (const auto* child : node->children) {
    if (!child) {
     return setTreeError(errorMessage, QStringLiteral("Child pointer is null."));
    }
    if (child->parent != node) {
      return setTreeError(errorMessage, QStringLiteral("Child parent pointer mismatch."));
    }
    if (!walk(child)) {
     return false;
    }
   }
   activePath.remove(node);
   return true;
  };

  if (!walk(root)) {
   return false;
  }

  for (const auto& up : impl_->ownedItems_) {
   const ProjectItem* node = up.get();
   if (!visited.contains(node)) {
    return setTreeError(errorMessage, QStringLiteral("Found unreachable project item from root."));
   }
   if (node != root && node->parent == nullptr) {
    return setTreeError(errorMessage, QStringLiteral("Non-root item has null parent."));
   }
  }

  if (errorMessage) {
    errorMessage->clear();
      }
      return true;
    }

 std::vector<ProjectValidationIssue> ArtifactProject::validate() const
 {
  std::vector<ProjectValidationIssue> issues;

  // Settings validation
  const auto settingsIssues = settings().validate();
  issues.insert(issues.end(), settingsIssues.begin(), settingsIssues.end());

  // Composition validation
  for (const auto& item : impl_->ownedItems_) {
    if (!item) continue;
    std::function<void(const ProjectItem*)> walk = [&](const ProjectItem* node) {
      if (!node) return;
      if (node->type() == eProjectItemType::Composition) {
        const auto* compItem = static_cast<const CompositionItem*>(node);
        auto findResult = impl_->findComposition(compItem->compositionId);
        if (findResult.success) {
          if (auto comp = findResult.ptr.lock()) {
            const QString compName = compItem->name.toQString().trimmed();
            if (compName.isEmpty()) {
              issues.push_back({
                ProjectValidationIssue::Severity::Warning,
                "composition.name",
                "コンポジション名が空です",
                "コンポジションに名前を設定してください"
              });
            }

            if (comp->layerCount() == 0) {
              issues.push_back({
                ProjectValidationIssue::Severity::Info,
                "composition.layers",
                QString("コンポジション '%1' にレイヤーがありません").arg(compName),
                "レイヤーを追加してください"
              });
            }

            const auto layers = comp->allLayer();
            for (int i = 0; i < layers.size(); ++i) {
              const auto& layer = layers[i];
              if (!layer) continue;
              const QString layerName = layer->layerName().trimmed();
              if (layerName.isEmpty()) {
                issues.push_back({
                  ProjectValidationIssue::Severity::Info,
                  "layer.name",
                  QString("コンポジション '%1' 内のレイヤー名が空です").arg(compName),
                  "レイヤーに名前を設定してください"
                });
              }
            }
          }
        }
      }
      for (const auto* child : node->children) {
        walk(child);
      }
    };
    walk(item.get());
  }

  // Footage validation - check for missing files
  for (const auto& item : impl_->ownedItems_) {
    if (!item) continue;
    std::function<void(const ProjectItem*)> walkFootage = [&](const ProjectItem* node) {
      if (!node) return;
      if (node->type() == eProjectItemType::Footage) {
        const auto* footage = static_cast<const FootageItem*>(node);
        if (!footage->filePath.isEmpty()) {
          QFileInfo fi(footage->filePath);
          if (!fi.exists()) {
            issues.push_back({
              ProjectValidationIssue::Severity::Error,
              "footage.missing",
              QString("ファイルが見つかりません: %1").arg(footage->name.toQString()),
              QString("参照先: %1").arg(footage->filePath)
            });
          }
        }
      }
      for (const auto* child : node->children) {
        walkFootage(child);
      }
    };
    walkFootage(item.get());
  }

  return issues;
 }
		
ArtifactProject::ArtifactProject() :impl_(new Impl())
{
  // Always keep a stable project-root placeholder at index 0.
  auto rootUp = std::make_unique<FolderItem>();
  rootUp->name.setQString("Project Root");
  impl_->ownedItems_.push_back(std::move(rootUp));

 }

 ArtifactProject::ArtifactProject(const QString& name) :impl_(new Impl())
 {
  auto rootUp = std::make_unique<FolderItem>();
  rootUp->name.setQString("Project Root");
  impl_->ownedItems_.push_back(std::move(rootUp));
  impl_->setProjectName(name);

 }

 ArtifactProject::ArtifactProject(const ArtifactProjectSettings& setting) :impl_(new Impl())
 {
  auto rootUp = std::make_unique<FolderItem>();
  rootUp->name.setQString("Project Root");
  impl_->ownedItems_.push_back(std::move(rootUp));
  impl_->setProjectName(setting.projectName());
  impl_->setAuthor(setting.author().toQString());

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
          publishProjectChangedEvent();
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
        publishProjectChangedEvent();
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
  publishCompositionCreatedEvent(res.id);
  // log using captured name (compItemUp is null after move)
  QString idStr = res.id.toString();
  qDebug() << "Composition created:" << capturedName << "(ID:" << idStr << ")";
 }
  publishProjectChangedEvent();
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

 void ArtifactProject::addAssetFromPath(const QString& filepath, const QStringList& sequencePaths, double frameRate)
 {
  if (!impl_) {
   return;
  }
  impl_->addAssetFromPath(filepath, sequencePaths, frameRate);
 }


  bool ArtifactProject::removeCompositionById(const CompositionID& id)
  {
   return impl_->removeById(id);
  }

  void ArtifactProject::createFolder(const QString& name)
  {
    createFolder(name, nullptr);
  }

  void ArtifactProject::createFolder(const QString& name, FolderItem* parentFolder)
  {
    auto folderUp = std::make_unique<FolderItem>();
    folderUp->name.setQString(name);

    ProjectItem* projectRoot = nullptr;
    if (!impl_->ownedItems_.empty()) {
      projectRoot = impl_->ownedItems_.front().get();
    }

    ProjectItem* targetParent = parentFolder ? static_cast<ProjectItem*>(parentFolder) : projectRoot;
    if (targetParent) {
        folderUp->parent = targetParent;
        targetParent->children.append(folderUp.get());
    }
    impl_->ownedItems_.push_back(std::move(folderUp));
    publishProjectChangedEvent();
  }

  bool ArtifactProject::moveItem(ProjectItem* item, ProjectItem* newParent)
  {
    if (!item || !newParent) {
      return false;
    }
    if (impl_->ownedItems_.empty()) {
      return false;
    }

    ProjectItem* const projectRoot = impl_->ownedItems_.front().get();
    if (!projectRoot) {
      return false;
    }
    // Keep root placeholder fixed.
    if (item == projectRoot) {
      return false;
    }
    if (newParent->type() != eProjectItemType::Folder) {
      return false;
    }
    if (item == newParent) {
      return false;
    }

    // Avoid folder cycles (moving a folder under its own descendant).
    for (ProjectItem* p = newParent; p; p = p->parent) {
      if (p == item) {
        return false;
      }
    }

    if (item->parent == newParent) {
      return true;
    }

    if (item->parent) {
      item->parent->children.removeOne(item);
    }
    item->parent = newParent;
    if (!newParent->children.contains(item)) {
      newParent->children.append(item);
    }

    impl_->setDirty(true);
    publishProjectChangedEvent();
    return true;
  }

  bool ArtifactProject::removeItem(ProjectItem* item)
  {
    if (!item) return false;
    
    if (item->type() == eProjectItemType::Composition) {
        CompositionItem* ci = static_cast<CompositionItem*>(item);
        impl_->container_.removeById(ci->compositionId);
    }
    
    while (!item->children.isEmpty()) {
        removeItem(item->children.back());
    }
    
    if (item->parent) {
        item->parent->children.removeOne(item);
    }
    
    auto it = std::find_if(impl_->ownedItems_.begin(), impl_->ownedItems_.end(), 
        [item](const auto& up) { return up.get() == item; });
    
    if (it != impl_->ownedItems_.end()) {
        impl_->ownedItems_.erase(it);
        publishProjectChangedEvent();
        return true;
    }
    return false;
  }

 void ArtifactProject::setDirty(bool dirty)
 {
  if (!impl_) {
   return;
  }
  const bool before = impl_->isDirty();
  impl_->setDirty(dirty);
  if (before != dirty) {
   Q_EMIT projectDirtyChanged(dirty);
  }
 }

 bool ArtifactProject::addImportedComposition(ArtifactCompositionPtr comp, const QString& name)
 {
 if (!impl_->addImportedComposition(comp, name)) return false;
  publishCompositionCreatedEvent(comp->id());
    publishProjectChangedEvent();
 return true;
 }

 void ArtifactProject::setProjectName(const QString& name)
 {
  impl_->setProjectName(name);
 }

ArtifactProjectSettings ArtifactProject::settings() const
{
  return impl_->projectSettings();
}

CreationDefaultsState ArtifactProject::creationDefaultsState() const
{
  return impl_->creationDefaultsState();
}

void ArtifactProject::setCreationDefaultsState(const CreationDefaultsState& state)
{
  impl_->setCreationDefaultsState(state);
}

GuideSet ArtifactProject::guideSet() const
{
  return impl_->guideSet();
}

void ArtifactProject::setGuideSet(const GuideSet& guideSet)
{
  impl_->setGuideSet(guideSet);
  impl_->setDirty(true);
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

QJsonObject ArtifactProject::extensionData() const
{
  return impl_ ? impl_->extensionData() : QJsonObject();
}

void ArtifactProject::setExtensionData(const QJsonObject& data)
{
  if (impl_) {
    impl_->setExtensionData(data);
  }
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

 CompositionID ArtifactProject::currentCompositionId() const
 {
  if (!impl_) {
   return CompositionID();
  }
  return impl_->currentCompositionId();
 }

 void ArtifactProject::setCurrentCompositionId(const CompositionID& id, bool markDirty)
 {
  if (!impl_) {
   return;
  }
  impl_->setCurrentCompositionId(id, markDirty);
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

	  QJsonObject ArtifactProject::toJson() const
	  {
   QString treeError;
   if (!validateProjectTree(&treeError)) {
    qWarning() << "[ArtifactProject::toJson] Project tree integrity check failed:" << treeError;
   }
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
    
    QString baseName = layerToDuplicate->layerName();
    if (baseName.isEmpty()) {
      baseName = QStringLiteral("Layer");
    }
    if (auto svgLayer = std::dynamic_pointer_cast<ArtifactSvgLayer>(layerToDuplicate)) {
      ArtifactSvgInitParams svgParams(baseName + QStringLiteral(" Copy"));
      svgParams.setSvgPath(svgLayer->sourcePath());
      result = createLayerAndAddToComposition(compositionId, svgParams);
    } else {
      LayerType inferredType = inferLayerTypeFromRuntimeName(layerToDuplicate);
      if (inferredType == LayerType::Unknown || inferredType == LayerType::None) {
        inferredType = LayerType::Solid;
      }
      ArtifactLayerInitParams params(baseName + QStringLiteral(" Copy"), inferredType);
      result = createLayerAndAddToComposition(compositionId, params);
    }
    if (result.success && result.layer) {
      copyLayerProperties(layerToDuplicate, result.layer, baseName + QStringLiteral(" Copy"));
      setDirty(true);
    }
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
    
    ArtifactCompositionInitParams params;
    const QString sourceName = compositionNameFromItems(ownedItems_, compositionId);
    UniString copyName;
    copyName.setQString(sourceName + QStringLiteral(" Copy"));
    params.setCompositionName(copyName);
    
    auto newCompResult = createComposition(params);
    if (!newCompResult.success) {
      qDebug() << "Impl::duplicateComposition failed: could not create new composition";
      return newCompResult;
    }
    
    auto newCompPtr = findComposition(newCompResult.id).ptr.lock();
    if (!newCompPtr) {
      result.success = false;
      result.message.setQString("Could not find newly created composition");
      qDebug() << "Impl::duplicateComposition failed: could not find new composition";
      return result;
    }
    
    const auto sourceLayers = compositionPtr->allLayer();
    int copiedCount = 0;
    for (const auto& sourceLayer : sourceLayers) {
      if (!sourceLayer) continue;
      QString layerName = sourceLayer->layerName();
      if (layerName.isEmpty()) {
        layerName = QStringLiteral("Layer");
      }
      auto layerCreate = [&]() -> ArtifactLayerResult {
        if (auto svgLayer = std::dynamic_pointer_cast<ArtifactSvgLayer>(sourceLayer)) {
          ArtifactSvgInitParams layerParams(layerName);
          layerParams.setSvgPath(svgLayer->sourcePath());
          return createLayerAndAddToComposition(newCompResult.id, layerParams);
        }
        LayerType inferredType = inferLayerTypeFromRuntimeName(sourceLayer);
        if (inferredType == LayerType::Unknown || inferredType == LayerType::None) {
          inferredType = LayerType::Solid;
        }
        ArtifactLayerInitParams layerParams(layerName, inferredType);
        return createLayerAndAddToComposition(newCompResult.id, layerParams);
      }();
      if (layerCreate.success) {
        copyLayerProperties(sourceLayer, layerCreate.layer, layerName);
        ++copiedCount;
      }
    }

    result.success = true;
    result.id = newCompResult.id;
    result.message.setQString(QStringLiteral("Composition duplicated (%1 layers copied)").arg(copiedCount));
    qDebug() << "Impl::duplicateComposition succeeded: new id=" << newCompResult.id.toString()
             << " copiedLayers=" << copiedCount;
    
    return result;
  }

  
  
  ArtifactLayerResult ArtifactProject::createLayerAndAddToComposition(const CompositionID& compositionId, ArtifactLayerInitParams& params)
  {
  auto result = impl_->createLayerAndAddToComposition(compositionId, params);
  if (result.success && result.layer) {
   publishLayerChangedEvent(compositionId, result.layer->id(), LayerChangedEvent::ChangeType::Created);
  }
  return result;
  }


  ArtifactLayerResult ArtifactProject::duplicateLayerInComposition(const CompositionID& compositionId, const LayerID& layerId)
  {
  auto result = impl_->duplicateLayerInComposition(compositionId, layerId);
  if (result.success && result.layer) {
   publishLayerChangedEvent(compositionId, result.layer->id(), LayerChangedEvent::ChangeType::Created);
     publishProjectChangedEvent();
  }
  return result;
  }

  CreateCompositionResult ArtifactProject::duplicateComposition(const CompositionID& compositionId)
  {
  auto result = impl_->duplicateComposition(compositionId);
  if (result.success) {
    publishCompositionCreatedEvent(result.id);
    publishProjectChangedEvent();
  }
  return result;
  }

  void ArtifactProject::createCompositions(const QStringList& names) {}

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
    if (ok) {
      publishLayerChangedEvent(compositionId, layerId, LayerChangedEvent::ChangeType::Removed);
    }
    return ok;
  }

void ArtifactProject::removeAllAssets()
{
  if (!impl_) return;
  // Find the root folder
  ProjectItem* root = nullptr;
  if (!impl_->ownedItems_.empty()) {
    root = impl_->ownedItems_.front().get();
  }
  if (!root || root->type() != eProjectItemType::Folder) return;

  // Clear all children except for the root itself
  root->children.clear();

  // Mark as dirty
  impl_->setDirty(true);
  publishProjectChangedEvent();
}

void ArtifactProject::restoreProjectItems(const QJsonArray& items)
{
  if (!impl_) return;

  // Find the root folder
  ProjectItem* root = nullptr;
  if (!impl_->ownedItems_.empty()) {
    root = impl_->ownedItems_.front().get();
  }
  if (!root || root->type() != eProjectItemType::Folder) return;

  // Drop all existing items except the root placeholder to avoid orphaned
  // pointers and duplicate tree entries on repeated load.
  if (!impl_->ownedItems_.empty()) {
    auto rootHolder = std::move(impl_->ownedItems_.front());
    impl_->ownedItems_.clear();
    impl_->ownedItems_.push_back(std::move(rootHolder));
    root = impl_->ownedItems_.front().get();
  }

  // Clear existing children
  root->children.clear();

  // Helper function to restore items recursively
  std::function<void(const QJsonObject&, ProjectItem*)> restoreItem = [&](const QJsonObject& obj, ProjectItem* parent) {
    QString type = obj["type"].toString();
    QString name = obj["name"].toString();
    QString idStr = obj["id"].toString();

    if (type == "footage") {
      auto footageUp = std::make_unique<FootageItem>();
      footageUp->name.setQString(name);
      footageUp->filePath = obj["filePath"].toString();
      if (!idStr.isEmpty()) {
        footageUp->id = Id(idStr);
      }
      footageUp->parent = parent;
      parent->children.append(footageUp.get());
      impl_->ownedItems_.push_back(std::move(footageUp));
    } else if (type == "folder") {
      auto folderUp = std::make_unique<FolderItem>();
      folderUp->name.setQString(name);
      if (!idStr.isEmpty()) {
        folderUp->id = Id(idStr);
      }
      folderUp->parent = parent;
      parent->children.append(folderUp.get());
      impl_->ownedItems_.push_back(std::move(folderUp));

      // Restore children
      QJsonArray children = obj["children"].toArray();
      for (const auto& childVal : children) {
        if (childVal.isObject()) {
          restoreItem(childVal.toObject(), folderUp.get());
        }
      }
    } else if (type == "solid") {
      auto solidUp = std::make_unique<SolidItem>();
      solidUp->name.setQString(name);
      if (!idStr.isEmpty()) {
        solidUp->id = Id(idStr);
      }
      QString colorStr = obj["color"].toString();
      if (!colorStr.isEmpty()) {
        solidUp->color = QColor(colorStr);
      }
      solidUp->parent = parent;
      parent->children.append(solidUp.get());
      impl_->ownedItems_.push_back(std::move(solidUp));
    } else if (type == "composition") {
      // Composition items are restored separately in the importer
      // Just create a placeholder here
      auto compItemUp = std::make_unique<CompositionItem>();
      compItemUp->name.setQString(name);
      if (!idStr.isEmpty()) {
        compItemUp->id = Id(idStr);
      }
      QString compIdStr = obj["compositionId"].toString();
      if (!compIdStr.isEmpty()) {
        compItemUp->compositionId = CompositionID(compIdStr);
      }
      compItemUp->parent = parent;
      parent->children.append(compItemUp.get());
      impl_->ownedItems_.push_back(std::move(compItemUp));
    }
  };

  // Restore top-level items
  for (const auto& val : items) {
    if (val.isObject()) {
      restoreItem(val.toObject(), root);
    }
  }

  // Mark as dirty (since we modified the project)
  impl_->setDirty(true);
  publishProjectChangedEvent();
}

bool ArtifactProject::addProjectItemsFromJson(const QJsonArray& items, ProjectItem* parent)
{
  if (!impl_) {
    return false;
  }
  const bool ok = impl_->addProjectItemsFromJson(items, parent);
  if (ok) {
    publishProjectChangedEvent();
  }
  return ok;
}




};
