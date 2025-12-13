module;
#include <QDialog>
#include <wobjectimpl.h>

module Artifact.Dialog.EditComposition;

import std;

namespace Artifact
{
 class ArtifactEditCompositionSettingPage::Impl
 {
 public:
 	
 };
	
 ArtifactEditCompositionSettingPage::ArtifactEditCompositionSettingPage(QWidget* parent /*= nullptr*/):QWidget(parent),impl_(new Impl)
 {

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

