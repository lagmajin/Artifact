module;
#include <utility>
#include <algorithm>
#include <wobjectimpl.h>
#include <QDialog>
#include <QFormLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QFileDialog>
#include <QDialogButtonBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFrame>
#include <QLabel>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QGroupBox>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QStringList>
#include <QCoreApplication>
#include <QProcess>
#include <QProcessEnvironment>
#include <QDir>
#include <QFileInfo>
#include <QPalette>
#include <QColor>
#include <QFont>
#include <Widgets/Dialog/ArtifactDialogButtons.hpp>
module Artifact.Widget.Dialog.RenderOutputSetting;

import Encoder.FFmpegEncoder;
import Artifact.Render.Queue.Presets;
import Artifact.Widgets.RelativeSpinBox;


namespace Artifact
{
	
 class ArtifactRenderOutputSettingDialog::Impl
 {
 private:

 public:
  Impl();
  ~Impl();
  QLineEdit* outputPathEdit = nullptr;
  QPushButton* browseButton = nullptr;
  QComboBox* presetCombo = nullptr;  // フォーマットプリセット選択
  QComboBox* formatCombo = nullptr;
    QComboBox* codecCombo = nullptr;
    QString codecProfile;
  QComboBox* backendCombo = nullptr;
  QLabel* backendInfoLabel = nullptr;
  QLabel* presetSummaryLabel = nullptr;
  QLabel* formatGuideLabel = nullptr;
  QCheckBox* alphaEnabledCheck = nullptr;
  QGroupBox* advancedGroup = nullptr;
  QLabel* advancedAlphaLabel = nullptr;
  QLabel* advancedFilenameLabel = nullptr;
  QLabel* advancedTimecodeLabel = nullptr;
  QLabel* advancedColorLabel = nullptr;
  QLabel* advancedEncodeLabel = nullptr;
  QLabel* preflightSummaryLabel = nullptr;
  QLabel* preflightDetailsLabel = nullptr;
  QFrame* playbackGuideFrame = nullptr;
  QFrame* intermediateGuideFrame = nullptr;
  QFrame* noAlphaGuideFrame = nullptr;
  QFrame* alphaGuideFrame = nullptr;
  QFrame* straightGuideFrame = nullptr;
  QFrame* premultipliedGuideFrame = nullptr;
  QLabel* recommendationLabel = nullptr;
  QLabel* outputPackageLabel = nullptr;
  double compositionFrameRate = 30.0;
    QComboBox* renderBackendCombo = nullptr;
    QComboBox* resolutionCombo = nullptr;
  QSpinBox* widthSpin = nullptr;
  QSpinBox* heightSpin = nullptr;
  QDoubleSpinBox* fpsSpin = nullptr;
  QSpinBox* bitrateSpin = nullptr;
  QCheckBox* includeAudioCheck = nullptr;
  QCheckBox* multiChannelCheck = nullptr;
  QGroupBox* multiChannelGroup = nullptr;
  QCheckBox* beautyChannelCheck = nullptr;
  QCheckBox* alphaChannelCheck = nullptr;
  QCheckBox* depthChannelCheck = nullptr;
  QCheckBox* normalChannelCheck = nullptr;
  QCheckBox* velocityChannelCheck = nullptr;
  QCheckBox* objectIdChannelCheck = nullptr;
  QCheckBox* materialIdChannelCheck = nullptr;
  QCheckBox* albedoChannelCheck = nullptr;
  QCheckBox* emissionChannelCheck = nullptr;
  QSpinBox* framePaddingSpin = nullptr;
  QComboBox* audioCodecCombo = nullptr;
  QComboBox* audioChannelCombo = nullptr;
  QComboBox* audioSampleRateCombo = nullptr;
  QSpinBox* audioBitrateSpin = nullptr;
  QWidget* buttonRow = nullptr;
  QPushButton* okButton = nullptr;
  QPushButton* cancelButton = nullptr;

  void handleBrowseClicked(ArtifactRenderOutputSettingDialog* dialog);
  void syncResolutionEditors();
  void syncResolutionPreset();
  void updateBackendInfo();
  static QString resolveFfmpegExePath();
  static bool ffmpegExeSupportsEncoder(const QString& ffmpegPath, const QString& encoderName);
  static void ensureComboContains(QComboBox* combo, const QString& value);
  void loadFormatPresets();
  void applyPresetToEditors(const QString& presetId);
  void updatePresetSummary();
  void updateFormatGuide();
  void updateAdvancedSummary();
  void updateActionLabels();
  void updateFrameRatePreflight();
  void updateMultiChannelUi();
  void updateBeginnerGuide();
  void updateOutputPackageGuide();
  void updateContainerCompatibility();
  QStringList selectedMultiChannelChannels() const;
  void setSelectedMultiChannelChannels(const QStringList& channels);
 static QString normalizeBackend(const QString& backend);
 static QString normalizeRenderBackend(const QString& backend);
 };

 ArtifactRenderOutputSettingDialog::Impl::Impl()
 {

 }

 ArtifactRenderOutputSettingDialog::Impl::~Impl()
 {

 }

 void ArtifactRenderOutputSettingDialog::Impl::handleBrowseClicked(ArtifactRenderOutputSettingDialog* dialog)
 {
      QString filePath = QFileDialog::getSaveFileName(
          dialog,
          "Select Output File",
          "",
          "Video (*.mp4 *.mov *.avi *.mkv *.webm);;Image Sequence (*.png *.jpg *.tiff *.bmp *.exr);;All Files (*.*)"
      );
     if (!filePath.isEmpty()) {
         outputPathEdit->setText(filePath);
     }
 }

 void ArtifactRenderOutputSettingDialog::Impl::syncResolutionEditors()
 {
   if (!resolutionCombo || !widthSpin || !heightSpin) {
     return;
   }
   const QString preset = resolutionCombo->currentText();
   if (preset == "3840 x 2160") {
     widthSpin->setValue(3840);
     heightSpin->setValue(2160);
     widthSpin->setEnabled(false);
     heightSpin->setEnabled(false);
     return;
   }
   if (preset == "1920 x 1080") {
     widthSpin->setValue(1920);
     heightSpin->setValue(1080);
     widthSpin->setEnabled(false);
     heightSpin->setEnabled(false);
     return;
   }
   if (preset == "1280 x 720") {
     widthSpin->setValue(1280);
     heightSpin->setValue(720);
     widthSpin->setEnabled(false);
     heightSpin->setEnabled(false);
     return;
   }
   widthSpin->setEnabled(true);
   heightSpin->setEnabled(true);
 }

 void ArtifactRenderOutputSettingDialog::Impl::syncResolutionPreset()
 {
   if (!resolutionCombo || !widthSpin || !heightSpin) {
     return;
   }
   QString preset = "Custom";
   if (widthSpin->value() == 3840 && heightSpin->value() == 2160) {
     preset = "3840 x 2160";
   } else if (widthSpin->value() == 1920 && heightSpin->value() == 1080) {
     preset = "1920 x 1080";
   } else if (widthSpin->value() == 1280 && heightSpin->value() == 720) {
     preset = "1280 x 720";
   }
   const QSignalBlocker blocker(resolutionCombo);
   const int index = resolutionCombo->findText(preset);
   resolutionCombo->setCurrentIndex(index >= 0 ? index : resolutionCombo->findText("Custom"));
   widthSpin->setEnabled(preset == "Custom");
   heightSpin->setEnabled(preset == "Custom");
 }

 void ArtifactRenderOutputSettingDialog::Impl::updateBackendInfo()
 {
   if (!backendInfoLabel) {
     return;
   }

  const QStringList codecs = ArtifactCore::FFmpegEncoder::availableVideoCodecs();
  QStringList visibleCodecs;
   for (const QString& codec : codecs) {
     const QString normalized = codec.trimmed().toLower();
     if (normalized == QStringLiteral("h264") || normalized == QStringLiteral("h265")
         || normalized == QStringLiteral("hevc") || normalized == QStringLiteral("prores")
         || normalized == QStringLiteral("vp9") || normalized == QStringLiteral("dnxhd")
         || normalized == QStringLiteral("mjpeg")) {
       visibleCodecs << codec;
     }
   }
   if (visibleCodecs.isEmpty()) {
     visibleCodecs = codecs.mid(0, 6);
   }

   const QString ffmpegPath = resolveFfmpegExePath();
   const bool nvencH264 = ffmpegExeSupportsEncoder(ffmpegPath, QStringLiteral("h264_nvenc"));
   const bool nvencHevc = ffmpegExeSupportsEncoder(ffmpegPath, QStringLiteral("hevc_nvenc"));
   const bool vulkanH264 = ffmpegExeSupportsEncoder(ffmpegPath, QStringLiteral("h264_vulkan"));
   const bool vulkanHevc = ffmpegExeSupportsEncoder(ffmpegPath, QStringLiteral("hevc_vulkan"));

   QStringList lines;
   lines << QStringLiteral("Hardware FFmpeg: %1")
               .arg((nvencH264 || nvencHevc) ? QStringLiteral("NVENC available") : QStringLiteral("NVENC unavailable, will fallback"));
   lines << QStringLiteral("Vulkan encode: %1")
               .arg((vulkanH264 || vulkanHevc) ? QStringLiteral("available") : QStringLiteral("unavailable"));
   lines << QStringLiteral("Fallback order: pipe-hw -> pipe-vulkan -> native -> pipe");
   if (visibleCodecs.isEmpty()) {
     lines << QStringLiteral("FFmpeg codecs: unavailable");
   } else {
     lines << QStringLiteral("FFmpeg codecs: %1").arg(visibleCodecs.join(QStringLiteral(", ")));
   }
   backendInfoLabel->setText(lines.join(QStringLiteral(" | ")));
   backendInfoLabel->setToolTip(QStringLiteral("pipe-hw tries NVENC through FFmpeg.exe. pipe-vulkan tries h264_vulkan/hevc_vulkan. If they fail, Render Queue falls back in order to native FFmpeg, then pipe."));
 }

 QString ArtifactRenderOutputSettingDialog::Impl::resolveFfmpegExePath()
 {
   const QString executableName = QStringLiteral("ffmpeg.exe");
   const QString executableStem = QStringLiteral("ffmpeg");
   const QString appDir = QCoreApplication::applicationDirPath();
   const QString currentDir = QDir::currentPath();
   const QStringList candidatePaths = {
     QDir(appDir).filePath(executableName),
     QDir(appDir).filePath(executableStem),
     QDir(appDir).filePath(QStringLiteral("bin/") + executableName),
     QDir(appDir).filePath(QStringLiteral("bin/") + executableStem),
     QDir(currentDir).filePath(executableName),
     QDir(currentDir).filePath(executableStem),
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

 bool ArtifactRenderOutputSettingDialog::Impl::ffmpegExeSupportsEncoder(const QString& ffmpegPath, const QString& encoderName)
 {
   if (ffmpegPath.trimmed().isEmpty() || encoderName.trimmed().isEmpty()) {
     return false;
   }

   QProcess probe;
   probe.setProcessChannelMode(QProcess::MergedChannels);
   probe.start(ffmpegPath, {QStringLiteral("-hide_banner"), QStringLiteral("-encoders")});
   if (!probe.waitForFinished(5000)) {
     probe.kill();
     probe.waitForFinished(1000);
     return false;
   }
   const QString output = QString::fromUtf8(probe.readAllStandardOutput());
   return output.contains(encoderName, Qt::CaseInsensitive);
 }

 void ArtifactRenderOutputSettingDialog::Impl::ensureComboContains(QComboBox* combo, const QString& value)
 {
   if (!combo || value.trimmed().isEmpty()) {
     return;
   }
   if (combo->findText(value) < 0) {
     combo->addItem(value);
   }
 }

 void ArtifactRenderOutputSettingDialog::Impl::loadFormatPresets()
 {
   if (!presetCombo) return;
   
   presetCombo->clear();
   presetCombo->addItem(QStringLiteral("─ プリセットを選択 ─"), QString());
   presetCombo->addItem(QStringLiteral("再生・配布用 — MP4 / H.264"), QStringLiteral("guide.playback"));
   presetCombo->addItem(QStringLiteral("編集用中間素材 — ProRes 422"), QStringLiteral("guide.intermediate"));
   presetCombo->addItem(QStringLiteral("透過つき編集素材 — ProRes 4444"), QStringLiteral("guide.alpha"));
   presetCombo->addItem(QStringLiteral("連番素材 — PNG Sequence"), QStringLiteral("guide.sequence"));
   presetCombo->insertSeparator(presetCombo->count());
   
   const auto presets = ArtifactRenderFormatPresetManager::instance().allPresets();
  for (const auto& preset : presets) {
     presetCombo->addItem(
       QStringLiteral("%1 (%2/%3)").arg(preset.name, preset.container, preset.codec),
       preset.id);
   }
 }

 void ArtifactRenderOutputSettingDialog::Impl::updatePresetSummary()
 {
   if (!presetSummaryLabel) {
     return;
   }

  const QString presetName = presetCombo ? presetCombo->currentText() : QStringLiteral("未選択");
  const QString container = formatCombo ? formatCombo->currentText() : QStringLiteral("MP4");
  const QString codec = codecCombo ? codecCombo->currentText() : QStringLiteral("H.264");
  const bool audioEnabled = includeAudioCheck ? includeAudioCheck->isChecked() : false;
   const bool alphaEnabled = alphaEnabledCheck ? alphaEnabledCheck->isChecked()
       : (container != QStringLiteral("MP4") || codec == QStringLiteral("ProRes") || codec == QStringLiteral("VP9"));

  const QString alphaText = alphaEnabled ? QStringLiteral("Alphaあり") : QStringLiteral("Alphaなし");
  const bool isWebPlayer = container == QStringLiteral("HTML") || codec == QStringLiteral("CSS");
  const QString usageText = isWebPlayer
      ? QStringLiteral("Web player")
      : (container == QStringLiteral("WebM") || codec == QStringLiteral("VP9")
             ? QStringLiteral("Web video")
             : (container == QStringLiteral("PNG Sequence")
                    ? QStringLiteral("Image sequence")
                    : QStringLiteral("Standard export")));
   QStringList lines;
   lines << QStringLiteral("<span style='font-size:14px;font-weight:600;'>%1</span>").arg(presetName.isEmpty() ? QStringLiteral("未選択") : presetName);
   lines << QStringLiteral("<span>Usage: %1</span>").arg(usageText);
   lines << QStringLiteral("<span>Container: %1</span>").arg(container);
   lines << QStringLiteral("<span>Codec: %1</span>").arg(codec);
   lines << QStringLiteral("<span>%1</span>").arg(alphaText);
   lines << QStringLiteral("<span>Audio: %1</span>").arg(audioEnabled ? QStringLiteral("あり") : QStringLiteral("なし"));
   presetSummaryLabel->setText(QStringLiteral("<div style='line-height:1.5;'>%1</div>").arg(lines.join(QStringLiteral("<br/>"))));
 }

 void ArtifactRenderOutputSettingDialog::Impl::updateFormatGuide()
 {
   if (!formatGuideLabel) {
     return;
   }

  const QString format = formatCombo ? formatCombo->currentText() : QStringLiteral("MP4");
  const QString codec = codecCombo ? codecCombo->currentText() : QStringLiteral("H.264");
  const bool alphaEnabled = alphaEnabledCheck ? alphaEnabledCheck->isChecked() : true;

  QString guide;
  if (format == QStringLiteral("HTML") || codec == QStringLiteral("CSS")) {
    guide = QStringLiteral("Web向け。ブラウザで直接開ける self-contained player。");
  } else if (format == QStringLiteral("WebM") || codec == QStringLiteral("VP9")) {
    guide = QStringLiteral("Web向け。透過を扱いやすい。");
   } else if (format == QStringLiteral("MOV") && codec == QStringLiteral("ProRes")) {
     guide = QStringLiteral("編集向け。ProRes 4444 なら透過を残しやすい。");
   } else if (format == QStringLiteral("PNG Sequence")) {
     guide = QStringLiteral("静止画連番。1枚ずつ透明を保てる。");
   } else if (format == QStringLiteral("MP4")) {
     guide = QStringLiteral("配布向け。迷ったら H.264 + AAC が無難。");
  } else {
    guide = QStringLiteral("用途に応じてコンテナとコーデックを選ぶ。");
  }
  if (!alphaEnabled) {
    guide = QStringLiteral("Alphaなし。透過は書き出されません。");
  }
  formatGuideLabel->setText(guide);
  updateBeginnerGuide();
 }

 void setGuideFrameState(QFrame* frame, bool selected, bool warning = false)
 {
   if (!frame) {
     return;
   }
   QPalette palette = frame->palette();
   const QColor base = palette.color(QPalette::Window);
   palette.setColor(QPalette::Window, selected ? QColor(QStringLiteral("#263A53")) : base);
   palette.setColor(QPalette::WindowText,
                    warning ? QColor(QStringLiteral("#F09A3E"))
                            : (selected ? QColor(QStringLiteral("#78AFFF"))
                                        : palette.color(QPalette::WindowText)));
   frame->setPalette(palette);
   frame->setAutoFillBackground(selected);
   frame->setFrameShadow(selected ? QFrame::Raised : QFrame::Plain);
 }

 void ArtifactRenderOutputSettingDialog::Impl::updateBeginnerGuide()
 {
   const QString container = formatCombo ? formatCombo->currentText() : QStringLiteral("MP4");
   const QString codec = codecCombo ? codecCombo->currentText() : QStringLiteral("H.264");
   const bool alphaEnabled = alphaEnabledCheck && alphaEnabledCheck->isChecked();
   const bool intermediate = container == QStringLiteral("MOV")
       && (codec == QStringLiteral("ProRes") || codec == QStringLiteral("DNxHD"));

   setGuideFrameState(playbackGuideFrame, !intermediate);
   setGuideFrameState(intermediateGuideFrame, intermediate);
   setGuideFrameState(noAlphaGuideFrame, !alphaEnabled);
   setGuideFrameState(alphaGuideFrame, alphaEnabled);
   setGuideFrameState(straightGuideFrame, alphaEnabled);
   setGuideFrameState(premultipliedGuideFrame, false, true);

   if (recommendationLabel) {
     if (!intermediate) {
       recommendationLabel->setText(
           QStringLiteral("おすすめ: 再生・配布用 / MP4 / H.264 / 透過なし"));
     } else if (alphaEnabled) {
       recommendationLabel->setText(
           QStringLiteral("おすすめ: 編集用中間素材 / ProRes 4444 / Straight"));
     } else {
       recommendationLabel->setText(
           QStringLiteral("おすすめ: 編集用中間素材 / ProRes 422 / 透過なし"));
     }
   }
 }

 void ArtifactRenderOutputSettingDialog::Impl::updateOutputPackageGuide()
 {
   if (!outputPackageLabel || !formatCombo || !includeAudioCheck) {
     return;
   }

   const QString format = formatCombo->currentText();
   const bool imageSequence = format.endsWith(QStringLiteral(" Sequence"));
   const bool supportsIntegratedAudio =
       format == QStringLiteral("MP4") || format == QStringLiteral("MOV")
       || format == QStringLiteral("WebM") || format == QStringLiteral("MKV")
       || format == QStringLiteral("AVI");
   if (!supportsIntegratedAudio && includeAudioCheck->isChecked()) {
     const QSignalBlocker blocker(includeAudioCheck);
     includeAudioCheck->setChecked(false);
   }
   includeAudioCheck->setEnabled(supportsIntegratedAudio);

   QPalette palette = outputPackageLabel->palette();
   if (imageSequence) {
     outputPackageLabel->setText(QStringLiteral(
         "出力パッケージ: 画像連番のみ。連番には音声を格納できません。"
         "音声が必要な場合は別途WAVなどを用意してください（音声別出力の自動生成は未対応）。"));
     palette.setColor(QPalette::WindowText, QColor(QStringLiteral("#F09A3E")));
   } else if (!supportsIntegratedAudio) {
     outputPackageLabel->setText(QStringLiteral(
         "出力パッケージ: この形式では音声同梱を利用できません。"));
     palette.setColor(QPalette::WindowText, QColor(QStringLiteral("#F09A3E")));
   } else if (includeAudioCheck->isChecked()) {
     outputPackageLabel->setText(QStringLiteral(
         "出力パッケージ: 動画1ファイル（映像＋音声を同梱）"));
     palette.setColor(QPalette::WindowText, QColor(QStringLiteral("#78AFFF")));
   } else {
     outputPackageLabel->setText(QStringLiteral(
         "出力パッケージ: 動画1ファイル（映像のみ）"));
     palette.setColor(QPalette::WindowText, palette.color(QPalette::Text));
   }
   outputPackageLabel->setPalette(palette);

   if (audioCodecCombo) audioCodecCombo->setEnabled(supportsIntegratedAudio && includeAudioCheck->isChecked());
   if (audioChannelCombo) audioChannelCombo->setEnabled(supportsIntegratedAudio && includeAudioCheck->isChecked());
   if (audioSampleRateCombo) audioSampleRateCombo->setEnabled(supportsIntegratedAudio && includeAudioCheck->isChecked());
   if (audioBitrateSpin) {
     const bool losslessPcm = audioCodecCombo
         && audioCodecCombo->currentText() == QStringLiteral("PCM 24-bit");
     audioBitrateSpin->setEnabled(
         supportsIntegratedAudio && includeAudioCheck->isChecked() && !losslessPcm);
     audioBitrateSpin->setToolTip(losslessPcm
         ? QStringLiteral("PCMは非圧縮のためビットレート指定を使用しません。")
         : QString());
   }
 }

 void ArtifactRenderOutputSettingDialog::Impl::updateContainerCompatibility()
 {
   if (!formatCombo || !codecCombo || !audioCodecCombo) {
     return;
   }

   const QString format = formatCombo->currentText();
   QStringList videoCodecs;
   QStringList audioCodecs;
   QString preferredVideo;
   QString preferredAudio;

   if (format == QStringLiteral("MP4")) {
     videoCodecs = {QStringLiteral("H.264"), QStringLiteral("H.265")};
     audioCodecs = {QStringLiteral("AAC")};
     preferredVideo = QStringLiteral("H.264");
     preferredAudio = QStringLiteral("AAC");
   } else if (format == QStringLiteral("MOV")) {
     videoCodecs = {QStringLiteral("ProRes"), QStringLiteral("DNxHD"),
                    QStringLiteral("H.264"), QStringLiteral("H.265")};
     audioCodecs = {QStringLiteral("PCM 24-bit")};
     preferredVideo = QStringLiteral("ProRes");
     preferredAudio = QStringLiteral("PCM 24-bit");
   } else if (format == QStringLiteral("WebM")) {
     videoCodecs = {QStringLiteral("VP9")};
     audioCodecs = {QStringLiteral("Opus"), QStringLiteral("Vorbis")};
     preferredVideo = QStringLiteral("VP9");
     preferredAudio = QStringLiteral("Opus");
   } else if (format == QStringLiteral("MKV")) {
     videoCodecs = {QStringLiteral("H.264"), QStringLiteral("H.265"), QStringLiteral("VP9")};
     audioCodecs = {QStringLiteral("AAC"), QStringLiteral("Opus"), QStringLiteral("FLAC")};
     preferredVideo = QStringLiteral("H.264");
     preferredAudio = QStringLiteral("AAC");
   } else if (format == QStringLiteral("AVI")) {
     videoCodecs = {QStringLiteral("rawvideo"), QStringLiteral("H.264")};
     audioCodecs = {QStringLiteral("PCM 24-bit")};
     preferredVideo = QStringLiteral("rawvideo");
     preferredAudio = QStringLiteral("PCM 24-bit");
   } else if (format == QStringLiteral("WMV")) {
     videoCodecs = {QStringLiteral("wmv2")};
     preferredVideo = QStringLiteral("wmv2");
   } else if (format == QStringLiteral("GIF")) {
     videoCodecs = {QStringLiteral("gif")};
     preferredVideo = QStringLiteral("gif");
   } else if (format == QStringLiteral("APNG")) {
     videoCodecs = {QStringLiteral("apng")};
     preferredVideo = QStringLiteral("apng");
   } else if (format == QStringLiteral("WEBP")) {
     videoCodecs = {QStringLiteral("webp")};
     preferredVideo = QStringLiteral("webp");
   } else if (format == QStringLiteral("HTML")) {
     videoCodecs = {QStringLiteral("CSS / HTML player")};
     preferredVideo = videoCodecs.front();
   } else {
     QString imageCodec = format;
     imageCodec.remove(QStringLiteral(" Sequence"));
     videoCodecs = {imageCodec};
     preferredVideo = imageCodec;
   }

   {
     const QSignalBlocker videoBlocker(codecCombo);
     codecCombo->clear();
     codecCombo->addItems(videoCodecs);
     codecCombo->setCurrentText(preferredVideo);
   }
   {
     const QSignalBlocker audioBlocker(audioCodecCombo);
     audioCodecCombo->clear();
     audioCodecCombo->addItems(audioCodecs);
     if (!audioCodecs.isEmpty()) {
       audioCodecCombo->setCurrentText(preferredAudio);
     }
   }
   if (codecCombo->currentText() == QStringLiteral("ProRes") && codecProfile.isEmpty()) {
     codecProfile = QStringLiteral("hq");
   } else if (codecCombo->currentText() != QStringLiteral("ProRes")) {
     codecProfile.clear();
   }
   if (audioChannelCombo) {
     audioChannelCombo->setCurrentIndex(
         audioChannelCombo->findData(format == QStringLiteral("MOV")
                                         ? QStringLiteral("source")
                                         : QStringLiteral("stereo")));
   }
   if (audioSampleRateCombo) {
     audioSampleRateCombo->setCurrentIndex(audioSampleRateCombo->findData(48000));
   }
   updateOutputPackageGuide();
 }

 void ArtifactRenderOutputSettingDialog::Impl::updateAdvancedSummary()
 {
   const QString container = formatCombo ? formatCombo->currentText() : QStringLiteral("MP4");
   const QString codec = codecCombo ? codecCombo->currentText() : QStringLiteral("H.264");
   const QString backend = backendCombo ? backendCombo->currentText() : QStringLiteral("auto");
   const bool audioEnabled = includeAudioCheck ? includeAudioCheck->isChecked() : false;
   const bool alphaEnabled = alphaEnabledCheck ? alphaEnabledCheck->isChecked() : true;
   const QString fpsText = fpsSpin ? QString::number(fpsSpin->value(), 'f', 3) : QStringLiteral("30.000");
   const QString bitrateText = bitrateSpin ? QString::number(bitrateSpin->value()) + QStringLiteral(" kbps") : QStringLiteral("8000 kbps");

   if (advancedAlphaLabel) {
     advancedAlphaLabel->setText(alphaEnabled ? QStringLiteral("Alpha: あり") : QStringLiteral("Alpha: なし"));
   }
   if (advancedFilenameLabel) {
     advancedFilenameLabel->setText(QStringLiteral("ProjectName_[Preset]_[Date] / %1").arg(container));
   }
   if (advancedTimecodeLabel) {
     advancedTimecodeLabel->setText(QStringLiteral("ソース準拠 / %1 fps").arg(fpsText));
   }
  if (advancedColorLabel) {
    advancedColorLabel->setText(QStringLiteral("%1 / Rec.709").arg(backend == QStringLiteral("gpu") ? QStringLiteral("GPU") : QStringLiteral("自動")));
  }
  if (advancedFilenameLabel) {
    const QString backendText = backend == QStringLiteral("auto")
        ? QStringLiteral("auto")
        : backend;
    advancedFilenameLabel->setText(QStringLiteral("ProjectName_[Preset]_[Date] / %1 / %2")
                                      .arg(container, backendText));
  }
  if (advancedEncodeLabel) {
    QString text = QStringLiteral("2-pass / HW 支援 / 連続書き出し");
     if (container == QStringLiteral("MOV") && codec == QStringLiteral("ProRes")) {
       text = QStringLiteral("ProRes 4444 推奨 / 透過対応");
     } else if (container == QStringLiteral("WebM") || codec == QStringLiteral("VP9")) {
       text = QStringLiteral("WebM / VP9 / 透過向け");
     } else if (container == QStringLiteral("MP4")) {
       text = QStringLiteral("H.264 + AAC / 配布向け / %1").arg(bitrateText);
     }
     advancedEncodeLabel->setText(text);
   }
   if (!audioEnabled && advancedEncodeLabel) {
     advancedEncodeLabel->setText(advancedEncodeLabel->text() + QStringLiteral(" / 音声なし"));
   }
 }

void ArtifactRenderOutputSettingDialog::Impl::updateActionLabels()
{
   if (!okButton) {
     return;
   }
   okButton->setText(QStringLiteral("この設定でレンダー"));
  if (cancelButton) {
    cancelButton->setText(QStringLiteral("キャンセル"));
  }
}

void ArtifactRenderOutputSettingDialog::Impl::updateFrameRatePreflight()
{
  if (!preflightSummaryLabel || !preflightDetailsLabel || !fpsSpin) {
    return;
  }
  const double outputFps = std::max(1.0, fpsSpin->value());
  const double compFps = compositionFrameRate > 0.0 ? compositionFrameRate : outputFps;
  if (std::abs(outputFps - compFps) > 0.01) {
    preflightSummaryLabel->setText(QStringLiteral("Preflight: frame rate mismatch"));
    preflightDetailsLabel->setText(
        QStringLiteral("Composition is %1 fps, output is %2 fps.")
            .arg(QString::number(compFps, 'f', 3))
            .arg(QString::number(outputFps, 'f', 3)));
  } else {
    preflightSummaryLabel->setText(QStringLiteral("Preflight: frame rate matches composition"));
    preflightDetailsLabel->setText(QStringLiteral("Output frame rate matches the active composition."));
  }
}

void ArtifactRenderOutputSettingDialog::Impl::updateMultiChannelUi()
{
  const bool enabled = multiChannelCheck && multiChannelCheck->isChecked();
  if (multiChannelGroup) {
    multiChannelGroup->setEnabled(enabled);
  }
  if (enabled && formatCombo) {
    formatCombo->setCurrentText(QStringLiteral("EXR Sequence"));
  }
  if (enabled && codecCombo) {
    codecCombo->setCurrentText(QStringLiteral("EXR"));
  }
}

QStringList ArtifactRenderOutputSettingDialog::Impl::selectedMultiChannelChannels() const
{
  QStringList channels;
  if (beautyChannelCheck && beautyChannelCheck->isChecked()) {
    channels << QStringLiteral("R") << QStringLiteral("G") << QStringLiteral("B");
  }
  if (alphaChannelCheck && alphaChannelCheck->isChecked()) {
    channels << QStringLiteral("A");
  }
  if (depthChannelCheck && depthChannelCheck->isChecked()) {
    channels << QStringLiteral("Depth");
  }
  if (normalChannelCheck && normalChannelCheck->isChecked()) {
    channels << QStringLiteral("Normal.X") << QStringLiteral("Normal.Y")
             << QStringLiteral("Normal.Z");
  }
  if (velocityChannelCheck && velocityChannelCheck->isChecked()) {
    channels << QStringLiteral("Velocity.X") << QStringLiteral("Velocity.Y");
  }
  if (objectIdChannelCheck && objectIdChannelCheck->isChecked()) {
    channels << QStringLiteral("ObjectId");
  }
  if (materialIdChannelCheck && materialIdChannelCheck->isChecked()) {
    channels << QStringLiteral("MaterialId");
  }
  if (albedoChannelCheck && albedoChannelCheck->isChecked()) {
    channels << QStringLiteral("Albedo.R") << QStringLiteral("Albedo.G")
             << QStringLiteral("Albedo.B");
  }
  if (emissionChannelCheck && emissionChannelCheck->isChecked()) {
    channels << QStringLiteral("Emission");
  }
  return channels;
}

void ArtifactRenderOutputSettingDialog::Impl::setSelectedMultiChannelChannels(const QStringList& channels)
{
  const auto hasAny = [&](const QStringList& names) {
    for (const auto& name : names) {
      if (channels.contains(name, Qt::CaseInsensitive)) {
        return true;
      }
    }
    return false;
  };
  if (beautyChannelCheck) {
    beautyChannelCheck->setChecked(hasAny({QStringLiteral("R"), QStringLiteral("G"), QStringLiteral("B")}));
  }
  if (alphaChannelCheck) {
    alphaChannelCheck->setChecked(hasAny({QStringLiteral("A"), QStringLiteral("Alpha")}));
  }
  if (depthChannelCheck) {
    depthChannelCheck->setChecked(hasAny({QStringLiteral("Depth")}));
  }
  if (normalChannelCheck) {
    normalChannelCheck->setChecked(hasAny({QStringLiteral("Normal.X"), QStringLiteral("Normal.Y"), QStringLiteral("Normal.Z")}));
  }
  if (velocityChannelCheck) {
    velocityChannelCheck->setChecked(hasAny({QStringLiteral("Velocity.X"), QStringLiteral("Velocity.Y")}));
  }
  if (objectIdChannelCheck) {
    objectIdChannelCheck->setChecked(hasAny({QStringLiteral("ObjectId")}));
  }
  if (materialIdChannelCheck) {
    materialIdChannelCheck->setChecked(hasAny({QStringLiteral("MaterialId")}));
  }
  if (albedoChannelCheck) {
    albedoChannelCheck->setChecked(hasAny({QStringLiteral("Albedo.R"), QStringLiteral("Albedo.G"), QStringLiteral("Albedo.B")}));
  }
  if (emissionChannelCheck) {
    emissionChannelCheck->setChecked(hasAny({QStringLiteral("Emission")}));
  }
}

 void updateAlphaUi(QCheckBox* alphaEnabledCheck, QLabel* formatGuideLabel, const QString& container, const QString& codec)
 {
   if (!alphaEnabledCheck || !formatGuideLabel) {
     return;
   }
   const bool alphaEnabled = alphaEnabledCheck->isChecked();
   QString alphaText;
   if (!alphaEnabled) {
     alphaText = QStringLiteral("この設定では透過は書き出されません");
   } else if (container == QStringLiteral("MOV") && codec == QStringLiteral("ProRes")) {
     alphaText = QStringLiteral("透過を保持します");
   } else if (container == QStringLiteral("WebM") || codec == QStringLiteral("VP9")) {
     alphaText = QStringLiteral("透過を保持します");
   } else {
     alphaText = QStringLiteral("透過設定を確認してください");
   }
   formatGuideLabel->setText(alphaText);
 }

 void updateAlphaPreflight(QCheckBox* alphaEnabledCheck, QLabel* preflightSummaryLabel, QLabel* preflightDetailsLabel, const QString& container, const QString& codec)
 {
   if (!alphaEnabledCheck || !preflightSummaryLabel || !preflightDetailsLabel) {
     return;
   }

   const bool alphaEnabled = alphaEnabledCheck->isChecked();
   if (!alphaEnabled) {
     preflightSummaryLabel->setText(QStringLiteral("Preflight: Alpha は無効です"));
     preflightDetailsLabel->setText(QStringLiteral("この設定では透過は書き出されません。"));
     return;
   }

   if (container == QStringLiteral("MOV") && codec == QStringLiteral("ProRes")) {
     preflightSummaryLabel->setText(QStringLiteral("Preflight: 透過対応"));
     preflightDetailsLabel->setText(QStringLiteral("ProRes 4444 系の透過設定を確認できます。"));
   } else if (container == QStringLiteral("WebM") || codec == QStringLiteral("VP9")) {
     preflightSummaryLabel->setText(QStringLiteral("Preflight: 透過対応"));
     preflightDetailsLabel->setText(QStringLiteral("WebM / VP9 は透過向けの候補です。"));
   } else {
     preflightSummaryLabel->setText(QStringLiteral("Preflight: 設定確認"));
     preflightDetailsLabel->setText(QStringLiteral("Alpha が必要なら、コンテナとコーデックを確認してください。"));
   }
 }

  void ArtifactRenderOutputSettingDialog::Impl::applyPresetToEditors(const QString& presetId)
  {
   if (presetId.isEmpty() || !formatCombo || !codecCombo) return;

   if (presetId == QStringLiteral("guide.playback")) {
     formatCombo->setCurrentText(QStringLiteral("MP4"));
     codecCombo->setCurrentText(QStringLiteral("H.264"));
     codecProfile.clear();
     if (alphaEnabledCheck) alphaEnabledCheck->setChecked(false);
     if (includeAudioCheck) includeAudioCheck->setChecked(true);
     return;
   }
   if (presetId == QStringLiteral("guide.intermediate")) {
     formatCombo->setCurrentText(QStringLiteral("MOV"));
     codecCombo->setCurrentText(QStringLiteral("ProRes"));
     codecProfile = QStringLiteral("hq");
     if (alphaEnabledCheck) alphaEnabledCheck->setChecked(false);
     return;
   }
   if (presetId == QStringLiteral("guide.alpha")) {
     formatCombo->setCurrentText(QStringLiteral("MOV"));
     codecCombo->setCurrentText(QStringLiteral("ProRes"));
     codecProfile = QStringLiteral("4444");
     if (alphaEnabledCheck) alphaEnabledCheck->setChecked(true);
     return;
   }
   if (presetId == QStringLiteral("guide.sequence")) {
     formatCombo->setCurrentText(QStringLiteral("PNG Sequence"));
     codecCombo->setCurrentText(QStringLiteral("PNG"));
     codecProfile.clear();
     if (alphaEnabledCheck) alphaEnabledCheck->setChecked(true);
     if (includeAudioCheck) includeAudioCheck->setChecked(false);
     return;
   }
   
   const auto* preset = ArtifactRenderFormatPresetManager::instance().findPresetById(presetId);
   if (!preset) return;
   
   // Format
    const QString presetContainer = preset->container.toLower();
    QString formatText;
    if (presetContainer == "png") formatText = "PNG Sequence";
    else if (presetContainer == "jpg" || presetContainer == "jpeg") formatText = "JPEG Sequence";
    else if (presetContainer == "tiff" || presetContainer == "tif") formatText = "TIFF Sequence";
    else if (presetContainer == "bmp") formatText = "BMP Sequence";
    else if (presetContainer == "exr") formatText = "EXR Sequence";
    else if (presetContainer == "html") formatText = "HTML";
    else formatText = preset->container.toUpper();

    const int formatIndex = formatCombo->findText(formatText);
    if (formatIndex >= 0) {
     formatCombo->setCurrentIndex(formatIndex);
    } else {
     formatCombo->addItem(formatText);
     formatCombo->setCurrentText(formatText);
    }

    // Codec
    const QString presetCodec = preset->codec.toLower();
    QString codecText;
   if (presetCodec == "h264") codecText = "H.264";
    else if (presetCodec == "h265") codecText = "H.265";
    else if (presetCodec == "prores") codecText = "ProRes";
    else if (presetCodec == "css") codecText = "CSS / HTML player";
    else if (presetCodec == "mjpeg" || presetCodec == "jpeg") codecText = "JPEG";
    else codecText = preset->codec;
    codecProfile = preset->codecProfile;

    const int codecIndex = codecCombo->findText(codecText);
    if (codecIndex >= 0) {
     codecCombo->setCurrentIndex(codecIndex);
    } else {
     codecCombo->addItem(codecText);
     codecCombo->setCurrentText(codecText);
    }

   // 拡張子を自動更新
   if (outputPathEdit) {
    QString path = outputPathEdit->text().trimmed();
    if (!path.isEmpty()) {
     QString ext;
     if (preset->container.toLower() == QStringLiteral("html") || preset->codec.toLower() == QStringLiteral("css")) {
      ext = QStringLiteral("html");
     }
     if (preset->container.toLower() == QStringLiteral("html")) {
      ext = QStringLiteral("html");
     } else if (preset->isImageSequence) {
      ext = preset->container.toLower();
      } else if (preset->codec.toLower() == QStringLiteral("prores")) {
       ext = QStringLiteral("mov");
      } else {
       ext = preset->container.toLower();
     }
     QFileInfo info(path);
     QString newPath = info.absolutePath() + "/" + info.completeBaseName() + "." + ext;
     outputPathEdit->setText(newPath);
    }
   }
  }

QString ArtifactRenderOutputSettingDialog::Impl::normalizeBackend(const QString& backend)
{
  const QString value = backend.trimmed().toLower();
  if (value == QStringLiteral("pipe") || value == QStringLiteral("ffmpeg.exe") || value == QStringLiteral("ffmpeg")) {
     return QStringLiteral("pipe");
   }
  if (value == QStringLiteral("pipe-hw") || value == QStringLiteral("pipe-hw (nvenc)")
      || value == QStringLiteral("ffmpeg-hw") || value == QStringLiteral("hardware")
      || value == QStringLiteral("hw")) {
    return QStringLiteral("pipe-hw");
  }
  if (value == QStringLiteral("native") || value == QStringLiteral("api") || value == QStringLiteral("ffmpegapi")) {
    return QStringLiteral("native");
  }
  if (value == QStringLiteral("gpu")) {
    return QStringLiteral("gpu");
  }
  return QStringLiteral("auto");
}

QString ArtifactRenderOutputSettingDialog::Impl::normalizeRenderBackend(const QString& backend)
{
  const QString value = backend.trimmed().toLower();
  if (value == QStringLiteral("gpu") || value == QStringLiteral("diligent") || value == QStringLiteral("hardware")) {
    return QStringLiteral("gpu");
  }
  if (value == QStringLiteral("cpu") || value == QStringLiteral("software") || value == QStringLiteral("qpainter")) {
    return QStringLiteral("cpu");
  }
  if (value == QStringLiteral("external-cycles") || value == QStringLiteral("blender-cycles")) {
    return QStringLiteral("external-cycles");
  }
  if (value == QStringLiteral("external") || value == QStringLiteral("process") || value == QStringLiteral("outofprocess")) {
    return QStringLiteral("external");
  }
  return QStringLiteral("auto");
}
	
	W_OBJECT_IMPL(ArtifactRenderOutputSettingDialog)
	
 ArtifactRenderOutputSettingDialog::ArtifactRenderOutputSettingDialog(QWidget* parent /*= nullptr*/):QDialog(parent),impl_(new Impl())
 {
    setWindowTitle(QStringLiteral("レンダー出力の設定"));
    setMinimumWidth(920);

    auto mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(14, 14, 14, 14);
    mainLayout->setSpacing(10);
    auto formLayout = new QFormLayout();

    const auto createGuideFrame = [this](const QString& title, const QString& detail) {
      auto* frame = new QFrame(this);
      frame->setFrameShape(QFrame::StyledPanel);
      frame->setFrameShadow(QFrame::Plain);
      auto* layout = new QVBoxLayout(frame);
      layout->setContentsMargins(10, 8, 10, 8);
      layout->setSpacing(3);
      auto* titleLabel = new QLabel(title, frame);
      QFont titleFont = titleLabel->font();
      titleFont.setBold(true);
      titleLabel->setFont(titleFont);
      auto* detailLabel = new QLabel(detail, frame);
      detailLabel->setWordWrap(true);
      layout->addWidget(titleLabel);
      layout->addWidget(detailLabel);
      return frame;
    };

    auto* beginnerGuide = new QFrame(this);
    beginnerGuide->setFrameShape(QFrame::StyledPanel);
    beginnerGuide->setFrameShadow(QFrame::Raised);
    auto* beginnerLayout = new QHBoxLayout(beginnerGuide);
    beginnerLayout->setContentsMargins(10, 10, 10, 10);
    beginnerLayout->setSpacing(10);

    auto* purposeGroup = new QGroupBox(QStringLiteral("1  何に使いますか？"), beginnerGuide);
    auto* purposeLayout = new QHBoxLayout(purposeGroup);
    impl_->playbackGuideFrame = createGuideFrame(
        QStringLiteral("再生・配布用"), QStringLiteral("MP4 / H.264\n軽い・すぐ見られる"));
    impl_->intermediateGuideFrame = createGuideFrame(
        QStringLiteral("編集用の中間素材"), QStringLiteral("ProRes / DNxHD\n高品質・再編集向け"));
    purposeLayout->addWidget(impl_->playbackGuideFrame);
    purposeLayout->addWidget(impl_->intermediateGuideFrame);

    auto* alphaGroup = new QGroupBox(QStringLiteral("2  透明を残しますか？"), beginnerGuide);
    auto* guideAlphaLayout = new QHBoxLayout(alphaGroup);
    impl_->noAlphaGuideFrame = createGuideFrame(
        QStringLiteral("不要"), QStringLiteral("通常の動画・配布向け"));
    impl_->alphaGuideFrame = createGuideFrame(
        QStringLiteral("必要"), QStringLiteral("ProRes 4444 / PNG連番"));
    guideAlphaLayout->addWidget(impl_->noAlphaGuideFrame);
    guideAlphaLayout->addWidget(impl_->alphaGuideFrame);

    auto* alphaModeGroup = new QGroupBox(QStringLiteral("3  透明の計算方法"), beginnerGuide);
    auto* alphaModeLayout = new QHBoxLayout(alphaModeGroup);
    impl_->straightGuideFrame = createGuideFrame(
        QStringLiteral("Straight"), QStringLiteral("通常はこちら\n色と透明度を別に保持"));
    impl_->premultipliedGuideFrame = createGuideFrame(
        QStringLiteral("Premultiplied"), QStringLiteral("受け渡し先の指定時のみ\n誤ると輪郭にフチ"));
    alphaModeLayout->addWidget(impl_->straightGuideFrame);
    alphaModeLayout->addWidget(impl_->premultipliedGuideFrame);

    beginnerLayout->addWidget(purposeGroup, 2);
    beginnerLayout->addWidget(alphaGroup, 2);
    beginnerLayout->addWidget(alphaModeGroup, 2);

    impl_->recommendationLabel = new QLabel(
        QStringLiteral("おすすめ: 再生・配布用 / MP4 / H.264 / 透過なし"), this);
    QFont recommendationFont = impl_->recommendationLabel->font();
    recommendationFont.setBold(true);
    impl_->recommendationLabel->setFont(recommendationFont);
    QPalette recommendationPalette = impl_->recommendationLabel->palette();
    recommendationPalette.setColor(QPalette::WindowText, QColor(QStringLiteral("#78AFFF")));
    impl_->recommendationLabel->setPalette(recommendationPalette);

    auto* summaryFrame = new QFrame(this);
    summaryFrame->setFrameShape(QFrame::StyledPanel);
    summaryFrame->setFrameShadow(QFrame::Raised);
    auto* summaryLayout = new QHBoxLayout(summaryFrame);
    auto* summaryTitle = new QLabel(QStringLiteral("用途サマリ"), summaryFrame);
    impl_->presetSummaryLabel = new QLabel(QStringLiteral("未選択"), summaryFrame);
    impl_->presetSummaryLabel->setWordWrap(true);
    impl_->formatGuideLabel = new QLabel(QStringLiteral("用途を選ぶと、コンテナとコーデックの意味がここに表示されます。"), summaryFrame);
    impl_->formatGuideLabel->setWordWrap(true);
    impl_->presetSummaryLabel->setFrameShape(QFrame::NoFrame);
    impl_->formatGuideLabel->setFrameShape(QFrame::NoFrame);
    summaryLayout->addWidget(summaryTitle, 0);
    summaryLayout->addWidget(impl_->presetSummaryLabel, 1);
    summaryLayout->addWidget(impl_->formatGuideLabel, 2);

    impl_->alphaEnabledCheck = new QCheckBox(QStringLiteral("Alphaあり"), this);
    impl_->alphaEnabledCheck->setChecked(false);
    auto* alphaDisabledLabel = new QLabel(QStringLiteral("Alphaなし"), this);

    auto* alphaRow = new QHBoxLayout();
    alphaRow->addWidget(impl_->alphaEnabledCheck);
    alphaRow->addWidget(alphaDisabledLabel);
    alphaRow->addStretch();

    // Output path
    impl_->outputPathEdit = new QLineEdit();
    impl_->browseButton = new QPushButton("Browse...");

    auto pathLayout = new QHBoxLayout();
    pathLayout->addWidget(impl_->outputPathEdit);
    pathLayout->addWidget(impl_->browseButton);

    formLayout->addRow("Output To:", pathLayout);

    // Format preset selection (After Effects style)
    impl_->presetCombo = new QComboBox();
    impl_->loadFormatPresets();
    formLayout->addRow(QStringLiteral("用途プリセット:"), impl_->presetCombo);

    // Format selection
    impl_->formatCombo = new QComboBox();
    impl_->formatCombo->addItems(QStringList{
      "MP4", "MOV", "AVI", "WebM", "MKV",
      "PNG Sequence", "JPEG Sequence", "TIFF Sequence", "BMP Sequence", "EXR Sequence",
      "HTML"
    });
    formLayout->addRow("Container:", impl_->formatCombo);

    impl_->codecCombo = new QComboBox();
    impl_->codecCombo->addItems(QStringList{
      "H.264", "H.265", "ProRes", "VP9", "DNxHD",
      "PNG", "JPEG", "TIFF", "BMP", "EXR"
    });
    formLayout->addRow("Codec:", impl_->codecCombo);

    impl_->backendCombo = new QComboBox();
    impl_->backendCombo->addItems(QStringList{
      "auto", "pipe", "pipe-hw (NVENC)", "pipe-vulkan", "native", "gpu"
    });
    formLayout->addRow("Encoder Backend:", impl_->backendCombo);
    impl_->backendInfoLabel = new QLabel(this);
    impl_->backendInfoLabel->setWordWrap(true);
    formLayout->addRow(QString(), impl_->backendInfoLabel);
    impl_->updateBackendInfo();

    impl_->renderBackendCombo = new QComboBox();
    impl_->renderBackendCombo->addItems(QStringList{
      "auto", "cpu", "gpu", "external", "external-cycles"
    });
    formLayout->addRow("Render Backend:", impl_->renderBackendCombo);

    impl_->preflightSummaryLabel = new QLabel(QStringLiteral("Preflight: not checked yet"), this);
    impl_->preflightSummaryLabel->setTextFormat(Qt::PlainText);
    impl_->preflightSummaryLabel->setWordWrap(true);
    impl_->preflightDetailsLabel = new QLabel(QStringLiteral("Open this dialog from the render queue to see job-specific warnings and errors."), this);
    impl_->preflightDetailsLabel->setTextFormat(Qt::PlainText);
    impl_->preflightDetailsLabel->setWordWrap(true);
    formLayout->addRow("Preflight:", impl_->preflightSummaryLabel);
    formLayout->addRow(QString(), impl_->preflightDetailsLabel);

    // Resolution presets + custom width/height
    impl_->resolutionCombo = new QComboBox();
    impl_->resolutionCombo->addItem("3840 x 2160");
    impl_->resolutionCombo->addItem("1920 x 1080");
    impl_->resolutionCombo->addItem("1280 x 720");
    impl_->resolutionCombo->addItem("Custom");

    impl_->widthSpin = new ArtifactRelativeSpinBox();
    impl_->widthSpin->setRange(1, 16384);
    impl_->heightSpin = new ArtifactRelativeSpinBox();
    impl_->heightSpin->setRange(1, 16384);
    impl_->widthSpin->setValue(1920);
    impl_->heightSpin->setValue(1080);
    impl_->widthSpin->setEnabled(false);
    impl_->heightSpin->setEnabled(false);

    auto resLayout = new QHBoxLayout();
    resLayout->addWidget(impl_->resolutionCombo);
    resLayout->addWidget(new QLabel("W:"));
    resLayout->addWidget(impl_->widthSpin);
    resLayout->addWidget(new QLabel("H:"));
    resLayout->addWidget(impl_->heightSpin);
    formLayout->addRow("Resolution:", resLayout);

    // Frame rate selection
    impl_->fpsSpin = new ArtifactRelativeDoubleSpinBox();
    impl_->fpsSpin->setRange(1.0, 240.0);
    impl_->fpsSpin->setDecimals(3);
    impl_->fpsSpin->setSingleStep(0.5);
    impl_->fpsSpin->setValue(30.0);
    formLayout->addRow("Frame Rate:", impl_->fpsSpin);

    impl_->bitrateSpin = new ArtifactRelativeSpinBox();
    impl_->bitrateSpin->setRange(128, 200000);
    impl_->bitrateSpin->setSingleStep(100);
    impl_->bitrateSpin->setValue(8000);
    impl_->bitrateSpin->setSuffix(" kbps");
    formLayout->addRow("Bitrate:", impl_->bitrateSpin);

    // Audio settings
    impl_->includeAudioCheck = new QCheckBox(QStringLiteral("動画ファイルに音声を含める"));
    formLayout->addRow(QStringLiteral("音声の扱い:"), impl_->includeAudioCheck);

    impl_->outputPackageLabel = new QLabel(this);
    impl_->outputPackageLabel->setWordWrap(true);
    formLayout->addRow(QStringLiteral("出力内容:"), impl_->outputPackageLabel);

    impl_->audioCodecCombo = new QComboBox();
    impl_->audioCodecCombo->addItem(QStringLiteral("AAC"));
    formLayout->addRow(QStringLiteral("音声コーデック:"), impl_->audioCodecCombo);

    impl_->audioChannelCombo = new QComboBox(this);
    impl_->audioChannelCombo->addItem(QStringLiteral("ソース準拠"), QStringLiteral("source"));
    impl_->audioChannelCombo->addItem(QStringLiteral("Mono"), QStringLiteral("mono"));
    impl_->audioChannelCombo->addItem(QStringLiteral("Stereo（配布向け推奨）"), QStringLiteral("stereo"));
    impl_->audioChannelCombo->addItem(QStringLiteral("5.1"), QStringLiteral("5.1"));
    impl_->audioChannelCombo->addItem(QStringLiteral("7.1"), QStringLiteral("7.1"));
    impl_->audioChannelCombo->setToolTip(QStringLiteral(
        "ソース準拠ではチャンネル数を引き継ぎます。Mono・Stereo・5.1・7.1を選ぶとFFmpegで変換します。"));
    formLayout->addRow(QStringLiteral("音声チャンネル:"), impl_->audioChannelCombo);

    impl_->audioSampleRateCombo = new QComboBox(this);
    impl_->audioSampleRateCombo->addItem(QStringLiteral("ソース準拠"), 0);
    impl_->audioSampleRateCombo->addItem(QStringLiteral("48 kHz（映像向け推奨）"), 48000);
    impl_->audioSampleRateCombo->addItem(QStringLiteral("96 kHz"), 96000);
    impl_->audioSampleRateCombo->setCurrentIndex(1);
    formLayout->addRow(QStringLiteral("サンプルレート:"), impl_->audioSampleRateCombo);

    impl_->audioBitrateSpin = new ArtifactRelativeSpinBox();
    impl_->audioBitrateSpin->setRange(32, 512);
    impl_->audioBitrateSpin->setSingleStep(32);
    impl_->audioBitrateSpin->setValue(128);
    impl_->audioBitrateSpin->setSuffix(" kbps");
    formLayout->addRow(QStringLiteral("音声ビットレート:"), impl_->audioBitrateSpin);

    // Multi-channel (AOV) export toggle
    impl_->multiChannelCheck = new QCheckBox(QStringLiteral("Multi-channel EXR (AOV: Depth/Normal/Velocity/ObjectID/MaterialID/Albedo/Emission)"), this);
    impl_->multiChannelCheck->setChecked(false);
    impl_->multiChannelCheck->setToolTip(QStringLiteral("有効にすると Beauty RGBA に加えて Depth / Normal / Velocity / ObjectID / MaterialID / Albedo / Emission チャンネルを含む EXR を書き出します。コンテナは自動で EXR に切り替わります。"));
    formLayout->addRow("AOV:", impl_->multiChannelCheck);

    impl_->multiChannelGroup = new QGroupBox(QStringLiteral("AOV Channels"), this);
    auto* multiChannelLayout = new QVBoxLayout(impl_->multiChannelGroup);
    impl_->beautyChannelCheck = new QCheckBox(QStringLiteral("Beauty RGB"), impl_->multiChannelGroup);
    impl_->alphaChannelCheck = new QCheckBox(QStringLiteral("Alpha"), impl_->multiChannelGroup);
    impl_->depthChannelCheck = new QCheckBox(QStringLiteral("Depth"), impl_->multiChannelGroup);
    impl_->normalChannelCheck = new QCheckBox(QStringLiteral("Normal XYZ"), impl_->multiChannelGroup);
    impl_->velocityChannelCheck = new QCheckBox(QStringLiteral("Velocity XY"), impl_->multiChannelGroup);
    impl_->objectIdChannelCheck = new QCheckBox(QStringLiteral("Object ID"), impl_->multiChannelGroup);
    impl_->materialIdChannelCheck = new QCheckBox(QStringLiteral("Material ID"), impl_->multiChannelGroup);
    impl_->albedoChannelCheck = new QCheckBox(QStringLiteral("Albedo RGB"), impl_->multiChannelGroup);
    impl_->emissionChannelCheck = new QCheckBox(QStringLiteral("Emission"), impl_->multiChannelGroup);
    multiChannelLayout->addWidget(impl_->beautyChannelCheck);
    multiChannelLayout->addWidget(impl_->alphaChannelCheck);
    multiChannelLayout->addWidget(impl_->depthChannelCheck);
    multiChannelLayout->addWidget(impl_->normalChannelCheck);
    multiChannelLayout->addWidget(impl_->velocityChannelCheck);
    multiChannelLayout->addWidget(impl_->objectIdChannelCheck);
    multiChannelLayout->addWidget(impl_->materialIdChannelCheck);
    multiChannelLayout->addWidget(impl_->albedoChannelCheck);
    multiChannelLayout->addWidget(impl_->emissionChannelCheck);
    formLayout->addRow(QString(), impl_->multiChannelGroup);

    // Frame padding digits
    impl_->framePaddingSpin = new QSpinBox();
    impl_->framePaddingSpin->setRange(1, 10);
    impl_->framePaddingSpin->setValue(4);
    impl_->framePaddingSpin->setToolTip(QStringLiteral("画像シーケンスのフレーム番号の桁数（例: 4 → frame_0001.png）"));
    formLayout->addRow("Frame Padding:", impl_->framePaddingSpin);

    impl_->advancedGroup = new QGroupBox(QStringLiteral("その他の設定"), this);
    impl_->advancedGroup->setCheckable(true);
    impl_->advancedGroup->setChecked(false);
    auto* advancedLayout = new QFormLayout(impl_->advancedGroup);
    impl_->advancedAlphaLabel = new QLabel(QStringLiteral("Alpha: あり"), impl_->advancedGroup);
    impl_->advancedFilenameLabel = new QLabel(QStringLiteral("ProjectName_[Preset]_[Date]"), impl_->advancedGroup);
    impl_->advancedTimecodeLabel = new QLabel(QStringLiteral("ソース準拠"), impl_->advancedGroup);
    impl_->advancedColorLabel = new QLabel(QStringLiteral("自動 / Rec.709"), impl_->advancedGroup);
    impl_->advancedEncodeLabel = new QLabel(QStringLiteral("2-pass / HW 支援 / 連続書き出し"), impl_->advancedGroup);
    advancedLayout->addRow(QStringLiteral("Alpha モード:"), alphaRow);
    advancedLayout->addRow(QStringLiteral("Alpha 状態:"), impl_->advancedAlphaLabel);
    advancedLayout->addRow(QStringLiteral("ファイル名規則:"), impl_->advancedFilenameLabel);
    advancedLayout->addRow(QStringLiteral("タイムコード:"), impl_->advancedTimecodeLabel);
    advancedLayout->addRow(QStringLiteral("カラーメタデータ:"), impl_->advancedColorLabel);
    advancedLayout->addRow(QStringLiteral("エンコード補助:"), impl_->advancedEncodeLabel);

    // Enable/disable audio codec and bitrate based on checkbox
    const auto updateAudioEnabled = [this]() {
        const bool on = impl_->includeAudioCheck->isChecked();
        if (impl_->audioCodecCombo) impl_->audioCodecCombo->setEnabled(on);
        if (impl_->audioChannelCombo) impl_->audioChannelCombo->setEnabled(on);
        if (impl_->audioSampleRateCombo) impl_->audioSampleRateCombo->setEnabled(on);
        if (impl_->audioBitrateSpin) impl_->audioBitrateSpin->setEnabled(on);
    };
    updateAudioEnabled();
    QObject::connect(impl_->includeAudioCheck, &QCheckBox::toggled, [updateAudioEnabled](bool) {
        updateAudioEnabled();
    });
    QObject::connect(impl_->advancedGroup, &QGroupBox::toggled, [this](bool checked) {
        if (impl_->advancedGroup) {
          impl_->advancedGroup->setFlat(!checked);
        }
    });
    // OK/Cancel buttons
    const DialogButtonRow buttons = createWindowsDialogButtonRow(this);
    impl_->buttonRow = buttons.widget;
    impl_->okButton = buttons.okButton;
    impl_->cancelButton = buttons.cancelButton;
    if (impl_->okButton) {
        impl_->okButton->setText(QStringLiteral("書き出し"));
    }
    if (impl_->cancelButton) {
        impl_->cancelButton->setText(QStringLiteral("キャンセル"));
    }

    mainLayout->addWidget(beginnerGuide);
    mainLayout->addWidget(impl_->recommendationLabel);
    mainLayout->addWidget(summaryFrame);
    mainLayout->addLayout(formLayout);
    mainLayout->addWidget(impl_->advancedGroup);
    mainLayout->addStretch();
    mainLayout->addWidget(impl_->buttonRow);
    
    setLayout(mainLayout);

    // Connections
    QObject::connect(impl_->browseButton, &QPushButton::clicked, [this]() {
        impl_->handleBrowseClicked(this);
    });

    // プリセット選択時にフォーマット・コーデックを自動設定
    QObject::connect(impl_->presetCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), [this](int index) {
        const QString presetId = impl_->presetCombo->itemData(index).toString();
        if (!presetId.isEmpty()) {
            impl_->applyPresetToEditors(presetId);
        }
        impl_->updatePresetSummary();
        impl_->updateFormatGuide();
    });

    QObject::connect(impl_->resolutionCombo, &QComboBox::currentTextChanged, [this](const QString&) {
        impl_->syncResolutionEditors();
    });
    QObject::connect(impl_->fpsSpin, qOverload<double>(&QDoubleSpinBox::valueChanged), [this](double) {
        impl_->updateFrameRatePreflight();
        impl_->updateAdvancedSummary();
    });
    QObject::connect(impl_->alphaEnabledCheck, &QCheckBox::toggled, [this](bool) {
        impl_->updatePresetSummary();
        impl_->updateFormatGuide();
        impl_->updateAdvancedSummary();
        impl_->updateActionLabels();
    });
    QObject::connect(impl_->includeAudioCheck, &QCheckBox::toggled, [this](bool) {
        impl_->updatePresetSummary();
        impl_->updateAdvancedSummary();
        impl_->updateOutputPackageGuide();
    });
    QObject::connect(impl_->multiChannelCheck, &QCheckBox::toggled, [this](bool) {
        impl_->updateMultiChannelUi();
        impl_->updatePresetSummary();
        impl_->updateFormatGuide();
        impl_->updateAdvancedSummary();
        impl_->updateOutputPackageGuide();
    });
    QObject::connect(impl_->widthSpin, qOverload<int>(&QSpinBox::valueChanged), [this](int) {
        impl_->syncResolutionPreset();
    });
    QObject::connect(impl_->heightSpin, qOverload<int>(&QSpinBox::valueChanged), [this](int) {
        impl_->syncResolutionPreset();
    });
    QObject::connect(impl_->backendCombo, &QComboBox::currentTextChanged, [this](const QString&) {
        impl_->updateBackendInfo();
    });
    QObject::connect(impl_->okButton, &QPushButton::clicked, this, &QDialog::accept);
    QObject::connect(impl_->cancelButton, &QPushButton::clicked, this, &QDialog::reject);

    // フォーマット変更時に拡張子を自動更新
    QObject::connect(impl_->formatCombo, &QComboBox::currentTextChanged, [this](const QString& format) {
        impl_->updatePresetSummary();
        impl_->updateFormatGuide();
        impl_->updateAdvancedSummary();
        impl_->updateContainerCompatibility();
        impl_->updateOutputPackageGuide();
        if (!impl_->outputPathEdit) return;
        QString path = impl_->outputPathEdit->text().trimmed();
        if (path.isEmpty()) return;

        // フォーマットから拡張子を決定
        QString ext;
        if (format == QStringLiteral("MP4")) ext = QStringLiteral("mp4");
        else if (format == QStringLiteral("MOV")) ext = QStringLiteral("mov");
        else if (format == QStringLiteral("AVI")) ext = QStringLiteral("avi");
        else if (format == QStringLiteral("WebM")) ext = QStringLiteral("webm");
        else if (format == QStringLiteral("MKV")) ext = QStringLiteral("mkv");
        else if (format == QStringLiteral("PNG Sequence")) ext = QStringLiteral("png");
        else if (format == QStringLiteral("JPEG Sequence")) ext = QStringLiteral("jpg");
        else if (format == QStringLiteral("TIFF Sequence")) ext = QStringLiteral("tiff");
        else if (format == QStringLiteral("BMP Sequence")) ext = QStringLiteral("bmp");
        else if (format == QStringLiteral("EXR Sequence")) ext = QStringLiteral("exr");
        else ext = QStringLiteral("mp4");

        // 拡張子を置換
        QFileInfo info(path);
        QString newPath = info.absolutePath() + "/" + info.completeBaseName() + "." + ext;
        impl_->outputPathEdit->setText(newPath);
    });

    // コーデック変更時も拡張子を更新（ProRes → .mov など）
    QObject::connect(impl_->codecCombo, &QComboBox::currentTextChanged, [this](const QString& codec) {
        impl_->updatePresetSummary();
        impl_->updateFormatGuide();
        impl_->updateAdvancedSummary();
        const QString normalizedCodec = codec.trimmed().toLower();
        if (normalizedCodec == QStringLiteral("prores")) {
            if (impl_->codecProfile.trimmed().isEmpty()) {
                impl_->codecProfile = QStringLiteral("hq");
            }
        } else {
            impl_->codecProfile.clear();
        }
        if (!impl_->outputPathEdit) return;
        QString path = impl_->outputPathEdit->text().trimmed();
        if (path.isEmpty()) return;

        QString ext;
        if (codec == QStringLiteral("ProRes")) ext = QStringLiteral("mov");
        else if (codec == QStringLiteral("VP9")) ext = QStringLiteral("webm");
        else if (codec == QStringLiteral("DNxHD")) ext = QStringLiteral("mov");
        else if (codec == QStringLiteral("PNG")) ext = QStringLiteral("png");
        else if (codec == QStringLiteral("JPEG")) ext = QStringLiteral("jpg");
        else if (codec == QStringLiteral("TIFF")) ext = QStringLiteral("tiff");
        else if (codec == QStringLiteral("BMP")) ext = QStringLiteral("bmp");
        else if (codec == QStringLiteral("EXR")) ext = QStringLiteral("exr");
        else return; // H.264/H.265 はフォーマットに依存するためスキップ

        QFileInfo info(path);
        QString newPath = info.absolutePath() + "/" + info.completeBaseName() + "." + ext;
        impl_->outputPathEdit->setText(newPath);
        impl_->updatePresetSummary();
        impl_->updateFormatGuide();
    });

    impl_->syncResolutionPreset();
    impl_->updatePresetSummary();
    impl_->updateFormatGuide();
    impl_->updateAdvancedSummary();
    impl_->setSelectedMultiChannelChannels({
        QStringLiteral("R"), QStringLiteral("G"), QStringLiteral("B"),
        QStringLiteral("A"), QStringLiteral("Depth"),
        QStringLiteral("Normal.X"), QStringLiteral("Normal.Y"), QStringLiteral("Normal.Z"),
        QStringLiteral("Velocity.X"), QStringLiteral("Velocity.Y"),
        QStringLiteral("ObjectId"),
        QStringLiteral("MaterialId"), QStringLiteral("Albedo.R"),
        QStringLiteral("Albedo.G"), QStringLiteral("Albedo.B"),
        QStringLiteral("Emission")
    });
    impl_->updateMultiChannelUi();
    impl_->updateActionLabels();
    impl_->updateBeginnerGuide();
    impl_->updateContainerCompatibility();
    impl_->updateOutputPackageGuide();
    updateAlphaPreflight(impl_->alphaEnabledCheck, impl_->preflightSummaryLabel, impl_->preflightDetailsLabel,
                         impl_->formatCombo ? impl_->formatCombo->currentText() : QStringLiteral("MP4"),
                         impl_->codecCombo ? impl_->codecCombo->currentText() : QStringLiteral("H.264"));
    impl_->updateFrameRatePreflight();
 }

 ArtifactRenderOutputSettingDialog::~ArtifactRenderOutputSettingDialog()
 {
  delete impl_;
 }

 void ArtifactRenderOutputSettingDialog::setOutputPath(const QString& path)
 {
   if (impl_->outputPathEdit) {
     impl_->outputPathEdit->setText(path);
   }
 }

 QString ArtifactRenderOutputSettingDialog::outputPath() const
 {
   return impl_->outputPathEdit ? impl_->outputPathEdit->text() : QString();
 }

 void ArtifactRenderOutputSettingDialog::setOutputFormat(const QString& format)
 {
   Impl::ensureComboContains(impl_->formatCombo, format);
   if (!impl_->formatCombo) {
     return;
   }
   impl_->formatCombo->setCurrentIndex(std::max(0, impl_->formatCombo->findText(format)));
 }

 QString ArtifactRenderOutputSettingDialog::outputFormat() const
 {
   return impl_->formatCombo ? impl_->formatCombo->currentText() : QStringLiteral("MP4");
 }

 void ArtifactRenderOutputSettingDialog::setCodec(const QString& codec)
 {
   QString normalizedCodec = codec.trimmed();
   const QString lower = normalizedCodec.toLower();
   if (lower == QStringLiteral("prores")) {
    normalizedCodec = QStringLiteral("ProRes");
   } else if (lower == QStringLiteral("h264")) {
    normalizedCodec = QStringLiteral("H.264");
   } else if (lower == QStringLiteral("h265")) {
    normalizedCodec = QStringLiteral("H.265");
   } else if (lower == QStringLiteral("mjpeg")) {
    normalizedCodec = QStringLiteral("JPEG");
   }
   if (!impl_->codecCombo) {
     return;
   }
   const int compatibleIndex = impl_->codecCombo->findText(normalizedCodec);
   if (compatibleIndex < 0) {
     return;
   }
   impl_->codecCombo->setCurrentIndex(compatibleIndex);
   if (lower == QStringLiteral("prores") && impl_->codecProfile.trimmed().isEmpty()) {
    impl_->codecProfile = QStringLiteral("hq");
   }
 }

 QString ArtifactRenderOutputSettingDialog::codec() const
 {
   return impl_->codecCombo ? impl_->codecCombo->currentText() : QStringLiteral("H.264");
 }

 void ArtifactRenderOutputSettingDialog::setCodecProfile(const QString& profile)
 {
   if (impl_) {
    impl_->codecProfile = profile.trimmed();
   }
 }

 QString ArtifactRenderOutputSettingDialog::codecProfile() const
 {
   return impl_ ? impl_->codecProfile : QString();
 }

void ArtifactRenderOutputSettingDialog::setEncoderBackend(const QString& backend)
{
  if (!impl_->backendCombo) {
    return;
  }
  const QString normalized = Impl::normalizeBackend(backend);
  const QString displayText = (normalized == QStringLiteral("pipe-hw"))
      ? QStringLiteral("pipe-hw (NVENC)")
      : normalized;
  const int index = impl_->backendCombo->findText(displayText);
  impl_->backendCombo->setCurrentIndex(index >= 0 ? index : 0);
}

QString ArtifactRenderOutputSettingDialog::encoderBackend() const
{
  return impl_->backendCombo ? Impl::normalizeBackend(impl_->backendCombo->currentText()) : QStringLiteral("auto");
}

void ArtifactRenderOutputSettingDialog::setRenderBackend(const QString& backend)
{
  if (!impl_->renderBackendCombo) {
    return;
  }
  const QString normalized = Impl::normalizeRenderBackend(backend);
  const int index = impl_->renderBackendCombo->findText(normalized);
  impl_->renderBackendCombo->setCurrentIndex(index >= 0 ? index : 0);
}

QString ArtifactRenderOutputSettingDialog::renderBackend() const
{
  return impl_->renderBackendCombo ? Impl::normalizeRenderBackend(impl_->renderBackendCombo->currentText()) : QStringLiteral("auto");
}

void ArtifactRenderOutputSettingDialog::setMultiChannelEnabled(bool enabled)
{
  if (impl_->multiChannelCheck) {
    impl_->multiChannelCheck->setChecked(enabled);
  }
  if (impl_) {
    impl_->updateMultiChannelUi();
  }
}

bool ArtifactRenderOutputSettingDialog::multiChannelEnabled() const
{
  return impl_->multiChannelCheck ? impl_->multiChannelCheck->isChecked() : false;
}

void ArtifactRenderOutputSettingDialog::setMultiChannelChannels(const QStringList& channels)
{
  if (!impl_) {
    return;
  }
  impl_->setSelectedMultiChannelChannels(channels);
}

QStringList ArtifactRenderOutputSettingDialog::multiChannelChannels() const
{
  return impl_ ? impl_->selectedMultiChannelChannels() : QStringList{};
}

void ArtifactRenderOutputSettingDialog::setFramePadding(int digits)
{
  if (impl_->framePaddingSpin) {
    impl_->framePaddingSpin->setValue(std::clamp(digits, 1, 10));
  }
}

int ArtifactRenderOutputSettingDialog::framePadding() const
{
  return impl_->framePaddingSpin ? impl_->framePaddingSpin->value() : 4;
}
 void ArtifactRenderOutputSettingDialog::setResolution(int width, int height)
 {
   if (!impl_->widthSpin || !impl_->heightSpin) {
     return;
   }
   {
     const QSignalBlocker widthBlock(impl_->widthSpin);
     const QSignalBlocker heightBlock(impl_->heightSpin);
     impl_->widthSpin->setValue(std::max(1, width));
     impl_->heightSpin->setValue(std::max(1, height));
   }
   impl_->syncResolutionPreset();
 }

void ArtifactRenderOutputSettingDialog::setCompositionFrameRate(double fps)
{
  if (!impl_) {
    return;
  }
  impl_->compositionFrameRate = std::max(1.0, fps);
  impl_->updateFrameRatePreflight();
}

double ArtifactRenderOutputSettingDialog::compositionFrameRate() const
{
  return impl_ ? impl_->compositionFrameRate : 30.0;
}

 int ArtifactRenderOutputSettingDialog::outputWidth() const
 {
   return impl_->widthSpin ? impl_->widthSpin->value() : 1920;
 }

 int ArtifactRenderOutputSettingDialog::outputHeight() const
 {
   return impl_->heightSpin ? impl_->heightSpin->value() : 1080;
 }

 void ArtifactRenderOutputSettingDialog::setFrameRate(double fps)
 {
   if (impl_->fpsSpin) {
     impl_->fpsSpin->setValue(std::max(1.0, fps));
      impl_->updateFrameRatePreflight();
   }
 }

 double ArtifactRenderOutputSettingDialog::frameRate() const
 {
   return impl_->fpsSpin ? impl_->fpsSpin->value() : 30.0;
 }

 void ArtifactRenderOutputSettingDialog::setBitrateKbps(int bitrateKbps)
 {
   if (impl_->bitrateSpin) {
     impl_->bitrateSpin->setValue(std::max(128, bitrateKbps));
   }
 }

 int ArtifactRenderOutputSettingDialog::bitrateKbps() const
 {
   return impl_->bitrateSpin ? impl_->bitrateSpin->value() : 8000;
 }

 void ArtifactRenderOutputSettingDialog::setIncludeAudio(bool include)
 {
   if (impl_->includeAudioCheck) {
     const QSignalBlocker blocker(impl_->includeAudioCheck);
     impl_->includeAudioCheck->setChecked(include);
     if (impl_->audioCodecCombo) impl_->audioCodecCombo->setEnabled(include);
     if (impl_->audioChannelCombo) impl_->audioChannelCombo->setEnabled(include);
     if (impl_->audioSampleRateCombo) impl_->audioSampleRateCombo->setEnabled(include);
     if (impl_->audioBitrateSpin) impl_->audioBitrateSpin->setEnabled(include);
     impl_->updateOutputPackageGuide();
   }
 }

 bool ArtifactRenderOutputSettingDialog::includeAudio() const
 {
   return impl_->includeAudioCheck ? impl_->includeAudioCheck->isChecked() : false;
 }

 void ArtifactRenderOutputSettingDialog::setAudioCodec(const QString& codec)
 {
   if (!impl_->audioCodecCombo) return;
   const QString lower = codec.trimmed().toLower();
   QString display;
   if (lower == QStringLiteral("aac") || lower.isEmpty()) display = QStringLiteral("AAC");
   else if (lower == QStringLiteral("mp3")) display = QStringLiteral("MP3");
   else if (lower == QStringLiteral("flac")) display = QStringLiteral("FLAC");
   else if (lower == QStringLiteral("opus")) display = QStringLiteral("Opus");
   else if (lower == QStringLiteral("vorbis")) display = QStringLiteral("Vorbis");
   else if (lower == QStringLiteral("pcm_s24le") || lower == QStringLiteral("pcm")) display = QStringLiteral("PCM 24-bit");
   else display = codec;
   const int idx = impl_->audioCodecCombo->findText(display);
   impl_->audioCodecCombo->setCurrentIndex(idx >= 0 ? idx : 0);
 }

 QString ArtifactRenderOutputSettingDialog::audioCodec() const
 {
   if (!impl_->audioCodecCombo) return QStringLiteral("aac");
   const QString text = impl_->audioCodecCombo->currentText().toLower();
   if (text == QStringLiteral("mp3")) return QStringLiteral("mp3");
   if (text == QStringLiteral("flac")) return QStringLiteral("flac");
   if (text == QStringLiteral("opus")) return QStringLiteral("opus");
   if (text == QStringLiteral("vorbis")) return QStringLiteral("vorbis");
   if (text == QStringLiteral("pcm 24-bit")) return QStringLiteral("pcm_s24le");
   return QStringLiteral("aac");
 }

 void ArtifactRenderOutputSettingDialog::setAudioBitrateKbps(int bitrateKbps)
 {
   if (impl_->audioBitrateSpin) {
     impl_->audioBitrateSpin->setValue(std::max(32, bitrateKbps));
   }
 }

 int ArtifactRenderOutputSettingDialog::audioBitrateKbps() const
 {
   return impl_->audioBitrateSpin ? impl_->audioBitrateSpin->value() : 128;
 }

 void ArtifactRenderOutputSettingDialog::setAudioChannelMode(const QString& mode)
 {
   if (!impl_->audioChannelCombo) return;
   const int index = impl_->audioChannelCombo->findData(mode.trimmed().toLower());
   impl_->audioChannelCombo->setCurrentIndex(index >= 0 ? index : 0);
 }

 QString ArtifactRenderOutputSettingDialog::audioChannelMode() const
 {
   return impl_->audioChannelCombo
       ? impl_->audioChannelCombo->currentData().toString()
       : QStringLiteral("source");
 }

 void ArtifactRenderOutputSettingDialog::setAudioSampleRate(int sampleRate)
 {
   if (!impl_->audioSampleRateCombo) return;
   const int index = impl_->audioSampleRateCombo->findData(sampleRate);
   impl_->audioSampleRateCombo->setCurrentIndex(index >= 0 ? index : 1);
 }

 int ArtifactRenderOutputSettingDialog::audioSampleRate() const
 {
   return impl_->audioSampleRateCombo
       ? impl_->audioSampleRateCombo->currentData().toInt()
       : 48000;
 }

 void ArtifactRenderOutputSettingDialog::setPreflightSummary(const QString& summary)
 {
   if (impl_->preflightSummaryLabel) {
     impl_->preflightSummaryLabel->setText(summary.isEmpty()
         ? QStringLiteral("Preflight: none")
         : summary);
   }
 }

 void ArtifactRenderOutputSettingDialog::setPreflightDetails(const QStringList& details)
 {
   if (!impl_->preflightDetailsLabel) {
     return;
   }
   if (details.isEmpty()) {
     impl_->preflightDetailsLabel->setText(QStringLiteral("No preflight issues were detected."));
     return;
   }
   impl_->preflightDetailsLabel->setText(details.join(QStringLiteral("\n")));
 }

};
