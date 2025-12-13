module;
#include <wobjectdefs.h>
#include <QDialog>
export module Artifact.Dialog.EditComposition;
import std;
import Widgets.Dialog.Abstract;

export namespace Artifact {

 class ArtifactEditCompositionSettingPage:public QWidget
 {
 private:
  class Impl;
  Impl* impl_;
 public:
  explicit ArtifactEditCompositionSettingPage(QWidget* parent = nullptr);
  ~ArtifactEditCompositionSettingPage();
 };
	
 class ArtifactEditCompositionDialog:public QDialog
 {
 	W_OBJECT(ArtifactEditCompositionDialog)
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