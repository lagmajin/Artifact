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
#include <Widgets/Dialog/ArtifactDialogButtons.hpp>
module Artifact.Widget.Dialog.RenderOutputSetting;

import Encoder.FFmpegEncoder;
import Artifact.Render.Queue.Presets;


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
  double compositionFrameRate = 30.0;
    QComboBox* renderBackendCombo = nullptr;
    QComboBox* resolutionCombo = nullptr;
  QSpinBox* widthSpin = nullptr;
  QSpinBox* heightSpin = nullptr;
  QDoubleSpinBox* fpsSpin = nullptr;
  QSpinBox* bitrateSpin = nullptr;
  QCheckBox* includeAudioCheck = nullptr;
  QComboBox* audioCodecCombo = nullptr;
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

   QStringList lines;
   lines << QStringLiteral("Hardware FFmpeg: %1")
               .arg((nvencH264 || nvencHevc) ? QStringLiteral("NVENC available") : QStringLiteral("NVENC unavailable, will fallback"));
   if (visibleCodecs.isEmpty()) {
     lines << QStringLiteral("FFmpeg codecs: unavailable");
   } else {
     lines << QStringLiteral("FFmpeg codecs: %1").arg(visibleCodecs.join(QStringLiteral(", ")));
   }
   backendInfoLabel->setText(lines.join(QStringLiteral(" | ")));
   backendInfoLabel->setToolTip(QStringLiteral("pipe-hw first tries NVENC through FFmpeg.exe. If the encoder is unavailable, the job falls back to the software pipe backend."));
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
   QStringList lines;
   lines << QStringLiteral("<span style='font-size:14px;font-weight:600;'>%1</span>").arg(presetName.isEmpty() ? QStringLiteral("未選択") : presetName);
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
  if (format == QStringLiteral("WebM") || codec == QStringLiteral("VP9")) {
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
   const bool alphaEnabled = alphaEnabledCheck ? alphaEnabledCheck->isChecked() : true;
   okButton->setText(alphaEnabled ? QStringLiteral("書き出し") : QStringLiteral("Alphaなしで書き出し"));
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
     if (preset->isImageSequence) {
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
  if (value == QStringLiteral("external") || value == QStringLiteral("process") || value == QStringLiteral("outofprocess")) {
    return QStringLiteral("external");
  }
  return QStringLiteral("auto");
}
	
	W_OBJECT_IMPL(ArtifactRenderOutputSettingDialog)
	
 ArtifactRenderOutputSettingDialog::ArtifactRenderOutputSettingDialog(QWidget* parent /*= nullptr*/):QDialog(parent),impl_(new Impl())
 {
    setWindowTitle("Render Output Settings");
    setMinimumWidth(760);

    auto mainLayout = new QVBoxLayout(this);
    auto formLayout = new QFormLayout();

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
    impl_->alphaEnabledCheck->setChecked(true);
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
    formLayout->addRow("Format Preset:", impl_->presetCombo);

    // Format selection
    impl_->formatCombo = new QComboBox();
    impl_->formatCombo->addItems(QStringList{
      "MP4", "MOV", "AVI", "WebM", "MKV",
      "PNG Sequence", "JPEG Sequence", "TIFF Sequence", "BMP Sequence", "EXR Sequence"
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
      "auto", "pipe", "pipe-hw (NVENC)", "native", "gpu"
    });
    formLayout->addRow("Encoder Backend:", impl_->backendCombo);
    impl_->backendInfoLabel = new QLabel(this);
    impl_->backendInfoLabel->setWordWrap(true);
    formLayout->addRow(QString(), impl_->backendInfoLabel);
    impl_->updateBackendInfo();

    impl_->renderBackendCombo = new QComboBox();
    impl_->renderBackendCombo->addItems(QStringList{
      "auto", "cpu", "gpu", "external"
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

    impl_->widthSpin = new QSpinBox();
    impl_->widthSpin->setRange(1, 16384);
    impl_->heightSpin = new QSpinBox();
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
    impl_->fpsSpin = new QDoubleSpinBox();
    impl_->fpsSpin->setRange(1.0, 240.0);
    impl_->fpsSpin->setDecimals(3);
    impl_->fpsSpin->setSingleStep(0.5);
    impl_->fpsSpin->setValue(30.0);
    formLayout->addRow("Frame Rate:", impl_->fpsSpin);

    impl_->bitrateSpin = new QSpinBox();
    impl_->bitrateSpin->setRange(128, 200000);
    impl_->bitrateSpin->setSingleStep(100);
    impl_->bitrateSpin->setValue(8000);
    impl_->bitrateSpin->setSuffix(" kbps");
    formLayout->addRow("Bitrate:", impl_->bitrateSpin);

    // Audio settings
    impl_->includeAudioCheck = new QCheckBox("Include audio in output");
    formLayout->addRow("Audio:", impl_->includeAudioCheck);

    impl_->audioCodecCombo = new QComboBox();
    impl_->audioCodecCombo->addItems(QStringList{"AAC", "MP3", "FLAC", "Opus"});
    formLayout->addRow("Audio Codec:", impl_->audioCodecCombo);

    impl_->audioBitrateSpin = new QSpinBox();
    impl_->audioBitrateSpin->setRange(32, 512);
    impl_->audioBitrateSpin->setSingleStep(32);
    impl_->audioBitrateSpin->setValue(128);
    impl_->audioBitrateSpin->setSuffix(" kbps");
    formLayout->addRow("Audio Bitrate:", impl_->audioBitrateSpin);

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
    impl_->updateActionLabels();
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
   Impl::ensureComboContains(impl_->codecCombo, normalizedCodec);
   if (!impl_->codecCombo) {
     return;
   }
   impl_->codecCombo->setCurrentIndex(std::max(0, impl_->codecCombo->findText(normalizedCodec)));
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
     if (impl_->audioBitrateSpin) impl_->audioBitrateSpin->setEnabled(include);
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
