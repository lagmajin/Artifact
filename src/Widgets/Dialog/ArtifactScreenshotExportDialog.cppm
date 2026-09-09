module;
#include <utility>

#include <algorithm>

#include <QCheckBox>
#include <QColor>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QFont>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QObject>
#include <QPainter>
#include <QPalette>
#include <QPaintEvent>
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

const QColor kDialogBackground(30, 32, 36);
const QColor kPanelBackground(35, 38, 43);
const QColor kBorderColor(61, 66, 74);
const QColor kPrimaryText(224, 226, 231);
const QColor kMutedText(145, 151, 161);
const QColor kAccentColor(225, 151, 63);

QString normalizedScreenshotFormat(const QString& format);

void setWindowColor(QWidget* widget, const QColor& color)
{
 QPalette palette = widget->palette();
 palette.setColor(QPalette::Window, color);
 widget->setPalette(palette);
 widget->setAutoFillBackground(true);
}

void setLabelColor(QLabel* label, const QColor& color)
{
 QPalette palette = label->palette();
 palette.setColor(QPalette::WindowText, color);
 label->setPalette(palette);
}

QLabel* makeSectionLabel(const QString& text, QWidget* parent)
{
 auto* label = new QLabel(text, parent);
 QFont font = label->font();
 font.setBold(true);
 font.setPointSize(9);
 label->setFont(font);
 setLabelColor(label, kAccentColor);
 return label;
}

class ScreenshotSummaryWidget final : public QWidget
{
public:
 explicit ScreenshotSummaryWidget(QWidget* parent = nullptr) : QWidget(parent)
 {
  setMinimumWidth(250);
  setAccessibleName(QStringLiteral("Screenshot export summary"));
 }

 void setSources(QLineEdit* path, QComboBox* format, QSpinBox* quality,
                 QCheckBox* wholeWindow, QCheckBox* multiChannel)
 {
  path_ = path;
  format_ = format;
  quality_ = quality;
  wholeWindow_ = wholeWindow;
  multiChannel_ = multiChannel;
 }

protected:
 void paintEvent(QPaintEvent*) override
 {
  QPainter painter(this);
  painter.fillRect(rect(), kPanelBackground);
  painter.setPen(kBorderColor);
  painter.drawRect(rect().adjusted(0, 0, -1, -1));

  QFont heading = painter.font();
  heading.setBold(true);
  painter.setFont(heading);
  painter.setPen(kPrimaryText);
  painter.drawText(18, 30, QStringLiteral("Export summary"));

  heading.setBold(false);
  painter.setFont(heading);
  int y = 64;
  drawRow(painter, y, QStringLiteral("Destination"),
          path_ && !path_->text().trimmed().isEmpty()
              ? QFileInfo(path_->text()).fileName() : QStringLiteral("Not selected"));
  drawRow(painter, y, QStringLiteral("Format"),
          format_ ? format_->currentText() : QStringLiteral("PNG"));
  if (format_ && normalizedScreenshotFormat(format_->currentData().toString()) == QStringLiteral("jpg")) {
   drawRow(painter, y, QStringLiteral("Quality"),
           quality_ ? QString::number(quality_->value()) : QStringLiteral("95"));
  }
  drawRow(painter, y, QStringLiteral("Capture"),
          wholeWindow_ && wholeWindow_->isChecked()
              ? QStringLiteral("Whole editor window") : QStringLiteral("Renderer area"));
  drawRow(painter, y, QStringLiteral("Channels"),
          multiChannel_ && multiChannel_->isChecked()
              ? QStringLiteral("Multi-channel AOV") : QStringLiteral("Standard image"));

  painter.setPen(kMutedText);
  painter.drawText(rect().adjusted(18, y + 10, -18, -16),
                   Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap,
                   QStringLiteral("Summary only. No preview image or additional GPU readback is generated."));
 }

private:
 QLineEdit* path_ = nullptr;
 QComboBox* format_ = nullptr;
 QSpinBox* quality_ = nullptr;
 QCheckBox* wholeWindow_ = nullptr;
 QCheckBox* multiChannel_ = nullptr;

 static void drawRow(QPainter& painter, int& y, const QString& label, const QString& value)
 {
  painter.setPen(kMutedText);
  painter.drawText(18, y, label);
  painter.setPen(kPrimaryText);
  painter.drawText(QRect(104, y - 16, painter.viewport().width() - 122, 22),
                   Qt::AlignRight | Qt::AlignVCenter, value);
  y += 34;
 }
};

class SummaryRefreshFilter final : public QObject
{
public:
 explicit SummaryRefreshFilter(ScreenshotSummaryWidget* summary, QObject* parent)
     : QObject(parent), summary_(summary) {}

protected:
 bool eventFilter(QObject* watched, QEvent* event) override
 {
  const auto type = event->type();
  if (type == QEvent::KeyRelease || type == QEvent::MouseButtonRelease ||
      type == QEvent::Wheel || type == QEvent::FocusOut) {
   summary_->update();
  }
  return QObject::eventFilter(watched, event);
 }

private:
 ScreenshotSummaryWidget* summary_ = nullptr;
};

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
 QCheckBox* multiChannelCheck = nullptr;
 QDialogButtonBox* buttonBox = nullptr;
 ScreenshotSummaryWidget* summary = nullptr;

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
 const bool isExr = normalizedScreenshotFormat(format) == QStringLiteral("exr");
 jpegQualityLabel->setEnabled(isJpeg);
 jpegQualitySpin->setEnabled(isJpeg);
 if (multiChannelCheck) {
  const QSignalBlocker blocker(multiChannelCheck);
  multiChannelCheck->setEnabled(isExr);
  if (!isExr) {
   multiChannelCheck->setChecked(false);
  }
 }

 if (filePathEdit && !filePathEdit->text().trimmed().isEmpty()) {
  const QSignalBlocker blocker(filePathEdit);
  filePathEdit->setText(pathWithScreenshotSuffix(filePathEdit->text(), format));
 }
 if (summary) {
  summary->update();
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
 if (summary) {
  summary->update();
 }
}

W_OBJECT_IMPL(ArtifactScreenshotExportDialog)

ArtifactScreenshotExportDialog::ArtifactScreenshotExportDialog(QWidget* parent)
    : QDialog(parent), impl_(new Impl())
{
 setWindowTitle(QStringLiteral("Screenshot Export"));
 setAccessibleName(QStringLiteral("Screenshot Export Dialog"));
 setAccessibleDescription(QStringLiteral("Configure the destination and format for a composition screenshot"));
 setMinimumSize(760, 430);
 setWindowColor(this, kDialogBackground);

 auto* root = new QVBoxLayout(this);
 root->setContentsMargins(24, 22, 24, 18);
 root->setSpacing(16);
 auto* title = new QLabel(QStringLiteral("Screenshot Export"), this);
 QFont titleFont = title->font();
 titleFont.setBold(true);
 titleFont.setPointSize(14);
 title->setFont(titleFont);
 setLabelColor(title, kPrimaryText);
 root->addWidget(title);

 auto* body = new QHBoxLayout();
 body->setSpacing(24);
 auto* controls = new QWidget(this);
 auto* controlsLayout = new QVBoxLayout(controls);
 controlsLayout->setContentsMargins(0, 0, 0, 0);
 controlsLayout->setSpacing(10);

 controlsLayout->addWidget(makeSectionLabel(QStringLiteral("OUTPUT DESTINATION"), controls));
 auto* pathRow = new QHBoxLayout();

 auto* pathLabel = new QLabel(QStringLiteral("File"), controls);
 impl_->filePathEdit = new QLineEdit(controls);
 impl_->browseButton = new QPushButton(QStringLiteral("Browse..."), controls);
 pathLabel->setBuddy(impl_->filePathEdit);
 impl_->filePathEdit->setAccessibleName(QStringLiteral("Screenshot file path"));
 impl_->filePathEdit->setAccessibleDescription(QStringLiteral("Path where the screenshot will be saved"));
 impl_->browseButton->setAccessibleName(QStringLiteral("Browse for screenshot path"));
 impl_->browseButton->setAccessibleDescription(QStringLiteral("Choose the screenshot output file"));
 pathRow->addWidget(pathLabel);
 pathRow->addWidget(impl_->filePathEdit, 1);
 pathRow->addWidget(impl_->browseButton);
 controlsLayout->addLayout(pathRow);

 auto addDivider = [controls, controlsLayout]() {
  auto* divider = new QFrame(controls);
  divider->setFrameShape(QFrame::HLine);
  divider->setFrameShadow(QFrame::Plain);
  QPalette palette = divider->palette();
  palette.setColor(QPalette::WindowText, kBorderColor);
  divider->setPalette(palette);
  controlsLayout->addWidget(divider);
 };
 addDivider();
 controlsLayout->addWidget(makeSectionLabel(QStringLiteral("IMAGE FORMAT"), controls));
 auto* formatGrid = new QGridLayout();
 formatGrid->setHorizontalSpacing(12);
 formatGrid->setVerticalSpacing(10);

 auto* formatLabel = new QLabel(QStringLiteral("Format"), controls);
 impl_->formatCombo = new QComboBox(controls);
 formatLabel->setBuddy(impl_->formatCombo);
 impl_->formatCombo->setAccessibleName(QStringLiteral("Screenshot format"));
 impl_->formatCombo->setAccessibleDescription(QStringLiteral("Image format for the screenshot"));
 impl_->formatCombo->addItem(QStringLiteral("PNG"), QStringLiteral("png"));
 impl_->formatCombo->addItem(QStringLiteral("JPEG"), QStringLiteral("jpg"));
 impl_->formatCombo->addItem(QStringLiteral("EXR"), QStringLiteral("exr"));
 formatGrid->addWidget(formatLabel, 0, 0);
 formatGrid->addWidget(impl_->formatCombo, 0, 1);

 impl_->jpegQualityLabel = new QLabel(QStringLiteral("JPEG Quality"), controls);
 impl_->jpegQualitySpin = new QSpinBox(controls);
 impl_->jpegQualityLabel->setBuddy(impl_->jpegQualitySpin);
 impl_->jpegQualitySpin->setAccessibleName(QStringLiteral("JPEG quality"));
 impl_->jpegQualitySpin->setAccessibleDescription(QStringLiteral("JPEG compression quality from 1 to 100"));
 impl_->jpegQualitySpin->setRange(1, 100);
 impl_->jpegQualitySpin->setValue(95);
 formatGrid->addWidget(impl_->jpegQualityLabel, 1, 0);
 formatGrid->addWidget(impl_->jpegQualitySpin, 1, 1);
 controlsLayout->addLayout(formatGrid);

 impl_->captureWholeWindowCheck =
     new QCheckBox(QStringLiteral("Capture whole editor window"), controls);
 impl_->captureWholeWindowCheck->setAccessibleName(QStringLiteral("Capture whole editor window"));
 impl_->captureWholeWindowCheck->setAccessibleDescription(QStringLiteral("Capture the entire editor window instead of the renderer area"));
 impl_->captureWholeWindowCheck->setChecked(false);
 impl_->multiChannelCheck =
     new QCheckBox(QStringLiteral("Multi-channel EXR (AOV)"), controls);
 impl_->multiChannelCheck->setAccessibleName(QStringLiteral("Multi-channel EXR AOV"));
 impl_->multiChannelCheck->setAccessibleDescription(QStringLiteral("Include multiple render channels when exporting EXR"));
 impl_->multiChannelCheck->setChecked(false);

 addDivider();
 controlsLayout->addWidget(makeSectionLabel(QStringLiteral("CAPTURE RANGE"), controls));
 controlsLayout->addWidget(impl_->captureWholeWindowCheck);
 controlsLayout->addWidget(impl_->multiChannelCheck);
 controlsLayout->addStretch();

 impl_->summary = new ScreenshotSummaryWidget(this);
 impl_->summary->setSources(impl_->filePathEdit, impl_->formatCombo, impl_->jpegQualitySpin,
                           impl_->captureWholeWindowCheck, impl_->multiChannelCheck);
 body->addWidget(controls, 3);
 body->addWidget(impl_->summary, 2);
 root->addLayout(body, 1);

 impl_->buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
 impl_->buttonBox->setAccessibleName(QStringLiteral("Screenshot export actions"));
 if (auto* okButton = impl_->buttonBox->button(QDialogButtonBox::Ok)) {
  okButton->setText(QStringLiteral("Export"));
  okButton->setAccessibleName(QStringLiteral("Export screenshot"));
  okButton->setAccessibleDescription(QStringLiteral("Save the screenshot with the selected settings"));
 }
 if (auto* cancelButton = impl_->buttonBox->button(QDialogButtonBox::Cancel)) {
  cancelButton->setAccessibleName(QStringLiteral("Cancel screenshot export"));
  cancelButton->setAccessibleDescription(QStringLiteral("Close without exporting"));
 }

 root->addWidget(impl_->buttonBox);

 auto* refreshFilter = new SummaryRefreshFilter(impl_->summary, this);
 impl_->filePathEdit->installEventFilter(refreshFilter);
 impl_->formatCombo->installEventFilter(refreshFilter);
 impl_->jpegQualitySpin->installEventFilter(refreshFilter);
 impl_->captureWholeWindowCheck->installEventFilter(refreshFilter);
 impl_->multiChannelCheck->installEventFilter(refreshFilter);

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
 if (impl_->summary) {
  impl_->summary->update();
 }
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
  if (impl_->summary) {
   impl_->summary->update();
  }
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
  if (impl_->summary) {
   impl_->summary->update();
  }
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

void ArtifactScreenshotExportDialog::setMultiChannelEnabled(bool enabled)
{
 if (impl_->multiChannelCheck) {
  const QSignalBlocker blocker(impl_->multiChannelCheck);
  impl_->multiChannelCheck->setChecked(enabled);
  if (impl_->summary) {
   impl_->summary->update();
  }
 }
}

bool ArtifactScreenshotExportDialog::multiChannelEnabled() const
{
 return impl_->multiChannelCheck ? impl_->multiChannelCheck->isChecked() : false;
}

void ArtifactScreenshotExportDialog::setOptions(const ScreenshotExportOptions& options)
{
 setFilePath(options.filePath);
 setFormat(options.format);
 setJpegQuality(options.jpegQuality);
 setCaptureSource(options.multiChannel ? ScreenshotCaptureSource::Renderer
                                       : options.captureSource);
 setMultiChannelEnabled(options.multiChannel);
}

ScreenshotExportOptions ArtifactScreenshotExportDialog::options() const
{
 ScreenshotExportOptions options;
 options.filePath = filePath();
 options.format = multiChannelEnabled() ? QStringLiteral("exr") : format();
 options.jpegQuality = jpegQuality();
 options.captureSource = multiChannelEnabled() ? ScreenshotCaptureSource::Renderer
                                               : captureSource();
 options.multiChannel = multiChannelEnabled();
 return options;
}

}
