module;
#include <utility>
#include <QMenu>
#include <wobjectimpl.h>
#include <QAction>
#include <QKeySequence>
#include <QDebug>
#include <QStatusBar>
#include <QClipboard>
#include <QApplication>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QInputDialog>
#include <QLineEdit>
module Artifact.Menu.Edit;
import std;

import Artifact.Application.Manager;
import Artifact.Project.Manager;
import Artifact.Service.ActiveContext;
import Artifact.Layers.Selection.Manager;
import Artifact.Layer.Abstract;
import Artifact.Composition.Abstract;
import Undo.UndoManager;
import UI.ShortcutBindings;
import ApplicationSettingDialog;
import Artifact.Service.Project;
import Utils.Path;

namespace Artifact {
 using namespace ArtifactCore;

class  ArtifactEditMenu::Impl {
private:

  bool projectCreated_ = false;
  QWidget* parentWidget_ = nullptr;
 public:
  Impl(QMenu* menu, QWidget* mainWindow);
  void rebuildMenu();
  void syncUIState();  // UI 状態を同期
  QAction* undoAction = nullptr;
  QAction* redoAction = nullptr;
  QAction* duplicateAction = nullptr;
  QAction* splitAction = nullptr;
  QAction* trimInAction = nullptr;
  QAction* trimOutAction = nullptr;
  QAction* selectAllAction = nullptr;
  QAction* selectNoneAction = nullptr;
  QAction* invertSelectionAction = nullptr;
  QAction* selectSameTypeAction = nullptr;
  QAction* findAction = nullptr;
  QAction* preferencesAction = nullptr;

  QAction* copyAction_ = nullptr;
  QAction* cutAction_ = nullptr;
  QAction* pasteAction_ = nullptr;
  QAction* deleteAction_ = nullptr;

  void handleCopyAction();
  void handleCutAction();
  void handlePasteAction();
  void handleDelete();
  void handleUndo();
  void handleRedo();
  void handleDuplicate();
  void handleSplit();
  void handleTrimIn();
  void handleTrimOut();
  void handleSelectAll();
  void handleSelectNone();
  void handleInvertSelection();
  void handleSelectSameType();
  void handleFind();
  void handlePreferences();

  ArtifactActiveContextService* getActiveContext();
 };

 ArtifactEditMenu::Impl::Impl(QMenu* menu, QWidget* mainWindow)
 {
  parentWidget_ = mainWindow ? mainWindow : menu;
  auto& shortcuts = ShortcutBindings::instance();
  // Basic edit actions
  undoAction = new QAction("元に戻す (&U)");
  undoAction->setShortcut(shortcuts.shortcut(ShortcutId::Undo));
  undoAction->setIcon(QIcon(resolveIconPath("Material/undo.svg")));

  redoAction = new QAction("やり直し (&R)");
  redoAction->setShortcut(shortcuts.shortcut(ShortcutId::Redo));
  redoAction->setIcon(QIcon(resolveIconPath("Material/redo.svg")));

  duplicateAction = new QAction("複製 (&D)");
  duplicateAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_D));
  duplicateAction->setIcon(QIcon(resolveIconPath("Material/content_copy.svg")));

  splitAction = new QAction("レイヤーを分割 (&S)");
  splitAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_D));
  splitAction->setIcon(QIcon(resolveIconPath("Material/content_cut.svg")));

  trimInAction = new QAction("インポイントを現在の時間にトリム");
  trimOutAction = new QAction("アウトポイントを現在の時間にトリム");

  selectAllAction = new QAction("すべて選択 (&A)");
  selectAllAction->setShortcut(QKeySequence::SelectAll);

  selectNoneAction = new QAction("選択解除");
  selectNoneAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_A));

  invertSelectionAction = new QAction("選択を反転");

  selectSameTypeAction = new QAction("同じ種類を選択");

  findAction = new QAction("検索 (&F)...");
  findAction->setShortcut(QKeySequence::Find);
  findAction->setIcon(QIcon(resolveIconPath("Material/search.svg")));

  preferencesAction = new QAction("環境設定 (&P)...");
  preferencesAction->setIcon(QIcon(resolveIconPath("Material/settings.svg")));

  copyAction_ = new QAction("コピー (&C)");
  copyAction_->setShortcut(QKeySequence::Copy);
  copyAction_->setIcon(QIcon(resolveIconPath("Material/content_copy.svg")));

  cutAction_ = new QAction("切り取り (&T)");
  cutAction_->setShortcut(QKeySequence::Cut);
  cutAction_->setIcon(QIcon(resolveIconPath("Material/content_cut.svg")));

  pasteAction_ = new QAction("貼り付け (&P)");
  pasteAction_->setShortcut(QKeySequence::Paste);
  pasteAction_->setIcon(QIcon(resolveIconPath("Material/content_paste.svg")));

  deleteAction_ = new QAction("削除 (&D)");
  deleteAction_->setShortcut(QKeySequence::Delete);
  deleteAction_->setIcon(QIcon(resolveIconPath("Material/delete.svg")));

  // Build menu
  menu->addAction(undoAction);
  menu->addAction(redoAction);
  menu->addSeparator();
  menu->addAction(copyAction_);
  menu->addAction(cutAction_);
  menu->addAction(pasteAction_);
  menu->addAction(deleteAction_);
  menu->addAction(duplicateAction);
  menu->addSeparator();
  menu->addAction(splitAction);
  menu->addAction(trimInAction);
  menu->addAction(trimOutAction);
  menu->addSeparator();
  
  // Select submenu
  QMenu* selectMenu = menu->addMenu("選択");
  selectMenu->addAction(selectAllAction);
  selectMenu->addAction(selectNoneAction);
  selectMenu->addAction(invertSelectionAction);
  selectMenu->addSeparator();
  selectMenu->addAction(selectSameTypeAction);

  menu->addSeparator();
  menu->addAction(findAction);
  menu->addSeparator();
  menu->addAction(preferencesAction);

  // Connections
  QObject::connect(undoAction, &QAction::triggered, menu, [this]() { handleUndo(); });
  QObject::connect(redoAction, &QAction::triggered, menu, [this]() { handleRedo(); });
  QObject::connect(copyAction_, &QAction::triggered, menu, [this]() { handleCopyAction(); });
  QObject::connect(cutAction_, &QAction::triggered, menu, [this]() { handleCutAction(); });
  QObject::connect(pasteAction_, &QAction::triggered, menu, [this]() { handlePasteAction(); });
  QObject::connect(deleteAction_, &QAction::triggered, menu, [this]() { handleDelete(); });
  QObject::connect(duplicateAction, &QAction::triggered, menu, [this]() { handleDuplicate(); });
  QObject::connect(splitAction, &QAction::triggered, menu, [this]() { handleSplit(); });
  QObject::connect(trimInAction, &QAction::triggered, menu, [this]() { handleTrimIn(); });
  QObject::connect(trimOutAction, &QAction::triggered, menu, [this]() { handleTrimOut(); });
  QObject::connect(selectAllAction, &QAction::triggered, menu, [this]() { handleSelectAll(); });
  QObject::connect(selectNoneAction, &QAction::triggered, menu, [this]() { handleSelectNone(); });
  QObject::connect(invertSelectionAction, &QAction::triggered, menu, [this]() { handleInvertSelection(); });
  QObject::connect(selectSameTypeAction, &QAction::triggered, menu, [this]() { handleSelectSameType(); });
  QObject::connect(findAction, &QAction::triggered, menu, [this]() { handleFind(); });
  QObject::connect(preferencesAction, &QAction::triggered, menu, [this]() { handlePreferences(); });

  QObject::connect(menu, &QMenu::aboutToShow, menu, [this]() { rebuildMenu(); });
 }

 ArtifactActiveContextService* ArtifactEditMenu::Impl::getActiveContext() {
  return ArtifactApplicationManager::instance()->activeContextService();
 }

 void ArtifactEditMenu::Impl::handleUndo() {
  if (auto mgr = UndoManager::instance()) {
   const QString desc = mgr->undoDescription();
   mgr->undo();
   qDebug().noquote() << "[Undo]" << desc;
   if (auto* sb = parentWidget_ ? parentWidget_->findChild<QStatusBar*>() : nullptr) {
    sb->showMessage(QString("Undo: %1").arg(desc), 3000);
   }
   // UI 状態を同期
   syncUIState();
  }
 }
 void ArtifactEditMenu::Impl::handleRedo() {
  if (auto mgr = UndoManager::instance()) {
   const QString desc = mgr->redoDescription();
   mgr->redo();
   qDebug().noquote() << "[Redo]" << desc;
   if (auto* sb = parentWidget_ ? parentWidget_->findChild<QStatusBar*>() : nullptr) {
    sb->showMessage(QString("Redo: %1").arg(desc), 3000);
   }
   // UI 状態を同期
   syncUIState();
  }
 }
 
 void ArtifactEditMenu::Impl::syncUIState() {
  // Undo/Redo 後に UI 状態を同期
  if (auto* svc = ArtifactProjectService::instance()) {
    auto currentComp = svc->currentComposition();
    if (auto comp = currentComp.lock()) {
     emit svc->currentCompositionChanged(comp->id());
    }
  }
  if (auto* selMgr = ArtifactApplicationManager::instance()
                          ? ArtifactApplicationManager::instance()->layerSelectionManager()
                          : nullptr) {
   emit selMgr->activeCompositionChanged(selMgr->activeComposition());
  }
 }
 void ArtifactEditMenu::Impl::handleCopyAction()
 {
  auto* selMgr = ArtifactApplicationManager::instance()
                     ? ArtifactApplicationManager::instance()->layerSelectionManager()
                     : nullptr;
  if (!selMgr) return;

  const auto selected = selMgr->selectedLayers();
  if (selected.isEmpty()) return;

  // Serialize selected layers to JSON
  QJsonArray layersArray;
  for (const auto& layer : selected) {
   if (layer) {
    layersArray.append(layer->toJson());
   }
  }

  QJsonObject clipObj;
  clipObj["artifact_clipboard"] = QStringLiteral("layer");
  clipObj["layers"] = layersArray;

  QClipboard* clipboard = QApplication::clipboard();
  clipboard->setText(QJsonDocument(clipObj).toJson(QJsonDocument::Compact));
  qDebug() << "[Edit] Copied" << selected.size() << "layer(s)";
 }

 void ArtifactEditMenu::Impl::handleCutAction()
 {
  handleCopyAction();
  handleDelete();
 }

 void ArtifactEditMenu::Impl::handlePasteAction()
 {
  QClipboard* clipboard = QApplication::clipboard();
  const QString text = clipboard->text();
  if (text.isEmpty()) return;

  QJsonDocument doc = QJsonDocument::fromJson(text.toUtf8());
  if (!doc.isObject()) return;

  QJsonObject clipObj = doc.object();
  if (clipObj["artifact_clipboard"].toString() != QStringLiteral("layer")) {
   qWarning() << "[Edit] Paste: clipboard does not contain artifact layer data";
   return;
  }

  QJsonArray layersArray = clipObj["layers"].toArray();
  if (layersArray.isEmpty()) return;

  auto* svc = ArtifactProjectService::instance();
  if (!svc) return;

  auto comp = svc->currentComposition().lock();
  if (!comp) return;

  auto* selMgr = ArtifactApplicationManager::instance()
                     ? ArtifactApplicationManager::instance()->layerSelectionManager()
                     : nullptr;
  ArtifactAbstractLayerPtr anchorLayer;
  int anchorIndex = -1;
  if (selMgr) {
   anchorLayer = selMgr->currentLayer();
   if (anchorLayer) {
    const auto layers = comp->allLayer();
    for (int i = 0; i < layers.size(); ++i) {
     if (layers[i] && layers[i]->id() == anchorLayer->id()) {
      anchorIndex = i;
      break;
     }
    }
   }
  }

  if (selMgr) selMgr->clearSelection();
  int pasted = 0;

  for (const auto& val : layersArray) {
   if (!val.isObject()) continue;
   auto layer = ArtifactAbstractLayer::fromJson(val.toObject());
   if (!layer) continue;

   layer->setLayerName(layer->layerName() + " (Copy)");

   auto result = comp->appendLayerTop(layer);
   if (result.success) {
    if (anchorIndex >= 0) {
     const auto layers = comp->allLayer();
     int pastedIndex = -1;
     for (int i = 0; i < layers.size(); ++i) {
      if (layers[i] && layers[i]->id() == layer->id()) {
       pastedIndex = i;
       break;
      }
     }
     const int targetIndex = std::clamp(
         anchorIndex + pasted, 0, std::max(0, static_cast<int>(layers.size()) - 1));
     if (pastedIndex >= 0 && pastedIndex != targetIndex) {
      comp->moveLayerToIndex(layer->id(), targetIndex);
     }
    }
    if (selMgr) selMgr->addToSelection(layer);
    ++pasted;
   }
  }

  if (pasted > 0) {
   qDebug().noquote() << "[Edit] Pasted" << pasted << "layer(s)";
   if (auto* sb = parentWidget_ ? parentWidget_->findChild<QStatusBar*>() : nullptr) {
    sb->showMessage(QString("Pasted %1 layer(s)").arg(pasted), 3000);
   }
  }
 }
 void ArtifactEditMenu::Impl::handleDelete()
 {
  auto* selMgr = ArtifactApplicationManager::instance()
                     ? ArtifactApplicationManager::instance()->layerSelectionManager()
                     : nullptr;
  auto* svc = ArtifactProjectService::instance();
  if (!selMgr || !svc) return;

  const auto selected = selMgr->selectedLayers();
  if (selected.isEmpty()) return;

  auto comp = svc->currentComposition().lock();
  if (!comp) return;

  for (const auto& layer : selected) {
   if (layer) {
    svc->removeLayerFromComposition(comp->id(), layer->id());
   }
  }
  selMgr->clearSelection();
 }

 void ArtifactEditMenu::Impl::handleDuplicate()
 {
  auto* selMgr = ArtifactApplicationManager::instance()
                     ? ArtifactApplicationManager::instance()->layerSelectionManager()
                     : nullptr;
  auto* svc = ArtifactProjectService::instance();
  if (!selMgr || !svc) return;

  const auto selected = selMgr->selectedLayers();
  if (selected.isEmpty()) return;

  selMgr->clearSelection();
  for (const auto& layer : selected) {
   if (layer) {
    svc->duplicateLayerInCurrentComposition(layer->id());
   }
  }
 }
 void ArtifactEditMenu::Impl::handleSplit() { 
  if (auto* ctx = getActiveContext()) {
   ctx->splitLayerAtCurrentTime();
  }
 }
 void ArtifactEditMenu::Impl::handleTrimIn() { 
  if (auto* ctx = getActiveContext()) {
   ctx->trimLayerInAtCurrentTime();
  }
 }
 void ArtifactEditMenu::Impl::handleTrimOut() { 
  if (auto* ctx = getActiveContext()) {
   ctx->trimLayerOutAtCurrentTime();
  }
 }
 void ArtifactEditMenu::Impl::handleSelectAll() {
  auto* selMgr = ArtifactApplicationManager::instance()
                     ? ArtifactApplicationManager::instance()->layerSelectionManager()
                     : nullptr;
  auto* svc = ArtifactProjectService::instance();
  if (!selMgr || !svc) return;

  auto comp = svc->currentComposition().lock();
  if (!comp) return;

  selMgr->clearSelection();
  for (const auto& layer : comp->allLayer()) {
   if (layer) {
    selMgr->addToSelection(layer);
   }
  }
  qDebug() << "[Edit] Selected all" << comp->allLayer().size() << "layers";
 }

 void ArtifactEditMenu::Impl::handleSelectNone() {
  auto* selMgr = ArtifactApplicationManager::instance()
                     ? ArtifactApplicationManager::instance()->layerSelectionManager()
                     : nullptr;
  if (!selMgr) return;

  selMgr->clearSelection();
  qDebug() << "[Edit] Selection cleared";
 }

 void ArtifactEditMenu::Impl::handleInvertSelection() {
  auto* selMgr = ArtifactApplicationManager::instance()
                     ? ArtifactApplicationManager::instance()->layerSelectionManager()
                     : nullptr;
  auto* svc = ArtifactProjectService::instance();
  if (!selMgr || !svc) return;

  auto comp = svc->currentComposition().lock();
  if (!comp) return;

  const auto currentSelection = selMgr->selectedLayers();
  QSet<ArtifactAbstractLayerPtr> selectedSet(currentSelection.begin(), currentSelection.end());

  selMgr->clearSelection();
  for (const auto& layer : comp->allLayer()) {
   if (layer && !selectedSet.contains(layer)) {
    selMgr->addToSelection(layer);
   }
  }
  qDebug() << "[Edit] Inverted selection:" << selMgr->selectedLayers().size() << "layers";
 }

 void ArtifactEditMenu::Impl::handleSelectSameType() {
  auto* selMgr = ArtifactApplicationManager::instance()
                     ? ArtifactApplicationManager::instance()->layerSelectionManager()
                     : nullptr;
  auto* svc = ArtifactProjectService::instance();
  if (!selMgr || !svc) return;

  auto comp = svc->currentComposition().lock();
  if (!comp) return;

  const auto currentSelection = selMgr->selectedLayers();
  if (currentSelection.isEmpty()) return;

  // Get the type of the first selected layer
  auto targetType = (*currentSelection.constBegin())->type_index();

  selMgr->clearSelection();
  for (const auto& layer : comp->allLayer()) {
   if (layer && layer->type_index() == targetType) {
    selMgr->addToSelection(layer);
   }
  }
  qDebug() << "[Edit] Selected same type:" << selMgr->selectedLayers().size() << "layers";
 }

 void ArtifactEditMenu::Impl::handleFind() {
  if (!parentWidget_) return;

  bool ok = false;
  const QString searchText = QInputDialog::getText(parentWidget_, "検索",
   "レイヤー名の一部を入力:", QLineEdit::Normal, QString(), &ok);
  if (!ok || searchText.trimmed().isEmpty()) return;

  auto* svc = ArtifactProjectService::instance();
  auto* selMgr = ArtifactApplicationManager::instance()
                     ? ArtifactApplicationManager::instance()->layerSelectionManager()
                     : nullptr;
  if (!svc || !selMgr) return;

  auto comp = svc->currentComposition().lock();
  if (!comp) return;

  selMgr->clearSelection();
  int foundCount = 0;
  const QString lowerSearch = searchText.toLower();

  for (const auto& layer : comp->allLayer()) {
   if (layer && layer->layerName().toLower().contains(lowerSearch)) {
    selMgr->addToSelection(layer);
    foundCount++;
   }
  }

  if (auto* sb = parentWidget_ ? parentWidget_->findChild<QStatusBar*>() : nullptr) {
   sb->showMessage(QString("Found %1 layer(s) matching \"%2\"").arg(foundCount).arg(searchText), 3000);
  }
  qDebug() << "[Edit] Found" << foundCount << "layers matching" << searchText;
 }
 void ArtifactEditMenu::Impl::handlePreferences() { 
  auto* dialog = new ApplicationSettingDialog(parentWidget_);
  dialog->setAttribute(Qt::WA_DeleteOnClose);
  dialog->show();
 }
void ArtifactEditMenu::Impl::rebuildMenu() { 
  bool hasProject = ArtifactProjectManager::getInstance().isProjectCreated();
  auto mgr = UndoManager::instance();
  auto& shortcuts = ShortcutBindings::instance();
  
  undoAction->setShortcut(shortcuts.shortcut(ShortcutId::Undo));
  redoAction->setShortcut(shortcuts.shortcut(ShortcutId::Redo));
  undoAction->setEnabled(hasProject && mgr && mgr->canUndo());
  redoAction->setEnabled(hasProject && mgr && mgr->canRedo());
  
  if (mgr && mgr->canUndo()) {
   undoAction->setText(QString("元に戻す: %1 (&U)").arg(mgr->undoDescription()));
  } else {
   undoAction->setText("元に戻す (&U)");
  }

  if (mgr && mgr->canRedo()) {
   redoAction->setText(QString("やり直し: %1 (&R)").arg(mgr->redoDescription()));
  } else {
   redoAction->setText("やり直し (&R)");
  }

  bool hasSelection = false;
  if (auto* sel = ArtifactApplicationManager::instance()->layerSelectionManager()) {
   hasSelection = !sel->selectedLayers().isEmpty();
  }

  copyAction_->setEnabled(hasProject && hasSelection);
  cutAction_->setEnabled(hasProject && hasSelection);
  pasteAction_->setEnabled(hasProject);
  deleteAction_->setEnabled(hasProject && hasSelection);
  duplicateAction->setEnabled(hasProject && hasSelection);
  splitAction->setEnabled(hasProject && hasSelection);
  trimInAction->setEnabled(hasProject && hasSelection);
  trimOutAction->setEnabled(hasProject && hasSelection);
   selectAllAction->setEnabled(hasProject);
   selectNoneAction->setEnabled(hasProject && hasSelection);
   invertSelectionAction->setEnabled(hasProject && hasSelection);
   selectSameTypeAction->setEnabled(hasProject && hasSelection);
   findAction->setEnabled(hasProject);
 }

 W_OBJECT_IMPL(ArtifactEditMenu)

 ArtifactEditMenu::ArtifactEditMenu(QWidget* mainWindow, QWidget* parent) : QMenu(parent), impl_(new Impl(this, mainWindow)) {
  setTitle("編集 (&E)");
 }

 ArtifactEditMenu::~ArtifactEditMenu() { delete impl_; }

 void ArtifactEditMenu::rebuildMenu() { impl_->rebuildMenu(); }

} // namespace Artifact
