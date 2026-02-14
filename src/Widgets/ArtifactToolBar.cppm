module;
#include <QAction>
#include <QToolBar>
#include <QActionGroup>
#include <QString>
#include <QSize>
#include <QIcon>
#include <wobjectimpl.h>

module Widgets.ToolBar;
import Utils;
import Artifact.Tool.Manager;
import Artifact.Service.Application;

namespace Artifact
{

 W_OBJECT_IMPL(ArtifactToolBar)

 class ArtifactToolBar::Impl {
 public:
  Impl();
  ~Impl();
  
  // Tool actions
  QAction* homeAction_ = nullptr;
  QAction* selectTool_ = nullptr;
  QAction* maskTool_ = nullptr;
  QAction* handTool_ = nullptr;
  QAction* shapeTool_ = nullptr;
  QAction* papetTool_ = nullptr;
  
  // Zoom actions
  QAction* zoomInAction_ = nullptr;
  QAction* zoomOutAction_ = nullptr;
  QAction* zoom100Action_ = nullptr;
  QAction* zoomFitAction_ = nullptr;
  
  // Grid/Guide actions
  QAction* gridToggleAction_ = nullptr;
  QAction* guideToggleAction_ = nullptr;
  
  // View mode actions
  QActionGroup* viewModeGroup_ = nullptr;
  QAction* normalViewAction_ = nullptr;
  QAction* gridViewAction_ = nullptr;
  QAction* detailViewAction_ = nullptr;
  
  float currentZoomLevel_ = 1.0f;
  bool gridVisible_ = true;
  bool guideVisible_ = true;
 };

 ArtifactToolBar::Impl::Impl()
 {
 }

 ArtifactToolBar::Impl::~Impl()
 {
 }

 ArtifactToolBar::ArtifactToolBar(QWidget* parent) : QToolBar(parent), impl_(new Impl())
 {
  setIconSize(QSize(32, 32));
  setMovable(false);
  setFloatable(false);
  
  // Main tool actions
  impl_->homeAction_ = new QAction();
  impl_->homeAction_->setIcon(QIcon(ArtifactCore::getIconPath() + "/Png/home.png"));
  impl_->homeAction_->setToolTip("Home");
  
  impl_->handTool_ = new QAction();
  impl_->handTool_->setIcon(QIcon(ArtifactCore::getIconPath() + "/Png/hand.png"));
  impl_->handTool_->setToolTip("Hand Tool (Pan)");

  impl_->maskTool_ = new QAction();
  impl_->maskTool_->setIcon(QIcon(ArtifactCore::getIconPath() + "/Png/pen.png"));
  impl_->maskTool_->setToolTip("Mask Tool");

  impl_->shapeTool_ = new QAction();
  impl_->shapeTool_->setIcon(QIcon(ArtifactCore::getIconPath() + "/Png/path.png"));
  impl_->shapeTool_->setToolTip("Shape Tool");

  // Add main tools
  addAction(impl_->homeAction_);
  addAction(impl_->handTool_);
  addAction(impl_->maskTool_);
  addAction(impl_->shapeTool_);
  
  // Separator
  addSeparator();
  
  // Zoom actions
  impl_->zoomInAction_ = new QAction("+");
  impl_->zoomInAction_->setToolTip("Zoom In (Ctrl++)");
  impl_->zoomInAction_->setIconText("Zoom In");
  
  impl_->zoomOutAction_ = new QAction("-");
  impl_->zoomOutAction_->setToolTip("Zoom Out (Ctrl+-)");
  impl_->zoomOutAction_->setIconText("Zoom Out");
  
  impl_->zoom100Action_ = new QAction("100%");
  impl_->zoom100Action_->setToolTip("Reset Zoom (Ctrl+0)");
  impl_->zoom100Action_->setIconText("100%");
  
  impl_->zoomFitAction_ = new QAction("Fit");
  impl_->zoomFitAction_->setToolTip("Fit Window");
  impl_->zoomFitAction_->setIconText("Fit");
  
  addAction(impl_->zoomInAction_);
  addAction(impl_->zoomOutAction_);
  addAction(impl_->zoom100Action_);
  addAction(impl_->zoomFitAction_);
  
  // Separator
  addSeparator();
  
  // Grid/Guide actions
  impl_->gridToggleAction_ = new QAction("Grid");
  impl_->gridToggleAction_->setCheckable(true);
  impl_->gridToggleAction_->setChecked(true);
  impl_->gridToggleAction_->setToolTip("Show Grid");
  impl_->gridToggleAction_->setIconText("Grid");
  
  impl_->guideToggleAction_ = new QAction("Guide");
  impl_->guideToggleAction_->setCheckable(true);
  impl_->guideToggleAction_->setChecked(true);
  impl_->guideToggleAction_->setToolTip("Show Guide Lines");
  impl_->guideToggleAction_->setIconText("Guide");
  
  addAction(impl_->gridToggleAction_);
  addAction(impl_->guideToggleAction_);
  
  // Separator
  addSeparator();
  
  // View mode actions
  impl_->viewModeGroup_ = new QActionGroup(this);
  
  impl_->normalViewAction_ = new QAction("Normal");
  impl_->normalViewAction_->setCheckable(true);
  impl_->normalViewAction_->setChecked(true);
  impl_->normalViewAction_->setToolTip("Normal View");
  impl_->normalViewAction_->setIconText("Normal");
  
  impl_->gridViewAction_ = new QAction("Grid");
  impl_->gridViewAction_->setCheckable(true);
  impl_->gridViewAction_->setToolTip("Grid View");
  impl_->gridViewAction_->setIconText("Grid");
  
  impl_->detailViewAction_ = new QAction("Detail");
  impl_->detailViewAction_->setCheckable(true);
  impl_->detailViewAction_->setToolTip("Detail View");
  impl_->detailViewAction_->setIconText("Detail");
  
  impl_->viewModeGroup_->addAction(impl_->normalViewAction_);
  impl_->viewModeGroup_->addAction(impl_->gridViewAction_);
  impl_->viewModeGroup_->addAction(impl_->detailViewAction_);
  
  addAction(impl_->normalViewAction_);
  addAction(impl_->gridViewAction_);
  addAction(impl_->detailViewAction_);

  // Connect signals
  QObject::connect(impl_->homeAction_, &QAction::triggered, this, [this]() {
   homeRequested();
  });
   
  QObject::connect(impl_->handTool_, &QAction::triggered, this, [this]() {
   handToolRequested();
  });
   
  QObject::connect(impl_->zoomInAction_, &QAction::triggered, this, [this]() {
   zoomInRequested();
  });
   
  QObject::connect(impl_->zoomOutAction_, &QAction::triggered, this, [this]() {
   zoomOutRequested();
  });
   
  QObject::connect(impl_->zoom100Action_, &QAction::triggered, this, [this]() {
   zoom100Requested();
  });
   
  QObject::connect(impl_->zoomFitAction_, &QAction::triggered, this, [this]() {
   zoomFitRequested();
  });
   
  QObject::connect(impl_->gridToggleAction_, &QAction::triggered, this, [this](bool checked) {
   gridToggled(checked);
  });
   
  QObject::connect(impl_->guideToggleAction_, &QAction::triggered, this, [this](bool checked) {
   guideToggled(checked);
  });
  
  setStyleSheet(R"(
QToolButton {
    background: transparent;
    border: none;
    padding: 4px;
    min-width: 32px;
    min-height: 32px;
}
QToolButton:hover {
    background: rgba(255,255,255,30);
    border-radius: 4px;
}
QToolButton:pressed {
    background: rgba(255,255,255,60);
    border-radius: 4px;
}
QToolButton:checked {
    background: rgba(100,150,255,80);
    border-radius: 4px;
}
)");
 }
 
 ArtifactToolBar::~ArtifactToolBar()
 {
  delete impl_;
 }

 void ArtifactToolBar::setCompactMode(bool enabled)
 {
  if (enabled) {
   setIconSize(QSize(24, 24));
  } else {
   setIconSize(QSize(32, 32));
  }
 }

 void ArtifactToolBar::setTextUnderIcon(bool enabled)
 {
  setToolButtonStyle(enabled ? Qt::ToolButtonTextUnderIcon : Qt::ToolButtonIconOnly);
 }
 
 void ArtifactToolBar::setZoomLevel(float zoomPercent)
 {
  impl_->currentZoomLevel_ = zoomPercent;
  if (impl_->zoom100Action_) {
   impl_->zoom100Action_->setText(QString("%1%").arg(static_cast<int>(zoomPercent)));
  }
 }
 
 void ArtifactToolBar::setGridVisible(bool visible)
 {
  impl_->gridVisible_ = visible;
  if (impl_->gridToggleAction_) {
   impl_->gridToggleAction_->setChecked(visible);
  }
 }
 
 void ArtifactToolBar::setGuideVisible(bool visible)
 {
  impl_->guideVisible_ = visible;
  if (impl_->guideToggleAction_) {
   impl_->guideToggleAction_->setChecked(visible);
  }
 }

 void ArtifactToolBar::setActionEnabledAnimated(QAction* action, bool enabled)
 {
  if (!action) return;
  
  // アニメーション付きで有効/無効を切り替え
  // シンプルに即時切り替え（アニメーションはオプション）
  action->setEnabled(enabled);
  
  // アニメーションが必要ならQPropertyAnimationを使用
  // QPropertyAnimation* anim = new QPropertyAnimation(action, "enabled");
  // anim->setDuration(200);
  // anim->setStartValue(action->isEnabled());
  // anim->setEndValue(enabled);
  // anim->start(QAbstractAnimation::DeleteWhenStopped);
 }

 void ArtifactToolBar::lockHeight(bool locked /*= true*/)
 {
  if (locked) {
   // 高さを固定（現在の高さで固定）
   setFixedHeight(height());
  } else {
   // 高さを可変に戻す
   setFixedHeight(QWIDGETSIZE_MAX);
  }
 }

} // namespace Artifact
