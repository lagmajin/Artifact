module;
#include <QTime>
#include <QWidget>
#include <QDialog>
#include <QTabWidget>
#include <QDialogButtonBox>
#include <QBoxLayout>

#include <QEvent>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMouseEvent>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QPropertyAnimation>
#include <wobjectimpl.h>
#include <QFormLayout>
#include <QLabel>
#include "qevent.h"
module Dialog.Composition;

namespace Artifact {

 CompositionSettingPage::CompositionSettingPage(QWidget* parent /*= nullptr*/) :QWidget(parent)
 {

  auto compositionNameLabel = new QLabel("コンポジション名:");
  auto compositionNameEdit = new QLineEdit();


  auto vboxLayout = new QFormLayout();
  vboxLayout->addRow(compositionNameLabel,compositionNameEdit);
  //vboxLayout->addWidget(compositionNameEdit);

  setLayout(vboxLayout);
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
  void ok(QDialog* dialog);
  void cancel(QDialog* dialog);
  QPropertyAnimation* m_showAnimation = nullptr;
  QPropertyAnimation* m_hideAnimation = nullptr;
 };

 CreateCompositionDialog::Impl::Impl(CreateCompositionDialog* pDialog)
 {
  //pTabWidget = new QTabWidget();


 }

 void CreateCompositionDialog::Impl::ok(QDialog* dialog)
{
  dialog->accept();
 }

 void CreateCompositionDialog::Impl::cancel(QDialog* dialog)
 {
  dialog->reject();
 }

 W_OBJECT_IMPL(CreateCompositionDialog)

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

  QObject::connect(pDialogButtonBox, &QDialogButtonBox::accepted, this, [this]() {
   impl_->ok(this);
   });
  QObject::connect(pDialogButtonBox, &QDialogButtonBox::rejected, this, [this]() {
   impl_->cancel(this);
   });
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

 void CreateCompositionDialog::showAnimated()
 {
  // ダイアログを一時的に画面外に配置
  QPoint startPos;
  if (parentWidget()) {
   QRect parentRect = parentWidget()->geometry();
   startPos.setX(parentRect.x() + (parentRect.width() - width()) / 2);
  }
  else {
   QScreen* screen = QGuiApplication::primaryScreen();
   QRect screenGeometry = screen->availableGeometry();
   startPos.setX(screenGeometry.x() + (screenGeometry.width() - width()) / 2);
  }
  startPos.setY(QGuiApplication::primaryScreen()->geometry().bottom()); // 画面下端

  // 最終的な表示位置
  QPoint endPos;
  if (parentWidget()) {
   QRect parentRect = parentWidget()->geometry();
   endPos.setX(parentRect.x() + (parentRect.width() - width()) / 2);
   endPos.setY(parentRect.y() + (parentRect.height() - height()) / 2);
  }
  else {
   QScreen* screen = QGuiApplication::primaryScreen();
   QRect screenGeometry = screen->availableGeometry();
   endPos.setX(screenGeometry.x() + (screenGeometry.width() - width()) / 2);
   endPos.setY(screenGeometry.y() + (screenGeometry.height() - height()) / 2);
  }

  // アニメーションの設定
  QPropertyAnimation* animation = new QPropertyAnimation(this, "pos");
  animation->setDuration(300);
  animation->setStartValue(startPos);
  animation->setEndValue(endPos);
  animation->setEasingCurve(QEasingCurve::OutQuad);

  // ダイアログを表示
  show(); // まずダイアログ自体を表示状態にする

  // アニメーション開始
  animation->start(QAbstractAnimation::DeleteWhenStopped); // アニメーション終了後に自動的にアニメーションオブジェクトを削除
 }

 void CreateCompositionDialog::showEvent(QShowEvent* event)
 {
  QDialog::showEvent(event);

  if (impl_->m_showAnimation &&impl_->m_showAnimation->state() == QAbstractAnimation::Running) {
   return; // すでにアニメーション中なら何もしない
  }

  QPoint endPos;
  if (parentWidget()) {
   QRect parentRect = parentWidget()->geometry();
   endPos.setX(parentRect.x() + (parentRect.width() - width()) / 2);
   endPos.setY(parentRect.y() + (parentRect.height() - height()) / 2);
  }
  else {
   QScreen* screen = QGuiApplication::primaryScreen();
   QRect screenGeometry = screen->availableGeometry();
   endPos.setX(screenGeometry.x() + (screenGeometry.width() - width()) / 2);
   endPos.setY(screenGeometry.y() + (screenGeometry.height() - height()) / 2);
  }

  // 2. アニメーションの開始位置を計算 (画面下端から出現)
  QPoint startPos=endPos;
  const float offsetFactor = 0.1f; // 動きの量 (ダイアログの高さに対する割合)
  startPos.setY(startPos.y() + static_cast<int>(height() * offsetFactor));
  // 3. ダイアログの初期位置をアニメーション開始位置に設定
  // これをしないと、show() でダイアログが一瞬本来の位置に表示されてしまう可能性がある
  move(startPos);

  // 4. QPropertyAnimation オブジェクトを作成（メンバ変数に割り当てる）
  impl_->m_showAnimation = new QPropertyAnimation(this, "pos", this); // 親を this にすることで、ダイアログが破棄されると自動的にアニメーションも破棄される
  impl_->m_showAnimation->setDuration(300); // アニメーション時間 (ミリ秒)
  impl_->m_showAnimation->setStartValue(startPos);
  impl_->m_showAnimation->setEndValue(endPos);
  impl_->m_showAnimation->setEasingCurve(QEasingCurve::OutQuad);

  // アニメーションの開始
  impl_->m_showAnimation->start(); // DeleteWhenStopped を指定しない (メンバ変数で管理する

 }

 void CreateCompositionDialog::closeEvent(QCloseEvent* event)
 {

 }

 CompositionExtendSettingPage::CompositionExtendSettingPage(QWidget* parent /*= nullptr*/) :QWidget(parent)
 {

 }

 CompositionExtendSettingPage::~CompositionExtendSettingPage()
 {

 }



}


