module;
#include <QDialogButtonBox>
#include <QVBoxLayout>
#include <QWidget>
#include <QDialog>
#include <QLineEdit>
#include <QFormLayout>
#include <QPropertyAnimation>
#include <wobjectimpl.h>
module Artifact.Widgets.CreateLayerDialog;

import Widgets.Dialog.Abstract;
import Artifact.Layers.Abstract;
import Widgets.Utils.CSS;
import Widgets.EditableLabel;

namespace Artifact {
	
 using namespace ArtifactWidgets;

 class PlaneLayerSettingPage::Impl {
 private:

 public:
  Impl();
  ~Impl() = default;
  EditableLabel* layerName = nullptr;
 };

 PlaneLayerSettingPage::Impl::Impl()
 {

 }

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
 // ReSharper disable CppUnusedFunction
 W_OBJECT_IMPL(CreateSolidLayerSettingDialog)
  // ReSharper restore CppUnusedFunction
	
  class CreateSolidLayerSettingDialog::Impl
 {

 private:

 public:
  EditableLabel* nameEditableLabel = nullptr;
  QDialogButtonBox* dialogButtonBox = nullptr;
  QPropertyAnimation* m_showAnimation = nullptr;
  QPropertyAnimation* m_hideAnimation = nullptr;
  Impl();
  ~Impl()=default;
 };

 CreateSolidLayerSettingDialog::Impl::Impl()
 {

 }

 CreateSolidLayerSettingDialog::CreateSolidLayerSettingDialog(QWidget* parent /*= nullptr*/) :QDialog(parent),impl_(new Impl())
 {
  setFixedSize(600, 400);
  setWindowFlags(Qt::FramelessWindowHint);
  setAttribute(Qt::WA_NoChildEventsForParent);
  QVBoxLayout* layout = new QVBoxLayout();
 	
  auto formLayout = new QFormLayout();

  auto editableLabel = impl_->nameEditableLabel = new EditableLabel();
  auto dialogButtonBox = impl_->dialogButtonBox = new QDialogButtonBox();
  dialogButtonBox->setStandardButtons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
 	layout->addWidget(editableLabel);
  layout->addWidget(dialogButtonBox);
  setLayout(layout);
  auto style = getDCCStyleSheetPreset(DccStylePreset::ModoStyle);

  setStyleSheet(style);
 }

 CreateSolidLayerSettingDialog::~CreateSolidLayerSettingDialog()
 {
  delete impl_;
 }

 void CreateSolidLayerSettingDialog::keyPressEvent(QKeyEvent* event)
 {

 }

 void CreateSolidLayerSettingDialog::mousePressEvent(QMouseEvent* event)
 {

 }

 void CreateSolidLayerSettingDialog::mouseReleaseEvent(QMouseEvent* event)
 {

 }

 void CreateSolidLayerSettingDialog::mouseMoveEvent(QMouseEvent* event)
 {

 }

 W_OBJECT_IMPL(EditPlaneLayerSettingDialog)

  EditPlaneLayerSettingDialog::EditPlaneLayerSettingDialog(QWidget* parent /*= nullptr*/) :QDialog(parent)
 {

 }

 EditPlaneLayerSettingDialog::~EditPlaneLayerSettingDialog()
 {

 }

 void EditPlaneLayerSettingDialog::keyPressEvent(QKeyEvent* event)
 {

 }

 void EditPlaneLayerSettingDialog::mousePressEvent(QMouseEvent* event)
 {

 }

 void EditPlaneLayerSettingDialog::mouseReleaseEvent(QMouseEvent* event)
 {

 }

 void EditPlaneLayerSettingDialog::mouseMoveEvent(QMouseEvent* event)
 {

 }

 void EditPlaneLayerSettingDialog::showAnimated()
 {

 }

};