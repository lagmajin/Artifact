module;
#include <wobjectimpl.h>
#include <QLabel>
#include <QWidget>
#include <QVBoxLayout>
#include <QGroupBox>
#include <QScrollArea>
#include <QMenu>
#include <QHBoxLayout>
#include <QTabWidget>
#include <QListWidget>
#include <QFormLayout>
#include <QLabel>
#include <QPushButton>
#include <QMenu>
#include <QCursor>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QCheckBox>
#include <QLineEdit>
#include <QColorDialog>
#include <QVariant>
#include <cstdlib>

module Widgets.Inspector;
import std;
import Utils.Id;
import Utils.String.UniString;
import Widgets.Utils.CSS;

import Artifact.Service.Project;
import Artifact.Composition.Abstract;
import Artifact.Effect.Abstract;
import Artifact.Widgets.ArtifactPropertyWidget;
import Undo.UndoManager;
import Generator.Effector;
import Artifact.Effect.Generator.Cloner;
import Artifact.Effect.Generator.FractalNoise;
import Artifact.Effect.Transform.Twist;
import Artifact.Effect.Transform.Bend;
import Artifact.Effect.Render.PBRMaterial;
import Artifact.Effect.LayerTransform.Transform2D;
import Artifact.Effect.Rasterizer.Blur;
import Artifact.Effect.Rasterizer.DropShadow;
import Artifact.Effect.Glow;
import Artifact.Effect.Wave;
import Artifact.Effect.Spherize;

namespace Artifact {

 using namespace ArtifactCore;

 //using namespace ArtifactWidgets;

 W_OBJECT_IMPL(ArtifactInspectorWidget)

  class ArtifactInspectorWidget::Impl {
  private:

  public:
   Impl();
   ~Impl();
   QWidget* containerWidget = nullptr;
   QTabWidget* tabWidget = nullptr;

   // Layer Info Tab
   QLabel* layerNameLabel = nullptr;
   QLabel* layerTypeLabel = nullptr;
   QLabel* statusLabel = nullptr;

   // Effects Pipeline Tab
   QScrollArea* effectsScrollArea = nullptr;
   QWidget* effectsTabWidget = nullptr;

   struct EffectRack {
       QListWidget* listWidget = nullptr;
       QPushButton* addButton = nullptr;
       QPushButton* removeButton = nullptr;
   };
   EffectRack racks[5];
   ArtifactPropertyWidget* propertyWidget = nullptr;

   QMenu* inspectorMenu_ = nullptr;

   CompositionID currentCompositionId_;
   LayerID currentLayerId_;

   void rebuildMenu();
   void defaultHandleKeyPressEvent(QKeyEvent* event);
   void defaultHandleMousePressEvent(QMouseEvent* event);

   void showContextMenu();
   void handleProjectCreated();
   void handleProjectClosed();
   void handleCompositionCreated(const CompositionID& id);
   void handleLayerSelected(const LayerID& id);
   void updateLayerInfo();
   void updateEffectsList();
   void updatePropertiesForEffect(const QString& effectId);
   void handleAddEffectClicked(int rackIndex);
   void handleAddGeneratorEffect(int rackIndex);
   void handleRemoveEffectClicked(int rackIndex);
   void setNoProjectState();
   void setNoLayerState();
  };

 ArtifactInspectorWidget::Impl::Impl()
 {

 }

void ArtifactInspectorWidget::Impl::updatePropertiesForEffect(const QString& effectId)
{
    // Now handled entirely by ArtifactPropertyWidget when a layer is set.
    // Kept here for compatibility until we completely remove old effect list coupling.
}

 ArtifactInspectorWidget::Impl::~Impl()
 {

 }

 void ArtifactInspectorWidget::Impl::rebuildMenu()
 {

 }

 void ArtifactInspectorWidget::Impl::defaultHandleKeyPressEvent(QKeyEvent* event)
 {
 }

 void ArtifactInspectorWidget::Impl::handleProjectCreated()
 {
  qDebug() << "[Inspector] Project created";
  // プロジェクト作成時はまだレイヤーがないので「レイヤー未選択」状態に
  containerWidget->setEnabled(true);
  layerNameLabel->setText("Layer: (No layer selected)");
  layerTypeLabel->setText("Type: N/A");
  statusLabel->setText("Status: Waiting for layer selection");
 }

 void ArtifactInspectorWidget::Impl::handleProjectClosed()
 {
  qDebug() << "[Inspector] Project closed";
  setNoProjectState();
 }

 void ArtifactInspectorWidget::Impl::handleCompositionCreated(const CompositionID& id)
 {
  qDebug() << "[Inspector] Composition created:" << id.toString();
  currentCompositionId_ = id;
  // コンポジション作成時はまだレイヤーなし
  layerNameLabel->setText("Layer: (No layer in composition)");
  layerTypeLabel->setText("Type: N/A");
  statusLabel->setText("Status: Add layers to the composition");
 }

 void ArtifactInspectorWidget::Impl::handleLayerSelected(const LayerID& id)
 {
  qDebug() << "[Inspector] Layer selected:" << id.toString();
  currentLayerId_ = id;
  updateLayerInfo();
  updateEffectsList();
 }

 void ArtifactInspectorWidget::Impl::updateLayerInfo()
 {
  if (currentLayerId_.isNil()) {
   setNoLayerState();
   return;
  }

  // レイヤー情報を取得
  auto projectService = ArtifactProjectService::instance();
  if (!projectService) {
   setNoProjectState();
   return;
  }

  // コンポジションを取得
  if (currentCompositionId_.isNil()) {
   setNoLayerState();
   return;
  }

  auto findResult = projectService->findComposition(currentCompositionId_);
  if (!findResult.success) {
   setNoLayerState();
   return;
  }

  auto comp = findResult.ptr.lock();
  if (!comp) {
   setNoLayerState();
   return;
  }

  // レイヤーを取得
  if (!comp->containsLayerById(currentLayerId_)) {
   setNoLayerState();
   return;
  }

  auto layer = comp->layerById(currentLayerId_);
  if (!layer) {
   setNoLayerState();
   return;
  }

  // レイヤー情報を表示
  QString layerName = layer->layerName();
  layerNameLabel->setText(QString("Layer: %1").arg(layerName.isEmpty() ? "(Unnamed)" : layerName));

  // レイヤータイプを判定
  QString layerType = "Unknown";
  if (layer->isNullLayer()) {
   layerType = "Null Layer";
  } else if (layer->isAdjustmentLayer()) {
   layerType = "Adjustment Layer";
  } else {
   // TODO: 他のレイヤータイプも判定
   layerType = "Layer";
  }
  layerTypeLabel->setText(QString("Type: %1").arg(layerType));

  statusLabel->setText(QString("Status: Layer selected - ID: %1").arg(currentLayerId_.toString()));

  // プロパティウィジェットにも選択レイヤーを反映
  if (propertyWidget) {
      propertyWidget->setLayer(layer);
  }

  qDebug() << "[Inspector] Updated layer info:" << layerName << "Type:" << layerType;
 }

 void ArtifactInspectorWidget::Impl::setNoProjectState()
 {
  containerWidget->setEnabled(false);
  layerNameLabel->setText("Layer: (No project)");
  layerTypeLabel->setText("Type: N/A");
  statusLabel->setText("Status: Create or open a project");
  currentCompositionId_ = CompositionID();
  currentLayerId_ = LayerID();
 }

 void ArtifactInspectorWidget::Impl::setNoLayerState()
 {
  layerNameLabel->setText("Layer: (No layer selected)");
  layerTypeLabel->setText("Type: N/A");
  statusLabel->setText("Status: Select a layer or create one");
  currentLayerId_ = LayerID();

  // エフェクトリストもクリア
  for (auto& rack : racks) {
   if (rack.listWidget) {
    rack.listWidget->clear();
   }
  }
  if (propertyWidget) {
      propertyWidget->clear();
  }
 }

  void ArtifactInspectorWidget::Impl::updateEffectsList()
 {
  for (int i=0; i<5; ++i) {
      if (racks[i].listWidget) racks[i].listWidget->clear();
  }
  if (currentLayerId_.isNil()) return;

  auto projectService = ArtifactProjectService::instance();
  if (!projectService) return;

  if (currentCompositionId_.isNil()) return;

  auto findResult = projectService->findComposition(currentCompositionId_);
  if (!findResult.success) return;

  auto comp = findResult.ptr.lock();
  if (!comp) return;

  auto layer = comp->layerById(currentLayerId_);
  if (!layer) return;

  auto effects = layer->getEffects();

  for (const auto& effect : effects) {
   if (effect) {
    QString effectName = effect->displayName().toQString();
    QString effectStatus = effect->isEnabled() ? "✓" : "✗";
    QString itemText = QString("[%1] %2").arg(effectStatus, effectName);

    auto item = new QListWidgetItem(itemText);
    item->setData(Qt::UserRole, effect->effectID().toQString());

    int stageIdx = static_cast<int>(effect->pipelineStage());
    if (stageIdx >= 0 && stageIdx < 5) {
        if (racks[stageIdx].listWidget) racks[stageIdx].listWidget->addItem(item);
    }
   }
  }
  for (int i=0; i<5; ++i) {
      if (racks[i].listWidget && racks[i].listWidget->count() == 0) {
          auto item = new QListWidgetItem("(No effects)");
          item->setFlags(item->flags() & ~Qt::ItemIsSelectable);
          racks[i].listWidget->addItem(item);
      }
  }
 }

 void ArtifactInspectorWidget::Impl::handleAddEffectClicked(int rackIndex)
 {
  if (currentLayerId_.isNil() || currentCompositionId_.isNil()) return;

  auto projectService = ArtifactProjectService::instance();
  if (!projectService) return;

  auto findResult = projectService->findComposition(currentCompositionId_);
  if (!findResult.success) return;

  auto comp = findResult.ptr.lock();
  if (!comp) return;

  auto layer = comp->layerById(currentLayerId_);
  if (!layer) return;

  QMenu effectMenu;
  
  auto addAndRefresh = [this, layer](std::shared_ptr<ArtifactAbstractEffect> newEffect) {
      if (newEffect) {
          // Generate a simple unique ID for the effect for now
          newEffect->setEffectID(ArtifactCore::UniString(std::to_string(std::rand()).c_str()));
          layer->addEffect(newEffect);
          updateEffectsList();
      }
  };

  switch (static_cast<EffectPipelineStage>(rackIndex)) {
      case EffectPipelineStage::Generator:
          effectMenu.addAction("Cloner", [addAndRefresh]() { addAndRefresh(std::make_shared<ClonerGenerator>()); });
          effectMenu.addAction("Fractal Noise", [addAndRefresh]() { addAndRefresh(std::make_shared<FractalNoiseGenerator>()); });
          break;
      case EffectPipelineStage::GeometryTransform:
          effectMenu.addAction("Twist", [addAndRefresh]() { addAndRefresh(std::make_shared<TwistTransform>()); });
          effectMenu.addAction("Bend", [addAndRefresh]() { addAndRefresh(std::make_shared<BendTransform>()); });
          break;
      case EffectPipelineStage::MaterialRender:
          effectMenu.addAction("PBR Material", [addAndRefresh]() { addAndRefresh(std::make_shared<PBRMaterialEffect>()); });
          break;
      case EffectPipelineStage::Rasterizer:
           effectMenu.addAction("Blur", [addAndRefresh]() { addAndRefresh(std::make_shared<BlurEffect>()); });
           effectMenu.addAction("Glow", [addAndRefresh]() { addAndRefresh(std::make_shared<GlowEffect>()); });
           effectMenu.addAction("Drop Shadow", [addAndRefresh]() { addAndRefresh(std::make_shared<DropShadowEffect>()); });
           effectMenu.addAction("Wave", [addAndRefresh]() { addAndRefresh(std::make_shared<WaveEffect>()); });
           effectMenu.addAction("Spherize", [addAndRefresh]() { addAndRefresh(std::make_shared<SpherizeEffect>()); });
           break;
      case EffectPipelineStage::LayerTransform:
          effectMenu.addAction("Transform 2D", [addAndRefresh]() { addAndRefresh(std::make_shared<LayerTransform2D>()); });
          break;
  }
  
  effectMenu.exec(QCursor::pos());
 }

 void ArtifactInspectorWidget::Impl::handleAddGeneratorEffect(int rackIndex)
 {
  // Obsolete function. Kept temporarily to appease class signature.
 }

 void ArtifactInspectorWidget::Impl::handleRemoveEffectClicked(int rackIndex)
 {
  if (!racks[rackIndex].listWidget) return;

  auto selectedItems = racks[rackIndex].listWidget->selectedItems();
  if (selectedItems.isEmpty()) return;

  if (currentLayerId_.isNil() || currentCompositionId_.isNil()) return;

  auto projectService = ArtifactProjectService::instance();
  if (!projectService) return;

  auto findResult = projectService->findComposition(currentCompositionId_);
  if (!findResult.success) return;

  auto comp = findResult.ptr.lock();
  if (!comp) return;

  auto layer = comp->layerById(currentLayerId_);
  if (!layer) return;

  for (auto item : selectedItems) {
   UniString effectID(item->data(Qt::UserRole).toString().toStdString());
   if(effectID.length() > 0) {
       layer->removeEffect(effectID);
       qDebug() << "[Inspector] Effect removed:" << effectID.toQString();
   }
  }

  updateEffectsList();
 }

 void ArtifactInspectorWidget::update()
 {

 }

  ArtifactInspectorWidget::ArtifactInspectorWidget(QWidget* parent /*= nullptr*/) :QScrollArea(parent),impl_(new Impl())
 {

  auto style = getDCCStyleSheetPreset(DccStylePreset::ModoStyle);

  setStyleSheet(style);

  // メインレイアウト
  auto mainLayout = new QVBoxLayout();
  impl_->containerWidget = new QWidget();

  // タブウィジェットを作成
  impl_->tabWidget = new QTabWidget();

  // ================== Layer Info Tab ==================
  auto layerInfoWidget = new QWidget();
  auto layerInfoLayout = new QVBoxLayout();

  // ステータスラベル
  impl_->statusLabel = new QLabel("Status: No project");
  impl_->statusLabel->setStyleSheet("QLabel { color: #888; font-style: italic; }");
  layerInfoLayout->addWidget(impl_->statusLabel);

  // レイヤー名ラベル
  impl_->layerNameLabel = new QLabel("Layer: (No project)");
  impl_->layerNameLabel->setStyleSheet("QLabel { font-weight: bold; }");
  layerInfoLayout->addWidget(impl_->layerNameLabel);

  // レイヤータイプラベル
  impl_->layerTypeLabel = new QLabel("Type: N/A");
  layerInfoLayout->addWidget(impl_->layerTypeLabel);

  layerInfoLayout->setAlignment(Qt::AlignTop);
  layerInfoLayout->setContentsMargins(8, 8, 8, 8);
  layerInfoLayout->setSpacing(4);

  layerInfoWidget->setLayout(layerInfoLayout);
  impl_->tabWidget->addTab(layerInfoWidget, "Layer Info");

  // ================== Effects Pipeline Tab ==================
  impl_->effectsScrollArea = new QScrollArea();
  impl_->effectsScrollArea->setWidgetResizable(true);
  impl_->effectsTabWidget = new QWidget();
  auto effectsLayout = new QVBoxLayout();

  QString rackNames[5] = {
      "1. Generator",
      "2. Geometry Transform",
      "3. Material & Render",
      "4. Rasterizer",
      "5. Layer Transform"
  };

  for (int i = 0; i < 5; ++i) {
      auto rackGroup = new QGroupBox(rackNames[i]);
      //! rackGroup->setStyleSheet("QGroupBox { font-weight: bold; border: 1px solid #555; margin-top: 10px; } QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 3px 0 3px; }");
      auto rackLayout = new QVBoxLayout();
      
      impl_->racks[i].listWidget = new QListWidget();
      impl_->racks[i].listWidget->setMaximumHeight(100);
      
      auto btnLayout = new QHBoxLayout();
      impl_->racks[i].addButton = new QPushButton("+ Add");
      impl_->racks[i].removeButton = new QPushButton("- Remove");
      btnLayout->addWidget(impl_->racks[i].addButton);
      btnLayout->addWidget(impl_->racks[i].removeButton);
      
      rackLayout->addWidget(impl_->racks[i].listWidget);
      rackLayout->addLayout(btnLayout);
      rackLayout->setContentsMargins(4, 12, 4, 4);
      rackGroup->setLayout(rackLayout);
      
      effectsLayout->addWidget(rackGroup);

      // Button signals
      QObject::connect(impl_->racks[i].addButton, &QPushButton::clicked, this, [this, i]() {
          impl_->handleAddEffectClicked(i);
      });
      QObject::connect(impl_->racks[i].removeButton, &QPushButton::clicked, this, [this, i]() {
          impl_->handleRemoveEffectClicked(i);
      });
  }

  effectsLayout->addStretch();
  effectsLayout->setContentsMargins(8, 8, 8, 8);
  effectsLayout->setSpacing(8);

  impl_->effectsTabWidget->setLayout(effectsLayout);
  impl_->effectsScrollArea->setWidget(impl_->effectsTabWidget);
  impl_->tabWidget->addTab(impl_->effectsScrollArea, "Effects Pipeline");

  // ================== Properties Tab ==================
  impl_->propertyWidget = new ArtifactPropertyWidget();
  impl_->tabWidget->addTab(impl_->propertyWidget, "Properties");


  // タブをメインレイアウトに追加
  mainLayout->addWidget(impl_->tabWidget);
  mainLayout->setContentsMargins(0, 0, 0, 0);
  mainLayout->setSpacing(0);

  impl_->containerWidget->setLayout(mainLayout);

  setWidget(impl_->containerWidget);
  setWidgetResizable(true);

  // 初期状態: プロジェクトなし -> 無効化
  impl_->setNoProjectState();

  auto projectService = ArtifactProjectService::instance();
  if (projectService) {
   // プロジェクト作成/クローズシグナルに接続
   QObject::connect(projectService, &ArtifactProjectService::projectCreated, this, [this]() {
    impl_->handleProjectCreated();
   });

   // TODO: projectClosed シグナルがあれば接続
   // QObject::connect(projectService, &ArtifactProjectService::projectClosed, this, [this]() {
   //  impl_->handleProjectClosed();
   // });

   // コンポジション作成シグナルに接続
   QObject::connect(projectService, &ArtifactProjectService::compositionCreated, this, [this](const CompositionID& id) {
    impl_->handleCompositionCreated(id);
   });

   // When effect list selection changes, update properties (now we show layer props in propertyWidget)
   

   // レイヤー作成シグナルに接続（作成されたレイヤーを自動選択）
   QObject::connect(projectService, &ArtifactProjectService::layerCreated, this, [this](const CompositionID& cid, const LayerID& id) {
    if (impl_->currentCompositionId_ == cid) {
        impl_->handleLayerSelected(id);
    }
   });

   // レイヤー選択シグナルに接続
   QObject::connect(projectService, &ArtifactProjectService::layerSelected, this, [this](const LayerID& id) {
    impl_->handleLayerSelected(id);
   });
  }
 }

 ArtifactInspectorWidget::~ArtifactInspectorWidget()
 {
  delete impl_;
 }

 void ArtifactInspectorWidget::triggerUpdate()
 {
  update();
 }

 void ArtifactInspectorWidget::contextMenuEvent(QContextMenuEvent*)
 {



 }

}