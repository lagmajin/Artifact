module;
#include <QDialog>
#include <QString>
#include <wobjectdefs.h>
export module Artifact.Widget.Dialog.RenderOutputSetting;

export namespace Artifact
{

 class ArtifactRenderOutputSettingDialog :public QDialog
 {
  W_OBJECT(ArtifactRenderOutputSettingDialog)
 private:
  class Impl;
  Impl* impl_;
 protected:
 public:
  explicit ArtifactRenderOutputSettingDialog(QWidget* parent = nullptr);
  ~ArtifactRenderOutputSettingDialog();
  void setOutputPath(const QString& path);
  [[nodiscard]] QString outputPath() const;
  void setOutputFormat(const QString& format);
  [[nodiscard]] QString outputFormat() const;
  void setCodec(const QString& codec);
  [[nodiscard]] QString codec() const;
  void setCodecProfile(const QString& profile);
  [[nodiscard]] QString codecProfile() const;
  void setEncoderBackend(const QString& backend);
  [[nodiscard]] QString encoderBackend() const;
  void setResolution(int width, int height);
  [[nodiscard]] int outputWidth() const;
  [[nodiscard]] int outputHeight() const;
  void setFrameRate(double fps);
  [[nodiscard]] double frameRate() const;
  void setBitrateKbps(int bitrateKbps);
  [[nodiscard]] int bitrateKbps() const;
 };

};
