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

 }

 bool projectManagerCurrentClose()
 {

  return true;
 }

}