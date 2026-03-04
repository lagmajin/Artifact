module;

#include <wobjectimpl.h>
#include <QMenu>
#include <QWidget>
#include <QDebug>
module Artifact.Menu.Layer;

import std; 
import Artifact.Project.Manager;
import Artifact.Service.Project;
import Artifact.Layer.InitParams;
import Artifact.Layer.Factory;


import Artifact.Widgets.CreateLayerDialog;

namespace Artifact {

 W_OBJECT_IMPL(ArtifactLayerMenu)

 class ArtifactLayerMenu::Impl {
 private:
  ArtifactLayerMenu*	parentPtr_ = nullptr;
 public:
  Impl(ArtifactLayerMenu* menu);
  QMenu* createLayerMenu = nullptr;
  QAction* createNullLayerAction_ = nullptr;
  QAction* createSolidLayerAction_ = nullptr;
  QAction* createAdjustableLayerAction_ = nullptr;
  QAction* createTextLayerAction_ = nullptr;
  QAction* createShapeLayerAction_ = nullptr;
  QAction* createImageLayerAction_ = nullptr;
  QAction* createCameraLayerAction_ = nullptr;
  QAction* createLightLayerAction_ = nullptr;
  QAction* createModel3DLayerAction_ = nullptr;
  
  QAction* layerSettingsAction_ = nullptr;
  QAction* openLayerAction_ = nullptr;
  QAction* openLayerSourceAction_ = nullptr;

  QMenu* maskMenu_ = nullptr;
  QMenu* transformMenu_ = nullptr;
  QAction* resetTransformAction_ = nullptr;
  QAction* centerAnchorPointAction_ = nullptr;
  QAction* autoOrientAction_ = nullptr;

  QMenu* arrangeMenu_ = nullptr;
  QAction* bringToFrontAction_ = nullptr;
  QAction* bringForwardAction_ = nullptr;
  QAction* sendBackwardAction_ = nullptr;
  QAction* sendToBackAction_ = nullptr;
  
  QAction* guideLayerAction_ = nullptr;
  QAction* addMarkerAction_ = nullptr;

  QMenu* layerTimeMenu_ = nullptr;

  void handleProjectOpend();
  void handleProjectClosed();
  void handleCreateNullLayer();
  void handleCreateSolidLayer();
  void handleAdjustableLayer();
  void handleTextLayer();
  void handleShapeLayer();
  void handleImageLayer();
  void handleCameraLayer();
  void handleLightLayer();
  void handleModel3DLayer();
  void handleOpenLayer();
 };

 ArtifactLayerMenu::Impl::Impl(ArtifactLayerMenu* menu)
 {
  parentPtr_ = static_cast<ArtifactLayerMenu*>(menu);

  createSolidLayerAction_ = new QAction("平面(&Y)...");
  createSolidLayerAction_->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Y));

  createAdjustableLayerAction_ = new QAction("調整レイヤー(&A)");
  createAdjustableLayerAction_->setShortcut(QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_Y));

  createNullLayerAction_ = new QAction("ヌルオブジェクト(&N)");
  createNullLayerAction_->setShortcut(QKeySequence(Qt::CTRL | Qt::ALT | Qt::SHIFT | Qt::Key_Y));

  createTextLayerAction_ = new QAction("テキスト(&T)");
  createTextLayerAction_->setShortcut(QKeySequence(Qt::CTRL | Qt::ALT | Qt::SHIFT | Qt::Key_T));

  createShapeLayerAction_ = new QAction("シェイプレイヤー(&S)");

  createImageLayerAction_ = new QAction("画像レイヤー(&I)");

  createCameraLayerAction_ = new QAction("カメラ(&C)...");
  createCameraLayerAction_->setShortcut(QKeySequence(Qt::CTRL | Qt::ALT | Qt::SHIFT | Qt::Key_C));

  createLightLayerAction_ = new QAction("ライト(&L)...");
  createLightLayerAction_->setShortcut(QKeySequence(Qt::CTRL | Qt::ALT | Qt::SHIFT | Qt::Key_L));

  createModel3DLayerAction_ = new QAction("3Dモデル(&M)...");

  layerSettingsAction_ = new QAction("平面設定(&Y)...");
  layerSettingsAction_->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Y));

  openLayerAction_ = new QAction("レイヤーを開く(&O)");
  openLayerAction_->setDisabled(true);

  openLayerSourceAction_ = new QAction("レイヤーソースを開く");
  
  maskMenu_ = new QMenu("マスク(&M)");
  maskMenu_->addAction("新規マスク")->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_N));
  maskMenu_->addAction("すべてのマスクのロック解除");
  maskMenu_->addAction("すべてのマスクを削除");
  
  transformMenu_ = new QMenu("トランスフォーム(&T)");
  resetTransformAction_ = transformMenu_->addAction("リセット");
  centerAnchorPointAction_ = transformMenu_->addAction("アンカーポイントをレイヤーコンテンツの中央に配置");
  centerAnchorPointAction_->setShortcut(QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_Home));
  autoOrientAction_ = transformMenu_->addAction("自動方向...");
  autoOrientAction_->setShortcut(QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_O));

  arrangeMenu_ = new QMenu("配置(&A)");
  bringToFrontAction_ = arrangeMenu_->addAction("最前面へ移動");
  bringToFrontAction_->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_BracketRight));
  bringForwardAction_ = arrangeMenu_->addAction("前面へ移動");
  bringForwardAction_->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_BracketRight));
  sendBackwardAction_ = arrangeMenu_->addAction("背面へ移動");
  sendBackwardAction_->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_BracketLeft));
  sendToBackAction_ = arrangeMenu_->addAction("最背面へ移動");
  sendToBackAction_->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_BracketLeft));

  guideLayerAction_ = new QAction("ガイドレイヤー(&G)");
  guideLayerAction_->setCheckable(true);

  addMarkerAction_ = new QAction("マーカーを追加(&M)");
  addMarkerAction_->setShortcut(QKeySequence(Qt::Key_Asterisk));

  layerTimeMenu_ = new QMenu("時間(&T)");
  layerTimeMenu_->addAction("タイムリマップレイヤー可能にする")->setShortcut(QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_T));
  layerTimeMenu_->addAction("時間反転レイヤー")->setShortcut(QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_R));
  layerTimeMenu_->addAction("フレームをフリーズ");
  
  createLayerMenu = new QMenu("新規(&N)");
  createLayerMenu->addAction(createTextLayerAction_);
  createLayerMenu->addAction(createSolidLayerAction_);
  createLayerMenu->addAction(createLightLayerAction_);
  createLayerMenu->addAction(createCameraLayerAction_);
  createLayerMenu->addAction(createNullLayerAction_);
  createLayerMenu->addAction(createShapeLayerAction_);
  createLayerMenu->addAction(createAdjustableLayerAction_);
  createLayerMenu->addAction(createImageLayerAction_);
  createLayerMenu->addAction(createModel3DLayerAction_);

  menu->addMenu(createLayerMenu);
  menu->addAction(layerSettingsAction_);
  menu->addSeparator();
  menu->addAction(openLayerAction_);
  menu->addAction(openLayerSourceAction_);
  menu->addSeparator();
  menu->addMenu(maskMenu_);
  menu->addMenu(transformMenu_);
  menu->addMenu(arrangeMenu_);
  menu->addSeparator();
  menu->addMenu(layerTimeMenu_);
  menu->addSeparator();
  menu->addAction(guideLayerAction_);
  menu->addAction(addMarkerAction_);




  QObject::connect(createNullLayerAction_, &QAction::triggered, [this]() {
   handleCreateNullLayer();
   });

  QObject::connect(createSolidLayerAction_, &QAction::triggered, [this]() {
   handleCreateSolidLayer();
   });
 }

  void ArtifactLayerMenu::Impl::handleCreateSolidLayer()
  {
   qDebug() << "[handleCreateSolidLayer] Called";
   auto createSolidLayer = new CreateSolidLayerSettingDialog(parentPtr_);
   
   auto service = ArtifactProjectService::instance();
   QObject::connect(createSolidLayer, &CreateSolidLayerSettingDialog::submit, parentPtr_, [service](const ArtifactSolidLayerInitParams& params) {
       qDebug() << "[handleCreateSolidLayer] Creating solid layer with name:" << params.name().toQString();
       service->addLayerToCurrentComposition(params);
   });
   
   createSolidLayer->setAttribute(Qt::WA_DeleteOnClose);
   createSolidLayer->exec();
  }

 void ArtifactLayerMenu::Impl::handleCreateNullLayer()
 {
   qDebug() << "[handleCreateNullLayer] Called";
   auto service = ArtifactProjectService::instance();
   if (!service) return;

   ArtifactNullLayerInitParams params(u8"Null 1");
   
   auto compWeak = service->currentComposition();
   if (auto comp = compWeak.lock()) {
       auto size = comp->settings().compositionSize();
       if (size.width() > 0 && size.height() > 0) {
           params.setWidth(size.width());
           params.setHeight(size.height());
       }
   }

   service->addLayerToCurrentComposition(params);
 }

  void ArtifactLayerMenu::Impl::handleAdjustableLayer()
  {
   qDebug() << "[handleAdjustableLayer] Called";
   auto service = ArtifactProjectService::instance();
   if (!service) return;

   ArtifactSolidLayerInitParams params(u8"Adjustment Layer");
   auto compWeak = service->currentComposition();
   if (auto comp = compWeak.lock()) {
       auto size = comp->settings().compositionSize();
       if (size.width() > 0 && size.height() > 0) {
           params.setWidth(size.width());
           params.setHeight(size.height());
       }
   }
   // Black color for adjustment layer (typically hidden or used just for effects)
   params.setColor(FloatColor(0.0f, 0.0f, 0.0f, 1.0f));

   service->addLayerToCurrentComposition(params);
   // Note: Since we don't have direct access to the layer ptr here directly,
   // normally we'd listen to layerAdded signal or the service handles adjustment layers natively.
   // Assuming adjustment layers might be a specific subtype in the future, or we set a flag after.
  }

  void ArtifactLayerMenu::Impl::handleTextLayer()
  {
   qDebug() << "[handleTextLayer] Called";
   ArtifactTextLayerInitParams params("Text Layer");
   auto service = ArtifactProjectService::instance();

   service->addLayerToCurrentComposition(params);
  }

  void ArtifactLayerMenu::Impl::handleShapeLayer()
  {
   qDebug() << "[handleShapeLayer] Called";
   // TODO: 実装
  }

  void ArtifactLayerMenu::Impl::handleImageLayer()
  {
   qDebug() << "[handleImageLayer] Called";
   // TODO: 実装
  }

  void ArtifactLayerMenu::Impl::handleLightLayer()
  {
   qDebug() << "[handleLightLayer] Called";
   // TODO: 実装
  }

  void ArtifactLayerMenu::Impl::handleModel3DLayer()
  {
   qDebug() << "[handleModel3DLayer] Called";
   // TODO: 実装
  }

 void ArtifactLayerMenu::Impl::handleCameraLayer()
 {
  //ArtifactLayerInitParams param;
 }

 void ArtifactLayerMenu::Impl::handleOpenLayer()
 {

 }
	

 void ArtifactLayerMenu::Impl::handleProjectClosed()
 {

 }

 ArtifactLayerMenu::ArtifactLayerMenu(QWidget* parent/*=nullptr*/):QMenu(parent),impl_(new Impl(this))
 {
  setTitle("レイヤー(&L)");
  setTearOffEnabled(false);
  setSeparatorsCollapsible(true);
  setMinimumWidth(160);

  auto projectService = ArtifactProjectService::instance();
 	
 	//connect(projectService,)
 }

 ArtifactLayerMenu::~ArtifactLayerMenu()
 {

 }

QMenu* ArtifactLayerMenu::newLayerMenu() const
 {

   return nullptr;
 }



}