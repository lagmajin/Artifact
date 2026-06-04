module;
#include <utility>
#include <QAction>
#include <QIcon>
#include <QWidget>
#include <QMenu>
#include <QKeySequence>
#include <wobjectimpl.h>

module Artifact.Menu.Effect;
import std;

import Event.Bus;
import Artifact.Event.Types;
import Artifact.Service.Effect;
import Artifact.Service.Project;
import Utils.Id;
import Utils.Path;

namespace Artifact
{
namespace {
QIcon menuIcon(const QString& path)
{
  return QIcon(resolveIconPath(path));
}

QAction* addObsoleteEffect(QMenu* menu, const QString& label)
{
  QAction* action = menu->addAction(menuIcon(QStringLiteral("Studio/obsolete.svg")), label);
  return action;
}
}

 W_OBJECT_IMPL(ArtifactEffectMenu)

 class ArtifactEffectMenu::Impl
 {
 private:
  
 public:
  Impl(ArtifactEffectMenu*menu);
  ~Impl();
  
  ArtifactEffectMenu* menu_ = nullptr;
  ArtifactCore::LayerID selectedLayerId_;
  std::vector<ArtifactCore::EventBus::Subscription> eventBusSubscriptions_;

  QAction* inspectorAction_ = nullptr;
  QAction* removeAllAction_ = nullptr;
  QMenu* keyframeAssistantMenu_ = nullptr;
  QMenu* expressionControlsMenu_ = nullptr;
  QMenu* channelMenu_ = nullptr;
  QMenu* stylizeMenu_ = nullptr;
  QMenu* colorCorrectionMenu_ = nullptr;
  QMenu* distortMenu_ = nullptr;
  QMenu* blurSharpenMenu_ = nullptr;
  QMenu* noiseGrainMenu_ = nullptr;
  QMenu* transitionMenu_ = nullptr;
  QMenu* utilityMenu_ = nullptr;
  QMenu* matteMenu_ = nullptr;
  QMenu* perspectiveMenu_ = nullptr;
  QMenu* keyingMenu_ = nullptr;
  QMenu* obsoleteMenu_ = nullptr;

  void refreshEnabledState();
 };

 ArtifactEffectMenu::Impl::Impl(ArtifactEffectMenu*menu) : menu_(menu)
{
  inspectorAction_ = new QAction("エフェクトコントロール", menu);
  inspectorAction_->setShortcut(QKeySequence(Qt::Key_F3));
  inspectorAction_->setIcon(menuIcon(QStringLiteral("Studio/inspector.svg")));

  removeAllAction_ = new QAction("すべてを削除", menu);
  removeAllAction_->setShortcut(QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_X));
  removeAllAction_->setIcon(menuIcon(QStringLiteral("Studio/remove_all.svg")));

 // Keyframe Assistant submenu
  keyframeAssistantMenu_ = new QMenu("キーフレーム補助(&K)", menu);
  keyframeAssistantMenu_->setIcon(menuIcon(QStringLiteral("Studio/tune.svg")));
  keyframeAssistantMenu_->addAction(menuIcon(QStringLiteral("Studio/show_chart.svg")), "イージーイーズ");
  keyframeAssistantMenu_->addAction(menuIcon(QStringLiteral("Studio/trending_up.svg")), "イージーイーズイン");
  keyframeAssistantMenu_->addAction(menuIcon(QStringLiteral("Studio/trending_down.svg")), "イージーイーズアウト");
  keyframeAssistantMenu_->addAction(menuIcon(QStringLiteral("Studio/show_chart.svg")), "指数スケール");
  keyframeAssistantMenu_->addAction(menuIcon(QStringLiteral("Studio/linear_scale.svg")), "レイヤーを順番に配置");
  keyframeAssistantMenu_->addAction(menuIcon(QStringLiteral("Studio/replay.svg")), "時間反転キーフレーム");
  keyframeAssistantMenu_->addAction(menuIcon(QStringLiteral("Studio/audiotrack.svg")), "オーディオをキーフレームに変換");
  keyframeAssistantMenu_->addAction(menuIcon(QStringLiteral("Studio/code.svg")), "エクスプレッションをキーフレームに変換");

  // Expression Controls submenu
  expressionControlsMenu_ = new QMenu("エクスプレッション制御(&E)", menu);
  expressionControlsMenu_->setIcon(menuIcon(QStringLiteral("Studio/functions.svg")));
  expressionControlsMenu_->addAction(menuIcon(QStringLiteral("Studio/redo.svg")), "角度制御");
  expressionControlsMenu_->addAction(menuIcon(QStringLiteral("Studio/check.svg")), "チェックボックス制御");
  expressionControlsMenu_->addAction(menuIcon(QStringLiteral("Studio/palette.svg")), "カラー制御");
  expressionControlsMenu_->addAction(menuIcon(QStringLiteral("Studio/layers.svg")), "レイヤー制御");
  expressionControlsMenu_->addAction(menuIcon(QStringLiteral("Studio/near_me.svg")), "ポイント制御");
  expressionControlsMenu_->addAction(menuIcon(QStringLiteral("Studio/tune.svg")), "スライダー制御");

  // Channel submenu
  channelMenu_ = new QMenu("チャンネル(&C)", menu);
  channelMenu_->setIcon(menuIcon(QStringLiteral("Studio/linear_scale.svg")));
  channelMenu_->addAction(menuIcon(QStringLiteral("Studio/visibility.svg")), "マット設定");
  channelMenu_->addAction(menuIcon(QStringLiteral("Studio/functions.svg")), "算術");
  channelMenu_->addAction(menuIcon(QStringLiteral("Studio/layers.svg")), "ブレンド");
  channelMenu_->addAction(menuIcon(QStringLiteral("Studio/invert_colors.svg")), "反転");
  channelMenu_->addAction(menuIcon(QStringLiteral("Studio/tune.svg")), "ミニマックス");
  channelMenu_->addAction(menuIcon(QStringLiteral("Studio/merge_type.svg")), "チャンネルコンバイナー");

  // Stylize submenu
  stylizeMenu_ = new QMenu("スタイライズ(&S)", menu);
  stylizeMenu_->setIcon(menuIcon(QStringLiteral("Studio/auto_awesome.svg")));
  stylizeMenu_->addAction(menuIcon(QStringLiteral("Studio/vignette.svg")), "ドロップシャドウ");
  stylizeMenu_->addAction(menuIcon(QStringLiteral("Studio/wb_sunny.svg")), "グロー");
  stylizeMenu_->addAction(menuIcon(QStringLiteral("Studio/draw.svg")), "ベベルアルファ");
  stylizeMenu_->addAction(menuIcon(QStringLiteral("Studio/grid_view.svg")), "ベベルエッジ");

  // Color Correction submenu
  colorCorrectionMenu_ = new QMenu("カラー補正(&C)", menu);
  colorCorrectionMenu_->setIcon(menuIcon(QStringLiteral("Studio/palette.svg")));
  colorCorrectionMenu_->addAction(menuIcon(QStringLiteral("Studio/brightness_6.svg")), "輝度＆コントラスト");
  colorCorrectionMenu_->addAction(menuIcon(QStringLiteral("Studio/show_chart.svg")), "トーンカーブ");
  colorCorrectionMenu_->addAction(menuIcon(QStringLiteral("Studio/colorize.svg")), "Tint");
  colorCorrectionMenu_->addAction(menuIcon(QStringLiteral("Studio/photo_filter.svg")), "Photo Filter");
  colorCorrectionMenu_->addAction(menuIcon(QStringLiteral("Studio/tonality.svg")), "Gradient Ramp");
  colorCorrectionMenu_->addAction(menuIcon(QStringLiteral("Studio/palette.svg")), "Fill");
  colorCorrectionMenu_->addAction(menuIcon(QStringLiteral("Studio/hdr_on.svg")), "色相/彩度");
  colorCorrectionMenu_->addAction(menuIcon(QStringLiteral("Studio/palette.svg")), "Color Wheels");
  colorCorrectionMenu_->addAction(menuIcon(QStringLiteral("Studio/show_chart.svg")), "Curves");
  colorCorrectionMenu_->addAction(menuIcon(QStringLiteral("Studio/tonality.svg")), "Tritone");
  colorCorrectionMenu_->addAction(menuIcon(QStringLiteral("Studio/palette.svg")), "Colorama");
  colorCorrectionMenu_->addAction(menuIcon(QStringLiteral("Studio/palette.svg")), "Color Balance");
  colorCorrectionMenu_->addAction(menuIcon(QStringLiteral("Studio/tune.svg")), "Levels");
  colorCorrectionMenu_->addAction(menuIcon(QStringLiteral("Studio/merge_type.svg")), "Channel Mixer");
  colorCorrectionMenu_->addAction(menuIcon(QStringLiteral("Studio/colorize.svg")), "Selective Color");
  colorCorrectionMenu_->addAction(menuIcon(QStringLiteral("Studio/tune.svg")), "レベル補正");
  colorCorrectionMenu_->addAction(menuIcon(QStringLiteral("Studio/palette.svg")), "カラーバランス");
  colorCorrectionMenu_->addAction(menuIcon(QStringLiteral("Studio/photo_filter.svg")), "写真フィルター");

  // Distort submenu
  distortMenu_ = new QMenu("ディストーション(&D)", menu);
  distortMenu_->setIcon(menuIcon(QStringLiteral("Studio/transform.svg")));
  distortMenu_->addAction(menuIcon(QStringLiteral("Studio/blur_on.svg")), "バルジ");
  distortMenu_->addAction(menuIcon(QStringLiteral("Studio/crop.svg")), "コーナーピン");
  distortMenu_->addAction(menuIcon(QStringLiteral("Studio/zoom_in.svg")), "レンズ補正");
  distortMenu_->addAction(menuIcon(QStringLiteral("Studio/search.svg")), "マグニファイ");
  distortMenu_->addAction(menuIcon(QStringLiteral("Studio/grid_on.svg")), "メッシュワープ");
  distortMenu_->addAction(menuIcon(QStringLiteral("Studio/tune.svg")), "オプティクス補償");
  distortMenu_->addAction(menuIcon(QStringLiteral("Studio/transform.svg")), "トランスフォーム");
  distortMenu_->addAction(menuIcon(QStringLiteral("Studio/swirl.svg")), "タービュレントディスプレイス");
  distortMenu_->addAction(menuIcon(QStringLiteral("Studio/swirl.svg")), "トワール");
  distortMenu_->addAction(menuIcon(QStringLiteral("Studio/transform.svg")), "ワープ");
  distortMenu_->addAction(menuIcon(QStringLiteral("Studio/swirl.svg")), "波形ワープ");
  distortMenu_->addAction(menuIcon(QStringLiteral("Studio/camera_alt.svg")), "レンズディストーション");

  // Blur & Sharpen submenu
  blurSharpenMenu_ = new QMenu("ブラー＆シャープ(&B)", menu);
  blurSharpenMenu_->setIcon(menuIcon(QStringLiteral("Studio/blur_on.svg")));
  blurSharpenMenu_->addAction(menuIcon(QStringLiteral("Studio/blur_on.svg")), "ブラー (ガウス)");
  blurSharpenMenu_->addAction(menuIcon(QStringLiteral("Studio/blur_on.svg")), "ブラー (滑らか)");
  blurSharpenMenu_->addAction(menuIcon(QStringLiteral("Studio/linear_scale.svg")), "ブラー (方向)");
  blurSharpenMenu_->addAction(menuIcon(QStringLiteral("Studio/filter_center_focus.svg")), "ブラー (放射状)");
  blurSharpenMenu_->addAction(menuIcon(QStringLiteral("Studio/auto_fix_high.svg")), "シャープ");
  blurSharpenMenu_->addAction(menuIcon(QStringLiteral("Studio/auto_fix_high.svg")), "アンシャープマスク");

  // Noise & Grain submenu
  noiseGrainMenu_ = new QMenu("Noise & Grain", menu);
  noiseGrainMenu_->setIcon(menuIcon(QStringLiteral("Studio/graphic_eq.svg")));
  noiseGrainMenu_->addAction(menuIcon(QStringLiteral("Studio/graphic_eq.svg")), "Add Grain");
  noiseGrainMenu_->addAction(menuIcon(QStringLiteral("Studio/brush.svg")), "Dust & Scratches");
  noiseGrainMenu_->addAction(menuIcon(QStringLiteral("Studio/auto_awesome.svg")), "Fractal Noise");
  noiseGrainMenu_->addAction(menuIcon(QStringLiteral("Studio/tune.svg")), "Median");
  noiseGrainMenu_->addAction(menuIcon(QStringLiteral("Studio/graphic_eq.svg")), "Noise");
  noiseGrainMenu_->addAction(menuIcon(QStringLiteral("Studio/hdr_on.svg")), "Noise HLS");
  noiseGrainMenu_->addAction(menuIcon(QStringLiteral("Studio/hdr_on.svg")), "Noise HLS Auto");
  noiseGrainMenu_->addAction(menuIcon(QStringLiteral("Studio/auto_fix_high.svg")), "Remove Grain");

  // Transition submenu
  transitionMenu_ = new QMenu("Transition", menu);
  transitionMenu_->setIcon(menuIcon(QStringLiteral("Studio/linear_scale.svg")));
  transitionMenu_->addAction(menuIcon(QStringLiteral("Studio/straighten.svg")), "Linear Wipe");
  transitionMenu_->addAction(menuIcon(QStringLiteral("Studio/filter_center_focus.svg")), "Radial Wipe");
  transitionMenu_->addAction(menuIcon(QStringLiteral("Studio/view_sidebar.svg")), "Venetian Blinds");
  transitionMenu_->addAction(menuIcon(QStringLiteral("Studio/tonality.svg")), "Dissolve");
  transitionMenu_->addAction(menuIcon(QStringLiteral("Studio/arrow_right.svg")), "Fade In");
  transitionMenu_->addAction(menuIcon(QStringLiteral("Studio/arrow_left.svg")), "Fade Out");

  // Utility submenu
  utilityMenu_ = new QMenu("Utility", menu);
  utilityMenu_->setIcon(menuIcon(QStringLiteral("Studio/settings.svg")));
  utilityMenu_->addAction(menuIcon(QStringLiteral("Studio/palette.svg")), "Apply Color LUT");
  utilityMenu_->addAction(menuIcon(QStringLiteral("Studio/tonality.svg")), "Change Colorspace");
  utilityMenu_->addAction(menuIcon(QStringLiteral("Studio/palette.svg")), "Change To Color");
  utilityMenu_->addAction(menuIcon(QStringLiteral("Studio/link.svg")), "Color Link");
  utilityMenu_->addAction(menuIcon(QStringLiteral("Studio/grid_on.svg")), "Displacement Map");
  utilityMenu_->addAction(menuIcon(QStringLiteral("Studio/vignette.svg")), "Drop Shadow");
  utilityMenu_->addAction(menuIcon(QStringLiteral("Studio/content_cut.svg")), "Extract");
  utilityMenu_->addAction(menuIcon(QStringLiteral("Studio/wb_sunny.svg")), "Glow");
  utilityMenu_->addAction(menuIcon(QStringLiteral("Studio/hdr_on.svg")), "HDR Tonemap");
  utilityMenu_->addAction(menuIcon(QStringLiteral("Studio/hdr_on.svg")), "HDR Highlight Compression");
  utilityMenu_->addAction(menuIcon(QStringLiteral("Studio/tune.svg")), "Levels (Individual Controls)");
  utilityMenu_->addAction(menuIcon(QStringLiteral("Studio/aspect_ratio.svg")), "Null Object");
  utilityMenu_->addAction(menuIcon(QStringLiteral("Studio/blur_on.svg")), "Particle Playground");
  utilityMenu_->addAction(menuIcon(QStringLiteral("Studio/colorize.svg")), "White Balance");
  utilityMenu_->addAction(menuIcon(QStringLiteral("Studio/edit.svg")), "Write-on");

  // Matte submenu
  matteMenu_ = new QMenu("Matte", menu);
  matteMenu_->setIcon(menuIcon(QStringLiteral("Studio/visibility.svg")));
  matteMenu_->addAction(menuIcon(QStringLiteral("Studio/tune.svg")), "Matte Choker");
  matteMenu_->addAction(menuIcon(QStringLiteral("Studio/tune.svg")), "Simple Choker");
  matteMenu_->addAction(menuIcon(QStringLiteral("Studio/opacity.svg")), "Alpha Levels");
  matteMenu_->addAction(menuIcon(QStringLiteral("Studio/opacity.svg")), "Alpha Matte");
  matteMenu_->addAction(menuIcon(QStringLiteral("Studio/contrast.svg")), "Difference Matte");
  matteMenu_->addAction(menuIcon(QStringLiteral("Studio/contrast.svg")), "Luma Matte");
  matteMenu_->addAction(menuIcon(QStringLiteral("Studio/filter_center_focus.svg")), "Track Matte");

  // Perspective submenu
  perspectiveMenu_ = new QMenu("Perspective", menu);
  perspectiveMenu_->setIcon(menuIcon(QStringLiteral("Studio/view_in_ar.svg")));
  perspectiveMenu_->addAction(menuIcon(QStringLiteral("Studio/grid_view.svg")), "Bevel Alpha");
  perspectiveMenu_->addAction(menuIcon(QStringLiteral("Studio/grid_view.svg")), "Bevel Edges");
  perspectiveMenu_->addAction(menuIcon(QStringLiteral("Studio/vignette.svg")), "Drop Shadow");

  // Keying submenu
  keyingMenu_ = new QMenu("Keying", menu);
  keyingMenu_->setIcon(menuIcon(QStringLiteral("Studio/check_circle.svg")));
  keyingMenu_->addAction(menuIcon(QStringLiteral("Studio/tonality.svg")), "Chroma Key");
  keyingMenu_->addAction(menuIcon(QStringLiteral("Studio/palette.svg")), "Color Key");
  keyingMenu_->addAction(menuIcon(QStringLiteral("Studio/brightness_6.svg")), "Luma Key");

  // Obsolete submenu
  obsoleteMenu_ = new QMenu("Obsolete", menu);
  obsoleteMenu_->setIcon(menuIcon(QStringLiteral("Studio/history.svg")));
  obsoleteMenu_->addAction(menuIcon(QStringLiteral("Studio/view_sidebar.svg")), "3D Glasses");
  obsoleteMenu_->addAction(menuIcon(QStringLiteral("Studio/audiotrack.svg")), "Audio Spectrum");
  obsoleteMenu_->addAction(menuIcon(QStringLiteral("Studio/graphic_eq.svg")), "Audio Waveform");
  addObsoleteEffect(obsoleteMenu_, QStringLiteral("Backlight"));
  addObsoleteEffect(obsoleteMenu_, QStringLiteral("Beam"));
  addObsoleteEffect(obsoleteMenu_, QStringLiteral("Block Dissolve"));
  addObsoleteEffect(obsoleteMenu_, QStringLiteral("Card Dance"));
  addObsoleteEffect(obsoleteMenu_, QStringLiteral("Card Wipe"));
  addObsoleteEffect(obsoleteMenu_, QStringLiteral("Caustics"));
  addObsoleteEffect(obsoleteMenu_, QStringLiteral("Channel Combiner"));
  addObsoleteEffect(obsoleteMenu_, QStringLiteral("Checkerboard"));
  addObsoleteEffect(obsoleteMenu_, QStringLiteral("Compound Arithmetic"));
  addObsoleteEffect(obsoleteMenu_, QStringLiteral("Cylinder"));
  addObsoleteEffect(obsoleteMenu_, QStringLiteral("Displacement Map (Obsolete)"));
  addObsoleteEffect(obsoleteMenu_, QStringLiteral("Exploding Text"));
  addObsoleteEffect(obsoleteMenu_, QStringLiteral("Flag"));
  addObsoleteEffect(obsoleteMenu_, QStringLiteral("Fractal"));
  addObsoleteEffect(obsoleteMenu_, QStringLiteral("Glass"));
  addObsoleteEffect(obsoleteMenu_, QStringLiteral("Glow (Obsolete)"));
  addObsoleteEffect(obsoleteMenu_, QStringLiteral("Gradient Ramp (Obsolete)"));
  addObsoleteEffect(obsoleteMenu_, QStringLiteral("Grid"));
  addObsoleteEffect(obsoleteMenu_, QStringLiteral("Lens Flare (Obsolete)"));
  addObsoleteEffect(obsoleteMenu_, QStringLiteral("Lightning"));
  addObsoleteEffect(obsoleteMenu_, QStringLiteral("Motion Tile"));
  addObsoleteEffect(obsoleteMenu_, QStringLiteral("Page Turn"));
  addObsoleteEffect(obsoleteMenu_, QStringLiteral("Paint Bucket"));
  addObsoleteEffect(obsoleteMenu_, QStringLiteral("Particle Playground (Obsolete)"));
  addObsoleteEffect(obsoleteMenu_, QStringLiteral("Polar Coordinates"));
  addObsoleteEffect(obsoleteMenu_, QStringLiteral("Radio Waves"));
  addObsoleteEffect(obsoleteMenu_, QStringLiteral("Ramp"));
  addObsoleteEffect(obsoleteMenu_, QStringLiteral("Ripple"));
  addObsoleteEffect(obsoleteMenu_, QStringLiteral("Scatter"));
  addObsoleteEffect(obsoleteMenu_, QStringLiteral("Shatter"));
  addObsoleteEffect(obsoleteMenu_, QStringLiteral("Spherize"));
  addObsoleteEffect(obsoleteMenu_, QStringLiteral("Texturize"));
  addObsoleteEffect(obsoleteMenu_, QStringLiteral("Threshold"));
  addObsoleteEffect(obsoleteMenu_, QStringLiteral("Timecode"));
  addObsoleteEffect(obsoleteMenu_, QStringLiteral("Tritone"));
  addObsoleteEffect(obsoleteMenu_, QStringLiteral("Vegas"));
  addObsoleteEffect(obsoleteMenu_, QStringLiteral("Wave"));
  addObsoleteEffect(obsoleteMenu_, QStringLiteral("Wave World"));
  addObsoleteEffect(obsoleteMenu_, QStringLiteral("Wood"));

  // Add actions to menu
  menu->addAction(inspectorAction_);
  menu->addSeparator();
  menu->addAction(removeAllAction_);
  menu->addSeparator();
  menu->addMenu(keyframeAssistantMenu_);
  menu->addMenu(expressionControlsMenu_);
  menu->addMenu(channelMenu_);
  menu->addSeparator();
  menu->addMenu(stylizeMenu_);
  menu->addMenu(colorCorrectionMenu_);
  menu->addMenu(distortMenu_);
  menu->addMenu(blurSharpenMenu_);
  menu->addMenu(noiseGrainMenu_);
  menu->addMenu(transitionMenu_);
  menu->addMenu(utilityMenu_);
  menu->addMenu(matteMenu_);
  menu->addMenu(keyingMenu_);
  menu->addMenu(perspectiveMenu_);
  menu->addMenu(obsoleteMenu_);
  
  auto& eventBus = ArtifactCore::globalEventBus();
  eventBusSubscriptions_.push_back(
      eventBus.subscribe<LayerSelectionChangedEvent>(
          [this](const LayerSelectionChangedEvent& event) {
              const ArtifactCore::LayerID layerId(event.layerId);
              if (!event.compositionId.isEmpty()) {
                  auto* service = ArtifactProjectService::instance();
                  if (service) {
                      if (const auto comp = service->currentComposition().lock()) {
                          if (comp->id().toString() != event.compositionId) {
                              return;
                          }
                      }
                  }
              }
              selectedLayerId_ = layerId;
              refreshEnabledState();
          }));
  eventBusSubscriptions_.push_back(
      eventBus.subscribe<LayerChangedEvent>(
          [this](const LayerChangedEvent& event) {
              if (!event.compositionId.isEmpty()) {
                  auto* service = ArtifactProjectService::instance();
                  if (service) {
                      if (const auto comp = service->currentComposition().lock()) {
                          if (comp->id().toString() != event.compositionId) {
                              return;
                          }
                      }
                  }
              }
              if (event.changeType == LayerChangedEvent::ChangeType::Removed &&
                  selectedLayerId_ == ArtifactCore::LayerID(event.layerId)) {
                  selectedLayerId_ = {};
              }
              refreshEnabledState();
          }));
  eventBusSubscriptions_.push_back(
      eventBus.subscribe<ProjectChangedEvent>(
          [this](const ProjectChangedEvent&) {
              refreshEnabledState();
          }));
  QObject::connect(menu, &QMenu::aboutToShow, menu, [this]() {
      refreshEnabledState();
  });
 }

 ArtifactEffectMenu::Impl::~Impl()
 {

 }

 void ArtifactEffectMenu::Impl::refreshEnabledState()
 {
     auto* service = ArtifactProjectService::instance();
     bool hasLayer = service && service->hasProject() && static_cast<bool>(service->currentComposition().lock()) && !selectedLayerId_.isNil();
     
     inspectorAction_->setEnabled(hasLayer);
     removeAllAction_->setEnabled(hasLayer);
     keyframeAssistantMenu_->setEnabled(hasLayer);
     expressionControlsMenu_->setEnabled(hasLayer);
     channelMenu_->setEnabled(hasLayer);
     stylizeMenu_->setEnabled(hasLayer);
     colorCorrectionMenu_->setEnabled(hasLayer);
     distortMenu_->setEnabled(hasLayer);
     blurSharpenMenu_->setEnabled(hasLayer);
     noiseGrainMenu_->setEnabled(hasLayer);
     transitionMenu_->setEnabled(hasLayer);
     utilityMenu_->setEnabled(hasLayer);
     matteMenu_->setEnabled(hasLayer);
     perspectiveMenu_->setEnabled(hasLayer);
     keyingMenu_->setEnabled(hasLayer);
     obsoleteMenu_->setEnabled(hasLayer);
 }

 ArtifactEffectMenu::ArtifactEffectMenu(QWidget* parent /*= nullptr*/):QMenu(parent),impl_(new Impl(this))
 {
  setTitle("エフェクト(&T)");
  setIcon(menuIcon(QStringLiteral("Studio/auto_awesome.svg")));
  setTearOffEnabled(false);

  // Menu is built in Impl constructor
  impl_->refreshEnabledState();
 }

 ArtifactEffectMenu::~ArtifactEffectMenu()
 {
  delete impl_;
 }



};
