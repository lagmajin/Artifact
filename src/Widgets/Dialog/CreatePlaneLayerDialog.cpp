module;
#include <QDialogButtonBox>
#include <QVBoxLayout>
#include <QWidget>
module Dialog;


//#include "../../../include/Widgets/Dialog/CreatePlaneLayerDialog.hpp"







namespace Artifact {

 class PlaneLayerSettingPagePrivate {
 private:

 public:
  PlaneLayerSettingPagePrivate();
  ~PlaneLayerSettingPagePrivate();
 };

 PlaneLayerSettingPagePrivate::PlaneLayerSettingPagePrivate()
 {

 }

 PlaneLayerSettingPagePrivate::~PlaneLayerSettingPagePrivate()
 {

 }

 PlaneLayerSettingPage::PlaneLayerSettingPage(QWidget* parent /*= nullptr*/) :QWidget(parent)
 {

 }

 PlaneLayerSettingPage::~PlaneLayerSettingPage()
 {

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
 public:
  QDialogButtonBox* dialogButtonBox = nullptr;
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