module;
#include <QList>
#include <wobjectimpl.h>
#include <glm/ext/matrix_projection.hpp>
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QFile>
#include <QImage>
#include <QImageReader>
#include <QFutureWatcher>
#include <QRectF>
#include <QtSvg/QSvgRenderer>
#include <QtConcurrent>
module Artifact.Service.Project;

import std;

import Utils.String.UniString;
import Artifact.Project.Manager;
import Artifact.Layer.Factory;
import Artifact.Layer.Result;
import Artifact.Composition.Abstract;
import Artifact.Project.Items;
import Artifact.Layer.Image;
import Artifact.Layer.Svg;
import Artifact.Layer.Audio;
import Artifact.Layer.Video;
import Image.PSDDocument;
import File.TypeDetector;
import Artifact.Layers.Selection.Manager;
import Artifact.Application.Manager;
import Artifact.Render.Queue.Service;
//import Artifact.Render.FrameCache;

namespace Artifact
{
 namespace {
  QSize imageSizeForPath(const QString& path)
  {
   QImageReader reader(path);
   const QSize size = reader.size();
   if (size.isValid()) {
    return size;
   }

   const QString suffix = QFileInfo(path).suffix().toLower();
   if (suffix == QStringLiteral("svg")) {
    QSvgRenderer renderer(path);
    if (renderer.isValid()) {
     const QSize svgSize = renderer.defaultSize();
     if (svgSize.isValid() && !svgSize.isEmpty()) {
      return svgSize;
     }
     const QRectF viewBox = renderer.viewBoxF();
     if (viewBox.isValid() && viewBox.width() > 0.0 && viewBox.height() > 0.0) {
      return viewBox.size().toSize();
     }
    }
   }

   if (suffix == QStringLiteral("psd") || suffix == QStringLiteral("psb")) {
    ArtifactCore::PsdDocument doc;
    if (doc.open(path)) {
     const auto& header = doc.header();
     return QSize(static_cast<int>(header.width), static_cast<int>(header.height));
    }
   }

   return {};
  }

  void notifyProjectMutation(ArtifactProjectManager& manager)
  {
   if (auto project = manager.getCurrentProjectSharedPtr()) {
    project->projectChanged();
   }
  }
 }
	
class ArtifactProjectService::Impl
{
private:
	
	
public:
  Impl();
  ~Impl();
  static ArtifactProjectManager& projectManager();
  void installSelectionBridge(ArtifactProjectService* owner);
  void addLayerToCurrentComposition(const ArtifactLayerInitParams& params);
  void addAssetFromPath(const UniString& path);
  QStringList importAssetsFromPaths(const QStringList& sourcePaths);
  void importAssetsFromPathsAsync(const QStringList& sourcePaths, std::function<void(QStringList)> onFinished);
  void setPreviewQualityPreset(PreviewQualityPreset preset);
  PreviewQualityPreset previewQualityPreset() const;
  UniString projectName() const;
  void changeProjectName(const UniString& name);



  ArtifactCompositionWeakPtr currentComposition();
  FindCompositionResult findComposition(const CompositionID& id);
  ChangeCompositionResult changeCurrentComposition(const CompositionID& id);

  void removeAllAssets();
 PreviewQualityPreset qualityPreset_ = PreviewQualityPreset::Preview;
  CompositionID currentCompositionId_{};
  //ProgressiveRenderer progressiveRenderer_;

  void checkImportedAssetCompatibility(const QStringList& importedPaths);
  bool forwardingSelectionChange_ = false;
  LayerID lastForwardedLayerId_;
 };

 ArtifactProjectService::Impl::Impl()
 {

 }

// Impl::removeLayerFromComposition was removed; use manager call in service wrapper

ArtifactProjectService::Impl::~Impl()
{

}

void ArtifactProjectService::Impl::installSelectionBridge(ArtifactProjectService* owner)
{
 if (!owner) {
  return;
 }
 if (auto* app = ArtifactApplicationManager::instance()) {
  if (auto* selectionManager = app->layerSelectionManager()) {
   QObject::connect(selectionManager, &ArtifactLayerSelectionManager::selectionChanged, owner,
                    [this, owner, selectionManager]() {
                      if (forwardingSelectionChange_) {
                        return;
                      }
                      const auto current = selectionManager ? selectionManager->currentLayer() : ArtifactAbstractLayerPtr{};
                      const LayerID nextId = current ? current->id() : LayerID();
                      if (nextId == lastForwardedLayerId_) {
                        return;
                      }
                      lastForwardedLayerId_ = nextId;
                      owner->layerSelected(nextId);
                    });
  }
 }
}

 ArtifactProjectManager& ArtifactProjectService::Impl::projectManager()
 {
  return ArtifactProjectManager::getInstance();
 }

void ArtifactProjectService::Impl::addLayerToCurrentComposition(const ArtifactLayerInitParams& params)
{
  auto& manager = ArtifactProjectService::Impl::projectManager();
  LayerID selectedLayerId;
  if (auto* app = ArtifactApplicationManager::instance()) {
   if (auto* selectionManager = app->layerSelectionManager()) {
    if (auto selectedLayer = selectionManager->currentLayer()) {
     selectedLayerId = selectedLayer->id();
    }
   }
  }

  ArtifactLayerResult result;
  bool targetedCurrentComposition = false;
  if (auto comp = currentComposition().lock()) {
   result = manager.addLayerToComposition(comp->id(), const_cast<ArtifactLayerInitParams&>(params));
   targetedCurrentComposition = true;
  } else {
   result = manager.addLayerToCurrentComposition(const_cast<ArtifactLayerInitParams&>(params));
  }
  if (result.success && result.layer) {
   if (targetedCurrentComposition) {
    if (auto comp = currentComposition().lock()) {
    if (!selectedLayerId.isNil() && comp->containsLayerById(selectedLayerId)) {
     const auto allLayers = comp->allLayer();
     int selectedIndex = -1;
     int newLayerIndex = -1;
     for (int i = 0; i < allLayers.size(); ++i) {
      const auto& layer = allLayers[i];
      if (!layer) {
       continue;
      }
      if (layer->id() == selectedLayerId) {
       selectedIndex = i;
      }
      if (layer->id() == result.layer->id()) {
       newLayerIndex = i;
      }
     }

     if (selectedIndex >= 0 && newLayerIndex >= 0 && newLayerIndex != selectedIndex) {
      int targetIndex = selectedIndex;
      if (newLayerIndex < targetIndex) {
       targetIndex -= 1;
      }
      targetIndex = std::clamp(targetIndex, 0, std::max(0, static_cast<int>(allLayers.size()) - 1));
      comp->moveLayerToIndex(result.layer->id(), targetIndex);
     }

     const auto selectedLayer = comp->layerById(selectedLayerId);
     if (selectedLayer && selectedLayer->hasParent()) {
      result.layer->setParentById(selectedLayer->parentLayerId());
     }
    }
    }

    notifyProjectMutation(manager);
   }

   if (auto* service = ArtifactProjectService::instance()) {
    service->selectLayer(result.layer->id());
   }
  }
  qDebug() << "[ArtifactProjectService::Impl::addLayerToCurrentComposition] delegated to manager, result=" << result.success;
 }

 void ArtifactProjectService::Impl::addAssetFromPath(const UniString& path)
 {
    QStringList input;
    input.append(path.toQString());
    importAssetsFromPaths(input);
 }

QStringList ArtifactProjectService::Impl::importAssetsFromPaths(const QStringList& sourcePaths)
{
  QStringList importedPaths;
  if (sourcePaths.isEmpty()) {
   return importedPaths;
  }

  auto& manager = ArtifactProjectService::Impl::projectManager();
  QString assetsRoot = manager.currentProjectAssetsPath();
  QStringList toCopy;
  QStringList alreadyInProject;

  for (const auto& src : sourcePaths) {
   if (src.isEmpty()) continue;
   QFileInfo info(src);
   if (!info.exists() || !info.isFile()) continue;

   QString abs = info.absoluteFilePath();
   if (!assetsRoot.isEmpty() && abs.startsWith(assetsRoot, Qt::CaseInsensitive)) {
    alreadyInProject.append(abs);
   } else {
    toCopy.append(abs);
   }
  }

  QStringList copied = manager.copyFilesToProjectAssets(toCopy);
  importedPaths.append(copied);
  importedPaths.append(alreadyInProject);

  if (!importedPaths.isEmpty()) {
   manager.addAssetsFromFilePaths(importedPaths);
   // [Fix 2] importAssetsFromPathsAsync の非同期ブロック内で既に
   // QImageReader::size() によるチェックが完了しているため、
   // ここで同期実行すると同じファイルに対して 3 回 QImageReader が走る。
   // checkImportedAssetCompatibility(importedPaths);
  }

  return importedPaths;
 }

void ArtifactProjectService::Impl::importAssetsFromPathsAsync(
  const QStringList& sourcePaths,
  std::function<void(QStringList)> onFinished)
{
  auto* service = ArtifactProjectService::instance();
  if (!service) {
   if (onFinished) {
    onFinished({});
   }
   return;
  }

  auto& manager = ArtifactProjectService::Impl::projectManager();
  const QString assetsRoot = manager.currentProjectAssetsPath();
  QSize compSize;
  if (auto comp = this->currentComposition().lock()) {
    compSize = comp->settings().compositionSize();
  }

  auto* watcher = new QFutureWatcher<QStringList>(service);
  QObject::connect(watcher, &QFutureWatcher<QStringList>::finished, service,
                   [watcher, onFinished = std::move(onFinished)]() mutable {
    const QStringList importedPaths = watcher->result();
    watcher->deleteLater();
    if (onFinished) {
     onFinished(importedPaths);
    }
   });

  watcher->setFuture(QtConcurrent::run([sourcePaths, assetsRoot, compSize]() {
   QStringList importedPaths;
   if (sourcePaths.isEmpty()) {
    return importedPaths;
   }

   auto makeUniqueAssetPath = [](const QString& directory, const QString& fileName) {
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
      numbered = QStringLiteral("%1_%2").arg(baseName).arg(counter);
     }
     candidate = dir.filePath(extension.isEmpty() ? numbered : QStringLiteral("%1.%2").arg(numbered).arg(extension));
     ++counter;
    }

    return candidate;
   };

   ArtifactCore::FileTypeDetector detector;
   for (const auto& src : sourcePaths) {
    if (src.isEmpty()) continue;

    QFileInfo info(src);
    if (!info.exists() || !info.isFile()) continue;

    const QString abs = info.absoluteFilePath();
    if (!assetsRoot.isEmpty() && abs.startsWith(assetsRoot, Qt::CaseInsensitive)) {
     importedPaths.append(abs);
    } else {
     if (assetsRoot.isEmpty()) continue;
     const QString finalFile = makeUniqueAssetPath(assetsRoot, QFileInfo(abs).fileName());
     if (finalFile.isEmpty()) continue;
     if (!QFile::copy(abs, finalFile)) {
      continue;
     }
     importedPaths.append(finalFile);
    }

    if (!importedPaths.isEmpty() && detector.detect(importedPaths.back()) == ArtifactCore::FileType::Image) {
     const QSize imageSize = imageSizeForPath(importedPaths.back());
     if (imageSize.isValid() && compSize.isValid() &&
         (imageSize.width() != compSize.width() || imageSize.height() != compSize.height())) {
      qWarning() << "[CompatibilityGuard] Image resolution differs from composition. image="
                 << imageSize.width() << "x" << imageSize.height()
                 << " comp=" << compSize.width() << "x" << compSize.height()
                 << " path=" << importedPaths.back();
     }
    }
   }

   return importedPaths;
  }));
 }

 void ArtifactProjectService::Impl::checkImportedAssetCompatibility(const QStringList& importedPaths)
 {
  if (importedPaths.isEmpty()) return;

  QSize compSize;
  if (auto comp = currentComposition().lock()) {
   compSize = comp->settings().compositionSize();
  }

  ArtifactCore::FileTypeDetector detector;
  for (const auto& path : importedPaths) {
   if (path.isEmpty()) continue;

   auto type = detector.detect(path);
   if (type == ArtifactCore::FileType::Unknown) {
    qWarning() << "[CompatibilityGuard] Unknown/unsupported file type:" << path;
    continue;
   }

   if (type == ArtifactCore::FileType::Image) {
    const QSize imageSize = imageSizeForPath(path);
    if (!imageSize.isValid()) {
     qWarning() << "[CompatibilityGuard] Image size unavailable:" << path;
     continue;
    }
    if (compSize.width() > 0 && compSize.height() > 0 &&
        (imageSize.width() != compSize.width() || imageSize.height() != compSize.height())) {
      qWarning() << "[CompatibilityGuard] Image resolution differs from composition. image="
                 << imageSize.width() << "x" << imageSize.height()
                 << " comp=" << compSize.width() << "x" << compSize.height()
                 << " path=" << path;
    }
   }
  }
 }

 void ArtifactProjectService::Impl::setPreviewQualityPreset(PreviewQualityPreset preset)
 {
  qualityPreset_ = preset;
  switch (preset) {
  case PreviewQualityPreset::Draft:
   //progressiveRenderer_.setQuality(RenderQuality::Draft);
   //progressiveRenderer_.setDraftQuality(4);
   //progressiveRenderer_.setPreviewQuality(2);
   break;
  case PreviewQualityPreset::Preview:
   //progressiveRenderer_.setQuality(RenderQuality::Preview);
   //progressiveRenderer_.setDraftQuality(4);
   //progressiveRenderer_.setPreviewQuality(2);
   break;
  case PreviewQualityPreset::Final:
   //progressiveRenderer_.setQuality(RenderQuality::Final);
   //progressiveRenderer_.setDraftQuality(2);
   //progressiveRenderer_.setPreviewQuality(1);
   break;
  }
 }

 PreviewQualityPreset ArtifactProjectService::Impl::previewQualityPreset() const
 {
  return qualityPreset_;
 }

 UniString ArtifactProjectService::Impl::projectName() const
 {
  	
  return UniString();
 }

 void ArtifactProjectService::Impl::changeProjectName(const UniString& name)
 {
  	

 }

 ChangeCompositionResult ArtifactProjectService::Impl::changeCurrentComposition(const CompositionID& id)
 {
  ChangeCompositionResult result;
  if (id.isNil()) {
   result.success = false;
   result.message.setQString(QStringLiteral("Invalid composition id"));
   return result;
  }

  auto find = projectManager().findComposition(id);
  if (!find.success || find.ptr.expired()) {
   result.success = false;
   result.message.setQString(QStringLiteral("Composition not found"));
   return result;
  }

  currentCompositionId_ = id;
  if (auto comp = find.ptr.lock()) {
   if (auto* app = ArtifactApplicationManager::instance()) {
    if (auto* active = app->activeContextService()) {
     active->setActiveComposition(comp);
    }
   }
  }

  result.success = true;
  return result;
 }

 void ArtifactProjectService::Impl::removeAllAssets()
 {

 }

 FindCompositionResult ArtifactProjectService::Impl::findComposition(const CompositionID& id)
 {
  return ArtifactProjectManager::getInstance().findComposition(id);
 }

 ArtifactCompositionWeakPtr ArtifactProjectService::Impl::currentComposition()
 {
  if (!currentCompositionId_.isNil()) {
   auto found = projectManager().findComposition(currentCompositionId_);
   if (found.success && !found.ptr.expired()) {
    return found.ptr;
   }
   currentCompositionId_ = {};
  }

  auto fallback = projectManager().currentComposition();
  if (fallback) {
   currentCompositionId_ = fallback->id();
   return fallback;
  }

  return {};
 }

 W_OBJECT_IMPL(ArtifactProjectService)
	
ArtifactProjectService::ArtifactProjectService(QObject*parent):QObject(parent),impl_(new Impl())
{
  connect(&impl_->projectManager(),&ArtifactProjectManager::projectCreated,this,[this]() {
   impl_->currentCompositionId_ = {};
   projectCreated();
  });
 connect(&impl_->projectManager(), &ArtifactProjectManager::compositionCreated, this, [this](const CompositionID& id) {
   if (impl_->currentCompositionId_.isNil()) {
    changeCurrentComposition(id);
   }
   compositionCreated(id);
  });
  connect(&impl_->projectManager(), &ArtifactProjectManager::layerCreated, this, &ArtifactProjectService::layerCreated);
  connect(&impl_->projectManager(), &ArtifactProjectManager::projectChanged, this, &ArtifactProjectService::projectChanged);
  impl_->installSelectionBridge(this);
   
   
 }

 ArtifactProjectService::~ArtifactProjectService()
 {
  delete impl_;
 }

 ArtifactProjectService* ArtifactProjectService::instance()
 {
  static ArtifactProjectService service;
  return&service;


 }

 bool ArtifactProjectService::hasProject() const
 {
  return impl_->projectManager().getCurrentProjectSharedPtr() != nullptr;
 }

 void ArtifactProjectService::projectSettingChanged(const ArtifactProjectSettings& setting)
 {

 }

void ArtifactProjectService::selectLayer(const LayerID& id)
{
  if (auto* app = ArtifactApplicationManager::instance()) {
    if (auto* selectionManager = app->layerSelectionManager()) {
     const auto current = selectionManager->currentLayer();
     if (current && current->id() == id) {
      selectionManager->setActiveComposition(currentComposition().lock());
      layerSelected(id);
      return;
     }
     impl_->forwardingSelectionChange_ = true;
     selectionManager->setActiveComposition(currentComposition().lock());
     if (id.isNil()) {
      selectionManager->clearSelection();
    } else if (auto comp = currentComposition().lock()) {
      selectionManager->selectLayer(comp->layerById(id));
    } else {
      selectionManager->clearSelection();
    }
     impl_->forwardingSelectionChange_ = false;
   }
  }
  layerSelected(id);
}

 void ArtifactProjectService::addLayer(const CompositionID& id, const ArtifactLayerInitParams& param)
 {
  auto result = impl_->projectManager().addLayerToComposition(id, const_cast<ArtifactLayerInitParams&>(param));
  if (result.success) {
   notifyProjectMutation(impl_->projectManager());
  }
 }

void ArtifactProjectService::addLayerToCurrentComposition(const ArtifactLayerInitParams& params)
{
 impl_->addLayerToCurrentComposition(params);
}

bool ArtifactProjectService::removeLayerFromComposition(const CompositionID& compositionId, const LayerID& layerId)
{
    bool ok = impl_->projectManager().removeLayerFromComposition(compositionId, layerId);
    if (ok) {
        // [Optimization] Only emit layerRemoved. notifyProjectMutation triggers global projectChanged 
        // which causes heavy UI rebuilds. Most widgets handle layerRemoved specifically.
        layerRemoved(compositionId, layerId);
    }
    return ok;
}
bool ArtifactProjectService::moveLayerInCurrentComposition(const LayerID& layerId, int newIndex)
{
    auto comp = currentComposition().lock();
    if (!comp || layerId.isNil()) {
        return false;
    }

    const auto layers = comp->allLayer();
    if (layers.isEmpty()) {
        return false;
    }

    const int lastIndex = static_cast<int>(layers.size()) - 1;
    const int clampedIndex = std::clamp<int>(newIndex, 0, lastIndex);
    comp->moveLayerToIndex(layerId, clampedIndex);
    notifyProjectMutation(impl_->projectManager());
    return true;
}

bool ArtifactProjectService::duplicateLayerInCurrentComposition(const LayerID& layerId)
{
    auto comp = currentComposition().lock();
    if (!comp || layerId.isNil()) {
        return false;
    }

    auto result = impl_->projectManager().duplicateLayerInComposition(comp->id(), layerId);
    return result.success;
}

bool ArtifactProjectService::renameLayerInCurrentComposition(const LayerID& layerId, const QString& newName)
{
    auto comp = currentComposition().lock();
    if (!comp || layerId.isNil()) {
        return false;
    }

    auto layer = comp->layerById(layerId);
    if (!layer) {
        return false;
    }

    const QString trimmed = newName.trimmed();
    if (trimmed.isEmpty()) {
        return false;
    }
    layer->setLayerName(trimmed);
    notifyProjectMutation(impl_->projectManager());
    return true;
}

bool ArtifactProjectService::replaceLayerSourceInCurrentComposition(const LayerID& layerId, const QString& sourcePath)
{
    auto comp = currentComposition().lock();
    if (!comp || layerId.isNil()) {
        return false;
    }

    auto layer = comp->layerById(layerId);
    if (!layer) {
        return false;
    }

    const QString trimmed = sourcePath.trimmed();
    if (trimmed.isEmpty()) {
        return false;
    }

    bool replaced = false;
    if (auto imageLayer = std::dynamic_pointer_cast<ArtifactImageLayer>(layer)) {
        replaced = imageLayer->loadFromPath(trimmed);
    } else if (auto svgLayer = std::dynamic_pointer_cast<ArtifactSvgLayer>(layer)) {
        replaced = svgLayer->loadFromPath(trimmed);
    } else if (auto audioLayer = std::dynamic_pointer_cast<ArtifactAudioLayer>(layer)) {
        replaced = audioLayer->loadFromPath(trimmed);
    } else if (auto videoLayer = std::dynamic_pointer_cast<ArtifactVideoLayer>(layer)) {
        replaced = videoLayer->loadFromPath(trimmed);
    }

    if (!replaced) {
        return false;
    }

    notifyProjectMutation(impl_->projectManager());
    return true;
}

bool ArtifactProjectService::isLayerVisibleInCurrentComposition(const LayerID& layerId)
{
    auto comp = currentComposition().lock();
    if (!comp || layerId.isNil()) {
        return false;
    }
    auto layer = comp->layerById(layerId);
    return layer ? layer->isVisible() : false;
}

bool ArtifactProjectService::isLayerLockedInCurrentComposition(const LayerID& layerId)
{
    auto comp = currentComposition().lock();
    if (!comp || layerId.isNil()) {
        return false;
    }
    auto layer = comp->layerById(layerId);
    return layer ? layer->isLocked() : false;
}

bool ArtifactProjectService::isLayerSoloInCurrentComposition(const LayerID& layerId)
{
    auto comp = currentComposition().lock();
    if (!comp || layerId.isNil()) {
        return false;
    }
    auto layer = comp->layerById(layerId);
    return layer ? layer->isSolo() : false;
}

bool ArtifactProjectService::isLayerShyInCurrentComposition(const LayerID& layerId)
{
    auto comp = currentComposition().lock();
    if (!comp || layerId.isNil()) {
        return false;
    }
    auto layer = comp->layerById(layerId);
    return layer ? layer->isShy() : false;
}

bool ArtifactProjectService::setLayerVisibleInCurrentComposition(const LayerID& layerId, bool visible)
{
    auto comp = currentComposition().lock();
    if (!comp || layerId.isNil()) {
        return false;
    }
    auto layer = comp->layerById(layerId);
    if (!layer) {
        return false;
    }
    layer->setVisible(visible);
    return true;
}

bool ArtifactProjectService::setLayerLockedInCurrentComposition(const LayerID& layerId, bool locked)
{
    auto comp = currentComposition().lock();
    if (!comp || layerId.isNil()) {
        return false;
    }
    auto layer = comp->layerById(layerId);
    if (!layer) {
        return false;
    }
    layer->setLocked(locked);
    return true;
}

bool ArtifactProjectService::setLayerSoloInCurrentComposition(const LayerID& layerId, bool solo)
{
    auto comp = currentComposition().lock();
    if (!comp || layerId.isNil()) {
        return false;
    }
    auto layer = comp->layerById(layerId);
    if (!layer) {
        return false;
    }
    layer->setSolo(solo);
    return true;
}

bool ArtifactProjectService::setLayerShyInCurrentComposition(const LayerID& layerId, bool shy)
{
    auto comp = currentComposition().lock();
    if (!comp || layerId.isNil()) {
        return false;
    }
    auto layer = comp->layerById(layerId);
    if (!layer) {
        return false;
    }
    layer->setShy(shy);
    notifyProjectMutation(impl_->projectManager());
    return true;
}

bool ArtifactProjectService::soloOnlyLayerInCurrentComposition(const LayerID& layerId)
{
    auto comp = currentComposition().lock();
    if (!comp || layerId.isNil()) {
        return false;
    }
    auto selected = comp->layerById(layerId);
    if (!selected) {
        return false;
    }

    for (const auto& candidate : comp->allLayer()) {
        if (!candidate) continue;
        candidate->setSolo(candidate->id() == layerId);
    }
    notifyProjectMutation(impl_->projectManager());
    return true;
}

bool ArtifactProjectService::setLayerParentInCurrentComposition(const LayerID& layerId, const LayerID& parentLayerId)
{
    auto comp = currentComposition().lock();
    if (!comp || layerId.isNil()) {
        return false;
    }
    auto layer = comp->layerById(layerId);
    if (!layer) {
        return false;
    }
    if (parentLayerId.isNil()) {
        layer->clearParent();
    } else {
        auto parent = comp->layerById(parentLayerId);
        if (!parent || parent->id() == layerId) {
            return false;
        }
        layer->setParentById(parentLayerId);
    }
    notifyProjectMutation(impl_->projectManager());
    return true;
}

bool ArtifactProjectService::precomposeLayersInCurrentComposition(const QVector<LayerID>& layerIds, const UniString& newCompositionName)
{
    // TODO: Implement precompose (pre-render selected layers into a new composition)
    qWarning() << "[ArtifactProjectService] precomposeLayersInCurrentComposition is not yet implemented";
    Q_UNUSED(layerIds);
    Q_UNUSED(newCompositionName);
    return false;
}

void ArtifactProjectService::splitLayerAtCurrentTime(const CompositionID& compositionId, const LayerID& layerId)
{
    auto comp = findComposition(compositionId).ptr.lock();
    if (!comp || layerId.isNil()) {
        return;
    }

    auto layer = comp->layerById(layerId);
    if (!layer) {
        return;
    }

    const auto now = comp->framePosition();
    const qint64 nowFrame = now.framePosition();
    const qint64 inFrame = layer->inPoint().framePosition();
    const qint64 outFrame = layer->outPoint().framePosition();
    if (nowFrame <= inFrame || nowFrame >= outFrame) {
        return;
    }

    const auto oldOut = layer->outPoint();
    layer->setOutPoint(now);

    auto result = impl_->projectManager().duplicateLayerInComposition(compositionId, layerId);
    if (!result.success || !result.layer) {
        layer->setOutPoint(oldOut);
        return;
    }

    auto newLayer = result.layer;
    newLayer->setInPoint(now);
    newLayer->setOutPoint(oldOut);
    projectChanged();
}

bool ArtifactProjectService::clearLayerParentInCurrentComposition(const LayerID& layerId)
{
    auto comp = currentComposition().lock();
    if (!comp || layerId.isNil()) {
        return false;
    }
    auto layer = comp->layerById(layerId);
    if (!layer) {
        return false;
    }
    layer->clearParent();
    notifyProjectMutation(impl_->projectManager());
    return true;
}

bool ArtifactProjectService::layerHasParentInCurrentComposition(const LayerID& layerId)
{
    auto comp = currentComposition().lock();
    if (!comp || layerId.isNil()) {
        return false;
    }
    auto layer = comp->layerById(layerId);
    return layer ? layer->hasParent() : false;
}

LayerID ArtifactProjectService::layerParentIdInCurrentComposition(const LayerID& layerId)
{
    auto comp = currentComposition().lock();
    if (!comp || layerId.isNil()) {
        return {};
    }
    auto layer = comp->layerById(layerId);
    if (!layer || !layer->hasParent()) {
        return {};
    }
    return layer->parentLayerId();
}

QString ArtifactProjectService::layerNameInCurrentComposition(const LayerID& layerId)
{
    auto comp = currentComposition().lock();
    if (!comp || layerId.isNil()) {
        return QString();
    }
    auto layer = comp->layerById(layerId);
    return layer ? layer->layerName() : QString();
}

bool ArtifactProjectService::addEffectToLayerInCurrentComposition(const LayerID& layerId, std::shared_ptr<ArtifactAbstractEffect> effect)
{
    auto comp = currentComposition().lock();
    if (!comp || layerId.isNil() || !effect) {
        return false;
    }
    auto layer = comp->layerById(layerId);
    if (!layer) {
        return false;
    }
    layer->addEffect(effect);
    Q_EMIT layer->changed();
    Q_EMIT projectChanged();
    return true;
}

bool ArtifactProjectService::removeEffectFromLayerInCurrentComposition(const LayerID& layerId, const QString& effectId)
{
    if (effectId.trimmed().isEmpty()) {
        return false;
    }
    auto comp = currentComposition().lock();
    if (!comp || layerId.isNil()) {
        return false;
    }
    auto layer = comp->layerById(layerId);
    if (!layer) {
        return false;
    }
    layer->removeEffect(UniString(effectId.toStdString()));
    Q_EMIT layer->changed();
    Q_EMIT projectChanged();
    return true;
}

bool ArtifactProjectService::setEffectEnabledInLayerInCurrentComposition(const LayerID& layerId, const QString& effectId, bool enabled)
{
    if (effectId.trimmed().isEmpty()) {
        return false;
    }
    auto comp = currentComposition().lock();
    if (!comp || layerId.isNil()) {
        return false;
    }
    auto layer = comp->layerById(layerId);
    if (!layer) {
        return false;
    }

    for (const auto& effect : layer->getEffects()) {
        if (effect && effect->effectID().toQString() == effectId) {
            effect->setEnabled(enabled);
            Q_EMIT layer->changed();
            Q_EMIT projectChanged();
            return true;
        }
    }
    return false;
}

bool ArtifactProjectService::moveEffectInLayerInCurrentComposition(const LayerID& layerId, const QString& effectId, int direction)
{
    if (effectId.trimmed().isEmpty() || direction == 0) {
        return false;
    }
    auto comp = currentComposition().lock();
    if (!comp || layerId.isNil()) {
        return false;
    }
    auto layer = comp->layerById(layerId);
    if (!layer) {
        return false;
    }

    auto effects = layer->getEffects();
    if (effects.empty()) {
        return false;
    }

    int currentIndex = -1;
    EffectPipelineStage currentStage = EffectPipelineStage::Generator;
    for (int i = 0; i < static_cast<int>(effects.size()); ++i) {
        const auto& effect = effects[i];
        if (effect && effect->effectID().toQString() == effectId) {
            currentIndex = i;
            currentStage = effect->pipelineStage();
            break;
        }
    }
    if (currentIndex < 0) {
        return false;
    }

    int swapIndex = -1;
    if (direction < 0) {
        for (int i = currentIndex - 1; i >= 0; --i) {
            if (effects[i] && effects[i]->pipelineStage() == currentStage) {
                swapIndex = i;
                break;
            }
        }
    } else {
        for (int i = currentIndex + 1; i < static_cast<int>(effects.size()); ++i) {
            if (effects[i] && effects[i]->pipelineStage() == currentStage) {
                swapIndex = i;
                break;
            }
        }
    }

    if (swapIndex < 0 || swapIndex == currentIndex) {
        return false;
    }

    std::swap(effects[currentIndex], effects[swapIndex]);
    layer->clearEffects();
    for (const auto& effect : effects) {
        if (effect) {
            layer->addEffect(effect);
        }
    }
    Q_EMIT layer->changed();
    Q_EMIT projectChanged();
    return true;
}

QString ArtifactProjectService::layerRemovalConfirmationMessage(const CompositionID& compositionId, const LayerID& layerId) const
{  
  if (compositionId.isNil() || layerId.isNil()) {
    return QStringLiteral("このレイヤーを削除しますか？");
  }

  auto findResult = impl_->projectManager().findComposition(compositionId);
  if (!findResult.success) {
    return QStringLiteral("このレイヤーを削除しますか？");
  }
  auto comp = findResult.ptr.lock();
  if (!comp) {
    return QStringLiteral("このレイヤーを削除しますか？");
  }

  auto layer = comp->layerById(layerId);
  if (!layer) {
    return QStringLiteral("このレイヤーを削除しますか？");
  }

  int childCount = 0;
  for (const auto& candidate : comp->allLayer()) {
    if (!candidate) continue;
    if (candidate->parentLayerId() == layerId) {
      ++childCount;
    }
  }
  const int effectCount = layer->effectCount();
  const QString layerName = layer->layerName().trimmed().isEmpty()
    ? QStringLiteral("(Unnamed)")
    : layer->layerName().trimmed();

  if (childCount <= 0 && effectCount <= 0) {
    return QStringLiteral("レイヤー \"%1\" を削除しますか？").arg(layerName);
  }
  return QStringLiteral(
    "レイヤー \"%1\" を削除しますか？\n"
    "子レイヤー: %2 / エフェクト: %3\n"
    "この操作は元に戻せない場合があります。")
    .arg(layerName)
    .arg(childCount)
    .arg(effectCount);
}

bool ArtifactProjectService::removeProjectItem(ProjectItem* item)
{
  if (!item) {
    return false;
  }
  if (item->type() == eProjectItemType::Composition) {
    auto* compItem = static_cast<CompositionItem*>(item);
    return removeCompositionWithRenderQueueCleanup(compItem->compositionId);
  }

  auto shared = getCurrentProjectSharedPtr();
  if (!shared) {
    return false;
  }
  shared->removeItem(item);
  return true;
}

bool ArtifactProjectService::moveProjectItem(ProjectItem* item, ProjectItem* newParent)
{
  if (!item || !newParent) {
    return false;
  }
  auto shared = getCurrentProjectSharedPtr();
  if (!shared) {
    return false;
  }
  return shared->moveItem(item, newParent);
}

QString ArtifactProjectService::projectItemRemovalConfirmationMessage(ProjectItem* item) const
{
  if (!item) {
    return QStringLiteral("この項目を削除しますか？");
  }
  if (item->type() == eProjectItemType::Composition) {
    auto* compItem = static_cast<CompositionItem*>(item);
    return compositionRemovalConfirmationMessage(compItem->compositionId);
  }
  if (item->type() == eProjectItemType::Footage) {
    return QStringLiteral("フッテージ項目を削除しますか？\n（元ファイル自体は削除されません）");
  }
  if (item->type() == eProjectItemType::Folder) {
    return QStringLiteral("フォルダ項目を削除しますか？\n（子項目も同時に削除されます）");
  }
  return QStringLiteral("この項目を削除しますか？");
}

 bool ArtifactProjectService::removeComposition(const CompositionID& id)
 {
   auto& pm = impl_->projectManager();
   auto projectShared = pm.getCurrentProjectSharedPtr();
   if (!projectShared) return false;
   bool ok = projectShared->removeCompositionById(id);
   if (ok) {
    compositionRemoved(id);
    projectShared->projectChanged();
   }
   return ok;
 }

 int ArtifactProjectService::renderQueueCountForComposition(const CompositionID& id) const
 {
  auto* queueService = ArtifactRenderQueueService::instance();
  return queueService ? queueService->renderQueueCountForComposition(id) : 0;
 }

 QString ArtifactProjectService::compositionRemovalConfirmationMessage(const CompositionID& id) const
 {
  const int queuedCount = renderQueueCountForComposition(id);
  if (queuedCount <= 0) {
   return QStringLiteral("このコンポジションを削除しますか？");
  }
  return QStringLiteral(
   "このコンポジションはレンダーキューに %1 件登録されています。\n"
   "削除すると該当キューも削除されます。\n"
   "続行しますか？").arg(queuedCount);
 }

 bool ArtifactProjectService::removeCompositionWithRenderQueueCleanup(const CompositionID& id, int* removedQueueCount)
 {
  int queuedCount = renderQueueCountForComposition(id);
  if (removedQueueCount) {
   *removedQueueCount = queuedCount;
  }
  if (queuedCount > 0) {
   if (auto* queueService = ArtifactRenderQueueService::instance()) {
    queueService->removeRenderQueuesForComposition(id);
   }
  }
  return removeComposition(id);
 }

 bool ArtifactProjectService::duplicateComposition(const CompositionID& id)
 {
  auto result = impl_->projectManager().duplicateComposition(id);
  if (!result.success) {
   return false;
  }
  changeCurrentComposition(result.id);
  return true;
 }

 bool ArtifactProjectService::renameComposition(const CompositionID& id, const UniString& name)
 {
   auto& pm = impl_->projectManager();
   auto projectShared = pm.getCurrentProjectSharedPtr();
   if (!projectShared) return false;
   if (auto comp = pm.findComposition(id).ptr.lock()) {
     comp->setCompositionName(name);
   }
   auto items = projectShared->projectItems();
   for (auto root : items) {
     if (!root) continue;
     for (auto c : root->children) {
       if (!c) continue;
       if (c->type() == eProjectItemType::Composition) {
         CompositionItem* ci = static_cast<CompositionItem*>(c);
         if (ci->compositionId == id) {
           ci->name = name;
           projectShared->projectChanged();
           return true;
         }
       }
     }
   }
   return false;
 }

 UniString ArtifactProjectService::projectName() const
 {
 	
  return impl_->projectName();
 }

 void ArtifactProjectService::changeProjectName(const UniString& string)
 {
  impl_->changeProjectName(string);
 }

 void ArtifactProjectService::addAssetFromPath(const UniString& path)
 {
  impl_->addAssetFromPath(path);
 }

 QStringList ArtifactProjectService::importAssetsFromPaths(const QStringList& sourcePaths)
 {
  return impl_->importAssetsFromPaths(sourcePaths);
 }

 void ArtifactProjectService::importAssetsFromPathsAsync(
  const QStringList& sourcePaths,
  std::function<void(QStringList)> onFinished)
 {
  impl_->importAssetsFromPathsAsync(sourcePaths, std::move(onFinished));
 }

 ArtifactCompositionWeakPtr ArtifactProjectService::currentComposition()
 {

  return impl_->currentComposition();
 }

std::shared_ptr<ArtifactProject> ArtifactProjectService::getCurrentProjectSharedPtr() const
{
    return impl_->projectManager().getCurrentProjectSharedPtr();
}

ChangeCompositionResult ArtifactProjectService::changeCurrentComposition(const CompositionID& id)
{
  auto result = impl_->changeCurrentComposition(id);
  if (result.success) {
   currentCompositionChanged(id);
  }
  return result;
}

FindCompositionResult ArtifactProjectService::findComposition(const CompositionID& id)
 {
 return impl_->findComposition(id);
 }

QVector<ProjectItem*> ArtifactProjectService::projectItems() const
{ // truncated for brevity
 return impl_->projectManager().getCurrentProjectSharedPtr() ? impl_->projectManager().getCurrentProjectSharedPtr()->projectItems() : QVector<ProjectItem*>();
}

void ArtifactProjectService::createComposition(const UniString& name)
{
 auto& manager = impl_->projectManager();
 if (!hasProject()) {
  manager.createProject();
 }
 auto result = manager.createComposition(name);
 if (result.success) {
  changeCurrentComposition(result.id);
  qDebug() << "[ArtifactProjectService::createComposition(UniString)] succeeded, id:" << result.id.toString();
 } else {
  qDebug() << "[ArtifactProjectService::createComposition(UniString)] failed";
 }
}

void ArtifactProjectService::createComposition(const ArtifactCompositionInitParams& params)
{
 auto& manager = impl_->projectManager();
 if (!hasProject()) {
  manager.createProject();
 }
 auto result = manager.createComposition(params);
 if (result.success) {
  changeCurrentComposition(result.id);
  qDebug() << "[ArtifactProjectService::createComposition] succeeded, id:" << result.id.toString();
 } else {
  qDebug() << "[ArtifactProjectService::createComposition] failed";
 }
}

void ArtifactProjectService::createProject(const ArtifactProjectSettings& setting)
{
 auto& manager = impl_->projectManager();
 manager.createProject(setting.projectName());
}

void ArtifactProjectService::removeAllAssets()
{
 //removeall assets via projectmanager instance

 impl_->projectManager().removeAllAssets();
  
  
}

void ArtifactProjectService::setPreviewQualityPreset(PreviewQualityPreset preset)
{
 impl_->setPreviewQualityPreset(preset);
 previewQualityPresetChanged(preset);
}

PreviewQualityPreset ArtifactProjectService::previewQualityPreset() const
{
 return impl_->previewQualityPreset();
}

};

//W_REGISTER_ARGTYPE(ArtifactCore::CompositionID)
