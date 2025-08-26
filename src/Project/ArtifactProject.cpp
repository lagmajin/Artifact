module;
#include <wobjectimpl.h>
#include <wobjectdefs.h>

#include <QtTest/QtTest>
//#include <QtCore/QString>


module Project;

import Utils;

import Composition.Settings;

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

 public:
  Impl();
  ~Impl();
  void addAssetFromPath(const QString& string);
  void createComposition(const QString& str);
  void createComposition(const CompositionSettings& settings);
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

 bool ArtifactProject::isNull() const
 {
  return false;
 }

 void ArtifactProject::addAssetFromPath(const QString& filepath)
 {

 }

}