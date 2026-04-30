module;
#include <utility>
#include <QMenu>
#include <QAction>
#include <QDebug>
#include <QInputDialog>
#include <QLineEdit>
#include <QMessageBox>
#include <QTimer>
#include <wobjectimpl.h>

module Menu.Composition;
import std;

import Artifact.Service.Project;
import Utils.Path;
import Artifact.Composition.InitParams;
import Artifact.Layer.InitParams;
import Artifact.Widgets.SoftwareRenderInspectors;
import Dialog.Composition;
import FloatColorPickerDialog;
import Artifact.Widgets.AppDialogs;

namespace Artifact {
using namespace ArtifactCore;

W_OBJECT_IMPL(ArtifactCompositionMenu)

class ArtifactCompositionMenu::Impl {
public:
 Impl(ArtifactCompositionMenu* menu, QWidget* mainWindow);
 ~Impl() = default;

 ArtifactCompositionMenu* menu_ = nullptr;
 QWidget* mainWindow_ = nullptr;
 QAction* createAction = nullptr;
 QMenu* presetMenu = nullptr;
 QAction* presetHdAction = nullptr;
 QAction* preset4kAction = nullptr;
 QAction* presetVerticalAction = nullptr;
 QAction* duplicateAction = nullptr;
 QAction* renameAction = nullptr;
 QAction* deleteAction = nullptr;
 QAction* colorAction = nullptr;

 void showCreate();
 void createFromPreset(const ArtifactCompositionInitParams& params);
 void duplicateCurrent();
 void renameCurrent();
 void removeCurrent();
 void showSettings();
 void showColor();
};

ArtifactCompositionMenu::Impl::Impl(ArtifactCompositionMenu* menu, QWidget* mainWindow)
 : menu_(menu), mainWindow_(mainWindow)
{
 createAction = new QAction("新規コンポジション(&N)...");
 createAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_N));
 createAction->setIcon(QIcon(resolveIconPath("Material/movie_creation.svg")));

 presetMenu = new QMenu("プリセットから作成(&P)", menu);
 presetHdAction = presetMenu->addAction("HD 1080p 30fps");
 preset4kAction = presetMenu->addAction("4K UHD 30fps");
 presetVerticalAction = presetMenu->addAction("Vertical 1080x1920 30fps");

 duplicateAction = new QAction("コンポジションを複製(&D)");
 duplicateAction->setIcon(QIcon(resolveIconPath("Material/content_copy.svg")));
 renameAction = new QAction("名前を変更(&R)...");
 renameAction->setIcon(QIcon(resolveIconPath("Material/edit.svg")));
 deleteAction = new QAction("コンポジションを削除(&X)...");
 deleteAction->setIcon(QIcon(resolveIconPath("Material/delete.svg")));

 colorAction = new QAction("背景色(&B)...");
 colorAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_B));
 colorAction->setIcon(QIcon(resolveIconPath("Material/palette.svg")));

 menu->addAction(createAction);
 menu->addMenu(presetMenu);
 menu->addSeparator();
 menu->addAction(duplicateAction);
 menu->addAction(renameAction);
 menu->addAction(deleteAction);
 menu->addSeparator();
 menu->addAction(colorAction);

 QObject::connect(createAction, &QAction::triggered, menu, [this]() { showCreate(); });
 QObject::connect(presetHdAction, &QAction::triggered, menu, [this]() { createFromPreset(ArtifactCompositionInitParams::hdPreset()); });
 QObject::connect(preset4kAction, &QAction::triggered, menu, [this]() { createFromPreset(ArtifactCompositionInitParams::fourKPreset()); });
 QObject::connect(presetVerticalAction, &QAction::triggered, menu, [this]() { createFromPreset(ArtifactCompositionInitParams::verticalPreset()); });
 QObject::connect(duplicateAction, &QAction::triggered, menu, [this]() { duplicateCurrent(); });
 QObject::connect(renameAction, &QAction::triggered, menu, [this]() { renameCurrent(); });
 QObject::connect(deleteAction, &QAction::triggered, menu, [this]() { removeCurrent(); });
 QObject::connect(colorAction, &QAction::triggered, menu, [this]() { showColor(); });
}

void ArtifactCompositionMenu::Impl::showCreate()
{
 auto dialog = new CreateCompositionDialog(mainWindow_);
 if (dialog->exec()) {
  const ArtifactCompositionInitParams params = dialog->acceptedInitParams();
  QTimer::singleShot(0, mainWindow_ ? mainWindow_ : menu_, [params]() {
   if (auto* service = ArtifactProjectService::instance()) {
    service->createComposition(params);
   }
  });
 }
 dialog->deleteLater();
}

void ArtifactCompositionMenu::Impl::createFromPreset(const ArtifactCompositionInitParams& params)
{
 auto* service = ArtifactProjectService::instance();
 if (!service) {
  QMessageBox::warning(mainWindow_ ? mainWindow_ : menu_,
   "Composition",
   "プロジェクトサービスが利用できません。");
  return;
 }
 service->createComposition(params);
}

void ArtifactCompositionMenu::Impl::duplicateCurrent()
{
 auto* service = ArtifactProjectService::instance();
 auto current = service->currentComposition().lock();
 if (!current) {
  return;
 }

 if (!service->duplicateComposition(current->id())) {
  QMessageBox::warning(mainWindow_ ? mainWindow_ : menu_,
   "Composition",
   "コンポジションの複製に失敗しました。");
 }
}

void ArtifactCompositionMenu::Impl::renameCurrent()
{
 auto* service = ArtifactProjectService::instance();
 auto current = service->currentComposition().lock();
 if (!current) {
  return;
 }

 bool ok = false;
 const QString newName = QInputDialog::getText(
  mainWindow_ ? mainWindow_ : menu_,
  "コンポジション名の変更",
  "新しい名前:",
  QLineEdit::Normal,
  current->settings().compositionName().toQString().trimmed(),
  &ok);
 if (!ok) {
  return;
 }
 const QString trimmed = newName.trimmed();
 if (trimmed.isEmpty()) {
  return;
 }
 if (!service->renameComposition(current->id(), UniString(trimmed))) {
  QMessageBox::warning(mainWindow_ ? mainWindow_ : menu_,
   "Composition",
   "コンポジション名の変更に失敗しました。");
 }
}

void ArtifactCompositionMenu::Impl::removeCurrent()
{
 auto* service = ArtifactProjectService::instance();
 auto current = service->currentComposition().lock();
 if (!current) {
  return;
 }

 const QString message = service->compositionRemovalConfirmationMessage(current->id());

 if (!ArtifactMessageBox::confirmDelete(mainWindow_ ? mainWindow_ : menu_, "コンポジション削除", message)) {
  return;
 }

 if (!service->removeCompositionWithRenderQueueCleanup(current->id())) {
  QMessageBox::warning(mainWindow_ ? mainWindow_ : menu_,
   "Composition",
   "コンポジションの削除に失敗しました。");
 }
}

void ArtifactCompositionMenu::Impl::showColor()
{
 auto service = ArtifactProjectService::instance();
 if (auto comp = service->currentComposition().lock()) {
  ArtifactWidgets::FloatColorPicker picker(mainWindow_ ? mainWindow_ : menu_);
  picker.setWindowTitle(QStringLiteral("Background Color"));
  picker.setInitialColor(FloatColor(comp->backgroundColor().r(),
                                    comp->backgroundColor().g(),
                                    comp->backgroundColor().b(),
                                    comp->backgroundColor().a()));
  if (picker.exec() == QDialog::Accepted) {
   const FloatColor picked = picker.getColor();
   comp->setBackGroundColor(
       FloatColor(picked.r(), picked.g(), picked.b(), picked.a()));
  }
 }
}

ArtifactCompositionMenu::ArtifactCompositionMenu(QWidget* mainWindow, QWidget* parent)
 : QMenu(parent), impl_(new Impl(this, mainWindow))
{
 setTitle("コンポジション(&C)");
 connect(this, &QMenu::aboutToShow, this, &ArtifactCompositionMenu::rebuildMenu);
}

ArtifactCompositionMenu::ArtifactCompositionMenu(QWidget* parent)
 : QMenu(parent), impl_(new Impl(this, nullptr))
{
 setTitle("コンポジション(&C)");
 connect(this, &QMenu::aboutToShow, this, &ArtifactCompositionMenu::rebuildMenu);
}

ArtifactCompositionMenu::~ArtifactCompositionMenu()
{
 delete impl_;
}

void ArtifactCompositionMenu::rebuildMenu()
{
 if (!impl_) return;
 auto service = ArtifactProjectService::instance();
 const bool hasComp = service && !service->currentComposition().expired();
 impl_->createAction->setEnabled(service != nullptr);
 if (impl_->presetMenu) {
  impl_->presetMenu->setEnabled(service != nullptr);
 }
 impl_->duplicateAction->setEnabled(hasComp);
 impl_->renameAction->setEnabled(hasComp);
 impl_->deleteAction->setEnabled(hasComp);
 impl_->colorAction->setEnabled(hasComp);
}

void ArtifactCompositionMenu::handleCreateCompositionRequested()
{
 if (impl_) impl_->showCreate();
}

}
