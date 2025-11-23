module;
#include <QDialog>
export module Dialog.EditComposition;

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
 };












};