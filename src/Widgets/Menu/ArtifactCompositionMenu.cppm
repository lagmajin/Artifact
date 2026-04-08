module;
#include <utility>
#include <QMenu>
#include <QAction>
#include <QColorDialog>
#include <QDebug>
#include <QInputDialog>
#include <QLineEdit>
#include <QMessageBox>
#include <QDateTime>
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
 QAction* milestoneDummyAction = nullptr;

 void showCreate();
 void createFromPreset(const ArtifactCompositionInitParams& params);
 void duplicateCurrent();
 void renameCurrent();
 void removeCurrent();
 void showSettings();
 void showColor();
 void runMilestoneDummyPipeline();
 void startSoftwareTestPipeline(QWidget* parent);
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
 milestoneDummyAction = new QAction("マイルストーン: Software制作パスを初期化(&M)...");

 menu->addAction(createAction);
 menu->addMenu(presetMenu);
 menu->addSeparator();
 menu->addAction(duplicateAction);
 menu->addAction(renameAction);
 menu->addAction(deleteAction);
 menu->addSeparator();
 menu->addAction(colorAction);
 menu->addSeparator();
 menu->addAction(milestoneDummyAction);

 QObject::connect(createAction, &QAction::triggered, menu, [this]() { showCreate(); });
 QObject::connect(presetHdAction, &QAction::triggered, menu, [this]() { createFromPreset(ArtifactCompositionInitParams::hdPreset()); });
 QObject::connect(preset4kAction, &QAction::triggered, menu, [this]() { createFromPreset(ArtifactCompositionInitParams::fourKPreset()); });
 QObject::connect(presetVerticalAction, &QAction::triggered, menu, [this]() { createFromPreset(ArtifactCompositionInitParams::verticalPreset()); });
 QObject::connect(duplicateAction, &QAction::triggered, menu, [this]() { duplicateCurrent(); });
 QObject::connect(renameAction, &QAction::triggered, menu, [this]() { renameCurrent(); });
 QObject::connect(deleteAction, &QAction::triggered, menu, [this]() { removeCurrent(); });
 QObject::connect(colorAction, &QAction::triggered, menu, [this]() { showColor(); });
 QObject::connect(milestoneDummyAction, &QAction::triggered, menu, [this]() { runMilestoneDummyPipeline(); });
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
  QColor color = QColorDialog::getColor(Qt::black, mainWindow_, "Background Color");
  if (color.isValid()) {
   comp->setBackGroundColor(FloatColor(color.redF(), color.greenF(), color.blueF(), color.alphaF()));
  }
 }
}

void ArtifactCompositionMenu::Impl::runMilestoneDummyPipeline()
{
 startSoftwareTestPipeline(mainWindow_);
}

void ArtifactCompositionMenu::Impl::startSoftwareTestPipeline(QWidget* parent)
{
 auto* projectService = ArtifactProjectService::instance();
 if (!projectService) {
  QMessageBox::warning(parent ? parent : menu_, "Software Test", "ProjectService が利用できません。");
  return;
 }

 // 1. コンポジション作成
 ArtifactCompositionInitParams params = ArtifactCompositionInitParams::hdPreset();
 const QString stamp = QDateTime::currentDateTime().toString(QStringLiteral("MMdd_HHmm"));
 params.setCompositionName(UniString(QStringLiteral("SoftwarePipeline_%1").arg(stamp)));

 projectService->createComposition(params);
 auto currentComp = projectService->currentComposition().lock();
 if (!currentComp) {
  QMessageBox::warning(parent ? parent : menu_, "Software Test", "コンポジション作成に失敗しました。");
  return;
 }
 const int beforeLayerCount = currentComp->allLayer().size();

 // 2. 平面レイヤー追加
 ArtifactSolidLayerInitParams solidParams(QStringLiteral("Solid 1"));
 solidParams.setWidth(params.width());
 solidParams.setHeight(params.height());
 solidParams.setColor(FloatColor(0.22f, 0.52f, 0.88f, 1.0f));

 projectService->addLayerToCurrentComposition(solidParams);
 currentComp = projectService->currentComposition().lock();
 if (!currentComp || currentComp->allLayer().size() <= beforeLayerCount) {
  QMessageBox::warning(parent ? parent : menu_, "Software Test", "平面レイヤー追加に失敗しました。");
  return;
 }

 // 3. Software Composition Test を起動
 auto* preview = new ArtifactSoftwareCompositionTestWidget();
 preview->setAttribute(Qt::WA_DeleteOnClose, true);
 preview->resize(1100, 760);
 preview->show();
 preview->raise();
 preview->activateWindow();

 QMessageBox::information(
  parent ? parent : menu_,
  "Software Test",
  QStringLiteral("Software Test Pipeline を初期化しました。\n\n"
   "1) コンポジション作成\n"
   "2) 平面レイヤー追加\n"
   "3) Software Composition Test 起動\n\n"
   "このウィンドウを閉じて、Test メニューからいつでも再起動できます。"));
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
 impl_->milestoneDummyAction->setEnabled(service != nullptr);
}

void ArtifactCompositionMenu::handleCreateCompositionRequested()
{
 if (impl_) impl_->showCreate();
}

}
