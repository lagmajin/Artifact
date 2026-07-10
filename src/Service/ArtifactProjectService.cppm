module;
#include <algorithm>
#include <deque>
#include <QApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QFutureWatcher>
#include <QInputDialog>
#include <QImage>
#include <QImageReader>
#include <QList>
#include <QMessageBox>
#include <QPointer>
#include <QRectF>
#include <QSet>
#include <QVector3D>
#include <QtConcurrent>
#include <QtSvg/QSvgRenderer>
#include <QFileSystemWatcher>
#include <QTimer>
#include <glm/ext/matrix_projection.hpp>
#include <wobjectimpl.h>

module Artifact.Service.Project;

import std;

import Utils.String.UniString;
import Artifact.Layer.Composition;
import Artifact.Project.Manager;
import Artifact.Render.Queue.Service;
import Artifact.Project.RevisionService;
import Artifact.Project.CreationDefaults;
import Artifact.Layer.Factory;
import Artifact.Layer.Result;
import Artifact.Composition.Abstract;
import Artifact.Project.Items;
import Artifact.Effect.Abstract;
import Artifact.Layer.Image;
import Artifact.Layer.Svg;
import Artifact.Layer.Audio;
import Artifact.Layer.Video;
import Artifact.Layer.Group;
import Artifact.Project.Health;
import Asset.Sequence;
import Artifact.Diagnostics.AppValidationRules;
import Core.Diagnostics.DiagnosticEngine;
import Image.PSDDocument;
import File.TypeDetector;
import Artifact.Layers.Selection.Manager;
import Event.Bus;
import Artifact.Event.Types;
import Core.Diagnostics.SessionLedger;
import MediaSource;
import Core.Diagnostics.ProjectDiagnostic;
import Artifact.Service.ActiveContext;
import Artifact.Service.Playback;
import Artifact.Audio.ScrubController;
import Undo.UndoManager;
// import Artifact.Render.FrameCache;

namespace Artifact {
namespace {
QString slugifyEffectId(const QString &text) {
  QString slug;
  slug.reserve(text.size());
  bool lastWasDash = false;
  for (const QChar ch : text.trimmed().toLower()) {
    if (ch.isLetterOrNumber()) {
      slug.append(ch);
      lastWasDash = false;
    } else if (!slug.isEmpty() && !lastWasDash) {
      slug.append(QChar('-'));
      lastWasDash = true;
    }
  }
  while (slug.endsWith(QChar('-'))) {
    slug.chop(1);
  }
  if (slug.isEmpty()) {
    slug = QStringLiteral("effect");
  }
  return slug;
}

QString uniqueEffectIdForLayer(
    const std::vector<std::shared_ptr<ArtifactAbstractEffect>> &effects,
    const QString &displayName, const QString &preferredId) {
  QString baseId = preferredId.trimmed();
  if (baseId.isEmpty()) {
    baseId = slugifyEffectId(displayName);
  }
  if (baseId.isEmpty()) {
    baseId = QStringLiteral("effect");
  }

  auto idExists = [&effects](const QString &candidate) {
    return std::any_of(
        effects.begin(), effects.end(),
        [&candidate](const std::shared_ptr<ArtifactAbstractEffect> &effect) {
          return effect && effect->effectID().toQString() == candidate;
        });
  };

  QString uniqueId = baseId;
  int suffix = 2;
  while (idExists(uniqueId)) {
    uniqueId = QStringLiteral("%1-%2").arg(baseId).arg(suffix++);
  }
  return uniqueId;
}

void collectSmartSoloLayerIds(const ArtifactCompositionPtr& comp,
                              const ArtifactAbstractLayerPtr& layer,
                              QSet<QString>& visited,
                              QSet<QString>& included)
{
  if (!comp || !layer) {
    return;
  }

  const LayerID layerId = layer->id();
  if (layerId.isNil()) {
    return;
  }

  const QString layerKey = layerId.toString();
  if (visited.contains(layerKey)) {
    return;
  }
  visited.insert(layerKey);
  included.insert(layerKey);

  const LayerID parentId = layer->parentLayerId();
  if (!parentId.isNil()) {
    auto parent = comp->layerById(parentId);
    if (parent) {
      collectSmartSoloLayerIds(comp, parent, visited, included);
    }
  }

  for (const auto& matteRef : layer->matteReferences()) {
    if (!matteRef.enabled || matteRef.sourceLayerId.isNil()) {
      continue;
    }
    auto matteSource = comp->layerById(matteRef.sourceLayerId);
    if (matteSource) {
      collectSmartSoloLayerIds(comp, matteSource, visited, included);
    }
  }
}

QSet<QString> resolveSmartSoloLayerIds(const ArtifactCompositionPtr& comp,
                                      const LayerID& layerId)
{
  QSet<QString> included;
  if (!comp || layerId.isNil()) {
    return included;
  }

  auto layer = comp->layerById(layerId);
  if (!layer) {
    return included;
  }

  QSet<QString> visited;
  collectSmartSoloLayerIds(comp, layer, visited, included);
  return included;
}

struct SequenceImportGroup {
  QString representativePath;
  QStringList sequencePaths;
};

bool isImageImportCandidate(const QString &path)
{
  ArtifactCore::FileTypeDetector detector;
  return detector.detectByExtension(path) == ArtifactCore::FileType::Image;
}

QVector<SequenceImportGroup> detectSequenceImportGroups(
    const QStringList &importedPaths,
    QSet<QString> *consumedPaths = nullptr)
{
  QVector<SequenceImportGroup> groups;
  QHash<QString, QStringList> imagePathsByDirectory;

  for (const QString &path : importedPaths) {
    if (path.isEmpty() || !isImageImportCandidate(path)) {
      continue;
    }
    const QFileInfo info(path);
    imagePathsByDirectory[info.absolutePath()].append(info.absoluteFilePath());
  }

  for (auto it = imagePathsByDirectory.begin(); it != imagePathsByDirectory.end(); ++it) {
    const QStringList &paths = it.value();
    if (paths.size() < 2) {
      continue;
    }

    std::vector<std::string> filenames;
    filenames.reserve(static_cast<size_t>(paths.size()));
    QHash<QString, QString> pathByFileName;
    for (const QString &path : paths) {
      const QString fileName = QFileInfo(path).fileName();
      filenames.push_back(fileName.toStdString());
      pathByFileName.insert(fileName, path);
    }

    const auto detection = ArtifactCore::detectSequences(filenames, 2);
    for (const auto &sequence : detection.sequences) {
      QStringList sequencePaths;
      sequencePaths.reserve(static_cast<int>(sequence.filenames.size()));
      for (const std::string &filename : sequence.filenames) {
        const QString key = QString::fromStdString(filename);
        const auto pathIt = pathByFileName.constFind(key);
        if (pathIt == pathByFileName.constEnd()) {
          continue;
        }
        sequencePaths.append(pathIt.value());
      }

      if (sequencePaths.size() < 2) {
        continue;
      }

      groups.push_back(SequenceImportGroup{sequencePaths.first(), sequencePaths});

      if (consumedPaths) {
        for (const QString &path : sequencePaths) {
          consumedPaths->insert(path);
        }
      }
    }
  }

  return groups;
}

QSize imageSizeForPath(const QString &path) {
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
      if (viewBox.isValid() && viewBox.width() > 0.0 &&
          viewBox.height() > 0.0) {
        return viewBox.size().toSize();
      }
    }
  }

  if (suffix == QStringLiteral("psd") || suffix == QStringLiteral("psb")) {
    ArtifactCore::PsdDocument doc;
    if (doc.open(path)) {
      const auto &header = doc.header();
      return QSize(static_cast<int>(header.width),
                   static_cast<int>(header.height));
    }
  }

  return {};
}

void notifyProjectMutation(ArtifactProjectManager &manager) {
  if (auto project = manager.getCurrentProjectSharedPtr()) {
    ArtifactCore::globalEventBus().publish<ProjectChangedEvent>({QString(), QString()});
  }
}

void notifyLayerMutation(const QString &compositionId, const LayerID &layerId) {
  ArtifactCore::globalEventBus().post<LayerChangedEvent>(
      LayerChangedEvent{compositionId, layerId.toString(),
                        LayerChangedEvent::ChangeType::Modified});
}

auto mapHealthSeverityToDiagnostic(HealthIssueSeverity severity)
    -> ArtifactCore::DiagnosticSeverity {
  switch (severity) {
  case HealthIssueSeverity::Error:
    return ArtifactCore::DiagnosticSeverity::Error;
  case HealthIssueSeverity::Warning:
    return ArtifactCore::DiagnosticSeverity::Warning;
  case HealthIssueSeverity::Info:
  default:
    return ArtifactCore::DiagnosticSeverity::Info;
  }
}

auto mapHealthCategoryToDiagnostic(const QString &category)
    -> ArtifactCore::DiagnosticCategory {
  if (category == QStringLiteral("CircularReference")) {
    return ArtifactCore::DiagnosticCategory::CircularDep;
  }
  if (category == QStringLiteral("MissingAsset")) {
    return ArtifactCore::DiagnosticCategory::File;
  }
  if (category == QStringLiteral("FrameRange")) {
    return ArtifactCore::DiagnosticCategory::Configuration;
  }
  if (category == QStringLiteral("BrokenReference")) {
    return ArtifactCore::DiagnosticCategory::Reference;
  }
  return ArtifactCore::DiagnosticCategory::Custom;
}

auto convertProjectHealthReportToDiagnosticsImpl(const ProjectHealthReport &report)
    -> std::vector<ArtifactCore::ProjectDiagnostic> {
  std::vector<ArtifactCore::ProjectDiagnostic> diagnostics;
  diagnostics.reserve(static_cast<size_t>(report.issues.size()));

  const auto fixActionForCategory = [](const QString &category) {
    if (category == QStringLiteral("MissingAsset")) {
      return QStringLiteral("Relink the missing asset or remove the footage entry");
    }
    if (category == QStringLiteral("BrokenReference")) {
      return QStringLiteral("Open the composition and replace or remove the broken reference");
    }
    if (category == QStringLiteral("CircularReference")) {
      return QStringLiteral("Break the composition nesting cycle");
    }
    if (category == QStringLiteral("FrameRange")) {
      return QStringLiteral("Normalize the composition or layer frame range");
    }
    if (category == QStringLiteral("Naming")) {
      return QStringLiteral("Rename the item to a production-safe label");
    }
    if (category == QStringLiteral("Spelling")) {
      return QStringLiteral("Review the suggested spelling correction");
    }
    return QStringLiteral("Inspect the reported issue");
  };

  for (const auto &issue : report.issues) {
    ArtifactCore::ProjectDiagnostic diagnostic(
        mapHealthSeverityToDiagnostic(issue.severity),
        mapHealthCategoryToDiagnostic(issue.category), issue.message);
    diagnostic.setDescription(issue.message);
    diagnostic.setSourceCompId(issue.targetName);
    diagnostic.setFixAction(fixActionForCategory(issue.category));
    diagnostics.push_back(diagnostic);
  }

  return diagnostics;
}

auto appValidationEngine() -> ArtifactCore::DiagnosticEngine&
{
  static ArtifactCore::DiagnosticEngine engine;
  static const bool initialized = []() {
    engine.ruleRegistry().clearRules();
    engine.ruleRegistry().registerRule(std::make_unique<ArtifactMissingFileRule>());
    engine.ruleRegistry().registerRule(std::make_unique<ArtifactPerformanceRule>());
    engine.ruleRegistry().registerRule(std::make_unique<ArtifactMatteReferenceRule>());
    return true;
  }();
  (void)initialized;
  return engine;
}

auto convertDiagnosticResultToDiagnostics(const ArtifactCore::DiagnosticResult& result)
    -> std::vector<ArtifactCore::ProjectDiagnostic> {
  return result.getDiagnostics();
}

QString projectHealthSummaryFromDiagnostics(const std::vector<ArtifactCore::ProjectDiagnostic>& diagnostics)
{
  const int errorCount = static_cast<int>(std::count_if(
      diagnostics.begin(), diagnostics.end(),
      [](const auto& diagnostic) { return diagnostic.isError(); }));
  const int warningCount = static_cast<int>(std::count_if(
      diagnostics.begin(), diagnostics.end(),
      [](const auto& diagnostic) { return diagnostic.isWarning(); }));
  const int issueCount = errorCount + warningCount;
  return QStringLiteral("Status: Project %1 (%2 issue%3)")
      .arg(issueCount == 0 ? QStringLiteral("healthy") : QStringLiteral("issues"))
      .arg(issueCount)
      .arg(issueCount == 1 ? QString() : QStringLiteral("s"));
}

void appendAppValidationDiagnostics(
    const std::shared_ptr<ArtifactProject>& project,
    std::vector<ArtifactCore::ProjectDiagnostic>& diagnostics)
{
  if (!project) {
    return;
  }

  const auto items = project->projectItems();
  for (auto* root : items) {
    std::function<void(ProjectItem*)> walk = [&](ProjectItem* item) {
      if (!item) {
        return;
      }

      if (item->type() == eProjectItemType::Composition) {
        auto* compItem = static_cast<CompositionItem*>(item);
        const auto found = project->findComposition(compItem->compositionId);
        if (found.success) {
          if (auto comp = found.ptr.lock()) {
            const auto result = appValidationEngine().validateAll(comp.get());
            const auto validationDiagnostics = convertDiagnosticResultToDiagnostics(result);
            diagnostics.insert(diagnostics.end(),
                               validationDiagnostics.begin(),
                               validationDiagnostics.end());
          }
        }
      }

      for (auto* child : item->children) {
        walk(child);
      }
    };

    walk(root);
  }
}

void appendUniqueDiagnostic(
    std::vector<ArtifactCore::ProjectDiagnostic>& diagnostics,
    const ArtifactCore::ProjectDiagnostic& diagnostic)
{
  const auto signature = [&diagnostic]() {
    return QStringLiteral("%1|%2|%3|%4|%5")
        .arg(static_cast<int>(diagnostic.getSeverity()))
        .arg(static_cast<int>(diagnostic.getCategory()))
        .arg(diagnostic.getMessage())
        .arg(diagnostic.getSourceCompId())
        .arg(diagnostic.getSourceLayerId());
  }();

  const bool alreadyPresent = std::any_of(
      diagnostics.begin(), diagnostics.end(),
      [&signature](const ArtifactCore::ProjectDiagnostic& existing) {
        return QStringLiteral("%1|%2|%3|%4|%5")
            .arg(static_cast<int>(existing.getSeverity()))
            .arg(static_cast<int>(existing.getCategory()))
            .arg(existing.getMessage())
            .arg(existing.getSourceCompId())
            .arg(existing.getSourceLayerId()) == signature;
      });

  if (!alreadyPresent) {
    diagnostics.push_back(diagnostic);
  }
}

void updateLastUsedCreationDefaults(const std::shared_ptr<ArtifactProject>& project,
                                    const ArtifactCompositionInitParams& params)
{
  if (!project) {
    return;
  }
  auto state = project->creationDefaultsState();
  CreationCompositionDefaults defaults;
  defaults.composition = params;
  state.lastUsed.composition = defaults;
  project->setCreationDefaultsState(state);
}

void updateLastUsedCreationDefaults(const std::shared_ptr<ArtifactProject>& project,
                                    const ArtifactLayerInitParams& params)
{
  if (!project) {
    return;
  }
  auto state = project->creationDefaultsState();
  CreationLayerDefaults defaults;
  defaults.layer = params;
  switch (params.layerType()) {
  case LayerType::Shape:
    state.lastUsed.shape = defaults;
    break;
  case LayerType::Text:
    state.lastUsed.text = defaults;
    break;
  case LayerType::Image:
    state.lastUsed.image = defaults;
    break;
  default:
    break;
  }
  project->setCreationDefaultsState(state);
}

// Precompose cycle detection:
// Returns true if moving `layers` into a fresh child composition of `parent`
// would create a cycle. This happens when any of the selected layers is a
// composition layer whose source composition (transitively, through nested
// composition layers) contains `parent` — i.e. the parent would end up
// referencing itself through the new nesting.
bool wouldCreatePrecomposeCycle(
    const ArtifactCompositionPtr &parent,
    const QVector<ArtifactAbstractLayerPtr> &layers) {
  if (!parent) {
    return false;
  }
  const CompositionID parentId = parent->id();

  // `pending` holds compositions that we still need to descend into.
  // `visited` guards against revisiting (and against pre-existing cycles
  // already stored in the project data).
  QSet<QString> visited;
  std::deque<CompositionID> pending;

  for (const auto &layer : layers) {
    auto compLayer =
        std::dynamic_pointer_cast<ArtifactCompositionLayer>(layer);
    if (!compLayer) {
      continue;
    }
    const CompositionID srcId = compLayer->sourceCompositionId();
    if (srcId.isNil()) {
      continue;
    }
    if (srcId == parentId) {
      return true;
    }
    pending.push_back(srcId);
  }

  auto *service = ArtifactProjectService::instance();
  if (!service) {
    return false;
  }

  while (!pending.empty()) {
    const CompositionID currentId = pending.front();
    pending.pop_front();

    const QString key = currentId.toString();
    if (visited.contains(key)) {
      continue;
    }
    visited.insert(key);

    auto current = service->findComposition(currentId).ptr.lock();
    if (!current) {
      continue;
    }

    for (const auto &layer : current->allLayer()) {
      auto compLayer =
          std::dynamic_pointer_cast<ArtifactCompositionLayer>(layer);
      if (!compLayer) {
        continue;
      }
      const CompositionID srcId = compLayer->sourceCompositionId();
      if (srcId.isNil()) {
        continue;
      }
      if (srcId == parentId) {
        return true;
      }
      pending.push_back(srcId);
    }
  }

  return false;
}

// === Precompose undo commands ===
//
// These commands wrap the Service mutation methods so that precompose /
// unprecompose participate in the global undo stack. They delegate to
// ArtifactProjectService for the actual mutation (single-sourced logic) and
// read the `last*Outcome()` getters to learn the generated/removed ids.
//
// redo()  runs the forward operation and records the result.
// undo()  runs the inverse operation using the recorded ids.
// Repeated redo/undo rely on the ids being stable across the round trip,
// which holds because layer/comp ids are not recycled within a session.

class PrecomposeUndoCommand : public UndoCommand {
public:
  PrecomposeUndoCommand(QVector<LayerID> layerIds, UniString name,
                        bool openNewComposition, bool matchWorkspaceDuration,
                        PrecomposeMode mode)
      : layerIds_(std::move(layerIds)), name_(std::move(name)),
        openNewComposition_(openNewComposition),
        matchWorkspaceDuration_(matchWorkspaceDuration), mode_(mode) {}

  void redo() override {
    auto *svc = ArtifactProjectService::instance();
    if (!svc) {
      return;
    }
    // On subsequent redo (after an undo), openNewComposition must stay false so
    // we don't yank the user's current composition focus around.
    const bool openForThisCall = firstRedo_ ? openNewComposition_ : false;
    if (!svc->precomposeLayersInCurrentComposition(
            layerIds_, name_, openForThisCall, matchWorkspaceDuration_, mode_)) {
      return;
    }
    const PrecomposeOutcome outcome = svc->lastPrecomposeOutcome();
    precompLayerId_ = outcome.precompLayerId;
    childCompId_ = outcome.childCompId;
    firstRedo_ = false;
    if (auto *mgr = UndoManager::instance()) {
      mgr->notifyAnythingChanged();
    }
  }

  void undo() override {
    auto *svc = ArtifactProjectService::instance();
    if (!svc || precompLayerId_.isNil()) {
      return;
    }
    // Undo the precompose by unprecomposing the generated layer and deleting
    // the child composition. keepComposition=false mirrors the original state
    // where the child did not exist.
    svc->unprecomposeLayerInCurrentComposition(precompLayerId_, false);
    precompLayerId_ = LayerID();
    childCompId_ = CompositionID();
    if (auto *mgr = UndoManager::instance()) {
      mgr->notifyAnythingChanged();
    }
  }

  QString label() const override {
    return QStringLiteral("Precompose");
  }

private:
  QVector<LayerID> layerIds_;
  UniString name_;
  bool openNewComposition_;
  bool matchWorkspaceDuration_;
  PrecomposeMode mode_;
  LayerID precompLayerId_;
  CompositionID childCompId_;
  bool firstRedo_ = true;
};

class UnprecomposeUndoCommand : public UndoCommand {
public:
  explicit UnprecomposeUndoCommand(LayerID precompLayerId, bool keepComposition)
      : precompLayerId_(precompLayerId), keepComposition_(keepComposition) {
    auto *svc = ArtifactProjectService::instance();
    auto comp = svc ? svc->currentComposition().lock() : ArtifactCompositionPtr{};
    auto layer = comp ? comp->layerById(precompLayerId_) : ArtifactAbstractLayerPtr{};
    auto compLayer =
        layer ? std::dynamic_pointer_cast<ArtifactCompositionLayer>(layer)
              : nullptr;
    if (compLayer) {
      mode_ = compLayer->restoreMoveAllAttributes()
                  ? PrecomposeMode::MoveAllAttributes
                  : PrecomposeMode::MoveSelected;
    }
  }

  void redo() override {
    auto *svc = ArtifactProjectService::instance();
    if (!svc || precompLayerId_.isNil()) {
      return;
    }
    if (!svc->unprecomposeLayerInCurrentComposition(precompLayerId_,
                                                    keepComposition_)) {
      return;
    }
    // Capture what we need to rebuild the precomp on undo. These are only
    // valid right after a successful unprecompose.
    movedLayerIds_ = svc->lastUnprecomposeMovedLayerIds();
    childName_ = svc->lastUnprecomposeChildName();
    if (auto *mgr = UndoManager::instance()) {
      mgr->notifyAnythingChanged();
    }
  }

  void undo() override {
    auto *svc = ArtifactProjectService::instance();
    if (!svc || movedLayerIds_.isEmpty() || childName_.toQString().isEmpty()) {
      return;
    }
    // Re-create the precomp by precomposing the moved layers back together.
    // openNewComposition=false keeps the user's current focus.
    if (!svc->precomposeLayersInCurrentComposition(movedLayerIds_, childName_,
                                                   false, true, mode_)) {
      return;
    }
    const PrecomposeOutcome outcome = svc->lastPrecomposeOutcome();
    precompLayerId_ = outcome.precompLayerId;
    if (auto *mgr = UndoManager::instance()) {
      mgr->notifyAnythingChanged();
    }
  }

  QString label() const override {
    return QStringLiteral("Unprecompose");
  }

private:
  LayerID precompLayerId_;
  bool keepComposition_;
  QVector<LayerID> movedLayerIds_;
  UniString childName_;
  PrecomposeMode mode_ = PrecomposeMode::MoveSelected;
};

class AddEffectUndoCommand : public UndoCommand {
public:
  AddEffectUndoCommand(const LayerID &layerId,
                       std::shared_ptr<ArtifactAbstractEffect> effect)
      : layerId_(layerId), effect_(std::move(effect)) {}

  void redo() override {
    auto *svc = ArtifactProjectService::instance();
    if (!svc || !effect_) return;
    if (effectId_.isEmpty()) {
      if (!svc->addEffectToLayerInCurrentComposition(layerId_, effect_)) return;
      effectId_ = effect_->effectID().toQString();
    } else {
      effect_->setEffectID(UniString::fromQString(effectId_));
      auto comp = svc->currentComposition().lock();
      if (!comp) return;
      auto layer = comp->layerById(layerId_);
      if (!layer) return;
      layer->addEffect(effect_);
      notifyLayerMutation(comp->id().toString(), layerId_);
    }
    if (auto *mgr = UndoManager::instance()) {
      mgr->notifyAnythingChanged();
    }
  }

  void undo() override {
    if (effectId_.isEmpty()) return;
    auto *svc = ArtifactProjectService::instance();
    if (!svc) return;
    svc->removeEffectFromLayerInCurrentComposition(layerId_, effectId_);
    if (auto *mgr = UndoManager::instance()) {
      mgr->notifyAnythingChanged();
    }
  }

  QString label() const override { return QStringLiteral("Add Effect"); }

private:
  LayerID layerId_;
  std::shared_ptr<ArtifactAbstractEffect> effect_;
  QString effectId_;
};

class RemoveEffectUndoCommand : public UndoCommand {
public:
  RemoveEffectUndoCommand(const LayerID &layerId,
                          std::shared_ptr<ArtifactAbstractEffect> effect)
      : layerId_(layerId), effect_(std::move(effect)) {}

  void redo() override {
    if (!effect_) return;
    const QString id = effect_->effectID().toQString();
    if (id.isEmpty()) return;
    auto *svc = ArtifactProjectService::instance();
    if (!svc) return;
    auto comp = svc->currentComposition().lock();
    if (!comp) return;
    auto layer = comp->layerById(layerId_);
    if (!layer) return;
    const auto effects = layer->getEffects();
    for (int i = 0; i < static_cast<int>(effects.size()); ++i) {
      if (effects[i] && effects[i]->effectID().toQString() == id) {
        effectIndex_ = i;
        break;
      }
    }
    svc->removeEffectFromLayerInCurrentComposition(layerId_, id);
    if (auto *mgr = UndoManager::instance()) {
      mgr->notifyAnythingChanged();
    }
  }

  void undo() override {
    if (!effect_) return;
    auto *svc = ArtifactProjectService::instance();
    if (!svc) return;
    auto comp = svc->currentComposition().lock();
    if (!comp) return;
    auto layer = comp->layerById(layerId_);
    if (!layer) return;
    if (effectIndex_ >= 0) {
      auto effects = layer->getEffects();
      effects.insert(effects.begin() + effectIndex_, effect_);
      layer->clearEffects();
      for (const auto &e : effects) {
        if (e) layer->addEffect(e);
      }
    } else {
      layer->addEffect(effect_);
    }
    notifyLayerMutation(comp->id().toString(), layerId_);
    if (auto project = svc->getCurrentProjectSharedPtr()) {
      ArtifactCore::globalEventBus().publish<ProjectChangedEvent>(
          {QString(), QString()});
    }
    if (auto *mgr = UndoManager::instance()) {
      mgr->notifyAnythingChanged();
    }
  }

  QString label() const override { return QStringLiteral("Remove Effect"); }

private:
  LayerID layerId_;
  std::shared_ptr<ArtifactAbstractEffect> effect_;
  int effectIndex_ = -1;
};

class SetEffectEnabledUndoCommand : public UndoCommand {
public:
  SetEffectEnabledUndoCommand(const LayerID &layerId, const QString &effectId,
                              bool wasEnabled, bool nowEnabled)
      : layerId_(layerId), effectId_(effectId), wasEnabled_(wasEnabled),
        nowEnabled_(nowEnabled) {}

  void redo() override {
    auto *svc = ArtifactProjectService::instance();
    if (!svc) return;
    svc->setEffectEnabledInLayerInCurrentComposition(layerId_, effectId_,
                                                     nowEnabled_);
    if (auto *mgr = UndoManager::instance()) {
      mgr->notifyAnythingChanged();
    }
  }

  void undo() override {
    auto *svc = ArtifactProjectService::instance();
    if (!svc) return;
    svc->setEffectEnabledInLayerInCurrentComposition(layerId_, effectId_,
                                                     wasEnabled_);
    if (auto *mgr = UndoManager::instance()) {
      mgr->notifyAnythingChanged();
    }
  }

  QString label() const override { return QStringLiteral("Toggle Effect"); }

private:
  LayerID layerId_;
  QString effectId_;
  bool wasEnabled_;
  bool nowEnabled_;
};

class MoveEffectUndoCommand : public UndoCommand {
public:
  MoveEffectUndoCommand(const LayerID &layerId, const QString &effectId,
                        int direction)
      : layerId_(layerId), effectId_(effectId), direction_(direction) {}

  void redo() override {
    auto *svc = ArtifactProjectService::instance();
    if (!svc) return;
    svc->moveEffectInLayerInCurrentComposition(layerId_, effectId_, direction_);
    if (auto *mgr = UndoManager::instance()) {
      mgr->notifyAnythingChanged();
    }
  }

  void undo() override {
    auto *svc = ArtifactProjectService::instance();
    if (!svc) return;
    svc->moveEffectInLayerInCurrentComposition(layerId_, effectId_,
                                               -direction_);
    if (auto *mgr = UndoManager::instance()) {
      mgr->notifyAnythingChanged();
    }
  }

  QString label() const override { return QStringLiteral("Move Effect"); }

private:
  LayerID layerId_;
  QString effectId_;
  int direction_;
};

// --- GroupLayersUndoCommand ---
class GroupLayersUndoCommand : public UndoCommand {
public:
  GroupLayersUndoCommand(QVector<LayerID> layerIds, UniString groupName)
      : layerIds_(std::move(layerIds)), groupName_(std::move(groupName)) {}

  void redo() override {
    auto *svc = ArtifactProjectService::instance();
    if (!svc) return;
    auto *sel = ArtifactLayerSelectionManager::instance();
    if (!sel) return;
    auto comp = svc->currentComposition().lock();
    if (!comp) return;
    // Re-select original layers before calling service
    sel->clearSelection();
    for (const auto &id : layerIds_) {
      if (auto l = comp->layerById(id)) sel->addToSelection(l);
    }
    if (!svc->groupSelectedLayersInCurrentComposition(groupName_)) return;
    auto current = sel->currentLayer();
    if (current) groupLayerId_ = current->id();
    if (auto *mgr = UndoManager::instance()) mgr->notifyAnythingChanged();
  }

  void undo() override {
    auto *svc = ArtifactProjectService::instance();
    if (!svc || groupLayerId_.isNil()) return;
    auto comp = svc->currentComposition().lock();
    if (!comp) return;
    auto groupLayer = comp->layerById(groupLayerId_);
    if (!groupLayer) return;
    // Capture children if first undo
    if (childIds_.isEmpty()) {
      if (auto g = std::dynamic_pointer_cast<ArtifactGroupLayer>(groupLayer)) {
        for (const auto &child : g->children()) {
          if (child) childIds_.push_back(child->id());
        }
      }
      auto allLayers = comp->allLayer();
      for (int i = 0; i < allLayers.size(); ++i) {
        if (allLayers[i]->id() == groupLayerId_) {
          groupIndex_ = i; break;
        }
      }
    }
    // Move children out of group
    for (int i = childIds_.size() - 1; i >= 0; --i) {
      if (auto child = comp->layerById(childIds_[i])) {
        child->clearParent();
        comp->insertLayerAt(child, groupIndex_);
      }
    }
    // Remove group layer
    comp->removeLayer(groupLayerId_);
    groupLayerId_ = LayerID();
    // Restore selection to children
    if (auto *sel = ArtifactLayerSelectionManager::instance()) {
      sel->clearSelection();
      for (const auto &cid : childIds_) {
        if (auto child = comp->layerById(cid)) sel->addToSelection(child);
      }
    }
    if (auto *mgr = UndoManager::instance()) mgr->notifyAnythingChanged();
  }

  QString label() const override {
    return QStringLiteral("Group Layers");
  }

private:
  QVector<LayerID> layerIds_;
  UniString groupName_;
  LayerID groupLayerId_;
  int groupIndex_ = -1;
  QVector<LayerID> childIds_;
};

// --- UngroupLayersUndoCommand ---
class UngroupLayersUndoCommand : public UndoCommand {
public:
  UngroupLayersUndoCommand(LayerID groupLayerId, UniString groupName)
      : groupLayerId_(groupLayerId), groupName_(std::move(groupName)) {}

  void redo() override {
    auto *svc = ArtifactProjectService::instance();
    if (!svc || groupLayerId_.isNil()) return;
    auto comp = svc->currentComposition().lock();
    if (!comp) return;
    auto groupLayer = comp->layerById(groupLayerId_);
    auto g = std::dynamic_pointer_cast<ArtifactGroupLayer>(groupLayer);
    if (!g) return;
    // Capture children and group index
    if (childIds_.isEmpty()) {
      auto children = g->children();
      for (const auto &child : children) {
        if (child) childIds_.push_back(child->id());
      }
      auto allLayers = comp->allLayer();
      for (int i = 0; i < allLayers.size(); ++i) {
        if (allLayers[i]->id() == groupLayerId_) {
          groupIndex_ = i; break;
        }
      }
    }
    // Move children out of group
    for (int i = childIds_.size() - 1; i >= 0; --i) {
      if (auto child = comp->layerById(childIds_[i])) {
        child->clearParent();
        comp->insertLayerAt(child, groupIndex_);
      }
    }
    // Remove group layer
    comp->removeLayer(groupLayerId_);
    groupLayerId_ = LayerID();
    if (auto *sel = ArtifactLayerSelectionManager::instance()) {
      sel->clearSelection();
      for (const auto &cid : childIds_) {
        if (auto child = comp->layerById(cid)) sel->addToSelection(child);
      }
    }
    if (auto *mgr = UndoManager::instance()) mgr->notifyAnythingChanged();
  }

  void undo() override {
    auto *svc = ArtifactProjectService::instance();
    if (!svc || childIds_.isEmpty()) return;
    auto *sel = ArtifactLayerSelectionManager::instance();
    if (!sel) return;
    sel->clearSelection();
    // Re-select children and call group
    auto comp = svc->currentComposition().lock();
    if (!comp) return;
    for (const auto &cid : childIds_) {
      if (auto child = comp->layerById(cid)) sel->addToSelection(child);
    }
    if (!svc->groupSelectedLayersInCurrentComposition(groupName_)) return;
    auto current = sel->currentLayer();
    if (current) groupLayerId_ = current->id();
    childIds_.clear();
    if (auto *mgr = UndoManager::instance()) mgr->notifyAnythingChanged();
  }

  QString label() const override {
    return QStringLiteral("Ungroup Layers");
  }

private:
  LayerID groupLayerId_;
  UniString groupName_;
  QVector<LayerID> childIds_;
  int groupIndex_ = -1;
};

// --- SplitLayerUndoCommand ---
class SplitLayerUndoCommand : public UndoCommand {
public:
  SplitLayerUndoCommand(CompositionID compId, LayerID layerId)
      : compId_(compId), layerId_(layerId) {}

  void redo() override {
    auto *svc = ArtifactProjectService::instance();
    if (!svc) return;
    // Try current composition first, then project lookup
    auto comp = svc->currentComposition().lock();
    if (!comp || comp->id() != compId_) {
      auto project = svc->getCurrentProjectSharedPtr();
      if (!project) return;
      comp = project->findComposition(compId_).ptr.lock();
      if (!comp) return;
    }
    auto layer = comp->layerById(layerId_);
    if (!layer) return;
    auto now = comp->framePosition();
    if (now.framePosition() <= layer->inPoint().framePosition() ||
        now.framePosition() >= layer->outPoint().framePosition()) return;
    // Store pre-split state for undo
    oldOutFrame_ = layer->outPoint().framePosition();
    splitFrame_ = now.framePosition();
    // Truncate original
    layer->setOutPoint(FramePosition(splitFrame_));
    // Duplicate for second half
    auto project = svc->getCurrentProjectSharedPtr();
    if (!project) return;
    auto result = project->duplicateLayerInComposition(comp->id(), layerId_);
    if (result.success && result.layer) {
      newLayer_ = result.layer;
      newLayer_->setInPoint(FramePosition(splitFrame_));
      newLayer_->setOutPoint(FramePosition(oldOutFrame_));
    }
    if (auto *mgr = UndoManager::instance()) mgr->notifyAnythingChanged();
  }

  void undo() override {
    if (!newLayer_) return;
    auto *svc = ArtifactProjectService::instance();
    if (!svc) return;
    auto comp = svc->currentComposition().lock();
    if (!comp || comp->id() != compId_) {
      auto project = svc->getCurrentProjectSharedPtr();
      if (!project) return;
      comp = project->findComposition(compId_).ptr.lock();
      if (!comp) return;
    }
    // Remove the cloned layer
    comp->removeLayer(newLayer_->id());
    newLayer_.reset();
    // Restore original layer's out point
    if (auto layer = comp->layerById(layerId_)) {
      layer->setOutPoint(FramePosition(oldOutFrame_));
    }
    if (auto *mgr = UndoManager::instance()) mgr->notifyAnythingChanged();
  }

  QString label() const override {
    return QStringLiteral("Split Layer");
  }

private:
  CompositionID compId_;
  LayerID layerId_;
  ArtifactAbstractLayerPtr newLayer_;
  int64_t oldOutFrame_ = 0;
  int64_t splitFrame_ = 0;
};

} // namespace

std::vector<ArtifactCore::ProjectDiagnostic>
Artifact::convertProjectHealthReportToDiagnostics(const ProjectHealthReport &report)
{
  return convertProjectHealthReportToDiagnosticsImpl(report);
}

class ArtifactProjectService::Impl {
private:
public:
  Impl();
  ~Impl();
  static ArtifactProjectManager &projectManager();
  void installSelectionBridge(ArtifactProjectService *owner);
  void addLayerToCurrentComposition(const ArtifactLayerInitParams &params,
                                    bool selectNewLayer = true,
                                    bool placeAtCurrentFrame = false,
                                    bool startHidden = false);
  void setDefaultNewLayerHidden(bool hidden);
  bool defaultNewLayerHidden() const;
  void addAssetFromPath(const UniString &path);
  QStringList importAssetsFromPaths(const QStringList &sourcePaths);
  void importAssetsFromPathsAsync(const QStringList &sourcePaths,
                                  std::function<void(QStringList)> onFinished);
  void setPreviewQualityPreset(PreviewQualityPreset preset);
  PreviewQualityPreset previewQualityPreset() const;
  UniString projectName() const;
  void changeProjectName(const UniString &name);

  ArtifactCompositionWeakPtr currentComposition();
  FindCompositionResult findComposition(const CompositionID &id);
  ChangeCompositionResult changeCurrentComposition(const CompositionID &id);

  void removeAllAssets();
  PreviewQualityPreset qualityPreset_ = PreviewQualityPreset::Preview;
  CompositionID currentCompositionId_{};
  // ProgressiveRenderer progressiveRenderer_;

  void checkImportedAssetCompatibility(const QStringList &importedPaths);
  // Records the most recent precompose/unprecompose outcome so undo commands
  // (constructed right after the mutation) can target the exact generated/
  // removed ids. Intentionally transient: only meaningful in the synchronous
  // window between mutate and command push.
  PrecomposeOutcome lastPrecomposeOutcome_;
  LayerID lastUnprecomposePrecompLayerId_{};
  CompositionID lastUnprecomposeChildCompId_{};
  QVector<LayerID> lastUnprecomposeMovedLayerIds_;
  UniString lastUnprecomposeChildName_;
  bool forwardingSelectionChange_ = false;
  bool defaultNewLayerHidden_ = false;
  LayerID lastForwardedLayerId_;
  ArtifactCore::EventBus eventBus_ = ArtifactCore::globalEventBus();
  std::vector<ArtifactCore::EventBus::Subscription> eventBusSubscriptions_;

  QFileSystemWatcher* fileWatcher_ = nullptr;
  QTimer* statusCheckTimer_ = nullptr;
  void setupFileWatcher(ArtifactProjectService* owner);
  void updateAllAssetStatuses();
  void handleFileChanged(const QString& path);
};

void ArtifactProjectService::Impl::setupFileWatcher(ArtifactProjectService* owner) {
  fileWatcher_ = new QFileSystemWatcher(owner);
  statusCheckTimer_ = new QTimer(owner);
  statusCheckTimer_->setInterval(2000); // 2秒ごとに存在確認（Watcherが効かない削除等のため）

  QObject::connect(fileWatcher_, &QFileSystemWatcher::fileChanged, owner, [this](const QString& path) {
    handleFileChanged(path);
  });

  QObject::connect(statusCheckTimer_, &QTimer::timeout, owner, [this]() {
    updateAllAssetStatuses();
  });

  statusCheckTimer_->start();
}

void ArtifactProjectService::Impl::updateAllAssetStatuses() {
  auto project = projectManager().getCurrentProjectSharedPtr();
  if (!project) return;

  const auto items = project->projectItems();
  std::function<void(ProjectItem*)> checkRecursive = [&](ProjectItem* item) {
    if (!item) return;
    if (item->type() == eProjectItemType::Footage) {
      auto* footage = static_cast<FootageItem*>(item);
      QFileInfo info(footage->filePath);
      
      // 仮の解決策として、ここではステータスを直接判定して更新する
      // 本来的には Asset オブジェクト側で保持すべきだが、まずはUIに反映させる
      if (!info.exists()) {
        // Missing
        qDebug() << "[AssetMonitor] Missing:" << footage->filePath;
      }
    }
    for (auto* child : item->children) checkRecursive(child);
  };

  for (auto* root : items) checkRecursive(root);
}

void ArtifactProjectService::Impl::handleFileChanged(const QString& path) {
  qDebug() << "[AssetMonitor] File modified:" << path;
  // ここでキャッシュの破棄やプレビューの更新イベントを飛ばす
  ArtifactCore::globalEventBus().publish<ProjectChangedEvent>({QString(), QString()});
}

ArtifactProjectService::Impl::Impl() {}

// Impl::removeLayerFromComposition was removed; use manager call in service
// wrapper

ArtifactProjectService::Impl::~Impl() {}

void ArtifactProjectService::Impl::installSelectionBridge(
    ArtifactProjectService *owner) {
  if (!owner) {
    return;
  }
  auto *selectionManager = ArtifactLayerSelectionManager::instance();
  if (!selectionManager) {
    return;
  }
  auto publishSelectionChanged =
      [this](ArtifactProjectService *targetOwner, const LayerID &layerId,
             LayerSelectionChangeReason reason) {
        if (!targetOwner) {
          return;
        }
        if (layerId == lastForwardedLayerId_) {
          return;
        }
        lastForwardedLayerId_ = layerId;
        ArtifactCore::globalEventBus().publish<LayerSelectionChangedEvent>(
            LayerSelectionChangedEvent{
                targetOwner->currentComposition().lock()
                    ? targetOwner->currentComposition().lock()->id().toString()
                    : QString(),
                layerId.toString(), reason});
      };
  QObject::connect(
      selectionManager, &ArtifactLayerSelectionManager::selectionChanged,
      owner, [this, owner, selectionManager, publishSelectionChanged]() {
        if (forwardingSelectionChange_) {
          return;
        }
        const auto current = selectionManager
                                 ? selectionManager->currentLayer()
                                 : ArtifactAbstractLayerPtr{};
        const LayerID nextId = current ? current->id() : LayerID();
        if (!nextId.isNil()) {
          publishSelectionChanged(owner, nextId,
                                  LayerSelectionChangeReason::SelectionBridgeSync);
          return;
        }
        if (lastForwardedLayerId_.isNil()) {
          return;
        }

        const auto currentComp = owner->currentComposition().lock();
        qDebug() << "[ProjectService] selection bridge cleared"
                 << "composition="
                 << (currentComp ? currentComp->id().toString() : QString())
                 << "lastForwarded="
                 << lastForwardedLayerId_.toString();

        QPointer<ArtifactProjectService> safeOwner(owner);
        QPointer<ArtifactLayerSelectionManager> safeSelectionManager(
            selectionManager);
        QTimer::singleShot(
            0, owner,
            [this, safeOwner, safeSelectionManager,
             publishSelectionChanged]() {
              if (!safeOwner || !safeSelectionManager ||
                  forwardingSelectionChange_) {
                return;
              }
              const auto resolvedCurrent =
                  safeSelectionManager->currentLayer();
              const LayerID resolvedId =
                  resolvedCurrent ? resolvedCurrent->id() : LayerID();
              publishSelectionChanged(
                  safeOwner.data(), resolvedId,
                  LayerSelectionChangeReason::TransientSync);
            });
      });
}

ArtifactProjectManager &ArtifactProjectService::Impl::projectManager() {
  return ArtifactProjectManager::getInstance();
}

void ArtifactProjectService::Impl::addLayerToCurrentComposition(
    const ArtifactLayerInitParams &params, bool selectNewLayer,
    bool placeAtCurrentFrame, bool startHidden) {
  auto &manager = ArtifactProjectService::Impl::projectManager();
  LayerID selectedLayerId;
  if (auto *selectionManager = ArtifactLayerSelectionManager::instance()) {
    if (auto selectedLayer = selectionManager->currentLayer()) {
      selectedLayerId = selectedLayer->id();
    }
  }

  ArtifactLayerResult result;
  bool targetedCurrentComposition = false;
  if (auto comp = currentComposition().lock()) {
    result = manager.addLayerToComposition(
        comp->id(), const_cast<ArtifactLayerInitParams &>(params));
    targetedCurrentComposition = true;
  } else {
    result = manager.addLayerToCurrentComposition(
        const_cast<ArtifactLayerInitParams &>(params));
  }
  if (result.success && result.layer) {
    if (startHidden) {
      result.layer->setVisible(false);
    }
    if (auto comp = currentComposition().lock()) {
      if (result.layer->is3D()) {
        const QSize compSize = comp->settings().compositionSize();
        const float compCenterX =
            static_cast<float>(compSize.width() > 0 ? compSize.width() : 1920) *
            0.5f;
        const float compCenterY =
            static_cast<float>(compSize.height() > 0 ? compSize.height() : 1080) *
            0.5f;
        const QVector3D current = result.layer->position3D();
        result.layer->setPosition3D(
            QVector3D(compCenterX, compCenterY, current.z()));
      }
      if (placeAtCurrentFrame) {
        const qint64 activeFrame =
            ArtifactPlaybackService::instance()
                ? ArtifactPlaybackService::instance()->currentFrame().framePosition()
                : comp->framePosition().framePosition();
        const qint64 duration =
            std::max<qint64>(1, result.layer->outPoint().framePosition() -
                                   result.layer->inPoint().framePosition());
        if (!result.layer->isTimingLocked()) {
        result.layer->setInPoint(FramePosition(activeFrame));
        result.layer->setOutPoint(FramePosition(activeFrame + duration));
        result.layer->setStartTime(FramePosition(activeFrame));
        }
      }

    }

    if (auto project = manager.getCurrentProjectSharedPtr()) {
      updateLastUsedCreationDefaults(project, params);
    }

    if (targetedCurrentComposition) {
      if (auto comp = currentComposition().lock()) {
        if (!selectedLayerId.isNil() &&
            comp->containsLayerById(selectedLayerId)) {
          const auto allLayers = comp->allLayer();
          int selectedIndex = -1;
          int newLayerIndex = -1;
          for (int i = 0; i < allLayers.size(); ++i) {
            const auto &layer = allLayers[i];
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

          if (selectedIndex >= 0 && newLayerIndex >= 0 &&
              newLayerIndex != selectedIndex) {
            // Internal order is back-to-front. Insert one slot after the
            // selected layer so the new layer appears above it in the timeline.
            const int targetIndex =
                std::clamp(selectedIndex + 1, 0,
                           std::max(0, static_cast<int>(allLayers.size()) - 1));
            comp->moveLayerToIndex(result.layer->id(), targetIndex);
          }

          const auto selectedLayer = comp->layerById(selectedLayerId);
          if (selectedLayer && selectedLayer->hasParent()) {
            result.layer->setParentById(selectedLayer->parentLayerId());
          }
        }
      }
      // notifyProjectMutation は呼ばない。
      // LayerChangedEvent{Created} が projectManager::layerCreated
      // 経由で既に発火済みのため、 ProjectChangedEvent
      // を追加発火すると全ウィジェットが二重リビルドされる。
    }

    if (selectNewLayer) {
      if (auto *service = ArtifactProjectService::instance()) {
        service->selectLayer(result.layer->id());
      }
    }
  }
  qDebug() << "[ArtifactProjectService::Impl::addLayerToCurrentComposition] "
              "delegated to manager, result="
           << result.success;
}

void ArtifactProjectService::Impl::addAssetFromPath(const UniString &path) {
  QStringList input;
  input.append(path.toQString());
  importAssetsFromPaths(input);
}

QStringList ArtifactProjectService::Impl::importAssetsFromPaths(
    const QStringList &sourcePaths) {
  if (sourcePaths.isEmpty()) {
    return {};
  }

  auto &manager = ArtifactProjectService::Impl::projectManager();
  auto project = manager.getCurrentProjectSharedPtr();
  if (!project) {
    return {};
  }
  QString assetsRoot = manager.currentProjectAssetsPath();
  QStringList validSources;
  QStringList toCopy;
  QStringList toCopySources;
  QStringList alreadySources;

  for (const auto &src : sourcePaths) {
    if (src.isEmpty())
      continue;
    QFileInfo info(src);
    if (!info.exists() || !info.isFile())
      continue;

    QString abs = info.absoluteFilePath();
    validSources.append(abs);
    if (!assetsRoot.isEmpty() &&
        abs.startsWith(assetsRoot, Qt::CaseInsensitive)) {
      alreadySources.append(abs);
    } else {
      toCopy.append(abs);
      toCopySources.append(abs);
    }
  }

  if (validSources.isEmpty()) {
    return {};
  }

  QSet<QString> sourceSequenceConsumed;
  const QVector<SequenceImportGroup> sourceSequenceGroups =
      detectSequenceImportGroups(validSources, &sourceSequenceConsumed);
  double sequenceFrameRate = 0.0;
  if (!sourceSequenceGroups.isEmpty()) {
    bool ok = false;
    sequenceFrameRate = QInputDialog::getDouble(
        QApplication::activeWindow(),
        QStringLiteral("Image Sequence Frame Rate"),
        QStringLiteral("Frame rate for imported image sequences:"),
        24.0, 1.0, 240.0, 3, &ok);
    if (!ok) {
      return {};
    }
  }

  QStringList copied = manager.copyFilesToProjectAssets(toCopy);
  QHash<QString, QString> finalPathBySource;
  for (int i = 0; i < toCopySources.size() && i < copied.size(); ++i) {
    finalPathBySource.insert(toCopySources.at(i), copied.at(i));
  }
  for (const QString &src : alreadySources) {
    finalPathBySource.insert(src, src);
  }

  QStringList finalImported;
  finalImported.reserve(validSources.size());

  for (const SequenceImportGroup &group : sourceSequenceGroups) {
    QStringList resolvedSequencePaths;
    resolvedSequencePaths.reserve(group.sequencePaths.size());
    for (const QString &sourcePath : group.sequencePaths) {
      const QString resolvedPath = finalPathBySource.value(sourcePath);
      if (!resolvedPath.isEmpty()) {
        resolvedSequencePaths.append(resolvedPath);
      }
    }
    if (resolvedSequencePaths.size() < 2) {
      continue;
    }
    project->addAssetFromPath(resolvedSequencePaths.first(), resolvedSequencePaths,
                              sequenceFrameRate);
    finalImported.append(resolvedSequencePaths.first());
  }

  for (const QString &sourcePath : validSources) {
    if (sourceSequenceConsumed.contains(sourcePath)) {
      continue;
    }
    const QString resolvedPath = finalPathBySource.value(sourcePath);
    if (resolvedPath.isEmpty()) {
      continue;
    }
    project->addAssetFromPath(resolvedPath);
    finalImported.append(resolvedPath);
  }

  if (!finalImported.isEmpty()) {
    project->projectChanged();
  }

  checkImportedAssetCompatibility(finalImported);
  return finalImported;
}

void ArtifactProjectService::Impl::importAssetsFromPathsAsync(
    const QStringList &sourcePaths,
    std::function<void(QStringList)> onFinished) {
  auto *service = ArtifactProjectService::instance();
  if (!service) {
    if (onFinished) {
      onFinished({});
    }
    return;
  }

  auto &manager = ArtifactProjectService::Impl::projectManager();
  const QString assetsRoot = manager.currentProjectAssetsPath();
  QSize compSize;
  if (auto comp = this->currentComposition().lock()) {
    compSize = comp->settings().compositionSize();
  }

  auto *watcher = new QFutureWatcher<QStringList>(service);
  QObject::connect(watcher, &QFutureWatcher<QStringList>::finished, service,
                   [this, watcher, onFinished = std::move(onFinished)]() mutable {
                     const QStringList importedPaths = watcher->result();
                     watcher->deleteLater();
                     if (!importedPaths.isEmpty()) {
                       checkImportedAssetCompatibility(importedPaths);
                     }
                     if (onFinished) {
                       onFinished(importedPaths);
                     }
                   });

  watcher->setFuture(QtConcurrent::run([sourcePaths, assetsRoot, compSize]() {
    QStringList importedPaths;
    if (sourcePaths.isEmpty()) {
      return importedPaths;
    }

    auto makeUniqueAssetPath = [](const QString &directory,
                                  const QString &fileName) {
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
        candidate = dir.filePath(
            extension.isEmpty()
                ? numbered
                : QStringLiteral("%1.%2").arg(numbered).arg(extension));
        ++counter;
      }

      return candidate;
    };

    ArtifactCore::FileTypeDetector detector;
    for (const auto &src : sourcePaths) {
      if (src.isEmpty())
        continue;

      QFileInfo info(src);
      if (!info.exists() || !info.isFile())
        continue;

      const QString abs = info.absoluteFilePath();
      if (!assetsRoot.isEmpty() &&
          abs.startsWith(assetsRoot, Qt::CaseInsensitive)) {
        importedPaths.append(abs);
      } else {
        if (assetsRoot.isEmpty())
          continue;
        const QString finalFile =
            makeUniqueAssetPath(assetsRoot, QFileInfo(abs).fileName());
        if (finalFile.isEmpty())
          continue;
        if (!QFile::copy(abs, finalFile)) {
          continue;
        }
        importedPaths.append(finalFile);
      }

      if (!importedPaths.isEmpty() &&
          detector.detectByExtension(importedPaths.back()) ==
              ArtifactCore::FileType::Image) {
        const QSize imageSize = imageSizeForPath(importedPaths.back());
        if (imageSize.isValid() && compSize.isValid() &&
            (imageSize.width() != compSize.width() ||
             imageSize.height() != compSize.height())) {
          qWarning() << "[CompatibilityGuard] Image resolution differs from "
                        "composition. image="
                     << imageSize.width() << "x" << imageSize.height()
                     << " comp=" << compSize.width() << "x" << compSize.height()
                     << " path=" << importedPaths.back();
        }
      }
    }

    return importedPaths;
  }));
}

void ArtifactProjectService::Impl::checkImportedAssetCompatibility(
    const QStringList &importedPaths) {
  if (importedPaths.isEmpty())
    return;

  QSize compSize;
  if (auto comp = currentComposition().lock()) {
    compSize = comp->settings().compositionSize();
  }

  ArtifactCore::FileTypeDetector detector;
  QStringList videoWarnings;

  for (const auto &path : importedPaths) {
    if (path.isEmpty())
      continue;

    auto type = detector.detectByExtension(path);
    if (type == ArtifactCore::FileType::Unknown) {
      qWarning() << "[CompatibilityGuard] Unknown/unsupported file type:"
                 << path;
      continue;
    }

    if (type == ArtifactCore::FileType::Image) {
      const QSize imageSize = imageSizeForPath(path);
      if (!imageSize.isValid()) {
        qWarning() << "[CompatibilityGuard] Image size unavailable:" << path;
        continue;
      }
      if (compSize.width() > 0 && compSize.height() > 0 &&
          (imageSize.width() != compSize.width() ||
           imageSize.height() != compSize.height())) {
        qWarning() << "[CompatibilityGuard] Image resolution differs from "
                      "composition. image="
                   << imageSize.width() << "x" << imageSize.height()
                   << " comp=" << compSize.width() << "x" << compSize.height()
                   << " path=" << path;
      }
    }

    if (type == ArtifactCore::FileType::Video) {
      auto probe = ArtifactCore::probeVideoFile(path);
      if (!probe.hasVideoStream)
        continue;
      if (!probe.isEditingFriendly) {
        qWarning() << "[CompatibilityGuard] Non-editing-friendly video codec:"
                    << probe.codecName << "path=" << path;
        videoWarnings.append(
            QStringLiteral("  %1 (%2)").arg(QFileInfo(path).fileName())
                .arg(probe.codecName));
      }
    }
  }

  if (!videoWarnings.isEmpty()) {
    QString msg = QStringLiteral(
        "The following video files use a compression format that is not "
        "ideal for editing:\n\n%1\n\n"
        "Editing-friendly alternatives: ProRes, DNxHD, Animation, "
        "or image sequences.\n"
        "Consider transcoding before use for smoother playback "
        "and frame-accurate seeking.")
        .arg(videoWarnings.join(QStringLiteral("\n")));
    QMessageBox::warning(QApplication::activeWindow(),
                         QStringLiteral("Video Codec Warning"), msg);
  }
}

void ArtifactProjectService::Impl::setPreviewQualityPreset(
    PreviewQualityPreset preset) {
  qualityPreset_ = preset;
  switch (preset) {
  case PreviewQualityPreset::Draft:
    // progressiveRenderer_.setQuality(RenderQuality::Draft);
    // progressiveRenderer_.setDraftQuality(4);
    // progressiveRenderer_.setPreviewQuality(2);
    break;
  case PreviewQualityPreset::Preview:
    // progressiveRenderer_.setQuality(RenderQuality::Preview);
    // progressiveRenderer_.setDraftQuality(4);
    // progressiveRenderer_.setPreviewQuality(2);
    break;
  case PreviewQualityPreset::Final:
    // progressiveRenderer_.setQuality(RenderQuality::Final);
    // progressiveRenderer_.setDraftQuality(2);
    // progressiveRenderer_.setPreviewQuality(1);
    break;
  }
}

PreviewQualityPreset
ArtifactProjectService::Impl::previewQualityPreset() const {
  return qualityPreset_;
}

UniString ArtifactProjectService::Impl::projectName() const {
  if (auto project = projectManager().getCurrentProjectSharedPtr()) {
    return UniString(project->settings().projectName());
  }
  return UniString();
}

void ArtifactProjectService::Impl::changeProjectName(const UniString &name) {
  if (auto project = projectManager().getCurrentProjectSharedPtr()) {
    project->settings().setProjectName(name.toQString());
  }
}

ChangeCompositionResult ArtifactProjectService::Impl::changeCurrentComposition(
    const CompositionID &id) {
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
    if (auto *active = ArtifactActiveContextService::instance()) {
      active->setActiveComposition(comp);
    }
  }

  result.success = true;
  return result;
}

void ArtifactProjectService::Impl::removeAllAssets() {}

FindCompositionResult
ArtifactProjectService::Impl::findComposition(const CompositionID &id) {
  return ArtifactProjectManager::getInstance().findComposition(id);
}

ArtifactCompositionWeakPtr ArtifactProjectService::Impl::currentComposition() {
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

ArtifactProjectService::ArtifactProjectService(QObject *parent)
    : QObject(parent), impl_(new Impl()) {
  impl_->eventBusSubscriptions_.push_back(
      impl_->eventBus_.subscribe<ProjectCreatedEvent>([this](const ProjectCreatedEvent&) {
        impl_->currentCompositionId_ = {};
        ArtifactRevisionService::instance()->noteProjectChanged();
      }));
  impl_->eventBusSubscriptions_.push_back(
      impl_->eventBus_.subscribe<ProjectChangedEvent>([this](const ProjectChangedEvent&) {
        ArtifactRevisionService::instance()->noteProjectChanged();
      }));
  impl_->installSelectionBridge(this);
}

ArtifactProjectService::~ArtifactProjectService() { delete impl_; }

ArtifactProjectService *ArtifactProjectService::instance() {
  static ArtifactProjectService service;
  return &service;
}

bool ArtifactProjectService::hasProject() const {
  return impl_->projectManager().getCurrentProjectSharedPtr() != nullptr;
}

void ArtifactProjectService::projectSettingChanged(
    const ArtifactProjectSettings &setting) {}

void ArtifactProjectService::selectLayer(const LayerID &id) {
  if (auto *selectionManager = ArtifactLayerSelectionManager::instance()) {
    const auto currentComp = currentComposition().lock();
    const auto current = selectionManager->currentLayer();
    if (current && current->id() == id) {
      selectionManager->setActiveComposition(currentComp);
      ArtifactCore::globalEventBus().publish<LayerSelectionChangedEvent>(
          LayerSelectionChangedEvent{
              currentComp ? currentComp->id().toString() : QString(),
              id.toString(),
              LayerSelectionChangeReason::ProgrammaticReselect});
      return;
    }

    if (!id.isNil() && (!currentComp || !currentComp->layerById(id))) {
      qDebug() << "[ProjectService] selectLayer rejected"
               << "composition="
               << (currentComp ? currentComp->id().toString() : QString())
               << "layer=" << id.toString()
               << "reason=" << layerSelectionChangeReasonToString(
                      currentComp ? LayerSelectionChangeReason::InvalidSelection
                                  : LayerSelectionChangeReason::ProjectChanged);
    }

    impl_->forwardingSelectionChange_ = true;
    selectionManager->setActiveComposition(currentComp);
    if (id.isNil()) {
      selectionManager->clearSelection();
    } else if (currentComp) {
      selectionManager->selectLayer(currentComp->layerById(id));
    } else {
      selectionManager->clearSelection();
    }
    impl_->forwardingSelectionChange_ = false;

    const auto resolvedCurrent = selectionManager->currentLayer();
    const LayerID nextId = resolvedCurrent ? resolvedCurrent->id()
                                           : (id.isNil() ? LayerID() : id);
    const auto reason = id.isNil()
                            ? LayerSelectionChangeReason::UserCleared
                            : (resolvedCurrent
                                   ? LayerSelectionChangeReason::SelectionBridgeSync
                                   : LayerSelectionChangeReason::InvalidSelection);
    ArtifactCore::globalEventBus().publish<LayerSelectionChangedEvent>(
        LayerSelectionChangedEvent{
            currentComp ? currentComp->id().toString() : QString(),
            nextId.toString(),
            reason});
    qDebug() << "[ProjectService] selectLayer"
             << "composition="
             << (currentComp ? currentComp->id().toString() : QString())
             << "requested=" << id.toString()
             << "resolved=" << nextId.toString()
             << "reason=" << layerSelectionChangeReasonToString(reason);
  }
}

void ArtifactProjectService::addLayer(const CompositionID &id,
                                      const ArtifactLayerInitParams &param) {
  auto result = impl_->projectManager().addLayerToComposition(
      id, const_cast<ArtifactLayerInitParams &>(param));
  if (result.success) {
    notifyProjectMutation(impl_->projectManager());
  }
}

void ArtifactProjectService::addLayerToCurrentComposition(
    const ArtifactLayerInitParams &params) {
  impl_->addLayerToCurrentComposition(params, true);
}

void ArtifactProjectService::Impl::setDefaultNewLayerHidden(bool hidden) {
  defaultNewLayerHidden_ = hidden;
}

bool ArtifactProjectService::Impl::defaultNewLayerHidden() const {
  return defaultNewLayerHidden_;
}

void ArtifactProjectService::addLayerToCurrentComposition(
    const ArtifactLayerInitParams &params, bool selectNewLayer) {
  impl_->addLayerToCurrentComposition(params, selectNewLayer);
}

void ArtifactProjectService::addLayerToCurrentComposition(
    const ArtifactLayerInitParams &params, bool selectNewLayer,
    bool placeAtCurrentFrame) {
  impl_->addLayerToCurrentComposition(
      params, selectNewLayer, placeAtCurrentFrame,
      impl_->defaultNewLayerHidden());
}

void ArtifactProjectService::addLayerToCurrentComposition(
    const ArtifactLayerInitParams &params, bool selectNewLayer,
    bool placeAtCurrentFrame, bool startHidden) {
  impl_->addLayerToCurrentComposition(params, selectNewLayer, placeAtCurrentFrame, startHidden);
}

void ArtifactProjectService::setDefaultNewLayerHidden(bool hidden) {
  impl_->setDefaultNewLayerHidden(hidden);
}

bool ArtifactProjectService::defaultNewLayerHidden() const {
  return impl_->defaultNewLayerHidden();
}

bool ArtifactProjectService::ungroupSelectedGroupInCurrentComposition()
{
    auto comp = currentComposition().lock();
    if (!comp) {
        return false;
    }

    auto* selectionManager = ArtifactLayerSelectionManager::instance();
    if (!selectionManager) {
        return false;
    }

    // 現在選択されているのがグループか確認
    auto selectedLayer = selectionManager->currentLayer();
    if (!selectedLayer || !selectedLayer->isGroupLayer()) {
        return false;
    }

    auto groupLayer = std::dynamic_pointer_cast<ArtifactGroupLayer>(selectedLayer);
    if (!groupLayer) {
        return false;
    }

    // グループの子を退避
    const auto children = groupLayer->children();
    if (children.empty()) {
        // 空のグループは削除
        removeLayerFromComposition(comp->id(), groupLayer->id());
        return true;
    }

    // グループのインデックスを取得
    const auto allLayers = comp->allLayer();
    int groupIndex = -1;
    for (int i = 0; i < allLayers.size(); ++i) {
        if (allLayers[i]->id() == groupLayer->id()) {
            groupIndex = i;
            break;
        }
    }

    if (groupIndex < 0) {
        return false;
    }

    // 子をグループから外して親のレベルに移動
    // 子を逆順に処理（元の順序を維持するため）
    for (int i = children.size() - 1; i >= 0; --i) {
        const auto& child = children[i];
        if (!child) continue;

        // グループから子を外す
        groupLayer->removeChild(child->id());

        // 親のCompositionに直接追加（グループの位置に）
        comp->insertLayerAt(child, groupIndex);
    }

    // グループを削除
    removeLayerFromComposition(comp->id(), groupLayer->id());

    // 元の子らを選択状態に設定
    QVector<LayerID> childIds;
    childIds.reserve(children.size());
    for (const auto& child : children) {
        if (child) {
            childIds.push_back(child->id());
        }
    }
    selectionManager->clearSelection();
    for (const auto& child : children) {
        if (child) {
            selectionManager->addToSelection(child);
        }
    }

    return true;
}

bool ArtifactProjectService::groupSelectedLayersInCurrentComposition(
    const UniString &groupName) {
  auto comp = currentComposition().lock();
  if (!comp) {
    return false;
  }

  auto *selectionManager = ArtifactLayerSelectionManager::instance();
  if (!selectionManager) {
    return false;
  }

  const auto selected = selectionManager->selectedLayers();
  QVector<LayerID> selectedIds;
  selectedIds.reserve(selected.size());
  for (const auto &layer : selected) {
    if (!layer || layer->composition() != comp.get()) {
      continue;
    }
    selectedIds.push_back(layer->id());
  }

  if (selectedIds.isEmpty()) {
    return false;
  }

  ArtifactLayerInitParams groupParams(groupName.toQString(), LayerType::Group);
  addLayerToCurrentComposition(groupParams);

  auto newGroup = selectionManager->currentLayer();
  if (!newGroup || !newGroup->isGroupLayer()) {
    return false;
  }

  const LayerID groupId = newGroup->id();
  bool anyReparented = false;
  for (const auto &layerId : selectedIds) {
    if (layerId == groupId) {
      continue;
    }
    anyReparented |= setLayerParentInCurrentComposition(layerId, groupId);
  }

  if (auto groupLayer = std::dynamic_pointer_cast<ArtifactGroupLayer>(newGroup)) {
    groupLayer->setCollapsed(false);
  }
  selectLayer(groupId);
  return anyReparented;
}

bool ArtifactProjectService::removeLayerFromComposition(
    const CompositionID &compositionId, const LayerID &layerId) {
  ArtifactAbstractLayerPtr removedLayer;
  ArtifactAbstractLayerPtr selectedLayer;
  auto *selectionManager = ArtifactLayerSelectionManager::instance();
  if (auto comp = currentComposition().lock()) {
    removedLayer = comp->layerById(layerId);
    if (selectionManager) {
      selectedLayer = selectionManager->currentLayer();
    }
  }

  bool ok = impl_->projectManager().removeLayerFromComposition(compositionId,
                                                               layerId);
  if (ok) {
    const bool selectionMatchedRemoved =
        selectedLayer && selectedLayer->id() == layerId;
    if (selectionManager && selectionMatchedRemoved) {
      selectionManager->removeFromSelection(selectedLayer);
    }

    // [Optimization] Only emit layerRemoved. notifyProjectMutation triggers
    // global projectChanged which causes heavy UI rebuilds. Most widgets handle
    // layerRemoved specifically.
    qDebug() << "[ProjectService] removeLayerFromComposition"
             << "composition=" << compositionId.toString()
             << "layer=" << layerId.toString()
             << "selectedMatchedRemoved=" << selectionMatchedRemoved
             << "hadLayer=" << static_cast<bool>(removedLayer);
  }
  return ok;
}
bool ArtifactProjectService::moveLayerInCurrentComposition(
    const LayerID &layerId, int newIndex) {
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

bool ArtifactProjectService::duplicateLayerInCurrentComposition(
    const LayerID &layerId) {
  auto comp = currentComposition().lock();
  if (!comp || layerId.isNil()) {
    return false;
  }

  auto result =
      impl_->projectManager().duplicateLayerInComposition(comp->id(), layerId);
  if (!result.success || !result.layer) {
    return false;
  }

  const auto layers = comp->allLayer();
  int sourceIndex = -1;
  int newIndex = -1;
  for (int i = 0; i < layers.size(); ++i) {
    const auto &layer = layers[i];
    if (!layer) {
      continue;
    }
    if (layer->id() == layerId) {
      sourceIndex = i;
    }
    if (layer->id() == result.layer->id()) {
      newIndex = i;
    }
  }

  if (sourceIndex >= 0 && newIndex >= 0 && newIndex != sourceIndex) {
    comp->moveLayerToIndex(result.layer->id(), sourceIndex);
    notifyProjectMutation(impl_->projectManager());
  }
  selectLayer(result.layer->id());
  return true;
}

bool ArtifactProjectService::renameLayerInCurrentComposition(
    const LayerID &layerId, const QString &newName) {
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

bool ArtifactProjectService::replaceLayerSourceInCurrentComposition(
    const LayerID &layerId, const QString &sourcePath) {
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
  } else if (auto svgLayer =
                 std::dynamic_pointer_cast<ArtifactSvgLayer>(layer)) {
    replaced = svgLayer->loadFromPath(trimmed);
  } else if (auto audioLayer =
                 std::dynamic_pointer_cast<ArtifactAudioLayer>(layer)) {
    replaced = audioLayer->loadFromPath(trimmed);
  } else if (auto videoLayer =
                 std::dynamic_pointer_cast<ArtifactVideoLayer>(layer)) {
    replaced = videoLayer->loadFromPath(trimmed);
  }

  if (!replaced) {
    return false;
  }

  notifyProjectMutation(impl_->projectManager());
  return true;
}

bool ArtifactProjectService::isLayerVisibleInCurrentComposition(
    const LayerID &layerId) {
  auto comp = currentComposition().lock();
  if (!comp || layerId.isNil()) {
    return false;
  }
  auto layer = comp->layerById(layerId);
  return layer ? layer->isVisible() : false;
}

bool ArtifactProjectService::isLayerLockedInCurrentComposition(
    const LayerID &layerId) {
  auto comp = currentComposition().lock();
  if (!comp || layerId.isNil()) {
    return false;
  }
  auto layer = comp->layerById(layerId);
  return layer ? (layer->isLocked() || layer->isTimingLocked()) : false;
}

bool ArtifactProjectService::isLayerSoloInCurrentComposition(
    const LayerID &layerId) {
  auto comp = currentComposition().lock();
  if (!comp || layerId.isNil()) {
    return false;
  }
  auto layer = comp->layerById(layerId);
  return layer ? layer->isSolo() : false;
}

bool ArtifactProjectService::isLayerShyInCurrentComposition(
    const LayerID &layerId) {
  auto comp = currentComposition().lock();
  if (!comp || layerId.isNil()) {
    return false;
  }
  auto layer = comp->layerById(layerId);
  return layer ? layer->isShy() : false;
}

bool ArtifactProjectService::setLayerVisibleInCurrentComposition(
    const LayerID &layerId, bool visible) {
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

bool ArtifactProjectService::setLayerLockedInCurrentComposition(
    const LayerID &layerId, bool locked) {
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

bool ArtifactProjectService::setLayerSoloInCurrentComposition(
    const LayerID &layerId, bool solo) {
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

bool ArtifactProjectService::setLayerShyInCurrentComposition(
    const LayerID &layerId, bool shy) {
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

bool ArtifactProjectService::soloOnlyLayerInCurrentComposition(
    const LayerID &layerId) {
  auto comp = currentComposition().lock();
  if (!comp || layerId.isNil()) {
    return false;
  }
  auto selected = comp->layerById(layerId);
  if (!selected) {
    return false;
  }

  for (const auto &candidate : comp->allLayer()) {
    if (!candidate)
      continue;
    candidate->setSolo(candidate->id() == layerId);
  }
  notifyProjectMutation(impl_->projectManager());
  return true;
}

bool ArtifactProjectService::smartSoloOnlyLayerInCurrentComposition(
    const LayerID &layerId) {
  auto comp = currentComposition().lock();
  if (!comp || layerId.isNil()) {
    return false;
  }

  auto selected = comp->layerById(layerId);
  if (!selected) {
    return false;
  }

  const QSet<QString> smartSoloLayerIds =
      resolveSmartSoloLayerIds(comp, layerId);
  if (smartSoloLayerIds.isEmpty()) {
    return false;
  }

  for (const auto &candidate : comp->allLayer()) {
    if (!candidate) {
      continue;
    }
    candidate->setSolo(smartSoloLayerIds.contains(candidate->id().toString()));
  }
  notifyProjectMutation(impl_->projectManager());
  return true;
}

bool ArtifactProjectService::setLayerParentInCurrentComposition(
    const LayerID &layerId, const LayerID &parentLayerId) {
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

bool ArtifactProjectService::precomposeLayersInCurrentComposition(
    const QVector<LayerID> &layerIds, const UniString &newCompositionName,
    bool openNewComposition, bool matchWorkspaceDuration,
    PrecomposeMode mode) {
  auto project = getCurrentProjectSharedPtr();
  auto comp = currentComposition().lock();
  // Reset the outcome up front so a failure cannot leak a stale id.
  impl_->lastPrecomposeOutcome_ = PrecomposeOutcome{LayerID(), CompositionID()};
  if (!project || !comp) {
    return false;
  }
  // MoveAllAttributes folds the whole composition in, so an empty layerIds is
  // acceptable in that mode. MoveSelected still requires an explicit selection.
  if (mode != PrecomposeMode::MoveAllAttributes && layerIds.isEmpty()) {
    return false;
  }

  QVector<ArtifactAbstractLayerPtr> orderedLayers;
  QVector<LayerID> originalParentIds;
  orderedLayers.reserve(layerIds.size());
  originalParentIds.reserve(layerIds.size());
  auto containsLayerId = [&layerIds](const LayerID &id) {
    for (const auto &candidate : layerIds) {
      if (candidate == id) {
        return true;
      }
    }
    return false;
  };
  const bool moveAll = (mode == PrecomposeMode::MoveAllAttributes);

  int firstIndex = -1;
  int64_t minInFrame = 0;
  int64_t maxOutFrame = 0;
  bool firstTiming = true;
  const auto allLayers = comp->allLayer();
  for (int i = 0; i < allLayers.size(); ++i) {
    const auto &layer = allLayers[i];
    if (!layer) {
      continue;
    }
    // MoveAllAttributes takes every layer; MoveSelected filters by the id set.
    if (!moveAll && !containsLayerId(layer->id())) {
      continue;
    }
    if (firstIndex < 0) {
      firstIndex = i;
    }
    orderedLayers.push_back(layer);
    originalParentIds.push_back(layer->parentLayerId());
    const int64_t inFrame = layer->inPoint().framePosition();
    const int64_t outFrame = layer->outPoint().framePosition();
    if (firstTiming) {
      minInFrame = inFrame;
      maxOutFrame = outFrame;
      firstTiming = false;
    } else {
      minInFrame = std::min<int64_t>(minInFrame, inFrame);
      maxOutFrame = std::max<int64_t>(maxOutFrame, outFrame);
    }
  }

  if (orderedLayers.isEmpty() || firstIndex < 0) {
    return false;
  }

  // Guard against self-nesting: refuse to precompose if any selected layer is a
  // composition layer whose (transitive) source contains this composition —
  // that would create a cycle once the layers move into the new child comp.
  if (wouldCreatePrecomposeCycle(comp, orderedLayers)) {
    qWarning() << "[precompose] rejected: would create composition cycle";
    return false;
  }

  if (newCompositionName.toQString().trimmed().isEmpty()) {
    return false;
  }

  const int64_t childDurationFrames = [&]() -> int64_t {
    if (matchWorkspaceDuration) {
      const int64_t workDuration = comp->workAreaRange().duration();
      if (workDuration > 0) {
        return workDuration;
      }
    }
    return std::max<int64_t>(1, comp->frameRange().duration());
  }();
  ArtifactCompositionInitParams childParams;
  childParams.setCompositionName(newCompositionName);
  childParams.setResolution(comp->settings().compositionSize().width(),
                            comp->settings().compositionSize().height());
  childParams.setFrameRate(comp->frameRate());
  childParams.setDurationFrames(childDurationFrames);
  childParams.setBackgroundColor(comp->backgroundColor());
  const int64_t fpsScale =
      std::max<int64_t>(1, static_cast<int64_t>(std::llround(
                               comp->frameRate().framerate())));
  childParams.setWorkArea(RationalTime(0, fpsScale),
                          RationalTime(childDurationFrames, fpsScale));

  auto childComp = std::make_shared<ArtifactAbstractComposition>(
      CompositionID(), childParams);
  if (!childComp) {
    return false;
  }

  if (!project->addImportedComposition(childComp, newCompositionName.toQString())) {
    return false;
  }

  const CompositionID childCompId = childComp->id();
  QVector<LayerID> movedLayerIds;
  movedLayerIds.reserve(orderedLayers.size());

  for (const auto &layer : orderedLayers) {
    if (!layer) {
      continue;
    }
    const LayerID layerId = layer->id();
    const LayerID parentId = layer->parentLayerId();
    if (!parentId.isNil() && !containsLayerId(parentId)) {
      layer->clearParent();
    }
    if (!project->addLayerToComposition(childCompId, layer).success) {
      return false;
    }
    movedLayerIds.push_back(layerId);
  }

  for (const auto &layer : orderedLayers) {
    if (!layer) {
      continue;
    }
    if (!project->removeLayerFromComposition(comp->id(), layer->id())) {
      return false;
    }
  }

  for (const auto &layer : orderedLayers) {
    if (!layer) {
      continue;
    }
    layer->setComposition(childComp.get());
  }

  for (int i = 0; i < orderedLayers.size(); ++i) {
    const auto &layer = orderedLayers[i];
    if (!layer) {
      continue;
    }
    const LayerID parentId = originalParentIds.value(i);
    if (!parentId.isNil() && containsLayerId(parentId)) {
      layer->setParentById(parentId);
    } else {
      layer->clearParent();
    }
  }

  ArtifactCompositionLayerInitParams precompParams;
  ArtifactLayerResult precompLayerResult =
      project->createLayerAndAddToComposition(comp->id(), precompParams);
  if (!precompLayerResult.success || !precompLayerResult.layer) {
    return false;
  }

  auto precompLayer =
      std::dynamic_pointer_cast<ArtifactCompositionLayer>(precompLayerResult.layer);
  if (!precompLayer) {
    return false;
  }

  precompLayer->setLayerName(newCompositionName.toQString());
  precompLayer->setCompositionId(childCompId);
  precompLayer->setRestoreMoveAllAttributes(moveAll);
  precompLayer->clearRestoreExternalParents();
  for (int i = 0; i < orderedLayers.size(); ++i) {
    const auto &layer = orderedLayers[i];
    if (!layer) {
      continue;
    }
    const LayerID parentId = originalParentIds.value(i);
    if (!parentId.isNil() && !containsLayerId(parentId)) {
      precompLayer->setRestoreExternalParent(layer->id(), parentId);
    }
  }
  if (moveAll) {
    // The new child holds the whole original timeline verbatim (layers keep
    // their in/out), so the precomp layer plays it from frame 0 for the full
    // duration — matching AE's "Move all attributes" semantics.
    precompLayer->setInPoint(FramePosition(0));
    precompLayer->setOutPoint(FramePosition(childDurationFrames));
    precompLayer->setStartTime(FramePosition(0));
  } else {
    // Selected-only mode: the precomp layer stands in for the moved layers'
    // original extent, offsetting the child's playback to line up with the
    // earliest moved layer.
    precompLayer->setInPoint(FramePosition(minInFrame));
    precompLayer->setOutPoint(FramePosition(maxOutFrame));
    precompLayer->setStartTime(FramePosition(minInFrame));
  }

  comp->moveLayerToIndex(precompLayer->id(), firstIndex);
  // Stash the outcome so the caller can build an undo command targeting the
  // freshly created precomp layer / child composition.
  impl_->lastPrecomposeOutcome_ = PrecomposeOutcome{precompLayer->id(), childCompId};
  ArtifactCore::globalEventBus().publish<ProjectChangedEvent>({QString(), QString()});

  if (openNewComposition) {
    changeCurrentComposition(childCompId);
    if (!movedLayerIds.isEmpty()) {
      selectLayer(movedLayerIds.front());
    } else {
      selectLayer(LayerID());
    }
  } else {
    selectLayer(precompLayer->id());
  }

  return true;
}

bool ArtifactProjectService::unprecomposeLayerInCurrentComposition(
    const LayerID &layerId, bool keepComposition) {
  auto project = getCurrentProjectSharedPtr();
  auto comp = currentComposition().lock();
  // Reset undo bookkeeping up front so a failure cannot leak stale ids.
  impl_->lastUnprecomposePrecompLayerId_ = LayerID();
  impl_->lastUnprecomposeChildCompId_ = CompositionID();
  impl_->lastUnprecomposeMovedLayerIds_.clear();
  impl_->lastUnprecomposeChildName_ = UniString();
  if (!project || !comp || layerId.isNil()) {
    return false;
  }

  auto layer = comp->layerById(layerId);
  auto compLayer = layer ? std::dynamic_pointer_cast<ArtifactCompositionLayer>(layer)
                         : nullptr;
  if (!compLayer) {
    return false;
  }

  const CompositionID childCompId = compLayer->sourceCompositionId();
  if (childCompId.isNil() || childCompId == comp->id()) {
    return false;
  }

  auto childFind = findComposition(childCompId);
  auto childComp = childFind.ptr.lock();
  if (!childFind.success || !childComp) {
    return false;
  }

  const auto childLayers = childComp->allLayer();
  if (childLayers.isEmpty()) {
    // An empty precompose has no layer state to restore and should not be
    // consumed by the command/undo path as a successful operation.
    return false;
  }
  // Validate the complete restore set before mutating either composition.
  // addLayerToComposition() can succeed for an early layer and fail later;
  // rejecting collisions up front keeps unprecompose atomic at this layer.
  QSet<QString> childLayerIdSet;
  childLayerIdSet.reserve(childLayers.size());
  for (const auto &childLayer : childLayers) {
    if (!childLayer || childLayer->id().isNil() ||
        childLayerIdSet.contains(childLayer->id().toString())) {
      return false;
    }
    if (comp->containsLayerById(childLayer->id())) {
      return false;
    }
    childLayerIdSet.insert(childLayer->id().toString());
  }
  const bool restoreMoveAll = compLayer->restoreMoveAllAttributes();
  const qint64 timeOffsetFrames =
      !restoreMoveAll ? compLayer->startTime().framePosition() : 0;
  int insertIndex = -1;
  const auto parentLayers = comp->allLayer();
  for (int i = 0; i < parentLayers.size(); ++i) {
    const auto &candidate = parentLayers[i];
    if (candidate && candidate->id() == layerId) {
      insertIndex = i;
      break;
    }
  }
  if (insertIndex < 0) {
    return false;
  }

  QVector<LayerID> movedLayerIds;
  movedLayerIds.reserve(childLayers.size());
  QSet<QString> movedLayerIdSet;
  movedLayerIdSet.reserve(childLayers.size());
  for (const auto &childLayer : childLayers) {
    if (!childLayer) {
      continue;
    }
    movedLayerIds.push_back(childLayer->id());
    movedLayerIdSet.insert(childLayer->id().toString());
    if (!project->addLayerToComposition(comp->id(), childLayer).success) {
      for (const auto &addedId : movedLayerIds) {
        project->removeLayerFromComposition(comp->id(), addedId);
      }
      return false;
    }
  }

  if (!project->removeLayerFromComposition(comp->id(), layerId)) {
    for (const auto &addedId : movedLayerIds) {
      project->removeLayerFromComposition(comp->id(), addedId);
      for (const auto &childLayer : childLayers) {
        if (childLayer && childLayer->id() == addedId) {
          project->addLayerToComposition(childCompId, childLayer);
          break;
        }
      }
    }
    return false;
  }

  for (int i = 0; i < movedLayerIds.size(); ++i) {
    const LayerID &movedId = movedLayerIds[i];
    if (!project->removeLayerFromComposition(childCompId, movedId)) {
      for (const auto &addedId : movedLayerIds) {
        project->removeLayerFromComposition(comp->id(), addedId);
        for (const auto &childLayer : childLayers) {
          if (childLayer && childLayer->id() == addedId) {
            project->addLayerToComposition(childCompId, childLayer);
            break;
          }
        }
      }
      return false;
    }
    auto movedLayer = comp->layerById(movedId);
    if (movedLayer) {
      movedLayer->setComposition(comp.get());
      if (timeOffsetFrames != 0) {
        movedLayer->slideTimingBy(timeOffsetFrames);
      }
      const LayerID restoredExternalParent =
          compLayer->restoreExternalParent(movedId);
      if (!restoredExternalParent.isNil() &&
          comp->containsLayerById(restoredExternalParent)) {
        movedLayer->setParentById(restoredExternalParent);
      } else {
        const LayerID currentParentId = movedLayer->parentLayerId();
        if (!currentParentId.isNil() &&
            !movedLayerIdSet.contains(currentParentId.toString()) &&
            !comp->containsLayerById(currentParentId)) {
          movedLayer->clearParent();
        }
      }
    }
    comp->moveLayerToIndex(movedId, std::max(0, insertIndex + i));
  }

  // Record the moved layer ids for undo (re-precompose), after success.
  impl_->lastUnprecomposePrecompLayerId_ = layerId;
  impl_->lastUnprecomposeChildCompId_ = childCompId;
  impl_->lastUnprecomposeChildName_ =
      UniString(childComp->settings().compositionName());
  impl_->lastUnprecomposeMovedLayerIds_ = movedLayerIds;

  if (!keepComposition) {
    if (!removeComposition(childCompId)) {
      for (const auto &movedId : movedLayerIds) {
        project->removeLayerFromComposition(comp->id(), movedId);
        for (const auto &childLayer : childLayers) {
          if (childLayer && childLayer->id() == movedId) {
            project->addLayerToComposition(childCompId, childLayer);
            break;
          }
        }
      }
      project->addLayerToComposition(comp->id(), compLayer);
      impl_->lastUnprecomposePrecompLayerId_ = LayerID();
      impl_->lastUnprecomposeChildCompId_ = CompositionID();
      impl_->lastUnprecomposeChildName_ = UniString();
      impl_->lastUnprecomposeMovedLayerIds_.clear();
      return false;
    }
  }

  if (!movedLayerIds.isEmpty()) {
    selectLayer(movedLayerIds.front());
  } else {
    selectLayer(LayerID());
  }
  ArtifactCore::globalEventBus().publish<ProjectChangedEvent>({QString(), QString()});
  return true;
}

PrecomposeOutcome ArtifactProjectService::lastPrecomposeOutcome() const {
  return impl_->lastPrecomposeOutcome_;
}

LayerID ArtifactProjectService::lastUnprecomposePrecompLayerId() const {
  return impl_->lastUnprecomposePrecompLayerId_;
}

CompositionID ArtifactProjectService::lastUnprecomposeChildCompId() const {
  return impl_->lastUnprecomposeChildCompId_;
}

QVector<LayerID> ArtifactProjectService::lastUnprecomposeMovedLayerIds() const {
  return impl_->lastUnprecomposeMovedLayerIds_;
}

UniString ArtifactProjectService::lastUnprecomposeChildName() const {
  return impl_->lastUnprecomposeChildName_;
}

bool ArtifactProjectService::precomposeLayersWithUndo(
    const QVector<LayerID> &layerIds, const UniString &newCompositionName,
    bool openNewComposition, bool matchWorkspaceDuration,
    PrecomposeMode mode) {
  auto *mgr = UndoManager::instance();
  if (!mgr) {
    // No undo infrastructure available — fall back to a direct mutation.
    return precomposeLayersInCurrentComposition(
        layerIds, newCompositionName, openNewComposition,
        matchWorkspaceDuration, mode);
  }
  auto cmd = std::make_unique<PrecomposeUndoCommand>(
      layerIds, newCompositionName, openNewComposition, matchWorkspaceDuration,
      mode);
  // UndoManager::push runs redo() once for us.
  mgr->push(std::move(cmd));
  const PrecomposeOutcome outcome = lastPrecomposeOutcome();
  return !outcome.precompLayerId.isNil() && !outcome.childCompId.isNil();
}

bool ArtifactProjectService::unprecomposeLayerWithUndo(
    const LayerID &layerId, bool keepComposition) {
  auto *mgr = UndoManager::instance();
  if (!mgr) {
    return unprecomposeLayerInCurrentComposition(layerId, keepComposition);
  }
  auto cmd = std::make_unique<UnprecomposeUndoCommand>(layerId, keepComposition);
  mgr->push(std::move(cmd));
  // After redo, the precomp layer is gone; success is implied by a non-empty
  // moved-layer record.
  return !lastUnprecomposeMovedLayerIds().isEmpty();
}

bool ArtifactProjectService::groupSelectedLayersWithUndo(
    const UniString &groupName) {
  auto *mgr = UndoManager::instance();
  if (!mgr) {
    return groupSelectedLayersInCurrentComposition(groupName);
  }
  auto *sel = ArtifactLayerSelectionManager::instance();
  if (!sel) return false;
  const auto selected = sel->selectedLayers();
  QVector<LayerID> ids;
  ids.reserve(selected.size());
  for (const auto &l : selected) {
    if (l) ids.push_back(l->id());
  }
  if (ids.isEmpty()) return false;
  auto cmd = std::make_unique<GroupLayersUndoCommand>(ids, groupName);
  mgr->push(std::move(cmd));
  return !ids.isEmpty();
}

bool ArtifactProjectService::ungroupSelectedGroupWithUndo() {
  auto *mgr = UndoManager::instance();
  if (!mgr) {
    return ungroupSelectedGroupInCurrentComposition();
  }
  auto *sel = ArtifactLayerSelectionManager::instance();
  if (!sel) return false;
  auto current = sel->currentLayer();
  if (!current || !current->isGroupLayer()) return false;
  auto cmd = std::make_unique<UngroupLayersUndoCommand>(
      current->id(), UniString(current->layerName()));
  mgr->push(std::move(cmd));
  return true;
}

bool ArtifactProjectService::splitLayerWithUndo(
    const CompositionID &compositionId, const LayerID &layerId) {
  auto *mgr = UndoManager::instance();
  if (!mgr) {
    // No undo — fall back to direct mutation
    splitLayerAtCurrentTime(compositionId, layerId);
    return true;
  }
  auto cmd = std::make_unique<SplitLayerUndoCommand>(compositionId, layerId);
  mgr->push(std::move(cmd));
  return true;
}

void ArtifactProjectService::splitLayerAtCurrentTime(
    const CompositionID &compositionId, const LayerID &layerId) {
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
  if (layer->isTimingLocked()) {
    return;
  }
  layer->setOutPoint(now);

  auto result = impl_->projectManager().duplicateLayerInComposition(
      compositionId, layerId);
  if (!result.success || !result.layer) {
    layer->setOutPoint(oldOut);
    return;
  }

  auto newLayer = result.layer;
  if (newLayer->isTimingLocked()) {
    layer->setOutPoint(oldOut);
    return;
  }
  newLayer->setInPoint(now);
  newLayer->setOutPoint(oldOut);
  ArtifactCore::globalEventBus().publish<ProjectChangedEvent>({QString(), QString()});
}

bool ArtifactProjectService::clearLayerParentInCurrentComposition(
    const LayerID &layerId) {
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

bool ArtifactProjectService::layerHasParentInCurrentComposition(
    const LayerID &layerId) {
  auto comp = currentComposition().lock();
  if (!comp || layerId.isNil()) {
    return false;
  }
  auto layer = comp->layerById(layerId);
  return layer ? layer->hasParent() : false;
}

LayerID ArtifactProjectService::layerParentIdInCurrentComposition(
    const LayerID &layerId) {
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

QString
ArtifactProjectService::layerNameInCurrentComposition(const LayerID &layerId) {
  auto comp = currentComposition().lock();
  if (!comp || layerId.isNil()) {
    return QString();
  }
  auto layer = comp->layerById(layerId);
  return layer ? layer->layerName() : QString();
}

bool ArtifactProjectService::addEffectToLayerInCurrentComposition(
    const LayerID &layerId, std::shared_ptr<ArtifactAbstractEffect> effect) {
  auto comp = currentComposition().lock();
  if (!comp || layerId.isNil() || !effect) {
    return false;
  }
  auto layer = comp->layerById(layerId);
  if (!layer) {
    return false;
  }
  const QString uniqueId = uniqueEffectIdForLayer(
      layer->getEffects(), effect->displayName().toQString(),
      effect->effectID().toQString());
  effect->setEffectID(UniString::fromQString(uniqueId));
  layer->addEffect(effect);
  // A layer-scoped queued notification is sufficient here. Publishing the
  // same LayerChangedEvent synchronously and then posting it again forced
  // rasterizer surface invalidation twice while the add action was active.
  notifyLayerMutation(comp->id().toString(), layerId);
  return true;
}

bool ArtifactProjectService::removeEffectFromLayerInCurrentComposition(
    const LayerID &layerId, const QString &effectId) {
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
  ArtifactCore::globalEventBus().publish(LayerChangedEvent{
      comp->id().toString(), layerId.toString(),
      LayerChangedEvent::ChangeType::Modified});
  notifyLayerMutation(comp->id().toString(), layerId);
  notifyProjectMutation(impl_->projectManager());
  return true;
}

bool ArtifactProjectService::setEffectEnabledInLayerInCurrentComposition(
    const LayerID &layerId, const QString &effectId, bool enabled) {
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

  for (const auto &effect : layer->getEffects()) {
    if (effect && effect->effectID().toQString() == effectId) {
      effect->setEnabled(enabled);
      ArtifactCore::globalEventBus().publish(LayerChangedEvent{
          comp->id().toString(), layerId.toString(),
          LayerChangedEvent::ChangeType::Modified});
      notifyLayerMutation(comp->id().toString(), layerId);
      notifyProjectMutation(impl_->projectManager());
      return true;
    }
  }
  return false;
}

bool ArtifactProjectService::moveEffectInLayerInCurrentComposition(
    const LayerID &layerId, const QString &effectId, int direction) {
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
    const auto &effect = effects[i];
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
  for (const auto &effect : effects) {
    if (effect) {
      layer->addEffect(effect);
    }
  }
  ArtifactCore::globalEventBus().publish(LayerChangedEvent{
      comp->id().toString(), layerId.toString(),
      LayerChangedEvent::ChangeType::Modified});
  notifyLayerMutation(comp->id().toString(), layerId);
  notifyProjectMutation(impl_->projectManager());
  return true;
}

bool ArtifactProjectService::addEffectToLayerWithUndo(
    const LayerID &layerId,
    std::shared_ptr<ArtifactAbstractEffect> effect) {
  auto *mgr = UndoManager::instance();
  if (!mgr) {
    return addEffectToLayerInCurrentComposition(layerId, std::move(effect));
  }
  auto cmd = std::make_unique<AddEffectUndoCommand>(layerId, std::move(effect));
  mgr->push(std::move(cmd));
  return true;
}

bool ArtifactProjectService::removeEffectFromLayerWithUndo(
    const LayerID &layerId, const QString &effectId,
    std::shared_ptr<ArtifactAbstractEffect> effect) {
  auto *mgr = UndoManager::instance();
  if (!mgr) {
    return removeEffectFromLayerInCurrentComposition(layerId, effectId);
  }
  auto cmd = std::make_unique<RemoveEffectUndoCommand>(layerId, std::move(effect));
  mgr->push(std::move(cmd));
  return true;
}

bool ArtifactProjectService::setEffectEnabledWithUndo(
    const LayerID &layerId, const QString &effectId,
    bool enabled, bool wasEnabled) {
  auto *mgr = UndoManager::instance();
  if (!mgr) {
    return setEffectEnabledInLayerInCurrentComposition(layerId, effectId, enabled);
  }
  auto cmd = std::make_unique<SetEffectEnabledUndoCommand>(
      layerId, effectId, wasEnabled, enabled);
  mgr->push(std::move(cmd));
  return true;
}

bool ArtifactProjectService::moveEffectWithUndo(
    const LayerID &layerId, const QString &effectId, int direction) {
  auto *mgr = UndoManager::instance();
  if (!mgr) {
    return moveEffectInLayerInCurrentComposition(layerId, effectId, direction);
  }
  auto cmd = std::make_unique<MoveEffectUndoCommand>(layerId, effectId, direction);
  mgr->push(std::move(cmd));
  return true;
}

bool ArtifactProjectService::addEffectToCurrentComposition(
    std::shared_ptr<ArtifactAbstractEffect> effect) {
  auto comp = currentComposition().lock();
  if (!comp || !effect) {
    return false;
  }
  const QString uniqueId = uniqueEffectIdForLayer(
      comp->getEffects(), effect->displayName().toQString(),
      effect->effectID().toQString());
  effect->setEffectID(UniString::fromQString(uniqueId));
  comp->addEffect(std::move(effect));
  notifyProjectMutation(impl_->projectManager());
  return true;
}

bool ArtifactProjectService::removeEffectFromCurrentComposition(
    const QString &effectId) {
  if (effectId.trimmed().isEmpty()) {
    return false;
  }
  auto comp = currentComposition().lock();
  if (!comp) {
    return false;
  }
  comp->removeEffect(UniString(effectId.toStdString()));
  notifyProjectMutation(impl_->projectManager());
  return true;
}

bool ArtifactProjectService::setEffectEnabledInCurrentComposition(
    const QString &effectId, bool enabled) {
  if (effectId.trimmed().isEmpty()) {
    return false;
  }
  auto comp = currentComposition().lock();
  if (!comp) {
    return false;
  }
  for (const auto &effect : comp->getEffects()) {
    if (effect && effect->effectID().toQString() == effectId) {
      effect->setEnabled(enabled);
      comp->changed();
      notifyProjectMutation(impl_->projectManager());
      return true;
    }
  }
  return false;
}

bool ArtifactProjectService::moveEffectInCurrentComposition(
    const QString &effectId, int direction) {
  if (effectId.trimmed().isEmpty() || direction == 0) {
    return false;
  }
  auto comp = currentComposition().lock();
  if (!comp) {
    return false;
  }
  auto effects = comp->getEffects();
  int currentIndex = -1;
  EffectPipelineStage currentStage = EffectPipelineStage::Generator;
  for (int i = 0; i < static_cast<int>(effects.size()); ++i) {
    const auto &effect = effects[i];
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
  comp->clearEffects();
  for (const auto &effect : effects) {
    if (effect) {
      comp->addEffect(effect);
    }
  }
  notifyProjectMutation(impl_->projectManager());
  return true;
}

QString ArtifactProjectService::layerRemovalConfirmationMessage(
    const CompositionID &compositionId, const LayerID &layerId) const {
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
  for (const auto &candidate : comp->allLayer()) {
    if (!candidate)
      continue;
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
  return QStringLiteral("レイヤー \"%1\" を削除しますか？\n"
                        "子レイヤー: %2 / エフェクト: %3\n"
                        "この操作は元に戻せない場合があります。")
      .arg(layerName)
      .arg(childCount)
      .arg(effectCount);
}

bool ArtifactProjectService::removeProjectItem(ProjectItem *item) {
  if (!item) {
    return false;
  }
  if (item->type() == eProjectItemType::Composition) {
    auto *compItem = static_cast<CompositionItem *>(item);
    return removeCompositionWithRenderQueueCleanup(compItem->compositionId);
  }

  auto shared = getCurrentProjectSharedPtr();
  if (!shared) {
    return false;
  }
  shared->removeItem(item);
  return true;
}

bool ArtifactProjectService::moveProjectItem(ProjectItem *item,
                                             ProjectItem *newParent) {
  if (!item || !newParent) {
    return false;
  }
  auto shared = getCurrentProjectSharedPtr();
  if (!shared) {
    return false;
  }
  return shared->moveItem(item, newParent);
}

QString ArtifactProjectService::projectItemRemovalConfirmationMessage(
    ProjectItem *item) const {
  if (!item) {
    return QStringLiteral("この項目を削除しますか？");
  }
  if (item->type() == eProjectItemType::Composition) {
    auto *compItem = static_cast<CompositionItem *>(item);
    return compositionRemovalConfirmationMessage(compItem->compositionId);
  }
  if (item->type() == eProjectItemType::Footage) {
    return QStringLiteral(
        "フッテージ項目を削除しますか？\n（元ファイル自体は削除されません）");
  }
  if (item->type() == eProjectItemType::Folder) {
    return QStringLiteral(
        "フォルダ項目を削除しますか？\n（子項目も同時に削除されます）");
  }
  return QStringLiteral("この項目を削除しますか？");
}

bool ArtifactProjectService::removeComposition(const CompositionID &id) {
  auto &pm = impl_->projectManager();
  auto projectShared = pm.getCurrentProjectSharedPtr();
  if (!projectShared)
    return false;
  bool ok = projectShared->removeCompositionById(id);
    if (ok) {
    compositionRemoved(id);
    ArtifactCore::globalEventBus().publish<CompositionRemovedEvent>(
        CompositionRemovedEvent{id.toString()});
    ArtifactCore::globalEventBus().publish<ProjectChangedEvent>({QString(), QString()});
  }
  return ok;
}

int ArtifactProjectService::renderQueueCountForComposition(
    const CompositionID &id) const {
  auto *queueService = ArtifactRenderQueueService::instance();
  return queueService ? queueService->renderQueueCountForComposition(id) : 0;
}

QString ArtifactProjectService::compositionRemovalConfirmationMessage(
    const CompositionID &id) const {
  const int queuedCount = renderQueueCountForComposition(id);
  if (queuedCount <= 0) {
    return QStringLiteral("このコンポジションを削除しますか？");
  }
  return QStringLiteral(
             "このコンポジションはレンダーキューに %1 件登録されています。\n"
             "削除すると該当キューも削除されます。\n"
             "続行しますか？")
      .arg(queuedCount);
}

bool ArtifactProjectService::removeCompositionWithRenderQueueCleanup(
    const CompositionID &id, int *removedQueueCount) {
  int queuedCount = renderQueueCountForComposition(id);
  if (removedQueueCount) {
    *removedQueueCount = queuedCount;
  }
  if (queuedCount > 0) {
    if (auto *queueService = ArtifactRenderQueueService::instance()) {
      queueService->removeRenderQueuesForComposition(id);
    }
  }
  return removeComposition(id);
}

bool ArtifactProjectService::duplicateComposition(const CompositionID &id) {
  auto result = impl_->projectManager().duplicateComposition(id);
  if (!result.success) {
    return false;
  }
  changeCurrentComposition(result.id);
  return true;
}

bool ArtifactProjectService::renameComposition(const CompositionID &id,
                                               const UniString &name) {
  auto &pm = impl_->projectManager();
  auto projectShared = pm.getCurrentProjectSharedPtr();
  if (!projectShared)
    return false;
  if (auto comp = pm.findComposition(id).ptr.lock()) {
    comp->setCompositionName(name);
  }
  auto items = projectShared->projectItems();
  for (auto root : items) {
    if (!root)
      continue;
    for (auto c : root->children) {
      if (!c)
        continue;
      if (c->type() == eProjectItemType::Composition) {
        CompositionItem *ci = static_cast<CompositionItem *>(c);
        if (ci->compositionId == id) {
          ci->name = name;
          ArtifactCore::globalEventBus().publish<ProjectChangedEvent>({QString(), QString()});
          return true;
        }
      }
    }
  }
  return false;
}

UniString ArtifactProjectService::projectName() const {

  return impl_->projectName();
}

void ArtifactProjectService::changeProjectName(const UniString &string) {
  impl_->changeProjectName(string);
}

void ArtifactProjectService::addAssetFromPath(const UniString &path) {
  impl_->addAssetFromPath(path);
}

QStringList
ArtifactProjectService::importAssetsFromPaths(const QStringList &sourcePaths) {
  return impl_->importAssetsFromPaths(sourcePaths);
}

void ArtifactProjectService::importAssetsFromPathsAsync(
    const QStringList &sourcePaths,
    std::function<void(QStringList)> onFinished) {
  impl_->importAssetsFromPathsAsync(sourcePaths, std::move(onFinished));
}

ArtifactCompositionWeakPtr ArtifactProjectService::currentComposition() {

  return impl_->currentComposition();
}

std::shared_ptr<ArtifactProject>
ArtifactProjectService::getCurrentProjectSharedPtr() const {
  return impl_->projectManager().getCurrentProjectSharedPtr();
}

ProjectHealthReport ArtifactProjectService::currentProjectHealthReport() const {
  auto project = getCurrentProjectSharedPtr();
  if (!project) {
    return {};
  }
  return ArtifactProjectHealthChecker::check(project.get());
}

std::vector<ArtifactCore::ProjectDiagnostic>
ArtifactProjectService::currentProjectDiagnostics() const {
  std::vector<ArtifactCore::ProjectDiagnostic> diagnostics =
      Artifact::convertProjectHealthReportToDiagnostics(currentProjectHealthReport());

  const auto project = getCurrentProjectSharedPtr();
  if (project) {
    std::vector<ArtifactCore::ProjectDiagnostic> appDiagnostics;
    appendAppValidationDiagnostics(project, appDiagnostics);
    for (const auto& diagnostic : appDiagnostics) {
      appendUniqueDiagnostic(diagnostics, diagnostic);
    }

    // Audio scrub runtime diagnostics
    const auto scrubDiagnostics =
        ArtifactAudioScrubController::instance().gatherDiagnostics();
    for (const auto& diag : scrubDiagnostics) {
      appendUniqueDiagnostic(diagnostics, diag);
    }
  }

  return diagnostics;
}

QString ArtifactProjectService::currentProjectHealthSummaryText() const
{
  const auto project = getCurrentProjectSharedPtr();
  if (!project) {
    return QStringLiteral("Status: Open a project to inspect details");
  }
  return projectHealthSummaryFromDiagnostics(currentProjectDiagnostics());
}

QString ArtifactProjectService::currentProjectHealthStateToken() const
{
  const auto project = getCurrentProjectSharedPtr();
  if (!project) {
    return QStringLiteral("none");
  }

  const auto diagnostics = currentProjectDiagnostics();
  const bool hasErrors = std::any_of(
      diagnostics.begin(), diagnostics.end(),
      [](const auto& diagnostic) { return diagnostic.isError(); });
  if (hasErrors) {
    return QStringLiteral("error");
  }

  const bool hasWarnings = std::any_of(
      diagnostics.begin(), diagnostics.end(),
      [](const auto& diagnostic) { return diagnostic.isWarning(); });
  if (hasWarnings) {
    return QStringLiteral("warning");
  }

  return QStringLiteral("healthy");
}

ChangeCompositionResult
ArtifactProjectService::changeCurrentComposition(const CompositionID &id) {
  const auto previousCompositionId =
      impl_ ? impl_->currentCompositionId_ : CompositionID{};
  auto result = impl_->changeCurrentComposition(id);
  if (result.success && previousCompositionId != id) {
    QTimer::singleShot(0, this, [this, id]() {
      ArtifactCore::globalEventBus().publish<CurrentCompositionChangedEvent>(
          CurrentCompositionChangedEvent{id.toString()});
    });
  }
  return result;
}

FindCompositionResult
ArtifactProjectService::findComposition(const CompositionID &id) {
  return impl_->findComposition(id);
}

QVector<ProjectItem *>
ArtifactProjectService::projectItems() const { // truncated for brevity
  return impl_->projectManager().getCurrentProjectSharedPtr()
             ? impl_->projectManager()
                   .getCurrentProjectSharedPtr()
                   ->projectItems()
             : QVector<ProjectItem *>();
}

void ArtifactProjectService::createComposition(const UniString &name) {
  auto &manager = impl_->projectManager();
  if (!hasProject()) {
    manager.createProject();
  }
  ArtifactCompositionInitParams params;
  params.setCompositionName(name);
  auto result = manager.createComposition(params);
  if (result.success) {
    if (auto project = manager.getCurrentProjectSharedPtr()) {
      updateLastUsedCreationDefaults(project, params);
    }
    changeCurrentComposition(result.id);
    qDebug() << "[ArtifactProjectService::createComposition(UniString)] "
                "succeeded, id:"
             << result.id.toString();
  } else {
    qDebug() << "[ArtifactProjectService::createComposition(UniString)] failed";
  }
}

void ArtifactProjectService::createComposition(
    const ArtifactCompositionInitParams &params) {
  auto &manager = impl_->projectManager();
  if (!hasProject()) {
    manager.createProject();
  }
  auto result = manager.createComposition(params);
  if (result.success) {
    if (auto project = manager.getCurrentProjectSharedPtr()) {
      updateLastUsedCreationDefaults(project, params);
    }
    changeCurrentComposition(result.id);
    qDebug() << "[ArtifactProjectService::createComposition] succeeded, id:"
             << result.id.toString();
  } else {
    qDebug() << "[ArtifactProjectService::createComposition] failed";
  }
}

void ArtifactProjectService::createProject(
    const ArtifactProjectSettings &setting) {
  auto &manager = impl_->projectManager();
  manager.createProject(setting.projectName());
    if (auto project = manager.getCurrentProjectSharedPtr()) {
      if (auto *rq = ArtifactRenderQueueService::instance()) {
      rq->sessionLedger().recordProjectOpened(
          project->settings().projectName(), setting.projectName());
      }
    }
  }

void ArtifactProjectService::removeAllAssets() {
  // removeall assets via projectmanager instance

  impl_->projectManager().removeAllAssets();
}

bool ArtifactProjectService::relinkFootage(ProjectItem *footageItem,
                                           const QString &newFilePath) {
  if (!footageItem || footageItem->type() != eProjectItemType::Footage) {
    return false;
  }
  auto *footage = static_cast<FootageItem *>(footageItem);
  if (newFilePath.isEmpty()) {
    return false;
  }
  const QFileInfo newFileInfo(newFilePath);
  if (!newFileInfo.exists()) {
    return false;
  }
  // Update the file path
  footage->filePath = newFileInfo.absoluteFilePath();
  // Notify project changed
  auto shared = getCurrentProjectSharedPtr();
  if (shared) {
    ArtifactCore::globalEventBus().publish<ProjectChangedEvent>({QString(), QString()});
  }
  return true;
}

int ArtifactProjectService::relinkFootageItems(
    const QVector<FootageItem *> &footageItems, const QString &newFilePath) {
  if (footageItems.isEmpty() || newFilePath.isEmpty()) {
    return 0;
  }
  int relinkedCount = 0;
  for (auto *footage : footageItems) {
    if (relinkFootage(footage, newFilePath)) {
      ++relinkedCount;
    }
  }
  return relinkedCount;
}

FootageItem *
ArtifactProjectService::findFootageItemByPath(const QString &filePath) const {
  auto shared = getCurrentProjectSharedPtr();
  if (!shared) {
    return nullptr;
  }

  QString normalizedPath =
      QDir::cleanPath(QFileInfo(filePath).absoluteFilePath());

  std::function<FootageItem *(ProjectItem *)> findRecursive =
      [&](ProjectItem *item) -> FootageItem * {
    if (!item)
      return nullptr;
    if (item->type() == eProjectItemType::Footage) {
      auto *footage = static_cast<FootageItem *>(item);
      QString itemPath =
          QDir::cleanPath(QFileInfo(footage->filePath).absoluteFilePath());
      if (itemPath == normalizedPath) {
        return footage;
      }
    }
    for (auto *child : item->children) {
      if (auto *found = findRecursive(child)) {
        return found;
      }
    }
    return nullptr;
  };

  const auto roots = shared->projectItems();
  for (auto *root : roots) {
    if (auto *found = findRecursive(root)) {
      return found;
    }
  }
  return nullptr;
}

bool ArtifactProjectService::relinkFootageByPath(const QString &oldFilePath,
                                                 const QString &newFilePath) {
  if (oldFilePath.isEmpty() || newFilePath.isEmpty()) {
    return false;
  }
  auto *footage = findFootageItemByPath(oldFilePath);
  if (!footage) {
    return false;
  }
  return relinkFootage(footage, newFilePath);
}

void ArtifactProjectService::setPreviewQualityPreset(
    PreviewQualityPreset preset) {
  impl_->setPreviewQualityPreset(preset);
  ArtifactCore::globalEventBus().publish<PreviewQualityPresetChangedEvent>(
      PreviewQualityPresetChangedEvent{static_cast<int>(preset)});
  previewQualityPresetChanged(preset);
}

PreviewQualityPreset ArtifactProjectService::previewQualityPreset() const {
  return impl_->previewQualityPreset();
}

}; // namespace Artifact

// W_REGISTER_ARGTYPE(ArtifactCore::CompositionID)
