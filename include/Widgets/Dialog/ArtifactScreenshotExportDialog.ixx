module;
#include <QDialog>
#include <QString>
#include <wobjectdefs.h>
export module Artifact.Widget.Dialog.ScreenshotExport;

export namespace Artifact
{

enum class ScreenshotCaptureSource
{
 Renderer,
 WholeWindow,
};

struct ScreenshotExportOptions
{
 QString filePath;
 QString format = QStringLiteral("png");
 int jpegQuality = 95;
 ScreenshotCaptureSource captureSource = ScreenshotCaptureSource::Renderer;
};

class ArtifactScreenshotExportDialog : public QDialog
{
 W_OBJECT(ArtifactScreenshotExportDialog)
private:
 class Impl;
 Impl* impl_;

protected:
 void accept() override;

public:
 explicit ArtifactScreenshotExportDialog(QWidget* parent = nullptr);
 ~ArtifactScreenshotExportDialog();

 void setFilePath(const QString& path);
 [[nodiscard]] QString filePath() const;
 void setFormat(const QString& format);
 [[nodiscard]] QString format() const;
 void setJpegQuality(int quality);
 [[nodiscard]] int jpegQuality() const;
 void setCaptureSource(ScreenshotCaptureSource source);
 [[nodiscard]] ScreenshotCaptureSource captureSource() const;
 void setOptions(const ScreenshotExportOptions& options);
 [[nodiscard]] ScreenshotExportOptions options() const;
};

}
