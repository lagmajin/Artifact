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
import Artifact.Layer.InitParams;
import Artifact.Layer.Result;
import Artifact.Layer.Factory;


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
    bool signalsConnected_ = false;
    bool suppressDefaultCreate_ = false;
    bool creatingComposition_ = false;
    void createProject();
    bool isProjectCreated() const;
    Id createNewComposition();
    //CompositionResult createComposition(const CompositionSettings& settings);
    CreateCompositionResult createComposition(const CompositionSettings& setting);
    CreateCompositionResult createComposition(const ArtifactCompositionInitParams& params);
    void addAssetFromFilePath(const QString& filePath);
    void addAssetsFromFilePaths(const QStringList& filePaths);
    
    // Layer management
    ArtifactLayerResult addLayerToCurrentComposition(ArtifactLayerInitParams& params);
    ArtifactLayerResult addLayerToComposition(const CompositionID& compositionId, ArtifactLayerInitParams& params);
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
  if (!currentProjectPtr_) {
    qDebug() << "addAssetFromFilePath failed: no current project";
    return;
  }
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
 if (!currentProjectPtr_) {
  CreateCompositionResult result;
  result.success = false;
  result.message.setQString("No project: cannot create composition");
  qDebug() << "Impl::createComposition failed: currentProjectPtr_ is null";
  return result;
 }
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

  // ensure current project pointer is valid before connecting signals
  if (impl_->currentProjectPtr_) {
    // Use lambda forwarding with weak_ptr capture to avoid using raw pointers
    if (!impl_->signalsConnected_) {
      auto shared = impl_->currentProjectPtr_;
      std::weak_ptr<ArtifactProject> weakProj = shared;
      connect(shared.get(), &ArtifactProject::projectChanged, this, [weakProj, this]() {
        if (weakProj.lock()) {
          // forward signal from project to manager
          projectChanged();
        }
      });
      connect(shared.get(), &ArtifactProject::compositionCreated, this, [weakProj, this](const CompositionID& id) {
        if (weakProj.lock()) {
          compositionCreated(id);
        }
      });
      impl_->signalsConnected_ = true;
    }
  } else {
    qDebug() << "createProject: failed to create currentProjectPtr_";
  }

  /*emit*/ projectCreated();
 }

// Call this to prevent project-created default composition creation in the
// current operation context (e.g., when UI immediately requests a named
// composition after creating a project).
void ArtifactProjectManager::suppressDefaultCreate(bool v)
{
  if (impl_) impl_->suppressDefaultCreate_ = v;
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
 // Ensure a project exists so UI/model get updated and signals are wired
 if (!impl_->currentProjectPtr_) {
   createProject();
 }
 // If suppression flag is set, do not create a default composition.
 if (impl_->suppressDefaultCreate_) {
   qDebug() << "Default composition creation suppressed";
   CreateCompositionResult res;
   res.success = false;
   return;
 }
 CreateCompositionResult res = impl_->createComposition(params);
 if (res.success) {
  // The underlying ArtifactProject emits `compositionCreated` and the manager
  // forwards that signal when a project exists. Avoid re-emitting here to
  // prevent duplicate notifications.
 } else {
  qDebug() << "ArtifactProjectManager::createComposition failed to create composition";
 }
 }

 void ArtifactProjectManager::createComposition(const QString, const QSize& size)
 {

 }

 CreateCompositionResult ArtifactProjectManager::createComposition(const ArtifactCompositionInitParams& params)
 {
 // Ensure a project exists so UI/model get updated and signals are wired
 if (!impl_->currentProjectPtr_) {
   createProject();
 }
 // guard reentrancy: if we're already creating a composition, skip duplicate
 if (impl_->creatingComposition_) {
   CreateCompositionResult r;
   r.success = false;
   return r;
 }
 impl_->creatingComposition_ = true;
 auto result = impl_->createComposition(params);
 impl_->creatingComposition_ = false;

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
 // Ensure a project exists before creating composition so UI updates
 if (!impl_->currentProjectPtr_) {
   createProject();
 }
 auto result = impl_->createComposition(params);
 if (result.success) {
  qDebug() << "ArtifactProjectManager::createComposition succeeded id:" << result.id.toString();
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

  ArtifactLayerResult ArtifactProjectManager::Impl::addLayerToCurrentComposition(ArtifactLayerInitParams& params)
  {
   ArtifactLayerResult result;
   
   if (!currentProjectPtr_) {
    result.success = false;
    return result;
   }

   // Get current composition - assuming first composition for now
   // You may need to implement getCurrentCompositionId() method
   auto projectItems = currentProjectPtr_->projectItems();
   CompositionID currentCompId;
   
   // Find the first composition item
   for (auto item : projectItems) {
    if (!item) continue;
    for (auto child : item->children) {
     if (child && child->type() == eProjectItemType::Composition) {
      CompositionItem* compItem = static_cast<CompositionItem*>(child);
      currentCompId = compItem->compositionId;
      break;
     }
    }
    if (!currentCompId.isNil()) break;
   }
   if (currentCompId.isNil()) {
    result.success = false;
    return result;
   }

   result = currentProjectPtr_->createLayerAndAddToComposition(currentCompId, params);
   
   return result;
  }

  ArtifactLayerResult ArtifactProjectManager::Impl::addLayerToComposition(const CompositionID& compositionId, ArtifactLayerInitParams& params)
  {
   ArtifactLayerResult result;
   
   if (!currentProjectPtr_) {
    result.success = false;
    return result;
   }

   result = currentProjectPtr_->createLayerAndAddToComposition(compositionId, params);
   
   return result;
  }

  ArtifactLayerResult ArtifactProjectManager::addLayerToCurrentComposition(ArtifactLayerInitParams& params)
  {
   auto result = impl_->addLayerToCurrentComposition(params);
   if (result.success && result.layer) {
    layerCreated(result.layer->id());
   }
   return result;
  }

  ArtifactLayerResult ArtifactProjectManager::addLayerToComposition(const CompositionID& compositionId, ArtifactLayerInitParams& params)
  {
   auto result = impl_->addLayerToComposition(compositionId, params);
   if (result.success && result.layer) {
    layerCreated(result.layer->id());
   }
   return result;
  }

  bool projectManagerCurrentClose()
  {

   return true;
  }

}