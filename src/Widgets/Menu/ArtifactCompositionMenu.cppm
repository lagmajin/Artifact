module;
#include <QMenu>
#include <QAction>
#include <QColorDialog>
#include <QDebug>
#include <QInputDialog>
#include <QLineEdit>
#include <QMessageBox>
#include <QDateTime>
#include <wobjectimpl.h>

module Menu.Composition;
import std;

import Artifact.Service.Project;
import Artifact.Composition.InitParams;
import Artifact.Layer.InitParams;
import Artifact.Widgets.SoftwareRenderInspectors;
import Dialog.Composition;

namespace Artifact {

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
 QAction* settingsAction = nullptr;
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
};

ArtifactCompositionMenu::Impl::Impl(ArtifactCompositionMenu* menu, QWidget* mainWindow)
 : menu_(menu), mainWindow_(mainWindow)
{
 createAction = new QAction("新規コンポジション(&N)...");
 createAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_N));

 presetMenu = new QMenu("プリセットから作成(&P)", menu);
 presetHdAction = presetMenu->addAction("HD 1080p 30fps");
 preset4kAction = presetMenu->addAction("4K UHD 30fps");
 presetVerticalAction = presetMenu->addAction("Vertical 1080x1920 30fps");

 duplicateAction = new QAction("コンポジションを複製(&D)");
 renameAction = new QAction("名前を変更(&R)...");
 deleteAction = new QAction("コンポジションを削除(&X)...");

 settingsAction = new QAction("レンダー出力設定(&S)...");
 settingsAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_K));

 colorAction = new QAction("背景色(&B)...");
 colorAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_B));
 milestoneDummyAction = new QAction("マイルストーン: Software制作パスを初期化(&M)...");

 menu->addAction(createAction);
 menu->addMenu(presetMenu);
 menu->addSeparator();
 menu->addAction(duplicateAction);
 menu->addAction(renameAction);
 menu->addAction(deleteAction);
 menu->addSeparator();
 menu->addAction(settingsAction);
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
 QObject::connect(settingsAction, &QAction::triggered, menu, [this]() { showSettings(); });
 QObject::connect(colorAction, &QAction::triggered, menu, [this]() { showColor(); });
 QObject::connect(milestoneDummyAction, &QAction::triggered, menu, [this]() { runMilestoneDummyPipeline(); });
}

void ArtifactCompositionMenu::Impl::showCreate()
{
 auto dialog = new CreateCompositionDialog(mainWindow_);
 if (dialog->exec()) {
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
  "Composition",
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

 const auto answer = QMessageBox::question(
  mainWindow_ ? mainWindow_ : menu_,
  "コンポジション削除",
  message,
  QMessageBox::Yes | QMessageBox::No,
  QMessageBox::No);
 if (answer != QMessageBox::Yes) {
  return;
 }

 if (!service->removeCompositionWithRenderQueueCleanup(current->id())) {
  QMessageBox::warning(mainWindow_ ? mainWindow_ : menu_,
   "Composition",
   "コンポジションの削除に失敗しました。");
 }
}

void ArtifactCompositionMenu::Impl::showSettings()
{
 QMessageBox::information(
  mainWindow_ ? mainWindow_ : menu_,
  "レンダー出力設定",
  "レンダー出力設定ダイアログはこれから接続します。");
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
 auto* parent = mainWindow_ ? mainWindow_ : menu_;
 auto* projectService = ArtifactProjectService::instance();
 if (!projectService) {
  QMessageBox::warning(parent, "Milestone", "ProjectService が利用できません。");
  return;
 }

 ArtifactCompositionInitParams params = ArtifactCompositionInitParams::hdPreset();
 const QString stamp = QDateTime::currentDateTime().toString(QStringLiteral("MMdd_HHmm"));
 params.setCompositionName(UniString(QStringLiteral("SoftwarePipeline_%1").arg(stamp)));

 projectService->createComposition(params);
 auto currentComp = projectService->currentComposition().lock();
 if (!currentComp) {
  QMessageBox::warning(parent, "Milestone", "コンポジション作成に失敗しました。");
  return;
 }
 const int beforeLayerCount = currentComp->allLayer().size();

 ArtifactSolidLayerInitParams solidParams(QStringLiteral("Solid 1"));
 solidParams.setWidth(params.width());
 solidParams.setHeight(params.height());
 solidParams.setColor(FloatColor(0.22f, 0.52f, 0.88f, 1.0f));

 projectService->addLayerToCurrentComposition(solidParams);
 currentComp = projectService->currentComposition().lock();
 if (!currentComp || currentComp->allLayer().size() <= beforeLayerCount) {
  QMessageBox::warning(parent, "Milestone", "平面レイヤー追加に失敗しました。");
  return;
 }

 auto* preview = new ArtifactSoftwareCompositionTestWidget();
 preview->setAttribute(Qt::WA_DeleteOnClose, true);
 preview->resize(1100, 760);
 preview->show();
 preview->raise();
 preview->activateWindow();

 QMessageBox::information(
  parent,
  "Milestone",
  QStringLiteral("初期化しました。\n1) コンポ作成\n2) 平面追加\n3) Software Composition Test を起動\n\n次は effect と image sequence render を詰めます。"));
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
 impl_->settingsAction->setEnabled(hasComp);
 impl_->colorAction->setEnabled(hasComp);
 impl_->milestoneDummyAction->setEnabled(service != nullptr);
}

void ArtifactCompositionMenu::handleCreateCompositionRequested()
{
 if (impl_) impl_->showCreate();
}

}
