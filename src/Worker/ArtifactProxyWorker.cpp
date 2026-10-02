#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QProcessEnvironment>
#include <QStringList>
#include <QTextStream>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <cstdio>
#include <functional>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/audio_fifo.h>
#include <libavutil/channel_layout.h>
#include <libavutil/error.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
#include <libavutil/samplefmt.h>
#include <libavutil/samplefmt.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
}

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <wrl/client.h>
#endif

namespace {

void writeMessage(QTextStream& stream, const QJsonObject& message)
{
    stream << QJsonDocument(message).toJson(QJsonDocument::Compact) << '\n';
    stream.flush();
}

QString resolveFfmpegExecutable()
{
    const QString executableName = QStringLiteral("ffmpeg.exe");
    const QString executableStem = QStringLiteral("ffmpeg");
    const QString appDir = QCoreApplication::applicationDirPath();
    const QStringList candidates{
        QDir(appDir).filePath(executableName),
        QDir(appDir).filePath(executableStem),
        QDir(appDir).filePath(QStringLiteral("ffmpeg/") + executableName),
        QDir(appDir).filePath(QStringLiteral("tools/") + executableName),
        QDir(appDir).filePath(QStringLiteral("tools/ffmpeg/") + executableName),
        QDir(appDir).filePath(QStringLiteral("../ffmpeg/") + executableName),
        QDir(appDir).filePath(QStringLiteral("../tools/ffmpeg/") + executableName)
    };
    for (const QString& candidate : candidates) {
        const QFileInfo info(candidate);
        if (info.isFile()) {
            return info.absoluteFilePath();
        }
    }
    const QStringList pathEntries = QProcessEnvironment::systemEnvironment()
                                        .value(QStringLiteral("PATH"))
                                        .split(QDir::listSeparator(), Qt::SkipEmptyParts);
    for (const QString& pathEntry : pathEntries) {
        const QFileInfo info(QDir(pathEntry).filePath(executableName));
        if (info.isFile()) {
            return info.absoluteFilePath();
        }
    }
    return executableName;
}

int fail(QTextStream& stream, const QString& jobId, const QString& reason)
{
    writeMessage(stream, {{QStringLiteral("type"), QStringLiteral("failed")},
                          {QStringLiteral("jobId"), jobId},
                          {QStringLiteral("reason"), reason}});
    return 1;
}

bool generateWithFfmpeg(const QString& sourcePath, const QString& temporaryOutputPath,
                        double scale, const QString& cancelPath, QString* failureReason)
{
    const QString scaleExpression = QStringLiteral("scale=trunc(iw*%1/2)*2:trunc(ih*%1/2)*2")
                                        .arg(scale, 0, 'f', 3);
    QProcess ffmpeg;
    ffmpeg.setProcessChannelMode(QProcess::SeparateChannels);
    ffmpeg.start(resolveFfmpegExecutable(),
                 {QStringLiteral("-y"), QStringLiteral("-i"), sourcePath,
                  QStringLiteral("-vf"), scaleExpression,
                  QStringLiteral("-c:v"), QStringLiteral("libx264"),
                  QStringLiteral("-preset"), QStringLiteral("fast"),
                  QStringLiteral("-crf"), QStringLiteral("23"),
                  QStringLiteral("-c:a"), QStringLiteral("aac"), temporaryOutputPath});
    if (!ffmpeg.waitForStarted(5000)) {
        if (failureReason) *failureReason = QStringLiteral("Unable to start ffmpeg");
        return false;
    }
    bool cancelled = false;
    while (!ffmpeg.waitForFinished(100)) {
        if (!cancelPath.isEmpty() && QFileInfo(cancelPath).exists()) {
            cancelled = true;
            ffmpeg.terminate();
            if (!ffmpeg.waitForFinished(500)) ffmpeg.kill();
            break;
        }
    }
    if (cancelled || ffmpeg.exitStatus() != QProcess::NormalExit ||
        ffmpeg.exitCode() != 0 || !QFileInfo(temporaryOutputPath).isFile()) {
        const QByteArray diagnostics = ffmpeg.readAllStandardError().trimmed();
        if (!diagnostics.isEmpty()) {
            QTextStream errorStream(stderr);
            errorStream << QString::fromLocal8Bit(diagnostics) << '\n';
            errorStream.flush();
        }
        QFile::remove(temporaryOutputPath);
        if (failureReason) *failureReason = cancelled
            ? QStringLiteral("Proxy job was cancelled")
            : QStringLiteral("ffmpeg proxy generation failed");
        return false;
    }
    return true;
}

QString ffmpegNativeError(const QString& operation, int error)
{
    char buffer[AV_ERROR_MAX_STRING_SIZE] = {};
    av_strerror(error, buffer, sizeof(buffer));
    return QStringLiteral("%1: %2").arg(operation, QString::fromUtf8(buffer));
}

bool isHardwareH264Encoder(const QString& encoderName)
{
    return encoderName == QStringLiteral("h264_nvenc") || encoderName == QStringLiteral("h264_qsv") ||
           encoderName == QStringLiteral("h264_amf") || encoderName == QStringLiteral("h264_mf");
}

bool validateGeneratedProxy(const QString& path, QString* failureReason)
{
    AVFormatContext* format = nullptr;
    const int openResult = avformat_open_input(&format, path.toUtf8().constData(), nullptr, nullptr);
    if (openResult < 0) {
        if (failureReason) *failureReason = ffmpegNativeError(QStringLiteral("Cannot reopen generated proxy"), openResult);
        return false;
    }
    const int infoResult = avformat_find_stream_info(format, nullptr);
    const int videoStream = infoResult < 0 ? infoResult : av_find_best_stream(format, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    const int audioStream = infoResult < 0 ? infoResult : av_find_best_stream(format, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
    const bool audioIsDecodable = audioStream < 0 ||
        avcodec_find_decoder(format->streams[audioStream]->codecpar->codec_id) != nullptr;
    avformat_close_input(&format);
    if (videoStream < 0) {
        if (failureReason) *failureReason = QStringLiteral("Generated proxy has no decodable video stream");
        return false;
    }
    if (!audioIsDecodable) {
        if (failureReason) *failureReason = QStringLiteral("Generated proxy has an undecodable audio stream");
        return false;
    }
    return true;
}

const AVCodec* findNativeH264Encoder(AVPixelFormat* pixelFormat, QStringList* candidates = nullptr,
                                     bool preferHardware = false)
{
    const QStringList preferredNames{QStringLiteral("h264_nvenc"), QStringLiteral("h264_qsv"),
                                     QStringLiteral("h264_amf"), QStringLiteral("h264_mf")};
    const AVCodec* fallbackCodec = nullptr;
    AVPixelFormat fallbackFormat = AV_PIX_FMT_NONE;
    void* iterator = nullptr;
    while (const AVCodec* candidate = av_codec_iterate(&iterator)) {
        if (!av_codec_is_encoder(candidate) || candidate->id != AV_CODEC_ID_H264 || !candidate->pix_fmts) continue;
        if (candidates) candidates->append(QString::fromUtf8(candidate->name));
        for (const AVPixelFormat* format = candidate->pix_fmts; *format != AV_PIX_FMT_NONE; ++format) {
            if (sws_isSupportedOutput(*format)) {
                if (!fallbackCodec) {
                    fallbackCodec = candidate;
                    fallbackFormat = *format;
                }
                if (!preferHardware || preferredNames.contains(QString::fromUtf8(candidate->name))) {
                    *pixelFormat = *format;
                    return candidate;
                }
                break;
            }
        }
    }
    if (fallbackCodec) *pixelFormat = fallbackFormat;
    return fallbackCodec;
}

bool generateWithFfmpegNative(const QString& sourcePath, const QString& temporaryOutputPath,
                              double scale, const QString& cancelPath,
                              const std::function<void(double)>& reportProgress,
                              QString* encoderName, QStringList* encoderCandidates,
                              QString* failureReason, bool preferHardware, bool reencodeAudio)
{
    AVFormatContext* input = nullptr;
    AVFormatContext* output = nullptr;
    AVCodecContext* decoder = nullptr;
    AVCodecContext* encoder = nullptr;
    AVPacket* packet = nullptr;
    AVPacket* encodedPacket = nullptr;
    AVFrame* decodedFrame = nullptr;
    AVFrame* encodedFrame = nullptr;
    AVCodecContext* audioDecoder = nullptr;
    AVCodecContext* audioEncoder = nullptr;
    AVFrame* audioDecodedFrame = nullptr;
    AVFrame* audioEncodedFrame = nullptr;
    AVPacket* audioPacket = nullptr;
    SwrContext* audioSwr = nullptr;
    AVAudioFifo* audioFifo = nullptr;
    int audioFrameSize = 0;
    int64_t audioPts = 0;
    AVChannelLayout audioSwrInLayout = {};
    AVChannelLayout audioSwrOutLayout = {};
    SwsContext* scaler = nullptr;
    int result = 0;
    bool succeeded = false;
    double lastReportedProgress = -1.0;

    do {
        result = avformat_open_input(&input, sourcePath.toUtf8().constData(), nullptr, nullptr);
        if (result < 0) break;
        result = avformat_find_stream_info(input, nullptr);
        if (result < 0) break;
        const int videoInputIndex = av_find_best_stream(input, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
        if (videoInputIndex < 0) { result = videoInputIndex; break; }
        int audioInputIndex = av_find_best_stream(input, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
        AVStream* videoInput = input->streams[videoInputIndex];
        const AVCodec* decoderCodec = avcodec_find_decoder(videoInput->codecpar->codec_id);
        if (!decoderCodec) { result = AVERROR_DECODER_NOT_FOUND; break; }
        decoder = avcodec_alloc_context3(decoderCodec);
        if (!decoder) { result = AVERROR(ENOMEM); break; }
        result = avcodec_parameters_to_context(decoder, videoInput->codecpar);
        if (result < 0) break;
        result = avcodec_open2(decoder, decoderCodec, nullptr);
        if (result < 0) break;

        int targetWidth = std::max(2, (static_cast<int>(std::lround(decoder->width * scale)) & ~1));
        int targetHeight = std::max(2, (static_cast<int>(std::lround(decoder->height * scale)) & ~1));
        const double floorScale = std::max(64.0 / targetWidth, 64.0 / targetHeight);
        targetWidth = std::max(64, static_cast<int>(std::ceil(targetWidth * floorScale)) & ~1);
        targetHeight = std::max(64, static_cast<int>(std::ceil(targetHeight * floorScale)) & ~1);
        AVRational frameRate = av_guess_frame_rate(input, videoInput, nullptr);
        if (frameRate.num <= 0 || frameRate.den <= 0) frameRate = AVRational{30, 1};

        result = avformat_alloc_output_context2(&output, nullptr, nullptr, temporaryOutputPath.toUtf8().constData());
        if (result < 0 || !output) { if (result >= 0) result = AVERROR_UNKNOWN; break; }
        AVPixelFormat encoderPixelFormat = AV_PIX_FMT_NONE;
        const AVCodec* encoderCodec = findNativeH264Encoder(&encoderPixelFormat, encoderCandidates, preferHardware);
        if (!encoderCodec) { result = AVERROR_ENCODER_NOT_FOUND; break; }
        if (encoderName) *encoderName = QString::fromUtf8(encoderCodec->name);
        AVStream* videoOutput = avformat_new_stream(output, nullptr);
        if (!videoOutput) { result = AVERROR(ENOMEM); break; }
        encoder = avcodec_alloc_context3(encoderCodec);
        if (!encoder) { result = AVERROR(ENOMEM); break; }
        encoder->codec_type = AVMEDIA_TYPE_VIDEO;
        encoder->codec_id = AV_CODEC_ID_H264;
        encoder->width = targetWidth;
        encoder->height = targetHeight;
        encoder->pix_fmt = encoderPixelFormat;
        encoder->time_base = av_inv_q(frameRate);
        encoder->framerate = frameRate;
        encoder->bit_rate = 2'000'000;
        encoder->gop_size = std::max(1, frameRate.num / frameRate.den);
        encoder->max_b_frames = 0;
        if (output->oformat->flags & AVFMT_GLOBALHEADER) encoder->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
        av_opt_set(encoder->priv_data, "preset", "medium", 0);
        result = avcodec_open2(encoder, encoderCodec, nullptr);
        if (result < 0) break;
        result = avcodec_parameters_from_context(videoOutput->codecpar, encoder);
        if (result < 0) break;
        videoOutput->time_base = encoder->time_base;

        AVStream* audioOutput = nullptr;
        AVStream* audioInputStream = audioInputIndex >= 0 ? input->streams[audioInputIndex] : nullptr;
        // Stream-copy the source audio when the container accepts its codec.
        // Otherwise (or when re-encoding is explicitly requested) decode and
        // encode to AAC so hardware video acceleration is not forfeited.
        const AVCodecID sourceAudioCodecId = audioInputStream
            ? audioInputStream->codecpar->codec_id : AV_CODEC_ID_NONE;
        const bool sourceAudioMuxable = sourceAudioCodecId != AV_CODEC_ID_NONE &&
            output->oformat->audio_codec != AV_CODEC_ID_NONE &&
            avformat_query_codec(output->oformat, sourceAudioCodecId, FF_COMPLIANCE_NORMAL) > 0;
        const bool transcodeAudio = audioInputStream && reencodeAudio;
        if (audioInputStream && !transcodeAudio) {
            if (!sourceAudioMuxable) {
                result = AVERROR_MUXER_NOT_FOUND;
                break;
            }
            audioOutput = avformat_new_stream(output, nullptr);
            if (!audioOutput) { result = AVERROR(ENOMEM); break; }
            result = avcodec_parameters_copy(audioOutput->codecpar, audioInputStream->codecpar);
            if (result < 0) break;
            audioOutput->time_base = audioInputStream->time_base;
        } else if (audioInputStream && transcodeAudio) {
            const AVCodec* audioDecoderCodec = avcodec_find_decoder(sourceAudioCodecId);
            const AVCodec* aacEncoderCodec = avcodec_find_encoder(AV_CODEC_ID_AAC);
            if (!audioDecoderCodec || !aacEncoderCodec) { result = AVERROR_ENCODER_NOT_FOUND; break; }
            audioDecoder = avcodec_alloc_context3(audioDecoderCodec);
            audioEncoder = avcodec_alloc_context3(aacEncoderCodec);
            if (!audioDecoder || !audioEncoder) { result = AVERROR(ENOMEM); break; }
            result = avcodec_parameters_to_context(audioDecoder, audioInputStream->codecpar);
            if (result < 0) break;
            result = avcodec_open2(audioDecoder, audioDecoderCodec, nullptr);
            if (result < 0) break;
            audioEncoder->codec_type = AVMEDIA_TYPE_AUDIO;
            audioEncoder->codec_id = AV_CODEC_ID_AAC;
            audioEncoder->sample_rate = audioDecoder->sample_rate > 0
                ? audioDecoder->sample_rate : 48000;
            audioEncoder->ch_layout = audioDecoder->ch_layout;
            audioEncoder->bit_rate = 128'000;
            if (output->oformat->flags & AVFMT_GLOBALHEADER) {
                audioEncoder->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
            }
            result = avcodec_open2(audioEncoder, aacEncoderCodec, nullptr);
            if (result < 0) break;
            audioOutput = avformat_new_stream(output, nullptr);
            if (!audioOutput) { result = AVERROR(ENOMEM); break; }
            result = avcodec_parameters_from_context(audioOutput->codecpar, audioEncoder);
            if (result < 0) break;
            audioOutput->time_base = audioEncoder->time_base;
            audioDecodedFrame = av_frame_alloc();
            audioEncodedFrame = av_frame_alloc();
            audioPacket = av_packet_alloc();
            if (!audioDecodedFrame || !audioEncodedFrame || !audioPacket) {
                result = AVERROR(ENOMEM); break;
            }
            result = av_channel_layout_copy(&audioSwrInLayout, &audioDecoder->ch_layout);
            if (result < 0) break;
            result = av_channel_layout_copy(&audioSwrOutLayout, &audioEncoder->ch_layout);
            if (result < 0) break;
            // Configure through the option API rather than swr_alloc_set_opts2,
            // whose signature differs between FFmpeg 6/7 and 8+.
            audioSwr = swr_alloc();
            if (!audioSwr) { result = AVERROR(ENOMEM); break; }
            const int outRate = audioEncoder->sample_rate;
            const int inRate = audioDecoder->sample_rate > 0
                ? audioDecoder->sample_rate : outRate;
            av_opt_set_chlayout(audioSwr, "in_chlayout", &audioSwrInLayout, 0);
            av_opt_set_chlayout(audioSwr, "out_chlayout", &audioSwrOutLayout, 0);
            av_opt_set_int(audioSwr, "in_sample_rate", inRate, 0);
            av_opt_set_int(audioSwr, "out_sample_rate", outRate, 0);
            av_opt_set_sample_fmt(audioSwr, "in_sample_fmt", audioDecoder->sample_fmt, 0);
            av_opt_set_sample_fmt(audioSwr, "out_sample_fmt", audioEncoder->sample_fmt, 0);
            result = swr_init(audioSwr);
            if (result < 0) break;
            audioFrameSize = audioEncoder->frame_size > 0 ? audioEncoder->frame_size : 1024;
            audioFifo = av_audio_fifo_alloc(audioEncoder->sample_fmt,
                                            audioEncoder->ch_layout.nb_channels,
                                            audioFrameSize);
            if (!audioFifo) { result = AVERROR(ENOMEM); break; }
            audioEncodedFrame->format = audioEncoder->sample_fmt;
            audioEncodedFrame->nb_samples = audioFrameSize;
            audioEncodedFrame->ch_layout = audioEncoder->ch_layout;
            audioEncodedFrame->sample_rate = audioEncoder->sample_rate;
            result = av_frame_get_buffer(audioEncodedFrame, 0);
            if (result < 0) break;
        }
        if (!(output->oformat->flags & AVFMT_NOFILE)) {
            result = avio_open(&output->pb, temporaryOutputPath.toUtf8().constData(), AVIO_FLAG_WRITE);
            if (result < 0) break;
        }
        result = avformat_write_header(output, nullptr);
        if (result < 0) break;

        scaler = sws_getContext(decoder->width, decoder->height, decoder->pix_fmt,
                                targetWidth, targetHeight, encoder->pix_fmt,
                                SWS_BILINEAR, nullptr, nullptr, nullptr);
        if (!scaler) { result = AVERROR(ENOMEM); break; }
        packet = av_packet_alloc();
        encodedPacket = av_packet_alloc();
        decodedFrame = av_frame_alloc();
        encodedFrame = av_frame_alloc();
        if (!packet || !encodedPacket || !decodedFrame || !encodedFrame) { result = AVERROR(ENOMEM); break; }
        encodedFrame->format = encoder->pix_fmt;
        encodedFrame->width = targetWidth;
        encodedFrame->height = targetHeight;
        result = av_frame_get_buffer(encodedFrame, 32);
        if (result < 0) break;

        const auto writeEncoded = [&]() -> int {
            while (true) {
                const int receiveResult = avcodec_receive_packet(encoder, encodedPacket);
                if (receiveResult == AVERROR(EAGAIN) || receiveResult == AVERROR_EOF) return 0;
                if (receiveResult < 0) return receiveResult;
                av_packet_rescale_ts(encodedPacket, encoder->time_base, videoOutput->time_base);
                encodedPacket->stream_index = videoOutput->index;
                encodedPacket->pos = -1;
                const int writeResult = av_interleaved_write_frame(output, encodedPacket);
                av_packet_unref(encodedPacket);
                if (writeResult < 0) return writeResult;
            }
        };
        const auto encodeFrame = [&](AVFrame* frame) -> int {
            const int sendResult = avcodec_send_frame(encoder, frame);
            if (sendResult < 0) return sendResult;
            return writeEncoded();
        };

        // Moves every packet the audio encoder has ready into the muxer.
        const auto drainAudioPackets = [&]() -> int {
            while (true) {
                const int receiveResult = avcodec_receive_packet(audioEncoder, audioPacket);
                if (receiveResult == AVERROR(EAGAIN) || receiveResult == AVERROR_EOF) return 0;
                if (receiveResult < 0) return receiveResult;
                av_packet_rescale_ts(audioPacket, audioEncoder->time_base, audioOutput->time_base);
                audioPacket->stream_index = audioOutput->index;
                audioPacket->pos = -1;
                const int writeResult = av_interleaved_write_frame(output, audioPacket);
                av_packet_unref(audioPacket);
                if (writeResult < 0) return writeResult;
            }
        };

        // Pulls frames the audio decoder can produce, resamples them into the
        // FIFO, and submits every whole encoder frame that becomes available.
        // Buffering across decoded frames keeps A/V in sync when the source
        // and output sample rates or channel layouts differ.
        const auto drainAudioFrames = [&]() -> int {
            while (true) {
                const int receiveResult = avcodec_receive_frame(audioDecoder, audioDecodedFrame);
                if (receiveResult == AVERROR(EAGAIN) || receiveResult == AVERROR_EOF) return 0;
                if (receiveResult < 0) return receiveResult;
                const int converted = swr_convert(audioSwr,
                    audioEncodedFrame->extended_data, audioEncodedFrame->nb_samples,
                    audioDecodedFrame->extended_data, audioDecodedFrame->nb_samples);
                av_frame_unref(audioDecodedFrame);
                if (converted < 0) return converted;
                if (converted > 0 &&
                    av_audio_fifo_write(audioFifo,
                        reinterpret_cast<void* const*>(audioEncodedFrame->extended_data),
                        converted) < 0) {
                    return AVERROR(ENOMEM);
                }
                while (av_audio_fifo_size(audioFifo) >= audioFrameSize) {
                    const int readResult = av_audio_fifo_read(audioFifo,
                        reinterpret_cast<void* const*>(audioEncodedFrame->extended_data),
                        audioFrameSize);
                    if (readResult < audioFrameSize) return AVERROR_BUG;
                    audioEncodedFrame->pts = audioPts;
                    audioPts += audioFrameSize;
                    const int sendResult = avcodec_send_frame(audioEncoder, audioEncodedFrame);
                    if (sendResult < 0) return sendResult;
                    const int drainResult = drainAudioPackets();
                    if (drainResult < 0) return drainResult;
                }
            }
        };

        // Encodes whatever remains in the FIFO, padding the tail with silence
        // so the fixed-size AAC encoder always receives a complete frame.
        const auto flushAudioTail = [&]() -> int {
            while (av_audio_fifo_size(audioFifo) > 0) {
                const int available = av_audio_fifo_size(audioFifo);
                const int readResult = av_audio_fifo_read(audioFifo,
                    reinterpret_cast<void* const*>(audioEncodedFrame->extended_data),
                    available);
                if (readResult < 0) return readResult;
                if (available < audioFrameSize) {
                    const int pad = audioFrameSize - available;
                    if (av_samples_set_silence(audioEncodedFrame->data, available, pad,
                            audioEncodedFrame->ch_layout.nb_channels,
                            static_cast<AVSampleFormat>(audioEncodedFrame->format)) < 0) {
                        return AVERROR(EINVAL);
                    }
                }
                audioEncodedFrame->nb_samples = audioFrameSize;
                audioEncodedFrame->pts = audioPts;
                audioPts += audioFrameSize;
                const int sendResult = avcodec_send_frame(audioEncoder, audioEncodedFrame);
                if (sendResult < 0) return sendResult;
                const int drainResult = drainAudioPackets();
                if (drainResult < 0) return drainResult;
            }
            return 0;
        };

        while ((result = av_read_frame(input, packet)) >= 0) {
            if (!cancelPath.isEmpty() && QFileInfo(cancelPath).exists()) {
                result = AVERROR_EXIT;
                av_packet_unref(packet);
                break;
            }
            if (packet->stream_index == videoInputIndex) {
                result = avcodec_send_packet(decoder, packet);
                av_packet_unref(packet);
                if (result < 0) break;
                while ((result = avcodec_receive_frame(decoder, decodedFrame)) >= 0) {
                    result = av_frame_make_writable(encodedFrame);
                    if (result < 0) break;
                    sws_scale(scaler, decodedFrame->data, decodedFrame->linesize, 0, decoder->height,
                              encodedFrame->data, encodedFrame->linesize);
                    const int64_t sourcePts = decodedFrame->best_effort_timestamp == AV_NOPTS_VALUE
                        ? 0 : decodedFrame->best_effort_timestamp;
                    encodedFrame->pts = av_rescale_q(sourcePts, videoInput->time_base, encoder->time_base);
                    result = encodeFrame(encodedFrame);
                    if (input->duration > 0) {
                        const double progress = std::clamp(static_cast<double>(decodedFrame->best_effort_timestamp * av_q2d(videoInput->time_base) * AV_TIME_BASE) /
                                                           static_cast<double>(input->duration), 0.01, 0.99);
                        if (progress - lastReportedProgress >= 0.05) {
                            reportProgress(progress);
                            lastReportedProgress = progress;
                        }
                    }
                    av_frame_unref(decodedFrame);
                    if (result < 0) break;
                }
                if (result == AVERROR(EAGAIN)) result = 0;
                if (result < 0) break;
            } else if (packet->stream_index == audioInputIndex && audioOutput) {
                if (transcodeAudio) {
                    result = avcodec_send_packet(audioDecoder, packet);
                    av_packet_unref(packet);
                    if (result < 0) break;
                    result = drainAudioFrames();
                    if (result < 0) break;
                } else {
                    av_packet_rescale_ts(packet, input->streams[audioInputIndex]->time_base, audioOutput->time_base);
                    packet->stream_index = audioOutput->index;
                    packet->pos = -1;
                    result = av_interleaved_write_frame(output, packet);
                    av_packet_unref(packet);
                    if (result < 0) break;
                }
            } else {
                av_packet_unref(packet);
            }
        }
        if (result == AVERROR_EOF) result = 0;
        if (result < 0) break;
        result = avcodec_send_packet(decoder, nullptr);
        if (result < 0) break;
        while ((result = avcodec_receive_frame(decoder, decodedFrame)) >= 0) {
            result = av_frame_make_writable(encodedFrame);
            if (result < 0) break;
            sws_scale(scaler, decodedFrame->data, decodedFrame->linesize, 0, decoder->height,
                      encodedFrame->data, encodedFrame->linesize);
            encodedFrame->pts = av_rescale_q(decodedFrame->best_effort_timestamp, videoInput->time_base, encoder->time_base);
            result = encodeFrame(encodedFrame);
            av_frame_unref(decodedFrame);
            if (result < 0) break;
        }
        if (result == AVERROR_EOF || result == AVERROR(EAGAIN)) result = 0;
        if (result < 0) break;
        result = encodeFrame(nullptr);
        if (result < 0) break;
        if (transcodeAudio) {
            // Drain the audio decoder, encode the tail, then flush the encoder.
            result = avcodec_send_packet(audioDecoder, nullptr);
            if (result < 0) break;
            result = drainAudioFrames();
            if (result < 0) break;
            result = flushAudioTail();
            if (result < 0) break;
            result = avcodec_send_frame(audioEncoder, nullptr);
            if (result < 0 && result != AVERROR_EOF && result != AVERROR(EAGAIN)) break;
            result = drainAudioPackets();
            if (result < 0) break;
        }
        result = av_write_trailer(output);
        if (result < 0) break;
        succeeded = QFileInfo(temporaryOutputPath).isFile();
    } while (false);

    av_packet_free(&packet);
    av_packet_free(&encodedPacket);
    av_frame_free(&decodedFrame);
    av_frame_free(&encodedFrame);
    av_packet_free(&audioPacket);
    av_frame_free(&audioDecodedFrame);
    av_frame_free(&audioEncodedFrame);
    if (audioFifo) av_audio_fifo_free(audioFifo);
    if (audioSwr) {
        swr_close(audioSwr);
        swr_free(&audioSwr);
    }
    av_channel_layout_uninit(&audioSwrInLayout);
    av_channel_layout_uninit(&audioSwrOutLayout);
    avcodec_free_context(&audioDecoder);
    avcodec_free_context(&audioEncoder);
    sws_freeContext(scaler);
    avcodec_free_context(&decoder);
    avcodec_free_context(&encoder);
    if (output) {
        if (!(output->oformat->flags & AVFMT_NOFILE) && output->pb) avio_closep(&output->pb);
        avformat_free_context(output);
    }
    avformat_close_input(&input);
    if (!succeeded) {
        QFile::remove(temporaryOutputPath);
        if (failureReason) *failureReason = ffmpegNativeError(QStringLiteral("FFmpeg C API proxy generation"), result);
    }
    return succeeded;
}

#ifdef _WIN32

using Microsoft::WRL::ComPtr;

QString hresultReason(const QString& operation, HRESULT hr)
{
    return QStringLiteral("%1 failed (0x%2)").arg(operation).arg(static_cast<quint32>(hr), 8, 16, QChar('0'));
}

bool generateWithMediaFoundation(const QString& sourcePath, const QString& temporaryOutputPath,
                                 double scale, const QString& cancelPath, QString* failureReason)
{
    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    const bool comInitialized = SUCCEEDED(hr);
    if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) {
        if (failureReason) *failureReason = hresultReason(QStringLiteral("CoInitializeEx"), hr);
        return false;
    }
    hr = MFStartup(MF_VERSION, MFSTARTUP_FULL);
    if (FAILED(hr)) {
        if (comInitialized) CoUninitialize();
        if (failureReason) *failureReason = hresultReason(QStringLiteral("MFStartup"), hr);
        return false;
    }

    bool succeeded = false;
    bool cancelled = false;
    do {
        ComPtr<IMFAttributes> readerAttributes;
        hr = MFCreateAttributes(&readerAttributes, 1);
        if (FAILED(hr)) break;
        readerAttributes->SetUINT32(MF_SOURCE_READER_ENABLE_ADVANCED_VIDEO_PROCESSING, TRUE);

        ComPtr<IMFSourceReader> reader;
        hr = MFCreateSourceReaderFromURL(reinterpret_cast<LPCWSTR>(sourcePath.utf16()), readerAttributes.Get(), &reader);
        if (FAILED(hr)) break;
        reader->SetStreamSelection(MF_SOURCE_READER_ALL_STREAMS, FALSE);
        hr = reader->SetStreamSelection(MF_SOURCE_READER_FIRST_VIDEO_STREAM, TRUE);
        if (FAILED(hr)) break;

        ComPtr<IMFMediaType> videoInputType;
        hr = MFCreateMediaType(&videoInputType);
        if (FAILED(hr)) break;
        videoInputType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
        videoInputType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_RGB32);
        hr = reader->SetCurrentMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM, nullptr, videoInputType.Get());
        if (FAILED(hr)) break;
        hr = reader->GetCurrentMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM, &videoInputType);
        if (FAILED(hr)) break;

        UINT32 sourceWidth = 0, sourceHeight = 0, frameRateNumerator = 0, frameRateDenominator = 0;
        hr = MFGetAttributeSize(videoInputType.Get(), MF_MT_FRAME_SIZE, &sourceWidth, &sourceHeight);
        if (FAILED(hr) || sourceWidth == 0 || sourceHeight == 0) break;
        if (FAILED(MFGetAttributeRatio(videoInputType.Get(), MF_MT_FRAME_RATE,
                                       &frameRateNumerator, &frameRateDenominator)) || frameRateDenominator == 0) {
            frameRateNumerator = 30;
            frameRateDenominator = 1;
        }
        // The Windows H.264 MFT rejects very small frame sizes even though
        // FFmpeg's software/native encoders accept them. Keep the requested
        // aspect ratio while applying a conservative 64-pixel floor per axis.
        UINT32 targetWidth = std::max<UINT32>(2, static_cast<UINT32>(std::lround(sourceWidth * scale)) & ~1U);
        UINT32 targetHeight = std::max<UINT32>(2, static_cast<UINT32>(std::lround(sourceHeight * scale)) & ~1U);
        const double floorScale = std::max(64.0 / targetWidth, 64.0 / targetHeight);
        targetWidth = std::max<UINT32>(64, static_cast<UINT32>(std::ceil(targetWidth * floorScale)) & ~1U);
        targetHeight = std::max<UINT32>(64, static_cast<UINT32>(std::ceil(targetHeight * floorScale)) & ~1U);

        ComPtr<IMFSinkWriter> writer;
        hr = MFCreateSinkWriterFromURL(reinterpret_cast<LPCWSTR>(temporaryOutputPath.utf16()), nullptr, nullptr, &writer);
        if (FAILED(hr)) break;

        ComPtr<IMFMediaType> videoOutputType;
        hr = MFCreateMediaType(&videoOutputType);
        if (FAILED(hr)) break;
        videoOutputType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
        videoOutputType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_H264);
        videoOutputType->SetUINT32(MF_MT_AVG_BITRATE, 4'000'000);
        videoOutputType->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);
        MFSetAttributeSize(videoOutputType.Get(), MF_MT_FRAME_SIZE, targetWidth, targetHeight);
        MFSetAttributeRatio(videoOutputType.Get(), MF_MT_FRAME_RATE, frameRateNumerator, frameRateDenominator);
        MFSetAttributeRatio(videoOutputType.Get(), MF_MT_PIXEL_ASPECT_RATIO, 1, 1);
        DWORD videoStream = 0;
        hr = writer->AddStream(videoOutputType.Get(), &videoStream);
        if (FAILED(hr)) break;
        MFSetAttributeSize(videoInputType.Get(), MF_MT_FRAME_SIZE, targetWidth, targetHeight);
        MFSetAttributeRatio(videoInputType.Get(), MF_MT_FRAME_RATE, frameRateNumerator, frameRateDenominator);
        hr = writer->SetInputMediaType(videoStream, videoInputType.Get(), nullptr);
        if (FAILED(hr)) break;

        hr = writer->BeginWriting();
        if (FAILED(hr)) break;
        bool videoEnded = false;
        while (!videoEnded) {
            if (!cancelPath.isEmpty() && QFileInfo(cancelPath).exists()) {
                cancelled = true;
                break;
            }
            DWORD flags = 0;
            LONGLONG timestamp = 0;
            ComPtr<IMFSample> sourceSample;
            hr = reader->ReadSample(MF_SOURCE_READER_FIRST_VIDEO_STREAM, 0, nullptr, &flags, &timestamp, &sourceSample);
            if (FAILED(hr)) break;
            if ((flags & MF_SOURCE_READERF_ENDOFSTREAM) != 0) {
                videoEnded = true;
                continue;
            }
            if (!sourceSample) continue;

            ComPtr<IMFMediaBuffer> sourceBuffer;
            hr = sourceSample->ConvertToContiguousBuffer(&sourceBuffer);
            if (FAILED(hr)) break;
            BYTE* sourceBytes = nullptr;
            DWORD maxLength = 0, currentLength = 0;
            hr = sourceBuffer->Lock(&sourceBytes, &maxLength, &currentLength);
            if (FAILED(hr)) break;
            const UINT32 sourceStride = sourceWidth * 4;
            ComPtr<IMFMediaBuffer> destinationBuffer;
            hr = MFCreateMemoryBuffer(targetWidth * targetHeight * 4, &destinationBuffer);
            if (SUCCEEDED(hr)) {
                BYTE* destinationBytes = nullptr;
                DWORD destinationMaxLength = 0, destinationCurrentLength = 0;
                hr = destinationBuffer->Lock(&destinationBytes, &destinationMaxLength, &destinationCurrentLength);
                if (SUCCEEDED(hr)) {
                    for (UINT32 y = 0; y < targetHeight; ++y) {
                        const UINT32 sourceY = std::min(sourceHeight - 1, y * sourceHeight / targetHeight);
                        for (UINT32 x = 0; x < targetWidth; ++x) {
                            const UINT32 sourceX = std::min(sourceWidth - 1, x * sourceWidth / targetWidth);
                            std::memcpy(destinationBytes + (y * targetWidth + x) * 4,
                                        sourceBytes + sourceY * sourceStride + sourceX * 4, 4);
                        }
                    }
                    destinationBuffer->SetCurrentLength(targetWidth * targetHeight * 4);
                    destinationBuffer->Unlock();
                }
            }
            sourceBuffer->Unlock();
            if (FAILED(hr)) break;
            ComPtr<IMFSample> destinationSample;
            hr = MFCreateSample(&destinationSample);
            if (FAILED(hr)) break;
            destinationSample->AddBuffer(destinationBuffer.Get());
            LONGLONG duration = 0;
            if (FAILED(sourceSample->GetSampleDuration(&duration))) duration = 10'000'000LL * frameRateDenominator / frameRateNumerator;
            destinationSample->SetSampleTime(timestamp);
            destinationSample->SetSampleDuration(duration);
            hr = writer->WriteSample(videoStream, destinationSample.Get());
            if (FAILED(hr)) break;
        }
        if (SUCCEEDED(hr)) hr = writer->Finalize();
        if (SUCCEEDED(hr) && QFileInfo(temporaryOutputPath).isFile()) succeeded = true;
    } while (false);

    if (!succeeded) {
        QFile::remove(temporaryOutputPath);
        if (failureReason) {
            *failureReason = cancelled
                ? QStringLiteral("Proxy job was cancelled")
                : hresultReason(QStringLiteral("Media Foundation proxy generation"), hr);
        }
    }
    MFShutdown();
    if (comInitialized) CoUninitialize();
    return succeeded;
}

#endif

} // namespace

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("ArtifactProxyWorker"));

    QCommandLineParser parser;
    parser.addHelpOption();
    const QCommandLineOption requestOption(QStringLiteral("request"),
                                            QStringLiteral("Proxy job request JSON file."),
                                            QStringLiteral("path"));
    parser.addOption(requestOption);
    parser.process(app);
    bool helpRequested = parser.isSet(QStringLiteral("help"));
    for (int index = 1; index < argc; ++index) {
        const QString argument = QString::fromLocal8Bit(argv[index]);
        if (argument == QStringLiteral("--help") || argument == QStringLiteral("-h")) {
            helpRequested = true;
            break;
        }
    }
    if (helpRequested) {
        parser.showHelp(0);
        return 0;
    }

    QTextStream output(stdout, QIODevice::WriteOnly);
    const QString requestPath = parser.value(requestOption);
    if (requestPath.isEmpty()) {
        return fail(output, {}, QStringLiteral("Missing --request argument"));
    }

    QFile requestFile(requestPath);
    if (!requestFile.open(QIODevice::ReadOnly)) {
        return fail(output, {}, QStringLiteral("Cannot read request file"));
    }
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(requestFile.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        return fail(output, {}, QStringLiteral("Invalid request JSON"));
    }
    const QJsonObject request = document.object();
    const QString jobId = request.value(QStringLiteral("jobId")).toString();
    const QString sourcePath = request.value(QStringLiteral("sourcePath")).toString();
    const QString outputPath = request.value(QStringLiteral("outputPath")).toString();
    const QString temporaryOutputPath = request.value(QStringLiteral("temporaryOutputPath")).toString();
    double scale = request.value(QStringLiteral("scale")).toDouble();
    const QString qualityPreset = request.value(QStringLiteral("qualityPreset")).toString().trimmed().toLower();
    if (qualityPreset == QStringLiteral("half")) scale = 0.5;
    else if (qualityPreset == QStringLiteral("quarter")) scale = 0.25;
    else if (qualityPreset == QStringLiteral("eighth")) scale = 0.125;
    const QString appliedQualityPreset = qualityPreset.isEmpty() ? QStringLiteral("custom") : qualityPreset;
    const QString requestedBackend = request.value(QStringLiteral("backend")).toString(QStringLiteral("ffmpeg")).trimmed().toLower();
    const bool preferHardware = request.value(QStringLiteral("hardwareAccel")).toBool(false);
    const bool audioReencode = request.value(QStringLiteral("audioReencode")).toBool(false);
    if (request.value(QStringLiteral("protocolVersion")).toInt() != 1 || jobId.isEmpty() ||
        sourcePath.isEmpty() || outputPath.isEmpty() || temporaryOutputPath.isEmpty() ||
        scale <= 0.0 || scale > 1.0 || !QFileInfo(sourcePath).isFile() ||
        (!qualityPreset.isEmpty() && qualityPreset != QStringLiteral("full") &&
         qualityPreset != QStringLiteral("half") && qualityPreset != QStringLiteral("quarter") &&
         qualityPreset != QStringLiteral("eighth")) ||
        (requestedBackend != QStringLiteral("auto") && requestedBackend != QStringLiteral("ffmpeg") &&
         requestedBackend != QStringLiteral("native") &&
         requestedBackend != QStringLiteral("mediafoundation"))) {
        return fail(output, jobId, QStringLiteral("Invalid proxy job request"));
    }
    if (audioReencode && requestedBackend == QStringLiteral("mediafoundation")) {
        return fail(output, jobId,
                    QStringLiteral("Media Foundation backend does not support audio re-encoding"));
    }
#ifndef _WIN32
    if (requestedBackend == QStringLiteral("mediafoundation")) {
        return fail(output, jobId,
                    QStringLiteral("Media Foundation backend is only available on Windows"));
    }
#endif
    if (!QDir().mkpath(QFileInfo(temporaryOutputPath).absolutePath())) {
        return fail(output, jobId, QStringLiteral("Cannot create proxy output directory"));
    }
    const QString cancelPath = request.value(QStringLiteral("cancelPath")).toString();
    if (!cancelPath.isEmpty() && QFileInfo(cancelPath).exists()) {
        return fail(output, jobId, QStringLiteral("Proxy job was cancelled before start"));
    }

    QFile::remove(temporaryOutputPath);
    writeMessage(output, {{QStringLiteral("type"), QStringLiteral("started")},
                          {QStringLiteral("jobId"), jobId}});
    writeMessage(output, {{QStringLiteral("type"), QStringLiteral("progress")},
                          {QStringLiteral("jobId"), jobId},
                          {QStringLiteral("fraction"), 0.0}});

    QString selectedBackend = requestedBackend;
    QString selectedEncoder;
    QStringList encoderCandidates;
    QString failureReason;
    bool generated = false;
    if (requestedBackend == QStringLiteral("native") || requestedBackend == QStringLiteral("auto")) {
        generated = generateWithFfmpegNative(sourcePath, temporaryOutputPath, scale, cancelPath,
            [&output, &jobId](double fraction) {
                writeMessage(output, {{QStringLiteral("type"), QStringLiteral("progress")},
                                      {QStringLiteral("jobId"), jobId},
                                      {QStringLiteral("fraction"), fraction}});
            }, &selectedEncoder, &encoderCandidates, &failureReason, preferHardware,
            audioReencode);
        selectedBackend = QStringLiteral("native");
        if (!generated && requestedBackend == QStringLiteral("native")) return fail(output, jobId, failureReason);
    }
#ifdef _WIN32
    if (!generated &&
        (requestedBackend == QStringLiteral("auto") || requestedBackend == QStringLiteral("mediafoundation"))) {
        generated = generateWithMediaFoundation(sourcePath, temporaryOutputPath, scale, cancelPath, &failureReason);
        if (generated) selectedBackend = QStringLiteral("mediaFoundation");
        if (!generated && requestedBackend == QStringLiteral("mediafoundation")) {
            return fail(output, jobId, failureReason);
        }
    }
#endif
    if (!generated) {
        generated = generateWithFfmpeg(sourcePath, temporaryOutputPath, scale, cancelPath, &failureReason);
        selectedBackend = QStringLiteral("ffmpeg");
    }
    if (!generated) return fail(output, jobId, failureReason);

    if (!cancelPath.isEmpty() && QFileInfo(cancelPath).exists()) {
        QFile::remove(temporaryOutputPath);
        return fail(output, jobId, QStringLiteral("Proxy job was cancelled"));
    }
    if (!validateGeneratedProxy(temporaryOutputPath, &failureReason)) {
        QFile::remove(temporaryOutputPath);
        return fail(output, jobId, failureReason);
    }
    writeMessage(output, {{QStringLiteral("type"), QStringLiteral("progress")},
                          {QStringLiteral("jobId"), jobId},
                          {QStringLiteral("fraction"), 1.0}});

    const QString backupOutputPath = outputPath + QStringLiteral(".") + jobId + QStringLiteral(".previous");
    QFile::remove(backupOutputPath);
    const bool hadPreviousOutput = QFileInfo(outputPath).isFile();
    if (hadPreviousOutput && !QFile::rename(outputPath, backupOutputPath)) {
        QFile::remove(temporaryOutputPath);
        return fail(output, jobId, QStringLiteral("Cannot preserve existing proxy output"));
    }
    if (!QFile::rename(temporaryOutputPath, outputPath)) {
        QFile::remove(temporaryOutputPath);
        if (hadPreviousOutput) {
            QFile::rename(backupOutputPath, outputPath);
        }
        return fail(output, jobId, QStringLiteral("Cannot finalize proxy output"));
    }
    QFile::remove(backupOutputPath);
    const QFileInfo outputInfo(outputPath);
    if (outputInfo.size() <= 0) {
        QFile::remove(outputPath);
        return fail(output, jobId, QStringLiteral("Generated proxy is empty"));
    }
    QJsonObject completedMessage{{QStringLiteral("type"), QStringLiteral("completed")},
                          {QStringLiteral("jobId"), jobId},
                          {QStringLiteral("outputPath"), outputPath},
                          {QStringLiteral("qualityPreset"), appliedQualityPreset},
                          {QStringLiteral("backendRequested"), requestedBackend},
                          {QStringLiteral("backend"), selectedBackend},
                          {QStringLiteral("encoder"), selectedEncoder},
                          {QStringLiteral("hardwareAccelRequested"), preferHardware},
                          {QStringLiteral("hardwareEncoderUsed"), isHardwareH264Encoder(selectedEncoder)},
                          {QStringLiteral("audioReencodeRequested"), audioReencode},
                          {QStringLiteral("outputBytes"), outputInfo.size()}};
    if (!encoderCandidates.isEmpty()) {
        completedMessage.insert(QStringLiteral("encoderCandidates"), QJsonArray::fromStringList(encoderCandidates));
    }
    writeMessage(output, completedMessage);
    return 0;
}
