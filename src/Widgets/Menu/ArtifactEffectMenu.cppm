module;
#include <QAction>
#include <QIcon>
#include <QWidget>
#include <QMenu>
#include <QKeySequence>
#include <wobjectimpl.h>

module Artifact.Menu.Effect;
import std;

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

  removeAllAction_ = new QAction("すべてを削除", menu);
  removeAllAction_->setShortcut(QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_X));

 // Keyframe Assistant submenu
  keyframeAssistantMenu_ = new QMenu("キーフレーム補助(&K)", menu);
  keyframeAssistantMenu_->setIcon(menuIcon(QStringLiteral("Material/tune.svg")));
  keyframeAssistantMenu_->addAction(menuIcon(QStringLiteral("Material/show_chart.svg")), "イージーイーズ");
  keyframeAssistantMenu_->addAction(menuIcon(QStringLiteral("Material/trending_up.svg")), "イージーイーズイン");
  keyframeAssistantMenu_->addAction(menuIcon(QStringLiteral("Material/trending_down.svg")), "イージーイーズアウト");
  keyframeAssistantMenu_->addAction(menuIcon(QStringLiteral("Material/stacked_line_chart.svg")), "指数スケール");
  keyframeAssistantMenu_->addAction(menuIcon(QStringLiteral("Material/sort.svg")), "レイヤーを順番に配置");
  keyframeAssistantMenu_->addAction(menuIcon(QStringLiteral("Material/replay.svg")), "時間反転キーフレーム");
  keyframeAssistantMenu_->addAction(menuIcon(QStringLiteral("Material/audiotrack.svg")), "オーディオをキーフレームに変換");
  keyframeAssistantMenu_->addAction(menuIcon(QStringLiteral("Material/code.svg")), "エクスプレッションをキーフレームに変換");

  // Expression Controls submenu
  expressionControlsMenu_ = new QMenu("エクスプレッション制御(&E)", menu);
  expressionControlsMenu_->setIcon(menuIcon(QStringLiteral("Material/functions.svg")));
  expressionControlsMenu_->addAction(menuIcon(QStringLiteral("Material/rotate_right.svg")), "角度制御");
  expressionControlsMenu_->addAction(menuIcon(QStringLiteral("Material/check_box.svg")), "チェックボックス制御");
  expressionControlsMenu_->addAction(menuIcon(QStringLiteral("Material/palette.svg")), "カラー制御");
  expressionControlsMenu_->addAction(menuIcon(QStringLiteral("Material/layers.svg")), "レイヤー制御");
  expressionControlsMenu_->addAction(menuIcon(QStringLiteral("Material/fiber_manual_record.svg")), "ポイント制御");
  expressionControlsMenu_->addAction(menuIcon(QStringLiteral("Material/tune.svg")), "スライダー制御");

  // Channel submenu
  channelMenu_ = new QMenu("チャンネル(&C)", menu);
  channelMenu_->setIcon(menuIcon(QStringLiteral("Material/compare_arrows.svg")));
  channelMenu_->addAction(menuIcon(QStringLiteral("Material/mask.svg")), "マット設定");
  channelMenu_->addAction(menuIcon(QStringLiteral("Material/functions.svg")), "算術");
  channelMenu_->addAction(menuIcon(QStringLiteral("Material/layers.svg")), "ブレンド");
  channelMenu_->addAction(menuIcon(QStringLiteral("Material/flip.svg")), "反転");
  channelMenu_->addAction(menuIcon(QStringLiteral("Material/tune.svg")), "ミニマックス");
  channelMenu_->addAction(menuIcon(QStringLiteral("Material/merge_type.svg")), "チャンネルコンバイナー");

  // Stylize submenu
  stylizeMenu_ = new QMenu("スタイライズ(&S)", menu);
  stylizeMenu_->setIcon(menuIcon(QStringLiteral("Material/style.svg")));
  stylizeMenu_->addAction(menuIcon(QStringLiteral("Material/shadow.svg")), "ドロップシャドウ");
  stylizeMenu_->addAction(menuIcon(QStringLiteral("Material/wb_sunny.svg")), "グロー");
  stylizeMenu_->addAction(menuIcon(QStringLiteral("Material/draw.svg")), "ベベルアルファ");
  stylizeMenu_->addAction(menuIcon(QStringLiteral("Material/texture.svg")), "ベベルエッジ");

  // Color Correction submenu
  colorCorrectionMenu_ = new QMenu("カラー補正(&C)", menu);
  colorCorrectionMenu_->setIcon(menuIcon(QStringLiteral("Material/palette.svg")));
  colorCorrectionMenu_->addAction(menuIcon(QStringLiteral("Material/brightness_6.svg")), "輝度＆コントラスト");
  colorCorrectionMenu_->addAction(menuIcon(QStringLiteral("Material/show_chart.svg")), "トーンカーブ");
  colorCorrectionMenu_->addAction(menuIcon(QStringLiteral("Material/hdr_on.svg")), "色相/彩度");
  colorCorrectionMenu_->addAction(menuIcon(QStringLiteral("Material/tune.svg")), "レベル補正");
  colorCorrectionMenu_->addAction(menuIcon(QStringLiteral("Material/color_lens.svg")), "カラーバランス");
  colorCorrectionMenu_->addAction(menuIcon(QStringLiteral("Material/filter.svg")), "写真フィルター");

  // Distort submenu
  distortMenu_ = new QMenu("ディストーション(&D)", menu);
  distortMenu_->setIcon(menuIcon(QStringLiteral("Material/transform.svg")));
  distortMenu_->addAction(menuIcon(QStringLiteral("Material/lens_blur.svg")), "バルジ");
  distortMenu_->addAction(menuIcon(QStringLiteral("Material/crop.svg")), "コーナーピン");
  distortMenu_->addAction(menuIcon(QStringLiteral("Material/zoom_in.svg")), "レンズ補正");
  distortMenu_->addAction(menuIcon(QStringLiteral("Material/search.svg")), "マグニファイ");
  distortMenu_->addAction(menuIcon(QStringLiteral("Material/grid_on.svg")), "メッシュワープ");
  distortMenu_->addAction(menuIcon(QStringLiteral("Material/adjust.svg")), "オプティクス補償");
  distortMenu_->addAction(menuIcon(QStringLiteral("Material/transform.svg")), "トランスフォーム");
  distortMenu_->addAction(menuIcon(QStringLiteral("Material/waves.svg")), "タービュレントディスプレイス");
  distortMenu_->addAction(menuIcon(QStringLiteral("Material/tornado.svg")), "トワール");
  distortMenu_->addAction(menuIcon(QStringLiteral("Material/warp.svg")), "ワープ");
  distortMenu_->addAction(menuIcon(QStringLiteral("Material/waves.svg")), "波形ワープ");

  // Blur & Sharpen submenu
  blurSharpenMenu_ = new QMenu("ブラー＆シャープ(&B)", menu);
  blurSharpenMenu_->setIcon(menuIcon(QStringLiteral("Material/blur_on.svg")));
  blurSharpenMenu_->addAction(menuIcon(QStringLiteral("Material/blur_on.svg")), "ブラー (ガウス)");
  blurSharpenMenu_->addAction(menuIcon(QStringLiteral("Material/blur_circular.svg")), "ブラー (滑らか)");
  blurSharpenMenu_->addAction(menuIcon(QStringLiteral("Material/blur_linear.svg")), "ブラー (方向)");
  blurSharpenMenu_->addAction(menuIcon(QStringLiteral("Material/radial_blur.svg")), "ブラー (放射状)");
  blurSharpenMenu_->addAction(menuIcon(QStringLiteral("Material/sharpen.svg")), "シャープ");
  blurSharpenMenu_->addAction(menuIcon(QStringLiteral("Material/auto_fix_high.svg")), "アンシャープマスク");

  // Noise & Grain submenu
  noiseGrainMenu_ = new QMenu("Noise & Grain", menu);
  noiseGrainMenu_->setIcon(menuIcon(QStringLiteral("Material/grain.svg")));
  noiseGrainMenu_->addAction(menuIcon(QStringLiteral("Material/grain.svg")), "Add Grain");
  noiseGrainMenu_->addAction(menuIcon(QStringLiteral("Material/brush.svg")), "Dust & Scratches");
  noiseGrainMenu_->addAction(menuIcon(QStringLiteral("Material/auto_awesome.svg")), "Fractal Noise");
  noiseGrainMenu_->addAction(menuIcon(QStringLiteral("Material/tune.svg")), "Median");
  noiseGrainMenu_->addAction(menuIcon(QStringLiteral("Material/noise_control_off.svg")), "Noise");
  noiseGrainMenu_->addAction(menuIcon(QStringLiteral("Material/hdr_auto.svg")), "Noise HLS");
  noiseGrainMenu_->addAction(menuIcon(QStringLiteral("Material/hdr_auto_select.svg")), "Noise HLS Auto");
  noiseGrainMenu_->addAction(menuIcon(QStringLiteral("Material/cleaning_services.svg")), "Remove Grain");

  // Transition submenu
  transitionMenu_ = new QMenu("Transition", menu);
  transitionMenu_->setIcon(menuIcon(QStringLiteral("Material/compare_arrows.svg")));
  transitionMenu_->addAction(menuIcon(QStringLiteral("Material/straighten.svg")), "Linear Wipe");
  transitionMenu_->addAction(menuIcon(QStringLiteral("Material/circle.svg")), "Radial Wipe");
  transitionMenu_->addAction(menuIcon(QStringLiteral("Material/view_day.svg")), "Venetian Blinds");
  transitionMenu_->addAction(menuIcon(QStringLiteral("Material/fade.svg")), "Dissolve");
  transitionMenu_->addAction(menuIcon(QStringLiteral("Material/arrow_forward.svg")), "Fade In");
  transitionMenu_->addAction(menuIcon(QStringLiteral("Material/arrow_back.svg")), "Fade Out");

  // Utility submenu
  utilityMenu_ = new QMenu("Utility", menu);
  utilityMenu_->setIcon(menuIcon(QStringLiteral("Material/settings.svg")));
  utilityMenu_->addAction(menuIcon(QStringLiteral("Material/color_lens.svg")), "Apply Color LUT");
  utilityMenu_->addAction(menuIcon(QStringLiteral("Material/tonality.svg")), "Change Colorspace");
  utilityMenu_->addAction(menuIcon(QStringLiteral("Material/palette.svg")), "Change To Color");
  utilityMenu_->addAction(menuIcon(QStringLiteral("Material/link.svg")), "Color Link");
  utilityMenu_->addAction(menuIcon(QStringLiteral("Material/grid_on.svg")), "Displacement Map");
  utilityMenu_->addAction(menuIcon(QStringLiteral("Material/shadow.svg")), "Drop Shadow");
  utilityMenu_->addAction(menuIcon(QStringLiteral("Material/content_cut.svg")), "Extract");
  utilityMenu_->addAction(menuIcon(QStringLiteral("Material/wb_sunny.svg")), "Glow");
  utilityMenu_->addAction(menuIcon(QStringLiteral("Material/hdr_auto.svg")), "HDR Tonemap");
  utilityMenu_->addAction(menuIcon(QStringLiteral("Material/hdr_strong.svg")), "HDR Highlight Compression");
  utilityMenu_->addAction(menuIcon(QStringLiteral("Material/tune.svg")), "Levels (Individual Controls)");
  utilityMenu_->addAction(menuIcon(QStringLiteral("Material/crop_din.svg")), "Null Object");
  utilityMenu_->addAction(menuIcon(QStringLiteral("Material/blur_on.svg")), "Particle Playground");
  utilityMenu_->addAction(menuIcon(QStringLiteral("Material/tint.svg")), "Tint");
  utilityMenu_->addAction(menuIcon(QStringLiteral("Material/edit.svg")), "Write-on");

  // Matte submenu
  matteMenu_ = new QMenu("Matte", menu);
  matteMenu_->setIcon(menuIcon(QStringLiteral("Material/mask.svg")));
  matteMenu_->addAction(menuIcon(QStringLiteral("Material/tune.svg")), "Matte Choker");
  matteMenu_->addAction(menuIcon(QStringLiteral("Material/tune.svg")), "Simple Choker");
  matteMenu_->addAction(menuIcon(QStringLiteral("Material/opacity.svg")), "Alpha Levels");
  matteMenu_->addAction(menuIcon(QStringLiteral("Material/opacity.svg")), "Alpha Matte");
  matteMenu_->addAction(menuIcon(QStringLiteral("Material/compare.svg")), "Difference Matte");
  matteMenu_->addAction(menuIcon(QStringLiteral("Material/contrast.svg")), "Luma Matte");
  matteMenu_->addAction(menuIcon(QStringLiteral("Material/track_changes.svg")), "Track Matte");

  // Perspective submenu
  perspectiveMenu_ = new QMenu("Perspective", menu);
  perspectiveMenu_->setIcon(menuIcon(QStringLiteral("Material/view_in_ar.svg")));
  perspectiveMenu_->addAction(menuIcon(QStringLiteral("Material/texture.svg")), "Bevel Alpha");
  perspectiveMenu_->addAction(menuIcon(QStringLiteral("Material/texture.svg")), "Bevel Edges");
  perspectiveMenu_->addAction(menuIcon(QStringLiteral("Material/shadow.svg")), "Drop Shadow");

  // Keying submenu
  keyingMenu_ = new QMenu("Keying", menu);
  keyingMenu_->setIcon(menuIcon(QStringLiteral("Material/verified_user.svg")));
  keyingMenu_->addAction(menuIcon(QStringLiteral("Material/gradient.svg")), "Chroma Key");
  keyingMenu_->addAction(menuIcon(QStringLiteral("Material/palette.svg")), "Color Key");
  keyingMenu_->addAction(menuIcon(QStringLiteral("Material/brightness_6.svg")), "Luma Key");

  // Obsolete submenu
  obsoleteMenu_ = new QMenu("Obsolete", menu);
  obsoleteMenu_->setIcon(menuIcon(QStringLiteral("Material/history.svg")));
  obsoleteMenu_->addAction(menuIcon(QStringLiteral("Material/view_in_ar.svg")), "3D Glasses");
  obsoleteMenu_->addAction(menuIcon(QStringLiteral("Material/audiotrack.svg")), "Audio Spectrum");
  obsoleteMenu_->addAction(menuIcon(QStringLiteral("Material/waves.svg")), "Audio Waveform");
  obsoleteMenu_->addAction("Backlight");
  obsoleteMenu_->addAction("Beam");
  obsoleteMenu_->addAction("Block Dissolve");
  obsoleteMenu_->addAction("Card Dance");
  obsoleteMenu_->addAction("Card Wipe");
  obsoleteMenu_->addAction("Caustics");
  obsoleteMenu_->addAction("Channel Combiner");
  obsoleteMenu_->addAction("Checkerboard");
  obsoleteMenu_->addAction("Compound Arithmetic");
  obsoleteMenu_->addAction("Cylinder");
  obsoleteMenu_->addAction("Displacement Map (Obsolete)");
  obsoleteMenu_->addAction("Exploding Text");
  obsoleteMenu_->addAction("Flag");
  obsoleteMenu_->addAction("Fractal");
  obsoleteMenu_->addAction("Glass");
  obsoleteMenu_->addAction("Glow (Obsolete)");
  obsoleteMenu_->addAction("Gradient Ramp (Obsolete)");
  obsoleteMenu_->addAction("Grid");
  obsoleteMenu_->addAction("Lens Flare (Obsolete)");
  obsoleteMenu_->addAction("Lightning");
  obsoleteMenu_->addAction("Motion Tile");
  obsoleteMenu_->addAction("Page Turn");
  obsoleteMenu_->addAction("Paint Bucket");
  obsoleteMenu_->addAction("Particle Playground (Obsolete)");
  obsoleteMenu_->addAction("Polar Coordinates");
  obsoleteMenu_->addAction("Radio Waves");
  obsoleteMenu_->addAction("Ramp");
  obsoleteMenu_->addAction("Ripple");
  obsoleteMenu_->addAction("Scatter");
  obsoleteMenu_->addAction("Shatter");
  obsoleteMenu_->addAction("Spherize");
  obsoleteMenu_->addAction("Texturize");
  obsoleteMenu_->addAction("Threshold");
  obsoleteMenu_->addAction("Timecode");
  obsoleteMenu_->addAction("Tritone");
  obsoleteMenu_->addAction("Vegas");
  obsoleteMenu_->addAction("Wave");
  obsoleteMenu_->addAction("Wave World");
  obsoleteMenu_->addAction("Wood");

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
  
  auto* service = ArtifactProjectService::instance();
  if (service) {
      QObject::connect(service, &ArtifactProjectService::layerSelected, menu, [this](const ArtifactCore::LayerID& id) {
          selectedLayerId_ = id;
          refreshEnabledState();
      });
      QObject::connect(service, &ArtifactProjectService::layerRemoved, menu, [this](const ArtifactCore::CompositionID&, const ArtifactCore::LayerID& id) {
          if (selectedLayerId_ == id) {
              selectedLayerId_ = {};
          }
          refreshEnabledState();
      });
      QObject::connect(service, &ArtifactProjectService::projectChanged, menu, [this]() {
          refreshEnabledState();
      });
  }
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
  setTearOffEnabled(false);

  // Menu is built in Impl constructor
  impl_->refreshEnabledState();
 }

 ArtifactEffectMenu::~ArtifactEffectMenu()
 {
  delete impl_;
 }



};
