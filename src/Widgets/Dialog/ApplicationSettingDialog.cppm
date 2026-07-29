module;
#include <QCheckBox>
#include <QAbstractItemView>
#include <QComboBox>
#include <QCoreApplication>
#include <QDesktopServices>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeyEvent>
#include <QEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QFileInfoList>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPluginLoader>
#include <QProgressBar>
#include <QPushButton>
#include <QMouseEvent>
#include <QKeySequenceEdit>
#include <QShowEvent>
#include <QSlider>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QStackedWidget>
#include <QStandardPaths>
#include <QString>
#include <QTableWidget>
#include <QThread>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>
#include <utility>
#include <cmath>

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <psapi.h>

#undef MessageBox
#endif
#include <wobjectimpl.h>

module ApplicationSettingDialog;

import Artifact.Service.Application;
import Artifact.Widgets.AppDialogs;
import Artifact.Widgets.AI.ArtifactAICloudSettingsWidget;
import Artifact.Widgets.PropertyEditor;
import Application.AppSettings;
import Artifact.Workspace.Modes;
import Artifact.Audio.ScrubController;
import FloatColorPickerDialog;
import Widgets.Utils.CSS;
import UI.ShortcutBindings;
import Settings.Accessibility;

namespace ArtifactCore {

namespace {

struct ThemePresetEntry {
  ArtifactCore::DccStylePreset preset;
};

static const ThemePresetEntry kThemePresetEntries[] = {
    {ArtifactCore::DccStylePreset::DefaultQt},
    {ArtifactCore::DccStylePreset::MayaStyle},
    {ArtifactCore::DccStylePreset::ModoStyle},
    {ArtifactCore::DccStylePreset::StudioStyle},
    {ArtifactCore::DccStylePreset::BlenderStyle},
    {ArtifactCore::DccStylePreset::DaVinciStyle},
    {ArtifactCore::DccStylePreset::_3dsMaxStyle},
    {ArtifactCore::DccStylePreset::NukeStyle},
    {ArtifactCore::DccStylePreset::AfterEffectsStyle},
    {ArtifactCore::DccStylePreset::HighContrast},
};

static void populateThemeCombo(QComboBox* combo)
{
  if (!combo) {
    return;
  }
  combo->clear();
  for (const auto& entry : kThemePresetEntries) {
    combo->addItem(ArtifactCore::themePresetLabel(entry.preset),
                   static_cast<int>(entry.preset));
  }
}

static QString canonicalThemeLabel(const QString& name)
{
  return ArtifactCore::themePresetLabel(ArtifactCore::themePresetFromName(name));
}

} // namespace

// GeneralSettingPage Implementation
class GeneralSettingPage::Impl {
public:
  Impl();
  ~Impl();

  QCheckBox *autoSaveCheckBox_;
  QSpinBox *autoSaveIntervalSpinBox_;
  QCheckBox *showStartupDialogCheckBox_;
  QCheckBox *showPropertyResetButtonsCheckBox_;
  QCheckBox *layerCacheEnabledCheckBox_;
  QSpinBox *menuBarFontScaleSpinBox_;
  QSpinBox *dockTabFontSizeSpinBox_;
  QComboBox *themeCombo_;
  QComboBox *handednessCombo_;
  QCheckBox *largeTargetsCheckBox_;
  QCheckBox *highContrastHintsCheckBox_;
  QSpinBox *accessibilityFontScaleSpinBox_;
  QComboBox *colorDeficiencyCombo_;
  QCheckBox *reduceHoverDependencyCheckBox_;
};

GeneralSettingPage::Impl::Impl() {}

GeneralSettingPage::Impl::~Impl() {}

GeneralSettingPage::GeneralSettingPage(QWidget *parent)
    : QWidget(parent), impl_(new Impl()) {
  setAccessibleName(QStringLiteral("General application settings"));
  setAccessibleDescription(QStringLiteral("Configure saving, startup, interface, and accessibility preferences"));
  auto *mainLayout = new QVBoxLayout(this);

  // Auto-Save Group
  auto *autoSaveGroup = new QGroupBox("Auto-Save", this);
  auto *autoSaveLayout = new QVBoxLayout(autoSaveGroup);

  impl_->autoSaveCheckBox_ = new QCheckBox("Enable Auto-Save", this);
  impl_->autoSaveCheckBox_->setAccessibleName(QStringLiteral("Enable auto-save"));
  impl_->autoSaveCheckBox_->setAccessibleDescription(QStringLiteral("Automatically save the current project"));
  autoSaveLayout->addWidget(impl_->autoSaveCheckBox_);

  auto *intervalLayout = new QHBoxLayout();
  intervalLayout->addWidget(new QLabel("Save every:", this));
  impl_->autoSaveIntervalSpinBox_ = new QSpinBox(this);
  impl_->autoSaveIntervalSpinBox_->setAccessibleName(QStringLiteral("Auto-save interval"));
  impl_->autoSaveIntervalSpinBox_->setAccessibleDescription(QStringLiteral("Minutes between automatic saves"));
  impl_->autoSaveIntervalSpinBox_->setRange(1, 60);
  impl_->autoSaveIntervalSpinBox_->setSuffix(" minutes");
  intervalLayout->addWidget(impl_->autoSaveIntervalSpinBox_);
  intervalLayout->addStretch();
  autoSaveLayout->addLayout(intervalLayout);

  mainLayout->addWidget(autoSaveGroup);

  // Startup Group
  auto *startupGroup = new QGroupBox("Startup", this);
  auto *startupLayout = new QVBoxLayout(startupGroup);

  impl_->showStartupDialogCheckBox_ =
      new QCheckBox("Load last project on startup", this);
  impl_->showStartupDialogCheckBox_->setAccessibleName(QStringLiteral("Load last project on startup"));
  impl_->showStartupDialogCheckBox_->setAccessibleDescription(QStringLiteral("Restore the last project when the application starts"));
  startupLayout->addWidget(impl_->showStartupDialogCheckBox_);

  mainLayout->addWidget(startupGroup);

  // UI Group
  auto *uiGroup = new QGroupBox("User Interface", this);
  auto *uiLayout = new QVBoxLayout(uiGroup);

  impl_->showPropertyResetButtonsCheckBox_ =
      new QCheckBox("Show reset buttons in property panels", this);
  impl_->showPropertyResetButtonsCheckBox_->setAccessibleName(QStringLiteral("Show property reset buttons"));
  impl_->showPropertyResetButtonsCheckBox_->setAccessibleDescription(QStringLiteral("Show reset controls in property panels"));
  uiLayout->addWidget(impl_->showPropertyResetButtonsCheckBox_);

  impl_->layerCacheEnabledCheckBox_ =
      new QCheckBox("Enable layer cache", this);
  impl_->layerCacheEnabledCheckBox_->setAccessibleName(QStringLiteral("Enable layer cache"));
  impl_->layerCacheEnabledCheckBox_->setAccessibleDescription(QStringLiteral("Use layer surface and texture caches"));
  impl_->layerCacheEnabledCheckBox_->setToolTip(
      QStringLiteral("Turn this off to bypass all layer surface and texture caches."));
  uiLayout->addWidget(impl_->layerCacheEnabledCheckBox_);

  auto *menuFontLayout = new QHBoxLayout();
  menuFontLayout->addWidget(new QLabel("Menu bar font scale:", this));
  impl_->menuBarFontScaleSpinBox_ = new QSpinBox(this);
  impl_->menuBarFontScaleSpinBox_->setAccessibleName(QStringLiteral("Menu bar font scale"));
  impl_->menuBarFontScaleSpinBox_->setAccessibleDescription(QStringLiteral("Scale menu bar text from 50 to 200 percent"));
  impl_->menuBarFontScaleSpinBox_->setRange(50, 200);
  impl_->menuBarFontScaleSpinBox_->setSuffix(" %");
  menuFontLayout->addWidget(impl_->menuBarFontScaleSpinBox_);
  menuFontLayout->addStretch();
  uiLayout->addLayout(menuFontLayout);

  auto *dockTabFontLayout = new QHBoxLayout();
  dockTabFontLayout->addWidget(new QLabel("Dock tab font size:", this));
  impl_->dockTabFontSizeSpinBox_ = new QSpinBox(this);
  impl_->dockTabFontSizeSpinBox_->setAccessibleName(QStringLiteral("Dock tab font size"));
  impl_->dockTabFontSizeSpinBox_->setAccessibleDescription(QStringLiteral("Set dock tab text size in points"));
  impl_->dockTabFontSizeSpinBox_->setRange(8, 30);
  impl_->dockTabFontSizeSpinBox_->setSuffix(" pt");
  dockTabFontLayout->addWidget(impl_->dockTabFontSizeSpinBox_);
  dockTabFontLayout->addStretch();
  uiLayout->addLayout(dockTabFontLayout);

  auto *themeLayout = new QHBoxLayout();
  themeLayout->addWidget(new QLabel("UI Theme:", this));
  impl_->themeCombo_ = new QComboBox(this);
  impl_->themeCombo_->setAccessibleName(QStringLiteral("UI theme"));
  impl_->themeCombo_->setAccessibleDescription(QStringLiteral("Choose the application color theme"));
  populateThemeCombo(impl_->themeCombo_);
  themeLayout->addWidget(impl_->themeCombo_);
  themeLayout->addStretch();
  uiLayout->addLayout(themeLayout);

  auto *accessibilityGroup = new QGroupBox("Accessibility", this);
  accessibilityGroup->setAccessibleName(QStringLiteral("Accessibility settings"));
  accessibilityGroup->setAccessibleDescription(
      QStringLiteral("Configure handedness, target size, contrast, text scale, and color vision assistance."));
  auto *accessibilityLayout = new QVBoxLayout(accessibilityGroup);
  auto *handednessLayout = new QHBoxLayout();
  handednessLayout->addWidget(new QLabel("Preferred hand:", this));
  impl_->handednessCombo_ = new QComboBox(this);
  impl_->handednessCombo_->addItem("Right", "right");
  impl_->handednessCombo_->addItem("Left", "left");
  impl_->handednessCombo_->setAccessibleName(QStringLiteral("Preferred hand"));
  impl_->handednessCombo_->setAccessibleDescription(
      QStringLiteral("Choose the preferred hand for context menu placement."));
  impl_->handednessCombo_->setMinimumHeight(
      Artifact::Accessibility::scaledSize(24));
  handednessLayout->addWidget(impl_->handednessCombo_);
  handednessLayout->addStretch();
  accessibilityLayout->addLayout(handednessLayout);

  impl_->largeTargetsCheckBox_ = new QCheckBox("Use larger hit targets", this);
  impl_->highContrastHintsCheckBox_ = new QCheckBox("Emphasize high-contrast hints", this);
  impl_->reduceHoverDependencyCheckBox_ = new QCheckBox("Reduce hover dependency", this);
  impl_->largeTargetsCheckBox_->setAccessibleName(QStringLiteral("Use larger hit targets"));
  impl_->largeTargetsCheckBox_->setAccessibleDescription(
      QStringLiteral("Increase interactive target sizes throughout the editor."));
  impl_->highContrastHintsCheckBox_->setAccessibleName(
      QStringLiteral("Emphasize high-contrast hints"));
  impl_->highContrastHintsCheckBox_->setAccessibleDescription(
      QStringLiteral("Increase contrast for accessibility hints and status cues."));
  impl_->reduceHoverDependencyCheckBox_->setAccessibleName(
      QStringLiteral("Reduce hover dependency"));
  impl_->reduceHoverDependencyCheckBox_->setAccessibleDescription(
      QStringLiteral("Keep important information available without relying on hover."));
  impl_->largeTargetsCheckBox_->setMinimumHeight(
      Artifact::Accessibility::scaledSize(24));
  impl_->highContrastHintsCheckBox_->setMinimumHeight(
      Artifact::Accessibility::scaledSize(24));
  impl_->reduceHoverDependencyCheckBox_->setMinimumHeight(
      Artifact::Accessibility::scaledSize(24));
  accessibilityLayout->addWidget(impl_->largeTargetsCheckBox_);
  accessibilityLayout->addWidget(impl_->highContrastHintsCheckBox_);
  accessibilityLayout->addWidget(impl_->reduceHoverDependencyCheckBox_);

  auto *accessibilityFontLayout = new QHBoxLayout();
  accessibilityFontLayout->addWidget(new QLabel("Accessibility font scale:", this));
  impl_->accessibilityFontScaleSpinBox_ = new QSpinBox(this);
  impl_->accessibilityFontScaleSpinBox_->setRange(100, 200);
  impl_->accessibilityFontScaleSpinBox_->setSuffix(" %");
  impl_->accessibilityFontScaleSpinBox_->setAccessibleName(
      QStringLiteral("Accessibility font scale"));
  impl_->accessibilityFontScaleSpinBox_->setAccessibleDescription(
      QStringLiteral("Scale accessibility text from 100 to 200 percent."));
  impl_->accessibilityFontScaleSpinBox_->setMinimumHeight(
      Artifact::Accessibility::scaledSize(24));
  accessibilityFontLayout->addWidget(impl_->accessibilityFontScaleSpinBox_);
  accessibilityFontLayout->addStretch();
  accessibilityLayout->addLayout(accessibilityFontLayout);

  auto *colorDeficiencyLayout = new QHBoxLayout();
  colorDeficiencyLayout->addWidget(new QLabel("Color vision assist:", this));
  impl_->colorDeficiencyCombo_ = new QComboBox(this);
  impl_->colorDeficiencyCombo_->addItem("None", "none");
  impl_->colorDeficiencyCombo_->addItem("Protanopia", "protanopia");
  impl_->colorDeficiencyCombo_->addItem("Deuteranopia", "deuteranopia");
  impl_->colorDeficiencyCombo_->addItem("Tritanopia", "tritanopia");
  impl_->colorDeficiencyCombo_->setAccessibleName(QStringLiteral("Color vision assist"));
  impl_->colorDeficiencyCombo_->setAccessibleDescription(
      QStringLiteral("Choose color adjustments for common color vision deficiencies."));
  impl_->colorDeficiencyCombo_->setMinimumHeight(
      Artifact::Accessibility::scaledSize(24));
  colorDeficiencyLayout->addWidget(impl_->colorDeficiencyCombo_);
  colorDeficiencyLayout->addStretch();
  accessibilityLayout->addLayout(colorDeficiencyLayout);
  setTabOrder(impl_->handednessCombo_, impl_->largeTargetsCheckBox_);
  setTabOrder(impl_->largeTargetsCheckBox_,
              impl_->highContrastHintsCheckBox_);
  setTabOrder(impl_->highContrastHintsCheckBox_,
              impl_->reduceHoverDependencyCheckBox_);
  setTabOrder(impl_->reduceHoverDependencyCheckBox_,
              impl_->accessibilityFontScaleSpinBox_);
  setTabOrder(impl_->accessibilityFontScaleSpinBox_,
              impl_->colorDeficiencyCombo_);
  uiLayout->addWidget(accessibilityGroup);

  mainLayout->addWidget(uiGroup);

  mainLayout->addStretch();

  loadSettings();
}

void GeneralSettingPage::loadSettings() {
  auto *settings = ArtifactAppSettings::instance();
  impl_->autoSaveIntervalSpinBox_->setValue(
      settings->autoSaveIntervalMinutes());
  impl_->showStartupDialogCheckBox_->setChecked(
      settings->loadLastProjectOnStartup());
  impl_->showPropertyResetButtonsCheckBox_->setChecked(
      Artifact::artifactShouldShowPropertyResetButtons());
  impl_->layerCacheEnabledCheckBox_->setChecked(
      settings->layerCacheEnabled());
  impl_->menuBarFontScaleSpinBox_->setValue(
      settings->menuBarFontScalePercent());
  impl_->dockTabFontSizeSpinBox_->setValue(settings->dockTabFontPointSize());
  if (impl_->themeCombo_) {
    const QString themeLabel = canonicalThemeLabel(settings->themeName());
    const int themeIndex = impl_->themeCombo_->findText(themeLabel);
    if (themeIndex >= 0) {
      impl_->themeCombo_->setCurrentIndex(themeIndex);
    } else if (impl_->themeCombo_->count() > 0) {
      impl_->themeCombo_->setCurrentIndex(0);
    }
  }
  impl_->handednessCombo_->setCurrentIndex(
      impl_->handednessCombo_->findData(settings->accessibilityHandedness()));
  impl_->largeTargetsCheckBox_->setChecked(settings->accessibilityPreferLargeTargets());
  impl_->highContrastHintsCheckBox_->setChecked(settings->accessibilityPreferHighContrastHints());
  impl_->accessibilityFontScaleSpinBox_->setValue(settings->accessibilityFontScalePercent());
  impl_->colorDeficiencyCombo_->setCurrentIndex(
      impl_->colorDeficiencyCombo_->findData(settings->accessibilityColorDeficiencyMode()));
  impl_->reduceHoverDependencyCheckBox_->setChecked(settings->accessibilityReduceHoverDependency());
  // autoSaveEnabled の項目が AppSettings にまだないので、将来的に追加が必要
}

void GeneralSettingPage::saveSettings() {
  auto *settings = ArtifactAppSettings::instance();
  settings->setAutoSaveIntervalMinutes(
      impl_->autoSaveIntervalSpinBox_->value());
  settings->setLoadLastProjectOnStartup(
      impl_->showStartupDialogCheckBox_->isChecked());
  Artifact::artifactSetShowPropertyResetButtons(
      impl_->showPropertyResetButtonsCheckBox_->isChecked());
  settings->setLayerCacheEnabled(impl_->layerCacheEnabledCheckBox_->isChecked());
  settings->setMenuBarFontScalePercent(
      impl_->menuBarFontScaleSpinBox_->value());
  settings->setDockTabFontPointSize(
      impl_->dockTabFontSizeSpinBox_->value());
  if (impl_->themeCombo_) {
    settings->setThemeName(impl_->themeCombo_->currentText());
  }
  settings->setAccessibilityHandedness(
      impl_->handednessCombo_->currentData().toString());
  settings->setAccessibilityPreferLargeTargets(
      impl_->largeTargetsCheckBox_->isChecked());
  settings->setAccessibilityPreferHighContrastHints(
      impl_->highContrastHintsCheckBox_->isChecked());
  settings->setAccessibilityFontScalePercent(
      impl_->accessibilityFontScaleSpinBox_->value());
  settings->setAccessibilityColorDeficiencyMode(
      impl_->colorDeficiencyCombo_->currentData().toString());
  settings->setAccessibilityReduceHoverDependency(
      impl_->reduceHoverDependencyCheckBox_->isChecked());
}

QList<SettingItemInfo> GeneralSettingPage::searchableItems() const {
  QList<SettingItemInfo> items;
  if (impl_ && impl_->themeCombo_) {
    items.push_back({"UI Theme",
                     "Built-in application theme preset",
                     "User Interface", impl_->themeCombo_});
  }
  return items;
}

GeneralSettingPage::~GeneralSettingPage() { delete impl_; }

// ImportSettingPage Implementation
class ImportSettingPage::Impl {
public:
  Impl();
  ~Impl();

  // Media Import Settings
  QComboBox *defaultFrameRateCombo_;
  QComboBox *colorSpaceCombo_;
  QComboBox *audioSampleRateCombo_;

  // Footage Interpretation
  QCheckBox *autoDetectAlphaCheckBox_;
  QCheckBox *interpretFootageCheckBox_;
  QComboBox *fieldOrderCombo_;

  // Sequence Settings
  QSpinBox *stillDurationSpinBox_;
  QCheckBox *createCompositionCheckBox_;
};

ImportSettingPage::Impl::Impl() {}

ImportSettingPage::Impl::~Impl() {}

ImportSettingPage::ImportSettingPage(QWidget *parent)
    : QWidget(parent), impl_(new Impl()) {
  setAccessibleName(QStringLiteral("Import settings"));
  setAccessibleDescription(QStringLiteral("Configure default media interpretation and image sequence import behavior"));
  auto *mainLayout = new QVBoxLayout(this);

  // Media Import Group
  auto *mediaImportGroup = new QGroupBox("Media Import", this);
  auto *mediaImportLayout = new QVBoxLayout(mediaImportGroup);

  // Frame Rate
  auto *frameRateLayout = new QHBoxLayout();
  frameRateLayout->addWidget(new QLabel("Default Frame Rate:", this));
  impl_->defaultFrameRateCombo_ = new QComboBox(this);
  impl_->defaultFrameRateCombo_->setAccessibleName(QStringLiteral("Default import frame rate"));
  impl_->defaultFrameRateCombo_->setAccessibleDescription(QStringLiteral("Frame rate used when importing media without embedded timing"));
  impl_->defaultFrameRateCombo_->addItems({"23.976 fps", "24 fps", "25 fps",
                                           "29.97 fps", "30 fps", "50 fps",
                                           "59.94 fps", "60 fps"});
  frameRateLayout->addWidget(impl_->defaultFrameRateCombo_);
  frameRateLayout->addStretch();
  mediaImportLayout->addLayout(frameRateLayout);

  // Color Space
  auto *colorSpaceLayout = new QHBoxLayout();
  colorSpaceLayout->addWidget(new QLabel("Color Space:", this));
  impl_->colorSpaceCombo_ = new QComboBox(this);
  impl_->colorSpaceCombo_->setAccessibleName(QStringLiteral("Default import color space"));
  impl_->colorSpaceCombo_->setAccessibleDescription(QStringLiteral("Color space used when importing media"));
  impl_->colorSpaceCombo_->addItems(
      {"sRGB", "Linear", "Rec.709", "Rec.2020", "DCI-P3", "Adobe RGB"});
  colorSpaceLayout->addWidget(impl_->colorSpaceCombo_);
  colorSpaceLayout->addStretch();
  mediaImportLayout->addLayout(colorSpaceLayout);

  // Audio Sample Rate
  auto *audioSampleLayout = new QHBoxLayout();
  audioSampleLayout->addWidget(new QLabel("Audio Sample Rate:", this));
  impl_->audioSampleRateCombo_ = new QComboBox(this);
  impl_->audioSampleRateCombo_->setAccessibleName(QStringLiteral("Default audio sample rate"));
  impl_->audioSampleRateCombo_->setAccessibleDescription(QStringLiteral("Sample rate used when importing audio"));
  impl_->audioSampleRateCombo_->addItems(
      {"44100 Hz", "48000 Hz", "96000 Hz", "192000 Hz"});
  audioSampleLayout->addWidget(impl_->audioSampleRateCombo_);
  audioSampleLayout->addStretch();
  mediaImportLayout->addLayout(audioSampleLayout);

  mainLayout->addWidget(mediaImportGroup);

  // Footage Interpretation Group
  auto *footageGroup = new QGroupBox("Footage Interpretation", this);
  auto *footageLayout = new QVBoxLayout(footageGroup);

  impl_->autoDetectAlphaCheckBox_ =
      new QCheckBox("Auto-detect alpha channel", this);
  impl_->autoDetectAlphaCheckBox_->setAccessibleName(QStringLiteral("Auto-detect alpha channel"));
  impl_->autoDetectAlphaCheckBox_->setAccessibleDescription(QStringLiteral("Detect transparency information when importing images"));
  footageLayout->addWidget(impl_->autoDetectAlphaCheckBox_);

  impl_->interpretFootageCheckBox_ =
      new QCheckBox("Interpret footage on import", this);
  impl_->interpretFootageCheckBox_->setAccessibleName(QStringLiteral("Interpret footage on import"));
  impl_->interpretFootageCheckBox_->setAccessibleDescription(QStringLiteral("Apply footage interpretation settings during import"));
  footageLayout->addWidget(impl_->interpretFootageCheckBox_);

  // Field Order
  auto *fieldOrderLayout = new QHBoxLayout();
  fieldOrderLayout->addWidget(new QLabel("Field Order:", this));
  impl_->fieldOrderCombo_ = new QComboBox(this);
  impl_->fieldOrderCombo_->setAccessibleName(QStringLiteral("Field order"));
  impl_->fieldOrderCombo_->setAccessibleDescription(QStringLiteral("Choose the field order for interlaced footage"));
  impl_->fieldOrderCombo_->addItems(
      {"Progressive", "Upper Field First", "Lower Field First"});
  fieldOrderLayout->addWidget(impl_->fieldOrderCombo_);
  fieldOrderLayout->addStretch();
  footageLayout->addLayout(fieldOrderLayout);

  mainLayout->addWidget(footageGroup);

  // Sequence Settings Group
  auto *sequenceGroup = new QGroupBox("Sequence Settings", this);
  auto *sequenceLayout = new QVBoxLayout(sequenceGroup);

  // Still Duration
  auto *durationLayout = new QHBoxLayout();
  durationLayout->addWidget(new QLabel("Still Image Duration:", this));
  impl_->stillDurationSpinBox_ = new QSpinBox(this);
  impl_->stillDurationSpinBox_->setAccessibleName(QStringLiteral("Still image duration"));
  impl_->stillDurationSpinBox_->setAccessibleDescription(QStringLiteral("Default duration of imported still images in seconds"));
  impl_->stillDurationSpinBox_->setRange(1, 3600);
  impl_->stillDurationSpinBox_->setSuffix(" seconds");
  durationLayout->addWidget(impl_->stillDurationSpinBox_);
  durationLayout->addStretch();
  sequenceLayout->addLayout(durationLayout);

  impl_->createCompositionCheckBox_ =
      new QCheckBox("Create composition when importing sequences", this);
  impl_->createCompositionCheckBox_->setAccessibleName(QStringLiteral("Create composition for image sequences"));
  impl_->createCompositionCheckBox_->setAccessibleDescription(QStringLiteral("Create a composition automatically when importing a numbered image sequence"));
  sequenceLayout->addWidget(impl_->createCompositionCheckBox_);

  mainLayout->addWidget(sequenceGroup);

  mainLayout->addStretch();
}

ImportSettingPage::~ImportSettingPage() { delete impl_; }

// PreviewSettingPage Implementation
class PreviewSettingPage::Impl {
public:
  Impl();
  ~Impl();

  // Preview Quality
  QComboBox *previewQualityCombo_;
  QSlider *previewResolutionSlider_;
  QLabel *resolutionLabel_;

  // Cache Settings
  QCheckBox *enableCacheCheckBox_;
  QSpinBox *cacheSizeSpinBox_;
  QCheckBox *enableDiskCacheCheckBox_;

  // Thumbnail Settings
  QCheckBox *generateThumbnailsCheckBox_;
  QComboBox *thumbnailQualityCombo_;

  // GPU Acceleration
  QCheckBox *enableGPUCheckBox_;
  QComboBox *gpuDeviceCombo_;
};

PreviewSettingPage::Impl::Impl() {}

PreviewSettingPage::Impl::~Impl() {}

PreviewSettingPage::PreviewSettingPage(QWidget *parent)
    : QWidget(parent), impl_(new Impl()) {
  setAccessibleName(QStringLiteral("Preview settings"));
  setAccessibleDescription(QStringLiteral("Configure preview quality, caches, thumbnails, and GPU acceleration"));
  auto *mainLayout = new QVBoxLayout(this);

  // Preview Quality Group
  auto *qualityGroup = new QGroupBox("Preview Quality", this);
  auto *qualityLayout = new QVBoxLayout(qualityGroup);

  // Quality Preset
  auto *presetLayout = new QHBoxLayout();
  presetLayout->addWidget(new QLabel("Quality Preset:", this));
  impl_->previewQualityCombo_ = new QComboBox(this);
  impl_->previewQualityCombo_->setAccessibleName(QStringLiteral("Preview quality preset"));
  impl_->previewQualityCombo_->setAccessibleDescription(QStringLiteral("Choose the quality preset used for previews"));
  impl_->previewQualityCombo_->addItems(
      {"Draft", "Fast", "Adaptive", "Full Quality"});
  impl_->previewQualityCombo_->setCurrentText("Adaptive");
  presetLayout->addWidget(impl_->previewQualityCombo_);
  presetLayout->addStretch();
  qualityLayout->addLayout(presetLayout);

  // Preview Resolution
  auto *resolutionLayout = new QVBoxLayout();
  auto *resLabelLayout = new QHBoxLayout();
  resLabelLayout->addWidget(new QLabel("Preview Resolution:", this));
  impl_->resolutionLabel_ = new QLabel("50%", this);
  resLabelLayout->addWidget(impl_->resolutionLabel_);
  resLabelLayout->addStretch();
  resolutionLayout->addLayout(resLabelLayout);

  impl_->previewResolutionSlider_ = new QSlider(Qt::Horizontal, this);
  impl_->previewResolutionSlider_->setAccessibleName(QStringLiteral("Preview resolution"));
  impl_->previewResolutionSlider_->setAccessibleDescription(QStringLiteral("Set preview resolution from 25 to 100 percent"));
  impl_->previewResolutionSlider_->setRange(25, 100);
  impl_->previewResolutionSlider_->setValue(50);
  impl_->previewResolutionSlider_->setTickPosition(QSlider::TicksBelow);
  impl_->previewResolutionSlider_->setTickInterval(25);
  resolutionLayout->addWidget(impl_->previewResolutionSlider_);
  qualityLayout->addLayout(resolutionLayout);

  QObject::connect(impl_->previewResolutionSlider_, &QSlider::valueChanged,
                   [this](int value) {
                     impl_->resolutionLabel_->setText(QString::number(value) +
                                                      "%");
                   });

  mainLayout->addWidget(qualityGroup);

  // Cache Settings Group
  auto *cacheGroup = new QGroupBox("Cache Settings", this);
  auto *cacheLayout = new QVBoxLayout(cacheGroup);

  impl_->enableCacheCheckBox_ = new QCheckBox("Enable RAM cache", this);
  impl_->enableCacheCheckBox_->setAccessibleName(QStringLiteral("Enable RAM cache"));
  impl_->enableCacheCheckBox_->setAccessibleDescription(QStringLiteral("Cache preview frames in memory"));
  impl_->enableCacheCheckBox_->setChecked(true);
  cacheLayout->addWidget(impl_->enableCacheCheckBox_);

  auto *cacheSizeLayout = new QHBoxLayout();
  cacheSizeLayout->addWidget(new QLabel("Cache Size:", this));
  impl_->cacheSizeSpinBox_ = new QSpinBox(this);
  impl_->cacheSizeSpinBox_->setAccessibleName(QStringLiteral("RAM cache size"));
  impl_->cacheSizeSpinBox_->setAccessibleDescription(QStringLiteral("Maximum RAM cache size in megabytes"));
  impl_->cacheSizeSpinBox_->setRange(512, 32768);
  impl_->cacheSizeSpinBox_->setValue(4096);
  impl_->cacheSizeSpinBox_->setSuffix(" MB");
  impl_->cacheSizeSpinBox_->setSingleStep(512);
  cacheSizeLayout->addWidget(impl_->cacheSizeSpinBox_);
  cacheSizeLayout->addStretch();
  cacheLayout->addLayout(cacheSizeLayout);

  impl_->enableDiskCacheCheckBox_ = new QCheckBox("Enable disk cache", this);
  impl_->enableDiskCacheCheckBox_->setAccessibleName(QStringLiteral("Enable disk cache"));
  impl_->enableDiskCacheCheckBox_->setAccessibleDescription(QStringLiteral("Store preview cache on disk"));
  impl_->enableDiskCacheCheckBox_->setChecked(false);
  cacheLayout->addWidget(impl_->enableDiskCacheCheckBox_);

  mainLayout->addWidget(cacheGroup);

  // Thumbnail Settings Group (using FFmpegThumbnailExtractor)
  auto *thumbnailGroup = new QGroupBox("Thumbnail Generation", this);
  auto *thumbnailLayout = new QVBoxLayout(thumbnailGroup);

  impl_->generateThumbnailsCheckBox_ =
      new QCheckBox("Generate thumbnails for media files", this);
  impl_->generateThumbnailsCheckBox_->setAccessibleName(QStringLiteral("Generate media thumbnails"));
  impl_->generateThumbnailsCheckBox_->setAccessibleDescription(QStringLiteral("Generate thumbnails for imported media files"));
  impl_->generateThumbnailsCheckBox_->setChecked(true);
  thumbnailLayout->addWidget(impl_->generateThumbnailsCheckBox_);

  auto *thumbQualityLayout = new QHBoxLayout();
  thumbQualityLayout->addWidget(new QLabel("Thumbnail Quality:", this));
  impl_->thumbnailQualityCombo_ = new QComboBox(this);
  impl_->thumbnailQualityCombo_->setAccessibleName(QStringLiteral("Thumbnail quality"));
  impl_->thumbnailQualityCombo_->setAccessibleDescription(QStringLiteral("Choose the quality of generated media thumbnails"));
  impl_->thumbnailQualityCombo_->addItems({"Low", "Medium", "High"});
  impl_->thumbnailQualityCombo_->setCurrentText("Medium");
  thumbQualityLayout->addWidget(impl_->thumbnailQualityCombo_);
  thumbQualityLayout->addStretch();
  thumbnailLayout->addLayout(thumbQualityLayout);

  mainLayout->addWidget(thumbnailGroup);

  // GPU Acceleration Group
  auto *gpuGroup = new QGroupBox("GPU Acceleration", this);
  auto *gpuLayout = new QVBoxLayout(gpuGroup);

  impl_->enableGPUCheckBox_ = new QCheckBox("Enable GPU acceleration", this);
  impl_->enableGPUCheckBox_->setAccessibleName(QStringLiteral("Enable GPU acceleration"));
  impl_->enableGPUCheckBox_->setAccessibleDescription(QStringLiteral("Use GPU acceleration for preview processing"));
  impl_->enableGPUCheckBox_->setChecked(true);
  gpuLayout->addWidget(impl_->enableGPUCheckBox_);

  auto *gpuDeviceLayout = new QHBoxLayout();
  gpuDeviceLayout->addWidget(new QLabel("GPU Device:", this));
  impl_->gpuDeviceCombo_ = new QComboBox(this);
  impl_->gpuDeviceCombo_->setAccessibleName(QStringLiteral("GPU device"));
  impl_->gpuDeviceCombo_->setAccessibleDescription(QStringLiteral("Choose the GPU used for preview processing"));
  impl_->gpuDeviceCombo_->addItems(
      {"Auto (Best Available)", "NVIDIA GPU", "AMD GPU", "Intel GPU"});
  impl_->gpuDeviceCombo_->setCurrentText("Auto (Best Available)");
  gpuDeviceLayout->addWidget(impl_->gpuDeviceCombo_);
  gpuDeviceLayout->addStretch();
  gpuLayout->addLayout(gpuDeviceLayout);

  mainLayout->addWidget(gpuGroup);

  mainLayout->addStretch();
}

PreviewSettingPage::~PreviewSettingPage() { delete impl_; }

namespace {
static QString frameRateLabelFor(double fps) {
  struct Pair {
    double value;
    const char *label;
  };
  static const Pair presets[] = {
      {23.976, "23.976 fps"}, {24.0, "24 fps"},   {25.0, "25 fps"},
      {29.97, "29.97 fps"},   {30.0, "30 fps"},   {50.0, "50 fps"},
      {59.94, "59.94 fps"},   {60.0, "60 fps"}};
  for (const auto &preset : presets) {
    if (std::abs(preset.value - fps) < 0.001) {
      return QString::fromLatin1(preset.label);
    }
  }
  return QStringLiteral("%1 fps").arg(fps, 0, 'f', 3);
}

static double frameRateValueForLabel(const QString &label, double fallback) {
  QString trimmed = label.trimmed().toLower();
  struct Pair {
    double value;
    const char *label;
  };
  static const Pair presets[] = {
      {23.976, "23.976 fps"}, {24.0, "24 fps"},   {25.0, "25 fps"},
      {29.97, "29.97 fps"},   {30.0, "30 fps"},   {50.0, "50 fps"},
      {59.94, "59.94 fps"},   {60.0, "60 fps"}};
  for (const auto &preset : presets) {
    if (trimmed == QString::fromLatin1(preset.label).toLower()) {
      return preset.value;
    }
  }
  bool ok = false;
  const double parsed = trimmed.remove(QStringLiteral("fps")).trimmed().toDouble(&ok);
  return ok ? parsed : fallback;
}

static QString workspaceModeValueForLabel(const QString &label,
                                          const QString &fallback) {
  const QString normalized = label.trimmed();
  if (!normalized.isEmpty()) {
    return Artifact::workspaceModeInfoForText(normalized).label;
  }
  return Artifact::workspaceModeInfoForText(fallback).label;
}

static void setButtonColor(QPushButton *button, const QColor &color) {
  if (!button) {
    return;
  }
  QPalette pal = button->palette();
  pal.setColor(QPalette::Button, color);
  const QColor text =
      color.lightnessF() < 0.45f ? QColor(Qt::white) : QColor(Qt::black);
  pal.setColor(QPalette::ButtonText, text);
  button->setPalette(pal);
  button->setAutoFillBackground(true);
  button->setText(color.name(QColor::HexArgb));
}
} // namespace

// ProjectDefaultsSettingPage Implementation
class ProjectDefaultsSettingPage::Impl {
public:
  Impl();
  ~Impl();

  QSpinBox *widthSpinBox_;
  QSpinBox *heightSpinBox_;
  QComboBox *frameRateCombo_;
  QComboBox *workspaceModeCombo_;
  QPushButton *backgroundColorButton_;
  QColor backgroundColor_;
};

ProjectDefaultsSettingPage::Impl::Impl() {}

ProjectDefaultsSettingPage::Impl::~Impl() {}

ProjectDefaultsSettingPage::ProjectDefaultsSettingPage(QWidget *parent)
    : QWidget(parent), impl_(new Impl()) {
  setAccessibleName(QStringLiteral("Project default settings"));
  setAccessibleDescription(QStringLiteral("Configure default composition size, frame rate, workspace, and background color"));
  auto *mainLayout = new QVBoxLayout(this);

  auto *group = new QGroupBox("Project Defaults", this);
  auto *groupLayout = new QVBoxLayout(group);

  auto *sizeLayout = new QHBoxLayout();
  sizeLayout->addWidget(new QLabel("Default Composition Size:", this));
  impl_->widthSpinBox_ = new QSpinBox(this);
  impl_->widthSpinBox_->setAccessibleName(QStringLiteral("Default composition width"));
  impl_->widthSpinBox_->setAccessibleDescription(QStringLiteral("Default composition width in pixels"));
  impl_->widthSpinBox_->setRange(1, 16384);
  impl_->widthSpinBox_->setSuffix(" px");
  impl_->widthSpinBox_->setValue(1920);
  sizeLayout->addWidget(impl_->widthSpinBox_);
  sizeLayout->addWidget(new QLabel("x", this));
  impl_->heightSpinBox_ = new QSpinBox(this);
  impl_->heightSpinBox_->setAccessibleName(QStringLiteral("Default composition height"));
  impl_->heightSpinBox_->setAccessibleDescription(QStringLiteral("Default composition height in pixels"));
  impl_->heightSpinBox_->setRange(1, 16384);
  impl_->heightSpinBox_->setSuffix(" px");
  impl_->heightSpinBox_->setValue(1080);
  sizeLayout->addWidget(impl_->heightSpinBox_);
  sizeLayout->addStretch();
  groupLayout->addLayout(sizeLayout);

  auto *fpsLayout = new QHBoxLayout();
  fpsLayout->addWidget(new QLabel("Default Frame Rate:", this));
  impl_->frameRateCombo_ = new QComboBox(this);
  impl_->frameRateCombo_->setAccessibleName(QStringLiteral("Default composition frame rate"));
  impl_->frameRateCombo_->setAccessibleDescription(QStringLiteral("Default frame rate for new compositions"));
  impl_->frameRateCombo_->addItem("23.976 fps", 23.976);
  impl_->frameRateCombo_->addItem("24 fps", 24.0);
  impl_->frameRateCombo_->addItem("25 fps", 25.0);
  impl_->frameRateCombo_->addItem("29.97 fps", 29.97);
  impl_->frameRateCombo_->addItem("30 fps", 30.0);
  impl_->frameRateCombo_->addItem("50 fps", 50.0);
  impl_->frameRateCombo_->addItem("59.94 fps", 59.94);
  impl_->frameRateCombo_->addItem("60 fps", 60.0);
  fpsLayout->addWidget(impl_->frameRateCombo_);
  fpsLayout->addStretch();
  groupLayout->addLayout(fpsLayout);

  auto *workspaceLayout = new QHBoxLayout();
  workspaceLayout->addWidget(new QLabel("Default Workspace:", this));
  impl_->workspaceModeCombo_ = new QComboBox(this);
  impl_->workspaceModeCombo_->setAccessibleName(QStringLiteral("Default workspace"));
  impl_->workspaceModeCombo_->setAccessibleDescription(QStringLiteral("Initial workspace mode for new projects"));
  for (const auto &info : Artifact::workspaceModeInfos()) {
    impl_->workspaceModeCombo_->addItem(info.label);
  }
  workspaceLayout->addWidget(impl_->workspaceModeCombo_);
  workspaceLayout->addStretch();
  groupLayout->addLayout(workspaceLayout);

  auto *bgLayout = new QHBoxLayout();
  bgLayout->addWidget(new QLabel("Default Composition Background Color:", this));
  impl_->backgroundColorButton_ = new QPushButton(this);
  impl_->backgroundColorButton_->setAccessibleName(QStringLiteral("Default composition background color"));
  impl_->backgroundColorButton_->setAccessibleDescription(QStringLiteral("Choose the default background color for new compositions"));
  bgLayout->addWidget(impl_->backgroundColorButton_);
  bgLayout->addStretch();
  groupLayout->addLayout(bgLayout);

  mainLayout->addWidget(group);
  mainLayout->addStretch();

  QObject::connect(impl_->backgroundColorButton_, &QPushButton::clicked, this,
                   [this]() {
                     ArtifactWidgets::FloatColorPicker picker(this);
                     picker.setWindowTitle(
                         QStringLiteral("Select Default Composition Background Color"));
                     picker.setInitialColor(ArtifactCore::FloatColor(
                         impl_->backgroundColor_.redF(),
                         impl_->backgroundColor_.greenF(),
                         impl_->backgroundColor_.blueF(),
                         impl_->backgroundColor_.alphaF()));
                     if (picker.exec() != QDialog::Accepted) {
                       return;
                     }
                     const ArtifactCore::FloatColor picked = picker.getColor();
                     const QColor color = QColor::fromRgbF(
                         picked.r(), picked.g(), picked.b(), picked.a());
                     if (!color.isValid()) {
                       return;
                     }
                     impl_->backgroundColor_ = color;
                     setButtonColor(impl_->backgroundColorButton_, color);
                   });

  loadSettings();
}

void ProjectDefaultsSettingPage::loadSettings() {
  auto *settings = ArtifactAppSettings::instance();
  if (!settings || !impl_) {
    return;
  }
  impl_->widthSpinBox_->setValue(settings->projectDefaultCompositionWidth());
  impl_->heightSpinBox_->setValue(settings->projectDefaultCompositionHeight());
  impl_->frameRateCombo_->setCurrentText(
      frameRateLabelFor(settings->projectDefaultCompositionFrameRate()));
  const auto modeInfo =
      Artifact::workspaceModeInfoForText(settings->projectDefaultWorkspaceModeText());
  impl_->workspaceModeCombo_->setCurrentText(modeInfo.label);
  impl_->backgroundColor_ =
      QColor(settings->projectDefaultCompositionBackgroundColor());
  if (!impl_->backgroundColor_.isValid()) {
    impl_->backgroundColor_ = QColor(0, 0, 0);
  }
  setButtonColor(impl_->backgroundColorButton_, impl_->backgroundColor_);
}

void ProjectDefaultsSettingPage::saveSettings() {
  auto *settings = ArtifactAppSettings::instance();
  if (!settings || !impl_) {
    return;
  }
  settings->setProjectDefaultCompositionWidth(impl_->widthSpinBox_->value());
  settings->setProjectDefaultCompositionHeight(impl_->heightSpinBox_->value());
  settings->setProjectDefaultCompositionFrameRate(frameRateValueForLabel(
      impl_->frameRateCombo_->currentText(),
      settings->projectDefaultCompositionFrameRate()));
  settings->setProjectDefaultWorkspaceModeText(workspaceModeValueForLabel(
      impl_->workspaceModeCombo_->currentText(),
      settings->projectDefaultWorkspaceModeText()));
  settings->setProjectDefaultCompositionBackgroundColor(
      impl_->backgroundColor_.name(QColor::HexArgb));
}

QList<SettingItemInfo> ProjectDefaultsSettingPage::searchableItems() const {
  QList<SettingItemInfo> items;
  if (!impl_) {
    return items;
  }

  items.push_back({"Default Composition Size",
                   "Default width and height for new compositions",
                   "Project Defaults", impl_->widthSpinBox_});
  items.push_back({"Default Frame Rate",
                   "Default frame rate for new compositions",
                   "Project Defaults", impl_->frameRateCombo_});
  items.push_back({"Default Workspace",
                   "Initial workspace mode used for new projects",
                   "Project Defaults", impl_->workspaceModeCombo_});
  items.push_back({"Default Background Color",
                   "Default background color for new compositions only",
                   "Project Defaults", impl_->backgroundColorButton_});
  return items;
}

ProjectDefaultsSettingPage::~ProjectDefaultsSettingPage() { delete impl_; }

// CompositionSettingPage Implementation
class CompositionSettingPage::Impl {
public:
  Impl();
  ~Impl();

  QCheckBox *showGizmoDuringDragCheckBox_;
};

CompositionSettingPage::Impl::Impl() : showGizmoDuringDragCheckBox_(nullptr) {}

CompositionSettingPage::Impl::~Impl() {}

CompositionSettingPage::CompositionSettingPage(QWidget *parent)
    : QWidget(parent), impl_(new Impl()) {
  setAccessibleName(QStringLiteral("Composition view settings"));
  setAccessibleDescription(QStringLiteral("Configure composition viewport interaction behavior"));
  auto *mainLayout = new QVBoxLayout(this);

  auto *group = new QGroupBox("Composition View", this);
  auto *groupLayout = new QVBoxLayout(group);

  impl_->showGizmoDuringDragCheckBox_ = new QCheckBox(
      "Show transform gizmo while dragging", this);
  impl_->showGizmoDuringDragCheckBox_->setAccessibleName(QStringLiteral("Show transform gizmo while dragging"));
  impl_->showGizmoDuringDragCheckBox_->setAccessibleDescription(QStringLiteral("Display the transform gizmo during drag operations in the composition view"));
  groupLayout->addWidget(impl_->showGizmoDuringDragCheckBox_);

  mainLayout->addWidget(group);
  mainLayout->addStretch();

  loadSettings();
}

void CompositionSettingPage::loadSettings() {
  auto *settings = ArtifactAppSettings::instance();
  if (!settings || !impl_) {
    return;
  }
  impl_->showGizmoDuringDragCheckBox_->setChecked(
      settings->compositionShowGizmoDuringDrag());
}

void CompositionSettingPage::saveSettings() {
  auto *settings = ArtifactAppSettings::instance();
  if (!settings || !impl_) {
    return;
  }
  settings->setCompositionShowGizmoDuringDrag(
      impl_->showGizmoDuringDragCheckBox_->isChecked());
}

QList<SettingItemInfo> CompositionSettingPage::searchableItems() const {
  QList<SettingItemInfo> items;
  if (!impl_) {
    return items;
  }
  items.push_back({"Show transform gizmo while dragging",
                   "Keep the transform gizmo visible while moving layers",
                   "Composition View", impl_->showGizmoDuringDragCheckBox_});
  return items;
}

CompositionSettingPage::~CompositionSettingPage() { delete impl_; }

LabelColorSettingWidget::LabelColorSettingWidget(const QString &labelname,
                                                 const QColor &color,
                                                 QWidget *parent /*= NULL*/) {}

LabelColorSettingWidget::~LabelColorSettingWidget() {}

// MemoryAndCpuSettingPage Implementation
class MemoryAndCpuSettingPage::Impl {
public:
  Impl()
      : memoryUsageBar_(nullptr), memoryLabel_(nullptr), cpuUsageBar_(nullptr),
        cpuLabel_(nullptr), workerThreadsSpinBox_(nullptr),
        autoTuneButton_(nullptr), clearCacheButton_(nullptr),
        updateTimer_(nullptr), prevProcessTimeMs_(0), prevTickMs_(0),
        processorCount_(1) {}
  ~Impl() {}

  QProgressBar *memoryUsageBar_;
  QLabel *memoryLabel_;
  QProgressBar *cpuUsageBar_;
  QLabel *cpuLabel_;
  QSpinBox *workerThreadsSpinBox_;
  QPushButton *autoTuneButton_;
  QPushButton *clearCacheButton_;
  QTimer *updateTimer_;

  // for CPU calculation (Windows)
  unsigned long long prevProcessTimeMs_;
  unsigned long long prevTickMs_;
  int processorCount_;

  struct CacheClearStats {
    int removedFiles = 0;
    int removedDirectories = 0;
    int failedPaths = 0;
  };

  void initializeProcessorCount() {
#ifdef Q_OS_WIN
    SYSTEM_INFO sysInfo;
    GetSystemInfo(&sysInfo);
    processorCount_ = (int)sysInfo.dwNumberOfProcessors;
    if (processorCount_ < 1)
      processorCount_ = 1;
#else
    processorCount_ = QThread::idealThreadCount();
    if (processorCount_ < 1)
      processorCount_ = 1;
#endif
  }

  unsigned long long fileTimeToMs(const FILETIME &ft) {
    unsigned long long high = (unsigned long long)ft.dwHighDateTime;
    unsigned long long low = (unsigned long long)ft.dwLowDateTime;
    unsigned long long val100ns = (high << 32) | low; // 100-ns intervals
    return val100ns / 10000ULL;                       // to ms
  }

  void updateStats(QWidget *parent) {
#ifdef Q_OS_WIN
    // Memory (system)
    MEMORYSTATUSEX memx;
    memx.dwLength = sizeof(memx);
    GlobalMemoryStatusEx(&memx);
    unsigned long long totalPhys = memx.ullTotalPhys;
    unsigned long long availPhys = memx.ullAvailPhys;
    unsigned long long usedPhys = totalPhys - availPhys;
    int memPercent = 0;
    if (totalPhys > 0)
      memPercent = int((usedPhys * 100ULL) / totalPhys);

    if (memoryUsageBar_)
      memoryUsageBar_->setValue(memPercent);
    if (memoryLabel_)
      memoryLabel_->setText(QString("%1 / %2 (%3%)")
                                .arg(QString::number(usedPhys / (1024 * 1024)))
                                .arg(QString::number(totalPhys / (1024 * 1024)))
                                .arg(memPercent));

    // CPU (process percentage)
    FILETIME ftCreation, ftExit, ftKernel, ftUser;
    if (GetProcessTimes(GetCurrentProcess(), &ftCreation, &ftExit, &ftKernel,
                        &ftUser)) {
      unsigned long long procMs = fileTimeToMs(ftKernel) + fileTimeToMs(ftUser);
      unsigned long long curTick = GetTickCount64();

      if (prevTickMs_ == 0) {
        prevTickMs_ = curTick;
        prevProcessTimeMs_ = procMs;
      }

      unsigned long long deltaProc = procMs - prevProcessTimeMs_;
      unsigned long long deltaTime = curTick - prevTickMs_;

      double cpuPercent = 0.0;
      if (deltaTime > 0) {
        cpuPercent = (double)deltaProc / (double)deltaTime /
                     (double)processorCount_ * 100.0;
        if (cpuPercent < 0.0)
          cpuPercent = 0.0;
        if (cpuPercent > 100.0)
          cpuPercent = 100.0 * processorCount_; // clamp high but allow
                                                // multi-core >100 theoretical
      }

      int cpuInt = int(cpuPercent);
      if (cpuUsageBar_)
        cpuUsageBar_->setValue(qBound(0, cpuInt, 100));
      if (cpuLabel_)
        cpuLabel_->setText(
            QString("%1% (process)").arg(QString::number(cpuPercent, 'f', 1)));

      prevProcessTimeMs_ = procMs;
      prevTickMs_ = curTick;
    }
#else
    Q_UNUSED(parent);
    // Non-Windows platforms: show placeholders
    if (memoryUsageBar_)
      memoryUsageBar_->setValue(0);
    if (memoryLabel_)
      memoryLabel_->setText("N/A");
    if (cpuUsageBar_)
      cpuUsageBar_->setValue(0);
    if (cpuLabel_)
      cpuLabel_->setText("N/A");
#endif
  }

  void clearPathRecursive(const QString &path, CacheClearStats &stats) {
    QFileInfo info(path);
    if (!info.exists()) {
      return;
    }

    if (info.isDir()) {
      QDir dir(path);
      const QFileInfoList entries =
          dir.entryInfoList(QDir::NoDotAndDotDot | QDir::AllEntries);
      for (const QFileInfo &entry : entries) {
        clearPathRecursive(entry.absoluteFilePath(), stats);
      }
      QDir parentDir = info.dir();
      if (parentDir.rmdir(info.fileName())) {
        ++stats.removedDirectories;
      } else {
        ++stats.failedPaths;
      }
      return;
    }

    if (QFile::remove(path)) {
      ++stats.removedFiles;
    } else {
      ++stats.failedPaths;
    }
  }

  CacheClearStats clearAppCaches() {
    CacheClearStats stats;
    const QString appDataDir =
        QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (appDataDir.isEmpty()) {
      return stats;
    }

    const QStringList cacheTargets = {
        QDir(appDataDir).filePath(QStringLiteral("ProxyCache")),
        QDir(appDataDir).filePath(QStringLiteral("Recovery")),
        QDir(appDataDir)
            .filePath(QStringLiteral("RecoveredProject.artifact.json"))};

    for (const QString &target : cacheTargets) {
      clearPathRecursive(target, stats);
    }

    return stats;
  }
};

MemoryAndCpuSettingPage::MemoryAndCpuSettingPage(QWidget *parent /*= nullptr*/)
    : QWidget(parent), impl_(new Impl()) {
  setAccessibleName(QStringLiteral("Memory and performance settings"));
  setAccessibleDescription(QStringLiteral("Monitor resource usage and configure worker thread performance"));
  impl_->initializeProcessorCount();

  auto *mainLayout = new QVBoxLayout(this);

  auto *statsGroup = new QGroupBox("Memory & CPU", this);
  auto *statsLayout = new QVBoxLayout(statsGroup);

  // Memory
  auto *memLayout = new QHBoxLayout();
  memLayout->addWidget(new QLabel("System Memory Usage:", this));
  impl_->memoryUsageBar_ = new QProgressBar(this);
  impl_->memoryUsageBar_->setAccessibleName(QStringLiteral("System memory usage"));
  impl_->memoryUsageBar_->setAccessibleDescription(QStringLiteral("Current system memory usage percentage"));
  impl_->memoryUsageBar_->setRange(0, 100);
  impl_->memoryUsageBar_->setValue(0);
  impl_->memoryUsageBar_->setTextVisible(false);
  memLayout->addWidget(impl_->memoryUsageBar_);
  impl_->memoryLabel_ = new QLabel("", this);
  memLayout->addWidget(impl_->memoryLabel_);
  statsLayout->addLayout(memLayout);

  // CPU
  auto *cpuLayout = new QHBoxLayout();
  cpuLayout->addWidget(new QLabel("CPU Usage:", this));
  impl_->cpuUsageBar_ = new QProgressBar(this);
  impl_->cpuUsageBar_->setAccessibleName(QStringLiteral("CPU usage"));
  impl_->cpuUsageBar_->setAccessibleDescription(QStringLiteral("Current processor usage percentage"));
  impl_->cpuUsageBar_->setRange(0, 100);
  impl_->cpuUsageBar_->setValue(0);
  impl_->cpuUsageBar_->setTextVisible(false);
  cpuLayout->addWidget(impl_->cpuUsageBar_);
  impl_->cpuLabel_ = new QLabel("", this);
  cpuLayout->addWidget(impl_->cpuLabel_);
  statsLayout->addLayout(cpuLayout);

  mainLayout->addWidget(statsGroup);

  // Performance tuning
  auto *perfGroup = new QGroupBox("Performance Tuning", this);
  auto *perfLayout = new QVBoxLayout(perfGroup);

  auto *threadLayout = new QHBoxLayout();
  threadLayout->addWidget(new QLabel("Worker Threads:", this));
  impl_->workerThreadsSpinBox_ = new QSpinBox(this);
  impl_->workerThreadsSpinBox_->setAccessibleName(QStringLiteral("Worker threads"));
  impl_->workerThreadsSpinBox_->setAccessibleDescription(QStringLiteral("Number of worker threads used for rendering"));
  impl_->workerThreadsSpinBox_->setRange(1, qMax(1, impl_->processorCount_));
  impl_->workerThreadsSpinBox_->setValue(qMax(1, impl_->processorCount_ - 1));
  threadLayout->addWidget(impl_->workerThreadsSpinBox_);
  impl_->autoTuneButton_ = new QPushButton("Auto-tune", this);
  impl_->autoTuneButton_->setAccessibleName(QStringLiteral("Auto-tune worker threads"));
  impl_->autoTuneButton_->setAccessibleDescription(QStringLiteral("Set the recommended worker thread count"));
  threadLayout->addWidget(impl_->autoTuneButton_);
  threadLayout->addStretch();
  perfLayout->addLayout(threadLayout);

  impl_->clearCacheButton_ = new QPushButton("Clear Cache", this);
  impl_->clearCacheButton_->setAccessibleName(QStringLiteral("Clear application cache"));
  impl_->clearCacheButton_->setAccessibleDescription(QStringLiteral("Remove generated proxy and recovery cache files"));
  perfLayout->addWidget(impl_->clearCacheButton_);

  mainLayout->addWidget(perfGroup);

  mainLayout->addStretch();

  // Timer for live updates
  impl_->updateTimer_ = new QTimer(this);
  connect(impl_->updateTimer_, &QTimer::timeout, this,
          [this]() { impl_->updateStats(this); });
  impl_->updateTimer_->start(1000);

  // Auto-tune handler
  connect(impl_->autoTuneButton_, &QPushButton::clicked, this, [this]() {
    int recommended = qMax(1, impl_->processorCount_ - 1);
    impl_->workerThreadsSpinBox_->setValue(recommended);
  });

  // Clear cache handler
  connect(impl_->clearCacheButton_, &QPushButton::clicked, this, [this]() {
    if (!Artifact::ArtifactMessageBox::confirmDelete(
            this, QStringLiteral("Clear Cache"),
            QStringLiteral(
                "Remove generated proxy and recovery cache files?"))) {
      return;
    }

    const Impl::CacheClearStats stats = impl_->clearAppCaches();
    const QString summary =
        QStringLiteral("Removed %1 file(s), %2 folder(s).%3")
            .arg(stats.removedFiles)
            .arg(stats.removedDirectories)
            .arg(stats.failedPaths > 0
                     ? QStringLiteral("\nFailed to remove %1 path(s).")
                           .arg(stats.failedPaths)
                     : QString());
    QMessageBox::information(this, QStringLiteral("Clear Cache"), summary);
  });

  loadSettings();
}

void MemoryAndCpuSettingPage::loadSettings() {
  auto *settings = ArtifactAppSettings::instance();
  int count = settings->renderThreadCount();
  if (count <= 0) {
    impl_->workerThreadsSpinBox_->setValue(qMax(1, impl_->processorCount_ - 1));
  } else {
    impl_->workerThreadsSpinBox_->setValue(count);
  }
}

void MemoryAndCpuSettingPage::saveSettings() {
  auto *settings = ArtifactAppSettings::instance();
  settings->setRenderThreadCount(impl_->workerThreadsSpinBox_->value());
}

QList<SettingItemInfo> MemoryAndCpuSettingPage::searchableItems() const {
  return {};
}

MemoryAndCpuSettingPage::~MemoryAndCpuSettingPage() {
  if (impl_) {
    if (impl_->updateTimer_)
      impl_->updateTimer_->stop();
    delete impl_;
    impl_ = nullptr;
  }
}

void ImportSettingPage::loadSettings() {
  auto *settings = ArtifactAppSettings::instance();
  if (!settings || !impl_) {
    return;
  }
  impl_->defaultFrameRateCombo_->setCurrentText(settings->importDefaultFrameRateText());
  impl_->colorSpaceCombo_->setCurrentText(settings->importColorSpaceText());
  impl_->audioSampleRateCombo_->setCurrentText(settings->importAudioSampleRateText());
  impl_->autoDetectAlphaCheckBox_->setChecked(settings->importAutoDetectAlpha());
  impl_->interpretFootageCheckBox_->setChecked(settings->importInterpretFootage());
  impl_->fieldOrderCombo_->setCurrentText(settings->importFieldOrderText());
  impl_->stillDurationSpinBox_->setValue(settings->importStillImageDurationSeconds());
  impl_->createCompositionCheckBox_->setChecked(settings->importCreateCompositionOnImport());
}

void ImportSettingPage::saveSettings() {
  auto *settings = ArtifactAppSettings::instance();
  if (!settings || !impl_) {
    return;
  }
  settings->setImportDefaultFrameRateText(impl_->defaultFrameRateCombo_->currentText());
  settings->setImportColorSpaceText(impl_->colorSpaceCombo_->currentText());
  settings->setImportAudioSampleRateText(impl_->audioSampleRateCombo_->currentText());
  settings->setImportAutoDetectAlpha(impl_->autoDetectAlphaCheckBox_->isChecked());
  settings->setImportInterpretFootage(impl_->interpretFootageCheckBox_->isChecked());
  settings->setImportFieldOrderText(impl_->fieldOrderCombo_->currentText());
  settings->setImportStillImageDurationSeconds(impl_->stillDurationSpinBox_->value());
  settings->setImportCreateCompositionOnImport(impl_->createCompositionCheckBox_->isChecked());
}
QList<SettingItemInfo> ImportSettingPage::searchableItems() const { return {}; }
void PreviewSettingPage::loadSettings() {
  auto *settings = ArtifactAppSettings::instance();
  if (!settings || !impl_) {
    return;
  }
  impl_->previewQualityCombo_->setCurrentText(settings->previewQualityText());
  impl_->previewResolutionSlider_->setValue(settings->previewResolutionPercent());
  impl_->enableCacheCheckBox_->setChecked(settings->previewEnableRamCache());
  impl_->cacheSizeSpinBox_->setValue(settings->previewCacheSizeMB());
  impl_->enableDiskCacheCheckBox_->setChecked(settings->previewEnableDiskCache());
  impl_->generateThumbnailsCheckBox_->setChecked(settings->previewGenerateThumbnails());
  impl_->thumbnailQualityCombo_->setCurrentText(settings->previewThumbnailQualityText());
  impl_->enableGPUCheckBox_->setChecked(settings->previewEnableGpuAcceleration());
  impl_->gpuDeviceCombo_->setCurrentText(settings->previewGpuDeviceText());
}

void PreviewSettingPage::saveSettings() {
  auto *settings = ArtifactAppSettings::instance();
  if (!settings || !impl_) {
    return;
  }
  settings->setPreviewQualityText(impl_->previewQualityCombo_->currentText());
  settings->setPreviewResolutionPercent(impl_->previewResolutionSlider_->value());
  settings->setPreviewEnableRamCache(impl_->enableCacheCheckBox_->isChecked());
  settings->setPreviewCacheSizeMB(impl_->cacheSizeSpinBox_->value());
  settings->setPreviewEnableDiskCache(impl_->enableDiskCacheCheckBox_->isChecked());
  settings->setPreviewGenerateThumbnails(impl_->generateThumbnailsCheckBox_->isChecked());
  settings->setPreviewThumbnailQualityText(impl_->thumbnailQualityCombo_->currentText());
  settings->setPreviewEnableGpuAcceleration(impl_->enableGPUCheckBox_->isChecked());
  settings->setPreviewGpuDeviceText(impl_->gpuDeviceCombo_->currentText());
}
QList<SettingItemInfo> PreviewSettingPage::searchableItems() const {
  return {};
}
class ShortcutSettingPage::Impl {
public:
  Impl();
  ~Impl();

  QVBoxLayout *layout_ = nullptr;
  QLabel *descriptionLabel_ = nullptr;
  QLabel *contextsLabel_ = nullptr;
  QTableWidget *table_ = nullptr;
  QPushButton *importPresetButton_ = nullptr;
  QPushButton *exportPresetButton_ = nullptr;
  QPushButton *resetDefaultsButton_ = nullptr;
};

ShortcutSettingPage::Impl::Impl() = default;
ShortcutSettingPage::Impl::~Impl() = default;

ShortcutSettingPage::ShortcutSettingPage(QWidget *parent)
    : QWidget(parent), impl_(new Impl()) {
  setAccessibleName(QStringLiteral("Shortcut settings"));
  setAccessibleDescription(QStringLiteral("Review and manage shortcut bindings for application and workspace commands"));
  impl_->layout_ = new QVBoxLayout(this);
  impl_->layout_->setContentsMargins(12, 12, 12, 12);
  impl_->layout_->setSpacing(10);

  impl_->descriptionLabel_ = new QLabel(this);
  impl_->descriptionLabel_->setWordWrap(true);
  impl_->descriptionLabel_->setText(
      QStringLiteral("Shared shortcut bindings used by timeline, viewport, and app-level commands."));
  impl_->layout_->addWidget(impl_->descriptionLabel_);

  impl_->contextsLabel_ = new QLabel(this);
  impl_->contextsLabel_->setWordWrap(true);
  impl_->contextsLabel_->setText(QStringLiteral(
      "Active shortcut contexts: Global, Workspace.Timeline, Workspace.Project, "
      "Viewport.Composition, Panel.LayerTree, Panel.AssetBrowser, Panel.Inspector."));
  impl_->layout_->addWidget(impl_->contextsLabel_);

  impl_->table_ = new QTableWidget(this);
  impl_->table_->setAccessibleName(QStringLiteral("Shortcut bindings table"));
  impl_->table_->setAccessibleDescription(QStringLiteral("Read-only list of shortcut categories, actions, defaults, and current bindings"));
  impl_->table_->setColumnCount(4);
  impl_->table_->setHorizontalHeaderLabels({
      QStringLiteral("Category"),
      QStringLiteral("Action"),
      QStringLiteral("Default"),
      QStringLiteral("Current"),
  });
  impl_->table_->horizontalHeader()->setStretchLastSection(true);
  impl_->table_->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
  impl_->table_->verticalHeader()->setVisible(false);
  impl_->table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
  impl_->table_->setSelectionBehavior(QAbstractItemView::SelectRows);
  impl_->table_->setSelectionMode(QAbstractItemView::SingleSelection);
  impl_->layout_->addWidget(impl_->table_, 1);

  auto *footerLayout = new QHBoxLayout();
  footerLayout->addWidget(impl_->importPresetButton_ = new QPushButton(QStringLiteral("Import Preset"), this));
  footerLayout->addWidget(impl_->exportPresetButton_ = new QPushButton(QStringLiteral("Export Preset"), this));
  footerLayout->addWidget(impl_->resetDefaultsButton_ = new QPushButton(QStringLiteral("Reset to Defaults"), this));
  impl_->importPresetButton_->setAccessibleName(QStringLiteral("Import shortcut preset"));
  impl_->importPresetButton_->setAccessibleDescription(QStringLiteral("Load shortcut bindings from a preset"));
  impl_->exportPresetButton_->setAccessibleName(QStringLiteral("Export shortcut preset"));
  impl_->exportPresetButton_->setAccessibleDescription(QStringLiteral("Save current shortcut bindings as a preset"));
  impl_->resetDefaultsButton_->setAccessibleName(QStringLiteral("Reset shortcuts to defaults"));
  impl_->resetDefaultsButton_->setAccessibleDescription(QStringLiteral("Restore default shortcut bindings"));
  footerLayout->addStretch();
  impl_->importPresetButton_->installEventFilter(this);
  impl_->exportPresetButton_->installEventFilter(this);
  impl_->resetDefaultsButton_->installEventFilter(this);
  impl_->layout_->addLayout(footerLayout);

  loadSettings();
}

ShortcutSettingPage::~ShortcutSettingPage() { delete impl_; }

QVector<QWidget *> ShortcutSettingPage::settingWidgets() const {
  return {impl_ ? impl_->table_ : nullptr};
}

void ShortcutSettingPage::resetToDefaults() {
  if (!impl_ || !impl_->table_) {
    return;
  }

  const auto ids = ArtifactCore::allShortcutIds();
  const auto &bindings = ArtifactCore::ShortcutBindings::instance();
  for (int row = 0; row < static_cast<int>(ids.size()); ++row) {
    const auto id = ids[static_cast<std::size_t>(row)];
    if (auto *editor = qobject_cast<QKeySequenceEdit *>(impl_->table_->cellWidget(row, 3))) {
      editor->setKeySequence(bindings.defaultShortcut(id));
    }
  }
}

void ShortcutSettingPage::applyTableToBindings() {
  if (!impl_ || !impl_->table_) {
    return;
  }

  const auto ids = ArtifactCore::allShortcutIds();
  auto &bindings = ArtifactCore::ShortcutBindings::instance();
  for (int row = 0; row < static_cast<int>(ids.size()); ++row) {
    const auto id = ids[static_cast<std::size_t>(row)];
    if (auto *editor = qobject_cast<QKeySequenceEdit *>(impl_->table_->cellWidget(row, 3))) {
      bindings.setShortcut(id, editor->keySequence());
    }
  }
}

void ShortcutSettingPage::exportPreset() {
  if (!impl_) {
    return;
  }

  const QString defaultDir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
  const QString defaultPath = defaultDir.isEmpty()
                                  ? QStringLiteral("shortcut-preset.json")
                                  : QDir(defaultDir).filePath(QStringLiteral("shortcut-preset.json"));
  const QString path = QFileDialog::getSaveFileName(
      this, QStringLiteral("Export Shortcut Preset"), defaultPath,
      QStringLiteral("Shortcut Preset (*.json)"));
  if (path.isEmpty()) {
    return;
  }

  QFile file(path);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
    QMessageBox::warning(this, QStringLiteral("Export Shortcut Preset"),
                         QStringLiteral("Could not open the preset file for writing."));
    return;
  }

  applyTableToBindings();
  const QJsonDocument doc(ArtifactCore::ShortcutBindings::instance().toJson());
  file.write(doc.toJson(QJsonDocument::Indented));
}

void ShortcutSettingPage::importPreset() {
  if (!impl_) {
    return;
  }

  const QString defaultDir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
  const QString defaultPath = defaultDir.isEmpty()
                                  ? QString()
                                  : QDir(defaultDir).filePath(QStringLiteral("shortcut-preset.json"));
  const QString path = QFileDialog::getOpenFileName(
      this, QStringLiteral("Import Shortcut Preset"), defaultPath,
      QStringLiteral("Shortcut Preset (*.json)"));
  if (path.isEmpty()) {
    return;
  }

  QFile file(path);
  if (!file.open(QIODevice::ReadOnly)) {
    QMessageBox::warning(this, QStringLiteral("Import Shortcut Preset"),
                         QStringLiteral("Could not open the preset file for reading."));
    return;
  }

  const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
  if (!doc.isObject()) {
    QMessageBox::warning(this, QStringLiteral("Import Shortcut Preset"),
                         QStringLiteral("The preset file does not contain a valid JSON object."));
    return;
  }

  auto &bindings = ArtifactCore::ShortcutBindings::instance();
  if (!bindings.loadFromJson(doc.object())) {
    QMessageBox::warning(this, QStringLiteral("Import Shortcut Preset"),
                         QStringLiteral("The preset file did not contain any recognized shortcuts."));
    return;
  }

  loadSettings();
}

void ShortcutSettingPage::loadSettings() {
  if (!impl_ || !impl_->table_) {
    return;
  }

  const auto ids = ArtifactCore::allShortcutIds();
  const auto &bindings = ArtifactCore::ShortcutBindings::instance();
  impl_->table_->setRowCount(static_cast<int>(ids.size()));

  for (int row = 0; row < static_cast<int>(ids.size()); ++row) {
    const auto id = ids[static_cast<std::size_t>(row)];
    const bool isTimeline = static_cast<int>(id) >= static_cast<int>(ArtifactCore::ShortcutId::TimelineCopySelectedKeyframes);
    const QString category = isTimeline ? QStringLiteral("Timeline") : QStringLiteral("Core");
    const QString actionLabel = ArtifactCore::shortcutDisplayName(id);
    const QString defaultShortcut = bindings.defaultShortcut(id).toString(QKeySequence::NativeText);

    impl_->table_->setItem(row, 0, new QTableWidgetItem(category));
    impl_->table_->setItem(row, 1, new QTableWidgetItem(actionLabel));
    impl_->table_->setItem(row, 2, new QTableWidgetItem(defaultShortcut));
    auto *editor = new QKeySequenceEdit(impl_->table_);
    editor->setKeySequence(bindings.shortcut(id));
    editor->setMaximumWidth(240);
    impl_->table_->setCellWidget(row, 3, editor);
  }
  impl_->table_->resizeColumnsToContents();
}

void ShortcutSettingPage::saveSettings() {
  if (!impl_ || !impl_->table_) {
    return;
  }

  applyTableToBindings();
}

bool ShortcutSettingPage::eventFilter(QObject *watched, QEvent *event) {
  if (impl_ && watched && event) {
    if (watched == impl_->importPresetButton_) {
      const auto type = event->type();
      if (type == QEvent::MouseButtonRelease) {
        auto *mouseEvent = static_cast<QMouseEvent *>(event);
        if (mouseEvent && mouseEvent->button() == Qt::LeftButton) {
          importPreset();
          return true;
        }
      } else if (type == QEvent::KeyRelease) {
        auto *keyEvent = static_cast<QKeyEvent *>(event);
        if (keyEvent && (keyEvent->key() == Qt::Key_Return ||
                         keyEvent->key() == Qt::Key_Enter ||
                         keyEvent->key() == Qt::Key_Space)) {
          importPreset();
          return true;
        }
      }
    } else if (watched == impl_->exportPresetButton_) {
      const auto type = event->type();
      if (type == QEvent::MouseButtonRelease) {
        auto *mouseEvent = static_cast<QMouseEvent *>(event);
        if (mouseEvent && mouseEvent->button() == Qt::LeftButton) {
          exportPreset();
          return true;
        }
      } else if (type == QEvent::KeyRelease) {
        auto *keyEvent = static_cast<QKeyEvent *>(event);
        if (keyEvent && (keyEvent->key() == Qt::Key_Return ||
                         keyEvent->key() == Qt::Key_Enter ||
                         keyEvent->key() == Qt::Key_Space)) {
          exportPreset();
          return true;
        }
      }
    } else if (watched == impl_->resetDefaultsButton_) {
      const auto type = event->type();
      if (type == QEvent::MouseButtonRelease) {
        auto *mouseEvent = static_cast<QMouseEvent *>(event);
        if (mouseEvent && mouseEvent->button() == Qt::LeftButton) {
          resetToDefaults();
          return true;
        }
      } else if (type == QEvent::KeyRelease) {
        auto *keyEvent = static_cast<QKeyEvent *>(event);
        if (keyEvent && (keyEvent->key() == Qt::Key_Return ||
                         keyEvent->key() == Qt::Key_Enter ||
                         keyEvent->key() == Qt::Key_Space)) {
          resetToDefaults();
          return true;
        }
      }
    }
  }

  return QObject::eventFilter(watched, event);
}

QList<SettingItemInfo> ShortcutSettingPage::searchableItems() const {
  QList<SettingItemInfo> items;
  const auto ids = ArtifactCore::allShortcutIds();
  const auto &bindings = ArtifactCore::ShortcutBindings::instance();
  for (const auto id : ids) {
    const bool isTimeline = static_cast<int>(id) >= static_cast<int>(ArtifactCore::ShortcutId::TimelineCopySelectedKeyframes);
    items.push_back({
        ArtifactCore::shortcutDisplayName(id),
        bindings.shortcutText(id),
        isTimeline ? QStringLiteral("Timeline") : QStringLiteral("Core"),
        nullptr,
    });
  }
  return items;
}

class ApplicationSettingDialog::Impl {
private:
public:
  Impl();
  ~Impl();
  QListWidget *categoryList_;
  QStackedWidget *settingPages_;
  QDialogButtonBox *buttonBox_;

  GeneralSettingPage *generalPage_;
  ImportSettingPage *importPage_;
  PreviewSettingPage *previewPage_;
  ProjectDefaultsSettingPage *projectPage_;
  CompositionSettingPage *compositionPage_;
  MemoryAndCpuSettingPage *memoryPage_;
  ShortcutSettingPage *shortcutPage_;
  PluginSettingPage *pluginPage_;
  AudioScrubSettingPage *audioScrubPage_;

  void setupUI(ApplicationSettingDialog *dialog);
  void onCategoryChanged(int index);
};

ApplicationSettingDialog::Impl::Impl()
    : categoryList_(nullptr), settingPages_(nullptr), buttonBox_(nullptr),
      generalPage_(nullptr), importPage_(nullptr), previewPage_(nullptr),
      projectPage_(nullptr), compositionPage_(nullptr), memoryPage_(nullptr),
      shortcutPage_(nullptr), pluginPage_(nullptr), audioScrubPage_(nullptr) {}

ApplicationSettingDialog::Impl::~Impl() {}

void ApplicationSettingDialog::Impl::setupUI(ApplicationSettingDialog *dialog) {
  // Main layout
  auto *mainLayout = new QVBoxLayout(dialog);

  // Content area (category list + settings pages)
  auto *contentLayout = new QHBoxLayout();

  // Category list (left side)
  categoryList_ = new QListWidget(dialog);
  categoryList_->setAccessibleName(QStringLiteral("Settings categories"));
  categoryList_->setAccessibleDescription(QStringLiteral("Choose an application settings category"));
  categoryList_->setMaximumWidth(150);
  categoryList_->addItem("General");
  categoryList_->addItem("Import");
  categoryList_->addItem("Preview");
  categoryList_->addItem("Project Defaults");
  categoryList_->addItem("Composition View");
  categoryList_->addItem("Memory & Performance");
  categoryList_->addItem("Shortcuts");
  categoryList_->addItem("Audio Scrubbing");
  categoryList_->addItem("Plugins");
  categoryList_->setCurrentRow(0);
  contentLayout->addWidget(categoryList_);

  // Settings pages (right side)
  settingPages_ = new QStackedWidget(dialog);
  settingPages_->setAccessibleName(QStringLiteral("Settings page"));

  // Add pages
  generalPage_ = new GeneralSettingPage(dialog);
  importPage_ = new ImportSettingPage(dialog);
  previewPage_ = new PreviewSettingPage(dialog);
  projectPage_ = new ProjectDefaultsSettingPage(dialog);
  compositionPage_ = new CompositionSettingPage(dialog);
  memoryPage_ = new MemoryAndCpuSettingPage(dialog);
  shortcutPage_ = new ShortcutSettingPage(dialog);

  settingPages_->addWidget(generalPage_);
  settingPages_->addWidget(importPage_);
  settingPages_->addWidget(previewPage_);
  settingPages_->addWidget(projectPage_);
  settingPages_->addWidget(compositionPage_);
  settingPages_->addWidget(memoryPage_);
  settingPages_->addWidget(shortcutPage_);
  settingPages_->addWidget(audioScrubPage_ = new AudioScrubSettingPage(dialog));
  settingPages_->addWidget(pluginPage_ = new PluginSettingPage(dialog));

  contentLayout->addWidget(settingPages_, 1);

  mainLayout->addLayout(contentLayout);

  // Button box
  buttonBox_ = new QDialogButtonBox(
      QDialogButtonBox::Ok | QDialogButtonBox::Cancel | QDialogButtonBox::Apply,
      dialog);
  buttonBox_->setAccessibleName(QStringLiteral("Application settings actions"));
  if (auto* ok = buttonBox_->button(QDialogButtonBox::Ok)) {
    ok->setAccessibleName(QStringLiteral("Save and close settings"));
    ok->setAccessibleDescription(QStringLiteral("Save changes and close the settings dialog"));
  }
  if (auto* cancel = buttonBox_->button(QDialogButtonBox::Cancel)) {
    cancel->setAccessibleName(QStringLiteral("Cancel settings"));
    cancel->setAccessibleDescription(QStringLiteral("Close without saving changes"));
  }
  if (auto* apply = buttonBox_->button(QDialogButtonBox::Apply)) {
    apply->setAccessibleName(QStringLiteral("Apply settings"));
    apply->setAccessibleDescription(QStringLiteral("Save settings and keep the dialog open"));
  }
  mainLayout->addWidget(buttonBox_);

  // Connect signals
  QObject::connect(categoryList_, &QListWidget::currentRowChanged,
                   [this](int index) { onCategoryChanged(index); });
  QObject::connect(buttonBox_, &QDialogButtonBox::accepted, dialog,
                   &ApplicationSettingDialog::accept);
  QObject::connect(buttonBox_, &QDialogButtonBox::rejected, dialog,
                   &QDialog::reject);
  QObject::connect(buttonBox_->button(QDialogButtonBox::Apply),
                   &QPushButton::clicked, dialog,
                   &ApplicationSettingDialog::saveSettings);
}

void ApplicationSettingDialog::Impl::onCategoryChanged(int index) {
  settingPages_->setCurrentIndex(index);
}

ApplicationSettingDialog::ApplicationSettingDialog(
    QWidget *parent /*= nullptr*/)
    : QDialog(parent), impl_(new Impl) {
  setWindowTitle("Application Settings");
  setAccessibleName(QStringLiteral("Application Settings Dialog"));
  setAccessibleDescription(QStringLiteral("Configure application, import, preview, composition, and accessibility settings"));
  setMinimumSize(700, 500);

  impl_->setupUI(this);
}

ApplicationSettingDialog::~ApplicationSettingDialog() { delete impl_; }

void ApplicationSettingDialog::loadSettings() {
  impl_->generalPage_->loadSettings();
  impl_->importPage_->loadSettings();
  impl_->previewPage_->loadSettings();
  impl_->projectPage_->loadSettings();
  impl_->compositionPage_->loadSettings();
  impl_->memoryPage_->loadSettings();
  if (impl_->shortcutPage_) {
    impl_->shortcutPage_->loadSettings();
  }
  if (impl_->audioScrubPage_) {
    impl_->audioScrubPage_->loadSettings();
  }
}

void ApplicationSettingDialog::saveSettings() {
  impl_->generalPage_->saveSettings();
  impl_->importPage_->saveSettings();
  impl_->previewPage_->saveSettings();
  impl_->projectPage_->saveSettings();
  impl_->compositionPage_->saveSettings();
  impl_->memoryPage_->saveSettings();
  if (impl_->shortcutPage_) {
    impl_->shortcutPage_->saveSettings();
  }
  if (impl_->audioScrubPage_) {
    impl_->audioScrubPage_->saveSettings();
  }

  ArtifactAppSettings::instance()->sync();
}

void ApplicationSettingDialog::accept() {
  saveSettings();
  QDialog::accept();
}

// PluginSettingPage Implementation

W_OBJECT_IMPL(PluginSettingPage)

class PluginSettingPage::Impl {
public:
  Impl();
  ~Impl();
  QTableWidget *pluginTable_;
  QPushButton *refreshButton_;
  QPushButton *openFolderButton_;
  QString pluginDirectory_;
  void loadPlugins(PluginSettingPage *page);
  QStringList getPluginPaths();
};

PluginSettingPage::Impl::Impl() {
  pluginDirectory_ = QCoreApplication::applicationDirPath() + "/plugins";
}
PluginSettingPage::Impl::~Impl() {}

QStringList PluginSettingPage::Impl::getPluginPaths() {
  QStringList paths;
  QDir dir(pluginDirectory_);
  if (dir.exists()) {
    QFileInfoList entries =
        dir.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QFileInfo &entry : entries) {
      if (entry.isDir()) {
        QDir subDir(entry.absoluteFilePath());
        QFileInfoList plugins = subDir.entryInfoList(
            QStringList() << "*.dll" << "*.so" << "*.dylib", QDir::Files);
        for (const QFileInfo &plugin : plugins)
          paths.append(plugin.absoluteFilePath());
      } else if (entry.suffix() == "dll" || entry.suffix() == "so" ||
                 entry.suffix() == "dylib") {
        paths.append(entry.absoluteFilePath());
      }
    }
  }
  return paths;
}

void PluginSettingPage::Impl::loadPlugins(PluginSettingPage *page) {
  if (!pluginTable_)
    return;
  pluginTable_->setRowCount(0);
  QStringList pluginPaths = getPluginPaths();
  for (const QString &path : pluginPaths) {
    QPluginLoader loader(path);
    QJsonObject metaData = loader.metaData();
    int row = pluginTable_->rowCount();
    pluginTable_->insertRow(row);
    QString name = metaData.value("Name").toString();
    if (name.isEmpty()) {
      QFileInfo info(path);
      name = info.baseName();
    }
    pluginTable_->setItem(row, 0, new QTableWidgetItem(name));
    pluginTable_->setItem(
        row, 1, new QTableWidgetItem(metaData.value("Version").toString()));
    QString vendor = metaData.value("Vendor").toString();
    if (vendor.isEmpty())
      vendor = metaData.value("Author").toString();
    pluginTable_->setItem(row, 2, new QTableWidgetItem(vendor));
    pluginTable_->setItem(
        row, 3, new QTableWidgetItem(metaData.value("Description").toString()));
    QString status =
        loader.isLoaded() ? "Loaded" : (loader.load() ? "Loaded" : "Failed");
    pluginTable_->setItem(row, 4, new QTableWidgetItem(status));
    pluginTable_->item(row, 0)->setData(Qt::UserRole, path);
  }
  pluginTable_->resizeColumnsToContents();
}

PluginSettingPage::PluginSettingPage(QWidget *parent)
    : QWidget(parent), impl_(new Impl()) {
  setAccessibleName(QStringLiteral("Plugin settings"));
  setAccessibleDescription(QStringLiteral("View installed plugins and manage plugin loading"));
  auto *mainLayout = new QVBoxLayout(this);
  auto *infoGroup = new QGroupBox("Plugin Directory", this);
  auto *infoLayout = new QHBoxLayout(infoGroup);
  auto *dirLabel = new QLabel(impl_->pluginDirectory_, this);
  {
    QPalette pal = dirLabel->palette();
    pal.setColor(QPalette::WindowText, Qt::gray);
    dirLabel->setPalette(pal);
  }
  infoLayout->addWidget(dirLabel);
  impl_->openFolderButton_ = new QPushButton("Open Folder", this);
  impl_->openFolderButton_->setAccessibleName(QStringLiteral("Open plugin folder"));
  impl_->openFolderButton_->setAccessibleDescription(QStringLiteral("Open the folder containing installed plugins"));
  infoLayout->addWidget(impl_->openFolderButton_);
  mainLayout->addWidget(infoGroup);
  auto *tableGroup = new QGroupBox("Installed Plugins", this);
  auto *tableLayout = new QVBoxLayout(tableGroup);
  impl_->pluginTable_ = new QTableWidget(this);
  impl_->pluginTable_->setAccessibleName(QStringLiteral("Installed plugins table"));
  impl_->pluginTable_->setAccessibleDescription(QStringLiteral("List of installed plugins with name, version, vendor, description, and status"));
  impl_->pluginTable_->setColumnCount(5);
  impl_->pluginTable_->setHorizontalHeaderLabels(
      {"Name", "Version", "Vendor", "Description", "Status"});
  impl_->pluginTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
  impl_->pluginTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
  impl_->pluginTable_->horizontalHeader()->setStretchLastSection(true);
  tableLayout->addWidget(impl_->pluginTable_);
  mainLayout->addWidget(tableGroup);
  auto *buttonLayout = new QHBoxLayout();
  impl_->refreshButton_ = new QPushButton("Refresh", this);
  impl_->refreshButton_->setAccessibleName(QStringLiteral("Refresh plugin list"));
  impl_->refreshButton_->setAccessibleDescription(QStringLiteral("Rescan the plugin folder"));
  buttonLayout->addWidget(impl_->refreshButton_);
  buttonLayout->addStretch();
  auto *unloadButton = new QPushButton("Unload Selected", this);
  unloadButton->setAccessibleName(QStringLiteral("Unload selected plugins"));
  unloadButton->setAccessibleDescription(QStringLiteral("Unload the selected plugins"));
  buttonLayout->addWidget(unloadButton);
  auto *loadButton = new QPushButton("Load Selected", this);
  loadButton->setAccessibleName(QStringLiteral("Load selected plugins"));
  loadButton->setAccessibleDescription(QStringLiteral("Load the selected plugins"));
  buttonLayout->addWidget(loadButton);
  mainLayout->addLayout(buttonLayout);
  impl_->loadPlugins(this);
  connect(impl_->refreshButton_, &QPushButton::clicked, this,
          [this]() { impl_->loadPlugins(this); });
  connect(impl_->openFolderButton_, &QPushButton::clicked, this, [this]() {
    QDesktopServices::openUrl(QUrl::fromLocalFile(impl_->pluginDirectory_));
  });
  connect(unloadButton, &QPushButton::clicked, this, [this]() {
    QModelIndexList selected =
        impl_->pluginTable_->selectionModel()->selectedRows();
    if (selected.isEmpty()) {
      QMessageBox::information(this, "Unload Plugin",
                               "Please select a plugin to unload.");
      return;
    }
    for (const QModelIndex &index : selected) {
      QString path = impl_->pluginTable_->item(index.row(), 0)
                         ->data(Qt::UserRole)
                         .toString();
      QPluginLoader loader(path);
      if (loader.isLoaded())
        loader.unload();
    }
    impl_->loadPlugins(this);
  });
  connect(loadButton, &QPushButton::clicked, this, [this]() {
    QModelIndexList selected =
        impl_->pluginTable_->selectionModel()->selectedRows();
    if (selected.isEmpty()) {
      QMessageBox::information(this, "Load Plugin",
                               "Please select a plugin to load.");
      return;
    }
    for (const QModelIndex &index : selected) {
      QString path = impl_->pluginTable_->item(index.row(), 0)
                         ->data(Qt::UserRole)
                         .toString();
      QPluginLoader loader(path);
      if (!loader.isLoaded())
        loader.load();
    }
    impl_->loadPlugins(this);
  });
}

PluginSettingPage::~PluginSettingPage() { delete impl_; }
QVector<QWidget *> PluginSettingPage::settingWidgets() const {
  return QVector<QWidget *>();
}
void PluginSettingPage::loadSettings() {
  if (impl_)
    impl_->loadPlugins(this);
}
void PluginSettingPage::saveSettings() {}
QList<SettingItemInfo> PluginSettingPage::searchableItems() const { return {}; }

// AISettingPage Implementation
class AISettingPage::Impl {
public:
  Impl() = default;
  ~Impl() = default;
  QLabel *label_ = nullptr;
  Artifact::ArtifactAICloudSettingsWidget *cloudSettings_ = nullptr;
};

W_OBJECT_IMPL(AISettingPage)

AISettingPage::AISettingPage(QWidget *parent)
    : QWidget(parent), impl_(new Impl()) {
  setAccessibleName(QStringLiteral("AI connection settings"));
  setAccessibleDescription(QStringLiteral("Configure the isolated cloud AI connection settings"));
  auto *layout = new QVBoxLayout(this);
  impl_->label_ = new QLabel(
      QStringLiteral("Cloud AI connection settings are isolated here."), this);
  impl_->label_->setAccessibleName(QStringLiteral("AI settings description"));
  impl_->label_->setWordWrap(true);
  layout->addWidget(impl_->label_);
  impl_->cloudSettings_ = new Artifact::ArtifactAICloudSettingsWidget(this);
  impl_->cloudSettings_->setAccessibleName(QStringLiteral("Cloud AI connection settings"));
  impl_->cloudSettings_->setAccessibleDescription(QStringLiteral("Configure cloud AI connection options"));
  layout->addWidget(impl_->cloudSettings_);
  layout->addStretch();
}

AISettingPage::~AISettingPage() { delete impl_; }

QVector<QWidget *> AISettingPage::settingWidgets() const { return {}; }

void AISettingPage::loadSettings() {
  if (impl_->cloudSettings_) {
    impl_->cloudSettings_->loadSettings();
  }
}
void AISettingPage::saveSettings() {
  if (impl_->cloudSettings_) {
    impl_->cloudSettings_->saveSettings();
  }
}
QList<SettingItemInfo> AISettingPage::searchableItems() const { return {}; }

// ── AudioScrubSettingPage ──────────────────────────
class AudioScrubSettingPage::Impl {
public:
  QCheckBox *enabledCheckBox_;
  QComboBox *latencyCombo_;
  QDoubleSpinBox *volumeScaleSpinBox_;
};

AudioScrubSettingPage::AudioScrubSettingPage(QWidget *parent)
    : QWidget(parent), impl_(new Impl()) {
  setAccessibleName(QStringLiteral("Audio scrubbing settings"));
  setAccessibleDescription(QStringLiteral("Configure audio preview while dragging the timeline"));
  auto *mainLayout = new QVBoxLayout(this);

  auto *group = new QGroupBox(QStringLiteral("Audio Scrubbing"), this);
  auto *layout = new QVBoxLayout(group);

  impl_->enabledCheckBox_ =
      new QCheckBox(QStringLiteral("Enable audio scrubbing during timeline drag"), this);
  impl_->enabledCheckBox_->setAccessibleName(QStringLiteral("Enable audio scrubbing during timeline drag"));
  impl_->enabledCheckBox_->setAccessibleDescription(QStringLiteral("Play a short audio preview while scrubbing the timeline"));
  layout->addWidget(impl_->enabledCheckBox_);
  layout->addWidget(new QLabel(
      QStringLiteral("When enabled, the timeline plays a short preview while you scrub."), this));

  auto *latencyRow = new QHBoxLayout();
  latencyRow->addWidget(new QLabel(QStringLiteral("Latency target:"), this));
  impl_->latencyCombo_ = new QComboBox(this);
  impl_->latencyCombo_->setAccessibleName(QStringLiteral("Audio scrubbing latency target"));
  impl_->latencyCombo_->setAccessibleDescription(QStringLiteral("Choose the response latency for audio scrubbing"));
  impl_->latencyCombo_->addItems({
      QStringLiteral("5 ms"),
      QStringLiteral("10 ms"),
      QStringLiteral("20 ms"),
      QStringLiteral("50 ms"),
  });
  latencyRow->addWidget(impl_->latencyCombo_);
  latencyRow->addStretch();
  layout->addLayout(latencyRow);
  layout->addWidget(new QLabel(
      QStringLiteral("Lower latency reacts faster; higher values are steadier on slow systems."), this));

  auto *volumeRow = new QHBoxLayout();
  volumeRow->addWidget(new QLabel(QStringLiteral("Volume scale:"), this));
  impl_->volumeScaleSpinBox_ = new QDoubleSpinBox(this);
  impl_->volumeScaleSpinBox_->setAccessibleName(QStringLiteral("Audio scrubbing volume scale"));
  impl_->volumeScaleSpinBox_->setAccessibleDescription(QStringLiteral("Set the volume of the audio scrubbing preview"));
  impl_->volumeScaleSpinBox_->setRange(0.0, 1.0);
  impl_->volumeScaleSpinBox_->setSingleStep(0.05);
  impl_->volumeScaleSpinBox_->setDecimals(2);
  impl_->volumeScaleSpinBox_->setValue(0.5);
  volumeRow->addWidget(impl_->volumeScaleSpinBox_);
  volumeRow->addStretch();
  layout->addLayout(volumeRow);
  layout->addWidget(new QLabel(
      QStringLiteral("Volume scales the scrub preview, independent of normal playback."), this));

  mainLayout->addWidget(group);
  mainLayout->addStretch();
}

AudioScrubSettingPage::~AudioScrubSettingPage() { delete impl_; }

void AudioScrubSettingPage::loadSettings() {
  auto &ctrl = Artifact::ArtifactAudioScrubController::instance();
  impl_->enabledCheckBox_->setChecked(ctrl.isEnabled());
  const int latency = ctrl.latencyTargetMs();
  int idx = 0;
  if (latency <= 5) idx = 0;
  else if (latency <= 10) idx = 1;
  else if (latency <= 20) idx = 2;
  else idx = 3;
  impl_->latencyCombo_->setCurrentIndex(idx);
  impl_->volumeScaleSpinBox_->setValue(ctrl.volumeScale());
}

void AudioScrubSettingPage::saveSettings() {
  auto &ctrl = Artifact::ArtifactAudioScrubController::instance();
  ctrl.setEnabled(impl_->enabledCheckBox_->isChecked());
  static constexpr int kLatencyValues[] = {5, 10, 20, 50};
  const int latencyMs = kLatencyValues[qMin(3, impl_->latencyCombo_->currentIndex())];
  ctrl.setLatencyTargetMs(latencyMs);
  ctrl.setVolumeScale(static_cast<float>(impl_->volumeScaleSpinBox_->value()));
  ctrl.saveSettings();
}

QList<SettingItemInfo> AudioScrubSettingPage::searchableItems() const { return {}; }

void ApplicationSettingDialog::showEvent(QShowEvent *event) {
  QDialog::showEvent(event);
  loadSettings();
}

}; // namespace ArtifactCore
