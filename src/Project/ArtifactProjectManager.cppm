module;
#include <QDir>
#include <QFile>
#include <QJsonObject>
#include <QStandardPaths>

#include <QTextStream>
#include <wobjectimpl.h>
//#include <folly\Singleton.h>

module Artifact.Project.Manager;

import std;
import Utils;
import Artifact.Project;
import Artifact.Composition.Result;
import Artifact.Composition.Abstract;
import Composition.Settings;



namespace Artifact {

 using namespace ArtifactCore;

 W_OBJECT_IMPL(ArtifactProjectManager)


  class ArtifactProjectManager::Impl {
  private:
   //ArtifactProjectPtr currentProjectPtr_;
  public:
   Impl();
   ~Impl();
   bool isCreated_ = false;
   std::shared_ptr<ArtifactProject> currentProjectPtr_;
   void createProject();
   Id createNewComposition();
   CompositionResult createComposition(const CompositionSettings& setting);
   void addAssetFromFilePath(const QString& filePath);
   void addAssetsFromFilePaths(const QStringList& filePaths);
 };

 ArtifactProjectManager::Impl::Impl()
 {

 }

 void ArtifactProjectManager::Impl::createProject()
 {
  if (!currentProjectPtr_)
  {
   currentProjectPtr_ = std::make_shared<ArtifactProject>();
  }
  else {


  }



 }

 ArtifactProjectManager::Impl::~Impl()
 {

 }

 Id ArtifactProjectManager::Impl::createNewComposition()
 {
  if (currentProjectPtr_)
  {
   currentProjectPtr_->createComposition("");
  }




  Id id;



  return id;
 }

 void ArtifactProjectManager::Impl::addAssetFromFilePath(const QString& filePath)
 {
  currentProjectPtr_->addAssetFromPath(filePath);
 	
 }

 void ArtifactProjectManager::Impl::addAssetsFromFilePaths(const QStringList& filePaths)
 {

 }

 CompositionResult ArtifactProjectManager::Impl::createComposition(const CompositionSettings& setting)
 {
  auto result = CompositionResult();

  return result;
 }

 ArtifactProjectManager::ArtifactProjectManager(QObject* parent /*= nullptr*/) :QObject(parent), Impl_(new Impl())
 {

 }

 ArtifactProjectManager::~ArtifactProjectManager()
 {
  delete Impl_;
 }

 bool ArtifactProjectManager::closeCurrentProject()
 {

  return true;
 }

 void ArtifactProjectManager::createProject()
 {
  Impl_->createProject();

  /*emit*/ newProjectCreated();
 }

 void ArtifactProjectManager::createProject(const QString& projectName, bool force/*=false*/)
 {
  qDebug() << "ArtifactProjectManager::createProject with name:" << projectName;

  Impl_->createProject();

  /*emit*/ newProjectCreated();
 }

 ArtifactProjectManager& ArtifactProjectManager::getInstance()
 {
  static ArtifactProjectManager instance; // 最初の呼び出し時にのみ初期化
  return instance;
 }

 void ArtifactProjectManager::loadFromFile(const QString& fullpath)
 {
  QFile file(fullpath);

  if (file.exists())
  {

  }

 }

 bool ArtifactProjectManager::projectCreated() const
 {
  return true;
 }

 std::shared_ptr<ArtifactProject> ArtifactProjectManager::getCurrentProjectSharedPtr()
 {

  return Impl_->currentProjectPtr_;
 }

 void ArtifactProjectManager::createComposition(const QString& str)
 {
  newCompositionCreated();

 }

 void ArtifactProjectManager::createComposition()
 {
  Impl_->createNewComposition();


  newCompositionCreated();
 }

 void ArtifactProjectManager::createComposition(const QString, const QSize& size)
 {

 }

 CompositionResult ArtifactProjectManager::createComposition(const CompositionSettings& setting)
 {

  auto result=Impl_->createComposition(setting);
 	
  newCompositionCreated();
 	
  return result;
 }

 void ArtifactProjectManager::addAssetFromFilePath(const QString& filePath)
 {
  Impl_->addAssetFromFilePath(filePath);
 }

 void ArtifactProjectManager::addAssetsFromFilePaths(const QStringList& filePaths)
 {
  Impl_->addAssetsFromFilePaths(filePaths);
 }

 ArtifactCompositionPtr ArtifactProjectManager::currentComposition()
 {

  return nullptr;
 }

 ArtifactCompositionPtr ArtifactProjectManager::findComposition(const CompositionID& id)
 {

  return nullptr;
 }

 bool projectManagerCurrentClose()
 {

  return true;
 }

}