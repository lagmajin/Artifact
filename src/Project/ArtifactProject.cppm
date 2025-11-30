module;
#include <wobjectimpl.h>
#include <wobjectdefs.h>

#include <QHash>
#include <QVector>
#include <QtTest/QtTest>
//#include <QtCore/QString>


module Project;

import Utils;
import Utils.String.Like;

import Composition.Settings;

import Artifact.Composition.Abstract;

import Container;

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
 


 class ArtifactProject::Impl {
 private:
  ArtifactProjectSettings projectSettings_;
  ArtifactCompositionMultiIndexContainer container_;
 public:
  Impl();
  ~Impl();
  void addAssetFromPath(const QString& string);
  CompositionResult createComposition(const QString& str);
  void createComposition(const CompositionSettings& settings);
 	
 	void createCompositions(const QStringList& names);
  bool removeById(const CompositionID& id);
  void removeAllCompositions();
 };


 ArtifactProject::Impl::Impl()
 {

 }

 ArtifactProject::Impl::~Impl()
 {

 }

 void ArtifactProject::Impl::addAssetFromPath(const QString& string)
 {

 }

 void ArtifactProject::Impl::createComposition(const CompositionSettings& settings)
 {
  

  //container_.add(settings);

 }

 void ArtifactProject::Impl::removeAllCompositions()
 {

 }

 bool ArtifactProject::Impl::removeById(const CompositionID& id)
 {


  return false;
 }

 ArtifactProject::ArtifactProject() :impl_(new Impl())
 {

 }

 ArtifactProject::ArtifactProject(const QString& name):impl_(new Impl())
 {

 }

 ArtifactProject::ArtifactProject(const ArtifactProjectSettings& setting):impl_(new Impl())
 {

 }

 ArtifactProject::~ArtifactProject()
 {
  delete impl_;
 }

 void ArtifactProject::createComposition(const QString& name)
 {
  Id id;

  QSignalSpy spy(this, &ArtifactProject::compositionCreated);

  compositionCreated("Test");

  QCOMPARE(spy.count(), 1);

  // 引数の中身を確認（QString）
  QList<QVariant> arguments = spy.takeFirst();
  QString idStr = arguments.at(0).toString();

  qDebug() << "シグナルで通知された ID:" << idStr;
 }

 CompositionResult ArtifactProject::createComposition(const CompositionSettings& settings)
 {

  return CompositionResult();
 }

 bool ArtifactProject::isNull() const
 {
  return false;
 }

 void ArtifactProject::addAssetFromPath(const QString& filepath)
 {

 }

 bool ArtifactProject::removeCompositionById(const CompositionID& id)
 {
  return impl_->removeById(id);
 }

 void ArtifactProject::removeAllCompositions()
 {

 }

 void ArtifactProject::addAssetFile()
 {

 }

}