module;
#include <QDebug>
#include <memory>
#include <vector>
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
  
  AssetMultiIndexContainer assetContainer_;
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

  QJsonObject toJson() const;
  ArtifactCompositionMultiIndexContainer container_;
  QVector<ProjectItem*> itemsRoot_;
  std::vector<std::unique_ptr<ProjectItem>> ownedItems_; // owns all allocated items
 };


 ArtifactProject::Impl::Impl()
 {

 }

QVector<ProjectItem*> ArtifactProject::projectItems() const
{
 // return shallow pointers to root items
 return impl_->itemsRoot_;
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

 ArtifactCompositionInitParams params;
 auto newComposition = new ArtifactComposition(id, params);

  CreateCompositionResult result;
  result.id = newComposition->id();
  result.success = true;


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
  impl_->itemsRoot_.push_back(root);

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

 // find created composition item and set its name
 for (auto root : impl_->itemsRoot_) {
  if (!root) continue;
  for (auto child : root->children) {
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

 auto res = impl_->createComposition(param);
 if (res.success) {
  // add composition item to project tree
  auto compItemUp = std::make_unique<CompositionItem>();
  compItemUp->compositionId = res.id;
  // set default name for composition (params don't provide name in current API)
  compItemUp->name.setQString(QStringLiteral("Composition"));
  // capture name before moving the unique_ptr
  QString createdName = compItemUp->name.toQString();
  ProjectItem* raw = compItemUp.get();
  impl_->ownedItems_.push_back(std::move(compItemUp));
  impl_->itemsRoot_.push_back(raw);
  // emit signal
  compositionCreated(res.id);
  // log using captured name (compItemUp is null after move)
  QString idStr = res.id.toString();
  qDebug() << "Composition created:" << createdName << "(ID:" << idStr << ")";
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




};