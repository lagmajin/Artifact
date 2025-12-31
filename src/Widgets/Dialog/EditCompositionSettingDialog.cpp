module;
#include <QDialog>
#include <wobjectimpl.h>
#include <QComboBox>

module Artifact.Dialog.EditComposition;

import std;
import Widgets.Utils.CSS;
import Widgets.EditableLabel;
import DragSpinBox;


namespace Artifact
{
 using namespace ArtifactCore;
 using namespace ArtifactWidgets;

 class ArtifactEditCompositionSettingPage::Impl
 {
 public:
  QComboBox* resolutionCombobox_ = nullptr;
  EditableLabel* compositionNameEdit_ = nullptr;
  DragSpinBox* widthSpinBox = nullptr;
  DragSpinBox* heightSpinBox = nullptr;
 };
	
 ArtifactEditCompositionSettingPage::ArtifactEditCompositionSettingPage(QWidget* parent /*= nullptr*/):QWidget(parent),impl_(new Impl)
 {
  impl_->widthSpinBox = new DragSpinBox();
  impl_->heightSpinBox = new DragSpinBox();

  auto line = new QFrame();
  line->setFrameShape(QFrame::HLine);
  line->setFrameShadow(QFrame::Sunken);
 }

 ArtifactEditCompositionSettingPage::~ArtifactEditCompositionSettingPage()
 {
  delete impl_;
 }
	
 class  ArtifactEditCompositionDialog::Impl
 {
 public:
  Impl();
  ~Impl();
 };

 ArtifactEditCompositionDialog::Impl::Impl()
 {

 }

	W_OBJECT_IMPL(ArtifactEditCompositionDialog)
	
 ArtifactEditCompositionDialog::ArtifactEditCompositionDialog(QWidget* parent /*= nullptr*/):QDialog(parent)
 {

 }

 ArtifactEditCompositionDialog::~ArtifactEditCompositionDialog()
 {

 }

 

};

