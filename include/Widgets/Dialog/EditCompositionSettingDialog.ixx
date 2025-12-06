module;
#include <wobjectdefs.h>
#include <QDialog>
export module Artifact.Dialog.EditComposition;
import std;
import Widgets.Dialog.Abstract;

export namespace Artifact {

 class ArtifactEditCompositionDialog:public QDialog
 {
 private:
  class Impl;
  Impl* impl_;
	
 protected:
 public:
  explicit ArtifactEditCompositionDialog(QWidget* parent = nullptr);
  ~ArtifactEditCompositionDialog();
 public /*signals*/: 
 };












};