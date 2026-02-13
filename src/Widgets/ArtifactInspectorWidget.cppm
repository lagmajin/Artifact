module;
#include <wobjectimpl.h>
#include <QLabel>
#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTabWidget>
#include <QListWidget>
#include <QPushButton>
#include <QMenu>
#include <QCursor>
#include <cstdlib>
module Widgets.Inspector;
import std;
import Utils.Id;
import Widgets.Utils.CSS;

import Artifact.Service.Project;
import Artifact.Composition.Abstract;
import Artifact.Effect.Abstract;
import Generator.Effector;

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

   // Rasterizer Effects Tab
   QWidget* effectsTabWidget = nullptr;
   QListWidget* effectsListWidget = nullptr;
   QPushButton* addEffectButton = nullptr;
   QPushButton* removeEffectButton = nullptr;

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
   void handleAddEffectClicked();
   void handleAddGeneratorEffect();
   void handleRemoveEffectClicked();
   void setNoProjectState();
   void setNoLayerState();
  };

 ArtifactInspectorWidget::Impl::Impl()
 {

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
  if (effectsListWidget) {
   effectsListWidget->clear();
  }
 }

 void ArtifactInspectorWidget::Impl::updateEffectsList()
 {
  if (!effectsListWidget || currentLayerId_.isNil()) {
   if (effectsListWidget) effectsListWidget->clear();
   return;
  }

  auto projectService = ArtifactProjectService::instance();
  if (!projectService) {
   if (effectsListWidget) effectsListWidget->clear();
   return;
  }

  if (currentCompositionId_.isNil()) {
   if (effectsListWidget) effectsListWidget->clear();
   return;
  }

  auto findResult = projectService->findComposition(currentCompositionId_);
  if (!findResult.success) {
   if (effectsListWidget) effectsListWidget->clear();
   return;
  }

  auto comp = findResult.ptr.lock();
  if (!comp) {
   if (effectsListWidget) effectsListWidget->clear();
   return;
  }

  auto layer = comp->layerById(currentLayerId_);
  if (!layer) {
   if (effectsListWidget) effectsListWidget->clear();
   return;
  }

  // レイヤーのエフェクトリストを取得して表示
  effectsListWidget->clear();
  auto effects = layer->getEffects();

  if (effects.empty()) {
   auto item = new QListWidgetItem("(No effects)");
   item->setFlags(item->flags() & ~Qt::ItemIsSelectable);
   effectsListWidget->addItem(item);
   qDebug() << "[Inspector] No effects for layer:" << currentLayerId_.toString();
   return;
  }

  for (const auto& effect : effects) {
   if (effect) {
    QString effectName = effect->displayName().toQString();
    QString effectStatus = effect->isEnabled() ? "✓" : "✗";
    QString itemText = QString("[%1] %2").arg(effectStatus, effectName);

    auto item = new QListWidgetItem(itemText);
    item->setData(Qt::UserRole, effect->effectID().toQString());
    effectsListWidget->addItem(item);

       qDebug() << "[Inspector] Effect listed:" << effectName;
      }
     }
    }

    void ArtifactInspectorWidget::Impl::handleAddEffectClicked()
    {
     if (currentLayerId_.isNil()) {
      qDebug() << "[Inspector] No layer selected";
      return;
     }

     // エフェクトメニューを作成
     QMenu effectMenu;
     effectMenu.addAction("Generator Effect", [this]() {
      handleAddGeneratorEffect();
     });
     effectMenu.addSeparator();
     effectMenu.addAction("Blur", [this]() {
      qDebug() << "[Inspector] Blur effect added (placeholder)";
      // TODO: Blur エフェクト実装
     });
     effectMenu.addAction("Glow", [this]() {
      qDebug() << "[Inspector] Glow effect added (placeholder)";
      // TODO: Glow エフェクト実装
     });
     effectMenu.addAction("Shadow", [this]() {
      qDebug() << "[Inspector] Shadow effect added (placeholder)";
      // TODO: Shadow エフェクト実装
     });

     // メニューを表示
     effectMenu.exec(QCursor::pos());
    }

    void ArtifactInspectorWidget::Impl::handleAddGeneratorEffect()
    {
     if (currentLayerId_.isNil() || currentCompositionId_.isNil()) {
      qDebug() << "[Inspector] Cannot add effect: no layer or composition";
      return;
     }

     auto projectService = ArtifactProjectService::instance();
     if (!projectService) {
      qDebug() << "[Inspector] Project service not available";
      return;
     }

     auto findResult = projectService->findComposition(currentCompositionId_);
     if (!findResult.success) {
      qDebug() << "[Inspector] Composition not found";
      return;
     }

     auto comp = findResult.ptr.lock();
     if (!comp) {
      qDebug() << "[Inspector] Composition is null";
      return;
     }

     auto layer = comp->layerById(currentLayerId_);
     if (!layer) {
      qDebug() << "[Inspector] Layer not found";
      return;
     }

     // SolidGeneratorEffector を作成してレイヤーに追加
     auto solidGenerator = std::make_shared<SolidGeneratorEffector>();
     solidGenerator->setName(UniString("Solid Color"));
     solidGenerator->setSolidColor(QColor(255, 255, 255, 255));

     // Generator を ArtifactAbstractEffect にラップ
     // TODO: Generator と Effect の統合インターフェース

     qDebug() << "[Inspector] SolidGenerator created - Name:" << solidGenerator->name().toQString();

     // エフェクトリストを更新
     updateEffectsList();
    }

    void ArtifactInspectorWidget::Impl::handleRemoveEffectClicked()
    {
     if (!effectsListWidget) return;

     auto selectedItems = effectsListWidget->selectedItems();
     if (selectedItems.isEmpty()) {
      qDebug() << "[Inspector] No effect selected";
      return;
     }

     if (currentLayerId_.isNil() || currentCompositionId_.isNil()) {
      qDebug() << "[Inspector] Cannot remove effect: no layer or composition";
      return;
     }

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
      layer->removeEffect(effectID);
      qDebug() << "[Inspector] Effect removed:" << effectID.toQString();
     }

     // エフェクトリストを更新
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

  // ================== Rasterizer Effects Tab ==================
  impl_->effectsTabWidget = new QWidget();
  auto effectsLayout = new QVBoxLayout();

  // エフェクトリストウィジェット
  impl_->effectsListWidget = new QListWidget();
  impl_->effectsListWidget->setMaximumHeight(200);
  effectsLayout->addWidget(new QLabel("Effects:"));
  effectsLayout->addWidget(impl_->effectsListWidget);

  // ボタンレイアウト
  auto buttonLayout = new QHBoxLayout();
  impl_->addEffectButton = new QPushButton("+ Add Effect");
  impl_->removeEffectButton = new QPushButton("- Remove");
  buttonLayout->addWidget(impl_->addEffectButton);
  buttonLayout->addWidget(impl_->removeEffectButton);
  effectsLayout->addLayout(buttonLayout);

  effectsLayout->addStretch();
  effectsLayout->setContentsMargins(8, 8, 8, 8);
  effectsLayout->setSpacing(4);

  impl_->effectsTabWidget->setLayout(effectsLayout);
  impl_->tabWidget->addTab(impl_->effectsTabWidget, "Rasterizer Effects");

  // ボタンシグナルを接続
  QObject::connect(impl_->addEffectButton, &QPushButton::clicked, this, [this]() {
   impl_->handleAddEffectClicked();
  });

  QObject::connect(impl_->removeEffectButton, &QPushButton::clicked, this, [this]() {
   impl_->handleRemoveEffectClicked();
  });

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

   // レイヤー作成シグナルに接続（作成されたレイヤーを自動選択）
   QObject::connect(projectService, &ArtifactProjectService::layerCreated, this, [this](const LayerID& id) {
    impl_->handleLayerSelected(id);
   });

   // TODO: レイヤー選択シグナルがあれば接続
   // QObject::connect(projectService, &ArtifactProjectService::layerSelected, this, [this](const LayerID& id) {
   //  impl_->handleLayerSelected(id);
   // });
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