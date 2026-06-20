module;
#include <utility>
#include <QString>
#include <QVector>
#include <QMap>
#include <QMutex>

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
            .name = "標準配布(H.264 MP4)",
            .container = "mp4",
            .codec = "h264",
            .description = "標準的な H.264 MP4 形式（互換性重視）",
            .isAnimatedImage = false,
            .isImageSequence = false,
            .usageCategory = ArtifactRenderUsageCategory::Distribution
        });
        
        // H.264 MP4 (高画質)
        standardPresets.push_back({
            .id = "h264_mp4_high",
            .name = "高画質配布(H.264 MP4)",
            .container = "mp4",
            .codec = "h264",
            .description = "高ビットレート H.264 MP4 形式",
            .isAnimatedImage = false,
            .isImageSequence = false,
            .usageCategory = ArtifactRenderUsageCategory::Distribution
        });
        
        // H.265 MP4
        standardPresets.push_back({
            .id = "h265_mp4",
            .name = "高効率配布(H.265 MP4)",
            .container = "mp4",
            .codec = "h265",
            .description = "高効率 H.265/HEVC MP4 形式",
            .isAnimatedImage = false,
            .isImageSequence = false,
            .usageCategory = ArtifactRenderUsageCategory::Distribution
        });
        
        // ProRes 422 MOV
        standardPresets.push_back({
            .id = "prores_422_mov",
            .name = "編集ソフト用(ProRes 422 MOV)",
            .container = "mov",
            .codec = "prores",
            .codecProfile = "hq",
            .description = "Apple ProRes 422 MOV 形式（編集向け）",
            .isAnimatedImage = false,
            .isImageSequence = false,
            .usageCategory = ArtifactRenderUsageCategory::Editing
        });
        
        // ProRes 4444 MOV
        standardPresets.push_back({
            .id = "prores_4444_mov",
            .name = "編集ソフト用(ProRes 4444 MOV)",
            .container = "mov",
            .codec = "prores",
            .codecProfile = "4444",
            .description = "Apple ProRes 4444 MOV 形式（アルファチャンネル対応）",
            .isAnimatedImage = false,
            .isImageSequence = false,
            .usageCategory = ArtifactRenderUsageCategory::Editing
        });
        
        // DNxHD MOV
        standardPresets.push_back({
            .id = "dnxhd_mov",
            .name = "編集ソフト用(DNxHD MOV)",
            .container = "mov",
            .codec = "dnxhd",
            .description = "Avid DNxHD MOV 形式",
            .isAnimatedImage = false,
            .isImageSequence = false,
            .usageCategory = ArtifactRenderUsageCategory::Editing
        });
        
        // WMV
        standardPresets.push_back({
            .id = "wmv_standard",
            .name = "配布向け(WMV)",
            .container = "wmv",
            .codec = "wmv2",
            .description = "Windows Media Video 形式",
            .isAnimatedImage = false,
            .isImageSequence = false,
            .usageCategory = ArtifactRenderUsageCategory::Distribution
        });
        
        // AVI (未圧縮)
        standardPresets.push_back({
            .id = "avi_uncompressed",
            .name = "配布向け(AVI 未圧縮)",
            .container = "avi",
            .codec = "rawvideo",
            .description = "未圧縮 AVI 形式（高品質・大容量）",
            .isAnimatedImage = false,
            .isImageSequence = false,
            .usageCategory = ArtifactRenderUsageCategory::Distribution
        });
        
        // WebM VP9
        standardPresets.push_back({
            .id = "webm_vp9",
            .name = "背景透過動画(WebM/VP9)",
            .container = "webm",
            .codec = "vp9",
            .description = "WebM VP9 形式（Web 向け）",
            .isAnimatedImage = false,
            .isImageSequence = false,
            .usageCategory = ArtifactRenderUsageCategory::Web
        });

        // === Animated Image Presets ===

        standardPresets.push_back({
            .id = "gif_animation",
            .name = "配布向け(GIF Animation)",
            .container = "gif",
            .codec = "gif",
            .description = "Web 向け GIF アニメーション（互換性重視）",
            .isAnimatedImage = true,
            .isImageSequence = false,
            .usageCategory = ArtifactRenderUsageCategory::Web
        });

        standardPresets.push_back({
            .id = "apng_animation",
            .name = "Web向け(APNG Animation)",
            .container = "apng",
            .codec = "apng",
            .description = "APNG アニメーション（alpha 対応・高画質）",
            .isAnimatedImage = true,
            .isImageSequence = false,
            .usageCategory = ArtifactRenderUsageCategory::Web
        });

        standardPresets.push_back({
            .id = "webp_animation",
            .name = "Web向け(Animated WebP)",
            .container = "webp",
            .codec = "webp",
            .description = "Animated WebP（Web 向け・alpha 対応）",
            .isAnimatedImage = true,
            .isImageSequence = false,
            .usageCategory = ArtifactRenderUsageCategory::Web
        });
        
        // === Image Sequence Presets ===
        
        // PNG Sequence
        standardPresets.push_back({
            .id = "png_sequence",
            .name = "静止画連番(PNG + 透明)",
            .container = "png",
            .codec = "png",
            .description = "PNG 連番画像（アルファチャンネル対応）",
            .isAnimatedImage = false,
            .isImageSequence = true,
            .usageCategory = ArtifactRenderUsageCategory::Sequence
        });
        
        // JPEG Sequence
        standardPresets.push_back({
            .id = "jpeg_sequence",
            .name = "静止画連番(JPEG)",
            .container = "jpg",
            .codec = "mjpeg",
            .description = "JPEG 連番画像（容量節約）",
            .isAnimatedImage = false,
            .isImageSequence = true,
            .usageCategory = ArtifactRenderUsageCategory::Sequence
        });
        
        // TIFF Sequence
        standardPresets.push_back({
            .id = "tiff_sequence",
            .name = "静止画連番(TIFF)",
            .container = "tiff",
            .codec = "tiff",
            .description = "TIFF 連番画像（高品質・編集向け）",
            .isAnimatedImage = false,
            .isImageSequence = true,
            .usageCategory = ArtifactRenderUsageCategory::Sequence
        });
        
        // EXR Sequence
        standardPresets.push_back({
            .id = "exr_sequence",
            .name = "静止画連番(OpenEXR)",
            .container = "exr",
            .codec = "exr",
            .description = "OpenEXR 連番画像（HDR・VFX 向け）",
            .isAnimatedImage = false,
            .isImageSequence = true,
            .usageCategory = ArtifactRenderUsageCategory::Sequence
        });
        
        // BMP Sequence
        standardPresets.push_back({
            .id = "bmp_sequence",
            .name = "静止画連番(BMP)",
            .container = "bmp",
            .codec = "bmp",
            .description = "BMP 連番画像（未圧縮）",
            .isAnimatedImage = false,
            .isImageSequence = true,
            .usageCategory = ArtifactRenderUsageCategory::Sequence
        });

        // === Vector / Code Export Presets ===
        standardPresets.push_back({
            .id = "animated_svg",
            .name = "Web向け(Animated SVG)",
            .container = "svg",
            .codec = "svg",
            .description = "SVG フレーム連番（ベクターアニメーション用）",
            .isAnimatedImage = false,
            .isImageSequence = true,
            .usageCategory = ArtifactRenderUsageCategory::Web
        });

        standardPresets.push_back({
            .id = "css_animation",
            .name = "Web向け(CSS Animation)",
            .container = "css",
            .codec = "css",
            .description = "CSS @keyframes アニメーション",
            .isAnimatedImage = false,
            .isImageSequence = false,
            .usageCategory = ArtifactRenderUsageCategory::Web
        });

        standardPresets.push_back({
            .id = "web_html_player",
            .name = "Web向け(Web Animation HTML)",
            .container = "html",
            .codec = "css",
            .description = "SVG + CSS + HTML の self-contained プレイヤー書き出し",
            .isAnimatedImage = false,
            .isImageSequence = false,
            .usageCategory = ArtifactRenderUsageCategory::Web
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
                return !preset.isImageSequence && !preset.isAnimatedImage;
            case ArtifactRenderFormatCategory::AnimatedImage:
                return preset.isAnimatedImage;
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

QVector<ArtifactRenderFormatPreset> ArtifactRenderFormatPresetManager::presetsByUsage(
    ArtifactRenderUsageCategory usage) const
{
    QMutexLocker locker(&impl_->mutex);
    QVector<ArtifactRenderFormatPreset> result;
    for (const auto& preset : impl_->standardPresets) {
        if (preset.usageCategory == usage) {
            result.push_back(preset);
        }
    }
    for (auto it = impl_->customPresets.begin(); it != impl_->customPresets.end(); ++it) {
        if (it.value().usageCategory == usage) {
            result.push_back(it.value());
        }
    }
    return result;
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
