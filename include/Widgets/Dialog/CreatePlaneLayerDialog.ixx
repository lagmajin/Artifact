module;
#include <QDialog>
#include <QWidget>
export module Dialog;
//#pragma once

//#include <QtCore/QScopedPointer>
//




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

 class PlaneLayerSettingDialog:public QDialog
 {
 private:
  class Impl;
  Impl* impl_;
 protected:

 public:
  explicit PlaneLayerSettingDialog(QWidget* parent = nullptr);
  ~PlaneLayerSettingDialog();
 };



};