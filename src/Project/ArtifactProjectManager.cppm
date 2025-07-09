module;
#include <QDir>
#include <QFile>
#include <QJsonObject>
#include <QStandardPaths>

#include <QTextStream>
#include <wobjectimpl.h>
//#include <folly\Singleton.h>

module Project.Manager;


namespace Artifact {
 
 W_OBJECT_IMPL(ArtifactProjectManager)


	class ArtifactProjectManager::Impl {
	private:

	public:


	};

 ArtifactProjectManager::ArtifactProjectManager(QObject* parent /*= nullptr*/):QObject(parent)
 {

 }

 ArtifactProjectManager::~ArtifactProjectManager()
 {

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
  static ArtifactProjectManager instance; // Å‰‚ÌŒÄ‚Ño‚µ‚É‚Ì‚İ‰Šú‰»
  return instance;
 }

 void ArtifactProjectManager::loadfromFile(const QString& fullpath)
 {

 }

 bool projectManagerCurrentClose()
 {

  return true;
 }

}