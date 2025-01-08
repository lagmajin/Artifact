#pragma once

#include <QtCore/QScopedPointer>
#include <QtWidgets/QWidget>

namespace Artifact {

class PlaneLayerSettingPagePrivate;

 class PlaneLayerSettingPage :public QWidget {
  Q_OBJECT
 private:
  
 protected:

 public:
  explicit PlaneLayerSettingPage(QWidget* parent = nullptr);
  ~PlaneLayerSettingPage();
  void setDefaultFocus();
 signals:
  void editComplete();
 private slots:
  void spouitMode();
  void resizeCompositionSize();
 public slots:

 };





};