module;
#include <QDialog>
#include <QWidget>

#include <wobjectdefs.h>
export module Artifact.Widgets.CreateLayerDialog;

import std;
import Widgets.Dialog.Abstract;
import Artifact.Layer.InitParams;

export namespace Artifact {
	

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

 class CreateSolidLayerSettingDialog final:public QDialog
 {
 	W_OBJECT(CreateSolidLayerSettingDialog)
 private:
  class Impl;
  Impl* impl_;
 protected:
  void keyPressEvent(QKeyEvent* event) override;
  void mousePressEvent(QMouseEvent* event) override;
  void mouseReleaseEvent(QMouseEvent* event) override;
  void mouseMoveEvent(QMouseEvent* event) override;
 public:
  explicit CreateSolidLayerSettingDialog(QWidget* parent = nullptr);
  ~CreateSolidLayerSettingDialog();
  void showAnimated();
  public/*signals*/:
 	void submit(const ArtifactSolidLayerInitParams& params) W_SIGNAL(submit,params)
 };

 class EditPlaneLayerSettingDialog final :public QDialog {
  W_OBJECT(EditPlaneLayerSettingDialog)
 private:
  class Impl;
  Impl* impl_;
 protected:
  void keyPressEvent(QKeyEvent* event) override;
  void mousePressEvent(QMouseEvent* event) override;
  void mouseReleaseEvent(QMouseEvent* event) override;
  void mouseMoveEvent(QMouseEvent* event) override;
 public:
  explicit EditPlaneLayerSettingDialog(QWidget* parent = nullptr);
  ~EditPlaneLayerSettingDialog();
  void showAnimated();
 public/*signals*/:
 };

};