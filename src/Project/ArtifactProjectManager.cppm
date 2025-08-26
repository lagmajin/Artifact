module;
#include <QDir>
#include <QFile>
#include <QJsonObject>
#include <QStandardPaths>

#include <QTextStream>
#include <wobjectimpl.h>
//#include <folly\Singleton.h>

module Project.Manager;

import Project;



import Utils;

namespace Artifact {

 using namespace ArtifactCore;
 
 W_OBJECT_IMPL(ArtifactProjectManager)


  class ArtifactProjectManager::Impl {
  private:




  public:
   Impl();
   ~Impl();
   std::shared_ptr<ArtifactProject> spProject_;
   void createProject();
   Id createNewComposition();
   void addAssetFromFilePath(const QString& filePath);
   void addAssetsFromFilePaths(const QStringList& filePaths);
 };

 ArtifactProjectManager::Impl::Impl()
 {

 }

 void ArtifactProjectManager::Impl::createProject()
 {
  if (!spProject_)
  {
   spProject_ = std::make_shared<ArtifactProject>();
  }

  

 }

 ArtifactProjectManager::Impl::~Impl()
 {

 }

 Id ArtifactProjectManager::Impl::createNewComposition()
 {
  Id id;



  return id;
 }

 void ArtifactProjectManager::Impl::addAssetFromFilePath(const QString& filePath)
 {

 }

 void ArtifactProjectManager::Impl::addAssetsFromFilePaths(const QStringList& filePaths)
 {

 }

 ArtifactProjectManager::ArtifactProjectManager(QObject* parent /*= nullptr*/):QObject(parent),Impl_(new Impl())
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

  emit newProjectCreated();
 }

 void ArtifactProjectManager::createProject(const QString& projectName)
 {
  Impl_->createProject();

  emit newProjectCreated();
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

  return Impl_->spProject_;
 }

 void ArtifactProjectManager::createNewComposition(const QString& str)
 {
  newCompositionCreated();

 }

 void ArtifactProjectManager::createNewComposition()
 {
  Impl_->createNewComposition();


  newCompositionCreated();
 }

 void ArtifactProjectManager::addAssetFromFilePath(const QString& filePath)
 {

 }

 void ArtifactProjectManager::addAssetsFromFilePaths(const QStringList& filePaths)
 {

 }

 bool projectManagerCurrentClose()
 {

  return true;
 }

}