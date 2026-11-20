module;

#include <QJsonObject>
#include <QList>
#include <QPair>
#include <QString>
#include <QStringList>

export module Artifact.Render.Queue.Job;

import Utils.Id;

export namespace Artifact {

class ArtifactRenderJob {
public:
    enum class FrameRangeMode {
        Composition,
        WorkArea,
        Custom,
        SelectedFrames,
        SingleFrame
    };

    enum class RegionMode {
        Full,
        RegionOfInterest,
        CustomCrop
    };

    enum class LayerFilterMode {
        All,
        Selected,
        Solo,
        Visible,
        Custom
    };

    enum class ResolutionPreset {
        Custom,
        Composition,
        Half,
        Third,
        Quarter
    };

    // Alpha contract shared by the render queue, encoder and preflight.
    // Auto keeps the legacy format/codec-derived behaviour so saved jobs and
    // existing presets are unchanged. The explicit states let a job request a
    // specific alpha treatment instead of relying on the container.
    enum class AlphaState {
        Auto,
        Opaque,
        Straight,
        Premultiplied,
        AlphaOnly
    };

    struct RenderPassConfig {
        QString name;
        LayerFilterMode layerFilter = LayerFilterMode::All;
        QList<ArtifactCore::LayerID> layerIds;
        bool enabled = true;

        // Per-pass AOV / Deep export overrides. Defaults keep the inherited
        // job-level settings so existing jobs and saved queues are unchanged.
        bool overrideChannels = false;
        QStringList channels;
        bool overrideDeepExport = false;
        bool deepExport = false;
    };

    enum class Status {
        Pending,
        Rendering,
        Completed,
        Failed,
        Canceled
    };

    ArtifactCore::CompositionID compositionId;
    QString compositionName;
    QString jobName;
    int priority = 0;
    int jobTimeoutMs = 0;
    int frameTimeoutMs = 0;
    QStringList dependsOn;
    QString workerPool;
    QJsonObject workerRequirements;
    QString outputPath;
    QString outputFormat;
    QString codec;
    QString codecProfile;
    QString encoderBackend;
    QString renderBackend;
    bool integratedRenderEnabled;
    QString audioSourcePath;
    QString audioCodec;
    int audioBitrateKbps;
    QString audioChannelMode;
    int audioSampleRate;
    int resolutionWidth;
    int resolutionHeight;
    double frameRate;
    // Timecode written into the container's tmcd track. -1 means "do not embed".
    // Composition already owns a TimeCode (ArtifactAbstractComposition::startTimeCode);
    // this field carries that value into the encoder.
    long long startTimeCodeFrame = -1;
    // Drop-frame (29.97 / 59.94) timecode numbering. Valid only for 30 / 60 fps.
    bool dropFrame = false;
    // Bit depth written to image-sequence outputs. 8 = 8-bit integer,
    // 16 = 16-bit half (EXR/TIFF), 32 = 32-bit float.
    int bitDepth = 8;
    // EXR compression: none / zip / zips / piz / pxr24. Ignored by other formats.
    // "zip" is the default because it is the only one every EXR reader supports.
    QString exrCompression = QStringLiteral("zip");
    // Append _vNNN to the output path when a file already exists there. The
    // RenderMatrixDialog UI offers this, so the job has to honour it.
    bool autoVersionOutput = true;
    int bitrate;
    int startFrame;
    int endFrame;
    FrameRangeMode frameRangeMode = FrameRangeMode::Composition;
    QList<QPair<int, int>> selectedFrameRanges;
    RegionMode regionMode = RegionMode::Full;
    int cropX = 0;
    int cropY = 0;
    int cropW = 0;
    int cropH = 0;
    LayerFilterMode layerFilterMode = LayerFilterMode::All;
    QList<ArtifactCore::LayerID> layerWhitelist;
    QList<ArtifactCore::LayerID> layerBlacklist;
    bool excludeGuideLayers = false;
    bool excludeAdjustmentLayers = false;
    bool splitPasses = false;
    QList<RenderPassConfig> renderPasses;
    ResolutionPreset resolutionPreset = ResolutionPreset::Composition;
    Status status;
    int progress;
    QString errorMessage;
    float overlayOffsetX;
    float overlayOffsetY;
    float overlayScale;
    float overlayRotationDeg;
    bool multiChannelExportEnabled = false;
    QStringList multiChannelExportChannels;
    // Writes a one-sample-per-pixel Deep EXR from Beauty and Depth AOVs.
    // This is intentionally separate from regular multi-channel EXR output.
    bool deepExportEnabled = false;
    int framePadding = 4;
    // See AlphaState above. Auto means "derive from format/codec" and keeps
    // the pre-contract behaviour of existing jobs.
    AlphaState alphaState = AlphaState::Auto;

    ArtifactRenderJob()
        : resolutionWidth(1920)
        , resolutionHeight(1080)
        , frameRate(30.0)
        , bitrate(8000)
        , startFrame(0)
        , endFrame(100)
        , codecProfile()
        , encoderBackend(QStringLiteral("auto"))
        , renderBackend(QStringLiteral("auto"))
        , integratedRenderEnabled(false)
        , audioSourcePath()
        , audioCodec(QStringLiteral("aac"))
        , audioBitrateKbps(128)
        , audioChannelMode(QStringLiteral("stereo"))
        , audioSampleRate(48000)
        , status(Status::Pending)
        , progress(0)
        , overlayOffsetX(0.0f)
        , overlayOffsetY(0.0f)
        , overlayScale(1.0f)
        , overlayRotationDeg(0.0f)
        , multiChannelExportEnabled(false)
        , deepExportEnabled(false)
        , framePadding(4)
    {
    }
};

}
