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

import Composition.Settings;
import Container;
import Asset.File;

import Artifact.Composition.Abstract;
import Artifact.Composition._2D;


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
  ArtifactCompositionMultiIndexContainer container_;
  AssetMultiIndexContainer assetContainer_;
 public:
  Impl();
  ~Impl();
  void addAssetFromPath(const QString& string);
  CreateCompositionResult createComposition(const QString& str);
  void createComposition(const CompositionSettings& settings);

  void createCompositions(const QStringList& names);
  void findComposition(const CompositionID& id);
  bool removeById(const CompositionID& id);
  void removeAllCompositions();

  QJsonObject toJson() const;
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

 void ArtifactProject::Impl::createComposition(const CompositionSettings& settings)
 {
  auto newComposition = new ArtifactComposition();


  //container_.add(settings);

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

ArtifactCompositionPtr ArtifactProject::findComposition()
 {

 return nullptr;
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

 CreateCompositionResult ArtifactProject::createComposition(const CompositionSettings& settings)
 {

  return CreateCompositionResult();
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

 QJsonObject ArtifactProject::toJson() const
 {

  return  impl_->toJson();
 }

 bool ArtifactProject::isDirty() const
 {
  return false;
 }



}