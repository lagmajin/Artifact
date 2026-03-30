module;
#include <QAction>
#include <QToolBar>
#include <QActionGroup>
#include <QString>
#include <QStringList>
#include <QSize>
#include <QIcon>
#include <QFileInfo>
#include <QDebug>
#include <wobjectimpl.h>

module Widgets.ToolBar;
import Utils;
import Icon.SvgToIcon;
import Artifact.Tool.Manager;
import Artifact.Service.Application;

namespace {
QIcon loadIconWithFallback(const QString& fileName)
{
  const QString resourcePath = ArtifactCore::resolveIconResourcePath(fileName);
  {
    const QIcon icon = ArtifactCore::svgToQIcon(resourcePath, QSize(32, 32));
    if (!icon.isNull()) {
      return icon;
    }
  }

  const QString filePath = ArtifactCore::resolveIconPath(fileName);
  {
    const QIcon fileIcon = ArtifactCore::svgToQIcon(filePath, QSize(32, 32));
    if (!fileIcon.isNull()) {
      return fileIcon;
    }
  }

  qWarning().noquote() << "[ArtifactToolBar] icon load failed:"
                       << "resource=" << resourcePath
                       << "exists=" << QFileInfo::exists(resourcePath)
                       << "file=" << filePath
                       << "exists=" << QFileInfo::exists(filePath);
  return QIcon();
}

QIcon loadIconWithFallback(const QStringList& fileNames)
{
  for (const auto& fileName : fileNames) {
    const QIcon icon = loadIconWithFallback(fileName);
    if (!icon.isNull()) {
      return icon;
    }
  }
  return QIcon();
}
}

namespace Artifact
{

 W_OBJECT_IMPL(ArtifactToolBar)

 class ArtifactToolBar::Impl {
 public:
  Impl();
  ~Impl();
  
  // Tool actions
  QActionGroup* toolsGroup_ = nullptr;
  QAction* homeAction_ = nullptr;
  QAction* selectTool_ = nullptr;
  QAction* handTool_ = nullptr;
  QAction* zoomTool_ = nullptr;
  QAction* moveTool_ = nullptr;
  QAction* rotationTool_ = nullptr;
  QAction* scaleTool_ = nullptr;
  QAction* cameraTool_ = nullptr;
  QAction* panBehindTool_ = nullptr;
  QAction* shapeTool_ = nullptr;
  QAction* penTool_ = nullptr;
  QAction* textTool_ = nullptr;
  QAction* brushTool_ = nullptr;
  QAction* cloneStampTool_ = nullptr;
  QAction* eraserTool_ = nullptr;
  QAction* puppetTool_ = nullptr;
  
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
  setToolButtonStyle(Qt::ToolButtonIconOnly);
  setMovable(false);
  setFloatable(false);
  
  impl_->toolsGroup_ = new QActionGroup(this);
  impl_->toolsGroup_->setExclusive(true);

  auto createTool = [this](QAction*& action, const QStringList& iconCandidates, const QString& text, const QString& tooltip, const QKeySequence& shortcut) {
    action = new QAction(this);
    action->setIcon(loadIconWithFallback(iconCandidates));
    action->setText(text);
    action->setToolTip(tooltip);
    action->setShortcut(shortcut);
    action->setCheckable(true);
    impl_->toolsGroup_->addAction(action);
    addAction(action);
  };

  // Main tool actions
  impl_->homeAction_ = new QAction(this);
  impl_->homeAction_->setIcon(loadIconWithFallback(QStringList{
    QStringLiteral("MaterialVS/colored/E3E3E3/start.svg"),
    QStringLiteral("MaterialVS/neutral/start.svg"),
    QStringLiteral("Png/home.png")
  }));
  impl_->homeAction_->setToolTip("ホーム");
  addAction(impl_->homeAction_);
  
  addSeparator();

  createTool(impl_->selectTool_, QStringList{
      QStringLiteral("MaterialVS/neutral/arrow_right.svg"),
      QStringLiteral("Material/arrow_right.svg")
    }, "選択", "選択ツール (V)", QKeySequence(Qt::Key_V));
  createTool(impl_->handTool_, QStringList{
      QStringLiteral("MaterialVS/neutral/pan_tool_alt.svg"),
      QStringLiteral("Material/pan_tool_alt.svg")
    }, "手のひら", "手のひらツール (H)", QKeySequence(Qt::Key_H));
  createTool(impl_->zoomTool_, QStringList{
      QStringLiteral("MaterialVS/neutral/zoom_in.svg"),
      QStringLiteral("Material/zoom_in.svg")
    }, "ズーム", "ズームツール (Z)", QKeySequence(Qt::Key_Z));
  createTool(impl_->moveTool_, QStringList{
      QStringLiteral("MaterialVS/neutral/arrow_right.svg"),
      QStringLiteral("Material/arrow_right.svg")
    }, "移動", "移動ツール (W)", QKeySequence(Qt::Key_W));
  createTool(impl_->rotationTool_, QStringList{
      QStringLiteral("MaterialVS/neutral/transform.svg"),
      QStringLiteral("Material/transform.svg")
    }, "回転", "回転ツール (E)", QKeySequence(Qt::Key_E));
  createTool(impl_->scaleTool_, QStringList{
      QStringLiteral("MaterialVS/neutral/zoom_in.svg"),
      QStringLiteral("Material/zoom_in.svg")
    }, "スケール", "スケールツール (R)", QKeySequence(Qt::Key_R));
  createTool(impl_->cameraTool_, QStringList{
      QStringLiteral("MaterialVS/neutral/camera_alt.svg"),
      QStringLiteral("Material/camera_alt.svg")
    }, "カメラ", "統合カメラーツール (C)", QKeySequence(Qt::Key_C));
  createTool(impl_->panBehindTool_, QStringList{
      QStringLiteral("MaterialVS/neutral/push_pin.svg"),
      QStringLiteral("Material/push_pin.svg")
    }, "アンカー", "アンカーポイントツール (Y)", QKeySequence(Qt::Key_Y));
  
  addSeparator();
  
  createTool(impl_->shapeTool_, QStringList{
      QStringLiteral("MaterialVS/neutral/crop.svg"),
      QStringLiteral("Material/crop.svg")
    }, "シェイプ", "シェイプツール (Q)", QKeySequence(Qt::Key_Q));
  createTool(impl_->penTool_, QStringList{
      QStringLiteral("MaterialVS/neutral/draw.svg"),
      QStringLiteral("Material/draw.svg")
    }, "ペン", "ペンツール (G)", QKeySequence(Qt::Key_G));
  createTool(impl_->textTool_, QStringList{
      QStringLiteral("MaterialVS/neutral/title.svg"),
      QStringLiteral("Material/title.svg")
    }, "テキスト", "横書き文字ツール (Ctrl+T)", QKeySequence(Qt::CTRL | Qt::Key_T));
  createTool(impl_->brushTool_, QStringList{
      QStringLiteral("MaterialVS/neutral/brush.svg"),
      QStringLiteral("Material/brush.svg")
    }, "ブラシ", "ブラシツール (Ctrl+B)", QKeySequence(Qt::CTRL | Qt::Key_B));
  createTool(impl_->cloneStampTool_, QStringList{
      QStringLiteral("MaterialVS/neutral/content_copy.svg"),
      QStringLiteral("Material/content_copy.svg")
    }, "コピースタンプ", "コピースタンプツール (Ctrl+B)", QKeySequence());
  createTool(impl_->eraserTool_, QStringList{
      QStringLiteral("MaterialVS/neutral/delete.svg"),
      QStringLiteral("Material/delete.svg")
    }, "消しゴム", "消しゴムツール (Ctrl+B)", QKeySequence());
  createTool(impl_->puppetTool_, QStringList{
      QStringLiteral("MaterialVS/neutral/push_pin.svg"),
      QStringLiteral("Material/push_pin.svg")
    }, "パペット", "パペットピンツール (Ctrl+P)", QKeySequence(Qt::CTRL | Qt::Key_P));
  
  // Set default tool
  impl_->selectTool_->setChecked(true);
  
  // Separator
  addSeparator();
  
  // Zoom actions
  impl_->zoomInAction_ = new QAction("+");
  impl_->zoomInAction_->setIcon(loadIconWithFallback("Material/zoom_in.svg"));
  impl_->zoomInAction_->setToolTip("Zoom In (Ctrl++)");
  impl_->zoomInAction_->setIconText("Zoom In");
  
  impl_->zoomOutAction_ = new QAction("-");
  impl_->zoomOutAction_->setIcon(loadIconWithFallback("Material/zoom_out.svg"));
  impl_->zoomOutAction_->setToolTip("Zoom Out (Ctrl+-)");
  impl_->zoomOutAction_->setIconText("Zoom Out");
  
  impl_->zoom100Action_ = new QAction("100%");
  impl_->zoom100Action_->setIcon(loadIconWithFallback("Material/aspect_ratio.svg"));
  impl_->zoom100Action_->setToolTip("Reset Zoom (Ctrl+0)");
  impl_->zoom100Action_->setIconText("100%");
  
  impl_->zoomFitAction_ = new QAction("Fit");
  impl_->zoomFitAction_->setIcon(loadIconWithFallback("Material/fit_screen.svg"));
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
  impl_->gridToggleAction_->setIcon(loadIconWithFallback("Material/grid_on.svg"));
  impl_->gridToggleAction_->setToolTip("Show Grid");
  impl_->gridToggleAction_->setIconText("Grid");
  
  impl_->guideToggleAction_ = new QAction("Guide");
  impl_->guideToggleAction_->setCheckable(true);
  impl_->guideToggleAction_->setChecked(true);
  impl_->guideToggleAction_->setIcon(loadIconWithFallback("Material/visibility.svg"));
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
  impl_->normalViewAction_->setIcon(loadIconWithFallback("Material/view_sidebar.svg"));
  impl_->normalViewAction_->setToolTip("Normal View");
  impl_->normalViewAction_->setIconText("Normal");
  
  impl_->gridViewAction_ = new QAction("Grid");
  impl_->gridViewAction_->setCheckable(true);
  impl_->gridViewAction_->setIcon(loadIconWithFallback("Material/grid_view.svg"));
  impl_->gridViewAction_->setToolTip("Grid View");
  impl_->gridViewAction_->setIconText("Grid");
  
  impl_->detailViewAction_ = new QAction("Detail");
  impl_->detailViewAction_->setCheckable(true);
  impl_->detailViewAction_->setIcon(loadIconWithFallback("Material/tune.svg"));
  impl_->detailViewAction_->setToolTip("Detail View");
  impl_->detailViewAction_->setIconText("Detail");
  
  impl_->viewModeGroup_->addAction(impl_->normalViewAction_);
  impl_->viewModeGroup_->addAction(impl_->gridViewAction_);
  impl_->viewModeGroup_->addAction(impl_->detailViewAction_);
  
  addAction(impl_->normalViewAction_);
  addAction(impl_->gridViewAction_);
  addAction(impl_->detailViewAction_);

  // Connect signals
  QObject::connect(impl_->homeAction_, &QAction::triggered, this, [this]() { homeRequested(); });
  QObject::connect(impl_->selectTool_, &QAction::triggered, this, [this]() { selectToolRequested(); });
  QObject::connect(impl_->handTool_, &QAction::triggered, this, [this]() { handToolRequested(); });
  QObject::connect(impl_->zoomTool_, &QAction::triggered, this, [this]() { zoomToolRequested(); });
  QObject::connect(impl_->moveTool_, &QAction::triggered, this, [this]() { moveToolRequested(); });
  QObject::connect(impl_->rotationTool_, &QAction::triggered, this, [this]() { rotationToolRequested(); });
  QObject::connect(impl_->scaleTool_, &QAction::triggered, this, [this]() { scaleToolRequested(); });
  QObject::connect(impl_->cameraTool_, &QAction::triggered, this, [this]() { cameraToolRequested(); });
  QObject::connect(impl_->panBehindTool_, &QAction::triggered, this, [this]() { panBehindToolRequested(); });
  QObject::connect(impl_->shapeTool_, &QAction::triggered, this, [this]() { shapeToolRequested(); });
  QObject::connect(impl_->penTool_, &QAction::triggered, this, [this]() { penToolRequested(); });
  QObject::connect(impl_->textTool_, &QAction::triggered, this, [this]() { textToolRequested(); });
  QObject::connect(impl_->brushTool_, &QAction::triggered, this, [this]() { brushToolRequested(); });
  QObject::connect(impl_->cloneStampTool_, &QAction::triggered, this, [this]() { cloneStampToolRequested(); });
  QObject::connect(impl_->eraserTool_, &QAction::triggered, this, [this]() { eraserToolRequested(); });
  QObject::connect(impl_->puppetTool_, &QAction::triggered, this, [this]() { puppetToolRequested(); });
   
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
   impl_->gridToggleAction_->setIcon(loadIconWithFallback(checked ? "Material/grid_on.svg" : "Material/grid_off.svg"));
   gridToggled(checked);
  });
   
  QObject::connect(impl_->guideToggleAction_, &QAction::triggered, this, [this](bool checked) {
   impl_->guideToggleAction_->setIcon(loadIconWithFallback(checked ? "Material/visibility.svg" : "Material/visibility_off.svg"));
   guideToggled(checked);
  });
  
  setStyleSheet(R"(
QToolButton {
    background: transparent;
    border: none;
    padding: 2px;
    margin: 2px;
    min-width: 28px;
    min-height: 28px;
}
QToolButton:hover {
    background: #3a3a3a;
    border-radius: 4px;
}
QToolButton:pressed {
    background: #094771;
    border-radius: 4px;
}
QToolButton:checked {
    background: #094771;
    border-radius: 4px;
}
QToolBar {
    background: #1E1E1E;
    border-bottom: 2px solid #0E0E0E;
    spacing: 2px;
}
QToolBar::separator {
    background: #3a3a3a;
    width: 1px;
    margin-top: 4px;
    margin-bottom: 4px;
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
