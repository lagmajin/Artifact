module;
#include <wobjectimpl.h>
#include <QDialog>
#include <QFormLayout>
module Artifact.Widget.Dialog.RenderOutputSetting;


namespace Artifact
{
	
 class ArtifactRenderOutputSettingDialog::Impl
 {
 private:
 	
 public:
  Impl();
  ~Impl();
 };

 ArtifactRenderOutputSettingDialog::Impl::Impl()
 {

 }

 ArtifactRenderOutputSettingDialog::Impl::~Impl()
 {

 }
	
	W_OBJECT_IMPL(ArtifactRenderOutputSettingDialog)
	
 ArtifactRenderOutputSettingDialog::ArtifactRenderOutputSettingDialog(QWidget* parent /*= nullptr*/):QDialog(parent),impl_(new Impl())
 {
	 auto vboxLayout = new QFormLayout();
 }

 ArtifactRenderOutputSettingDialog::~ArtifactRenderOutputSettingDialog()
 {
  delete impl_;
 }

};
