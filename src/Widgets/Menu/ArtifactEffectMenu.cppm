module;
#include <QAction>
#include <QWidget>
#include <QMenu>
#include <QKeySequence>
module Artifact.Menu.Effect;

#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <unordered_map>
#include <set>
#include <unordered_set>
#include <memory>
#include <algorithm>
#include <cmath>
#include <functional>
#include <optional>
#include <utility>
#include <array>
#include <mutex>
#include <thread>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <variant>
#include <any>
#include <atomic>
#include <condition_variable>
#include <queue>
#include <deque>
#include <list>
#include <tuple>
#include <numeric>
#include <regex>
#include <random>



import Artifact.Service.Effect;

namespace Artifact
{
 class ArtifactEffectMenu::Impl
 {
 private:
  
 public:
  Impl(QMenu*menu);
  ~Impl();
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
 };

 ArtifactEffectMenu::Impl::Impl(QMenu*menu)
{
  inspectorAction_ = new QAction("エフェクトコントロール");
  inspectorAction_->setShortcut(QKeySequence(Qt::Key_F3));

  removeAllAction_ = new QAction("すべてを削除");
  removeAllAction_->setShortcut(QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_X));

  // Keyframe Assistant submenu
  keyframeAssistantMenu_ = new QMenu("キーフレーム補助(&K)");
  keyframeAssistantMenu_->addAction("イージーイーズ");
  keyframeAssistantMenu_->addAction("イージーイーズイン");
  keyframeAssistantMenu_->addAction("イージーイーズアウト");
  keyframeAssistantMenu_->addAction("指数スケール");
  keyframeAssistantMenu_->addAction("レイヤーを順番に配置");
  keyframeAssistantMenu_->addAction("時間反転キーフレーム");
  keyframeAssistantMenu_->addAction("オーディオをキーフレームに変換");
  keyframeAssistantMenu_->addAction("エクスプレッションをキーフレームに変換");

  // Expression Controls submenu
  expressionControlsMenu_ = new QMenu("エクスプレッション制御(&E)");
  expressionControlsMenu_->addAction("角度制御");
  expressionControlsMenu_->addAction("チェックボックス制御");
  expressionControlsMenu_->addAction("カラー制御");
  expressionControlsMenu_->addAction("レイヤー制御");
  expressionControlsMenu_->addAction("ポイント制御");
  expressionControlsMenu_->addAction("スライダー制御");

  // Channel submenu
  channelMenu_ = new QMenu("チャンネル(&C)");
  channelMenu_->addAction("マット設定");
  channelMenu_->addAction("算術");
  channelMenu_->addAction("ブレンド");
  channelMenu_->addAction("反転");
  channelMenu_->addAction("ミニマックス");
  channelMenu_->addAction("チャンネルコンバイナー");

  // Stylize submenu
  stylizeMenu_ = new QMenu("スタイライズ(&S)");
  stylizeMenu_->addAction("ドロップシャドウ");
  stylizeMenu_->addAction("グロー");
  stylizeMenu_->addAction("ベベルアルファ");
  stylizeMenu_->addAction("ベベルエッジ");

  // Color Correction submenu
  colorCorrectionMenu_ = new QMenu("カラー補正(&C)");
  colorCorrectionMenu_->addAction("輝度＆コントラスト");
  colorCorrectionMenu_->addAction("トーンカーブ");
  colorCorrectionMenu_->addAction("色相/彩度");
  colorCorrectionMenu_->addAction("レベル補正");
  colorCorrectionMenu_->addAction("カラーバランス");
  colorCorrectionMenu_->addAction("写真フィルター");

  // Distort submenu
  distortMenu_ = new QMenu("ディストーション(&D)");
  distortMenu_->addAction("バルジ");
  distortMenu_->addAction("コーナーピン");
  distortMenu_->addAction("レンズ補正");
  distortMenu_->addAction("マグニファイ");
  distortMenu_->addAction("メッシュワープ");
  distortMenu_->addAction("オプティクス補償");
  distortMenu_->addAction("トランスフォーム");
  distortMenu_->addAction("タービュレントディスプレイス");
  distortMenu_->addAction("トワール");
  distortMenu_->addAction("ワープ");
  distortMenu_->addAction("波形ワープ");

  // Blur & Sharpen submenu
  blurSharpenMenu_ = new QMenu("ブラー＆シャープ(&B)");
  blurSharpenMenu_->addAction("ブラー (ガウス)");
  blurSharpenMenu_->addAction("ブラー (滑らか)");
  blurSharpenMenu_->addAction("ブラー (方向)");
  blurSharpenMenu_->addAction("ブラー (放射状)");
  blurSharpenMenu_->addAction("シャープ");
  blurSharpenMenu_->addAction("アンシャープマスク");

  // Noise & Grain submenu
  noiseGrainMenu_ = new QMenu("Noise & Grain");
  noiseGrainMenu_->addAction("Add Grain");
  noiseGrainMenu_->addAction("Dust & Scratches");
  noiseGrainMenu_->addAction("Fractal Noise");
  noiseGrainMenu_->addAction("Median");
  noiseGrainMenu_->addAction("Noise");
  noiseGrainMenu_->addAction("Noise HLS");
  noiseGrainMenu_->addAction("Noise HLS Auto");
  noiseGrainMenu_->addAction("Remove Grain");

  // Transition submenu
  transitionMenu_ = new QMenu("Transition");
  transitionMenu_->addAction("Linear Wipe");
  transitionMenu_->addAction("Radial Wipe");
  transitionMenu_->addAction("Venetian Blinds");
  transitionMenu_->addAction("Dissolve");
  transitionMenu_->addAction("Fade In");
  transitionMenu_->addAction("Fade Out");

  // Utility submenu
  utilityMenu_ = new QMenu("Utility");
  utilityMenu_->addAction("Apply Color LUT");
  utilityMenu_->addAction("Change Colorspace");
  utilityMenu_->addAction("Change To Color");
  utilityMenu_->addAction("Color Link");
  utilityMenu_->addAction("Displacement Map");
  utilityMenu_->addAction("Drop Shadow");
  utilityMenu_->addAction("Extract");
  utilityMenu_->addAction("Glow");
  utilityMenu_->addAction("HDR Tonemap");
  utilityMenu_->addAction("HDR Highlight Compression");
  utilityMenu_->addAction("Levels (Individual Controls)");
  utilityMenu_->addAction("Null Object");
  utilityMenu_->addAction("Particle Playground");
  utilityMenu_->addAction("Tint");
  utilityMenu_->addAction("Write-on");

  // Matte submenu
  matteMenu_ = new QMenu("Matte");
  matteMenu_->addAction("Matte Choker");
  matteMenu_->addAction("Simple Choker");
  matteMenu_->addAction("Alpha Levels");
  matteMenu_->addAction("Alpha Matte");
  matteMenu_->addAction("Difference Matte");
  matteMenu_->addAction("Luma Matte");
  matteMenu_->addAction("Track Matte");

  // Perspective submenu
  perspectiveMenu_ = new QMenu("Perspective");
  perspectiveMenu_->addAction("Bevel Alpha");
  perspectiveMenu_->addAction("Bevel Edges");
  perspectiveMenu_->addAction("Drop Shadow");

  // Keying submenu
  keyingMenu_ = new QMenu("Keying");
  keyingMenu_->addAction("Chroma Key");
  keyingMenu_->addAction("Color Key");
  keyingMenu_->addAction("Luma Key");

  // Obsolete submenu
  obsoleteMenu_ = new QMenu("Obsolete");
  obsoleteMenu_->addAction("3D Glasses");
  obsoleteMenu_->addAction("Audio Spectrum");
  obsoleteMenu_->addAction("Audio Waveform");
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
 }

 ArtifactEffectMenu::Impl::~Impl()
 {

 }

 ArtifactEffectMenu::ArtifactEffectMenu(QWidget* parent /*= nullptr*/):QMenu(parent),impl_(new Impl(this))
 {
  setTitle("エフェクト(&T)");
  setTearOffEnabled(false);

  // Menu is built in Impl constructor
 }

 ArtifactEffectMenu::~ArtifactEffectMenu()
 {
  delete impl_;
 }



};