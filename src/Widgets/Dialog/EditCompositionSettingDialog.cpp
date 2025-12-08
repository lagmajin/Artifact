module;
#include <QDialog>
#include <wobjectimpl.h>

module Artifact.Dialog.EditComposition;

import std;

namespace Artifact
{
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

