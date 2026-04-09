module;
#include <utility>
#include <QString>
#include <QCoreApplication>
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
#include <QProgressBar>
#include <QTimer>
#include <QMessageBox>
#include <QThread>
#include <QTableWidget>
#include <QHeaderView>
#include <QFileDialog>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFileInfoList>
#include <QPluginLoader>
#include <QDesktopServices>
#include <QStandardPaths>
#include <QUrl>
#include <QShowEvent>
#ifdef Q_OS_WIN
#include <windows.h>
#include <psapi.h>
#undef MessageBox
#endif
#include <wobjectimpl.h>


 module ApplicationSettingDialog;

import Artifact.Service.Application;
import Artifact.Widgets.AppDialogs;
import Artifact.Widgets.AI.ArtifactAICloudSettingsWidget;
import Application.AppSettings;

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
  autoSaveLayout->addWidget(impl_->autoSaveCheckBox_);
  
  auto* intervalLayout = new QHBoxLayout();
  intervalLayout->addWidget(new QLabel("Save every:", this));
  impl_->autoSaveIntervalSpinBox_ = new QSpinBox(this);
  impl_->autoSaveIntervalSpinBox_->setRange(1, 60);
  impl_->autoSaveIntervalSpinBox_->setSuffix(" minutes");
  intervalLayout->addWidget(impl_->autoSaveIntervalSpinBox_);
  intervalLayout->addStretch();
  autoSaveLayout->addLayout(intervalLayout);
  
  mainLayout->addWidget(autoSaveGroup);
  
  // Startup Group
  auto* startupGroup = new QGroupBox("Startup", this);
  auto* startupLayout = new QVBoxLayout(startupGroup);
  
  impl_->showStartupDialogCheckBox_ = new QCheckBox("Load last project on startup", this);
  startupLayout->addWidget(impl_->showStartupDialogCheckBox_);
  
  mainLayout->addWidget(startupGroup);
  
  mainLayout->addStretch();

  loadSettings();
 }

 void GeneralSettingPage::loadSettings()
 {
  auto* settings = ArtifactAppSettings::instance();
  impl_->autoSaveIntervalSpinBox_->setValue(settings->autoSaveIntervalMinutes());
  impl_->showStartupDialogCheckBox_->setChecked(settings->loadLastProjectOnStartup());
  // autoSaveEnabled の項目が AppSettings にまだないので、将来的に追加が必要
 }

void GeneralSettingPage::saveSettings()
{
  auto* settings = ArtifactAppSettings::instance();
  settings->setAutoSaveIntervalMinutes(impl_->autoSaveIntervalSpinBox_->value());
  settings->setLoadLastProjectOnStartup(impl_->showStartupDialogCheckBox_->isChecked());
}

QList<SettingItemInfo> GeneralSettingPage::searchableItems() const
{
  return {};
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
	
// MemoryAndCpuSettingPage Implementation
class MemoryAndCpuSettingPage::Impl {
public:
    Impl() : memoryUsageBar_(nullptr), memoryLabel_(nullptr), cpuUsageBar_(nullptr), cpuLabel_(nullptr), workerThreadsSpinBox_(nullptr), autoTuneButton_(nullptr), clearCacheButton_(nullptr), updateTimer_(nullptr), prevProcessTimeMs_(0), prevTickMs_(0), processorCount_(1) {}
    ~Impl() {}

    QProgressBar* memoryUsageBar_;
    QLabel* memoryLabel_;
    QProgressBar* cpuUsageBar_;
    QLabel* cpuLabel_;
    QSpinBox* workerThreadsSpinBox_;
    QPushButton* autoTuneButton_;
    QPushButton* clearCacheButton_;
    QTimer* updateTimer_;

    // for CPU calculation (Windows)
    unsigned long long prevProcessTimeMs_;
    unsigned long long prevTickMs_;
    int processorCount_;

    struct CacheClearStats {
        int removedFiles = 0;
        int removedDirectories = 0;
        int failedPaths = 0;
    };

    void initializeProcessorCount()
    {
#ifdef Q_OS_WIN
        SYSTEM_INFO sysInfo;
        GetSystemInfo(&sysInfo);
        processorCount_ = (int)sysInfo.dwNumberOfProcessors;
        if (processorCount_ < 1) processorCount_ = 1;
#else
        processorCount_ = QThread::idealThreadCount();
        if (processorCount_ < 1) processorCount_ = 1;
#endif
    }

    unsigned long long fileTimeToMs(const FILETIME& ft)
    {
        unsigned long long high = (unsigned long long)ft.dwHighDateTime;
        unsigned long long low = (unsigned long long)ft.dwLowDateTime;
        unsigned long long val100ns = (high << 32) | low; // 100-ns intervals
        return val100ns / 10000ULL; // to ms
    }

    void updateStats(QWidget* parent)
    {
#ifdef Q_OS_WIN
        // Memory (system)
        MEMORYSTATUSEX memx;
        memx.dwLength = sizeof(memx);
        GlobalMemoryStatusEx(&memx);
        unsigned long long totalPhys = memx.ullTotalPhys;
        unsigned long long availPhys = memx.ullAvailPhys;
        unsigned long long usedPhys = totalPhys - availPhys;
        int memPercent = 0;
        if (totalPhys > 0) memPercent = int((usedPhys * 100ULL) / totalPhys);

        if (memoryUsageBar_) memoryUsageBar_->setValue(memPercent);
        if (memoryLabel_) memoryLabel_->setText(QString("%1 / %2 (%3%)").arg(QString::number(usedPhys / (1024*1024))).arg(QString::number(totalPhys / (1024*1024))).arg(memPercent));

        // CPU (process percentage)
        FILETIME ftCreation, ftExit, ftKernel, ftUser;
        if (GetProcessTimes(GetCurrentProcess(), &ftCreation, &ftExit, &ftKernel, &ftUser)) {
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
                cpuPercent = (double)deltaProc / (double)deltaTime / (double)processorCount_ * 100.0;
                if (cpuPercent < 0.0) cpuPercent = 0.0;
                if (cpuPercent > 100.0) cpuPercent = 100.0 * processorCount_; // clamp high but allow multi-core >100 theoretical
            }

            int cpuInt = int(cpuPercent);
            if (cpuUsageBar_) cpuUsageBar_->setValue(qBound(0, cpuInt, 100));
            if (cpuLabel_) cpuLabel_->setText(QString("%1% (process)").arg(QString::number(cpuPercent, 'f', 1)));

            prevProcessTimeMs_ = procMs;
            prevTickMs_ = curTick;
        }
#else
        Q_UNUSED(parent);
        // Non-Windows platforms: show placeholders
        if (memoryUsageBar_) memoryUsageBar_->setValue(0);
        if (memoryLabel_) memoryLabel_->setText("N/A");
        if (cpuUsageBar_) cpuUsageBar_->setValue(0);
        if (cpuLabel_) cpuLabel_->setText("N/A");
#endif
    }

    void clearPathRecursive(const QString& path, CacheClearStats& stats)
    {
        QFileInfo info(path);
        if (!info.exists()) {
            return;
        }

        if (info.isDir()) {
            QDir dir(path);
            const QFileInfoList entries = dir.entryInfoList(QDir::NoDotAndDotDot | QDir::AllEntries);
            for (const QFileInfo& entry : entries) {
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

    CacheClearStats clearAppCaches()
    {
        CacheClearStats stats;
        const QString appDataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        if (appDataDir.isEmpty()) {
            return stats;
        }

        const QStringList cacheTargets = {
            QDir(appDataDir).filePath(QStringLiteral("ProxyCache")),
            QDir(appDataDir).filePath(QStringLiteral("Recovery")),
            QDir(appDataDir).filePath(QStringLiteral("RecoveredProject.artifact.json"))
        };

        for (const QString& target : cacheTargets) {
            clearPathRecursive(target, stats);
        }

        return stats;
    }
};

MemoryAndCpuSettingPage::MemoryAndCpuSettingPage(QWidget* parent /*= nullptr*/)
 : QWidget(parent), impl_(new Impl())
{
    impl_->initializeProcessorCount();

    auto* mainLayout = new QVBoxLayout(this);

    auto* statsGroup = new QGroupBox("Memory & CPU", this);
    auto* statsLayout = new QVBoxLayout(statsGroup);

    // Memory
    auto* memLayout = new QHBoxLayout();
    memLayout->addWidget(new QLabel("System Memory Usage:", this));
    impl_->memoryUsageBar_ = new QProgressBar(this);
    impl_->memoryUsageBar_->setRange(0, 100);
    impl_->memoryUsageBar_->setValue(0);
    impl_->memoryUsageBar_->setTextVisible(false);
    memLayout->addWidget(impl_->memoryUsageBar_);
    impl_->memoryLabel_ = new QLabel("", this);
    memLayout->addWidget(impl_->memoryLabel_);
    statsLayout->addLayout(memLayout);

    // CPU
    auto* cpuLayout = new QHBoxLayout();
    cpuLayout->addWidget(new QLabel("CPU Usage:", this));
    impl_->cpuUsageBar_ = new QProgressBar(this);
    impl_->cpuUsageBar_->setRange(0, 100);
    impl_->cpuUsageBar_->setValue(0);
    impl_->cpuUsageBar_->setTextVisible(false);
    cpuLayout->addWidget(impl_->cpuUsageBar_);
    impl_->cpuLabel_ = new QLabel("", this);
    cpuLayout->addWidget(impl_->cpuLabel_);
    statsLayout->addLayout(cpuLayout);

    mainLayout->addWidget(statsGroup);

    // Performance tuning
    auto* perfGroup = new QGroupBox("Performance Tuning", this);
    auto* perfLayout = new QVBoxLayout(perfGroup);

    auto* threadLayout = new QHBoxLayout();
    threadLayout->addWidget(new QLabel("Worker Threads:", this));
    impl_->workerThreadsSpinBox_ = new QSpinBox(this);
    impl_->workerThreadsSpinBox_->setRange(1, qMax(1, impl_->processorCount_));
    impl_->workerThreadsSpinBox_->setValue(qMax(1, impl_->processorCount_ - 1));
    threadLayout->addWidget(impl_->workerThreadsSpinBox_);
    impl_->autoTuneButton_ = new QPushButton("Auto-tune", this);
    threadLayout->addWidget(impl_->autoTuneButton_);
    threadLayout->addStretch();
    perfLayout->addLayout(threadLayout);

    impl_->clearCacheButton_ = new QPushButton("Clear Cache", this);
    perfLayout->addWidget(impl_->clearCacheButton_);

    mainLayout->addWidget(perfGroup);

    mainLayout->addStretch();

    // Timer for live updates
    impl_->updateTimer_ = new QTimer(this);
    connect(impl_->updateTimer_, &QTimer::timeout, this, [this]() {
        impl_->updateStats(this);
    });
    impl_->updateTimer_->start(1000);

    // Auto-tune handler
    connect(impl_->autoTuneButton_, &QPushButton::clicked, this, [this]() {
        int recommended = qMax(1, impl_->processorCount_ - 1);
        impl_->workerThreadsSpinBox_->setValue(recommended);
    });

    // Clear cache handler
    connect(impl_->clearCacheButton_, &QPushButton::clicked, this, [this]() {
        if (!Artifact::ArtifactMessageBox::confirmDelete(this,
            QStringLiteral("Clear Cache"),
            QStringLiteral("Remove generated proxy and recovery cache files?"))) {
            return;
        }

        const Impl::CacheClearStats stats = impl_->clearAppCaches();
        const QString summary = QStringLiteral("Removed %1 file(s), %2 folder(s).%3")
            .arg(stats.removedFiles)
            .arg(stats.removedDirectories)
            .arg(stats.failedPaths > 0
                ? QStringLiteral("\nFailed to remove %1 path(s).").arg(stats.failedPaths)
                : QString());
        QMessageBox::information(this, QStringLiteral("Clear Cache"), summary);
    });

    loadSettings();
}

void MemoryAndCpuSettingPage::loadSettings() {
    auto* settings = ArtifactAppSettings::instance();
    int count = settings->renderThreadCount();
    if (count <= 0) {
        impl_->workerThreadsSpinBox_->setValue(qMax(1, impl_->processorCount_ - 1));
    } else {
        impl_->workerThreadsSpinBox_->setValue(count);
    }
}

void MemoryAndCpuSettingPage::saveSettings() {
    auto* settings = ArtifactAppSettings::instance();
    settings->setRenderThreadCount(impl_->workerThreadsSpinBox_->value());
}

QList<SettingItemInfo> MemoryAndCpuSettingPage::searchableItems() const
{
    return {};
}

MemoryAndCpuSettingPage::~MemoryAndCpuSettingPage()
{
    if (impl_) {
        if (impl_->updateTimer_) impl_->updateTimer_->stop();
        delete impl_;
        impl_ = nullptr;
    }
}

// Add loadSettings/saveSettings stubs for other pages if not yet implemented
void ImportSettingPage::loadSettings() {}
void ImportSettingPage::saveSettings() {}
QList<SettingItemInfo> ImportSettingPage::searchableItems() const
{
  return {};
}
void PreviewSettingPage::loadSettings() {}
void PreviewSettingPage::saveSettings() {}
QList<SettingItemInfo> PreviewSettingPage::searchableItems() const
{
  return {};
}
void ShortcutSettingPage::loadSettings() {}
void ShortcutSettingPage::saveSettings() {}
QList<SettingItemInfo> ShortcutSettingPage::searchableItems() const
{
  return {};
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
  PluginSettingPage* pluginPage_;
  
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
  , pluginPage_(nullptr)
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
  settingPages_->addWidget(pluginPage_ = new PluginSettingPage(dialog));
  
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
  QObject::connect(buttonBox_, &QDialogButtonBox::accepted, dialog, &ApplicationSettingDialog::accept);
  QObject::connect(buttonBox_, &QDialogButtonBox::rejected, dialog, &QDialog::reject);
  QObject::connect(buttonBox_->button(QDialogButtonBox::Apply), &QPushButton::clicked, dialog, &ApplicationSettingDialog::saveSettings);
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

 void ApplicationSettingDialog::loadSettings()
 {
     impl_->generalPage_->loadSettings();
     impl_->importPage_->loadSettings();
     impl_->previewPage_->loadSettings();
     impl_->memoryPage_->loadSettings();
 }

 void ApplicationSettingDialog::saveSettings()
 {
     impl_->generalPage_->saveSettings();
     impl_->importPage_->saveSettings();
     impl_->previewPage_->saveSettings();
     impl_->memoryPage_->saveSettings();
     
     ArtifactAppSettings::instance()->sync();
 }

 void ApplicationSettingDialog::accept()
 {
     saveSettings();
     QDialog::accept();
 }

// PluginSettingPage Implementation 

 W_OBJECT_IMPL(PluginSettingPage)

 class PluginSettingPage::Impl {
 public:
  Impl();
  ~Impl();
  QTableWidget* pluginTable_;
  QPushButton* refreshButton_;
  QPushButton* openFolderButton_;
  QString pluginDirectory_;
  void loadPlugins(PluginSettingPage* page);
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
   QFileInfoList entries = dir.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);
   for (const QFileInfo& entry : entries) {
    if (entry.isDir()) {
     QDir subDir(entry.absoluteFilePath());
     QFileInfoList plugins = subDir.entryInfoList(QStringList() << "*.dll" << "*.so" << "*.dylib", QDir::Files);
     for (const QFileInfo& plugin : plugins) paths.append(plugin.absoluteFilePath());
    } else if (entry.suffix() == "dll" || entry.suffix() == "so" || entry.suffix() == "dylib") {
     paths.append(entry.absoluteFilePath());
    }
   }
  }
  return paths;
 }

 void PluginSettingPage::Impl::loadPlugins(PluginSettingPage* page) {
  if (!pluginTable_) return;
  pluginTable_->setRowCount(0);
  QStringList pluginPaths = getPluginPaths();
  for (const QString& path : pluginPaths) {
   QPluginLoader loader(path);
   QJsonObject metaData = loader.metaData();
   int row = pluginTable_->rowCount();
   pluginTable_->insertRow(row);
   QString name = metaData.value("Name").toString();
   if (name.isEmpty()) { QFileInfo info(path); name = info.baseName(); }
   pluginTable_->setItem(row, 0, new QTableWidgetItem(name));
   pluginTable_->setItem(row, 1, new QTableWidgetItem(metaData.value("Version").toString()));
   QString vendor = metaData.value("Vendor").toString();
   if (vendor.isEmpty()) vendor = metaData.value("Author").toString();
   pluginTable_->setItem(row, 2, new QTableWidgetItem(vendor));
   pluginTable_->setItem(row, 3, new QTableWidgetItem(metaData.value("Description").toString()));
   QString status = loader.isLoaded() ? "Loaded" : (loader.load() ? "Loaded" : "Failed");
   pluginTable_->setItem(row, 4, new QTableWidgetItem(status));
   pluginTable_->item(row, 0)->setData(Qt::UserRole, path);
  }
  pluginTable_->resizeColumnsToContents();
 }

 PluginSettingPage::PluginSettingPage(QWidget* parent) : QWidget(parent), impl_(new Impl()) {
  auto* mainLayout = new QVBoxLayout(this);
  auto* infoGroup = new QGroupBox("Plugin Directory", this);
  auto* infoLayout = new QHBoxLayout(infoGroup);
  auto* dirLabel = new QLabel(impl_->pluginDirectory_, this);
  dirLabel->setStyleSheet("color: gray;");
  infoLayout->addWidget(dirLabel);
  impl_->openFolderButton_ = new QPushButton("Open Folder", this);
  infoLayout->addWidget(impl_->openFolderButton_);
  mainLayout->addWidget(infoGroup);
  auto* tableGroup = new QGroupBox("Installed Plugins", this);
  auto* tableLayout = new QVBoxLayout(tableGroup);
  impl_->pluginTable_ = new QTableWidget(this);
  impl_->pluginTable_->setColumnCount(5);
  impl_->pluginTable_->setHorizontalHeaderLabels({"Name", "Version", "Vendor", "Description", "Status"});
  impl_->pluginTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
  impl_->pluginTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
  impl_->pluginTable_->horizontalHeader()->setStretchLastSection(true);
  tableLayout->addWidget(impl_->pluginTable_);
  mainLayout->addWidget(tableGroup);
  auto* buttonLayout = new QHBoxLayout();
  impl_->refreshButton_ = new QPushButton("Refresh", this);
  buttonLayout->addWidget(impl_->refreshButton_);
  buttonLayout->addStretch();
  auto* unloadButton = new QPushButton("Unload Selected", this);
  buttonLayout->addWidget(unloadButton);
  auto* loadButton = new QPushButton("Load Selected", this);
  buttonLayout->addWidget(loadButton);
  mainLayout->addLayout(buttonLayout);
  impl_->loadPlugins(this);
  connect(impl_->refreshButton_, &QPushButton::clicked, this, [this]() { impl_->loadPlugins(this); });
  connect(impl_->openFolderButton_, &QPushButton::clicked, this, [this]() { QDesktopServices::openUrl(QUrl::fromLocalFile(impl_->pluginDirectory_)); });
  connect(unloadButton, &QPushButton::clicked, this, [this]() {
   QModelIndexList selected = impl_->pluginTable_->selectionModel()->selectedRows();
   if (selected.isEmpty()) { QMessageBox::information(this, "Unload Plugin", "Please select a plugin to unload."); return; }
   for (const QModelIndex& index : selected) {
    QString path = impl_->pluginTable_->item(index.row(), 0)->data(Qt::UserRole).toString();
    QPluginLoader loader(path);
    if (loader.isLoaded()) loader.unload();
   }
   impl_->loadPlugins(this);
  });
  connect(loadButton, &QPushButton::clicked, this, [this]() {
   QModelIndexList selected = impl_->pluginTable_->selectionModel()->selectedRows();
   if (selected.isEmpty()) { QMessageBox::information(this, "Load Plugin", "Please select a plugin to load."); return; }
   for (const QModelIndex& index : selected) {
    QString path = impl_->pluginTable_->item(index.row(), 0)->data(Qt::UserRole).toString();
    QPluginLoader loader(path);
    if (!loader.isLoaded()) loader.load();
   }
   impl_->loadPlugins(this);
  });
 }

PluginSettingPage::~PluginSettingPage() { delete impl_; }
QVector<QWidget*> PluginSettingPage::settingWidgets() const { return QVector<QWidget*>(); }
void PluginSettingPage::loadSettings() { if (impl_) impl_->loadPlugins(this); }
void PluginSettingPage::saveSettings() {}
QList<SettingItemInfo> PluginSettingPage::searchableItems() const { return {}; }

// AISettingPage Implementation
class AISettingPage::Impl {
public:
  Impl() = default;
  ~Impl() = default;
  QLabel* label_ = nullptr;
  Artifact::ArtifactAICloudSettingsWidget* cloudSettings_ = nullptr;
};

W_OBJECT_IMPL(AISettingPage)

AISettingPage::AISettingPage(QWidget* parent)
  : QWidget(parent), impl_(new Impl())
{
  auto* layout = new QVBoxLayout(this);
  impl_->label_ = new QLabel(QStringLiteral("Cloud AI connection settings are isolated here."), this);
  impl_->label_->setWordWrap(true);
  layout->addWidget(impl_->label_);
  impl_->cloudSettings_ = new Artifact::ArtifactAICloudSettingsWidget(this);
  layout->addWidget(impl_->cloudSettings_);
  layout->addStretch();
}

AISettingPage::~AISettingPage()
{
  delete impl_;
}

QVector<QWidget*> AISettingPage::settingWidgets() const
{
  return {};
}

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
QList<SettingItemInfo> AISettingPage::searchableItems() const
{
  return {};
}

void ApplicationSettingDialog::showEvent(QShowEvent* event)
{
  QDialog::showEvent(event);
  loadSettings();
}
 
}; 
