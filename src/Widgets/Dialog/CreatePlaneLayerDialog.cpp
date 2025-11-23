module;
#include <QDialogButtonBox>
#include <QVBoxLayout>
#include <QWidget>
module Dialog;

import Widgets.Dialog.Abstract;
import Artifact.Layers.Abstract;

namespace Artifact {

 class PlaneLayerSettingPage::Impl {
 private:

 public:
  
 };


 PlaneLayerSettingPage::PlaneLayerSettingPage(QWidget* parent /*= nullptr*/) :QWidget(parent), impl_(new Impl())
 {

 }

PlaneLayerSettingPage::~PlaneLayerSettingPage()
 {
  delete impl_;
 }

 void PlaneLayerSettingPage::setDefaultFocus()
 {

 }

 void PlaneLayerSettingPage::spouitMode()
 {

 }

 void PlaneLayerSettingPage::resizeCompositionSize()
 {

 }

 class PlaneLayerSettingDialog::Impl
 {

 private:

 public:
  QDialogButtonBox* dialogButtonBox = nullptr;
  Impl();
  ~Impl();
 };

 PlaneLayerSettingDialog::PlaneLayerSettingDialog(QWidget* parent /*= nullptr*/):QDialog(parent)
 {
  QVBoxLayout* layout = new QVBoxLayout();

  auto dialogButtonBox = impl_->dialogButtonBox = new QDialogButtonBox();

  layout->addWidget(dialogButtonBox);
  setLayout(layout);

 }

 PlaneLayerSettingDialog::~PlaneLayerSettingDialog()
 {

 }

};