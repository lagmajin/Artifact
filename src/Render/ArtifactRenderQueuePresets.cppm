module;
#include <QString>
#include <QVector>
#include <QMap>
#include <QMutex>
#include <QIcon>

module Artifact.Render.Queue.Presets;

import std;

namespace Artifact {

class ArtifactRenderFormatPresetManager::Impl {
public:
    Impl() {
        initializeStandardPresets();
    }
    
    ~Impl() = default;
    
    QVector<ArtifactRenderFormatPreset> standardPresets;
    QMap<QString, ArtifactRenderFormatPreset> customPresets;
    QMutex mutex;
    
    void initializeStandardPresets() {
        // === Video Presets ===
        
        // H.264 MP4 (標準)
        standardPresets.push_back({
            .id = "h264_mp4_standard",
            .name = "H.264 MP4",
            .container = "mp4",
            .codec = "h264",
            .description = "標準的な H.264 MP4 形式（互換性重視）",
            .isImageSequence = false
        });
        
        // H.264 MP4 (高画質)
        standardPresets.push_back({
            .id = "h264_mp4_high",
            .name = "H.264 MP4 (高画質)",
            .container = "mp4",
            .codec = "h264",
            .description = "高ビットレート H.264 MP4 形式",
            .isImageSequence = false
        });
        
        // H.265 MP4
        standardPresets.push_back({
            .id = "h265_mp4",
            .name = "H.265 MP4",
            .container = "mp4",
            .codec = "h265",
            .description = "高効率 H.265/HEVC MP4 形式",
            .isImageSequence = false
        });
        
        // ProRes 422 MOV
        standardPresets.push_back({
            .id = "prores_422_mov",
            .name = "ProRes 422 MOV",
            .container = "mov",
            .codec = "prores",
            .codecProfile = "hq",
            .description = "Apple ProRes 422 MOV 形式（編集向け）",
            .isImageSequence = false
        });
        
        // ProRes 4444 MOV
        standardPresets.push_back({
            .id = "prores_4444_mov",
            .name = "ProRes 4444 MOV",
            .container = "mov",
            .codec = "prores",
            .codecProfile = "4444",
            .description = "Apple ProRes 4444 MOV 形式（アルファチャンネル対応）",
            .isImageSequence = false
        });
        
        // DNxHD MOV
        standardPresets.push_back({
            .id = "dnxhd_mov",
            .name = "DNxHD MOV",
            .container = "mov",
            .codec = "dnxhd",
            .description = "Avid DNxHD MOV 形式",
            .isImageSequence = false
        });
        
        // WMV
        standardPresets.push_back({
            .id = "wmv_standard",
            .name = "WMV",
            .container = "wmv",
            .codec = "wmv2",
            .description = "Windows Media Video 形式",
            .isImageSequence = false
        });
        
        // AVI (未圧縮)
        standardPresets.push_back({
            .id = "avi_uncompressed",
            .name = "AVI (未圧縮)",
            .container = "avi",
            .codec = "rawvideo",
            .description = "未圧縮 AVI 形式（高品質・大容量）",
            .isImageSequence = false
        });
        
        // WebM VP9
        standardPresets.push_back({
            .id = "webm_vp9",
            .name = "WebM VP9",
            .container = "webm",
            .codec = "vp9",
            .description = "WebM VP9 形式（Web 向け）",
            .isImageSequence = false
        });
        
        // === Image Sequence Presets ===
        
        // PNG Sequence
        standardPresets.push_back({
            .id = "png_sequence",
            .name = "PNG Sequence",
            .container = "png",
            .codec = "png",
            .description = "PNG 連番画像（アルファチャンネル対応）",
            .isImageSequence = true
        });
        
        // JPEG Sequence
        standardPresets.push_back({
            .id = "jpeg_sequence",
            .name = "JPEG Sequence",
            .container = "jpg",
            .codec = "mjpeg",
            .description = "JPEG 連番画像（容量節約）",
            .isImageSequence = true
        });
        
        // TIFF Sequence
        standardPresets.push_back({
            .id = "tiff_sequence",
            .name = "TIFF Sequence",
            .container = "tiff",
            .codec = "tiff",
            .description = "TIFF 連番画像（高品質・編集向け）",
            .isImageSequence = true
        });
        
        // EXR Sequence
        standardPresets.push_back({
            .id = "exr_sequence",
            .name = "OpenEXR Sequence",
            .container = "exr",
            .codec = "exr",
            .description = "OpenEXR 連番画像（HDR・VFX 向け）",
            .isImageSequence = true
        });
        
        // BMP Sequence
        standardPresets.push_back({
            .id = "bmp_sequence",
            .name = "BMP Sequence",
            .container = "bmp",
            .codec = "bmp",
            .description = "BMP 連番画像（未圧縮）",
            .isImageSequence = true
        });
    }
};

// Static instance management
static QMutex g_presetManagerMutex;
static ArtifactRenderFormatPresetManager* g_presetManagerInstance = nullptr;

ArtifactRenderFormatPresetManager::ArtifactRenderFormatPresetManager()
    : impl_(new Impl()) {
}

ArtifactRenderFormatPresetManager::~ArtifactRenderFormatPresetManager() {
    delete impl_;
}

ArtifactRenderFormatPresetManager& ArtifactRenderFormatPresetManager::instance() {
    QMutexLocker locker(&g_presetManagerMutex);
    if (!g_presetManagerInstance) {
        g_presetManagerInstance = new ArtifactRenderFormatPresetManager();
    }
    return *g_presetManagerInstance;
}

QVector<ArtifactRenderFormatPreset> ArtifactRenderFormatPresetManager::allPresets() const {
    QMutexLocker locker(&impl_->mutex);
    QVector<ArtifactRenderFormatPreset> result = impl_->standardPresets;
    for (auto it = impl_->customPresets.begin(); it != impl_->customPresets.end(); ++it) {
        result.push_back(it.value());
    }
    return result;
}

QVector<ArtifactRenderFormatPreset> ArtifactRenderFormatPresetManager::presetsByCategory(
    ArtifactRenderFormatCategory category) const 
{
    QMutexLocker locker(&impl_->mutex);
    QVector<ArtifactRenderFormatPreset> result;
    
    const auto filterByCategory = [category](const ArtifactRenderFormatPreset& preset) -> bool {
        switch (category) {
            case ArtifactRenderFormatCategory::Video:
                return !preset.isImageSequence;
            case ArtifactRenderFormatCategory::ImageSequence:
                return preset.isImageSequence;
            case ArtifactRenderFormatCategory::Audio:
                return false; // Audio presets not implemented yet
            case ArtifactRenderFormatCategory::Custom:
                return true;
        }
        return true;
    };
    
    for (const auto& preset : impl_->standardPresets) {
        if (filterByCategory(preset)) {
            result.push_back(preset);
        }
    }
    
    for (auto it = impl_->customPresets.begin(); it != impl_->customPresets.end(); ++it) {
        if (filterByCategory(it.value())) {
            result.push_back(it.value());
        }
    }
    
    return result;
}

const ArtifactRenderFormatPreset* ArtifactRenderFormatPresetManager::findPresetById(const QString& id) const {
    QMutexLocker locker(&impl_->mutex);
    
    // Search standard presets
    for (const auto& preset : impl_->standardPresets) {
        if (preset.id == id) {
            return &preset;
        }
    }
    
    // Search custom presets
    auto it = impl_->customPresets.find(id);
    if (it != impl_->customPresets.end()) {
        return &it.value();
    }
    
    return nullptr;
}

void ArtifactRenderFormatPresetManager::addCustomPreset(const ArtifactRenderFormatPreset& preset) {
    QMutexLocker locker(&impl_->mutex);
    impl_->customPresets.insert(preset.id, preset);
}

void ArtifactRenderFormatPresetManager::removeCustomPreset(const QString& id) {
    QMutexLocker locker(&impl_->mutex);
    impl_->customPresets.remove(id);
}

// Static factory methods
QVector<ArtifactRenderFormatPreset> ArtifactRenderFormatPreset::getStandardPresets() {
    return ArtifactRenderFormatPresetManager::instance().allPresets();
}

QVector<ArtifactRenderFormatPreset> ArtifactRenderFormatPreset::getImageSequencePresets() {
    return ArtifactRenderFormatPresetManager::instance().presetsByCategory(
        ArtifactRenderFormatCategory::ImageSequence);
}

QVector<ArtifactRenderFormatPreset> ArtifactRenderFormatPreset::getVideoPresets() {
    return ArtifactRenderFormatPresetManager::instance().presetsByCategory(
        ArtifactRenderFormatCategory::Video);
}

} // namespace Artifact
