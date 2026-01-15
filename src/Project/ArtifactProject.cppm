module;
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
 };


 ArtifactProject::Impl::Impl()
 {

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

 }

 ArtifactProject::ArtifactProject(const QString& name) :impl_(new Impl())
 {

 }

 ArtifactProject::ArtifactProject(const ArtifactProjectSettings& setting) :impl_(new Impl())
 {

 }

 ArtifactProject::~ArtifactProject()
 {
  delete impl_;
 }

 void ArtifactProject::createComposition(const QString& name)
 {
  CompositionID id;

  QSignalSpy spy(this, &ArtifactProject::compositionCreated);



  /*emit*/compositionCreated(id);

  QCOMPARE(spy.count(), 1);

  // 引数の中身を確認（QString）
  QList<QVariant> arguments = spy.takeFirst();
  QString idStr = arguments.at(0).toString();

  qDebug() << "シグナルで通知された ID:" << idStr;
 }

 CreateCompositionResult ArtifactProject::createComposition(const ArtifactCompositionInitParams& param)
 {

  return impl_->createComposition(param);
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