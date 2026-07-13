module;
#include <utility>
#include <QWidget>
#include <QMenu>
#include <QAction>
#include <QKeySequence>
#include <QActionGroup>
#include <QPointer>
#include <QApplication>
#include <QGuiApplication>
#include <QDir>
#include <QList>
#include <QDockWidget>
#include <QInputDialog>
#include <QLineEdit>
#include <QImage>
#include <QQuaternion>
#include <QStandardPaths>
#include <QVariantList>
#include <QVariantMap>


#include <wobjectimpl.h>

#include <QMessageBox>

module Artifact.Menu.View;
import std;

import Artifact.Service.Project;
import Artifact.Service.Playback;
import Artifact.Application.Manager;
import Artifact.MainWindow;
import Artifact.Widgets.CompositionEditor;
import Application.AppSettings;
import Core.FastSettingsStore;
import Artifact.Workspace.Manager;
import Artifact.Workspace.Modes;
import Artifact.Widgets.ColorPaletteWidget;
import Artifact.Widgets.ColorSciencePanel;
import Artifact.Widgets.SecondaryPreviewWindow;
import Widgets.AssetBrowser;
import Widgets.ToolBar;
import Artifact.Widgets.ReactiveEventEditorWindow;
import Utils.Path;
import Event.Bus;
import Artifact.Event.Types;
import Image.ImageF32x4_RGBA;
import UI.ShortcutBindings;

namespace Artifact {
 using namespace ArtifactCore;
 namespace {
  ArtifactMainWindow* asArtifactMainWindow(QWidget* window)
  {
   return dynamic_cast<ArtifactMainWindow*>(window);
  }

 QWidget* findWidgetByClassHint(const QString& classHint)
  {
   const auto widgets = QApplication::allWidgets();
   for (QWidget* w : widgets) {
    if (!w) continue;
    const QString className = QString::fromLatin1(w->metaObject()->className());
    if (className.contains(classHint, Qt::CaseInsensitive)) {
     return w;
    }
  }
  return nullptr;
  }

  QDockWidget* findDockByTitle(QWidget* window, const QString& title)
  {
   if (!window) return nullptr;
   const auto docks = window->findChildren<QDockWidget*>();
   for (QDockWidget* dock : docks) {
    if (dock && dock->windowTitle() == title) {
     return dock;
    }
   }
   return nullptr;
  }

  QStringList dockTitles(QWidget* window)
  {
   if (auto* artifactWindow = asArtifactMainWindow(window)) {
    return artifactWindow->dockTitles();
   }

   QStringList titles;
   if (!window) return titles;
   const auto docks = window->findChildren<QDockWidget*>();
   for (QDockWidget* dock : docks) {
    if (!dock) continue;
    const QString title = dock->windowTitle().trimmed();
    if (!title.isEmpty() && !titles.contains(title)) {
     titles.push_back(title);
    }
   }
   return titles;
  }

  bool isDockVisible(QWidget* window, const QString& title)
  {
   if (auto* artifactWindow = asArtifactMainWindow(window)) {
    return artifactWindow->isDockVisible(title);
   }

   const auto* dock = findDockByTitle(window, title);
   return dock ? dock->isVisible() : false;
  }

  void setDockVisible(QWidget* window, const QString& title, bool visible)
  {
   if (auto* artifactWindow = asArtifactMainWindow(window)) {
    artifactWindow->setDockVisible(title, visible);
    return;
   }

   auto* dock = findDockByTitle(window, title);
   if (!dock) return;
   dock->setVisible(visible);
   if (visible) {
    dock->raise();
   }
  }

  void activateDock(QWidget* window, const QString& title)
  {
   if (auto* artifactWindow = asArtifactMainWindow(window)) {
    artifactWindow->activateDock(title);
    return;
   }

   auto* dock = findDockByTitle(window, title);
   if (!dock) return;
   dock->setVisible(true);
   dock->raise();
   dock->activateWindow();
  }

  void addFloatingDock(QWidget* window, const QString& title,
                       const QString& dockId, QWidget* widget,
                       const QRect& floatingGeometry)
  {
   if (!window || !widget) return;
   auto* dock = new QDockWidget(title, window);
   dock->setObjectName(dockId);
   dock->setWidget(widget);
   dock->setFloating(true);
   dock->setGeometry(floatingGeometry);
   dock->show();
  }

  struct ViewportBookmarkEntry {
   QString name;
   double zoom = 1.0;
   double panX = 0.0;
   double panY = 0.0;
   QQuaternion orientation;
  };

  struct ViewportTemplateEntry {
   QString name;
   double zoom = 1.0;
   double panX = 0.0;
   double panY = 0.0;
   bool showGrid = true;
   bool showGuides = true;
   bool showSafeMargins = true;
   bool showReferenceOverlay = true;
   int previewQualityPreset = static_cast<int>(::PreviewQualityPreset::Preview);
  };

  struct SelectionSetEntry {
   QString name;
   QStringList layerIds;
   int frame = 0;
  };

  class ViewportBookmarkStore {
  public:
   QString storePath() const
   {
    const QString appData =
        QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    return QDir(appData).filePath(QStringLiteral("ViewportBookmarks/viewport_bookmarks.cbor"));
   }

   static QString entriesKey(const QString& compositionId)
   {
    return QStringLiteral("ViewportBookmarks/%1/Entries").arg(compositionId.trimmed());
   }

   static QVariantMap entryToVariantMap(const ViewportBookmarkEntry& entry)
   {
    QVariantMap map;
    map.insert(QStringLiteral("name"), entry.name);
    map.insert(QStringLiteral("zoom"), entry.zoom);
    map.insert(QStringLiteral("panX"), entry.panX);
    map.insert(QStringLiteral("panY"), entry.panY);
    map.insert(QStringLiteral("orientationW"), entry.orientation.scalar());
    map.insert(QStringLiteral("orientationX"), entry.orientation.x());
    map.insert(QStringLiteral("orientationY"), entry.orientation.y());
    map.insert(QStringLiteral("orientationZ"), entry.orientation.z());
    return map;
   }

   static std::optional<ViewportBookmarkEntry> entryFromVariantMap(
       const QVariantMap& map)
   {
    const QString name = map.value(QStringLiteral("name")).toString().trimmed();
    if (name.isEmpty()) {
     return std::nullopt;
    }

    ViewportBookmarkEntry entry;
    entry.name = name;
    entry.zoom = map.value(QStringLiteral("zoom"), 1.0).toDouble();
    entry.panX = map.value(QStringLiteral("panX"), 0.0).toDouble();
    entry.panY = map.value(QStringLiteral("panY"), 0.0).toDouble();
    entry.orientation = QQuaternion(
        static_cast<float>(map.value(QStringLiteral("orientationW"), 1.0).toDouble()),
        static_cast<float>(map.value(QStringLiteral("orientationX"), 0.0).toDouble()),
        static_cast<float>(map.value(QStringLiteral("orientationY"), 0.0).toDouble()),
        static_cast<float>(map.value(QStringLiteral("orientationZ"), 0.0).toDouble()));
    return entry;
   }

   QStringList bookmarkNames(const QString& compositionId) const
   {
    if (compositionId.trimmed().isEmpty()) {
     return {};
    }

    ArtifactCore::FastSettingsStore store(storePath());
    const QVariantList list = store.value(entriesKey(compositionId), QVariantList()).toList();
    QStringList names;
    names.reserve(list.size());
   for (const QVariant& item : list) {
     const auto entry = entryFromVariantMap(item.toMap());
     if (entry) {
     names.push_back(entry->name);
     }
    }
    names.removeDuplicates();
    std::sort(names.begin(), names.end(),
              [](const QString& lhs, const QString& rhs) {
               const int cmp = lhs.compare(rhs, Qt::CaseInsensitive);
               if (cmp != 0) {
                return cmp < 0;
               }
               return lhs < rhs;
              });
    return names;
   }

   std::optional<ViewportBookmarkEntry> bookmark(const QString& compositionId,
                                                 const QString& bookmarkName) const
   {
    if (compositionId.trimmed().isEmpty()) {
     return std::nullopt;
    }
    const QString wanted = bookmarkName.trimmed();
    if (wanted.isEmpty()) {
     return std::nullopt;
    }

    ArtifactCore::FastSettingsStore store(storePath());
    const QVariantList list = store.value(entriesKey(compositionId), QVariantList()).toList();
    for (const QVariant& item : list) {
     const auto entry = entryFromVariantMap(item.toMap());
     if (entry && entry->name.compare(wanted, Qt::CaseInsensitive) == 0) {
      return entry;
     }
    }
    return std::nullopt;
   }

   bool saveBookmark(const QString& compositionId, const ViewportBookmarkEntry& entry) const
   {
    const QString compId = compositionId.trimmed();
    if (compId.isEmpty() || entry.name.trimmed().isEmpty()) {
     return false;
    }

    ArtifactCore::FastSettingsStore store(storePath());
    const QString key = entriesKey(compId);
    QVariantList list = store.value(key, QVariantList()).toList();

    bool replaced = false;
    for (QVariant& item : list) {
     QVariantMap map = item.toMap();
     const QString existingName = map.value(QStringLiteral("name")).toString().trimmed();
     if (existingName.compare(entry.name, Qt::CaseInsensitive) == 0) {
      map = entryToVariantMap(entry);
      item = map;
      replaced = true;
      break;
     }
    }

    if (!replaced) {
     list.push_back(entryToVariantMap(entry));
    }

    store.setValue(key, list);
    return store.sync();
   }

   bool deleteBookmark(const QString& compositionId, const QString& bookmarkName) const
   {
    const QString compId = compositionId.trimmed();
    const QString wanted = bookmarkName.trimmed();
    if (compId.isEmpty() || wanted.isEmpty()) {
     return false;
    }

    ArtifactCore::FastSettingsStore store(storePath());
    const QString key = entriesKey(compId);
    QVariantList list = store.value(key, QVariantList()).toList();
    bool removed = false;
    for (int i = list.size() - 1; i >= 0; --i) {
     const QVariantMap map = list[i].toMap();
     const QString existingName = map.value(QStringLiteral("name")).toString().trimmed();
     if (existingName.compare(wanted, Qt::CaseInsensitive) == 0) {
      list.removeAt(i);
      removed = true;
     }
    }
    if (!removed) {
     return false;
    }

    store.setValue(key, list);
    return store.sync();
   }
  };

  class ViewportTemplateStore {
  public:
   QString storePath() const
   {
    const QString appData =
        QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    return QDir(appData).filePath(QStringLiteral("ViewportTemplates/view_templates.cbor"));
   }

   static QString entriesKey(const QString& compositionId)
   {
    return QStringLiteral("ViewportTemplates/%1/Entries").arg(compositionId.trimmed());
   }

   static QVariantMap entryToVariantMap(const ViewportTemplateEntry& entry)
   {
    QVariantMap map;
    map.insert(QStringLiteral("name"), entry.name);
    map.insert(QStringLiteral("zoom"), entry.zoom);
    map.insert(QStringLiteral("panX"), entry.panX);
    map.insert(QStringLiteral("panY"), entry.panY);
    map.insert(QStringLiteral("showGrid"), entry.showGrid);
    map.insert(QStringLiteral("showGuides"), entry.showGuides);
    map.insert(QStringLiteral("showSafeMargins"), entry.showSafeMargins);
    map.insert(QStringLiteral("showReferenceOverlay"), entry.showReferenceOverlay);
    map.insert(QStringLiteral("previewQualityPreset"), entry.previewQualityPreset);
    return map;
   }

   static std::optional<ViewportTemplateEntry> entryFromVariantMap(
       const QVariantMap& map)
   {
    const QString name = map.value(QStringLiteral("name")).toString().trimmed();
    if (name.isEmpty()) {
     return std::nullopt;
    }

    ViewportTemplateEntry entry;
    entry.name = name;
    entry.zoom = map.value(QStringLiteral("zoom"), 1.0).toDouble();
    entry.panX = map.value(QStringLiteral("panX"), 0.0).toDouble();
    entry.panY = map.value(QStringLiteral("panY"), 0.0).toDouble();
    entry.showGrid = map.value(QStringLiteral("showGrid"), true).toBool();
    entry.showGuides = map.value(QStringLiteral("showGuides"), true).toBool();
    entry.showSafeMargins = map.value(QStringLiteral("showSafeMargins"), true).toBool();
    entry.showReferenceOverlay = map.value(QStringLiteral("showReferenceOverlay"), true).toBool();
    entry.previewQualityPreset =
        map.value(QStringLiteral("previewQualityPreset"), 0).toInt();
    return entry;
   }

   QStringList templateNames(const QString& compositionId) const
   {
    if (compositionId.trimmed().isEmpty()) {
     return {};
    }

    ArtifactCore::FastSettingsStore store(storePath());
    const QVariantList list = store.value(entriesKey(compositionId), QVariantList()).toList();
    QStringList names;
    names.reserve(list.size());
    for (const QVariant& item : list) {
     const auto entry = entryFromVariantMap(item.toMap());
     if (entry) {
      names.push_back(entry->name);
     }
    }
    names.removeDuplicates();
    std::sort(names.begin(), names.end(),
              [](const QString& lhs, const QString& rhs) {
               const int cmp = lhs.compare(rhs, Qt::CaseInsensitive);
               if (cmp != 0) {
                return cmp < 0;
               }
               return lhs < rhs;
              });
    return names;
   }

   std::optional<ViewportTemplateEntry> templateEntry(const QString& compositionId,
                                                      const QString& templateName) const
   {
    if (compositionId.trimmed().isEmpty()) {
     return std::nullopt;
    }
    const QString wanted = templateName.trimmed();
    if (wanted.isEmpty()) {
     return std::nullopt;
    }

    ArtifactCore::FastSettingsStore store(storePath());
    const QVariantList list = store.value(entriesKey(compositionId), QVariantList()).toList();
    for (const QVariant& item : list) {
     const auto entry = entryFromVariantMap(item.toMap());
     if (entry && entry->name.compare(wanted, Qt::CaseInsensitive) == 0) {
      return entry;
     }
    }
    return std::nullopt;
   }

   bool saveTemplate(const QString& compositionId, const ViewportTemplateEntry& entry) const
   {
    const QString compId = compositionId.trimmed();
    if (compId.isEmpty() || entry.name.trimmed().isEmpty()) {
     return false;
    }

    ArtifactCore::FastSettingsStore store(storePath());
    const QString key = entriesKey(compId);
    QVariantList list = store.value(key, QVariantList()).toList();

    bool replaced = false;
    for (QVariant& item : list) {
     QVariantMap map = item.toMap();
     const QString existingName = map.value(QStringLiteral("name")).toString().trimmed();
     if (existingName.compare(entry.name, Qt::CaseInsensitive) == 0) {
      map = entryToVariantMap(entry);
      item = map;
      replaced = true;
      break;
     }
    }

    if (!replaced) {
     list.push_back(entryToVariantMap(entry));
    }

    store.setValue(key, list);
    return store.sync();
   }

   bool deleteTemplate(const QString& compositionId, const QString& templateName) const
   {
    const QString compId = compositionId.trimmed();
    const QString wanted = templateName.trimmed();
    if (compId.isEmpty() || wanted.isEmpty()) {
     return false;
    }

    ArtifactCore::FastSettingsStore store(storePath());
    const QString key = entriesKey(compId);
    QVariantList list = store.value(key, QVariantList()).toList();
    bool removed = false;
    for (int i = list.size() - 1; i >= 0; --i) {
     const QVariantMap map = list[i].toMap();
     const QString existingName = map.value(QStringLiteral("name")).toString().trimmed();
     if (existingName.compare(wanted, Qt::CaseInsensitive) == 0) {
      list.removeAt(i);
      removed = true;
     }
    }
    if (!removed) {
     return false;
    }

    store.setValue(key, list);
    return store.sync();
   }
  };

  class SelectionSetStore {
  public:
   QString storePath() const
   {
    const QString appData =
        QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    return QDir(appData).filePath(QStringLiteral("SelectionSets/selection_sets.cbor"));
   }

   static QString entriesKey(const QString& compositionId)
   {
    return QStringLiteral("SelectionSets/%1/Entries").arg(compositionId.trimmed());
   }

   static QVariantMap entryToVariantMap(const SelectionSetEntry& entry)
   {
    QVariantMap map;
    map.insert(QStringLiteral("name"), entry.name);
    map.insert(QStringLiteral("layerIds"), entry.layerIds);
    map.insert(QStringLiteral("frame"), entry.frame);
    return map;
   }

   static std::optional<SelectionSetEntry> entryFromVariantMap(const QVariantMap& map)
   {
    const QString name = map.value(QStringLiteral("name")).toString().trimmed();
    if (name.isEmpty()) {
     return std::nullopt;
    }
    SelectionSetEntry entry;
    entry.name = name;
    entry.layerIds = map.value(QStringLiteral("layerIds")).toStringList();
    entry.frame = map.value(QStringLiteral("frame"), 0).toInt();
    return entry;
   }

   QStringList selectionSetNames(const QString& compositionId) const
   {
    if (compositionId.trimmed().isEmpty()) {
     return {};
    }
    ArtifactCore::FastSettingsStore store(storePath());
    const QVariantList list = store.value(entriesKey(compositionId), QVariantList()).toList();
    QStringList names;
    for (const QVariant& item : list) {
     const auto entry = entryFromVariantMap(item.toMap());
     if (entry) {
      names.push_back(entry->name);
     }
    }
    names.removeDuplicates();
    return names;
   }

   std::optional<SelectionSetEntry> selectionSet(const QString& compositionId,
                                                 const QString& selectionSetName) const
   {
    if (compositionId.trimmed().isEmpty()) {
     return std::nullopt;
    }
    const QString wanted = selectionSetName.trimmed();
    if (wanted.isEmpty()) {
     return std::nullopt;
    }
    ArtifactCore::FastSettingsStore store(storePath());
    const QVariantList list = store.value(entriesKey(compositionId), QVariantList()).toList();
    for (const QVariant& item : list) {
     const auto entry = entryFromVariantMap(item.toMap());
     if (entry && entry->name.compare(wanted, Qt::CaseInsensitive) == 0) {
      return entry;
     }
    }
    return std::nullopt;
   }

   bool saveSelectionSet(const QString& compositionId, const SelectionSetEntry& entry) const
   {
    const QString compId = compositionId.trimmed();
    if (compId.isEmpty() || entry.name.trimmed().isEmpty()) {
     return false;
    }
    ArtifactCore::FastSettingsStore store(storePath());
    const QString key = entriesKey(compId);
    QVariantList list = store.value(key, QVariantList()).toList();
    bool replaced = false;
    for (QVariant& item : list) {
     QVariantMap map = item.toMap();
     const QString existingName = map.value(QStringLiteral("name")).toString().trimmed();
     if (existingName.compare(entry.name, Qt::CaseInsensitive) == 0) {
      item = entryToVariantMap(entry);
      replaced = true;
      break;
     }
    }
    if (!replaced) {
     list.push_back(entryToVariantMap(entry));
    }
    store.setValue(key, list);
    return store.sync();
   }
  };

  std::optional<SelectionSetEntry> currentSelectionSetState()
  {
   auto* app = ArtifactApplicationManager::instance();
   auto* selection = app ? app->layerSelectionManager() : nullptr;
   auto* playback = ArtifactPlaybackService::instance();
   if (!selection) {
    return std::nullopt;
   }
   const auto layers = selection->selectedLayers();
   if (layers.isEmpty()) {
    return std::nullopt;
   }
   SelectionSetEntry entry;
   for (const auto& layer : layers) {
    if (layer) {
     entry.layerIds.push_back(layer->id().toString());
    }
   }
   entry.frame = playback ? playback->currentFrame().framePosition() : 0;
   return entry;
  }

  std::optional<ViewportBookmarkEntry> currentViewportBookmarkState(
      ArtifactCompositionEditor* editor)
  {
   if (!editor) {
    return std::nullopt;
   }

   auto* controller = editor->renderController();
   if (!controller) {
    return std::nullopt;
   }

   auto* renderer = controller->renderer();
   if (!renderer) {
    return std::nullopt;
   }

   float panX = 0.0f;
   float panY = 0.0f;
   renderer->getPan(panX, panY);

   ViewportBookmarkEntry entry;
   entry.zoom = std::max(0.001, static_cast<double>(renderer->getZoom()));
   entry.panX = static_cast<double>(panX);
   entry.panY = static_cast<double>(panY);
   entry.orientation = controller->viewportOrientationQuaternion();
   return entry;
  }

  std::optional<ViewportTemplateEntry> currentViewportTemplateState(
      ArtifactCompositionEditor* editor)
  {
   if (!editor) {
    return std::nullopt;
   }

   auto* controller = editor->renderController();
   if (!controller) {
    return std::nullopt;
   }

   auto* renderer = controller->renderer();
   if (!renderer) {
    return std::nullopt;
   }

   float panX = 0.0f;
   float panY = 0.0f;
   renderer->getPan(panX, panY);

   ViewportTemplateEntry entry;
   entry.zoom = std::max(0.001, static_cast<double>(renderer->getZoom()));
   entry.panX = static_cast<double>(panX);
   entry.panY = static_cast<double>(panY);
   entry.showGrid = controller->isShowGrid();
   entry.showGuides = controller->isShowGuides();
   entry.showSafeMargins = controller->isShowSafeMargins();
   entry.showReferenceOverlay = controller->isShowReferenceOverlay();
   if (auto* svc = ArtifactProjectService::instance()) {
    entry.previewQualityPreset = static_cast<int>(svc->previewQualityPreset());
   }
   return entry;
  }

  ArtifactCompositionPtr currentViewportComposition(ArtifactCompositionEditor* editor)
  {
   if (editor) {
    if (auto* controller = editor->renderController()) {
     if (auto comp = controller->composition()) {
      return comp;
     }
    }
   }

   auto* service = ArtifactProjectService::instance();
   return service ? service->currentComposition().lock() : ArtifactCompositionPtr{};
  }

  bool applyViewportBookmarkState(ArtifactCompositionEditor* editor,
                                  const ViewportBookmarkEntry& entry)
  {
   if (!editor) {
    return false;
   }

   auto* controller = editor->renderController();
   if (!controller) {
    return false;
   }

   auto* renderer = controller->renderer();
   if (!renderer) {
    return false;
   }

   renderer->setZoom(static_cast<float>(std::max(0.001, entry.zoom)));
   renderer->setPan(static_cast<float>(entry.panX), static_cast<float>(entry.panY));
   controller->setViewportOrientationQuaternion(entry.orientation);
   controller->markRenderDirty();
   return true;
  }

  bool applyViewportTemplateState(ArtifactCompositionEditor* editor,
                                  const ViewportTemplateEntry& entry)
  {
   if (!editor) {
    return false;
   }

   auto* controller = editor->renderController();
   if (!controller) {
    return false;
   }

   auto* renderer = controller->renderer();
   if (!renderer) {
    return false;
   }

   renderer->setZoom(static_cast<float>(std::max(0.001, entry.zoom)));
   renderer->setPan(static_cast<float>(entry.panX), static_cast<float>(entry.panY));
   controller->setShowGrid(entry.showGrid);
   controller->setShowGuides(entry.showGuides);
   controller->setShowSafeMargins(entry.showSafeMargins);
   controller->setShowReferenceOverlay(entry.showReferenceOverlay);
   if (auto* svc = ArtifactProjectService::instance()) {
    svc->setPreviewQualityPreset(static_cast<::PreviewQualityPreset>(
        std::clamp(entry.previewQualityPreset, 0, 2)));
   }
   controller->markRenderDirty();
   return true;
  }

  ArtifactCompositionEditor* activeCompositionEditor(QWidget* root)
  {
   const auto pickBest = [](const auto& widgets) -> ArtifactCompositionEditor* {
    for (auto* editor : widgets) {
     if (editor && editor->hasFocus()) {
      return editor;
     }
    }
    for (auto* editor : widgets) {
     if (editor && editor->isVisible()) {
      return editor;
     }
    }
    return widgets.isEmpty() ? nullptr : widgets.front();
   };

   if (root) {
    const auto editors = root->findChildren<ArtifactCompositionEditor*>();
    if (auto* editor = pickBest(editors)) {
     return editor;
    }
   }

   const auto allWidgets = QApplication::allWidgets();
   QList<ArtifactCompositionEditor*> editors;
   editors.reserve(allWidgets.size());
   for (QWidget* widget : allWidgets) {
    if (auto* editor = dynamic_cast<ArtifactCompositionEditor*>(widget)) {
     editors.push_back(editor);
    }
   }
   return pickBest(editors);
  }
 }

  class ArtifactViewMenu::Impl {
  public:
  Impl(ArtifactViewMenu* menu);
  ~Impl();

   ArtifactViewMenu* menu_ = nullptr;
   QAction* zoomInAction = nullptr;
   QAction* zoomOutAction = nullptr;
   QAction* defaultZoomAction = nullptr;
   QAction* fitToScreenAction = nullptr;
   QMenu* viewportBookmarkMenu = nullptr;
   QAction* saveViewportBookmarkAction = nullptr;
   QAction* deleteViewportBookmarkAction = nullptr;
   QMenu* viewportTemplateMenu = nullptr;
   QAction* saveViewportTemplateAction = nullptr;
   QAction* deleteViewportTemplateAction = nullptr;
   QMenu* compareMenu = nullptr;
   QActionGroup* compareGroup = nullptr;
   QAction* compareSurfaceAction = nullptr;
   QAction* compareOffAction = nullptr;
   QAction* compareAAction = nullptr;
   QAction* compareBAction = nullptr;
   QAction* compareDiffAction = nullptr;
   QAction* compareReferenceAction = nullptr;
   QAction* xRayAction = nullptr;
   QAction* isolationAction = nullptr;
   QMenu* selectionSetMenu = nullptr;
   QAction* saveSelectionSetAction = nullptr;
   
   QMenu* resolutionMenu = nullptr;
   QActionGroup* resolutionGroup = nullptr;
   QAction* resFullAction = nullptr;
   QAction* resHalfAction = nullptr;
   QAction* resThirdAction = nullptr;
   QAction* resQuarterAction = nullptr;

   QAction* showGridAction = nullptr;
   QAction* snapToGridAction = nullptr;
   QAction* showGuidesAction = nullptr;
   QAction* snapToGuidesAction = nullptr;
   QAction* showRulersAction = nullptr;
   QAction* useDisplayColorManagementAction = nullptr;

   QMenu* qualityPresetMenu = nullptr;
   QActionGroup* qualityGroup = nullptr;
   QAction* qualityDraftAction = nullptr;
   QAction* qualityPreviewAction = nullptr;
   QAction* qualityFinalAction = nullptr;
   QMenu* workspaceMenu = nullptr;
   QMenu* workspacePresetMenu = nullptr;
   QActionGroup* workspaceGroup = nullptr;
   QAction* workspaceDefaultAction = nullptr;
   QAction* workspaceAnimationAction = nullptr;
   QAction* workspaceVfxAction = nullptr;
   QAction* workspaceCompositingAction = nullptr;
   QAction* workspaceAudioAction = nullptr;
   QList<QAction*> workspaceModeActions_;
   QAction* saveWorkspacePresetAction = nullptr;
   QAction* deleteWorkspacePresetAction = nullptr;
   QAction* restoreWorkspaceSessionAction = nullptr;
   QMenu* windowPanelsMenu = nullptr;
   QStringList cachedWorkspacePresetNames_;
   QStringList cachedDockTitles_;
   QWidget* mainWindow = nullptr;
   QPointer<ArtifactReactiveEventEditorWindow> reactiveEventEditorWindow;
     int newBrowserCount_ = 1;
     QAction* openContentsViewerAction = nullptr;
     QAction* openProjectPanelAction = nullptr;
     QAction* openColorPaletteAction = nullptr;
     QAction* openColorScienceAction = nullptr;
     QAction* openReactiveEventEditorAction = nullptr;
   QAction* secondaryPreviewAction = nullptr;
   QPointer<ArtifactSecondaryPreviewWindow> secondaryPreviewWindow;
   ArtifactCore::EventBus eventBus_ = ArtifactCore::globalEventBus();
   std::vector<ArtifactCore::EventBus::Subscription> eventBusSubscriptions_;

   void refreshEnabledState();
   void refreshWorkspaceState();
   void refreshWorkspacePresetMenu();
   void refreshViewportBookmarkMenu();
   void refreshViewportTemplateMenu();
   void refreshCompareMenu();
   void refreshSelectionSetMenu();
   void rebuildWindowPanelsMenu();
   void showProjectPanel();
   void showSecondaryPreview();
   void refreshSecondaryPreview();
   };

  ArtifactViewMenu::Impl::Impl(ArtifactViewMenu* menu)
  {
   menu_ = menu;
   auto& shortcuts = ShortcutBindings::instance();
   zoomInAction = new QAction("ズームイン(&I)");
   zoomInAction->setShortcut(shortcuts.shortcut(ShortcutId::ViewZoomIn));
   zoomInAction->setIcon(QIcon(resolveIconPath("Studio/viewmenu_zoom_in.svg")));
   
   zoomOutAction = new QAction("ズームアウト(&O)");
   zoomOutAction->setShortcut(shortcuts.shortcut(ShortcutId::ViewZoomOut));
   zoomOutAction->setIcon(QIcon(resolveIconPath("Studio/viewmenu_zoom_out.svg")));

   defaultZoomAction = new QAction("100% ズーム");
   defaultZoomAction->setShortcut(shortcuts.shortcut(ShortcutId::ViewDefaultZoom));
   defaultZoomAction->setIcon(QIcon(resolveIconPath("Studio/viewmenu_aspect_ratio.svg")));

   fitToScreenAction = new QAction("画面に合わせる(&F)");
   fitToScreenAction->setShortcut(shortcuts.shortcut(ShortcutId::ViewFitToScreen));
   fitToScreenAction->setIcon(QIcon(resolveIconPath("Studio/viewmenu_fit_screen.svg")));

   viewportBookmarkMenu = new QMenu("Camera ブックマーク(&B)");
   viewportBookmarkMenu->setObjectName(QStringLiteral("viewportBookmarkMenu"));
   viewportBookmarkMenu->setIcon(QIcon(resolveIconPath("Studio/viewmenu_bookmarks.svg")));
   viewportBookmarkMenu->setToolTip("Save and restore named camera / viewport states");
   viewportTemplateMenu = new QMenu("View Template(&T)");
   viewportTemplateMenu->setIcon(QIcon(resolveIconPath("Studio/viewmenu_presets.svg")));
   compareMenu = new QMenu("Compare(&C)");
   compareMenu->setIcon(QIcon(resolveIconPath("Studio/viewmenu_contents_viewer.svg")));
   compareGroup = new QActionGroup(menu);
   compareGroup->setExclusive(true);
   selectionSetMenu = new QMenu("Selection セット(&S)");
   selectionSetMenu->setIcon(QIcon(resolveIconPath("Studio/viewmenu_bookmarks.svg")));

   resolutionMenu = new QMenu("解像度(&R)");
   resolutionMenu->setIcon(QIcon(resolveIconPath("Studio/viewmenu_resolution_full.svg")));
   resolutionGroup = new QActionGroup(menu);
   resolutionGroup->setExclusive(true);
   resFullAction = resolutionMenu->addAction("フル画質");
   resFullAction->setIcon(QIcon(resolveIconPath("Studio/viewmenu_resolution_full.svg")));
   resHalfAction = resolutionMenu->addAction("1/2画質");
   resHalfAction->setIcon(QIcon(resolveIconPath("Studio/viewmenu_resolution_half.svg")));
   resThirdAction = resolutionMenu->addAction("1/3画質");
   resThirdAction->setIcon(QIcon(resolveIconPath("Studio/viewmenu_resolution_third.svg")));
   resQuarterAction = resolutionMenu->addAction("1/4画質");
   resQuarterAction->setIcon(QIcon(resolveIconPath("Studio/viewmenu_resolution_quarter.svg")));
   resFullAction->setCheckable(true);
   resFullAction->setChecked(true);
   resHalfAction->setCheckable(true);
   resThirdAction->setCheckable(true);
   resQuarterAction->setCheckable(true);
   resolutionGroup->addAction(resFullAction);
   resolutionGroup->addAction(resHalfAction);
   resolutionGroup->addAction(resThirdAction);
   resolutionGroup->addAction(resQuarterAction);

   showGridAction = new QAction("グリッドを表示(&G)");
   showGridAction->setShortcut(shortcuts.shortcut(ShortcutId::ViewShowGrid));
   showGridAction->setCheckable(true);
   showGridAction->setIcon(QIcon(resolveIconPath("Studio/viewmenu_grid_on.svg")));

   snapToGridAction = new QAction("グリッドにスナップ(&S)");
   snapToGridAction->setShortcut(shortcuts.shortcut(ShortcutId::ViewSnapToGrid));
   snapToGridAction->setCheckable(true);
   snapToGridAction->setIcon(QIcon(resolveIconPath("Studio/viewmenu_grid_on.svg")));

   showGuidesAction = new QAction("ガイドを表示");
   showGuidesAction->setShortcut(shortcuts.shortcut(ShortcutId::ViewShowGuides));
   showGuidesAction->setCheckable(true);
   showGuidesAction->setIcon(QIcon(resolveIconPath("Studio/viewmenu_straighten.svg")));

   snapToGuidesAction = new QAction("ガイドにスナップ");
   snapToGuidesAction->setShortcut(shortcuts.shortcut(ShortcutId::ViewSnapToGuides));
   snapToGuidesAction->setCheckable(true);
   snapToGuidesAction->setIcon(QIcon(resolveIconPath("Studio/viewmenu_linear_scale.svg")));

   showRulersAction = new QAction("定規を表示(&R)");
   showRulersAction->setShortcut(shortcuts.shortcut(ShortcutId::ViewShowRulers));
   showRulersAction->setCheckable(true);
   showRulersAction->setIcon(QIcon(resolveIconPath("Studio/viewmenu_straighten.svg")));

   useDisplayColorManagementAction = new QAction("ディスプレイのカラーマネジメントを使用");
   useDisplayColorManagementAction->setCheckable(true);
   useDisplayColorManagementAction->setIcon(QIcon(resolveIconPath("Studio/viewmenu_color_palette.svg")));

   qualityPresetMenu = new QMenu("品質プリセット(&Q)");
   qualityPresetMenu->setIcon(QIcon(resolveIconPath("Studio/viewmenu_quality_preview.svg")));
   qualityGroup = new QActionGroup(menu);
   qualityGroup->setExclusive(true);

   qualityDraftAction = qualityPresetMenu->addAction("Draft (編集優先)");
   qualityDraftAction->setIcon(QIcon(resolveIconPath("Studio/viewmenu_quality_draft.svg")));
   qualityPreviewAction = qualityPresetMenu->addAction("Preview (標準)");
   qualityPreviewAction->setIcon(QIcon(resolveIconPath("Studio/viewmenu_quality_preview.svg")));
   qualityFinalAction = qualityPresetMenu->addAction("Final (品質優先)");
   qualityFinalAction->setIcon(QIcon(resolveIconPath("Studio/viewmenu_quality_final.svg")));

   qualityDraftAction->setCheckable(true);
   qualityPreviewAction->setCheckable(true);
   qualityFinalAction->setCheckable(true);
   qualityPreviewAction->setChecked(true);

   qualityGroup->addAction(qualityDraftAction);
   qualityGroup->addAction(qualityPreviewAction);
   qualityGroup->addAction(qualityFinalAction);

   QObject::connect(resFullAction, &QAction::triggered, menu, []() {
    if (auto* settings = ArtifactCore::ArtifactAppSettings::instance()) {
     settings->setPreviewResolutionPercent(100);
    }
   });
   QObject::connect(resHalfAction, &QAction::triggered, menu, []() {
    if (auto* settings = ArtifactCore::ArtifactAppSettings::instance()) {
     settings->setPreviewResolutionPercent(50);
    }
   });
   QObject::connect(resThirdAction, &QAction::triggered, menu, []() {
    if (auto* settings = ArtifactCore::ArtifactAppSettings::instance()) {
     settings->setPreviewResolutionPercent(33);
    }
   });
   QObject::connect(resQuarterAction, &QAction::triggered, menu, []() {
    if (auto* settings = ArtifactCore::ArtifactAppSettings::instance()) {
     settings->setPreviewResolutionPercent(25);
    }
   });

   if (auto* settings = ArtifactCore::ArtifactAppSettings::instance()) {
    QObject::connect(settings, &ArtifactCore::ArtifactAppSettings::settingsChanged, menu,
                     [this]() {
                      refreshEnabledState();
                     });
   }

   auto* svc = ArtifactProjectService::instance();
   if (svc) {
    QObject::connect(qualityDraftAction, &QAction::triggered, menu, [svc]() {
     svc->setPreviewQualityPreset(::PreviewQualityPreset::Draft);
    });
    QObject::connect(qualityPreviewAction, &QAction::triggered, menu, [svc]() {
     svc->setPreviewQualityPreset(::PreviewQualityPreset::Preview);
    });
    QObject::connect(qualityFinalAction, &QAction::triggered, menu, [svc]() {
     svc->setPreviewQualityPreset(::PreviewQualityPreset::Final);
    });

    eventBusSubscriptions_.push_back(eventBus_.subscribe<PreviewQualityPresetChangedEvent>(
        [this](const PreviewQualityPresetChangedEvent& event) {
         switch (static_cast<::PreviewQualityPreset>(event.preset)) {
         case ::PreviewQualityPreset::Draft:
          qualityDraftAction->setChecked(true);
          break;
         case ::PreviewQualityPreset::Preview:
          qualityPreviewAction->setChecked(true);
          break;
         case ::PreviewQualityPreset::Final:
          qualityFinalAction->setChecked(true);
          break;
         }
        }));

    eventBusSubscriptions_.push_back(eventBus_.subscribe<ProjectChangedEvent>(
        [this](const ProjectChangedEvent&) {
         refreshEnabledState();
         refreshSecondaryPreview();
        }));
    eventBusSubscriptions_.push_back(eventBus_.subscribe<CompositionCreatedEvent>(
        [this](const CompositionCreatedEvent&) {
         refreshEnabledState();
         refreshSecondaryPreview();
        }));
    eventBusSubscriptions_.push_back(eventBus_.subscribe<CurrentCompositionChangedEvent>(
        [this](const CurrentCompositionChangedEvent&) {
         refreshSecondaryPreview();
        }));
    eventBusSubscriptions_.push_back(eventBus_.subscribe<FrameChangedEvent>(
        [this](const FrameChangedEvent&) {
         refreshSecondaryPreview();
        }));
    eventBusSubscriptions_.push_back(eventBus_.subscribe<PlaybackStateChangedEvent>(
        [this](const PlaybackStateChangedEvent&) {
         refreshSecondaryPreview();
        }));
   }

   workspaceMenu = new QMenu("ワークスペース(&K)");
   workspaceMenu->setIcon(QIcon(resolveIconPath("Studio/viewmenu_workspace.svg")));
   workspaceGroup = new QActionGroup(menu);
   workspaceGroup->setExclusive(true);

   for (const auto &info : Artifact::workspaceModeInfos()) {
    QAction *action = workspaceMenu->addAction(info.label);
    action->setIcon(QIcon(resolveIconPath(info.iconPath)));
    action->setCheckable(true);
    action->setData(static_cast<int>(info.mode));
    workspaceGroup->addAction(action);
    workspaceModeActions_.push_back(action);
    QObject::connect(action, &QAction::triggered, menu, [this, mode = info.mode]() {
     if (auto* window = qobject_cast<ArtifactMainWindow*>(mainWindow)) {
      window->setWorkspaceMode(mode);
     }
    });
    if (info.mode == WorkspaceMode::Default) {
     workspaceDefaultAction = action;
    } else if (info.mode == WorkspaceMode::Animation) {
     workspaceAnimationAction = action;
    } else if (info.mode == WorkspaceMode::VFX) {
     workspaceVfxAction = action;
    } else if (info.mode == WorkspaceMode::Compositing) {
     workspaceCompositingAction = action;
    } else if (info.mode == WorkspaceMode::Audio) {
     workspaceAudioAction = action;
    }
   }

   workspacePresetMenu = new QMenu("プリセット");
   workspacePresetMenu->setIcon(QIcon(resolveIconPath("Studio/viewmenu_presets.svg")));
   saveWorkspacePresetAction = workspacePresetMenu->addAction("現在のレイアウトを保存...");
   saveWorkspacePresetAction->setIcon(QIcon(resolveIconPath("Studio/viewmenu_save_layout.svg")));
   deleteWorkspacePresetAction = workspacePresetMenu->addAction("プリセットを削除...");
   deleteWorkspacePresetAction->setIcon(QIcon(resolveIconPath("Studio/viewmenu_delete.svg")));
   restoreWorkspaceSessionAction = workspacePresetMenu->addAction("最後のセッションを復元");
   restoreWorkspaceSessionAction->setIcon(QIcon(resolveIconPath("Studio/viewmenu_restore_session.svg")));
   QObject::connect(saveWorkspacePresetAction, &QAction::triggered, menu, [this]() {
    if (!mainWindow) return;
    const QString defaultName = QStringLiteral("Custom");
    bool ok = false;
    const QString presetName = QInputDialog::getText(
        mainWindow, QStringLiteral("ワークスペースを保存"),
        QStringLiteral("プリセット名を入力してください"), QLineEdit::Normal,
        defaultName, &ok)
                                    .trimmed();
    if (!ok || presetName.isEmpty()) {
     return;
    }
    ArtifactWorkspaceManager manager;
    if (!manager.savePreset(presetName, mainWindow)) {
     QMessageBox::warning(mainWindow, QStringLiteral("ワークスペースを保存"),
                          QStringLiteral("ワークスペースの保存に失敗しました。"));
    }
                   });
   QObject::connect(deleteWorkspacePresetAction, &QAction::triggered, menu, [this]() {
    if (!mainWindow) return;
    ArtifactWorkspaceManager manager;
    const QStringList presets = manager.presetNames();
    if (presets.isEmpty()) {
     QMessageBox::information(mainWindow, QStringLiteral("ワークスペース"),
                              QStringLiteral("削除できるプリセットがありません。"));
     return;
    }
    bool ok = false;
    const QString presetName = QInputDialog::getItem(
        mainWindow, QStringLiteral("プリセットを削除"),
        QStringLiteral("削除するプリセットを選択してください"),
        presets, 0, false, &ok);
    if (!ok || presetName.trimmed().isEmpty()) {
     return;
    }
    const QString confirmMessage = QStringLiteral("プリセット「%1」を削除しますか？").arg(presetName);
    if (QMessageBox::question(mainWindow, QStringLiteral("プリセットを削除"),
                              confirmMessage,
                              QMessageBox::Yes | QMessageBox::No,
                              QMessageBox::No) != QMessageBox::Yes) {
     return;
    }
    if (!manager.deletePreset(presetName)) {
     QMessageBox::warning(mainWindow, QStringLiteral("プリセットを削除"),
                          QStringLiteral("プリセットの削除に失敗しました。"));
    }
   });
   QObject::connect(restoreWorkspaceSessionAction, &QAction::triggered, menu, [this]() {
    if (!mainWindow) return;
    ArtifactWorkspaceManager manager;
    if (!manager.restoreSession(mainWindow)) {
     QMessageBox::information(mainWindow, QStringLiteral("ワークスペースを復元"),
                              QStringLiteral("復元できるセッションがありません。"));
    }
   });
   
   QObject::connect(menu, &QMenu::aboutToShow, menu, [this]() {
    refreshEnabledState();
    refreshWorkspaceState();
    refreshWorkspacePresetMenu();
    refreshViewportBookmarkMenu();
    refreshViewportTemplateMenu();
    refreshCompareMenu();
    refreshSelectionSetMenu();
   });

   menu->addAction(zoomInAction);
   menu->addAction(zoomOutAction);
   menu->addAction(defaultZoomAction);
   menu->addAction(fitToScreenAction);
   menu->addSeparator();
   menu->addMenu(viewportBookmarkMenu);
   menu->addMenu(viewportTemplateMenu);
   menu->addMenu(compareMenu);
   menu->addMenu(selectionSetMenu);
   menu->addSeparator();
   menu->addMenu(resolutionMenu);
   menu->addMenu(qualityPresetMenu);
   menu->addSeparator();
   menu->addAction(useDisplayColorManagementAction);
   menu->addSeparator();
   openContentsViewerAction = menu->addAction("Contents Viewer");
   openContentsViewerAction->setIcon(QIcon(resolveIconPath("Studio/viewmenu_contents_viewer.svg")));
   QObject::connect(openContentsViewerAction, &QAction::triggered, menu, [this]() {
    if (!mainWindow) return;
    setDockVisible(mainWindow, QStringLiteral("Contents Viewer"), true);
    activateDock(mainWindow, QStringLiteral("Contents Viewer"));
   });

   openProjectPanelAction = menu->addAction("Project パネル(&P)");
   openProjectPanelAction->setIcon(QIcon(resolveIconPath("Studio/viewmenu_panels.svg")));
   QObject::connect(openProjectPanelAction, &QAction::triggered, menu, [this]() {
    showProjectPanel();
   });

   openColorPaletteAction = menu->addAction("カラーパレット(&P)");
   openColorPaletteAction->setIcon(QIcon(resolveIconPath("Studio/viewmenu_color_palette.svg")));
   QObject::connect(openColorPaletteAction, &QAction::triggered, menu, [this]() {
    if (!mainWindow) return;
    const QString dockTitle = QStringLiteral("Color Palette");
    if (findDockByTitle(mainWindow, dockTitle)) {
     setDockVisible(mainWindow, dockTitle, true);
     activateDock(mainWindow, dockTitle);
     return;
    }
    auto* paletteWidget = new ArtifactColorPaletteWidget(mainWindow);
    addFloatingDock(mainWindow, dockTitle, QStringLiteral("color_palette_dock"),
                    paletteWidget, QRect(120, 120, 560, 640));
   });

   openColorScienceAction = menu->addAction("Color Science");
   openColorScienceAction->setIcon(QIcon(resolveIconPath("Studio/viewmenu_color_palette.svg")));
   QObject::connect(openColorScienceAction, &QAction::triggered, menu, [this]() {
    if (!mainWindow) return;
    const QString dockTitle = QStringLiteral("Color Science");
    if (findDockByTitle(mainWindow, dockTitle)) {
     setDockVisible(mainWindow, dockTitle, true);
     activateDock(mainWindow, dockTitle);
     return;
    }
    auto* colorScienceWidget = new ArtifactColorSciencePanel(mainWindow);
    addFloatingDock(mainWindow, dockTitle, QStringLiteral("color_science_dock"),
                    colorScienceWidget, QRect(140, 140, 720, 720));
   });

   menu->addSeparator();
   menu->addMenu(workspaceMenu);
   menu->addMenu(workspacePresetMenu);
   menu->addSeparator();
   menu->addAction(showGridAction);
   menu->addAction(snapToGridAction);
   menu->addAction(showGuidesAction);
   menu->addAction(snapToGuidesAction);
   menu->addAction(showRulersAction);
   menu->addAction(openColorScienceAction);
   menu->addSeparator();
    windowPanelsMenu = menu->addMenu("ウィンドウパネル(&W)");
    windowPanelsMenu->setIcon(QIcon(resolveIconPath("Studio/viewmenu_panels.svg")));

    // Dynamically rebuild the panels menu each time it opens
   QObject::connect(windowPanelsMenu, &QMenu::aboutToShow, menu, [this]() {
     rebuildWindowPanelsMenu();
    });

    menu->addSeparator();
    openReactiveEventEditorAction = menu->addAction("リアクティブイベントエディタ(&E)...");
    openReactiveEventEditorAction->setIcon(QIcon(resolveIconPath("Studio/viewmenu_reactive_events.svg")));
    QObject::connect(openReactiveEventEditorAction, &QAction::triggered, menu, [this]() {
    if (!mainWindow) return;
     if (!reactiveEventEditorWindow) {
      reactiveEventEditorWindow = new ArtifactReactiveEventEditorWindow(mainWindow);
      reactiveEventEditorWindow->setAttribute(Qt::WA_DeleteOnClose, true);
     }
     reactiveEventEditorWindow->present();
    });

    menu->addSeparator();
     auto* newBrowserAction = menu->addAction("新規アセットブラウザ(&A)");
     newBrowserAction->setIcon(QIcon(resolveIconPath("Studio/viewmenu_asset_browser.svg")));
     QObject::connect(newBrowserAction, &QAction::triggered, menu, [this]() {
      if (!mainWindow) return;
      newBrowserCount_++;
      auto* browser = new ArtifactAssetBrowser(mainWindow);
      const QString title = QStringLiteral("Asset Browser (%1)").arg(newBrowserCount_);
      addFloatingDock(mainWindow, title,
                      QStringLiteral("asset_browser_%1").arg(newBrowserCount_),
                      browser, QRect(100, 100, 800, 600));
     });

     menu->addSeparator();
     secondaryPreviewAction = menu->addAction("セカンドモニタープレビュー(&S)");
     secondaryPreviewAction->setShortcut(shortcuts.shortcut(ShortcutId::ViewSecondaryPreview));
     secondaryPreviewAction->setIcon(QIcon(resolveIconPath("Studio/viewmenu_secondary_preview.svg")));
     QObject::connect(secondaryPreviewAction, &QAction::triggered, menu, [this]() {
      showSecondaryPreview();
     });
    }

 ArtifactViewMenu::Impl::~Impl()
 {

 }

 void ArtifactViewMenu::Impl::refreshEnabledState()
 {
  auto* svc = ArtifactProjectService::instance();
  const bool hasProject = svc && svc->hasProject();
  const bool hasComp = hasProject && static_cast<bool>(svc->currentComposition().lock());
  auto* editor =
      activeCompositionEditor(mainWindow ? mainWindow : (menu_ ? menu_->window() : nullptr));
  const bool hasViewport = hasComp && editor && editor->renderController() &&
                           editor->renderController()->renderer();

  zoomInAction->setEnabled(hasComp);
  zoomOutAction->setEnabled(hasComp);
  defaultZoomAction->setEnabled(hasComp);
  fitToScreenAction->setEnabled(hasComp);
  if (viewportBookmarkMenu) {
   viewportBookmarkMenu->setEnabled(hasViewport);
  }
  if (viewportTemplateMenu) {
   viewportTemplateMenu->setEnabled(hasViewport);
  }
  if (compareMenu) {
   compareMenu->setEnabled(hasViewport || hasComp);
  }
  
  resolutionMenu->setEnabled(hasComp);
  if (resFullAction) {
    resFullAction->setEnabled(hasComp);
  }
  if (resHalfAction) {
    resHalfAction->setEnabled(hasComp);
  }
  if (resThirdAction) {
    resThirdAction->setEnabled(hasComp);
  }
  if (resQuarterAction) {
    resQuarterAction->setEnabled(hasComp);
  }
  qualityPresetMenu->setEnabled(hasComp);
  if (hasComp) {
    if (auto* settings = ArtifactCore::ArtifactAppSettings::instance()) {
      const int percent = settings->previewResolutionPercent();
      if (resFullAction) {
        resFullAction->setChecked(percent >= 88);
      }
      if (resHalfAction) {
        resHalfAction->setChecked(percent >= 42 && percent < 88);
      }
      if (resThirdAction) {
        resThirdAction->setChecked(percent >= 28 && percent < 42);
      }
      if (resQuarterAction) {
        resQuarterAction->setChecked(percent < 28);
      }
    }
  }
  
  showGridAction->setEnabled(hasComp);
  snapToGridAction->setEnabled(hasComp);
  showGuidesAction->setEnabled(hasComp);
  snapToGuidesAction->setEnabled(hasComp);
  showRulersAction->setEnabled(hasComp);
  useDisplayColorManagementAction->setEnabled(hasComp);
  if (openContentsViewerAction) {
   openContentsViewerAction->setEnabled(true);
  }
  if (openProjectPanelAction) {
   openProjectPanelAction->setEnabled(static_cast<bool>(mainWindow));
  }
  if (openColorPaletteAction) {
   openColorPaletteAction->setEnabled(true);
  }
  if (openColorScienceAction) {
   openColorScienceAction->setEnabled(true);
  }
  if (openReactiveEventEditorAction) {
   openReactiveEventEditorAction->setEnabled(true);
  }
 }

 void ArtifactViewMenu::Impl::refreshWorkspaceState()
 {
  if (workspaceModeActions_.isEmpty()) {
   return;
  }

  const auto* window = qobject_cast<ArtifactMainWindow*>(mainWindow);
  const WorkspaceMode mode = window ? window->workspaceMode() : WorkspaceMode::Default;
   for (QAction *action : workspaceModeActions_) {
   if (!action) {
    continue;
   }
   const bool checked = action->data().isValid() &&
                        action->data().toInt() == static_cast<int>(mode);
   action->setChecked(checked);
  }
 }

 void ArtifactViewMenu::Impl::refreshWorkspacePresetMenu()
 {
  if (!workspacePresetMenu) {
   return;
  }

  if (!mainWindow) {
   workspacePresetMenu->clear();
   workspacePresetMenu->setEnabled(false);
   cachedWorkspacePresetNames_.clear();
   return;
  }

  ArtifactWorkspaceManager manager;
  const QStringList presets = manager.presetNames();
  workspacePresetMenu->setEnabled(true);

  if (presets == cachedWorkspacePresetNames_ && !workspacePresetMenu->actions().isEmpty()) {
   return;
  }

  cachedWorkspacePresetNames_ = presets;
  workspacePresetMenu->clear();
  saveWorkspacePresetAction =
      workspacePresetMenu->addAction("現在のレイアウトを保存...");
  saveWorkspacePresetAction->setIcon(QIcon(resolveIconPath("Studio/viewmenu_save_layout.svg")));
  deleteWorkspacePresetAction =
      workspacePresetMenu->addAction("プリセットを削除...");
  deleteWorkspacePresetAction->setIcon(QIcon(resolveIconPath("Studio/viewmenu_delete.svg")));
  restoreWorkspaceSessionAction =
      workspacePresetMenu->addAction("最後のセッションを復元");
  restoreWorkspaceSessionAction->setIcon(QIcon(resolveIconPath("Studio/viewmenu_restore_session.svg")));

  QObject::connect(saveWorkspacePresetAction, &QAction::triggered, mainWindow,
                   [mw = mainWindow]() {
                     if (!mw) {
                       return;
                     }
                     const QString defaultName = QStringLiteral("Custom");
                     bool ok = false;
                     const QString presetName = QInputDialog::getText(
                                                    mw,
                                                    QStringLiteral("ワークスペースを保存"),
                                                    QStringLiteral("プリセット名を入力してください"),
                                                    QLineEdit::Normal, defaultName,
                                                    &ok)
                                                    .trimmed();
                     if (!ok || presetName.isEmpty()) {
                       return;
                     }
                     ArtifactWorkspaceManager manager;
                     if (!manager.savePreset(presetName, mw)) {
                       QMessageBox::warning(
                           mw, QStringLiteral("ワークスペースを保存"),
                           QStringLiteral("ワークスペースの保存に失敗しました。"));
                     }
                   });

  QObject::connect(deleteWorkspacePresetAction, &QAction::triggered,
                   mainWindow, [mw = mainWindow]() {
                     if (!mw) {
                       return;
                     }
                     ArtifactWorkspaceManager manager;
                     const QStringList presets = manager.presetNames();
                     if (presets.isEmpty()) {
                       QMessageBox::information(
                           mw, QStringLiteral("ワークスペース"),
                           QStringLiteral("削除できるプリセットがありません。"));
                       return;
                     }
                     bool ok = false;
                     const QString presetName = QInputDialog::getItem(
                         mw, QStringLiteral("プリセットを削除"),
                         QStringLiteral("削除するプリセットを選択してください"),
                         presets, 0, false, &ok);
                     if (!ok || presetName.trimmed().isEmpty()) {
                       return;
                     }
                     const QString confirmMessage =
                         QStringLiteral("プリセット「%1」を削除しますか？").arg(presetName);
                     if (QMessageBox::question(
                             mw, QStringLiteral("プリセットを削除"),
                             confirmMessage, QMessageBox::Yes | QMessageBox::No,
                             QMessageBox::No) != QMessageBox::Yes) {
                       return;
                     }
                     if (!manager.deletePreset(presetName)) {
                       QMessageBox::warning(
                           mw, QStringLiteral("プリセットを削除"),
                           QStringLiteral("プリセットの削除に失敗しました。"));
                     }
                   });

  QObject::connect(restoreWorkspaceSessionAction, &QAction::triggered,
                   mainWindow, [mw = mainWindow]() {
                     if (!mw) {
                       return;
                     }
                     ArtifactWorkspaceManager manager;
                     if (!manager.restoreSession(mw)) {
                       QMessageBox::information(
                           mw, QStringLiteral("ワークスペースを復元"),
                           QStringLiteral("復元できるセッションがありません。"));
                     }
                   });

  workspacePresetMenu->addSeparator();

  if (presets.isEmpty()) {
   QAction* empty = workspacePresetMenu->addAction("(no presets)");
   empty->setIcon(QIcon(resolveIconPath("Studio/viewmenu_empty_state.svg")));
   empty->setEnabled(false);
   return;
  }

  for (const QString& preset : presets) {
   QAction* action = workspacePresetMenu->addAction(preset);
   action->setIcon(QIcon(resolveIconPath("Studio/viewmenu_presets.svg")));
   QObject::connect(action, &QAction::triggered, mainWindow,
                    [mw = mainWindow, preset]() {
                     ArtifactWorkspaceManager manager;
                     if (!manager.restorePreset(preset, mw)) {
                      QMessageBox::warning(mw,
                                           QStringLiteral("ワークスペース"),
                                           QStringLiteral("プリセットの復元に失敗しました。"));
                     }
                    });
  }
 }

 W_OBJECT_IMPL(ArtifactViewMenu)

 ArtifactViewMenu::ArtifactViewMenu(QWidget* parent/*=nullptr*/):QMenu(parent),impl_(new Impl(this))
 {
  setTitle("表示(&V)");
  setTearOffEnabled(false);
  impl_->refreshEnabledState();
 }

 ArtifactViewMenu::~ArtifactViewMenu()
 {
  delete impl_;
 }

 void ArtifactViewMenu::registerView(const QString& name, QWidget* view)
 {
  if (!impl_ || !impl_->windowPanelsMenu || !view) return;

  QAction* action = impl_->windowPanelsMenu->addAction(name);
  action->setIcon(QIcon(resolveIconPath("Studio/viewmenu_panels.svg")));
  action->setCheckable(true);
  action->setChecked(view->isVisible());

  QPointer<QWidget> guardedView(view);
  QObject::connect(action, &QAction::toggled, this, [guardedView](bool checked) {
   if (!guardedView) return;
   guardedView->setVisible(checked);
   if (checked) {
    guardedView->raise();
    guardedView->activateWindow();
   }
  });

  QObject::connect(view, &QWidget::destroyed, this, [this, action]() {
   if (impl_ && impl_->windowPanelsMenu && action) {
    impl_->windowPanelsMenu->removeAction(action);
   }
   if (action) {
    action->deleteLater();
   }
  });

  auto syncAction = [action, guardedView]() {
   if (!action || !guardedView) return;
   const bool vis = guardedView->isVisible();
   if (action->isChecked() != vis) {
    action->setChecked(vis);
   }
  };

  if (view->isWindow()) {
   QObject::connect(view, &QWidget::windowTitleChanged, this, [action](const QString& t) {
    if (action && !t.isEmpty()) action->setText(t);
   });
  }

  QObject::connect(action, &QAction::hovered, this, [syncAction]() mutable {
   syncAction();
  });
 }

 void ArtifactViewMenu::setMainWindow(QWidget* mw)
 {
  impl_->mainWindow = mw;
  if (impl_) {
   impl_->cachedWorkspacePresetNames_.clear();
   impl_->cachedDockTitles_.clear();
   impl_->refreshWorkspaceState();
  }
 }

void ArtifactViewMenu::Impl::rebuildWindowPanelsMenu()
{
  if (!windowPanelsMenu || !mainWindow) return;

  const QStringList titles = dockTitles(mainWindow);
  if (titles == cachedDockTitles_) {
   for (QAction* action : windowPanelsMenu->actions()) {
    if (!action || !action->isCheckable()) {
     continue;
    }
    const QString title = action->text();
    action->setChecked(isDockVisible(mainWindow, title));
   }
   return;
  }

  cachedDockTitles_ = titles;
  windowPanelsMenu->clear();

  for (const QString& title : titles) {
   QAction* action = windowPanelsMenu->addAction(title);
   action->setIcon(QIcon(resolveIconPath("Studio/viewmenu_panels.svg")));
   action->setCheckable(true);
   action->setChecked(isDockVisible(mainWindow, title));

   QObject::connect(action, &QAction::triggered, mainWindow, [mw = mainWindow, title](bool checked) {
    setDockVisible(mw, title, checked);
    if (checked) {
     activateDock(mw, title);
    }
   });
  }

  if (titles.isEmpty()) {
   QAction* none = windowPanelsMenu->addAction("(no panels)");
   none->setIcon(QIcon(resolveIconPath("Studio/viewmenu_empty_state.svg")));
   none->setEnabled(false);
  }
}

void ArtifactViewMenu::Impl::refreshViewportBookmarkMenu()
{
  if (!viewportBookmarkMenu || !menu_) {
  return;
  }

 viewportBookmarkMenu->clear();
 saveViewportBookmarkAction =
     viewportBookmarkMenu->addAction("現在のカメラを保存...");
 saveViewportBookmarkAction->setIcon(QIcon(resolveIconPath("Studio/viewmenu_save.svg")));
 saveViewportBookmarkAction->setToolTip("Store the current camera / viewport state under a name");
 deleteViewportBookmarkAction =
     viewportBookmarkMenu->addAction("カメラブックマークを削除...");
 deleteViewportBookmarkAction->setIcon(QIcon(resolveIconPath("Studio/viewmenu_delete.svg")));
 deleteViewportBookmarkAction->setToolTip("Remove a saved camera / viewport bookmark");

 QObject::connect(saveViewportBookmarkAction, &QAction::triggered, menu_,
                   [this]() {
                    QWidget* dialogParent = mainWindow
                                                ? static_cast<QWidget*>(mainWindow)
                                                : static_cast<QWidget*>(menu_);
                   if (!dialogParent) {
                    return;
                   }
                   auto* editor = activeCompositionEditor(dialogParent);
                   const auto comp = currentViewportComposition(editor);
                   if (!editor || !comp) {
                    QMessageBox::information(
                        dialogParent, QStringLiteral("Camera ブックマーク"),
                        QStringLiteral("保存先のコンポジションまたは viewport が見つかりません。"));
                    return;
                   }

                   const auto state = currentViewportBookmarkState(editor);
                   if (!state) {
                    QMessageBox::warning(
                        dialogParent, QStringLiteral("Camera ブックマーク"),
                        QStringLiteral("現在の viewport 状態を取得できませんでした。"));
                    return;
                   }

                   bool ok = false;
                   const QString defaultName = QStringLiteral("Bookmark");
                   const QString bookmarkName = QInputDialog::getText(
                       dialogParent, QStringLiteral("Camera ブックマークを保存"),
                       QStringLiteral("ブックマーク名を入力してください"), QLineEdit::Normal,
                       defaultName, &ok)
                                                    .trimmed();
                   if (!ok || bookmarkName.isEmpty()) {
                    return;
                   }

                   ViewportBookmarkEntry entry = *state;
                   entry.name = bookmarkName;
                   ViewportBookmarkStore store;
                   if (!store.saveBookmark(comp->id().toString(), entry)) {
                    QMessageBox::warning(
                        dialogParent, QStringLiteral("Camera ブックマーク"),
                        QStringLiteral("ブックマークの保存に失敗しました。"));
                   }
                  });

 QObject::connect(deleteViewportBookmarkAction, &QAction::triggered, menu_,
                   [this]() {
                    QWidget* dialogParent = mainWindow
                                                ? static_cast<QWidget*>(mainWindow)
                                                : static_cast<QWidget*>(menu_);
                   if (!dialogParent) {
                    return;
                   }
                   const auto comp = currentViewportComposition(
                       activeCompositionEditor(dialogParent));
                   if (!comp) {
                    QMessageBox::information(
                        dialogParent, QStringLiteral("Camera ブックマーク"),
                        QStringLiteral("削除対象のコンポジションが見つかりません。"));
                    return;
                   }

                   ViewportBookmarkStore store;
                   const QStringList names = store.bookmarkNames(comp->id().toString());
                   if (names.isEmpty()) {
                    QMessageBox::information(
                        dialogParent, QStringLiteral("Camera ブックマーク"),
                        QStringLiteral("削除できるブックマークがありません。"));
                    return;
                   }

                   bool ok = false;
                   const QString bookmarkName = QInputDialog::getItem(
                       dialogParent, QStringLiteral("Camera ブックマークを削除"),
                       QStringLiteral("削除するブックマークを選択してください"), names, 0, false,
                       &ok);
                   if (!ok || bookmarkName.trimmed().isEmpty()) {
                    return;
                   }

                   const QString confirmMessage =
                       QStringLiteral("ブックマーク「%1」を削除しますか？").arg(bookmarkName);
                   if (QMessageBox::question(
                           dialogParent, QStringLiteral("Camera ブックマークを削除"),
                           confirmMessage, QMessageBox::Yes | QMessageBox::No,
                           QMessageBox::No) != QMessageBox::Yes) {
                    return;
                   }

                   if (!store.deleteBookmark(comp->id().toString(), bookmarkName)) {
                    QMessageBox::warning(
                        dialogParent, QStringLiteral("Camera ブックマーク"),
                        QStringLiteral("ブックマークの削除に失敗しました。"));
                   }
                  });

 const auto editor =
     activeCompositionEditor(mainWindow ? mainWindow : (menu_ ? menu_->window() : nullptr));
 const auto comp = currentViewportComposition(editor);
 const QString compositionId = comp ? comp->id().toString() : QString();
 ViewportBookmarkStore store;
 const QStringList names =
     comp ? store.bookmarkNames(compositionId) : QStringList{};

 viewportBookmarkMenu->addSeparator();

 if (!editor || !comp || names.isEmpty()) {
  QAction* empty = viewportBookmarkMenu->addAction("(no bookmarks)");
  empty->setIcon(QIcon(resolveIconPath("Studio/viewmenu_empty_state.svg")));
  empty->setEnabled(false);
  return;
 }

 for (const QString& bookmarkName : names) {
  QAction* action = viewportBookmarkMenu->addAction(bookmarkName);
  action->setIcon(QIcon(resolveIconPath("Studio/viewmenu_bookmarks.svg")));
  QObject::connect(action, &QAction::triggered, menu_,
                    [this, bookmarkName, compositionId]() {
                     QWidget* dialogParent = mainWindow
                                                 ? static_cast<QWidget*>(mainWindow)
                                                 : static_cast<QWidget*>(menu_);
                    if (!dialogParent) {
                     return;
                    }
                    auto* editor = activeCompositionEditor(dialogParent);
                    if (!editor) {
                     QMessageBox::information(
                         dialogParent, QStringLiteral("Camera ブックマーク"),
                         QStringLiteral("復元先の viewport が見つかりません。"));
                     return;
                    }
                    ViewportBookmarkStore store;
                    const auto entry = store.bookmark(compositionId, bookmarkName);
                    if (!entry) {
                     QMessageBox::warning(
                         dialogParent, QStringLiteral("Camera ブックマーク"),
                         QStringLiteral("ブックマーク「%1」を読み込めませんでした。")
                             .arg(bookmarkName));
                     return;
                    }
                   if (!applyViewportBookmarkState(editor, *entry)) {
                     QMessageBox::warning(
                         dialogParent, QStringLiteral("Camera ブックマーク"),
                         QStringLiteral("ブックマーク「%1」の復元に失敗しました。")
                             .arg(bookmarkName));
                    }
                   });
 }
}

void ArtifactViewMenu::Impl::refreshViewportTemplateMenu()
{
 if (!viewportTemplateMenu || !menu_) {
  return;
 }

 viewportTemplateMenu->clear();
 saveViewportTemplateAction =
     viewportTemplateMenu->addAction("現在のビュー設定を保存...");
 saveViewportTemplateAction->setIcon(QIcon(resolveIconPath("Studio/viewmenu_save.svg")));
 deleteViewportTemplateAction =
     viewportTemplateMenu->addAction("ビュー設定を削除...");
 deleteViewportTemplateAction->setIcon(QIcon(resolveIconPath("Studio/viewmenu_delete.svg")));

 QObject::connect(saveViewportTemplateAction, &QAction::triggered, menu_,
                   [this]() {
                    QWidget* dialogParent = mainWindow
                                                ? static_cast<QWidget*>(mainWindow)
                                                : static_cast<QWidget*>(menu_);
                    if (!dialogParent) {
                     return;
                    }
                    auto* editor = activeCompositionEditor(dialogParent);
                    const auto comp = currentViewportComposition(editor);
                    if (!editor || !comp) {
                     QMessageBox::information(
                         dialogParent, QStringLiteral("View Template"),
                         QStringLiteral("保存先のコンポジションまたは viewport が見つかりません。"));
                     return;
                    }

                    const auto state = currentViewportTemplateState(editor);
                    if (!state) {
                     QMessageBox::warning(
                         dialogParent, QStringLiteral("View Template"),
                         QStringLiteral("現在の viewport 設定を取得できませんでした。"));
                     return;
                    }

                    bool ok = false;
                    const QString defaultName = QStringLiteral("Template");
                    const QString templateName = QInputDialog::getText(
                        dialogParent, QStringLiteral("View Template を保存"),
                        QStringLiteral("テンプレート名を入力してください"), QLineEdit::Normal,
                        defaultName, &ok)
                                                    .trimmed();
                    if (!ok || templateName.isEmpty()) {
                     return;
                    }

                    ViewportTemplateEntry entry = *state;
                    entry.name = templateName;
                    ViewportTemplateStore store;
                    if (!store.saveTemplate(comp->id().toString(), entry)) {
                     QMessageBox::warning(
                         dialogParent, QStringLiteral("View Template"),
                         QStringLiteral("ビュー設定の保存に失敗しました。"));
                    }
                   });

 QObject::connect(deleteViewportTemplateAction, &QAction::triggered, menu_,
                   [this]() {
                    QWidget* dialogParent = mainWindow
                                                ? static_cast<QWidget*>(mainWindow)
                                                : static_cast<QWidget*>(menu_);
                    if (!dialogParent) {
                     return;
                    }
                    const auto comp = currentViewportComposition(
                        activeCompositionEditor(dialogParent));
                    if (!comp) {
                     QMessageBox::information(
                         dialogParent, QStringLiteral("View Template"),
                         QStringLiteral("削除対象のコンポジションが見つかりません。"));
                     return;
                    }

                    ViewportTemplateStore store;
                    const QStringList names = store.templateNames(comp->id().toString());
                    if (names.isEmpty()) {
                     QMessageBox::information(
                         dialogParent, QStringLiteral("View Template"),
                         QStringLiteral("削除できるビュー設定がありません。"));
                     return;
                    }

                    bool ok = false;
                    const QString templateName = QInputDialog::getItem(
                        dialogParent, QStringLiteral("View Template を削除"),
                        QStringLiteral("削除するテンプレートを選択してください"), names, 0,
                        false, &ok);
                    if (!ok || templateName.trimmed().isEmpty()) {
                     return;
                    }

                    const QString confirmMessage =
                        QStringLiteral("ビュー設定「%1」を削除しますか？").arg(templateName);
                    if (QMessageBox::question(
                            dialogParent, QStringLiteral("View Template を削除"),
                            confirmMessage, QMessageBox::Yes | QMessageBox::No,
                            QMessageBox::No) != QMessageBox::Yes) {
                     return;
                    }

                    if (!store.deleteTemplate(comp->id().toString(), templateName)) {
                     QMessageBox::warning(
                         dialogParent, QStringLiteral("View Template"),
                         QStringLiteral("ビュー設定の削除に失敗しました。"));
                    }
                   });

 const auto editor =
     activeCompositionEditor(mainWindow ? mainWindow : (menu_ ? menu_->window() : nullptr));
 const auto comp = currentViewportComposition(editor);
 const QString compositionId = comp ? comp->id().toString() : QString();
 ViewportTemplateStore store;
 const QStringList names =
     comp ? store.templateNames(compositionId) : QStringList{};

 viewportTemplateMenu->addSeparator();

 if (!editor || !comp || names.isEmpty()) {
  QAction* empty = viewportTemplateMenu->addAction("(no templates)");
  empty->setIcon(QIcon(resolveIconPath("Studio/viewmenu_empty_state.svg")));
  empty->setEnabled(false);
  return;
 }

 for (const QString& templateName : names) {
  QAction* action = viewportTemplateMenu->addAction(templateName);
  action->setIcon(QIcon(resolveIconPath("Studio/viewmenu_presets.svg")));
  QObject::connect(action, &QAction::triggered, menu_,
                    [this, templateName, compositionId]() {
                     QWidget* dialogParent = mainWindow
                                                 ? static_cast<QWidget*>(mainWindow)
                                                 : static_cast<QWidget*>(menu_);
                    if (!dialogParent) {
                     return;
                    }
                    auto* editor = activeCompositionEditor(dialogParent);
                    if (!editor) {
                     QMessageBox::information(
                         dialogParent, QStringLiteral("View Template"),
                         QStringLiteral("復元先の viewport が見つかりません。"));
                     return;
                    }
                    ViewportTemplateStore store;
                    const auto entry = store.templateEntry(compositionId, templateName);
                    if (!entry) {
                     QMessageBox::warning(
                         dialogParent, QStringLiteral("View Template"),
                         QStringLiteral("ビュー設定「%1」を読み込めませんでした。")
                             .arg(templateName));
                     return;
                    }
                   if (!applyViewportTemplateState(editor, *entry)) {
                     QMessageBox::warning(
                         dialogParent, QStringLiteral("View Template"),
                         QStringLiteral("ビュー設定「%1」の復元に失敗しました。")
                             .arg(templateName));
                    }
                   });
 }
}

void ArtifactViewMenu::Impl::refreshCompareMenu()
{
 if (!compareMenu || !menu_ || !compareGroup) {
  return;
 }

 compareMenu->clear();
 compareGroup->setExclusive(true);

 compareSurfaceAction = compareMenu->addAction("A/B Surface in Contents Viewer");
 compareSurfaceAction->setIcon(QIcon(resolveIconPath("Studio/viewmenu_contents_viewer.svg")));
 QObject::connect(compareSurfaceAction, &QAction::triggered, menu_,
                   []() { openContentsViewerCompareSurface(); });

 compareMenu->addSeparator();

 const auto bindCompareAction = [this](QAction* action, CompositionCompareMode mode) {
  if (!action) {
   return;
  }
  action->setCheckable(true);
  compareGroup->addAction(action);
  QObject::connect(action, &QAction::triggered, menu_, [this, mode]() {
   auto* editor = activeCompositionEditor(mainWindow ? mainWindow : (menu_ ? menu_->window() : nullptr));
   auto* controller = editor ? editor->renderController() : nullptr;
   if (!controller) {
    return;
   }
   controller->setCompareMode(mode);
  });
 };

 compareOffAction = compareMenu->addAction("Compare: Off");
 compareAAction = compareMenu->addAction("Compare: A");
 compareBAction = compareMenu->addAction("Compare: B");
 compareDiffAction = compareMenu->addAction("Compare: Diff");
   compareReferenceAction = compareMenu->addAction("Reference Pin");
   compareReferenceAction->setCheckable(true);
 compareReferenceAction->setIcon(QIcon(resolveIconPath("Studio/viewmenu_bookmarks.svg")));
   compareMenu->addSeparator();
   auto& shortcuts = ShortcutBindings::instance();
   xRayAction = compareMenu->addAction("X-Ray (透過)");
   xRayAction->setCheckable(true);
   xRayAction->setIcon(QIcon(resolveIconPath("Studio/viewmenu_color_palette.svg")));
   xRayAction->setToolTip(
       QStringLiteral("Show selected layer through occlusion (%1)")
           .arg(shortcuts.shortcutText(ShortcutId::ViewToggleXRay)));
   xRayAction->setShortcut(shortcuts.shortcut(ShortcutId::ViewToggleXRay));
   isolationAction = compareMenu->addAction("Isolation (選択のみ)");
   isolationAction->setCheckable(true);
   isolationAction->setIcon(QIcon(resolveIconPath("Studio/viewmenu_panels.svg")));
   isolationAction->setToolTip(
       QStringLiteral("Show only the selected layer (%1)")
           .arg(shortcuts.shortcutText(ShortcutId::ViewToggleIsolation)));
   isolationAction->setShortcut(shortcuts.shortcut(ShortcutId::ViewToggleIsolation));

 bindCompareAction(compareOffAction, CompositionCompareMode::Off);
 bindCompareAction(compareAAction, CompositionCompareMode::A);
 bindCompareAction(compareBAction, CompositionCompareMode::B);
 bindCompareAction(compareDiffAction, CompositionCompareMode::Diff);

  QObject::connect(compareReferenceAction, &QAction::toggled, menu_, [this](bool checked) {
  auto* editor = activeCompositionEditor(mainWindow ? mainWindow : (menu_ ? menu_->window() : nullptr));
  auto* controller = editor ? editor->renderController() : nullptr;
  if (!controller) {
   return;
  }
  controller->setReferencePinned(checked);
  if (checked) {
   if (auto* playback = ArtifactPlaybackService::instance()) {
    controller->setReferenceFrame(static_cast<int>(playback->currentFrame().framePosition()));
   }
  }
 });

 QObject::connect(xRayAction, &QAction::toggled, menu_, [this](bool checked) {
  auto* editor = activeCompositionEditor(mainWindow ? mainWindow : (menu_ ? menu_->window() : nullptr));
  auto* controller = editor ? editor->renderController() : nullptr;
  if (!controller) {
   return;
  }
  controller->setShowXRayOverlay(checked);
  if (editor->window()) {
   editor->window()->update();
  }
 });

 QObject::connect(isolationAction, &QAction::toggled, menu_, [this](bool checked) {
  auto* editor = activeCompositionEditor(mainWindow ? mainWindow : (menu_ ? menu_->window() : nullptr));
  auto* controller = editor ? editor->renderController() : nullptr;
  if (!controller) {
   return;
  }
  controller->setShowIsolationOverlay(checked);
  if (editor->window()) {
   editor->window()->update();
  }
 });

 const auto editor =
     activeCompositionEditor(mainWindow ? mainWindow : (menu_ ? menu_->window() : nullptr));
 const auto controller = editor ? editor->renderController() : nullptr;
 const auto mode = controller ? controller->compareMode() : CompositionCompareMode::Off;
 const bool pinned = controller && controller->isReferencePinned();
 const bool hasControl = controller != nullptr;

 compareSurfaceAction->setEnabled(editor != nullptr && currentViewportComposition(editor));
 compareOffAction->setChecked(mode == CompositionCompareMode::Off);
 compareAAction->setChecked(mode == CompositionCompareMode::A);
 compareBAction->setChecked(mode == CompositionCompareMode::B);
 compareDiffAction->setChecked(mode == CompositionCompareMode::Diff);
 compareReferenceAction->setChecked(pinned);
 compareOffAction->setEnabled(hasControl);
 compareAAction->setEnabled(hasControl);
 compareBAction->setEnabled(hasControl);
 compareDiffAction->setEnabled(hasControl);
 compareReferenceAction->setEnabled(hasControl);
 xRayAction->setChecked(hasControl && controller->isShowXRayOverlay());
 isolationAction->setChecked(hasControl && controller->isShowIsolationOverlay());
 xRayAction->setEnabled(hasControl);
 isolationAction->setEnabled(hasControl);

 compareMenu->addSeparator();
 compareMenu->setEnabled(true);
}

void ArtifactViewMenu::Impl::refreshSelectionSetMenu()
{
 if (!selectionSetMenu || !menu_) {
  return;
 }

 selectionSetMenu->clear();
 saveSelectionSetAction =
     selectionSetMenu->addAction("現在の選択を保存...");
 saveSelectionSetAction->setIcon(QIcon(resolveIconPath("Studio/viewmenu_save.svg")));

 QObject::connect(saveSelectionSetAction, &QAction::triggered, menu_,
                   [this]() {
                    QWidget* dialogParent = mainWindow
                                                ? static_cast<QWidget*>(mainWindow)
                                                : static_cast<QWidget*>(menu_);
                    if (!dialogParent) {
                     return;
                    }
                    auto* app = ArtifactApplicationManager::instance();
                    auto* selection = app ? app->layerSelectionManager() : nullptr;
                    auto comp = selection ? selection->activeComposition() : ArtifactCompositionPtr{};
                    if (!selection || !comp) {
                     QMessageBox::information(
                         dialogParent, QStringLiteral("Selection セット"),
                         QStringLiteral("保存先のコンポジションまたは選択が見つかりません。"));
                     return;
                    }
                    auto state = currentSelectionSetState();
                    if (!state) {
                     QMessageBox::warning(
                         dialogParent, QStringLiteral("Selection セット"),
                         QStringLiteral("現在の選択を取得できませんでした。"));
                     return;
                    }

                    bool ok = false;
                    const QString name = QInputDialog::getText(
                        dialogParent, QStringLiteral("Selection セットを保存"),
                        QStringLiteral("セット名を入力してください"), QLineEdit::Normal,
                        QStringLiteral("Selection Set"), &ok).trimmed();
                    if (!ok || name.isEmpty()) {
                     return;
                    }

                    state->name = name;
                    SelectionSetStore store;
                    if (!store.saveSelectionSet(comp->id().toString(), *state)) {
                     QMessageBox::warning(
                         dialogParent, QStringLiteral("Selection セット"),
                         QStringLiteral("Selection set の保存に失敗しました。"));
                    }
                   });

 selectionSetMenu->addSeparator();

 const auto editor = activeCompositionEditor(mainWindow ? mainWindow : (menu_ ? menu_->window() : nullptr));
 const auto comp = currentViewportComposition(editor);
 const QString compositionId = comp ? comp->id().toString() : QString();
 SelectionSetStore store;
 const QStringList names = comp ? store.selectionSetNames(compositionId) : QStringList{};

 if (!editor || !comp || names.isEmpty()) {
  QAction* empty = selectionSetMenu->addAction("(no selection sets)");
  empty->setIcon(QIcon(resolveIconPath("Studio/viewmenu_empty_state.svg")));
  empty->setEnabled(false);
  return;
 }

 for (const QString& name : names) {
  QAction* action = selectionSetMenu->addAction(name);
  action->setIcon(QIcon(resolveIconPath("Studio/viewmenu_bookmarks.svg")));
  QObject::connect(action, &QAction::triggered, menu_,
                    [this, name, compositionId]() {
                     QWidget* dialogParent = mainWindow
                                                 ? static_cast<QWidget*>(mainWindow)
                                                 : static_cast<QWidget*>(menu_);
                     if (!dialogParent) {
                      return;
                     }
                     auto* app = ArtifactApplicationManager::instance();
                     auto* selection = app ? app->layerSelectionManager() : nullptr;
                      auto comp = selection ? selection->activeComposition() : ArtifactCompositionPtr{};
                     if (!selection || !comp) {
                      return;
                     }
                     SelectionSetStore store;
                     const auto entry = store.selectionSet(compositionId, name);
                     if (!entry) {
                      QMessageBox::warning(
                          dialogParent, QStringLiteral("Selection セット"),
                          QStringLiteral("Selection set「%1」を読み込めませんでした。").arg(name));
                      return;
                     }
                     selection->clearSelection();
                     const auto layers = comp->allLayer();
                     for (const QString& idText : entry->layerIds) {
                      const LayerID id(idText);
                      for (const auto& layer : layers) {
                       if (layer && layer->id() == id) {
                        selection->addToSelection(layer);
                        break;
                       }
                      }
                     }
                     if (entry->frame >= 0) {
                      if (auto* playback = ArtifactPlaybackService::instance()) {
                       playback->goToFrame(FramePosition(entry->frame));
                      }
                     }
                    });
 }
}

void ArtifactViewMenu::Impl::showProjectPanel()
{
 if (!mainWindow) {
  return;
 }

 const QString dockTitle = QStringLiteral("Project");
 if (findDockByTitle(mainWindow, dockTitle)) {
  setDockVisible(mainWindow, dockTitle, true);
  activateDock(mainWindow, dockTitle);
  return;
 }

 QMessageBox::information(mainWindow, QStringLiteral("Project パネル"),
                          QStringLiteral("Project パネルが見つかりません。"));
}

void ArtifactViewMenu::Impl::refreshSecondaryPreview()
{
 if (!secondaryPreviewWindow || !secondaryPreviewWindow->isVisible()) {
  return;
 }

 auto* playback = ArtifactPlaybackService::instance();
 if (!playback || !playback->currentComposition()) {
  secondaryPreviewWindow->updatePreviewImage(QImage());
  secondaryPreviewWindow->setStatusMessage(
      QStringLiteral("RAM preview: unavailable"));
  return;
 }

 const auto currentFrame = playback->currentFrame().framePosition();
 const auto range = playback->frameRange();
 const auto currentComp = playback->currentComposition();
 const QString compName = currentComp ? currentComp->settings().compositionName().toQString()
                                      : QString();
 const auto state = playback->ramPreviewFrameState(currentFrame);
 const QString stateReason = ramPreviewNotReadyReason(state);

 secondaryPreviewWindow->updateFrameInfo(currentFrame, range.duration(), compName);
 secondaryPreviewWindow->setStatusMessage(
     QStringLiteral("RAM preview: frame %1 / %2 | %3%4")
         .arg(currentFrame)
         .arg(range.duration())
         .arg(state.playable ? QStringLiteral("ready") : stateReason)
         .arg(compName.isEmpty() ? QString() : QStringLiteral(" | %1").arg(compName)));

 ArtifactCore::ImageF32x4_RGBA previewImage;
 if (playback->tryGetRamPreviewFrameImage(currentFrame, previewImage)) {
  secondaryPreviewWindow->updatePreviewImage(previewImage.toQImage());
 } else {
  secondaryPreviewWindow->updatePreviewImage(QImage());
  if (!state.playable) {
    secondaryPreviewWindow->setStatusMessage(
        QStringLiteral("RAM preview: frame %1 / %2 | %3")
            .arg(currentFrame)
            .arg(range.duration())
            .arg(stateReason));
  }
 }
}

void ArtifactViewMenu::Impl::showSecondaryPreview()
{
 if (!mainWindow) {
  return;
 }

 const auto screens = QGuiApplication::screens();
 if (screens.size() < 2) {
  QMessageBox::information(mainWindow, QStringLiteral("セカンドモニタープレビュー"),
                           QStringLiteral("2つ目のモニターが検出されていません。\nマルチディスプレイ環境でご利用ください。"));
  return;
 }

  if (!secondaryPreviewWindow) {
  secondaryPreviewWindow = new ArtifactSecondaryPreviewWindow(mainWindow);
  secondaryPreviewWindow->setAttribute(Qt::WA_DeleteOnClose, false);
 }

 secondaryPreviewWindow->showOnScreen(1);
 refreshSecondaryPreview();
}

};

