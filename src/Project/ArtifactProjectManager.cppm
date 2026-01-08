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
import Artifact.Composition.InitParams;


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
   bool isProjectCreated() const;
   Id createNewComposition();
   //CompositionResult createComposition(const CompositionSettings& settings);
   CreateCompositionResult createComposition(const CompositionSettings& setting);
   CreateCompositionResult createComposition(const ArtifactCompositionInitParams& params);
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

 CreateCompositionResult ArtifactProjectManager::Impl::createComposition(const CompositionSettings& setting)
 {
  auto result = CreateCompositionResult();

  //auto newCompositionPtr = currentProjectPtr_->createComposition(setting.compositionName);


  return result;
 }

 CreateCompositionResult ArtifactProjectManager::Impl::createComposition(const ArtifactCompositionInitParams& params)
 {
  currentProjectPtr_->createComposition(params);
 	
 }

 bool ArtifactProjectManager::Impl::isProjectCreated() const
 {
  if(currentProjectPtr_)
  {
   return true;
  }
  return false;
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

  /*emit*/ projectCreated();
 }

 CreateProjectResult ArtifactProjectManager::createProject(const UniString& name, bool force)
 {
  CreateProjectResult result;

  return result;
 }

 void ArtifactProjectManager::createProject(const QString& projectName, bool force/*=false*/)
 {
  qDebug() << "ArtifactProjectManager::createProject with name:" << projectName;

  Impl_->createProject();

  /*emit*/ projectCreated();
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

 bool ArtifactProjectManager::isProjectCreated() const
 {
  return true;
 }

 std::shared_ptr<ArtifactProject> ArtifactProjectManager::getCurrentProjectSharedPtr()
 {

  return Impl_->currentProjectPtr_;
 }

 void ArtifactProjectManager::createComposition(const QString& str)
 {
  CompositionID id;

  /*emit*/ compositionCreated(id);

 }

 void ArtifactProjectManager::createComposition()
 {
  Impl_->createNewComposition();

  CompositionID id;

  /*emit*/compositionCreated(id);
 }

 void ArtifactProjectManager::createComposition(const QString, const QSize& size)
 {

 }

 CreateCompositionResult ArtifactProjectManager::createComposition(const CompositionSettings& setting)
 {

  auto result=Impl_->createComposition(setting);
 	
  //compositionCreated(CompositionID());
 	
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

 FindCompositionResult ArtifactProjectManager::findComposition(const CompositionID& id)
 {
  return Impl_->currentProjectPtr_->findComposition(id);
 	
 }

 bool ArtifactProjectManager::isProjectClosed() const
 {
  return true;
 }

 int ArtifactProjectManager::compositionCount() const
 {
  return 0;
 }

 void ArtifactProjectManager::removeAllAssets()
 {

 }

 bool projectManagerCurrentClose()
 {

  return true;
 }

}