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
  return currentProjectPtr_->createComposition(params);
 	
 }

 bool ArtifactProjectManager::Impl::isProjectCreated() const
 {
  if(currentProjectPtr_)
  {
   return true;
  }
  return false;
 }

 ArtifactProjectManager::ArtifactProjectManager(QObject* parent /*= nullptr*/) :QObject(parent), impl_(new Impl())
 {


 }

 ArtifactProjectManager::~ArtifactProjectManager()
 {
  delete impl_;
 }

 bool ArtifactProjectManager::closeCurrentProject()
 {

  return true;
 }

 void ArtifactProjectManager::createProject()
 {
  impl_->createProject();

  connect(impl_->currentProjectPtr_.get(), &ArtifactProject::projectChanged, this, &ArtifactProjectManager::projectChanged);

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

  impl_->createProject();

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
  return impl_->isCreated_ || (impl_->currentProjectPtr_ != nullptr);
 }

 std::shared_ptr<ArtifactProject> ArtifactProjectManager::getCurrentProjectSharedPtr()
 {

  return impl_->currentProjectPtr_;
 }

QVector<ProjectItem*> ArtifactProjectManager::projectItems() const
{
 if (impl_->currentProjectPtr_)
 {
  return impl_->currentProjectPtr_->projectItems();
 }
 return QVector<ProjectItem*>();
}
	
 void ArtifactProjectManager::createComposition()
 {
 // Create a composition using default init params and emit the created ID
 ArtifactCompositionInitParams params;
 CreateCompositionResult res = impl_->createComposition(params);
 if (res.success) {
  /*emit*/ compositionCreated(res.id);
 } else {
  qDebug() << "ArtifactProjectManager::createComposition failed to create composition";
 }
 }

 void ArtifactProjectManager::createComposition(const QString, const QSize& size)
 {

 }

 CreateCompositionResult ArtifactProjectManager::createComposition(const ArtifactCompositionInitParams& params)
 {
  auto result = impl_->createComposition(params);

  return result;
 }

 CreateCompositionResult ArtifactProjectManager::createComposition(const UniString& str)
 {
 ArtifactCompositionInitParams params;
 // try to set a name if provided
 try {
  params.setCompositionName(str);
 } catch (...) {
 }

 qDebug() << "ArtifactProjectManager::createComposition requested name:" << str.toQString();
 auto result = impl_->createComposition(params);
 if (result.success) {
  qDebug() << "ArtifactProjectManager::createComposition succeeded id:" << result.id.toString();
  /*emit*/ compositionCreated(result.id);
 } else {
  qDebug() << "ArtifactProjectManager::createComposition failed";
 }
 return result;
 }

 void ArtifactProjectManager::addAssetFromFilePath(const QString& filePath)
 {
  impl_->addAssetFromFilePath(filePath);
 }

 void ArtifactProjectManager::addAssetsFromFilePaths(const QStringList& filePaths)
 {
  impl_->addAssetsFromFilePaths(filePaths);
 }

 ArtifactCompositionPtr ArtifactProjectManager::currentComposition()
 {

  return nullptr;
 }

 FindCompositionResult ArtifactProjectManager::findComposition(const CompositionID& id)
 {
  return impl_->currentProjectPtr_->findComposition(id);
 	
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