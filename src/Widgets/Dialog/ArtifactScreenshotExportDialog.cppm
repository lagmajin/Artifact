module;
#include <utility>

#include <algorithm>

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QVBoxLayout>
#include <wobjectimpl.h>

module Artifact.Widget.Dialog.ScreenshotExport;

namespace Artifact
{

namespace
{

QString normalizedScreenshotFormat(const QString& format)
{
 const QString value = format.trimmed().toLower();
 if (value == QStringLiteral("jpg") || value == QStringLiteral("jpeg")) {
  return QStringLiteral("jpg");
 }
 if (value == QStringLiteral("exr")) {
  return QStringLiteral("exr");
 }
 return QStringLiteral("png");
}

QString pathWithScreenshotSuffix(const QString& path, const QString& format)
{
 if (path.trimmed().isEmpty()) {
  return path;
 }

 const QFileInfo info(path);
 const QString normalizedFormat = normalizedScreenshotFormat(format);
 const QString suffix = normalizedFormat;
 const QString directory = info.absolutePath().isEmpty() ? QStringLiteral(".") : info.absolutePath();
 return QDir(directory).filePath(info.completeBaseName() + QStringLiteral(".") + suffix);
}

QString inferFormatFromPath(const QString& path)
{
 const QString suffix = QFileInfo(path).suffix().trimmed().toLower();
 if (suffix == QStringLiteral("jpg") || suffix == QStringLiteral("jpeg")) {
  return QStringLiteral("jpg");
 }
 if (suffix == QStringLiteral("exr")) {
  return QStringLiteral("exr");
 }
 return QStringLiteral("png");
}

QString screenshotSaveFilter()
{
 return QStringLiteral("PNG Image (*.png);;JPEG Image (*.jpg *.jpeg);;OpenEXR (*.exr);;All Files (*.*)");
}

QString screenshotBrowseTitle()
{
 return QStringLiteral("Select screenshot file");
}

} // namespace

class ArtifactScreenshotExportDialog::Impl
{
public:
 QLineEdit* filePathEdit = nullptr;
 QPushButton* browseButton = nullptr;
 QComboBox* formatCombo = nullptr;
 QLabel* jpegQualityLabel = nullptr;
 QSpinBox* jpegQualitySpin = nullptr;
 QCheckBox* captureWholeWindowCheck = nullptr;
 QDialogButtonBox* buttonBox = nullptr;

 void syncFormatUi(ArtifactScreenshotExportDialog* dialog);
 void browseForFile(ArtifactScreenshotExportDialog* dialog);
};

void ArtifactScreenshotExportDialog::Impl::syncFormatUi(ArtifactScreenshotExportDialog* dialog)
{
 if (!formatCombo || !jpegQualityLabel || !jpegQualitySpin) {
  return;
 }

 const QString format = formatCombo->currentData().toString();
 const bool isJpeg = normalizedScreenshotFormat(format) == QStringLiteral("jpg");
 jpegQualityLabel->setEnabled(isJpeg);
 jpegQualitySpin->setEnabled(isJpeg);

 if (filePathEdit && !filePathEdit->text().trimmed().isEmpty()) {
  const QSignalBlocker blocker(filePathEdit);
  filePathEdit->setText(pathWithScreenshotSuffix(filePathEdit->text(), format));
 }
 Q_UNUSED(dialog);
}

void ArtifactScreenshotExportDialog::Impl::browseForFile(ArtifactScreenshotExportDialog* dialog)
{
 if (!dialog || !filePathEdit) {
  return;
 }

 const QString selected = QFileDialog::getSaveFileName(
     dialog,
     screenshotBrowseTitle(),
     filePathEdit->text(),
     screenshotSaveFilter());
 if (selected.isEmpty()) {
  return;
 }

 filePathEdit->setText(selected);
 const QString suffix = QFileInfo(selected).suffix().trimmed().toLower();
 if (suffix == QStringLiteral("jpg") || suffix == QStringLiteral("jpeg")) {
  dialog->setFormat(QStringLiteral("jpg"));
 } else if (suffix == QStringLiteral("exr")) {
  dialog->setFormat(QStringLiteral("exr"));
 } else {
  dialog->setFormat(formatCombo ? formatCombo->currentData().toString() : QStringLiteral("png"));
 }
}

W_OBJECT_IMPL(ArtifactScreenshotExportDialog)

ArtifactScreenshotExportDialog::ArtifactScreenshotExportDialog(QWidget* parent)
    : QDialog(parent), impl_(new Impl())
{
 setWindowTitle(QStringLiteral("Advanced Screenshot"));
 setMinimumWidth(420);

 auto* root = new QVBoxLayout(this);
 auto* pathRow = new QHBoxLayout();
 auto* formatRow = new QHBoxLayout();
 auto* qualityRow = new QHBoxLayout();

 auto* pathLabel = new QLabel(QStringLiteral("File"), this);
 impl_->filePathEdit = new QLineEdit(this);
 impl_->browseButton = new QPushButton(QStringLiteral("Browse..."), this);
 pathRow->addWidget(pathLabel);
 pathRow->addWidget(impl_->filePathEdit, 1);
 pathRow->addWidget(impl_->browseButton);

 auto* formatLabel = new QLabel(QStringLiteral("Format"), this);
 impl_->formatCombo = new QComboBox(this);
 impl_->formatCombo->addItem(QStringLiteral("PNG"), QStringLiteral("png"));
 impl_->formatCombo->addItem(QStringLiteral("JPEG"), QStringLiteral("jpg"));
 impl_->formatCombo->addItem(QStringLiteral("EXR"), QStringLiteral("exr"));
 formatRow->addWidget(formatLabel);
 formatRow->addWidget(impl_->formatCombo, 1);

 impl_->jpegQualityLabel = new QLabel(QStringLiteral("JPEG Quality"), this);
 impl_->jpegQualitySpin = new QSpinBox(this);
 impl_->jpegQualitySpin->setRange(1, 100);
 impl_->jpegQualitySpin->setValue(95);
 qualityRow->addWidget(impl_->jpegQualityLabel);
 qualityRow->addWidget(impl_->jpegQualitySpin, 1);

 impl_->captureWholeWindowCheck =
     new QCheckBox(QStringLiteral("Capture whole editor window"), this);
 impl_->captureWholeWindowCheck->setChecked(false);

 impl_->buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);

 root->addLayout(pathRow);
 root->addLayout(formatRow);
 root->addLayout(qualityRow);
 root->addWidget(impl_->captureWholeWindowCheck);
 root->addWidget(impl_->buttonBox);

 QObject::connect(impl_->browseButton, &QPushButton::clicked, this, [this]() {
  impl_->browseForFile(this);
 });
 QObject::connect(impl_->formatCombo, &QComboBox::currentIndexChanged, this, [this](int) {
  impl_->syncFormatUi(this);
 });
 QObject::connect(impl_->buttonBox, &QDialogButtonBox::accepted, this, [this]() {
  accept();
 });
 QObject::connect(impl_->buttonBox, &QDialogButtonBox::rejected, this, [this]() {
  reject();
 });

 setFilePath(QStringLiteral("composition_screenshot.png"));
 setFormat(inferFormatFromPath(filePath()));
 setJpegQuality(95);
 setCaptureSource(ScreenshotCaptureSource::Renderer);
}

ArtifactScreenshotExportDialog::~ArtifactScreenshotExportDialog()
{
 delete impl_;
}

void ArtifactScreenshotExportDialog::accept()
{
 if (!impl_ || !impl_->filePathEdit || impl_->filePathEdit->text().trimmed().isEmpty()) {
  QMessageBox::warning(this, QStringLiteral("Screenshot"), QStringLiteral("File path is empty."));
  return;
 }

 if (impl_->formatCombo) {
  const QString format = impl_->formatCombo->currentData().toString();
  impl_->filePathEdit->setText(pathWithScreenshotSuffix(impl_->filePathEdit->text(), format));
 }

 QDialog::accept();
}

void ArtifactScreenshotExportDialog::setFilePath(const QString& path)
{
 if (!impl_ || !impl_->filePathEdit) {
  return;
 }
 impl_->filePathEdit->setText(path);
}

QString ArtifactScreenshotExportDialog::filePath() const
{
 return impl_ && impl_->filePathEdit ? impl_->filePathEdit->text() : QString();
}

void ArtifactScreenshotExportDialog::setFormat(const QString& format)
{
 if (!impl_ || !impl_->formatCombo) {
  return;
 }

 const QString normalized = normalizedScreenshotFormat(format);
 const int index = impl_->formatCombo->findData(normalized);
 const QSignalBlocker blocker(impl_->formatCombo);
 impl_->formatCombo->setCurrentIndex(index >= 0 ? index : 0);
 impl_->syncFormatUi(this);
}

QString ArtifactScreenshotExportDialog::format() const
{
 if (!impl_ || !impl_->formatCombo) {
  return QStringLiteral("png");
 }
 return impl_->formatCombo->currentData().toString();
}

void ArtifactScreenshotExportDialog::setJpegQuality(int quality)
{
 if (impl_ && impl_->jpegQualitySpin) {
  impl_->jpegQualitySpin->setValue(std::clamp(quality, 1, 100));
 }
}

int ArtifactScreenshotExportDialog::jpegQuality() const
{
 return impl_ && impl_->jpegQualitySpin ? impl_->jpegQualitySpin->value() : 95;
}

void ArtifactScreenshotExportDialog::setCaptureSource(ScreenshotCaptureSource source)
{
 if (impl_ && impl_->captureWholeWindowCheck) {
  const QSignalBlocker blocker(impl_->captureWholeWindowCheck);
  impl_->captureWholeWindowCheck->setChecked(source == ScreenshotCaptureSource::WholeWindow);
 }
}

ScreenshotCaptureSource ArtifactScreenshotExportDialog::captureSource() const
{
 if (!impl_ || !impl_->captureWholeWindowCheck) {
  return ScreenshotCaptureSource::Renderer;
 }
 return impl_->captureWholeWindowCheck->isChecked()
     ? ScreenshotCaptureSource::WholeWindow
     : ScreenshotCaptureSource::Renderer;
}

void ArtifactScreenshotExportDialog::setOptions(const ScreenshotExportOptions& options)
{
 setFilePath(options.filePath);
 setFormat(options.format);
 setJpegQuality(options.jpegQuality);
 setCaptureSource(options.captureSource);
}

ScreenshotExportOptions ArtifactScreenshotExportDialog::options() const
{
 ScreenshotExportOptions options;
 options.filePath = filePath();
 options.format = format();
 options.jpegQuality = jpegQuality();
 options.captureSource = captureSource();
 return options;
}

}
