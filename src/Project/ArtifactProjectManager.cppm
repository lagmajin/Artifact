module;
#include <QDir>
#include <QFile>
#include <QJsonObject>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QStringList>

#include <QTextStream>
#include <wobjectimpl.h>
//#include <folly\Singleton.h>

module Artifact.Project.Manager;

import std;
import Utils;
import Artifact.Project;
import Artifact.Project.Exporter;
import Artifact.Project.Importer;
import Artifact.Composition.Result;
import Artifact.Composition.Abstract;
import Composition.Settings;
import Artifact.Composition.InitParams;
import Artifact.Layer.InitParams;
import Artifact.Layer.Result;
import Artifact.Layer.Factory;


namespace Artifact {

 using namespace ArtifactCore;

 namespace {
  static const QStringList kProjectSubfolders = {
    QStringLiteral("Assets"),
    QStringLiteral("Scenes"),
    QStringLiteral("Settings")
  };

  QString defaultProjectDisplayName()
  {
    return QStringLiteral("UntitledProject");
  }

  QString sanitizeProjectDirectoryName(const QString& rawName)
  {
    QString sanitized = rawName.trimmed();
    if (sanitized.isEmpty()) {
      return defaultProjectDisplayName();
    }

    sanitized.replace(' ', '_');
    static const QRegularExpression invalidChars(R"([^a-zA-Z0-9_\-])");
    sanitized.replace(invalidChars, QStringLiteral("_"));

    if (sanitized.isEmpty()) {
      return defaultProjectDisplayName();
    }

    return sanitized;
  }

  QString projectsDirectoryRoot()
  {
    QString base = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    if (base.isEmpty()) {
      base = QDir::homePath();
    }
    QDir root(base);
    root.mkpath(QStringLiteral("ArtifactProjects"));
    return root.filePath(QStringLiteral("ArtifactProjects"));
  }

  QString buildProjectRootPath(const QString& directoryName)
  {
    QDir base(projectsDirectoryRoot());
    base.mkpath(QStringLiteral("."));
    return base.filePath(directoryName);
  }

  void ensureProjectFolders(const QString& rootPath)
  {
    if (rootPath.isEmpty()) {
      return;
    }
    QDir root(rootPath);
    if (!root.exists()) {
      root.mkpath(QStringLiteral("."));
    }
    for (const QString& subfolder : kProjectSubfolders) {
      root.mkpath(subfolder);
    }
  }
 }

 W_OBJECT_IMPL(ArtifactProjectManager)


class ArtifactProjectManager::Impl {
public:
  QString currentProjectPath_;
public:
  Impl();
  ~Impl();
  bool isCreated_ = false;
  std::shared_ptr<ArtifactProject> currentProjectPtr_;
  bool signalsConnected_ = false;
  bool suppressDefaultCreate_ = false;
  bool creatingComposition_ = false;
  QString projectDisplayName_;
  QString projectDirectoryName_;
  QString projectRootPath_;
  void createProject(const QString& name, bool force);
  QString assetsFolderPath() const;
  bool copyAssetIntoProject(const QString& source, QString* outDestination);
  QString makeUniqueAssetPath(const QString& directory, const QString& fileName) const;
  QString relativeAssetPath(const QString& absoluteAssetPath) const;
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
  bool removeLayerFromComposition(const CompositionID& compositionId, const LayerID& layerId);
  // ArtifactLayerResult duplicateLayerInComposition(const CompositionID& compositionId, const LayerID& layerId);
  // CreateCompositionResult duplicateComposition(const CompositionID& compositionId);
};

 ArtifactProjectManager::Impl::Impl()
 {

 }

void ArtifactProjectManager::Impl::createProject(const QString& name, bool /*force*/)
{
 QString displayName = name.trimmed();
 if (displayName.isEmpty()) {
  displayName = defaultProjectDisplayName();
 }

 projectDisplayName_ = displayName;
 projectDirectoryName_ = sanitizeProjectDirectoryName(displayName);
 projectRootPath_ = buildProjectRootPath(projectDirectoryName_);
 currentProjectPath_ = projectRootPath_;

 ensureProjectFolders(projectRootPath_);

 if (!currentProjectPtr_) {
  currentProjectPtr_ = std::make_shared<ArtifactProject>(displayName);
 } else {
  currentProjectPtr_->setProjectName(displayName);
 }
 isCreated_ = true;
}

QString ArtifactProjectManager::Impl::assetsFolderPath() const
{
 if (projectRootPath_.isEmpty()) {
  return QString();
 }
 ensureProjectFolders(projectRootPath_);
 QDir assetsDir(projectRootPath_);
 assetsDir.mkpath(QStringLiteral("Assets"));
 return assetsDir.filePath(QStringLiteral("Assets"));
}

bool ArtifactProjectManager::Impl::copyAssetIntoProject(const QString& source, QString* outDestination)
{
 if (source.isEmpty()) {
  return false;
 }

 QString assetsPath = assetsFolderPath();
 if (assetsPath.isEmpty()) {
  return false;
 }

 QString finalFile = makeUniqueAssetPath(assetsPath, QFileInfo(source).fileName());
 if (finalFile.isEmpty()) {
  return false;
 }

 if (!QFile::copy(source, finalFile)) {
  return false;
 }

 if (outDestination) {
  *outDestination = finalFile;
 }
 return true;
}

QString ArtifactProjectManager::Impl::relativeAssetPath(const QString& absoluteAssetPath) const
{
 if (absoluteAssetPath.isEmpty()) {
  return QString();
 }
 QString assetsPath = assetsFolderPath();
 if (assetsPath.isEmpty()) {
  return absoluteAssetPath;
 }
 QDir assetsDir(assetsPath);
 return assetsDir.relativeFilePath(absoluteAssetPath);
}

QString ArtifactProjectManager::Impl::makeUniqueAssetPath(const QString& directory, const QString& fileName) const
{
 if (directory.isEmpty() || fileName.isEmpty()) {
  return QString();
 }

 QDir dir(directory);
 QFileInfo info(fileName);
 QString baseName = info.completeBaseName();
 QString extension = info.completeSuffix();
 QString candidate = dir.filePath(info.fileName());
 int counter = 1;

 while (QFile::exists(candidate)) {
  QString numbered = baseName;
  if (counter > 1) {
   numbered = QString("%1_%2").arg(baseName).arg(counter);
  }
  candidate = dir.filePath(extension.isEmpty() ? numbered : QString("%1.%2").arg(numbered).arg(extension));
  ++counter;
 }

 return candidate;
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
  if (!currentProjectPtr_) {
    qDebug() << "addAssetsFromFilePaths failed: no current project";
    return;
  }
  if (filePaths.isEmpty()) {
    qDebug() << "addAssetsFromFilePaths: file list is empty";
    return;
  }
  
  qDebug() << "addAssetsFromFilePaths: adding" << filePaths.size() << "files";
  for (const QString& filePath : filePaths) {
    if (filePath.isEmpty()) {
      qDebug() << "addAssetsFromFilePaths: skipping empty file path";
      continue;
    }
    currentProjectPtr_->addAssetFromPath(filePath);
  }
  qDebug() << "addAssetsFromFilePaths: completed";
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
  impl_->currentProjectPtr_.reset();
  impl_->currentProjectPath_.clear();
  impl_->signalsConnected_ = false;
  return true;
 }

void ArtifactProjectManager::createProject()
{
 createProject(defaultProjectDisplayName());
}

void ArtifactProjectManager::createProject(const QString& projectName, bool force/*=false*/)
{
 qDebug() << "ArtifactProjectManager::createProject with name:" << projectName;

 impl_->createProject(projectName, force);

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

 // CreateProjectResult ArtifactProjectManager::createProject(const UniString& name, bool force)
 // {
 //  CreateProjectResult result;
 //  
 //  // Check if project already exists and force flag is not set
 //  if (impl_->currentProjectPtr_ && !force) {
 //    result.success = false;
 //    result.message.setQString("Project already exists. Use force=true to overwrite.");
 //    qDebug() << "createProject failed: project already exists";
 //    return result;
 //  }
 //  
 //  // Create the project
 //  impl_->createProject();
 //  
 //  if (!impl_->currentProjectPtr_) {
 //    result.success = false;
 //    result.message.setQString("Failed to create project");
 //    qDebug() << "createProject failed: currentProjectPtr_ is null after creation";
 //    return result;
 //  }
 //  
 //  // Set project name if provided
 //  if (!name.isEmpty()) {
 //    impl_->currentProjectPtr_->setProjectName(name.toQString());
 //  }
 //  
 //  result.success = true;
 //  result.message.setQString("Project created successfully");
 //  // Note: projectId could be set here if CreateProjectResult has such field
 //  
 //  qDebug() << "createProject succeeded with name:" << (name.isEmpty() ? "<default>" : name.toQString());
 //  return result;
 // }

QString ArtifactProjectManager::currentProjectAssetsPath() const
{
 return impl_ ? impl_->assetsFolderPath() : QString();
}

QStringList ArtifactProjectManager::copyFilesToProjectAssets(const QStringList& sourcePaths)
{
 QStringList copied;
 if (!impl_) return copied;
 for (const QString& source : sourcePaths) {
  QString dest;
  if (impl_->copyAssetIntoProject(source, &dest)) {
   copied.append(dest);
  }
 }
 return copied;
}

QString ArtifactProjectManager::relativeAssetPath(const QString& absoluteAssetPath) const
{
 return impl_ ? impl_->relativeAssetPath(absoluteAssetPath) : QString();
}

ArtifactProjectManager& ArtifactProjectManager::getInstance()
{
  static ArtifactProjectManager instance; // 最初の呼び出し時にのみ初期化
  return instance;
 }

 void ArtifactProjectManager::loadFromFile(const QString& fullpath)
 {
  ArtifactProjectImporter importer;
  importer.setInputPath(fullpath);
  auto importResult = importer.importProject();

  if (!importResult.success || !importResult.project) {
   return;
  }

  impl_->currentProjectPtr_.reset();
  impl_->signalsConnected_ = false;
  impl_->currentProjectPtr_ = importResult.project;
  impl_->currentProjectPath_ = fullpath;

  if (impl_->currentProjectPtr_) {
   if (!impl_->signalsConnected_) {
    auto shared = impl_->currentProjectPtr_;
    std::weak_ptr<ArtifactProject> weakProj = shared;
    connect(shared.get(), &ArtifactProject::projectChanged, this, [weakProj, this]() {
     if (weakProj.lock()) projectChanged();
    });
    connect(shared.get(), &ArtifactProject::compositionCreated, this, [weakProj, this](const CompositionID& id) {
     if (weakProj.lock()) compositionCreated(id);
    });
    impl_->signalsConnected_ = true;
   }
   projectCreated();
  }
 }

 ArtifactProjectExporterResult ArtifactProjectManager::saveToFile(const QString& fullpath)
 {
  ArtifactProjectExporterResult result;
  result.success = false;

  auto projectPtr = impl_->currentProjectPtr_;
  if (!projectPtr || projectPtr->isNull()) {
   return result;
  }

  ArtifactProjectExporter exporter;
  exporter.setProject(projectPtr);
  exporter.setOutputPath(fullpath);
  result = exporter.exportProject();

  if (result.success) {
   impl_->currentProjectPath_ = fullpath;
  }
  return result;
 }

 QString ArtifactProjectManager::currentProjectPath() const
 {
  return impl_->currentProjectPath_;
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
  if (!impl_->currentProjectPtr_) return nullptr;
  
  auto projectItems = impl_->currentProjectPtr_->projectItems();
  CompositionID firstCompId;
  
  // Find the first composition item
  for (auto item : projectItems) {
    if (!item) continue;
    for (auto child : item->children) {
      if (child && child->type() == eProjectItemType::Composition) {
        CompositionItem* compItem = static_cast<CompositionItem*>(child);
        firstCompId = compItem->compositionId;
        break;
      }
    }
    if (!firstCompId.isNil()) break;
  }
  
  if (firstCompId.isNil()) return nullptr;
  
  auto findResult = impl_->currentProjectPtr_->findComposition(firstCompId);
  if (findResult.success && !findResult.ptr.expired()) {
    return findResult.ptr.lock();
  }
  
  return nullptr;
 }

 FindCompositionResult ArtifactProjectManager::findComposition(const CompositionID& id)
 {
  // shared_ptr の局所コピーを作成してスレッドセーフにアクセス
  // これにより、この関数実行中にポインタが無効化されることを防止
  auto projectPtr = impl_->currentProjectPtr_;

  if (!projectPtr) {
   FindCompositionResult result;
   return result;
  }

  return projectPtr->findComposition(id);

 }

 bool ArtifactProjectManager::isProjectClosed() const
 {
  return true;
 }

 int ArtifactProjectManager::compositionCount() const
 {
  // shared_ptr の局所コピーを作成してスレッドセーフにアクセス
  auto projectPtr = impl_->currentProjectPtr_;

  if (!projectPtr) return 0;

  auto projectItems = projectPtr->projectItems();
  int count = 0;
  for (auto item : projectItems) {
    if (!item) continue;
    // Count composition items in the tree
    std::function<void(ProjectItem*)> countCompositions = [&](ProjectItem* node) {
      if (!node) return;
      if (node->type() == eProjectItemType::Composition) {
        count++;
      }
      for (auto child : node->children) {
        countCompositions(child);
      }
    };
    countCompositions(item);
  }
  return count;
 }

  void ArtifactProjectManager::removeAllAssets()
  {
    if (!impl_->currentProjectPtr_) {
      qDebug() << "removeAllAssets: no current project";
      return;
    }
    
    // Access the project's implementation to remove assets
    // This is a placeholder - actual implementation would need to:
    // 1. Clear asset container
    // 2. Remove all asset items (FootageItem, etc.) from project tree
    // 3. Update UI/model
    
    qDebug() << "removeAllAssets: called (placeholder implementation)";
    // TODO: Implement proper asset removal logic
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
    qDebug() << "[addLayerToCurrentComposition] projectItems.size()=" << projectItems.size();
    
    CompositionID currentCompId;
    
    // Find the first composition item
    for (auto item : projectItems) {
     if (!item) {
      qDebug() << "[addLayerToCurrentComposition] projectItem is null";
      continue;
     }
     qDebug() << "[addLayerToCurrentComposition] projectItem children.size()=" << item->children.size();
     for (auto child : item->children) {
      if (child && child->type() == eProjectItemType::Composition) {
       CompositionItem* compItem = static_cast<CompositionItem*>(child);
       currentCompId = compItem->compositionId;
       qDebug() << "[addLayerToCurrentComposition] Found composition:" << currentCompId.toString();
       break;
      }
     }
     if (!currentCompId.isNil()) break;
    }
    
    qDebug() << "[addLayerToCurrentComposition] Final currentCompId.isNil()=" << currentCompId.isNil();
    
    if (currentCompId.isNil()) {
     result.success = false;
     qDebug() << "[addLayerToCurrentComposition] ERROR: No composition found!";
     return result;
    }

    result = currentProjectPtr_->createLayerAndAddToComposition(currentCompId, params);
    qDebug() << "[addLayerToCurrentComposition] createLayerAndAddToComposition result.success=" << result.success;
    
   
   return result;
  }

  bool ArtifactProjectManager::Impl::removeLayerFromComposition(const CompositionID& compositionId, const LayerID& layerId)
  {
    if (!currentProjectPtr_) return false;
    return currentProjectPtr_->removeLayerFromComposition(compositionId, layerId);
  }

   // ArtifactLayerResult ArtifactProjectManager::Impl::duplicateLayerInComposition(const CompositionID& compositionId, const LayerID& layerId)
   // {
   //   ArtifactLayerResult result;
   //   
   //   if (!currentProjectPtr_) {
   //     result.success = false;
   //     return result;
   //   }
   //   
   //   result = currentProjectPtr_->duplicateLayerInComposition(compositionId, layerId);
   //   
   //   return result;
   // }

   // CreateCompositionResult ArtifactProjectManager::Impl::duplicateComposition(const CompositionID& compositionId)
   // {
   //   CreateCompositionResult result;
   //   
   //   if (!currentProjectPtr_) {
   //     result.success = false;
   //     result.message.setQString("No project: cannot duplicate composition");
   //     qDebug() << "Impl::duplicateComposition failed: currentProjectPtr_ is null";
   //     return result;
   //   }
   //   
   //   result = currentProjectPtr_->duplicateComposition(compositionId);
   //   
   //   return result;
   // }

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

  bool ArtifactProjectManager::removeLayerFromComposition(const CompositionID& compositionId, const LayerID& layerId)
  {
    if (!impl_->currentProjectPtr_) return false;
    bool ok = impl_->currentProjectPtr_->removeLayerFromComposition(compositionId, layerId);
    if (ok) layerRemoved(layerId);
    return ok;
  }

  // ArtifactLayerResult ArtifactProjectManager::duplicateLayerInComposition(const CompositionID& compositionId, const LayerID& layerId)
  // {
  //   auto result = impl_->duplicateLayerInComposition(compositionId, layerId);
  //   if (result.success && result.layer) {
  //     layerCreated(result.layer->id());
  //   }
  //   return result;
  // }  // Duplicate method commented - see above
  // ArtifactLayerResult ArtifactProjectManager::duplicateLayerInComposition(const CompositionID& compositionId, const LayerID& layerId)
  // Already commented out above

  // CreateCompositionResult ArtifactProjectManager::duplicateComposition(const CompositionID& compositionId)
  // {
  //   auto result = impl_->duplicateComposition(compositionId);
  //   if (result.success) {
  //     qDebug() << "ArtifactProjectManager::duplicateComposition succeeded id:" << result.id.toString();
  //   } else {
  //     qDebug() << "ArtifactProjectManager::duplicateComposition failed";
  //   }
  //   return result;
  // }  // CreateCompositionResult ArtifactProjectManager::duplicateComposition(const CompositionID& compositionId)

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