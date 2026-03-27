module;
#include <tbb/parallel_for.h>
#include <tbb/blocked_range.h>
#include <tbb/task_arena.h>
#include <QObject>
#include <QList>
#include <QThread>
#include <QDir>
#include <QImage>
#include <QPainter>
#include <QFont>
#include <QTextOption>
#include <QDateTime>
#include <QFileInfo>
#include <QStringList>
#include <QPointF>
#include <QRegularExpression>
#include <QProcess>
#include <QCoreApplication>
#include <opencv2/opencv.hpp>
#include <wobjectimpl.h>
#include <mutex>
#include <map>

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
#include <tbb/parallel_for.h>
#include <tbb/blocked_range.h>
module Artifact.Render.Queue.Service;




import Render.Queue.Manager;
import Artifact.Project.Manager;
import Artifact.Project.Items;
import Artifact.Service.Project;
import Image.ImageF32x4_RGBA;
import CvUtils;
import Utils.Id;
import Artifact.Render.SoftwareCompositor;
import Artifact.Render.IRenderer;
import Encoder.FFmpegEncoder;
import IO.ImageExporter;
import Image.ExportOptions;
import Artifact.Composition.Abstract;
import Artifact.Layer.Abstract;
import Artifact.Layer.Image;
import Artifact.Layer.Svg;
import Artifact.Layer.Text;
import Artifact.Layer.Video;
import Artifact.Layers.SolidImage;
import Artifact.Layer.Solid2D;
import Layer.Blend;
import Color.Float;

namespace Artifact
{
    namespace {
        QString resolveFfmpegExePath()
        {
            const QString executableName = QStringLiteral("ffmpeg.exe");
            const QString executableStem = QStringLiteral("ffmpeg");

            const QString appDir = QCoreApplication::applicationDirPath();
            const QString currentDir = QDir::currentPath();

            const QStringList candidatePaths = {
                QDir(appDir).filePath(executableName),
                QDir(appDir).filePath(executableStem),
                QDir(appDir).filePath(QStringLiteral("ffmpeg/") + executableName),
                QDir(appDir).filePath(QStringLiteral("ffmpeg/") + executableStem),
                QDir(appDir).filePath(QStringLiteral("tools/") + executableName),
                QDir(appDir).filePath(QStringLiteral("tools/") + executableStem),
                QDir(appDir).filePath(QStringLiteral("tools/ffmpeg/") + executableName),
                QDir(appDir).filePath(QStringLiteral("tools/ffmpeg/") + executableStem),
                QDir(appDir).filePath(QStringLiteral("bin/") + executableName),
                QDir(appDir).filePath(QStringLiteral("bin/") + executableStem),
                QDir(appDir).filePath(QStringLiteral("../") + executableName),
                QDir(appDir).filePath(QStringLiteral("../") + executableStem),
                QDir(appDir).filePath(QStringLiteral("../ffmpeg/") + executableName),
                QDir(appDir).filePath(QStringLiteral("../ffmpeg/") + executableStem),
                QDir(appDir).filePath(QStringLiteral("../tools/") + executableName),
                QDir(appDir).filePath(QStringLiteral("../tools/") + executableStem),
                QDir(appDir).filePath(QStringLiteral("../tools/ffmpeg/") + executableName),
                QDir(appDir).filePath(QStringLiteral("../tools/ffmpeg/") + executableStem),
                QDir(appDir).filePath(QStringLiteral("../bin/") + executableName),
                QDir(appDir).filePath(QStringLiteral("../bin/") + executableStem),
                QDir(currentDir).filePath(executableName),
                QDir(currentDir).filePath(executableStem),
                QDir(currentDir).filePath(QStringLiteral("ffmpeg/") + executableName),
                QDir(currentDir).filePath(QStringLiteral("ffmpeg/") + executableStem),
                QDir(currentDir).filePath(QStringLiteral("tools/") + executableName),
                QDir(currentDir).filePath(QStringLiteral("tools/") + executableStem),
                QDir(currentDir).filePath(QStringLiteral("bin/") + executableName),
                QDir(currentDir).filePath(QStringLiteral("bin/") + executableStem)
            };

            for (const QString& candidate : candidatePaths) {
                const QFileInfo info(candidate);
                if (info.exists() && info.isFile()) {
                    return info.absoluteFilePath();
                }
            }

            const QStringList envPath = QProcessEnvironment::systemEnvironment()
                                            .value(QStringLiteral("PATH"))
                                            .split(QDir::listSeparator(), Qt::SkipEmptyParts);
            for (const QString& pathEntry : envPath) {
                const QString candidate = QDir(pathEntry).filePath(executableName);
                const QFileInfo info(candidate);
                if (info.exists() && info.isFile()) {
                    return info.absoluteFilePath();
                }
            }

            return executableName;
        }
    }

    // レンダリングジョブクラス
    class ArtifactRenderJob {
    public:
        enum class Status {
            Pending,       // 待機中
            Rendering,     // レンダリング中
            Completed,     // 完了
            Failed,        // 失敗
            Canceled       // キャンセル
        };

        ArtifactCore::CompositionID compositionId; // 対象コンポジションID
        QString compositionName;      // コンポジション名
        QString outputPath;           // 出力パス
        QString outputFormat;         // 出力形式 (MP4, PNG sequence等)
        QString codec;                // コーデック
        QString codecProfile;         // コーデックプロファイル (ProRes 422 / 4444 等)
        QString encoderBackend;       // "auto", "pipe", "native"
        int resolutionWidth;          // 解像度幅
        int resolutionHeight;         // 解像度高さ
        double frameRate;             // フレームレート
        int bitrate;                  // ビットレート (kbps)
        int startFrame;               // 開始フレーム
        int endFrame;                 // 終了フレーム
        Status status;                // ステータス
        int progress;                 // 進捗率 (0-100)
        QString errorMessage;         // エラーメッセージ
        float overlayOffsetX;
        float overlayOffsetY;
        float overlayScale;
        float overlayRotationDeg;

        ArtifactRenderJob()
            : resolutionWidth(1920)
            , resolutionHeight(1080)
            , frameRate(30.0)
            , bitrate(8000)
            , startFrame(0)
            , endFrame(100)
            , codecProfile()
            , encoderBackend(QStringLiteral("auto"))
            , status(Status::Pending)
            , progress(0)
            , overlayOffsetX(0.0f)
            , overlayOffsetY(0.0f)
            , overlayScale(1.0f)
            , overlayRotationDeg(0.0f)
        {
        }
    };

    namespace {
    enum class VideoEncodeBackendKind {
        Auto,
        Pipe,
        Native
    };

    static VideoEncodeBackendKind parseVideoEncodeBackend(const QString& backend)
    {
        const QString value = backend.trimmed().toLower();
        if (value == QLatin1String("pipe") || value == QLatin1String("ffmpeg.exe") || value == QLatin1String("ffmpeg")) {
            return VideoEncodeBackendKind::Pipe;
        }
        if (value == QLatin1String("native") || value == QLatin1String("api") || value == QLatin1String("ffmpegapi")) {
            return VideoEncodeBackendKind::Native;
        }
        return VideoEncodeBackendKind::Auto;
    }

    static QString deriveContainerFromJob(const ArtifactRenderJob& job)
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

    static bool isImageSequenceContainer(const QString& format)
    {
        const QString value = format.trimmed().toLower();
        return value.contains(QStringLiteral("sequence")) ||
               value == QStringLiteral("png") ||
               value == QStringLiteral("exr") ||
               value == QStringLiteral("tiff") ||
               value == QStringLiteral("tif") ||
               value == QStringLiteral("jpeg") ||
               value == QStringLiteral("jpg") ||
               value == QStringLiteral("bmp");
    }

    static QString sequenceExtension(const QString& format, const QString& codec)
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
        }
        return QStringLiteral("png"); // default
    }

    static bool isVideoContainer(const QString& format)
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

    static QString normalizeCodecName(const QString& codec)
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

    static QString ffmpegPipeEncoderName(const QString& codec)
    {
        const QString value = normalizeCodecName(codec);
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

    static QStringList ffmpegPipeQualityArgs(const ArtifactRenderJob& job)
    {
        const QString value = normalizeCodecName(job.codec);
        QStringList args;
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
        settings.container = deriveContainerFromJob(job);
        return settings;
    }

    static ArtifactCore::ImageF32x4_RGBA qImageToImageF32x4RGBA(const QImage& source)
    {
        const QImage rgba = source.convertToFormat(QImage::Format_RGBA8888);
        cv::Mat mat(rgba.height(), rgba.width(), CV_8UC4,
                    const_cast<uchar*>(rgba.constBits()),
                    rgba.bytesPerLine());
        ArtifactCore::ImageF32x4_RGBA out;
        out.setFromCVMat(mat);
        return out;
    }

    class IVideoEncodeBackend {
    public:
        virtual ~IVideoEncodeBackend() = default;
        virtual bool open(const ArtifactRenderJob& job, QString* errorMessage) = 0;
        virtual bool addFrame(const QImage& frame, int frameIndex, QString* errorMessage) = 0;
        virtual void close() = 0;
    };

    class PipeFFmpegExeBackend final : public IVideoEncodeBackend {
    public:
        bool open(const ArtifactRenderJob& job, QString* errorMessage) override
        {
            close();

            process_ = std::make_unique<QProcess>();
            process_->setProcessChannelMode(QProcess::MergedChannels);

            const int width = std::max(1, job.resolutionWidth);
            const int height = std::max(1, job.resolutionHeight);
            const double fps = job.frameRate > 0.0 ? job.frameRate : 30.0;
            const QString codec = normalizeCodecName(job.codec);

            QStringList args;
            args << QStringLiteral("-y")
                 << QStringLiteral("-f") << QStringLiteral("rawvideo")
                 << QStringLiteral("-pixel_format") << QStringLiteral("rgba")
                 << QStringLiteral("-video_size") << QStringLiteral("%1x%2").arg(width).arg(height)
                 << QStringLiteral("-framerate") << QString::number(fps, 'f', 6)
                 << QStringLiteral("-i") << QStringLiteral("-")
                 << QStringLiteral("-c:v") << ffmpegPipeEncoderName(codec);

            const QStringList qualityArgs = ffmpegPipeQualityArgs(job);
            for (const QString& arg : qualityArgs) {
                args << arg;
            }
            args << QStringLiteral("-pix_fmt") << ffmpegPipePixelFormat(codec, job.codecProfile)
                 << job.outputPath;

            const QString ffmpegPath = resolveFfmpegExePath();
            process_->start(ffmpegPath, args);
            if (!process_->waitForStarted()) {
                if (errorMessage) {
                    *errorMessage = QStringLiteral("Failed to start ffmpeg.exe bridge: %1").arg(ffmpegPath);
                }
                close();
                return false;
            }

            lastError_.clear();
            return true;
        }

        bool addFrame(const QImage& frame, int /*frameIndex*/, QString* errorMessage) override
        {
            if (!process_) {
                const QString message = QStringLiteral("ffmpeg.exe bridge is not open");
                lastError_ = message;
                if (errorMessage) *errorMessage = message;
                return false;
            }

            const QImage rgba = frame.convertToFormat(QImage::Format_RGBA8888);
            const qint64 expectedBytes = static_cast<qint64>(rgba.width()) * rgba.height() * 4;
            const qint64 written = process_->write(reinterpret_cast<const char*>(rgba.constBits()), expectedBytes);
            if (written != expectedBytes) {
                const QString message = QStringLiteral("Failed to write video frame to ffmpeg.exe");
                lastError_ = message;
                if (errorMessage) *errorMessage = message;
                return false;
            }

            if (!process_->waitForBytesWritten(30000)) {
                const QString message = QStringLiteral("Timed out while writing frame to ffmpeg.exe");
                lastError_ = message;
                if (errorMessage) *errorMessage = message;
                return false;
            }

            return true;
        }

        void close() override
        {
            if (!process_) {
                return;
            }

            process_->closeWriteChannel();
            if (!process_->waitForFinished(30000)) {
                process_->terminate();
                if (!process_->waitForFinished(5000)) {
                    process_->kill();
                    process_->waitForFinished(5000);
                }
            }
            process_.reset();
        }

        QString lastError() const { return lastError_; }

    private:
        std::unique_ptr<QProcess> process_;
        QString lastError_;
    };

    class NativeFFmpegBackend final : public IVideoEncodeBackend {
    public:
        bool open(const ArtifactRenderJob& job, QString* errorMessage) override
        {
            const ArtifactCore::FFmpegEncoderSettings settings = buildNativeVideoSettings(job);
            if (!encoder_.open(job.outputPath, settings)) {
                lastError_ = encoder_.lastError();
                if (errorMessage) *errorMessage = lastError_;
                return false;
            }

            lastError_.clear();
            return true;
        }

        bool addFrame(const QImage& frame, int /*frameIndex*/, QString* errorMessage) override
        {
            const ArtifactCore::ImageF32x4_RGBA image = qImageToImageF32x4RGBA(frame);
            if (!encoder_.addImage(image)) {
                lastError_ = encoder_.lastError();
                if (errorMessage) *errorMessage = lastError_;
                return false;
            }
            return true;
        }

        void close() override
        {
            encoder_.close();
        }

        QString lastError() const { return lastError_; }

    private:
        ArtifactCore::FFmpegEncoder encoder_;
        QString lastError_;
    };

    static std::unique_ptr<IVideoEncodeBackend> createVideoEncodeBackend(const ArtifactRenderJob& job,
                                                                          QString* backendName,
                                                                          QString* errorMessage)
    {
        const VideoEncodeBackendKind requested = parseVideoEncodeBackend(job.encoderBackend);
        const auto tryNative = [&]() -> std::unique_ptr<IVideoEncodeBackend> {
            QString localError;
            auto backend = std::make_unique<NativeFFmpegBackend>();
            if (backend->open(job, &localError)) {
                if (backendName) *backendName = QStringLiteral("native");
                if (errorMessage) errorMessage->clear();
                return backend;
            }
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
            if (errorMessage) *errorMessage = localError;
            return nullptr;
        };

        switch (requested) {
        case VideoEncodeBackendKind::Pipe:
            return tryPipe();
        case VideoEncodeBackendKind::Native:
            return tryNative();
        case VideoEncodeBackendKind::Auto:
        default:
            if (auto nativeBackend = tryNative()) {
                return nativeBackend;
            }
            return tryPipe();
        }
    }
    } // namespace

    // レンダリングキューマネージャクラス
    class ArtifactRenderQueueManager {
    public:
        // 失敗フレーム情報
        struct FailedFrameInfo {
            int jobId;
            int frameNumber;
            QString errorMessage;
            qint64 timestamp;
        };
        
        // 失敗フレーム検出器
        class FailedFrameDetector {
        public:
            QList<FailedFrameInfo> detectFailedFrames(int jobId, const ArtifactRenderJob& job) {
                QList<FailedFrameInfo> failedFrames;
                const int startFrame = job.startFrame;
                const int endFrame = job.endFrame;
                QString outputPath = job.outputPath.trimmed();
                
                // 画像シーケンスかどうかを判定
                const QString ext = QFileInfo(outputPath).suffix().toLower();
                const bool isSequence = (ext == QLatin1String("png") || ext == QLatin1String("exr") ||
                                        ext == QLatin1String("tiff") || ext == QLatin1String("tif") ||
                                        ext == QLatin1String("jpg") || ext == QLatin1String("jpeg") ||
                                        ext == QLatin1String("bmp"));
                
                if (isSequence) {
                    // シーケンスの場合、各フレームの存在をチェック
                    for (int f = startFrame; f <= endFrame; ++f) {
                        QString framePath = generateFramePath(outputPath, f);
                        if (!QFile::exists(framePath)) {
                            failedFrames.append(FailedFrameInfo{jobId, f, QStringLiteral("Frame not found"), QDateTime::currentMSecsSinceEpoch()});
                        } else if (QFileInfo(framePath).size() == 0) {
                            failedFrames.append(FailedFrameInfo{jobId, f, QStringLiteral("Frame is empty"), QDateTime::currentMSecsSinceEpoch()});
                        }
                    }
                } else {
                    // 動画ファイルの場合、ファイルの存在とサイズをチェック
                    if (!QFile::exists(outputPath)) {
                        failedFrames.append(FailedFrameInfo{jobId, -1, QStringLiteral("Output file not found"), QDateTime::currentMSecsSinceEpoch()});
                    } else if (QFileInfo(outputPath).size() == 0) {
                        failedFrames.append(FailedFrameInfo{jobId, -1, QStringLiteral("Output file is empty"), QDateTime::currentMSecsSinceEpoch()});
                    }
                }
                
                return failedFrames;
            }
            
        private:
            QString generateFramePath(const QString& basePath, int frameNumber) {
                QFileInfo fi(basePath);
                QString dir = fi.path();
                QString baseName = fi.completeBaseName();
                QString ext = fi.suffix();
                
                // アンダースコア区切りのフレーム番号を想定 (e.g., render_0001.png)
                QRegularExpression re("_(\\d+)$");
                QRegularExpressionMatch match = re.match(baseName);
                if (match.hasMatch()) {
                    baseName = baseName.left(match.capturedStart());
                }
                
                return QDir(dir).filePath(QString("%1_%2.%3").arg(baseName).arg(frameNumber, 4, 10, QChar('0')).arg(ext));
            }
        };
        void addJob(const ArtifactRenderJob& job) {
            jobs.append(job);
            if (jobAdded) jobAdded(jobs.size() - 1);
        }

        void removeJob(int index) {
            if (index >= 0 && index < jobs.size()) {
                jobs.removeAt(index);
                if (jobRemoved) jobRemoved(index);
            }
        }

        void removeAllJobs() {
            jobs.clear();
            if (allJobsRemoved) allJobsRemoved();
        }

        void moveJob(int fromIndex, int toIndex) {
            if (fromIndex >= 0 && fromIndex < jobs.size() && toIndex >= 0 && toIndex < jobs.size()) {
                ArtifactRenderJob job = jobs.takeAt(fromIndex);
                jobs.insert(toIndex, job);
                if (queueReordered) queueReordered(fromIndex, toIndex);
            }
        }

        void startRendering(int index) {
            if (index >= 0 && index < jobs.size()) {
                jobs[index].status = ArtifactRenderJob::Status::Rendering;
                if (jobStatusChanged) jobStatusChanged(index, jobs[index].status);
            }
        }

        void pauseRendering(int index) {
            if (index >= 0 && index < jobs.size()) {
                jobs[index].status = ArtifactRenderJob::Status::Pending;
                if (jobStatusChanged) jobStatusChanged(index, jobs[index].status);
            }
        }

        void cancelRendering(int index) {
            if (index >= 0 && index < jobs.size()) {
                jobs[index].status = ArtifactRenderJob::Status::Canceled;
                if (jobStatusChanged) jobStatusChanged(index, jobs[index].status);
            }
        }

        void startAllJobs() {
            for (int i = 0; i < jobs.size(); ++i) {
                if (jobs[i].status == ArtifactRenderJob::Status::Pending) {
                    jobs[i].status = ArtifactRenderJob::Status::Rendering;
                    if (jobStatusChanged) jobStatusChanged(i, jobs[i].status);
                }
            }
        }

        void setJobStatus(int index, ArtifactRenderJob::Status status) {
            if (index < 0 || index >= jobs.size()) {
                return;
            }
            jobs[index].status = status;
            if (jobStatusChanged) {
                jobStatusChanged(index, jobs[index].status);
            }
        }

        void setJobProgress(int index, int progress) {
            if (index < 0 || index >= jobs.size()) return;
            jobs[index].progress = std::clamp(progress, 0, 100);
            if (jobProgressChanged) jobProgressChanged(index, jobs[index].progress);
            if (jobUpdated) jobUpdated(index);
        }

        void setJobCompleted(int index) {
            if (index < 0 || index >= jobs.size()) return;
            jobs[index].progress = 100;  // progress を先に設定（100% 表示を確実にする）
            jobs[index].status = ArtifactRenderJob::Status::Completed;
            if (jobProgressChanged) jobProgressChanged(index, jobs[index].progress);
            if (jobStatusChanged) jobStatusChanged(index, jobs[index].status);
            if (jobUpdated) jobUpdated(index);
        }

        void setJobFailed(int index, const QString& message) {
            if (index < 0 || index >= jobs.size()) return;
            jobs[index].status = ArtifactRenderJob::Status::Failed;
            jobs[index].progress = 0;  // 失敗時に進捗率をリセット（再実行時に分かりやすくする）
            jobs[index].errorMessage = message;
            if (jobStatusChanged) jobStatusChanged(index, jobs[index].status);
            if (jobProgressChanged) jobProgressChanged(index, jobs[index].progress);  // 進捗更新も発火
            if (jobUpdated) jobUpdated(index);
        }

        void setJobOutputPath(int index, const QString& outputPath) {
            if (index < 0 || index >= jobs.size()) return;
            jobs[index].outputPath = outputPath;
            if (jobUpdated) jobUpdated(index);
        }

        bool jobFrameRangeAt(int index, int* startFrame, int* endFrame) const {
            if (index < 0 || index >= jobs.size()) {
                return false;
            }
            if (startFrame) *startFrame = jobs[index].startFrame;
            if (endFrame) *endFrame = jobs[index].endFrame;
            return true;
        }

        void setJobFrameRange(int index, int startFrame, int endFrame) {
            if (index < 0 || index >= jobs.size()) return;
            const int clampedStart = std::max(0, startFrame);
            const int clampedEnd = std::max(clampedStart, endFrame);
            jobs[index].startFrame = clampedStart;
            jobs[index].endFrame = clampedEnd;
            if (jobUpdated) jobUpdated(index);
        }

        bool jobOutputSettingsAt(
            int index,
            QString* outputFormat,
            QString* codec,
            QString* codecProfile,
            int* width,
            int* height,
            double* fps,
            int* bitrateKbps) const
        {
            if (index < 0 || index >= jobs.size()) {
                return false;
            }
            const auto& job = jobs[index];
            if (outputFormat) *outputFormat = job.outputFormat;
            if (codec) *codec = job.codec;
            if (codecProfile) *codecProfile = job.codecProfile;
            if (width) *width = job.resolutionWidth;
            if (height) *height = job.resolutionHeight;
            if (fps) *fps = job.frameRate;
            if (bitrateKbps) *bitrateKbps = job.bitrate;
            return true;
        }

        void setJobOutputSettings(
            int index,
            const QString& outputFormat,
            const QString& codec,
            const QString& codecProfile,
            int width,
            int height,
            double fps,
            int bitrateKbps)
        {
            if (index < 0 || index >= jobs.size()) return;
            auto& job = jobs[index];
            const QString fmt = outputFormat.trimmed();
            const QString cdc = codec.trimmed();
            ArtifactRenderJob normalizedJob;
            normalizedJob.outputFormat = fmt.isEmpty() ? QStringLiteral("MP4") : fmt;
            job.outputFormat = deriveContainerFromJob(normalizedJob);
            job.codec = cdc.isEmpty() ? QStringLiteral("H.264") : cdc;
            job.codecProfile = codecProfile.trimmed().isEmpty()
                ? (normalizeCodecName(job.codec) == QStringLiteral("prores") ? QStringLiteral("hq") : QString())
                : codecProfile.trimmed();
            job.resolutionWidth = std::clamp(width, 16, 16384);
            job.resolutionHeight = std::clamp(height, 16, 16384);
            job.frameRate = std::clamp(fps, 1.0, 240.0);
            job.bitrate = std::clamp(bitrateKbps, 128, 200000);
            if (jobUpdated) jobUpdated(index);
        }

        void updateJob(int index, const ArtifactRenderJob& job) {
            if (index < 0 || index >= jobs.size()) return;
            jobs[index] = job;
            if (jobUpdated) jobUpdated(index);
        }

        void pauseAllJobs() {
            for (int i = 0; i < jobs.size(); ++i) {
                if (jobs[i].status == ArtifactRenderJob::Status::Rendering) {
                    jobs[i].status = ArtifactRenderJob::Status::Pending;
                    if (jobStatusChanged) jobStatusChanged(i, jobs[i].status);
                }
            }
        }

        void cancelAllJobs() {
            for (int i = 0; i < jobs.size(); ++i) {
                if (jobs[i].status == ArtifactRenderJob::Status::Rendering ||
                    jobs[i].status == ArtifactRenderJob::Status::Pending) {
                    jobs[i].status = ArtifactRenderJob::Status::Canceled;
                    if (jobStatusChanged) jobStatusChanged(i, jobs[i].status);
                }
            }
        }

        ArtifactRenderJob getJob(int index) const {
            return index >= 0 && index < jobs.size() ? jobs[index] : ArtifactRenderJob();
        }

        QString jobCompositionNameAt(int index) const {
            if (index < 0 || index >= jobs.size()) {
                return {};
            }
            return jobs[index].compositionName;
        }

        QString jobStatusAt(int index) const {
            if (index < 0 || index >= jobs.size()) {
                return QStringLiteral("Pending");
            }
            switch (jobs[index].status) {
            case ArtifactRenderJob::Status::Pending: return QStringLiteral("Pending");
            case ArtifactRenderJob::Status::Rendering: return QStringLiteral("Rendering");
            case ArtifactRenderJob::Status::Completed: return QStringLiteral("Completed");
            case ArtifactRenderJob::Status::Failed: return QStringLiteral("Failed");
            case ArtifactRenderJob::Status::Canceled: return QStringLiteral("Canceled");
            default: return QStringLiteral("Pending");
            }
        }

        QString jobOutputPathAt(int index) const {
            if (index < 0 || index >= jobs.size()) {
                return {};
            }
            return jobs[index].outputPath;
        }

        int jobProgressAt(int index) const {
            if (index < 0 || index >= jobs.size()) {
                return 0;
            }
            return jobs[index].progress;
        }

        QString jobErrorMessageAt(int index) const {
            if (index < 0 || index >= jobs.size()) {
                return {};
            }
            return jobs[index].errorMessage;
        }

        bool jobOverlayTransformAt(int index, float* offsetX, float* offsetY, float* scale, float* rotationDeg) const {
            if (index < 0 || index >= jobs.size()) {
                return false;
            }
            const auto& job = jobs[index];
            if (offsetX) *offsetX = job.overlayOffsetX;
            if (offsetY) *offsetY = job.overlayOffsetY;
            if (scale) *scale = job.overlayScale;
            if (rotationDeg) *rotationDeg = job.overlayRotationDeg;
            return true;
        }

        void setJobOverlayTransform(int index, float offsetX, float offsetY, float scale, float rotationDeg) {
            if (index < 0 || index >= jobs.size()) {
                return;
            }
            auto& job = jobs[index];
            job.overlayOffsetX = offsetX;
            job.overlayOffsetY = offsetY;
            job.overlayScale = std::clamp(scale, 0.05f, 8.0f);
            job.overlayRotationDeg = rotationDeg;
            if (jobUpdated) jobUpdated(index);
        }

        bool resetJobForRerun(int index) {
            if (index < 0 || index >= jobs.size()) {
                return false;
            }
            auto& job = jobs[index];
            const bool wasRenderable = (job.status == ArtifactRenderJob::Status::Completed
                || job.status == ArtifactRenderJob::Status::Failed
                || job.status == ArtifactRenderJob::Status::Canceled);
            
            // 既存ファイルの上書き防止のためのリネーム処理 (_v1, _v2 ...)
            QFileInfo fi(job.outputPath);
            if (fi.exists() && job.status == ArtifactRenderJob::Status::Completed) {
                QString basePath = fi.path() + "/" + fi.completeBaseName();
                QString ext = fi.suffix();
                if (!ext.isEmpty()) ext = "." + ext;
                
                // すでに _v\d+ が付いている場合はそこからインクリメントを試みる
                QRegularExpression re("_v(\\d+)$");
                QRegularExpressionMatch match = re.match(basePath);
                int version = 1;
                if (match.hasMatch()) {
                    version = match.captured(1).toInt() + 1;
                    basePath = basePath.left(match.capturedStart());
                }

                QString newPath;
                do {
                    newPath = QString("%1_v%2%3").arg(basePath).arg(version).arg(ext);
                    version++;
                } while (QFile::exists(newPath));
                job.outputPath = newPath;
            }

            job.status = ArtifactRenderJob::Status::Pending;
            job.progress = 0;
            job.errorMessage.clear();
            if (jobStatusChanged) jobStatusChanged(index, job.status);
            if (jobProgressChanged) jobProgressChanged(index, job.progress);
            if (jobUpdated) jobUpdated(index);
            return wasRenderable;
        }

        int resetCompletedAndFailedJobsForRerun() {
            int changed = 0;
            for (int i = 0; i < jobs.size(); ++i) {
                const auto status = jobs[i].status;
                if (status == ArtifactRenderJob::Status::Completed
                    || status == ArtifactRenderJob::Status::Failed) {
                    resetJobForRerun(i);
                    ++changed;
                }
            }
            return changed;
        }

        int countJobsForComposition(const ArtifactCore::CompositionID& compositionId) const {
            int count = 0;
            for (const auto& job : jobs) {
                if (job.compositionId == compositionId) {
                    ++count;
                }
            }
            return count;
        }

        bool hasJobForComposition(const ArtifactCore::CompositionID& compositionId) const {
            return countJobsForComposition(compositionId) > 0;
        }

        ArtifactCore::CompositionID jobCompositionIdAt(int index) const {
            if (index < 0 || index >= jobs.size()) {
                return ArtifactCore::CompositionID::Nil();
            }
            return jobs[index].compositionId;
        }

        void removeJobsForComposition(const ArtifactCore::CompositionID& compositionId) {
            for (int i = jobs.size() - 1; i >= 0; --i) {
                if (jobs[i].compositionId == compositionId) {
                    jobs.removeAt(i);
                    if (jobRemoved) {
                        jobRemoved(i);
                    }
                }
            }
            if (jobs.isEmpty() && allJobsRemoved) {
                allJobsRemoved();
            }
        }

        int jobCount() const {
            return jobs.size();
        }

        int getTotalProgress() const {
            if (jobs.isEmpty()) return 0;

            int totalProgress = 0;
            int activeJobs = 0;

            for (const ArtifactRenderJob& job : jobs) {
                if (job.status == ArtifactRenderJob::Status::Rendering || job.status == ArtifactRenderJob::Status::Completed) {
                    totalProgress += job.progress;
                    activeJobs++;
                }
            }

            return activeJobs > 0 ? totalProgress / activeJobs : 0;
        }

        // シグナル
        std::function<void(int)> jobAdded;
        std::function<void(int)> jobRemoved;
        std::function<void(int)> jobUpdated;
        std::function<void(int, ArtifactRenderJob::Status)> jobStatusChanged;
        std::function<void(int, int)> jobStatusChangedForUi;
        std::function<void(int, int)> jobProgressChanged;
        std::function<void()> allJobsCompleted;
        std::function<void()> allJobsRemoved;
        std::function<void(int, int)> queueReordered;

    private:
        QList<ArtifactRenderJob> jobs;
    };

    class ArtifactRenderQueueService::Impl {
    public:
        Impl() {
            // ArtifactCoreのレンダリングキューマネージャにコールバックを登録
            auto& coreQueueManager = ArtifactCore::RendererQueueManager::instance();
            coreQueueManager.setRenderFrameFunc([this](const ArtifactCore::Id& compId, int frame, const QString& path) {
                // 順序制御を行いつつエンコーダに送る
                {
                    std::lock_guard<std::mutex> lock(encoderMutex);
                    // 本来はここでレンダリングされたImageを取得し、バッファに格納する
                    // frameBuffer[frame] = renderedImage; 

                    while (frameBuffer.count(nextFrameToEncode)) {
                        /* FFmpegEncoder disabled
                        if (ffmpegEncoder) {
                            ffmpegEncoder->addImage(frameBuffer[nextFrameToEncode]);
                        }
                        */
                        frameBuffer.erase(nextFrameToEncode);
                        nextFrameToEncode++;
                    }
                }
            });

            queueManager.jobAdded = [this](int index) {
                handleJobAdded(index);
            };

            queueManager.jobRemoved = [this](int index) {
                handleJobRemoved(index);
            };

            queueManager.jobUpdated = [this](int index) {
                handleJobUpdated(index);
            };

            queueManager.jobStatusChanged = [this](int index, ArtifactRenderJob::Status status) {
                handleJobStatusChanged(index, status);
            };

            queueManager.jobProgressChanged = [this](int index, int progress) {
                handleJobProgressChanged(index, progress);
            };

            queueManager.queueReordered = [this](int fromIndex, int toIndex) {
                if (queueReordered) {
                    queueReordered(fromIndex, toIndex);
                }
            };
        }

        ~Impl() {
            shutdownRequested_.store(true, std::memory_order_release);
            if (workerThread_.joinable()) {
                workerThread_.join();
            }
        }

        ArtifactRenderQueueManager queueManager;
        void* ffmpegEncoder = nullptr; // Temporarily void* to bypass build error
        std::map<int, ArtifactCore::ImageF32x4_RGBA> frameBuffer;
        int nextFrameToEncode = 0;
        std::mutex encoderMutex;
        std::mutex queueMutex;
        std::atomic<bool> isRendering_{false};
        std::atomic<bool> shutdownRequested_{false};
        std::thread workerThread_;

        QString resolveDummyOutputPath(const ArtifactRenderJob& job, int index) const {
            QString target = job.outputPath.trimmed();
            const QString safeName = job.compositionName.trimmed().isEmpty()
                ? QStringLiteral("Composition")
                : job.compositionName.trimmed();
            const QString stamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss"));
            const QString fileName = QStringLiteral("%1_%2_%3.png").arg(safeName).arg(index + 1).arg(stamp);

            if (target.isEmpty()) {
                return QDir(QDir::homePath() + QStringLiteral("/Desktop")).filePath(fileName);
            }

            QFileInfo info(target);
            if (info.isDir()) {
                return QDir(info.absoluteFilePath()).filePath(fileName);
            }

            QString dir = info.absolutePath();
            if (dir.isEmpty()) {
                dir = QDir::homePath() + QStringLiteral("/Desktop");
            }
            QString base = info.completeBaseName();
            if (base.isEmpty()) {
                base = QStringLiteral("render");
            }
            return QDir(dir).filePath(base + QStringLiteral(".png"));
        }

        QImage renderSingleFrameDummy(const ArtifactRenderJob& job, int frameNumber) const {
            const int width = std::max(16, job.resolutionWidth);
            const int height = std::max(16, job.resolutionHeight);
            QImage background(width, height, QImage::Format_ARGB32_Premultiplied);
            background.fill(QColor(24, 26, 30, 255));
            {
                QPainter painter(&background);
                painter.setRenderHint(QPainter::Antialiasing, true);
                painter.fillRect(QRect(0, 0, width, height), QColor(32, 36, 42));
            }

            QImage foreground(width, height, QImage::Format_ARGB32_Premultiplied);
            foreground.fill(Qt::transparent);
            {
                QPainter painter(&foreground);
                painter.setRenderHint(QPainter::Antialiasing, true);
                painter.fillRect(QRect(width / 16, height / 8, width * 7 / 8, height * 3 / 4), QColor(64, 84, 132, 220));
                painter.setPen(QColor(230, 230, 230));
                painter.drawText(QRect(0, 0, width, height / 3),
                                 Qt::AlignCenter,
                                 QStringLiteral("Artifact Dummy Render"));
                painter.drawText(QRect(0, height / 3, width, height / 3),
                                 Qt::AlignCenter,
                                 QStringLiteral("Comp: %1").arg(job.compositionName));
                painter.drawText(QRect(0, (height * 2) / 3, width, height / 3),
                                 Qt::AlignCenter,
                                 QStringLiteral("Frame: %1").arg(frameNumber));
            }

            QImage overlay(width, height, QImage::Format_ARGB32_Premultiplied);
            overlay.fill(Qt::transparent);
            {
                QPainter painter(&overlay);
                painter.setRenderHint(QPainter::Antialiasing, true);
                painter.setPen(Qt::NoPen);
                painter.setBrush(QColor(255, 190, 90, 180));
                // Animate the ellipse based on frame number
                const int animOffset = (frameNumber * 5) % (width / 2);
                painter.drawEllipse(QRect(width * 3 / 5 - animOffset, height / 8, width / 4, height / 4));
            }

            SoftwareRender::CompositeRequest request;
            request.background = background;
            request.foreground = foreground;
            request.overlay = overlay;
            request.outputSize = QSize(width, height);
            request.backend = SoftwareRender::CompositeBackend::OpenCV;
            request.blendMode = ArtifactCore::BlendMode::Screen;
            request.cvEffect = SoftwareRender::CvEffectMode::None;
            request.overlayOpacity = 0.70f;
            request.overlayOffset = QPointF(job.overlayOffsetX, job.overlayOffsetY);
            request.overlayScale = job.overlayScale;
            request.overlayRotationDeg = job.overlayRotationDeg;
            request.useForeground = true;

            return SoftwareRender::compose(request);
        }

        QColor toQColor(const ArtifactCore::FloatColor& color, const float alphaScale = 1.0f) const
        {
            return QColor::fromRgbF(
                std::clamp(color.r(), 0.0f, 1.0f),
                std::clamp(color.g(), 0.0f, 1.0f),
                std::clamp(color.b(), 0.0f, 1.0f),
                std::clamp(color.a() * alphaScale, 0.0f, 1.0f));
        }

        QPainter::CompositionMode compositionModeForLayer(const ArtifactAbstractLayerPtr& layer) const
        {
            const auto blend = ArtifactCore::toBlendMode(layer ? layer->layerBlendType() : LAYER_BLEND_TYPE::BLEND_NORMAL);
            switch (blend) {
            case ArtifactCore::BlendMode::Subtract: return QPainter::CompositionMode_Difference;
            case ArtifactCore::BlendMode::Add: return QPainter::CompositionMode_Plus;
            case ArtifactCore::BlendMode::Multiply: return QPainter::CompositionMode_Multiply;
            case ArtifactCore::BlendMode::Screen: return QPainter::CompositionMode_Screen;
            case ArtifactCore::BlendMode::Overlay: return QPainter::CompositionMode_Overlay;
            case ArtifactCore::BlendMode::Darken: return QPainter::CompositionMode_Darken;
            case ArtifactCore::BlendMode::Lighten: return QPainter::CompositionMode_Lighten;
            case ArtifactCore::BlendMode::ColorDodge: return QPainter::CompositionMode_ColorDodge;
            case ArtifactCore::BlendMode::ColorBurn: return QPainter::CompositionMode_ColorBurn;
            case ArtifactCore::BlendMode::HardLight: return QPainter::CompositionMode_HardLight;
            case ArtifactCore::BlendMode::SoftLight: return QPainter::CompositionMode_SoftLight;
            case ArtifactCore::BlendMode::Difference: return QPainter::CompositionMode_Difference;
            case ArtifactCore::BlendMode::Exclusion: return QPainter::CompositionMode_Exclusion;
            case ArtifactCore::BlendMode::Hue:
            case ArtifactCore::BlendMode::Saturation:
            case ArtifactCore::BlendMode::Color:
            case ArtifactCore::BlendMode::Luminosity:
                return QPainter::CompositionMode_SourceOver;
            case ArtifactCore::BlendMode::Normal:
            default:
                return QPainter::CompositionMode_SourceOver;
            }
        }

        QSize safeLayerSize(const ArtifactAbstractLayerPtr& layer, const QSize& fallback = QSize(320, 180)) const
        {
            if (!layer) {
                return fallback;
            }
            const auto source = layer->sourceSize();
            return QSize(std::max(16, source.width), std::max(16, source.height));
        }

        QImage renderTextLayerSurface(const std::shared_ptr<ArtifactTextLayer>& textLayer, const QSize& size) const
        {
            QImage image(size, QImage::Format_ARGB32_Premultiplied);
            image.fill(Qt::transparent);
            if (!textLayer) {
                return image;
            }

            QPainter painter(&image);
            painter.setRenderHint(QPainter::Antialiasing, true);
            QFont font(textLayer->fontFamily().toQString());
            font.setPointSizeF(std::max(6.0f, textLayer->fontSize()));
            font.setBold(textLayer->isBold());
            font.setItalic(textLayer->isItalic());
            painter.setFont(font);
            painter.setPen(QColor::fromRgbF(
                textLayer->textColor().r(),
                textLayer->textColor().g(),
                textLayer->textColor().b(),
                textLayer->textColor().a()));

            QTextOption option;
            option.setWrapMode(QTextOption::WordWrap);
            option.setAlignment(Qt::AlignLeft | Qt::AlignTop);
            painter.drawText(QRectF(0.0, 0.0, size.width(), size.height()), textLayer->text().toQString(), option);
            return image;
        }

        QImage renderVideoLayerSurface(const std::shared_ptr<ArtifactVideoLayer>& videoLayer, const QSize& size) const
        {
            if (!videoLayer) {
                return {};
            }
            QImage frame = videoLayer->currentFrameToQImage();
            if (!frame.isNull()) {
                return frame.convertToFormat(QImage::Format_ARGB32_Premultiplied);
            }

            QImage placeholder(size, QImage::Format_ARGB32_Premultiplied);
            placeholder.fill(QColor(28, 32, 38));
            QPainter p(&placeholder);
            p.setPen(QColor(220, 226, 234));
            p.drawText(placeholder.rect(), Qt::AlignCenter, QStringLiteral("Video frame unavailable"));
            return placeholder;
        }

        QImage renderLayerSurface(const ArtifactAbstractLayerPtr& layer) const
        {
            if (!layer) {
                return {};
            }

            const QSize layerSize = safeLayerSize(layer);
            if (const auto imageLayer = std::dynamic_pointer_cast<ArtifactImageLayer>(layer)) {
                const QImage image = imageLayer->toQImage();
                if (!image.isNull()) {
                    return image.convertToFormat(QImage::Format_ARGB32_Premultiplied);
                }
            }

            if (const auto svgLayer = std::dynamic_pointer_cast<ArtifactSvgLayer>(layer)) {
                const QImage image = svgLayer->toQImage();
                if (!image.isNull()) {
                    return image.convertToFormat(QImage::Format_ARGB32_Premultiplied);
                }
            }

            if (const auto solidLayer = std::dynamic_pointer_cast<ArtifactSolidImageLayer>(layer)) {
                QImage image(layerSize, QImage::Format_ARGB32_Premultiplied);
                image.fill(toQColor(solidLayer->color()));
                return image;
            }

            if (const auto solid2DLayer = std::dynamic_pointer_cast<ArtifactSolid2DLayer>(layer)) {
                QImage image(layerSize, QImage::Format_ARGB32_Premultiplied);
                image.fill(toQColor(solid2DLayer->color()));
                return image;
            }

            if (const auto textLayer = std::dynamic_pointer_cast<ArtifactTextLayer>(layer)) {
                return renderTextLayerSurface(textLayer, layerSize);
            }

            if (const auto videoLayer = std::dynamic_pointer_cast<ArtifactVideoLayer>(layer)) {
                return renderVideoLayerSurface(videoLayer, layerSize);
            }

            QImage fallback = layer->getThumbnail(layerSize.width(), layerSize.height());
            if (!fallback.isNull()) {
                return fallback.convertToFormat(QImage::Format_ARGB32_Premultiplied);
            }

            return {};
        }

        ArtifactCompositionPtr resolveComposition(const ArtifactRenderJob& job) const
        {
            if (job.compositionId == ArtifactCore::CompositionID::Nil()) {
                return {};
            }
            const auto found = ArtifactProjectManager::getInstance().findComposition(job.compositionId);
            if (!found.success) {
                return {};
            }
            return found.ptr.lock();
        }

        QImage renderSingleFrameComposition(const ArtifactRenderJob& job, const ArtifactCompositionPtr& composition, int frameNumber) const
        {
            if (!composition) {
                return renderSingleFrameDummy(job, frameNumber);
            }

            composition->goToFrame(frameNumber);
            const QSize compSize = composition->settings().compositionSize();
            const int compW = std::max(16, compSize.width());
            const int compH = std::max(16, compSize.height());
            QImage canvas(QSize(compW, compH), QImage::Format_ARGB32_Premultiplied);
            canvas.fill(toQColor(composition->backgroundColor()));

            QPainter painter(&canvas);
            painter.setRenderHint(QPainter::Antialiasing, true);
            painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

            const ArtifactCore::FramePosition currentPos(frameNumber);
            const auto layers = composition->allLayer();
            for (const auto& layer : layers) {
                if (!layer || !layer->isVisible() || !layer->isActiveAt(currentPos)) {
                    continue;
                }

                const QImage surface = renderLayerSurface(layer);
                if (surface.isNull()) {
                    continue;
                }

                const QSize layerSize = safeLayerSize(layer, surface.size());
                const qreal opacity = std::clamp(static_cast<qreal>(layer->opacity()), 0.0, 1.0);
                painter.save();
                painter.setOpacity(opacity);
                painter.setCompositionMode(compositionModeForLayer(layer));
                painter.setTransform(layer->getGlobalTransform(), true);
                painter.drawImage(
                    QRectF(0.0, 0.0, layerSize.width(), layerSize.height()),
                    surface,
                    QRectF(0.0, 0.0, surface.width(), surface.height()));
                painter.restore();
            }

            const int outW = std::max(16, job.resolutionWidth);
            const int outH = std::max(16, job.resolutionHeight);
            if (canvas.width() != outW || canvas.height() != outH) {
                return canvas.scaled(outW, outH, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
            }
            return canvas;
        }

        bool renderSingleFrameGPU(const ArtifactRenderJob& job, int index, QString* outputPath, QString* errorMessage) {
            const int width  = std::max(16, job.resolutionWidth);
            const int height = std::max(16, job.resolutionHeight);

            ArtifactIRenderer renderer;
            renderer.initializeHeadless(width, height);
            renderer.clear();
            renderer.flush();

            QImage frame = renderer.readbackToImage();
            renderer.destroy();

            if (frame.isNull()) {
                if (errorMessage) *errorMessage = QStringLiteral("GPU readback failed");
                return false;
            }

            const QString outPath = resolveDummyOutputPath(job, index);
            if (outputPath) *outputPath = outPath;

            QDir outDir(QFileInfo(outPath).absolutePath());
            if (!outDir.exists() && !outDir.mkpath(QStringLiteral("."))) {
                if (errorMessage) *errorMessage = QStringLiteral("Failed to create output directory: %1").arg(outDir.absolutePath());
                return false;
            }

            if (!frame.save(outPath, "PNG")) {
                if (errorMessage) *errorMessage = QStringLiteral("Failed to save image: %1").arg(outPath);
                return false;
            }
            return true;
        }

        void syncCoreQueueModel() {
            auto& coreQueue = ArtifactCore::RendererQueueManager::instance();
            coreQueue.clearRenderQueue();
            const int count = queueManager.jobCount();
            for (int i = 0; i < count; ++i) {
                const auto job = queueManager.getJob(i);
                coreQueue.addJob(job.compositionId, job.compositionName);
            }
        }

        void handleJobAdded(int index) {
            Q_EMIT owner_->jobAdded(index);
            if (jobAdded) jobAdded(index);
        }

        void handleJobRemoved(int index) {
            Q_EMIT owner_->jobRemoved(index);
            if (jobRemoved) jobRemoved(index);
        }

        void handleJobUpdated(int index) {
            Q_EMIT owner_->jobUpdated(index);
            if (jobUpdated) jobUpdated(index);
        }

        void handleJobStatusChanged(int index, ArtifactRenderJob::Status status) {
            Q_EMIT owner_->jobStatusChanged(index, static_cast<int>(status));
            if (jobStatusChangedForUi) jobStatusChangedForUi(index, static_cast<int>(status));
            if (jobStatusChanged) jobStatusChanged(index, status);
        }

        void handleJobProgressChanged(int index, int progress) {
            // 内部コールバックのみ発火（2 重発火防止）
            // Q_EMIT owner_->jobProgressChanged(index, progress);  // 削除
            if (jobProgressChanged) jobProgressChanged(index, progress);
        }

        ArtifactRenderQueueService* owner_ = nullptr;
        // シグナル
        std::function<void(int)> jobAdded;
        std::function<void(int)> jobRemoved;
        std::function<void(int)> jobUpdated;
        std::function<void(int, int)> jobStatusChangedForUi;
        std::function<void(int, ArtifactRenderJob::Status)> jobStatusChanged;
        std::function<void(int, int)> jobProgressChanged;
        std::function<void()> allJobsCompleted;
        std::function<void()> allJobsRemoved;
        std::function<void(int, int)> queueReordered;
    };

    W_OBJECT_IMPL(ArtifactRenderQueueService)

    ArtifactRenderQueueService::ArtifactRenderQueueService(QObject* parent /*= nullptr*/)
        : QObject(parent), impl_(new Impl) {
        impl_->owner_ = this;
    }

    ArtifactRenderQueueService::~ArtifactRenderQueueService() {
        delete impl_;
    }

    ArtifactRenderQueueService* ArtifactRenderQueueService::instance()
    {
        static ArtifactRenderQueueService service;
        return &service;
    }

    void ArtifactRenderQueueService::addRenderQueue() {
        ArtifactCompositionPtr currentComposition;
        if (auto* projectService = ArtifactProjectService::instance()) {
            currentComposition = projectService->currentComposition().lock();
        }
        if (!currentComposition) {
            currentComposition = ArtifactProjectManager::getInstance().currentComposition();
        }
        if (currentComposition) {
            const auto compositionName = currentComposition->settings().compositionName().toQString();
            addRenderQueueForComposition(currentComposition->id(), compositionName);
            return;
        }

        ArtifactRenderJob job;
        job.compositionId = ArtifactCore::CompositionID::Nil();
        job.compositionName = "New Render Job";
        job.status = ArtifactRenderJob::Status::Pending;
        job.outputPath = QDir::homePath() + "/Desktop/output.mp4";
        job.outputFormat = "MP4";
        job.codec = "H.264";
        job.codecProfile.clear();
        job.resolutionWidth = 1920;
        job.resolutionHeight = 1080;
        job.frameRate = 30.0;
        job.bitrate = 8000;
        job.startFrame = 0;
        job.endFrame = 100;

        impl_->queueManager.addJob(job);
        impl_->syncCoreQueueModel();
    }

    void ArtifactRenderQueueService::addRenderQueueForComposition(const ArtifactCore::CompositionID& compositionId, const QString& compositionName)
    {
        ArtifactRenderJob job;
        job.compositionId = compositionId;
        job.compositionName = compositionName.trimmed().isEmpty() ? QStringLiteral("Composition") : compositionName.trimmed();
        job.status = ArtifactRenderJob::Status::Pending;
        job.outputPath = QDir::homePath() + "/Desktop/output.mp4";
        job.outputFormat = "MP4";
        job.codec = "H.264";
        job.codecProfile.clear();
        job.resolutionWidth = 1920;
        job.resolutionHeight = 1080;
        job.frameRate = 30.0;
        job.bitrate = 8000;
        job.startFrame = 0;
        job.endFrame = 100;

        const auto found = ArtifactProjectManager::getInstance().findComposition(compositionId);
        if (found.success) {
            if (const auto comp = found.ptr.lock()) {
                const auto totalRange = comp->frameRange();
                const auto workAreaRange = comp->workAreaRange();
                job.startFrame = static_cast<int>(std::max<int64_t>(0, workAreaRange.start()));
                job.endFrame = static_cast<int>(std::max<int64_t>(job.startFrame + 1, workAreaRange.end()));
                job.frameRate = comp->frameRate().framerate();
                const QSize size = comp->settings().compositionSize();
                if (size.width() > 0 && size.height() > 0) {
                    job.resolutionWidth = size.width();
                    job.resolutionHeight = size.height();
                }
                if (!totalRange.isValid() || totalRange.duration() <= 0) {
                    job.startFrame = 0;
                    job.endFrame = 100;
                }
            }
        }

        impl_->queueManager.addJob(job);
        impl_->syncCoreQueueModel();
    }

    void ArtifactRenderQueueService::addRenderQueueWithPreset(
        const ArtifactCore::CompositionID& compositionId,
        const QString& compositionName,
        const QString& presetId)
    {
        ArtifactRenderJob job;
        job.compositionId = compositionId;
        job.compositionName = compositionName.trimmed().isEmpty() ? QStringLiteral("Composition") : compositionName.trimmed();
        job.status = ArtifactRenderJob::Status::Pending;
        job.outputPath = QDir::homePath() + "/Desktop/output";
        job.outputFormat = "MP4";
        job.codec = "H.264";
        job.codecProfile.clear();
        job.resolutionWidth = 1920;
        job.resolutionHeight = 1080;
        job.frameRate = 30.0;
        job.bitrate = 8000;
        job.startFrame = 0;
        job.endFrame = 100;

        // プリセットを適用
        const auto* preset = ArtifactRenderFormatPresetManager::instance().findPresetById(presetId);
        if (preset) {
            job.outputFormat = preset->container;
            job.codec = preset->codec;
            job.codecProfile = preset->codecProfile;
            if (preset->isImageSequence) {
                job.outputPath = QDir::homePath() + "/Desktop/output_sequence";
            } else if (preset->isAnimatedImage) {
                const QString suffix = preset->container.trimmed().isEmpty()
                    ? QStringLiteral("gif")
                    : preset->container.trimmed();
                job.outputPath = QDir::homePath() + "/Desktop/output." + suffix;
            }
        }

        const auto found = ArtifactProjectManager::getInstance().findComposition(compositionId);
        if (found.success) {
            if (const auto comp = found.ptr.lock()) {
                const auto totalRange = comp->frameRange();
                const auto workAreaRange = comp->workAreaRange();
                job.startFrame = static_cast<int>(std::max<int64_t>(0, workAreaRange.start()));
                job.endFrame = static_cast<int>(std::max<int64_t>(job.startFrame + 1, workAreaRange.end()));
                job.frameRate = comp->frameRate().framerate();
                const QSize size = comp->settings().compositionSize();
                if (size.width() > 0 && size.height() > 0) {
                    job.resolutionWidth = size.width();
                    job.resolutionHeight = size.height();
                }
                if (!totalRange.isValid() || totalRange.duration() <= 0) {
                    job.startFrame = 0;
                    job.endFrame = 100;
                }
            }
        }

        impl_->queueManager.addJob(job);
        impl_->syncCoreQueueModel();
    }

    void ArtifactRenderQueueService::addMultipleRenderQueuesForComposition(
        const ArtifactCore::CompositionID& compositionId,
        const QString& compositionName,
        const QVector<QString>& presetIds)
    {
        for (const auto& presetId : presetIds) {
            addRenderQueueWithPreset(compositionId, compositionName, presetId);
        }
    }

    void ArtifactRenderQueueService::removeRenderQueue() {
        const int count = impl_->queueManager.jobCount();
        if (count <= 0) {
            return;
        }
        impl_->queueManager.removeJob(count - 1);
        impl_->syncCoreQueueModel();
    }

    void ArtifactRenderQueueService::removeRenderQueueAt(int index) {
        impl_->queueManager.removeJob(index);
        impl_->syncCoreQueueModel();
    }

    void ArtifactRenderQueueService::duplicateRenderQueueAt(int index)
    {
        const int count = impl_->queueManager.jobCount();
        if (index < 0 || index >= count) {
            return;
        }
        const ArtifactRenderJob source = impl_->queueManager.getJob(index);
        ArtifactRenderJob copy = source;
        copy.status = ArtifactRenderJob::Status::Pending;
        copy.progress = 0;
        copy.errorMessage.clear();
        if (!copy.compositionName.trimmed().isEmpty()) {
            copy.compositionName += QStringLiteral(" Copy");
        }

        impl_->queueManager.addJob(copy);
        // 複製ジョブの進捗率 0% を即座に UI に反映
        const int newIndex = impl_->queueManager.jobCount() - 1;
        if (impl_->jobProgressChanged) {
            impl_->jobProgressChanged(newIndex, 0);
        }
        impl_->syncCoreQueueModel();
    }

    void ArtifactRenderQueueService::moveRenderQueue(int fromIndex, int toIndex)
    {
        const int count = impl_->queueManager.jobCount();
        if (fromIndex < 0 || fromIndex >= count || toIndex < 0 || toIndex >= count || fromIndex == toIndex) {
            return;
        }
        impl_->queueManager.moveJob(fromIndex, toIndex);
        impl_->syncCoreQueueModel();
    }

    void ArtifactRenderQueueService::removeAllRenderQueues() {
        impl_->queueManager.removeAllJobs();
        impl_->syncCoreQueueModel();
    }

    void ArtifactRenderQueueService::removeRenderQueuesForComposition(const ArtifactCore::CompositionID& compositionId)
    {
        impl_->queueManager.removeJobsForComposition(compositionId);
        impl_->syncCoreQueueModel();
    }

    bool ArtifactRenderQueueService::hasRenderQueueForComposition(const ArtifactCore::CompositionID& compositionId) const
    {
        return impl_->queueManager.hasJobForComposition(compositionId);
    }

    int ArtifactRenderQueueService::renderQueueCountForComposition(const ArtifactCore::CompositionID& compositionId) const
    {
        return impl_->queueManager.countJobsForComposition(compositionId);
    }

    ArtifactCore::CompositionID ArtifactRenderQueueService::jobCompositionIdAt(int index) const
    {
        return impl_->queueManager.jobCompositionIdAt(index);
    }

    QString ArtifactRenderQueueService::jobCompositionNameAt(int index) const
    {
        return impl_->queueManager.jobCompositionNameAt(index);
    }

    QString ArtifactRenderQueueService::jobStatusAt(int index) const
    {
        return impl_->queueManager.jobStatusAt(index);
    }

    int ArtifactRenderQueueService::jobProgressAt(int index) const
    {
        return impl_->queueManager.jobProgressAt(index);
    }

    QString ArtifactRenderQueueService::jobOutputPathAt(int index) const
    {
        return impl_->queueManager.jobOutputPathAt(index);
    }

    void ArtifactRenderQueueService::setJobOutputPathAt(int index, const QString& outputPath)
    {
        impl_->queueManager.setJobOutputPath(index, outputPath.trimmed());
        impl_->syncCoreQueueModel();
    }

    bool ArtifactRenderQueueService::jobFrameRangeAt(int index, int* startFrame, int* endFrame) const
    {
        return impl_->queueManager.jobFrameRangeAt(index, startFrame, endFrame);
    }

    void ArtifactRenderQueueService::setJobFrameRangeAt(int index, int startFrame, int endFrame)
    {
        impl_->queueManager.setJobFrameRange(index, startFrame, endFrame);
        impl_->syncCoreQueueModel();
    }

    bool ArtifactRenderQueueService::jobOutputSettingsAt(
        int index,
        QString* outputFormat,
        QString* codec,
        QString* codecProfile,
        int* width,
        int* height,
        double* fps,
        int* bitrateKbps) const
    {
        return impl_->queueManager.jobOutputSettingsAt(index, outputFormat, codec, codecProfile, width, height, fps, bitrateKbps);
    }

    bool ArtifactRenderQueueService::jobOutputSettingsAt(
        int index,
        QString* outputFormat,
        QString* codec,
        int* width,
        int* height,
        double* fps,
        int* bitrateKbps) const
    {
        QString codecProfile;
        return jobOutputSettingsAt(index, outputFormat, codec, &codecProfile, width, height, fps, bitrateKbps);
    }

    void ArtifactRenderQueueService::setJobOutputSettingsAt(
        int index,
        const QString& outputFormat,
        const QString& codec,
        const QString& codecProfile,
        int width,
        int height,
        double fps,
        int bitrateKbps)
    {
        impl_->queueManager.setJobOutputSettings(index, outputFormat, codec, codecProfile, width, height, fps, bitrateKbps);
        impl_->syncCoreQueueModel();
    }

    void ArtifactRenderQueueService::setJobOutputSettingsAt(
        int index,
        const QString& outputFormat,
        const QString& codec,
        int width,
        int height,
        double fps,
        int bitrateKbps)
    {
        setJobOutputSettingsAt(index, outputFormat, codec, QString(), width, height, fps, bitrateKbps);
    }

    QString ArtifactRenderQueueService::jobEncoderBackendAt(int index) const
    {
        const int count = impl_->queueManager.jobCount();
        if (index < 0 || index >= count) {
            return QStringLiteral("auto");
        }
        return impl_->queueManager.getJob(index).encoderBackend;
    }

    void ArtifactRenderQueueService::setJobEncoderBackendAt(int index, const QString& backend)
    {
        const int count = impl_->queueManager.jobCount();
        if (index < 0 || index >= count) {
            return;
        }
        auto job = impl_->queueManager.getJob(index);
        job.encoderBackend = backend.trimmed().isEmpty() ? QStringLiteral("auto") : backend.trimmed().toLower();
        impl_->queueManager.updateJob(index, job);
        impl_->syncCoreQueueModel();
    }

    QString ArtifactRenderQueueService::jobErrorMessageAt(int index) const
    {
        return impl_->queueManager.jobErrorMessageAt(index);
    }

    bool ArtifactRenderQueueService::jobOverlayTransformAt(int index, float* offsetX, float* offsetY, float* scale, float* rotationDeg) const
    {
        return impl_->queueManager.jobOverlayTransformAt(index, offsetX, offsetY, scale, rotationDeg);
    }

    void ArtifactRenderQueueService::setJobOverlayTransform(int index, float offsetX, float offsetY, float scale, float rotationDeg)
    {
        impl_->queueManager.setJobOverlayTransform(index, offsetX, offsetY, scale, rotationDeg);
        impl_->syncCoreQueueModel();
    }

    void ArtifactRenderQueueService::resetJobForRerun(int index)
    {
        impl_->queueManager.resetJobForRerun(index);
        impl_->syncCoreQueueModel();
    }

    int ArtifactRenderQueueService::resetCompletedAndFailedJobsForRerun()
    {
        const int changed = impl_->queueManager.resetCompletedAndFailedJobsForRerun();
        if (changed > 0) {
            impl_->syncCoreQueueModel();
        }
        return changed;
    }

    QList<ArtifactRenderQueueService::FailedFrameInfo> ArtifactRenderQueueService::detectFailedFrames(int jobIndex) const
    {
        const int count = impl_->queueManager.jobCount();
        if (jobIndex < 0 || jobIndex >= count) {
            return {};
        }
        
        const auto job = impl_->queueManager.getJob(jobIndex);
        ArtifactRenderQueueManager::FailedFrameDetector detector;
        const auto failedFrames = detector.detectFailedFrames(jobIndex, job);
        
        // 変換
        QList<FailedFrameInfo> result;
        for (const auto& ff : failedFrames) {
            result.append(FailedFrameInfo{ff.jobId, ff.frameNumber, ff.errorMessage, ff.timestamp});
        }
        return result;
    }

    int ArtifactRenderQueueService::rerenderFailedFrames(int jobIndex, const QList<int>& frameNumbers)
    {
        const int count = impl_->queueManager.jobCount();
        if (jobIndex < 0 || jobIndex >= count || frameNumbers.isEmpty()) {
            return 0;
        }
        
        // 失敗フレームのみを再レンダリングするジョブを作成
        // 実際の実装は複雑になるため、現在はジョブを再設定して再レンダリング
        impl_->queueManager.setJobStatus(jobIndex, ArtifactRenderJob::Status::Pending);
        impl_->syncCoreQueueModel();
        
        return frameNumbers.size();
    }

    void ArtifactRenderQueueService::startAllJobs() {
        if (impl_->isRendering_.exchange(true, std::memory_order_acq_rel)) return;
        impl_->shutdownRequested_.store(false, std::memory_order_release);

        impl_->queueManager.startAllJobs();

        // 既存のワーカースレッドがあれば待機
        if (impl_->workerThread_.joinable()) {
            impl_->workerThread_.join();
        }

        // UI スレッドをブロックしないよう、ワーカースレッドで実行
        impl_->workerThread_ = std::thread([this]() {
            try {
            const int count = impl_->queueManager.jobCount();
            for (int i = 0; i < count; ++i) {
                if (impl_->shutdownRequested_.load(std::memory_order_acquire)) break;

                const auto job = impl_->queueManager.getJob(i);
                if (job.status != ArtifactRenderJob::Status::Rendering &&
                    job.status != ArtifactRenderJob::Status::Pending) {
                    continue;
                }

                QMetaObject::invokeMethod(this, [this, i]() {
                    impl_->queueManager.setJobProgress(i, 0);
                }, Qt::QueuedConnection);

                // 出力設定
                const QString outputPath = job.outputPath.trimmed();
                const QFileInfo outInfo(outputPath);
                QDir outDir = outInfo.dir();
                if (!outDir.exists()) outDir.mkpath(".");

                const QString ext = outInfo.suffix().toLower();
                const QString format = deriveContainerFromJob(job);
                const bool isVideo = !isImageSequenceContainer(format) &&
                    (isVideoContainer(format) ||
                     ext == QStringLiteral("mp4") || ext == QStringLiteral("mov") ||
                     ext == QStringLiteral("avi") || ext == QStringLiteral("mkv") ||
                     ext == QStringLiteral("webm") || ext == QStringLiteral("wmv") ||
                     ext == QStringLiteral("gif") || ext == QStringLiteral("apng") ||
                     ext == QStringLiteral("webp"));

                const int startF = job.startFrame;
                const int endF = job.endFrame;
                const int totalFrames = std::max(1, endF - startF + 1);

                std::atomic<bool> success = true;
                std::atomic<int> framesRendered = 0;
                QString failureReason;
                std::unique_ptr<IVideoEncodeBackend> videoBackend;
                if (isVideo) {
                    QString backendName;
                    videoBackend = createVideoEncodeBackend(job, &backendName, &failureReason);
                    if (!videoBackend) {
                        qWarning() << "[RenderService] Failed to initialize video backend"
                                   << "job=" << i
                                   << "backend=" << backendName
                                   << "error=" << failureReason;
                        success.store(false, std::memory_order_relaxed);
                    }
                }

                const ArtifactCompositionPtr composition = impl_->resolveComposition(job);
                for (int f = startF; f <= endF; ++f) {
                    if (!success.load(std::memory_order_relaxed)) {
                        break;
                    }
                    if (impl_->shutdownRequested_.load(std::memory_order_acquire)) {
                        success.store(false, std::memory_order_relaxed);
                        failureReason = QStringLiteral("Rendering cancelled (shutdown)");
                        break;
                    }

                    // ステータスチェック
                    const auto currentJobStatus = impl_->queueManager.getJob(i).status;
                    if (currentJobStatus != ArtifactRenderJob::Status::Rendering) {
                        success.store(false, std::memory_order_relaxed);
                        break;
                    }

                    QImage qimg = impl_->renderSingleFrameComposition(job, composition, f);
                    if (qimg.isNull()) {
                        success.store(false, std::memory_order_relaxed);
                        failureReason = QStringLiteral("Rendered frame is null");
                        break;
                    }

                    if (isVideo) {
                        if (!videoBackend->addFrame(qimg, f, &failureReason)) {
                            success.store(false, std::memory_order_relaxed);
                            break;
                        }
                    } else {
                        const QString ext = sequenceExtension(job.outputFormat, job.codec);
                        QString baseName = outInfo.completeBaseName();
                        if (baseName.isEmpty()) baseName = "render";
                        QString framePath = outDir.filePath(QString("%1_%2.%3").arg(baseName).arg(f, 4, 10, QChar('0')).arg(ext));
                        ArtifactCore::ImageExporter exporter;
                        ArtifactCore::ImageExportOptions imgOpts;
                        imgOpts.format = ext;
                        auto result = exporter.write(qimg, framePath, imgOpts);
                        if (!result.success) {
                            success.store(false, std::memory_order_relaxed);
                            failureReason = QStringLiteral("Failed to save image sequence frame: %1").arg(result.errorMessage);
                            break;
                        }
                    }

                    int rendered = ++framesRendered;
                    int progress = static_cast<int>((static_cast<float>(rendered) / totalFrames) * 100);
                    QMetaObject::invokeMethod(this, [this, i, progress]() {
                        impl_->queueManager.setJobProgress(i, progress);
                    }, Qt::QueuedConnection);
                }

                // 終了処理
                if (videoBackend) {
                    videoBackend->close();
                }

                QMetaObject::invokeMethod(this, [this, i, success_val = success.load(), failureReason]() {
                    if (success_val) {
                        impl_->queueManager.setJobProgress(i, 100);
                        impl_->queueManager.setJobCompleted(i);
                    } else {
                        const QString reason = failureReason.trimmed().isEmpty()
                            ? QStringLiteral("Render interrupted or failed")
                            : failureReason;
                        impl_->queueManager.setJobFailed(i, reason);
                    }
                }, Qt::QueuedConnection);
            }

            QMetaObject::invokeMethod(this, [this]() {
                impl_->isRendering_.store(false, std::memory_order_release);
                if (impl_->allJobsCompleted) {
                    impl_->allJobsCompleted();
                }
                Q_EMIT allJobsCompleted();
            }, Qt::QueuedConnection);

            } catch (const std::exception& ex) {
                qCritical() << "[RenderService] Worker thread exception:" << ex.what();
                QMetaObject::invokeMethod(this, [this]() {
                    impl_->isRendering_.store(false, std::memory_order_release);
                }, Qt::QueuedConnection);
            } catch (...) {
                qCritical() << "[RenderService] Worker thread unknown exception";
                QMetaObject::invokeMethod(this, [this]() {
                    impl_->isRendering_.store(false, std::memory_order_release);
                }, Qt::QueuedConnection);
            }
        });

        ArtifactCore::RendererQueueManager::instance().startRendering();
    }

    void ArtifactRenderQueueService::pauseAllJobs() {
        impl_->queueManager.pauseAllJobs();
        impl_->shutdownRequested_.store(true, std::memory_order_release);
    }

    void ArtifactRenderQueueService::cancelAllJobs() {
        impl_->queueManager.cancelAllJobs();
        impl_->shutdownRequested_.store(true, std::memory_order_release);
    }

    int ArtifactRenderQueueService::jobCount() const {
        return impl_->queueManager.jobCount();
    }

    int ArtifactRenderQueueService::getTotalProgress() const {
        return impl_->queueManager.getTotalProgress();
    }

    // シグナルセッターメソッド
    void ArtifactRenderQueueService::setJobAddedCallback(std::function<void(int)> callback) {
        impl_->jobAdded = callback;
    }

    void ArtifactRenderQueueService::setJobRemovedCallback(std::function<void(int)> callback) {
        impl_->jobRemoved = callback;
    }

    void ArtifactRenderQueueService::setJobUpdatedCallback(std::function<void(int)> callback) {
        impl_->jobUpdated = callback;
    }

    void ArtifactRenderQueueService::setJobProgressChangedCallback(std::function<void(int, int)> callback) {
        impl_->jobProgressChanged = callback;
    }

    void ArtifactRenderQueueService::setJobStatusChangedCallback(std::function<void(int, int)> callback) {
        impl_->jobStatusChangedForUi = callback;
    }

    void ArtifactRenderQueueService::setAllJobsCompletedCallback(std::function<void()> callback) {
        impl_->allJobsCompleted = callback;
    }

    void ArtifactRenderQueueService::setAllJobsRemovedCallback(std::function<void()> callback) {
        impl_->allJobsRemoved = callback;
    }

    void ArtifactRenderQueueService::setQueueReorderedCallback(std::function<void(int, int)> callback) {
        impl_->queueReordered = callback;
    }

    QJsonArray ArtifactRenderQueueService::toJson() const {
        QJsonArray arr;
        const int count = impl_->queueManager.jobCount();
        for (int i = 0; i < count; ++i) {
            const auto job = impl_->queueManager.getJob(i);
            QJsonObject obj;
            obj["compositionId"] = job.compositionId.toString();
            obj["compositionName"] = job.compositionName;
            obj["outputPath"] = job.outputPath;
            obj["outputFormat"] = job.outputFormat;
            obj["codec"] = job.codec;
            obj["codecProfile"] = job.codecProfile;
            obj["encoderBackend"] = job.encoderBackend;
            obj["resolutionWidth"] = job.resolutionWidth;
            obj["resolutionHeight"] = job.resolutionHeight;
            obj["frameRate"] = job.frameRate;
            obj["bitrate"] = job.bitrate;
            obj["startFrame"] = job.startFrame;
            obj["endFrame"] = job.endFrame;
            obj["overlayOffsetX"] = static_cast<double>(job.overlayOffsetX);
            obj["overlayOffsetY"] = static_cast<double>(job.overlayOffsetY);
            obj["overlayScale"] = static_cast<double>(job.overlayScale);
            obj["overlayRotationDeg"] = static_cast<double>(job.overlayRotationDeg);
            arr.append(obj);
        }
        return arr;
    }

    void ArtifactRenderQueueService::fromJson(const QJsonArray& arr) {
        impl_->queueManager.removeAllJobs();
        for (const auto& val : arr) {
            if (!val.isObject()) continue;
            const QJsonObject obj = val.toObject();

            ArtifactRenderJob job;
            job.compositionId = ArtifactCore::CompositionID(obj["compositionId"].toString());
            job.compositionName = obj["compositionName"].toString();
            job.outputPath = obj["outputPath"].toString();
            job.outputFormat = obj["outputFormat"].toString();
            job.codec = obj["codec"].toString();
            job.codecProfile = obj["codecProfile"].toString();
            job.encoderBackend = obj["encoderBackend"].toString("auto");
            job.resolutionWidth = obj["resolutionWidth"].toInt(1920);
            job.resolutionHeight = obj["resolutionHeight"].toInt(1080);
            job.frameRate = obj["frameRate"].toDouble(30.0);
            job.bitrate = obj["bitrate"].toInt(8000);
            job.startFrame = obj["startFrame"].toInt(0);
            job.endFrame = obj["endFrame"].toInt(100);
            job.overlayOffsetX = static_cast<float>(obj["overlayOffsetX"].toDouble(0.0));
            job.overlayOffsetY = static_cast<float>(obj["overlayOffsetY"].toDouble(0.0));
            job.overlayScale = static_cast<float>(obj["overlayScale"].toDouble(1.0));
            job.overlayRotationDeg = static_cast<float>(obj["overlayRotationDeg"].toDouble(0.0));
            job.status = ArtifactRenderJob::Status::Pending;
            job.progress = 0;

            impl_->queueManager.addJob(job);
        }
    }

    void ArtifactRenderQueueService::clearQueueForLoad() {
        impl_->queueManager.removeAllJobs();
    }
};
