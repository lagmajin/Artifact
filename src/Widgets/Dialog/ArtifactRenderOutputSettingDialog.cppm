module;
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
#include <QLabel>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QStringList>
module Artifact.Widget.Dialog.RenderOutputSetting;

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
  QComboBox* backendCombo = nullptr;
  QComboBox* resolutionCombo = nullptr;
  QSpinBox* widthSpin = nullptr;
  QSpinBox* heightSpin = nullptr;
  QDoubleSpinBox* fpsSpin = nullptr;
  QSpinBox* bitrateSpin = nullptr;
  QDialogButtonBox* buttonBox = nullptr;

  void handleBrowseClicked(ArtifactRenderOutputSettingDialog* dialog);
  void syncResolutionEditors();
  void syncResolutionPreset();
  static void ensureComboContains(QComboBox* combo, const QString& value);
  void loadFormatPresets();
  void applyPresetToEditors(const QString& presetId);
  static QString normalizeBackend(const QString& backend);
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
    else if (presetCodec == "mjpeg" || presetCodec == "jpeg") codecText = "JPEG";
    else codecText = preset->codec;

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
     } else if (preset->codec == QStringLiteral("ProRes")) {
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
   if (value == QStringLiteral("native") || value == QStringLiteral("api") || value == QStringLiteral("ffmpegapi")) {
     return QStringLiteral("native");
   }
   return QStringLiteral("auto");
 }
	
	W_OBJECT_IMPL(ArtifactRenderOutputSettingDialog)
	
 ArtifactRenderOutputSettingDialog::ArtifactRenderOutputSettingDialog(QWidget* parent /*= nullptr*/):QDialog(parent),impl_(new Impl())
 {
    setWindowTitle("Render Output Settings");
    setMinimumWidth(500);

    auto mainLayout = new QVBoxLayout(this);
    auto formLayout = new QFormLayout();

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
      "auto", "pipe", "native"
    });
    formLayout->addRow("Encoder Backend:", impl_->backendCombo);

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
    
    // OK/Cancel buttons
    impl_->buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    
    mainLayout->addLayout(formLayout);
    mainLayout->addStretch();
    mainLayout->addWidget(impl_->buttonBox);
    
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
    });

    QObject::connect(impl_->resolutionCombo, &QComboBox::currentTextChanged, [this](const QString&) {
        impl_->syncResolutionEditors();
    });
    QObject::connect(impl_->widthSpin, qOverload<int>(&QSpinBox::valueChanged), [this](int) {
        impl_->syncResolutionPreset();
    });
    QObject::connect(impl_->heightSpin, qOverload<int>(&QSpinBox::valueChanged), [this](int) {
        impl_->syncResolutionPreset();
    });

    QObject::connect(impl_->buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    QObject::connect(impl_->buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

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
    });

    impl_->syncResolutionPreset();
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
   Impl::ensureComboContains(impl_->codecCombo, codec);
   if (!impl_->codecCombo) {
     return;
   }
   impl_->codecCombo->setCurrentIndex(std::max(0, impl_->codecCombo->findText(codec)));
 }

 QString ArtifactRenderOutputSettingDialog::codec() const
 {
   return impl_->codecCombo ? impl_->codecCombo->currentText() : QStringLiteral("H.264");
 }

 void ArtifactRenderOutputSettingDialog::setEncoderBackend(const QString& backend)
 {
   if (!impl_->backendCombo) {
     return;
   }
   const QString normalized = Impl::normalizeBackend(backend);
   const int index = impl_->backendCombo->findText(normalized);
   impl_->backendCombo->setCurrentIndex(index >= 0 ? index : 0);
 }

 QString ArtifactRenderOutputSettingDialog::encoderBackend() const
 {
   return impl_->backendCombo ? Impl::normalizeBackend(impl_->backendCombo->currentText()) : QStringLiteral("auto");
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

};
