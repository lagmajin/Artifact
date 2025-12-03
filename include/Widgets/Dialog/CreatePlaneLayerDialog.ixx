module;
#include <QDialog>
#include <QWidget>

#include <wobjectdefs.h>
export module Dialog;

import std;
import Widgets.Dialog.Abstract;
import Artifact.Layer.InitParams;

namespace Artifact {

class PlaneLayerSettingPagePrivate;

 class PlaneLayerSettingPage :public QWidget {
  //Q_OBJECT
 private:
  class Impl;
  Impl* impl_;
 protected:

 public:
  explicit PlaneLayerSettingPage(QWidget* parent = nullptr);
  ~PlaneLayerSettingPage();
  void setDefaultFocus();
 //signals:
  void editComplete();
 //private slots:
  void spouitMode();
  void resizeCompositionSize();
 //public slots:

 };

 class PlaneLayerSettingDialog final:public QDialog
 {
 	W_OBJECT(PlaneLayerSettingDialog)
 private:
  class Impl;
  Impl* impl_;
 protected:

 public:
  explicit PlaneLayerSettingDialog(QWidget* parent = nullptr);
  ~PlaneLayerSettingDialog();
  public/*signals*/:
 	void submit(const ArtifactSolidLayerInitParams& params) W_SIGNAL(submit,params)
 };

 class EditPlaneLayerSettingDialog final :public QDialog {
  W_OBJECT(PlaneLayerSettingDialog)
 private:
  class Impl;
  Impl* impl_;

 public:
  explicit EditPlaneLayerSettingDialog(QWidget* parent = nullptr);
  ~EditPlaneLayerSettingDialog();
 public/*signals*/:
 };

};