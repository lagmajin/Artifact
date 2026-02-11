module;
#include <QAction>
#include <QWidget>
#include <QMenu>
#include <QKeySequence>
module Artifact.Menu.Effect;

import std;
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
  inspectorAction_ = new QAction("Effect Controls");
  inspectorAction_->setShortcut(QKeySequence(Qt::Key_F3));

  removeAllAction_ = new QAction("Remove All");
  removeAllAction_->setShortcut(QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_X));

  // Keyframe Assistant submenu
  keyframeAssistantMenu_ = new QMenu("Keyframe Assistant");
  keyframeAssistantMenu_->addAction("Easy Ease");
  keyframeAssistantMenu_->addAction("Easy Ease In");
  keyframeAssistantMenu_->addAction("Easy Ease Out");
  keyframeAssistantMenu_->addAction("Exponential Scale");
  keyframeAssistantMenu_->addAction("Sequence Layers");
  keyframeAssistantMenu_->addAction("Time-Reverse Keyframes");
  keyframeAssistantMenu_->addAction("Convert Audio to Keyframes");
  keyframeAssistantMenu_->addAction("Convert Expression to Keyframes");

  // Expression Controls submenu
  expressionControlsMenu_ = new QMenu("Expression Controls");
  expressionControlsMenu_->addAction("Angle Control");
  expressionControlsMenu_->addAction("Checkbox Control");
  expressionControlsMenu_->addAction("Color Control");
  expressionControlsMenu_->addAction("Layer Control");
  expressionControlsMenu_->addAction("Point Control");
  expressionControlsMenu_->addAction("Slider Control");

  // Channel submenu
  channelMenu_ = new QMenu("Channel");
  channelMenu_->addAction("Set Matte");
  channelMenu_->addAction("Arithmetic");
  channelMenu_->addAction("Blend");
  channelMenu_->addAction("Invert");
  channelMenu_->addAction("Minimax");
  channelMenu_->addAction("Shift Channels");

  // Stylize submenu
  stylizeMenu_ = new QMenu("Stylize");
  stylizeMenu_->addAction("Drop Shadow");
  stylizeMenu_->addAction("Glow");
  stylizeMenu_->addAction("Bevel Alpha");
  stylizeMenu_->addAction("Bevel Edges");

  // Color Correction submenu
  colorCorrectionMenu_ = new QMenu("Color Correction");
  colorCorrectionMenu_->addAction("Brightness & Contrast");
  colorCorrectionMenu_->addAction("Curves");
  colorCorrectionMenu_->addAction("Hue/Saturation");
  colorCorrectionMenu_->addAction("Levels");
  colorCorrectionMenu_->addAction("Color Balance");
  colorCorrectionMenu_->addAction("Photo Filter");

  // Distort submenu
  distortMenu_ = new QMenu("Distort");
  distortMenu_->addAction("Bulge");
  distortMenu_->addAction("Corner Pin");
  distortMenu_->addAction("Lens Distortion");
  distortMenu_->addAction("Magnify");
  distortMenu_->addAction("Mesh Warp");
  distortMenu_->addAction("Optics Compensation");
  distortMenu_->addAction("Transform");
  distortMenu_->addAction("Turbulent Displace");
  distortMenu_->addAction("Twirl");
  distortMenu_->addAction("Warp");
  distortMenu_->addAction("Wave Warp");

  // Blur & Sharpen submenu
  blurSharpenMenu_ = new QMenu("Blur & Sharpen");
  blurSharpenMenu_->addAction("Gaussian Blur");
  blurSharpenMenu_->addAction("Fast Blur");
  blurSharpenMenu_->addAction("Directional Blur");
  blurSharpenMenu_->addAction("Radial Blur");
  blurSharpenMenu_->addAction("Sharpen");
  blurSharpenMenu_->addAction("Unsharp Mask");

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
  setTitle(tr("Effect"));

  // Menu is built in Impl constructor
 }

 ArtifactEffectMenu::~ArtifactEffectMenu()
 {
  delete impl_;
 }



};