module;
#include <QString>
#include <QListWidget>
#include <QStackedWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QCheckBox>
#include <QSpinBox>
#include <QGroupBox>
#include <QDialogButtonBox>
#include <QComboBox>
#include <QLineEdit>
#include <QSlider>


 module ApplicationSettingDialog;

import Artifact.Service.Application;

namespace ArtifactCore {

 // GeneralSettingPage Implementation
 class GeneralSettingPage::Impl {
 public:
  Impl();
  ~Impl();
  
  QCheckBox* autoSaveCheckBox_;
  QSpinBox* autoSaveIntervalSpinBox_;
  QCheckBox* showStartupDialogCheckBox_;
 };

 GeneralSettingPage::Impl::Impl()
 {
 }

 GeneralSettingPage::Impl::~Impl()
 {
 }

 GeneralSettingPage::GeneralSettingPage(QWidget* parent)
  : QWidget(parent), impl_(new Impl())
 {
  auto* mainLayout = new QVBoxLayout(this);
  
  // Auto-Save Group
  auto* autoSaveGroup = new QGroupBox("Auto-Save", this);
  auto* autoSaveLayout = new QVBoxLayout(autoSaveGroup);
  
  impl_->autoSaveCheckBox_ = new QCheckBox("Enable Auto-Save", this);
  impl_->autoSaveCheckBox_->setChecked(true);
  autoSaveLayout->addWidget(impl_->autoSaveCheckBox_);
  
  auto* intervalLayout = new QHBoxLayout();
  intervalLayout->addWidget(new QLabel("Save every:", this));
  impl_->autoSaveIntervalSpinBox_ = new QSpinBox(this);
  impl_->autoSaveIntervalSpinBox_->setRange(1, 60);
  impl_->autoSaveIntervalSpinBox_->setValue(5);
  impl_->autoSaveIntervalSpinBox_->setSuffix(" minutes");
  intervalLayout->addWidget(impl_->autoSaveIntervalSpinBox_);
  intervalLayout->addStretch();
  autoSaveLayout->addLayout(intervalLayout);
  
  mainLayout->addWidget(autoSaveGroup);
  
  // Startup Group
  auto* startupGroup = new QGroupBox("Startup", this);
  auto* startupLayout = new QVBoxLayout(startupGroup);
  
  impl_->showStartupDialogCheckBox_ = new QCheckBox("Show startup dialog", this);
  impl_->showStartupDialogCheckBox_->setChecked(true);
  startupLayout->addWidget(impl_->showStartupDialogCheckBox_);
  
  mainLayout->addWidget(startupGroup);
  
  mainLayout->addStretch();
 }

 GeneralSettingPage::~GeneralSettingPage()
 {
  delete impl_;
 }

 // ImportSettingPage Implementation
 class ImportSettingPage::Impl {
 public:
  Impl();
  ~Impl();
  
  // Media Import Settings
  QComboBox* defaultFrameRateCombo_;
  QComboBox* colorSpaceCombo_;
  QComboBox* audioSampleRateCombo_;
  
  // Footage Interpretation
  QCheckBox* autoDetectAlphaCheckBox_;
  QCheckBox* interpretFootageCheckBox_;
  QComboBox* fieldOrderCombo_;
  
  // Sequence Settings
  QSpinBox* stillDurationSpinBox_;
  QCheckBox* createCompositionCheckBox_;
 };

 ImportSettingPage::Impl::Impl()
 {
 }

 ImportSettingPage::Impl::~Impl()
 {
 }

 ImportSettingPage::ImportSettingPage(QWidget* parent)
  : QWidget(parent), impl_(new Impl())
 {
  auto* mainLayout = new QVBoxLayout(this);
  
  // Media Import Group
  auto* mediaImportGroup = new QGroupBox("Media Import", this);
  auto* mediaImportLayout = new QVBoxLayout(mediaImportGroup);
  
  // Frame Rate
  auto* frameRateLayout = new QHBoxLayout();
  frameRateLayout->addWidget(new QLabel("Default Frame Rate:", this));
  impl_->defaultFrameRateCombo_ = new QComboBox(this);
  impl_->defaultFrameRateCombo_->addItems({"23.976 fps", "24 fps", "25 fps", "29.97 fps", "30 fps", "50 fps", "59.94 fps", "60 fps"});
  impl_->defaultFrameRateCombo_->setCurrentText("30 fps");
  frameRateLayout->addWidget(impl_->defaultFrameRateCombo_);
  frameRateLayout->addStretch();
  mediaImportLayout->addLayout(frameRateLayout);
  
  // Color Space
  auto* colorSpaceLayout = new QHBoxLayout();
  colorSpaceLayout->addWidget(new QLabel("Color Space:", this));
  impl_->colorSpaceCombo_ = new QComboBox(this);
  impl_->colorSpaceCombo_->addItems({"sRGB", "Linear", "Rec.709", "Rec.2020", "DCI-P3", "Adobe RGB"});
  impl_->colorSpaceCombo_->setCurrentText("sRGB");
  colorSpaceLayout->addWidget(impl_->colorSpaceCombo_);
  colorSpaceLayout->addStretch();
  mediaImportLayout->addLayout(colorSpaceLayout);
  
  // Audio Sample Rate
  auto* audioSampleLayout = new QHBoxLayout();
  audioSampleLayout->addWidget(new QLabel("Audio Sample Rate:", this));
  impl_->audioSampleRateCombo_ = new QComboBox(this);
  impl_->audioSampleRateCombo_->addItems({"44100 Hz", "48000 Hz", "96000 Hz", "192000 Hz"});
  impl_->audioSampleRateCombo_->setCurrentText("48000 Hz");
  audioSampleLayout->addWidget(impl_->audioSampleRateCombo_);
  audioSampleLayout->addStretch();
  mediaImportLayout->addLayout(audioSampleLayout);
  
  mainLayout->addWidget(mediaImportGroup);
  
  // Footage Interpretation Group
  auto* footageGroup = new QGroupBox("Footage Interpretation", this);
  auto* footageLayout = new QVBoxLayout(footageGroup);
  
  impl_->autoDetectAlphaCheckBox_ = new QCheckBox("Auto-detect alpha channel", this);
  impl_->autoDetectAlphaCheckBox_->setChecked(true);
  footageLayout->addWidget(impl_->autoDetectAlphaCheckBox_);
  
  impl_->interpretFootageCheckBox_ = new QCheckBox("Interpret footage on import", this);
  impl_->interpretFootageCheckBox_->setChecked(true);
  footageLayout->addWidget(impl_->interpretFootageCheckBox_);
  
  // Field Order
  auto* fieldOrderLayout = new QHBoxLayout();
  fieldOrderLayout->addWidget(new QLabel("Field Order:", this));
  impl_->fieldOrderCombo_ = new QComboBox(this);
  impl_->fieldOrderCombo_->addItems({"Progressive", "Upper Field First", "Lower Field First"});
  impl_->fieldOrderCombo_->setCurrentText("Progressive");
  fieldOrderLayout->addWidget(impl_->fieldOrderCombo_);
  fieldOrderLayout->addStretch();
  footageLayout->addLayout(fieldOrderLayout);
  
  mainLayout->addWidget(footageGroup);
  
  // Sequence Settings Group
  auto* sequenceGroup = new QGroupBox("Sequence Settings", this);
  auto* sequenceLayout = new QVBoxLayout(sequenceGroup);
  
  // Still Duration
  auto* durationLayout = new QHBoxLayout();
  durationLayout->addWidget(new QLabel("Still Image Duration:", this));
  impl_->stillDurationSpinBox_ = new QSpinBox(this);
  impl_->stillDurationSpinBox_->setRange(1, 3600);
  impl_->stillDurationSpinBox_->setValue(5);
  impl_->stillDurationSpinBox_->setSuffix(" seconds");
  durationLayout->addWidget(impl_->stillDurationSpinBox_);
  durationLayout->addStretch();
  sequenceLayout->addLayout(durationLayout);
  
  impl_->createCompositionCheckBox_ = new QCheckBox("Create composition when importing sequences", this);
  impl_->createCompositionCheckBox_->setChecked(true);
  sequenceLayout->addWidget(impl_->createCompositionCheckBox_);
  
  mainLayout->addWidget(sequenceGroup);
  
  mainLayout->addStretch();
 }

 ImportSettingPage::~ImportSettingPage()
 {
  delete impl_;
 }

 // PreviewSettingPage Implementation
 class PreviewSettingPage::Impl {
 public:
  Impl();
  ~Impl();
  
  // Preview Quality
  QComboBox* previewQualityCombo_;
  QSlider* previewResolutionSlider_;
  QLabel* resolutionLabel_;
  
  // Cache Settings
  QCheckBox* enableCacheCheckBox_;
  QSpinBox* cacheSizeSpinBox_;
  QCheckBox* enableDiskCacheCheckBox_;
  
  // Thumbnail Settings
  QCheckBox* generateThumbnailsCheckBox_;
  QComboBox* thumbnailQualityCombo_;
  
  // GPU Acceleration
  QCheckBox* enableGPUCheckBox_;
  QComboBox* gpuDeviceCombo_;
 };

 PreviewSettingPage::Impl::Impl()
 {
 }

 PreviewSettingPage::Impl::~Impl()
 {
 }

 PreviewSettingPage::PreviewSettingPage(QWidget* parent)
  : QWidget(parent), impl_(new Impl())
 {
  auto* mainLayout = new QVBoxLayout(this);
  
  // Preview Quality Group
  auto* qualityGroup = new QGroupBox("Preview Quality", this);
  auto* qualityLayout = new QVBoxLayout(qualityGroup);
  
  // Quality Preset
  auto* presetLayout = new QHBoxLayout();
  presetLayout->addWidget(new QLabel("Quality Preset:", this));
  impl_->previewQualityCombo_ = new QComboBox(this);
  impl_->previewQualityCombo_->addItems({"Draft", "Fast", "Adaptive", "Full Quality"});
  impl_->previewQualityCombo_->setCurrentText("Adaptive");
  presetLayout->addWidget(impl_->previewQualityCombo_);
  presetLayout->addStretch();
  qualityLayout->addLayout(presetLayout);
  
  // Preview Resolution
  auto* resolutionLayout = new QVBoxLayout();
  auto* resLabelLayout = new QHBoxLayout();
  resLabelLayout->addWidget(new QLabel("Preview Resolution:", this));
  impl_->resolutionLabel_ = new QLabel("50%", this);
  resLabelLayout->addWidget(impl_->resolutionLabel_);
  resLabelLayout->addStretch();
  resolutionLayout->addLayout(resLabelLayout);
  
  impl_->previewResolutionSlider_ = new QSlider(Qt::Horizontal, this);
  impl_->previewResolutionSlider_->setRange(25, 100);
  impl_->previewResolutionSlider_->setValue(50);
  impl_->previewResolutionSlider_->setTickPosition(QSlider::TicksBelow);
  impl_->previewResolutionSlider_->setTickInterval(25);
  resolutionLayout->addWidget(impl_->previewResolutionSlider_);
  qualityLayout->addLayout(resolutionLayout);
  
  QObject::connect(impl_->previewResolutionSlider_, &QSlider::valueChanged, [this](int value) {
   impl_->resolutionLabel_->setText(QString::number(value) + "%");
  });
  
  mainLayout->addWidget(qualityGroup);
  
  // Cache Settings Group
  auto* cacheGroup = new QGroupBox("Cache Settings", this);
  auto* cacheLayout = new QVBoxLayout(cacheGroup);
  
  impl_->enableCacheCheckBox_ = new QCheckBox("Enable RAM cache", this);
  impl_->enableCacheCheckBox_->setChecked(true);
  cacheLayout->addWidget(impl_->enableCacheCheckBox_);
  
  auto* cacheSizeLayout = new QHBoxLayout();
  cacheSizeLayout->addWidget(new QLabel("Cache Size:", this));
  impl_->cacheSizeSpinBox_ = new QSpinBox(this);
  impl_->cacheSizeSpinBox_->setRange(512, 32768);
  impl_->cacheSizeSpinBox_->setValue(4096);
  impl_->cacheSizeSpinBox_->setSuffix(" MB");
  impl_->cacheSizeSpinBox_->setSingleStep(512);
  cacheSizeLayout->addWidget(impl_->cacheSizeSpinBox_);
  cacheSizeLayout->addStretch();
  cacheLayout->addLayout(cacheSizeLayout);
  
  impl_->enableDiskCacheCheckBox_ = new QCheckBox("Enable disk cache", this);
  impl_->enableDiskCacheCheckBox_->setChecked(false);
  cacheLayout->addWidget(impl_->enableDiskCacheCheckBox_);
  
  mainLayout->addWidget(cacheGroup);
  
  // Thumbnail Settings Group (using FFmpegThumbnailExtractor)
  auto* thumbnailGroup = new QGroupBox("Thumbnail Generation", this);
  auto* thumbnailLayout = new QVBoxLayout(thumbnailGroup);
  
  impl_->generateThumbnailsCheckBox_ = new QCheckBox("Generate thumbnails for media files", this);
  impl_->generateThumbnailsCheckBox_->setChecked(true);
  thumbnailLayout->addWidget(impl_->generateThumbnailsCheckBox_);
  
  auto* thumbQualityLayout = new QHBoxLayout();
  thumbQualityLayout->addWidget(new QLabel("Thumbnail Quality:", this));
  impl_->thumbnailQualityCombo_ = new QComboBox(this);
  impl_->thumbnailQualityCombo_->addItems({"Low", "Medium", "High"});
  impl_->thumbnailQualityCombo_->setCurrentText("Medium");
  thumbQualityLayout->addWidget(impl_->thumbnailQualityCombo_);
  thumbQualityLayout->addStretch();
  thumbnailLayout->addLayout(thumbQualityLayout);
  
  mainLayout->addWidget(thumbnailGroup);
  
  // GPU Acceleration Group
  auto* gpuGroup = new QGroupBox("GPU Acceleration", this);
  auto* gpuLayout = new QVBoxLayout(gpuGroup);
  
  impl_->enableGPUCheckBox_ = new QCheckBox("Enable GPU acceleration", this);
  impl_->enableGPUCheckBox_->setChecked(true);
  gpuLayout->addWidget(impl_->enableGPUCheckBox_);
  
  auto* gpuDeviceLayout = new QHBoxLayout();
  gpuDeviceLayout->addWidget(new QLabel("GPU Device:", this));
  impl_->gpuDeviceCombo_ = new QComboBox(this);
  impl_->gpuDeviceCombo_->addItems({"Auto (Best Available)", "NVIDIA GPU", "AMD GPU", "Intel GPU"});
  impl_->gpuDeviceCombo_->setCurrentText("Auto (Best Available)");
  gpuDeviceLayout->addWidget(impl_->gpuDeviceCombo_);
  gpuDeviceLayout->addStretch();
  gpuLayout->addLayout(gpuDeviceLayout);
  
  mainLayout->addWidget(gpuGroup);
  
  mainLayout->addStretch();
 }

 PreviewSettingPage::~PreviewSettingPage()
 {
  delete impl_;
 }


 LabelColorSettingWidget::LabelColorSettingWidget(const QString& labelname, const QColor& color, QWidget* parent /*= NULL*/)
 {

 }



 LabelColorSettingWidget::~LabelColorSettingWidget()
 {

 }
	
 MemoryAndCpuSettingPage::MemoryAndCpuSettingPage(QWidget* parent /*= nullptr*/)
 {

 }

 MemoryAndCpuSettingPage::~MemoryAndCpuSettingPage()
 {

 }
 class ApplicationSettingDialog::Impl
 {
 private:

 public:
  Impl();
  ~Impl();
  QListWidget* categoryList_;
  QStackedWidget* settingPages_;
  QDialogButtonBox* buttonBox_;
  
  GeneralSettingPage* generalPage_;
  ImportSettingPage* importPage_;
  PreviewSettingPage* previewPage_;
  MemoryAndCpuSettingPage* memoryPage_;
  
  void setupUI(ApplicationSettingDialog* dialog);
  void onCategoryChanged(int index);
 };

 ApplicationSettingDialog::Impl::Impl()
  : categoryList_(nullptr)
  , settingPages_(nullptr)
  , buttonBox_(nullptr)
  , generalPage_(nullptr)
  , importPage_(nullptr)
  , previewPage_(nullptr)
  , memoryPage_(nullptr)
 {

 }

 ApplicationSettingDialog::Impl::~Impl()
 {

 }

 void ApplicationSettingDialog::Impl::setupUI(ApplicationSettingDialog* dialog)
 {
  // Main layout
  auto* mainLayout = new QVBoxLayout(dialog);
  
  // Content area (category list + settings pages)
  auto* contentLayout = new QHBoxLayout();
  
  // Category list (left side)
  categoryList_ = new QListWidget(dialog);
  categoryList_->setMaximumWidth(150);
  categoryList_->addItem("General");
  categoryList_->addItem("Import");
  categoryList_->addItem("Preview");
  categoryList_->addItem("Memory & Performance");
  categoryList_->addItem("Shortcuts");
  categoryList_->addItem("Plugins");
  categoryList_->setCurrentRow(0);
  contentLayout->addWidget(categoryList_);
  
  // Settings pages (right side)
  settingPages_ = new QStackedWidget(dialog);
  
  // Add pages
  generalPage_ = new GeneralSettingPage(dialog);
  importPage_ = new ImportSettingPage(dialog);
  previewPage_ = new PreviewSettingPage(dialog);
  memoryPage_ = new MemoryAndCpuSettingPage(dialog);
  
  settingPages_->addWidget(generalPage_);
  settingPages_->addWidget(importPage_);
  settingPages_->addWidget(previewPage_);
  settingPages_->addWidget(memoryPage_);
  settingPages_->addWidget(new QWidget(dialog)); // Shortcuts placeholder
  settingPages_->addWidget(new QWidget(dialog)); // Plugins placeholder
  
  contentLayout->addWidget(settingPages_, 1);
  
  mainLayout->addLayout(contentLayout);
  
  // Button box
  buttonBox_ = new QDialogButtonBox(
   QDialogButtonBox::Ok | QDialogButtonBox::Cancel | QDialogButtonBox::Apply,
   dialog);
  mainLayout->addWidget(buttonBox_);
  
  // Connect signals
  QObject::connect(categoryList_, &QListWidget::currentRowChanged,
   [this](int index) { onCategoryChanged(index); });
  QObject::connect(buttonBox_, &QDialogButtonBox::accepted, dialog, &QDialog::accept);
  QObject::connect(buttonBox_, &QDialogButtonBox::rejected, dialog, &QDialog::reject);
 }

 void ApplicationSettingDialog::Impl::onCategoryChanged(int index)
 {
  settingPages_->setCurrentIndex(index);
 }

 ApplicationSettingDialog::ApplicationSettingDialog(QWidget* parent /*= nullptr*/):QDialog(parent),impl_(new Impl)
 {
  setWindowTitle("Application Settings");
  setMinimumSize(700, 500);
  
  impl_->setupUI(this);
 }

 ApplicationSettingDialog::~ApplicationSettingDialog()
 {
  delete impl_;
 }



};