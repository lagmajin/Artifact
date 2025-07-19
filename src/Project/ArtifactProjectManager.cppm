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


namespace Artifact {
 
 W_OBJECT_IMPL(ArtifactProjectManager)


	class ArtifactProjectManager::Impl {
	private:
	 ArtifactProject project_;

	public:
	 Impl();
	 ~Impl();

	};

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

 }

 void ArtifactProjectManager::createProject(const QString& projectName)
 {

 }

 ArtifactProjectManager& ArtifactProjectManager::getInstance()
 {
  static ArtifactProjectManager instance; // 最初の呼び出し時にのみ初期化
  return instance;
 }

 void ArtifactProjectManager::loadfromFile(const QString& fullpath)
 {
  QFile file(fullpath);

  if (file.exists())
  {

  }

 }

 bool projectManagerCurrentClose()
 {

  return true;
 }

}