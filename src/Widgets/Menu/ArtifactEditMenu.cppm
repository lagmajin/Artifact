module;
#include <utility>
#include <QMenu>
#include <wobjectimpl.h>
#include <QAction>
#include <QKeySequence>
#include <QDebug>
#include <QStatusBar>
#include <QJsonArray>
#include <QDialog>
#include <QDialogButtonBox>
#include <QVBoxLayout>
#include <QInputDialog>
#include <QLineEdit>
module Artifact.Menu.Edit;
import std;

import Event.Bus;
import Artifact.Event.Types;
import Artifact.Application.Manager;
import Artifact.Project.Manager;
import Artifact.Service.ActiveContext;
import Artifact.Service.Application;
import Artifact.Layers.Selection.Manager;
import Artifact.Layer.Abstract;
import Artifact.Composition.Abstract;
import Clipboard.ClipboardManager;
import Artifact.Widgets.UndoHistoryWidget;
import Undo.UndoManager;
import UI.ShortcutBindings;
import ApplicationSettingDialog;
import Artifact.Service.Project;
import Artifact.Service.Playback;
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
  QAction* undoHistoryAction = nullptr;

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
  void handleUndoHistory();

  ArtifactActiveContextService* getActiveContext();
  std::vector<ArtifactCore::EventBus::Subscription> eventBusSubscriptions_;
 };

 ArtifactEditMenu::Impl::Impl(QMenu* menu, QWidget* mainWindow)
 {
  parentWidget_ = mainWindow ? mainWindow : menu;
  auto& shortcuts = ShortcutBindings::instance();
  // Basic edit actions
  undoAction = new QAction("元に戻す (&U)");
  undoAction->setShortcut(shortcuts.shortcut(ShortcutId::Undo));
  undoAction->setIcon(QIcon(resolveIconPath("Studio/editmenu_undo.svg")));

  redoAction = new QAction("やり直し (&R)");
  redoAction->setShortcut(shortcuts.shortcut(ShortcutId::Redo));
  redoAction->setIcon(QIcon(resolveIconPath("Studio/editmenu_redo.svg")));

  duplicateAction = new QAction("複製 (&D)");
  duplicateAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_D));
  duplicateAction->setIcon(QIcon(resolveIconPath("Studio/editmenu_duplicate.svg")));

  splitAction = new QAction("レイヤーを分割 (&S)");
  splitAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_D));
  splitAction->setIcon(QIcon(resolveIconPath("Studio/editmenu_split.svg")));

  trimInAction = new QAction("インポイントを現在の時間にトリム");
  trimInAction->setIcon(QIcon(resolveIconPath("Studio/editmenu_trim_in.svg")));
  trimOutAction = new QAction("アウトポイントを現在の時間にトリム");
  trimOutAction->setIcon(QIcon(resolveIconPath("Studio/editmenu_trim_out.svg")));

  selectAllAction = new QAction("すべて選択 (&A)");
  selectAllAction->setShortcut(QKeySequence::SelectAll);
  selectAllAction->setIcon(QIcon(resolveIconPath("Studio/editmenu_select_all.svg")));

  selectNoneAction = new QAction("選択解除");
  selectNoneAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_A));
  selectNoneAction->setIcon(QIcon(resolveIconPath("Studio/editmenu_select_none.svg")));

  invertSelectionAction = new QAction("選択を反転");
  invertSelectionAction->setIcon(QIcon(resolveIconPath("Studio/editmenu_select_invert.svg")));

  selectSameTypeAction = new QAction("同じ種類を選択");
  selectSameTypeAction->setIcon(QIcon(resolveIconPath("Studio/editmenu_select_same_type.svg")));

  findAction = new QAction("検索 (&F)...");
  findAction->setShortcut(QKeySequence::Find);
  findAction->setIcon(QIcon(resolveIconPath("Studio/editmenu_find.svg")));

  preferencesAction = new QAction("環境設定 (&P)...");
  preferencesAction->setIcon(QIcon(resolveIconPath("Studio/editmenu_preferences.svg")));

  undoHistoryAction = new QAction("Undo 履歴 (&H)...");
  undoHistoryAction->setIcon(QIcon(resolveIconPath("Studio/editmenu_history.svg")));

  copyAction_ = new QAction("コピー (&C)");
  copyAction_->setShortcut(QKeySequence::Copy);
  copyAction_->setIcon(QIcon(resolveIconPath("Studio/editmenu_copy.svg")));

  cutAction_ = new QAction("切り取り (&T)");
  cutAction_->setShortcut(QKeySequence::Cut);
  cutAction_->setIcon(QIcon(resolveIconPath("Studio/editmenu_cut.svg")));

  pasteAction_ = new QAction("貼り付け (&P)");
  pasteAction_->setShortcut(QKeySequence::Paste);
  pasteAction_->setIcon(QIcon(resolveIconPath("Studio/editmenu_paste.svg")));

  deleteAction_ = new QAction("削除 (&D)");
  deleteAction_->setShortcut(QKeySequence::Delete);
  deleteAction_->setIcon(QIcon(resolveIconPath("Studio/editmenu_delete.svg")));

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
  selectMenu->setIcon(QIcon(resolveIconPath("Studio/editmenu_select_all.svg")));
  selectMenu->addAction(selectAllAction);
  selectMenu->addAction(selectNoneAction);
  selectMenu->addAction(invertSelectionAction);
  selectMenu->addSeparator();
  selectMenu->addAction(selectSameTypeAction);

  menu->addSeparator();
  menu->addAction(findAction);
  menu->addSeparator();
  menu->addAction(undoHistoryAction);
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
  QObject::connect(undoHistoryAction, &QAction::triggered, menu, [this]() { handleUndoHistory(); });
  QObject::connect(preferencesAction, &QAction::triggered, menu, [this]() { handlePreferences(); });

  auto& eventBus = ArtifactCore::globalEventBus();
  eventBusSubscriptions_.push_back(
      eventBus.subscribe<LayerSelectionChangedEvent>(
          [this](const LayerSelectionChangedEvent&) { rebuildMenu(); }));
  eventBusSubscriptions_.push_back(
      eventBus.subscribe<CurrentCompositionChangedEvent>(
          [this](const CurrentCompositionChangedEvent&) { rebuildMenu(); }));
  eventBusSubscriptions_.push_back(
      eventBus.subscribe<ProjectChangedEvent>(
          [this](const ProjectChangedEvent&) { rebuildMenu(); }));

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
  if (auto* selMgr = ArtifactApplicationManager::instance()
                          ? ArtifactApplicationManager::instance()->layerSelectionManager()
                          : nullptr) {
   emit selMgr->selectionChanged();
   emit selMgr->activeCompositionChanged();
  }
  rebuildMenu();
 }
 void ArtifactEditMenu::Impl::handleCopyAction()
 {
  auto* selMgr = ArtifactApplicationManager::instance()
                     ? ArtifactApplicationManager::instance()->layerSelectionManager()
                     : nullptr;
  if (!selMgr) return;

  const auto selected = selMgr->selectedLayers();
  if (selected.isEmpty()) return;

  QJsonArray layersArray;
  QString firstLayerName;
  for (const auto& layer : selected) {
   if (layer) {
    layersArray.append(layer->toJson());
    if (firstLayerName.isEmpty()) {
     firstLayerName = layer->layerName();
    }
   }
  }

  ArtifactCore::ClipboardManager::instance().copyLayers(layersArray,
                                                        layersArray.size());
  qDebug() << "[Edit] Copied" << selected.size() << "layer(s)";

  // Publish event for clip buffer panel
  qint64 currentFrame = 0;
  if (auto* ps = ArtifactPlaybackService::instance()) {
   currentFrame = ps->currentFrame().framePosition();
  }
  ArtifactCore::globalEventBus().publish<ClipCopiedEvent>(
      ClipCopiedEvent{QString(), QString(), firstLayerName, currentFrame, QVariant(layersArray)});

  rebuildMenu();
 }

 void ArtifactEditMenu::Impl::handleCutAction()
 {
  handleCopyAction();
  handleDelete();

  // Publish cut event for clip buffer panel
  qint64 currentFrame = 0;
  QString compId;
  if (auto* ps = ArtifactPlaybackService::instance()) {
   currentFrame = ps->currentFrame().framePosition();
  }
  if (auto* svc2 = ArtifactProjectService::instance()) {
   if (auto comp = svc2->currentComposition().lock()) {
    compId = comp->id().toString();
   }
  }
  ArtifactCore::globalEventBus().publish<ClipCutEvent>(
      ClipCutEvent{compId, QString(), QString(), currentFrame, QVariant()});
 }

 void ArtifactEditMenu::Impl::handlePasteAction()
 {
  ArtifactCore::ClipboardManager::instance().syncFromSystemClipboard();
  if (!ArtifactCore::ClipboardManager::instance().hasLayerData()) {
   qWarning() << "[Edit] Paste: clipboard does not contain artifact layer data";
   return;
  }

  const QJsonArray layersArray = ArtifactCore::ClipboardManager::instance().pasteLayers();
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
   rebuildMenu();
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
  rebuildMenu();
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
  rebuildMenu();
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
  rebuildMenu();
 }

 void ArtifactEditMenu::Impl::handleSelectNone() {
  auto* selMgr = ArtifactApplicationManager::instance()
                     ? ArtifactApplicationManager::instance()->layerSelectionManager()
                     : nullptr;
  if (!selMgr) return;

  selMgr->clearSelection();
  qDebug() << "[Edit] Selection cleared";
  rebuildMenu();
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
  rebuildMenu();
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
  rebuildMenu();
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
  rebuildMenu();
 }
 void ArtifactEditMenu::Impl::handlePreferences() { 
  auto* dialog = new ApplicationSettingDialog(parentWidget_);
  dialog->setAttribute(Qt::WA_DeleteOnClose);
  dialog->show();
 }

 void ArtifactEditMenu::Impl::handleUndoHistory() {
  auto* dialog = new QDialog(parentWidget_);
  dialog->setAttribute(Qt::WA_DeleteOnClose);
  dialog->setWindowTitle(QStringLiteral("Undo History"));
  dialog->resize(860, 520);

  auto* layout = new QVBoxLayout(dialog);
  layout->setContentsMargins(8, 8, 8, 8);
  layout->setSpacing(8);

  auto* historyWidget = new ArtifactUndoHistoryWidget(dialog);
  layout->addWidget(historyWidget, 1);

  auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, Qt::Horizontal, dialog);
  QObject::connect(buttons, &QDialogButtonBox::rejected, dialog, &QDialog::close);
  layout->addWidget(buttons);

  dialog->show();
 }
void ArtifactEditMenu::Impl::rebuildMenu() { 
  bool hasProject = ArtifactProjectManager::getInstance().isProjectCreated();
  auto mgr = UndoManager::instance();
  auto& shortcuts = ShortcutBindings::instance();
  auto& clipboard = ArtifactCore::ClipboardManager::instance();
  clipboard.syncFromSystemClipboard();
  
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
  bool hasComposition = false;
  if (auto* sel = ArtifactApplicationManager::instance()->layerSelectionManager()) {
   hasComposition = sel->activeComposition() != ArtifactCompositionPtr{};
   hasSelection = hasComposition && !sel->selectedLayers().isEmpty();
  }

  const bool layerClipboardReady = clipboard.hasLayerData();
  if (layerClipboardReady) {
   pasteAction_->setText(QString("貼り付け (&P): %1").arg(clipboard.description()));
  } else {
   pasteAction_->setText("貼り付け (&P)");
  }
  copyAction_->setEnabled(hasProject && hasSelection);
  cutAction_->setEnabled(hasProject && hasSelection);
  pasteAction_->setEnabled(hasProject && hasComposition && layerClipboardReady);
  deleteAction_->setEnabled(hasProject && hasSelection);
  duplicateAction->setEnabled(hasProject && hasSelection);
  splitAction->setEnabled(hasProject && hasSelection);
  trimInAction->setEnabled(hasProject && hasSelection);
  trimOutAction->setEnabled(hasProject && hasSelection);
   selectAllAction->setEnabled(hasProject && hasComposition);
   selectNoneAction->setEnabled(hasProject && hasSelection);
   invertSelectionAction->setEnabled(hasProject && hasSelection);
   selectSameTypeAction->setEnabled(hasProject && hasSelection);
   findAction->setEnabled(hasProject && hasComposition);
 }

 W_OBJECT_IMPL(ArtifactEditMenu)

 ArtifactEditMenu::ArtifactEditMenu(QWidget* mainWindow, QWidget* parent) : QMenu(parent), impl_(new Impl(this, mainWindow)) {
  setTitle("編集 (&E)");
  setIcon(QIcon(resolveIconPath("Studio/menubar_edit.svg")));
 }

 ArtifactEditMenu::~ArtifactEditMenu() { delete impl_; }

 void ArtifactEditMenu::rebuildMenu() { impl_->rebuildMenu(); }

} // namespace Artifact
