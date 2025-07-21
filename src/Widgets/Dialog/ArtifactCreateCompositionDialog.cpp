module;
#include <QTime>
#include <QWidget>
#include <QDialog>
#include <QTabWidget>
#include <QDialogButtonBox>
#include <QBoxLayout>

#include <QEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include "qevent.h"
module Dialog.Composition;

namespace Artifact {

 CompositionSettingPage::CompositionSettingPage(QWidget* parent /*= nullptr*/) :QWidget(parent)
 {

 }

 CompositionSettingPage::~CompositionSettingPage()
 {

 }

 class CreateCompositionDialog::Impl {
 private:

 public:
  Impl(CreateCompositionDialog* pDialog = nullptr);
  CompositionSettingPage* compositionSettingPage_ = nullptr;
  CompositionAudioSettingPage* audioSettingPage_ = nullptr;
  QTabWidget* pTabWidget = nullptr;
  QPoint m_dragPosition;
  bool m_isDragging = false;
  void ok();
  void cancel();
 };

 CreateCompositionDialog::Impl::Impl(CreateCompositionDialog* pDialog)
 {
  //pTabWidget = new QTabWidget();


 }

 void CreateCompositionDialog::Impl::ok()
 {

 }

 void CreateCompositionDialog::Impl::cancel()
 {

 }

 CreateCompositionDialog::CreateCompositionDialog(QWidget* parent /*= nullptr*/) :QDialog(parent),impl_(new Impl(this))
 {
  setWindowTitle(u8"コンポジション設定");
  setWindowFlags(Qt::FramelessWindowHint);
  setAttribute(Qt::WA_NoChildEventsForParent);

  setFixedSize(600, 400);
  static int16_t beforeSelectHeight = 400;

  static QTime	beforeSelectTime(0, 0, 0);

  static QTime	beforeSelectDurationTime(0, 0, 30);

  static float	beforeSelectFrameRate = 30.0f;

  impl_->pTabWidget = new QTabWidget(this);

  impl_->compositionSettingPage_ = new CompositionSettingPage();

  impl_->pTabWidget->addTab(impl_->compositionSettingPage_,"Settings");



  QDialogButtonBox* const pDialogButtonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
  
  QVBoxLayout* const pVBoxLayout = new QVBoxLayout();

  pVBoxLayout->addWidget(impl_->pTabWidget);
  pVBoxLayout->addWidget(pDialogButtonBox);
  setLayout(pVBoxLayout);



 }

 CreateCompositionDialog::~CreateCompositionDialog()
 {

 }

 void CreateCompositionDialog::setCompositionName(const QString& compositionName)
 {

 }

 void CreateCompositionDialog::keyPressEvent(QKeyEvent* event)
 {
  if (event->key() == Qt::Key_Escape)
  {
   close();
  }

  if (event->key() == Qt::Key_Enter)
  {
   accept();
  }
 }

 void CreateCompositionDialog::mousePressEvent(QMouseEvent* event)
 {
  // (ここではダイアログ全体をドラッグ対象とする)
  if (event->button() == Qt::LeftButton) {
   // ドラッグ開始時のマウスのグローバル座標を記録
   impl_->m_dragPosition = event->globalPosition().toPoint() - frameGeometry().topLeft();
   // ドラッグモードに入ったことを示すフラグ
   impl_->m_isDragging = true;
   event->accept(); // イベントを処理済みとしてマーク
  }
  else {
   // 親クラスのイベントハンドラを呼び出す（他のマウスボタンのため）
   QDialog::mousePressEvent(event);
  }
 }

 void CreateCompositionDialog::mouseReleaseEvent(QMouseEvent* event)
 {
  if (impl_->m_isDragging && event->button() == Qt::LeftButton) {
   // ドラッグモードを終了
   impl_->m_isDragging = false;
   event->accept(); // イベントを処理済みとしてマーク
  }
  else {
   // 親クラスのイベントハンドラを呼び出す
   QDialog::mouseReleaseEvent(event);
  }
 }

 void CreateCompositionDialog::mouseMoveEvent(QMouseEvent* event)
 {
  if (impl_->m_isDragging && event->buttons() & Qt::LeftButton) {
   // ダイアログの新しい位置を計算し、移動
   move(event->globalPosition().toPoint() - impl_->m_dragPosition);
   event->accept(); // イベントを処理済みとしてマーク
  }
  else {
   // 親クラスのイベントハンドラを呼び出す
   QDialog::mouseMoveEvent(event);
  }
 }

 CompositionExtendSettingPage::CompositionExtendSettingPage(QWidget* parent /*= nullptr*/) :QWidget(parent)
 {

 }

 CompositionExtendSettingPage::~CompositionExtendSettingPage()
 {

 }



}


