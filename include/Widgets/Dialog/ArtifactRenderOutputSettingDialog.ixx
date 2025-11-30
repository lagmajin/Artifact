module;
#include <QDialog>
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
 };

};
