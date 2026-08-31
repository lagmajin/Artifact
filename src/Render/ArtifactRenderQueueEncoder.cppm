module;

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QImage>
#include <QProcess>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QStringList>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <utility>

module Artifact.Render.Queue.Encoder;

import Artifact.Render.Queue.Job;
import Render.Queue.Manager;
import Encoder.FFmpegEncoder;
import Core.Diagnostics.Recorder;
import Core.Diagnostics.Trace;

namespace Artifact {

    enum class VideoEncodeBackendKind {
        Auto,
        Pipe,
        PipeHardware,
        PipeVulkan,
        Native,
        GPU
    };

    static VideoEncodeBackendKind parseVideoEncodeBackend(const QString& backend)
    {
        const QString value = backend.trimmed().toLower();
        if (value == QLatin1String("pipe-hw") || value == QLatin1String("pipe-hw (nvenc)")
            || value == QLatin1String("ffmpeg-hw")
            || value == QLatin1String("hardware") || value == QLatin1String("hw")) {
            return VideoEncodeBackendKind::PipeHardware;
        }
        if (value == QLatin1String("pipe-vulkan") || value == QLatin1String("ffmpeg-vulkan")
            || value == QLatin1String("vulkan-hw") || value == QLatin1String("vulkan")) {
            return VideoEncodeBackendKind::PipeVulkan;
        }
        if (value == QLatin1String("pipe") || value == QLatin1String("ffmpeg.exe") || value == QLatin1String("ffmpeg")) {
            return VideoEncodeBackendKind::Pipe;
        }
        if (value == QLatin1String("native") || value == QLatin1String("api") || value == QLatin1String("ffmpegapi")) {
            return VideoEncodeBackendKind::Native;
        }
        if (value == QLatin1String("gpu") || value == QLatin1String("hardware")) {
            return VideoEncodeBackendKind::GPU;
        }
        return VideoEncodeBackendKind::Auto;
    }

    QString normalizeVideoEncodeBackendName(const QString& backend)
    {
        switch (parseVideoEncodeBackend(backend)) {
        case VideoEncodeBackendKind::Pipe:
            return QStringLiteral("pipe");
        case VideoEncodeBackendKind::PipeHardware:
            return QStringLiteral("pipe-hw");
        case VideoEncodeBackendKind::PipeVulkan:
            return QStringLiteral("pipe-vulkan");
        case VideoEncodeBackendKind::Native:
            return QStringLiteral("native");
        case VideoEncodeBackendKind::GPU:
            return QStringLiteral("gpu");
        case VideoEncodeBackendKind::Auto:
        default:
            return QStringLiteral("auto");
        }
    }

    static QString hardwareEncoderNameForCodec(const QString& codec)
    {
        const QString c = codec.trimmed().toLower();
        const QStringList candidates = [&]() {
            if (c == QStringLiteral("h264")) {
                return QStringList{QStringLiteral("h264_nvenc"), QStringLiteral("h264_qsv"), QStringLiteral("h264_amf"), QStringLiteral("h264_vaapi")};
            }
            if (c == QStringLiteral("h265")) {
                return QStringList{QStringLiteral("hevc_nvenc"), QStringLiteral("hevc_qsv"), QStringLiteral("hevc_amf"), QStringLiteral("hevc_vaapi")};
            }
            if (c == QStringLiteral("vp9")) {
                return QStringList{QStringLiteral("vp9_qsv"), QStringLiteral("vp9_vaapi")};
            }
            return QStringList{};
        }();

        for (const auto& name : candidates) {
            if (ArtifactCore::FFmpegEncoder::isEncoderAvailableByName(name)) {
                return name;
            }
        }
        return QString();
    }

    QString normalizeRenderBackend(const QString& backend)
    {
        const QString value = backend.trimmed().toLower();
        if (value == QLatin1String("gpu") || value == QLatin1String("diligent") || value == QLatin1String("hardware")) {
            return QStringLiteral("gpu");
        }
        if (value == QLatin1String("cpu") || value == QLatin1String("software") || value == QLatin1String("qpainter") || value == QLatin1String("qpaint")) {
            return QStringLiteral("cpu");
        }
        if (value == QLatin1String("external") || value == QLatin1String("external-cycles") || value == QLatin1String("process") || value == QLatin1String("outofprocess")) {
            return QStringLiteral("external");
        }
        return QStringLiteral("auto");
    }

    QString deriveContainerFromJob(const ArtifactRenderJob& job)
    {
        const QFileInfo info(job.outputPath.trimmed());
        const QString suffix = info.suffix().toLower();
        if (!suffix.isEmpty()) {
            return suffix;
        }
        const QString fmt = job.outputFormat.trimmed().toLower();
        if (fmt.contains(QStringLiteral("sequence")) || fmt == QStringLiteral("png")) {
            return QStringLiteral("png");
        }
        if (fmt.contains(QStringLiteral("exr"))) {
            return QStringLiteral("exr");
        }
        if (fmt.contains(QStringLiteral("tiff")) || fmt.contains(QStringLiteral("tif"))) {
            return QStringLiteral("tiff");
        }
        if (fmt.contains(QStringLiteral("jpeg")) || fmt.contains(QStringLiteral("jpg"))) {
            return QStringLiteral("jpeg");
        }
        if (fmt.contains(QStringLiteral("bmp"))) {
            return QStringLiteral("bmp");
        }
        if (fmt.contains(QStringLiteral("webm"))) {
            return QStringLiteral("webm");
        }
        if (fmt.contains(QStringLiteral("mkv")) || fmt.contains(QStringLiteral("matroska"))) {
            return QStringLiteral("mkv");
        }
        if (fmt.contains(QStringLiteral("mov")) || fmt.contains(QStringLiteral("qt"))) {
            return QStringLiteral("mov");
        }
        if (fmt.contains(QStringLiteral("avi"))) {
            return QStringLiteral("avi");
        }
        if (fmt.contains(QStringLiteral("wmv"))) {
            return QStringLiteral("wmv");
        }
        if (fmt.contains(QStringLiteral("gif"))) {
            return QStringLiteral("gif");
        }
        if (fmt.contains(QStringLiteral("apng"))) {
            return QStringLiteral("apng");
        }
        if (fmt.contains(QStringLiteral("webp"))) {
            return QStringLiteral("webp");
        }
        if (fmt.contains(QStringLiteral("h.264")) || fmt.contains(QStringLiteral("h264")) ||
            fmt.contains(QStringLiteral("avc")) || fmt.contains(QStringLiteral("mpeg4"))) {
            return QStringLiteral("mp4");
        }
        if (fmt.contains(QStringLiteral("h.265")) || fmt.contains(QStringLiteral("h265")) ||
            fmt.contains(QStringLiteral("hevc"))) {
            return QStringLiteral("mkv");
        }
        if (!fmt.isEmpty()) {
            return fmt;
        }
        return QStringLiteral("mp4");
    }

    static QString sanitizeFileComponent(QString value)
    {
        value = value.trimmed();
        if (value.isEmpty()) {
            return QStringLiteral("output");
        }
        value.replace(QRegularExpression(QStringLiteral(R"([\\/:*?"<>|]+)")), QStringLiteral("_"));
        value.replace(QRegularExpression(QStringLiteral(R"(\s+)")), QStringLiteral("_"));
        value.remove(QRegularExpression(QStringLiteral(R"(^[._-]+)")));
        value.remove(QRegularExpression(QStringLiteral(R"([._-]+$)")));
        return value.isEmpty() ? QStringLiteral("output") : value;
    }

    static QString defaultOutputStemForJob(const ArtifactRenderJob& job)
    {
        const QString jobName = sanitizeFileComponent(job.jobName);
        if (jobName != QStringLiteral("output")) {
            return jobName;
        }
        const QString compName = sanitizeFileComponent(job.compositionName);
        return compName == QStringLiteral("output") ? QStringLiteral("output") : compName;
    }

    QString defaultOutputPathForJob(const ArtifactRenderJob& job)
    {
        const QString desktop = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
        const QDir baseDir(desktop.isEmpty() ? QDir::homePath() : desktop);
        const QString stem = defaultOutputStemForJob(job);

        const QString format = job.outputFormat.trimmed().toLower();
        if (format.contains(QStringLiteral("sequence")) ||
            format == QStringLiteral("png") ||
            format == QStringLiteral("exr") ||
            format == QStringLiteral("tiff") ||
            format == QStringLiteral("tif") ||
            format == QStringLiteral("jpeg") ||
            format == QStringLiteral("jpg") ||
            format == QStringLiteral("bmp")) {
            return baseDir.filePath(QStringLiteral("%1_sequence").arg(stem));
        }

        const QString ext = Artifact::deriveContainerFromJob(job);
        return baseDir.filePath(QStringLiteral("%1.%2").arg(stem, ext));
    }

    bool isImageSequenceContainer(const QString& format)
    {
        const QString value = format.trimmed().toLower();
        return value.contains(QStringLiteral("sequence")) ||
               value == QStringLiteral("png") ||
               value == QStringLiteral("exr") ||
               value == QStringLiteral("tiff") ||
               value == QStringLiteral("tif") ||
               value == QStringLiteral("jpeg") ||
               value == QStringLiteral("jpg") ||
               value == QStringLiteral("bmp") ||
               value == QStringLiteral("svg");
    }

    QString sequenceExtension(const QString& format, const QString& codec)
    {
        const QString fmt = format.trimmed().toLower();
        const QString cdc = codec.trimmed().toLower();
        // Check codec first, then format
        for (const auto& s : {cdc, fmt}) {
            if (s.contains("exr"))  return QStringLiteral("exr");
            if (s.contains("tiff") || s.contains("tif")) return QStringLiteral("tiff");
            if (s.contains("jpeg") || s.contains("jpg")) return QStringLiteral("jpg");
            if (s.contains("bmp"))  return QStringLiteral("bmp");
            if (s.contains("png"))  return QStringLiteral("png");
            if (s.contains("svg"))  return QStringLiteral("svg");
        }
        return QStringLiteral("png"); // default
    }

    bool isVideoContainer(const QString& format)
    {
        const QString value = format.trimmed().toLower();
        return value == QStringLiteral("mp4") ||
               value == QStringLiteral("mov") ||
               value == QStringLiteral("avi") ||
               value == QStringLiteral("mkv") ||
               value == QStringLiteral("webm") ||
               value == QStringLiteral("wmv") ||
               value == QStringLiteral("gif") ||
               value == QStringLiteral("apng") ||
               value == QStringLiteral("webp");
    }

    bool isAudioContainer(const QString& format)
    {
        const QString value = format.trimmed().toLower();
        return value == QStringLiteral("wav");
    }

    QString normalizeCodecName(const QString& codec)
    {
        const QString value = codec.trimmed().toLower();
        if (value.isEmpty() || value == QStringLiteral("h.264") || value == QStringLiteral("h264") ||
            value == QStringLiteral("avc") || value == QStringLiteral("libx264")) {
            return QStringLiteral("h264");
        }
        if (value == QStringLiteral("h.265") || value == QStringLiteral("h265") ||
            value == QStringLiteral("hevc") || value == QStringLiteral("libx265")) {
            return QStringLiteral("h265");
        }
        if (value == QStringLiteral("prores") || value == QStringLiteral("apple_prores")) {
            return QStringLiteral("prores");
        }
        if (value == QStringLiteral("mjpeg") || value == QStringLiteral("motion_jpeg")) {
            return QStringLiteral("mjpeg");
        }
        if (value == QStringLiteral("png")) {
            return QStringLiteral("png");
        }
        if (value == QStringLiteral("gif")) {
            return QStringLiteral("gif");
        }
        if (value == QStringLiteral("apng")) {
            return QStringLiteral("apng");
        }
        if (value == QStringLiteral("webp")) {
            return QStringLiteral("webp");
        }
        if (value == QStringLiteral("vp9") || value == QStringLiteral("libvpx-vp9")) {
            return QStringLiteral("vp9");
        }
        return value;
    }

    static QString ffmpegPipeEncoderName(const QString& codec, bool preferHardware = false)
    {
        const QString value = normalizeCodecName(codec);
        if (preferHardware) {
            if (value == QStringLiteral("h264")) return QStringLiteral("h264_nvenc");
            if (value == QStringLiteral("h265")) return QStringLiteral("hevc_nvenc");
        }
        if (value == QStringLiteral("h264")) return QStringLiteral("libx264");
        if (value == QStringLiteral("h265")) return QStringLiteral("libx265");
        if (value == QStringLiteral("prores")) return QStringLiteral("prores_ks");
        if (value == QStringLiteral("mjpeg")) return QStringLiteral("mjpeg");
        if (value == QStringLiteral("png")) return QStringLiteral("png");
        if (value == QStringLiteral("gif")) return QStringLiteral("gif");
        if (value == QStringLiteral("apng")) return QStringLiteral("apng");
        if (value == QStringLiteral("webp")) return QStringLiteral("libwebp_anim");
        if (value == QStringLiteral("vp9")) return QStringLiteral("libvpx-vp9");
        return QStringLiteral("libx264");
    }

    static QString ffmpegPipeVulkanEncoderName(const QString& codec)
    {
        const QString value = normalizeCodecName(codec);
        if (value == QStringLiteral("h264")) return QStringLiteral("h264_vulkan");
        if (value == QStringLiteral("h265")) return QStringLiteral("hevc_vulkan");
        return QString();
    }

    static QString normalizeProresProfile(const QString& profile)
    {
        const QString value = profile.trimmed().toLower();
        if (value.isEmpty() || value == QStringLiteral("422") || value == QStringLiteral("hq") || value == QStringLiteral("prores422")) {
            return QStringLiteral("hq");
        }
        if (value == QStringLiteral("4444") || value == QStringLiteral("prores4444")) {
            return QStringLiteral("4444");
        }
        if (value == QStringLiteral("lt") || value == QStringLiteral("proxy") || value == QStringLiteral("standard")) {
            return value;
        }
        return QStringLiteral("hq");
    }

    static QString ffmpegPipeProresProfileFlag(const QString& profile)
    {
        const QString value = normalizeProresProfile(profile);
        if (value == QStringLiteral("4444")) return QStringLiteral("4");
        if (value == QStringLiteral("lt")) return QStringLiteral("2");
        if (value == QStringLiteral("proxy")) return QStringLiteral("1");
        if (value == QStringLiteral("standard")) return QStringLiteral("3");
        return QStringLiteral("3");
    }

    static QString ffmpegPipePixelFormat(const QString& codec, const QString& codecProfile = QString())
    {
        const QString value = normalizeCodecName(codec);
        if (value == QStringLiteral("prores")) {
            const QString profile = normalizeProresProfile(codecProfile);
            if (profile == QStringLiteral("4444")) {
                return QStringLiteral("yuva444p10le");
            }
            return QStringLiteral("yuv422p10le");
        }
        if (value == QStringLiteral("mjpeg")) return QStringLiteral("yuvj420p");
        if (value == QStringLiteral("png")) return QStringLiteral("rgba");
        if (value == QStringLiteral("gif")) return QStringLiteral("pal8");
        if (value == QStringLiteral("apng")) return QStringLiteral("rgba");
        if (value == QStringLiteral("webp")) return QStringLiteral("rgba");
        return QStringLiteral("yuv420p");
    }

    static QStringList ffmpegPipeQualityArgs(const ArtifactRenderJob& job, bool preferHardware = false)
    {
        const QString value = normalizeCodecName(job.codec);
        QStringList args;
        if (preferHardware && (value == QStringLiteral("h264") || value == QStringLiteral("h265"))) {
            args << QStringLiteral("-preset") << QStringLiteral("p5")
                 << QStringLiteral("-b:v") << QString::number(std::max(1, job.bitrate) * 1000)
                 << QStringLiteral("-maxrate") << QString::number(std::max(1, job.bitrate) * 1200)
                 << QStringLiteral("-bufsize") << QString::number(std::max(1, job.bitrate) * 2000);
            return args;
        }
        if (value == QStringLiteral("h264") || value == QStringLiteral("h265")) {
            args << QStringLiteral("-preset") << QStringLiteral("slow")
                 << QStringLiteral("-crf") << QStringLiteral("18")
                 << QStringLiteral("-profile:v") << (value == QStringLiteral("h265") ? QStringLiteral("main") : QStringLiteral("high"));
            return args;
        }
        if (value == QStringLiteral("vp9")) {
            args << QStringLiteral("-b:v") << QStringLiteral("0")
                 << QStringLiteral("-crf") << QStringLiteral("18")
                 << QStringLiteral("-deadline") << QStringLiteral("good")
                 << QStringLiteral("-cpu-used") << QStringLiteral("2");
            return args;
        }
        if (value == QStringLiteral("prores")) {
            args << QStringLiteral("-profile:v") << ffmpegPipeProresProfileFlag(job.codecProfile);
            return args;
        }
        if (value == QStringLiteral("mjpeg")) {
            args << QStringLiteral("-q:v") << QStringLiteral("2");
            return args;
        }
        if (value == QStringLiteral("gif")) {
            args << QStringLiteral("-loop") << QStringLiteral("0");
            return args;
        }
        if (value == QStringLiteral("apng")) {
            args << QStringLiteral("-plays") << QStringLiteral("0");
            return args;
        }
        if (value == QStringLiteral("webp")) {
            args << QStringLiteral("-loop") << QStringLiteral("0");
            return args;
        }
        return args;
    }

    static ArtifactCore::FFmpegEncoderSettings buildNativeVideoSettings(const ArtifactRenderJob& job)
    {
        ArtifactCore::FFmpegEncoderSettings settings;
        settings.width = std::max(1, job.resolutionWidth);
        settings.height = std::max(1, job.resolutionHeight);
        settings.fps = job.frameRate > 0.0 ? job.frameRate : 30.0;
        settings.bitrateKbps = std::max(1, job.bitrate);
        const QString codec = normalizeCodecName(job.codec);
        if (codec == QStringLiteral("h264")) {
            settings.videoCodec = QStringLiteral("h264");
            settings.preset = QStringLiteral("slow");
            settings.crf = 18;
            settings.gopSize = std::max(1, static_cast<int>(std::round(settings.fps * 2.0)));
            settings.profile = QStringLiteral("high");
            settings.zerolatency = false;
        } else if (codec == QStringLiteral("h265")) {
            settings.videoCodec = QStringLiteral("h265");
            settings.preset = QStringLiteral("slow");
            settings.crf = 18;
            settings.gopSize = std::max(1, static_cast<int>(std::round(settings.fps * 2.0)));
            settings.profile = QStringLiteral("main");
            settings.zerolatency = false;
        } else if (codec == QStringLiteral("prores")) {
            settings.videoCodec = QStringLiteral("prores");
            settings.preset = QStringLiteral("slow");
            settings.profile = normalizeProresProfile(job.codecProfile);
            settings.zerolatency = false;
        } else if (codec == QStringLiteral("mjpeg")) {
            settings.videoCodec = QStringLiteral("mjpeg");
            settings.preset = QStringLiteral("slow");
            settings.zerolatency = false;
        } else if (codec == QStringLiteral("png")) {
            settings.videoCodec = QStringLiteral("png");
            settings.preset = QStringLiteral("slow");
            settings.zerolatency = false;
        } else if (codec == QStringLiteral("gif")) {
            settings.videoCodec = QStringLiteral("gif");
            settings.container = QStringLiteral("gif");
            settings.preset = QStringLiteral("slow");
            settings.zerolatency = false;
        } else if (codec == QStringLiteral("apng")) {
            settings.videoCodec = QStringLiteral("apng");
            settings.container = QStringLiteral("apng");
            settings.preset = QStringLiteral("slow");
            settings.zerolatency = false;
        } else if (codec == QStringLiteral("webp")) {
            settings.videoCodec = QStringLiteral("webp");
            settings.container = QStringLiteral("webp");
            settings.preset = QStringLiteral("slow");
            settings.zerolatency = false;
        } else if (codec == QStringLiteral("vp9")) {
            settings.videoCodec = QStringLiteral("vp9");
            settings.preset = QStringLiteral("slow");
            settings.crf = 18;
            settings.gopSize = std::max(1, static_cast<int>(std::round(settings.fps * 2.0)));
            settings.zerolatency = false;
        } else {
            settings.videoCodec = codec;
            settings.preset = QStringLiteral("slow");
            settings.crf = 18;
            settings.gopSize = std::max(1, static_cast<int>(std::round(settings.fps * 2.0)));
            settings.zerolatency = false;
        }
        settings.container = Artifact::deriveContainerFromJob(job);
        return settings;
    }

    static ArtifactCore::FFmpegEncoderSettings buildGpuVideoSettings(const ArtifactRenderJob& job,
                                                                     QString* selectedEncoder,
                                                                     QString* errorMessage)
    {
        ArtifactCore::FFmpegEncoderSettings settings = buildNativeVideoSettings(job);
        const QString encoderName = hardwareEncoderNameForCodec(job.codec);
        if (encoderName.isEmpty()) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("No hardware encoder found for codec: %1 (available: %2)")
                    .arg(normalizeCodecName(job.codec),
                         ArtifactCore::FFmpegEncoder::availableVideoCodecs().join(QStringLiteral(", ")));
            }
            if (selectedEncoder) {
                selectedEncoder->clear();
            }
            return settings;
        }

        settings.videoCodec = encoderName;
        settings.encoderName = encoderName;
        settings.preferHardware = true;
        settings.preset = QStringLiteral("p4");
        settings.zerolatency = false;
        if (selectedEncoder) {
            *selectedEncoder = encoderName;
        }
        if (errorMessage) {
            errorMessage->clear();
        }
        return settings;
    }

    static void recordEncodeFailure(const char* code,
                                    const QString& message,
                                    const QString& objectId = QString(),
                                    const std::int64_t frameIndex = -1)
    {
        auto diagnostic = ArtifactCore::makeDiagnosticEvent(
            ArtifactCore::CoreDiagnosticSeverity::Error,
            code,
            message.toStdString(),
            "RenderEncoder",
            code,
            objectId.toStdString());
        diagnostic.frameIndex = frameIndex;
        ArtifactCore::DiagnosticRecorder::instance().record(std::move(diagnostic));
    }

    class PipeFFmpegExeBackend final : public IVideoEncodeBackend {
    public:
        explicit PipeFFmpegExeBackend(bool preferHardware = false, bool preferVulkan = false)
            : preferHardware_(preferHardware)
            , preferVulkan_(preferVulkan)
        {
        }

        bool open(const ArtifactRenderJob& job, QString* errorMessage) override
        {
            close(nullptr);

            process_ = std::make_unique<QProcess>();
            process_->setProcessChannelMode(QProcess::MergedChannels);

            const int width = std::max(1, job.resolutionWidth);
            const int height = std::max(1, job.resolutionHeight);
            const double fps = job.frameRate > 0.0 ? job.frameRate : 30.0;
            const QString codec = normalizeCodecName(job.codec);
            if ((preferHardware_ || preferVulkan_) && codec != QStringLiteral("h264") && codec != QStringLiteral("h265")) {
                if (errorMessage) {
                    *errorMessage = QStringLiteral("Hardware pipe encoding is only supported for H.264/H.265 jobs");
                }
                lastError_ = QStringLiteral("Unsupported hardware codec: %1").arg(codec);
                recordEncodeFailure("encode.open_failed", lastError_, job.outputPath);
                return false;
            }

            QStringList args;
            args << QStringLiteral("-y");
            if (preferVulkan_) {
                args << QStringLiteral("-init_hw_device") << QStringLiteral("vulkan=artifact_vk:0")
                     << QStringLiteral("-filter_hw_device") << QStringLiteral("artifact_vk");
            }
            args << QStringLiteral("-f") << QStringLiteral("rawvideo")
                 << QStringLiteral("-pixel_format") << QStringLiteral("rgba")
                 << QStringLiteral("-video_size") << QStringLiteral("%1x%2").arg(width).arg(height)
                 << QStringLiteral("-framerate") << QString::number(fps, 'f', 6)
                 << QStringLiteral("-i") << QStringLiteral("-")
                 << QStringLiteral("-c:v") << (preferVulkan_ ? ffmpegPipeVulkanEncoderName(codec)
                                                             : ffmpegPipeEncoderName(codec, preferHardware_));

            const QStringList qualityArgs = ffmpegPipeQualityArgs(job, preferHardware_);
            for (const QString& arg : qualityArgs) {
                args << arg;
            }
            if (preferVulkan_) {
                args << QStringLiteral("-vf") << QStringLiteral("format=rgba,hwupload")
                     << QStringLiteral("-b:v") << QString::number(std::max(1, job.bitrate) * 1000);
            } else {
                args << QStringLiteral("-pix_fmt") << ffmpegPipePixelFormat(codec, job.codecProfile);
            }
            args << job.outputPath;

            const QString ffmpegPath = resolveFfmpegExePath();
            if (preferHardware_ || preferVulkan_) {
                const QString hwEncoder = preferVulkan_
                    ? ffmpegPipeVulkanEncoderName(codec)
                    : (codec == QStringLiteral("h265"))
                    ? QStringLiteral("hevc_nvenc")
                    : QStringLiteral("h264_nvenc");
                if (!ffmpegExeSupportsEncoder(ffmpegPath, hwEncoder)) {
                    if (errorMessage) {
                        *errorMessage = QStringLiteral("ffmpeg.exe does not advertise %1").arg(hwEncoder);
                    }
                    lastError_ = QStringLiteral("Missing hardware encoder: %1").arg(hwEncoder);
                    recordEncodeFailure("encode.open_failed", lastError_, job.outputPath);
                    close(nullptr);
                    return false;
                }
            }
            process_->start(ffmpegPath, args);
            if (!process_->waitForStarted()) {
                if (errorMessage) {
                    *errorMessage = QStringLiteral(
                        "Failed to start ffmpeg.exe bridge: %1 (%2)")
                        .arg(ffmpegPath, process_->errorString());
                }
                lastError_ = errorMessage ? *errorMessage
                                          : process_->errorString();
                recordEncodeFailure("encode.open_failed", lastError_, job.outputPath);
                close(nullptr);
                return false;
            }

            qInfo() << "[Encode][Pipe] started"
                    << "program=" << ffmpegPath
                    << "arguments=" << args
                    << "output=" << job.outputPath;
            lastError_.clear();
            return true;
        }

        bool addFrame(const QImage& frame, int frameIndex, QString* errorMessage) override
        {
            if (!process_) {
                const QString message = QStringLiteral("ffmpeg.exe bridge is not open");
                lastError_ = message;
                if (errorMessage) *errorMessage = message;
                return false;
            }

            // canvas は既に Format_RGBA8888 のため変換不要
            const QImage& rgba = (frame.format() == QImage::Format_RGBA8888)
                ? frame
                : frame.convertToFormat(QImage::Format_RGBA8888);
            const qint64 expectedBytes = static_cast<qint64>(rgba.width()) * rgba.height() * 4;
            const qint64 written = process_->write(reinterpret_cast<const char*>(rgba.constBits()), expectedBytes);
            if (written != expectedBytes) {
                const QString processOutput = QString::fromUtf8(process_->readAll());
                const QString message = QStringLiteral(
                    "Failed to write frame %1 to ffmpeg.exe: expected=%2 written=%3 "
                    "processState=%4 output=%5")
                    .arg(frameIndex)
                    .arg(expectedBytes)
                    .arg(written)
                    .arg(static_cast<int>(process_->state()))
                    .arg(processOutput.right(2048));
                lastError_ = message;
                if (errorMessage) *errorMessage = message;
                return false;
            }

            if (!process_->waitForBytesWritten(30000)) {
                const QString message = QStringLiteral(
                    "Timed out while writing frame %1 to ffmpeg.exe: %2")
                    .arg(frameIndex)
                    .arg(QString::fromUtf8(process_->readAll()).right(2048));
                lastError_ = message;
                if (errorMessage) *errorMessage = message;
                return false;
            }

            return true;
        }

        bool close(QString* errorMessage) override
        {
            if (!process_) {
                return lastError_.isEmpty();
            }

            process_->closeWriteChannel();
            bool finished = process_->state() == QProcess::NotRunning ||
                            process_->waitForFinished(30000);
            if (!finished) {
                process_->terminate();
                if (!process_->waitForFinished(5000)) {
                    process_->kill();
                    finished = process_->waitForFinished(5000);
                } else {
                    finished = true;
                }
            }
            const int exitCode = process_->exitCode();
            const QProcess::ExitStatus exitStatus = process_->exitStatus();
            const QString processOutput =
                QString::fromUtf8(process_->readAll()).right(4096);
            const bool success = finished &&
                                 exitStatus == QProcess::NormalExit &&
                                 exitCode == 0;
            if (!success) {
                lastError_ = QStringLiteral(
                    "ffmpeg.exe finalization failed: finished=%1 exitStatus=%2 "
                    "exitCode=%3 processError=%4 output=%5")
                    .arg(finished)
                    .arg(static_cast<int>(exitStatus))
                    .arg(exitCode)
                    .arg(process_->errorString())
                    .arg(processOutput);
                if (errorMessage) *errorMessage = lastError_;
                qWarning() << "[Encode][Pipe] finalize failed" << lastError_;
                recordEncodeFailure("encode.finalize_failed", lastError_);
            } else {
                qInfo() << "[Encode][Pipe] finalized"
                        << "exitCode=" << exitCode
                        << "output=" << processOutput;
            }
            process_.reset();
            return success;
        }

        QString lastError() const { return lastError_; }

    private:
        std::unique_ptr<QProcess> process_;
        QString lastError_;
        bool preferHardware_ = false;
        bool preferVulkan_ = false;
    };

    class NativeFFmpegBackend final : public IVideoEncodeBackend {
    public:
        explicit NativeFFmpegBackend(bool useGpuBackend = false)
            : useGpuBackend_(useGpuBackend)
        {
        }

        bool open(const ArtifactRenderJob& job, QString* errorMessage) override
        {
            QString selectedEncoder;
            ArtifactCore::FFmpegEncoderSettings settings = useGpuBackend_
                ? buildGpuVideoSettings(job, &selectedEncoder, errorMessage)
                : buildNativeVideoSettings(job);
            if (useGpuBackend_ && selectedEncoder.isEmpty()) {
                lastError_ = errorMessage ? *errorMessage : QStringLiteral("No hardware encoder available");
                recordEncodeFailure("encode.open_failed", lastError_, job.outputPath);
                return false;
            }
            if (!encoder_.open(job.outputPath, settings)) {
                lastError_ = encoder_.lastError();
                if (errorMessage) *errorMessage = lastError_;
                qWarning() << "[Encode][Native] open failed"
                           << "output=" << job.outputPath
                           << "codec=" << settings.videoCodec
                           << "encoder=" << settings.encoderName
                           << "container=" << settings.container
                           << "resolution=" << settings.width << "x"
                           << settings.height
                           << "fps=" << settings.fps
                           << "error=" << lastError_;
                recordEncodeFailure("encode.open_failed", lastError_, job.outputPath);
                return false;
            }

            qInfo() << "[Encode][Native] opened"
                    << "output=" << job.outputPath
                    << "codec=" << settings.videoCodec
                    << "encoder=" << settings.encoderName
                    << "container=" << settings.container
                    << "resolution=" << settings.width << "x"
                    << settings.height
                    << "fps=" << settings.fps
                    << "bitrateKbps=" << settings.bitrateKbps;
            lastError_.clear();
            return true;
        }

        bool addFrame(const QImage& frame, int /*frameIndex*/, QString* errorMessage) override
        {
            const QImage rgba = (frame.format() == QImage::Format_RGBA8888)
                ? frame
                : frame.convertToFormat(QImage::Format_RGBA8888);
            if (!encoder_.addImage(rgba)) {
                lastError_ = encoder_.lastError();
                if (errorMessage) *errorMessage = lastError_;
                return false;
            }
            return true;
        }

        bool close(QString* errorMessage) override
        {
            encoder_.close();
            lastError_ = encoder_.lastError();
            if (!lastError_.isEmpty()) {
                if (errorMessage) *errorMessage = lastError_;
                qWarning() << "[Encode][Native] finalize failed" << lastError_;
                recordEncodeFailure("encode.finalize_failed", lastError_);
                return false;
            }
            qInfo() << "[Encode][Native] finalized";
            return true;
        }

        QString lastError() const { return lastError_; }

    private:
        bool useGpuBackend_ = false;
        ArtifactCore::FFmpegEncoder encoder_;
        QString lastError_;
    };

    std::unique_ptr<IVideoEncodeBackend> createVideoEncodeBackend(const ArtifactRenderJob& job,
                                                                          QString* backendName,
                                                                          QString* errorMessage)
    {
        const VideoEncodeBackendKind requested = parseVideoEncodeBackend(job.encoderBackend);
        const auto tryGpu = [&]() -> std::unique_ptr<IVideoEncodeBackend> {
            QString localError;
            auto backend = std::make_unique<NativeFFmpegBackend>(true);
            if (backend->open(job, &localError)) {
                if (backendName) {
                    const QString encoderName = hardwareEncoderNameForCodec(job.codec);
                    *backendName = encoderName.isEmpty()
                        ? QStringLiteral("gpu")
                        : QStringLiteral("gpu:%1").arg(encoderName);
                }
                if (errorMessage) errorMessage->clear();
                return backend;
            }
            qWarning() << "[RenderQueue] GPU FFmpeg backend failed; falling back to native"
                       << "jobId=" << job.compositionId.toString()
                       << "codec=" << job.codec
                       << "error=" << localError;
            if (errorMessage) *errorMessage = localError;
            return nullptr;
        };
        const auto tryNative = [&]() -> std::unique_ptr<IVideoEncodeBackend> {
            QString localError;
            auto backend = std::make_unique<NativeFFmpegBackend>();
            if (backend->open(job, &localError)) {
                if (backendName) *backendName = QStringLiteral("native");
                if (errorMessage) errorMessage->clear();
                return backend;
            }
            qWarning() << "[Encode][Native] backend attempt failed"
                       << "jobId=" << job.compositionId.toString()
                       << "codec=" << job.codec
                       << "error=" << localError;
            if (errorMessage) *errorMessage = localError;
            return nullptr;
        };
        const auto tryPipe = [&]() -> std::unique_ptr<IVideoEncodeBackend> {
            QString localError;
            auto backend = std::make_unique<PipeFFmpegExeBackend>();
            if (backend->open(job, &localError)) {
                if (backendName) *backendName = QStringLiteral("pipe");
                if (errorMessage) errorMessage->clear();
                return backend;
            }
            qWarning() << "[Encode][Pipe] backend attempt failed"
                       << "jobId=" << job.compositionId.toString()
                       << "codec=" << job.codec
                       << "error=" << localError;
            if (errorMessage) *errorMessage = localError;
            return nullptr;
        };
        const auto tryPipeHardware = [&]() -> std::unique_ptr<IVideoEncodeBackend> {
            QString localError;
            auto backend = std::make_unique<PipeFFmpegExeBackend>(true);
            if (backend->open(job, &localError)) {
                if (backendName) *backendName = QStringLiteral("pipe-hw");
                if (errorMessage) errorMessage->clear();
                return backend;
            }
            qWarning() << "[RenderQueue] pipe-hw backend failed; falling back to pipe"
                       << "jobId=" << job.compositionId.toString()
                       << "codec=" << job.codec
                       << "error=" << localError;
            if (errorMessage) *errorMessage = localError;
            return nullptr;
        };
        const auto tryPipeVulkan = [&]() -> std::unique_ptr<IVideoEncodeBackend> {
            QString localError;
            auto backend = std::make_unique<PipeFFmpegExeBackend>(false, true);
            if (backend->open(job, &localError)) {
                if (backendName) *backendName = QStringLiteral("pipe-vulkan");
                if (errorMessage) errorMessage->clear();
                return backend;
            }
            qWarning() << "[RenderQueue] pipe-vulkan backend failed; falling back to pipe"
                       << "jobId=" << job.compositionId.toString()
                       << "codec=" << job.codec
                       << "error=" << localError;
            if (errorMessage) *errorMessage = localError;
            return nullptr;
        };

        switch (requested) {
        case VideoEncodeBackendKind::Pipe:
            return tryPipe();
        case VideoEncodeBackendKind::PipeHardware:
            if (auto hardwareBackend = tryPipeHardware()) {
                return hardwareBackend;
            }
            return tryPipe();
        case VideoEncodeBackendKind::PipeVulkan:
            if (auto vulkanBackend = tryPipeVulkan()) {
                return vulkanBackend;
            }
            return tryPipe();
        case VideoEncodeBackendKind::Native:
            return tryNative();
        case VideoEncodeBackendKind::GPU:
            if (auto gpuBackend = tryGpu()) {
                return gpuBackend;
            }
            if (auto hardwarePipeBackend = tryPipeHardware()) {
                return hardwarePipeBackend;
            }
            if (auto vulkanPipeBackend = tryPipeVulkan()) {
                return vulkanPipeBackend;
            }
            if (auto nativeBackend = tryNative()) {
                if (backendName) *backendName = QStringLiteral("native-fallback");
                if (errorMessage) {
                    *errorMessage = QStringLiteral("GPU backend unavailable, fell back to native FFmpeg encoding");
                }
                return nativeBackend;
            }
            if (errorMessage) {
                *errorMessage = QStringLiteral("GPU backend unavailable and native fallback failed");
            }
            return tryPipe();
        case VideoEncodeBackendKind::Auto:
        default:
            if (auto gpuBackend = tryGpu()) {
                return gpuBackend;
            }
            if (auto hardwarePipeBackend = tryPipeHardware()) {
                return hardwarePipeBackend;
            }
            if (auto vulkanPipeBackend = tryPipeVulkan()) {
                return vulkanPipeBackend;
            }
            if (auto nativeBackend = tryNative()) {
                return nativeBackend;
            }
            qWarning() << "[RenderQueue] auto backend fell back to pipe encoding"
                       << "jobId=" << job.compositionId.toString()
                       << "codec=" << job.codec;
            return tryPipe();
        }
    }


}
