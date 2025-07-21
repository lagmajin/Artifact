module;
#include <QDialog>
export module Dialog.Composition;




export namespace Artifact {

 enum class eCompositionSettingType {
  CreateNewComposition,
  ChangeCompositionSetting,
 };

 class CompositionSettingPage :public QWidget {
 private:

 public:
  explicit CompositionSettingPage(QWidget* parent = nullptr);
  ~CompositionSettingPage();
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
 private:
  class Impl;
  Impl* impl_;
 protected:
  void keyPressEvent(QKeyEvent* event) override;


  void mousePressEvent(QMouseEvent* event) override;


  void mouseReleaseEvent(QMouseEvent* event) override;


  void mouseMoveEvent(QMouseEvent* event) override;

 public:
  explicit CreateCompositionDialog(QWidget* parent = nullptr);
  ~CreateCompositionDialog();
  void setDefaultFocus();
  void setCompositionName(const QString& compositionName);
  //CompositionSetting compositionSetting() const;
 };




}