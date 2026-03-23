module;
#include <QDialog>
#include <QWidget>
#include <QString>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QShowEvent>
#include <QCloseEvent>
#include <wobjectdefs.h>
export module Dialog.Composition;

import Artifact.Composition.InitParams;


export namespace Artifact {

 enum class eCompositionSettingType {
  CreateNewComposition,
  ChangeCompositionSetting,
 };

 class CompositionSettingPage :public QWidget {
 private:
  class Impl;
  Impl* impl_;
 public:
  explicit CompositionSettingPage(QWidget* parent = nullptr);
  ~CompositionSettingPage();
  ArtifactCompositionInitParams getInitParams(const QString& name) const;
 };

 class CompositionExtendSettingPage :public QWidget {

 private:

 protected:

 public:
  explicit CompositionExtendSettingPage(QWidget* parent = nullptr);
  ~CompositionExtendSettingPage();


 };

 class CompositionAudioSettingPage :public QWidget {

 };




 class CreateCompositionDialog :public QDialog{
 W_OBJECT(CreateCompositionDialog)
 private:
  class Impl;
  Impl* impl_;
 protected:
  void keyPressEvent(QKeyEvent* event) override;
  void mousePressEvent(QMouseEvent* event) override;
  void mouseReleaseEvent(QMouseEvent* event) override;
  void mouseMoveEvent(QMouseEvent* event) override;
  void showEvent(QShowEvent* event) override;
  void closeEvent(QCloseEvent* event) override;
 public:
  explicit CreateCompositionDialog(QWidget* parent = nullptr);
  ~CreateCompositionDialog();
  void setDefaultFocus();
  void setCompositionName(const QString& compositionName);
  QString compositionName() const;
  ArtifactCompositionInitParams acceptedInitParams() const;
  void showAnimated();

  
  //void compositionSettingChanged()
 };




}
