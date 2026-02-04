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
#include <QComboBox>
#include "qevent.h"
module Dialog.Composition;

import Widgets.Utils.CSS;
import Widgets.EditableLabel;
import DragSpinBox;
import Artifact.Project.Manager;
import Utils.String.UniString;

namespace Artifact {

 using namespace ArtifactCore;
 using namespace ArtifactWidgets;
	
 class CompositionSettingPage::Impl
 {
 private:

 public:
  Impl();
  
 	QComboBox* resolutionCombobox_ = nullptr;
    EditableLabel* compositionNameEdit_=nullptr;
    DragSpinBox* widthSpinBox = nullptr;
    DragSpinBox* heightSpinBox = nullptr;
 };

 CompositionSettingPage::Impl::Impl()
 {

 }

 CompositionSettingPage::CompositionSettingPage(QWidget* parent /*= nullptr*/) :QWidget(parent),impl_(new Impl)
 {

  //auto compositionNameLabel = new QLabel("コンポジション名:");
  //auto compositionNameEdit=impl_->compositionNameEdit_ = new EditableLabel();
  //compositionNameEdit->setMaximumWidth(100);
 	
  impl_->widthSpinBox = new DragSpinBox();
  impl_->heightSpinBox = new DragSpinBox();
  impl_->resolutionCombobox_ = new QComboBox();
  impl_->resolutionCombobox_->addItem("1920x1080", QVariant::fromValue(QSize(1920,1080)));
  impl_->resolutionCombobox_->addItem("1280x720", QVariant::fromValue(QSize(1280,720)));
  impl_->resolutionCombobox_->addItem("カスタム", QVariant::fromValue(QSize(-1,-1)));

  QComboBox* fpsCombo = new QComboBox();
  fpsCombo->addItem("23.976");
  fpsCombo->addItem("24");
  fpsCombo->addItem("25");
  fpsCombo->addItem("29.97");
  fpsCombo->addItem("30");
  fpsCombo->addItem("60");

  auto line = new QFrame();
  line->setFrameShape(QFrame::HLine);
  line->setFrameShadow(QFrame::Sunken);

  auto vboxLayout = new QFormLayout();
  vboxLayout->addRow("解像度プリセット:", impl_->resolutionCombobox_);
  vboxLayout->addRow("Width:",impl_->widthSpinBox);
  vboxLayout->addRow("Height:", impl_->heightSpinBox);
  vboxLayout->addRow("フレームレート:", fpsCombo);
  vboxLayout->setAlignment(impl_->widthSpinBox, Qt::AlignRight);
  vboxLayout->setAlignment(impl_->heightSpinBox, Qt::AlignRight);
  vboxLayout->addRow(line);
  setLayout(vboxLayout);

  QObject::connect(impl_->resolutionCombobox_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx){
    QSize sz = impl_->resolutionCombobox_->currentData().toSize();
    if (sz.width() > 0 && sz.height() > 0) {
      impl_->widthSpinBox->setValue(sz.width());
      impl_->heightSpinBox->setValue(sz.height());
      impl_->widthSpinBox->setEnabled(false);
      impl_->heightSpinBox->setEnabled(false);
    } else {
      impl_->widthSpinBox->setEnabled(true);
      impl_->heightSpinBox->setEnabled(true);
    }
  });
  impl_->resolutionCombobox_->setCurrentIndex(0);

  auto style = getDCCStyleSheetPreset(DccStylePreset::ModoStyle);
  setStyleSheet(style);

 }

 CompositionSettingPage::~CompositionSettingPage()
 {

 }
 CompositionExtendSettingPage::CompositionExtendSettingPage(QWidget* parent /*= nullptr*/) :QWidget(parent)
 {

 }

 CompositionExtendSettingPage::~CompositionExtendSettingPage()
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

  QLabel* compositionLabel = nullptr;
  QLineEdit* compositionName = nullptr;
  EditableLabel* compositionNameEdit_;
 };

 CreateCompositionDialog::Impl::Impl(CreateCompositionDialog* pDialog)
 {
  //pTabWidget = new QTabWidget();


 }

 void CreateCompositionDialog::Impl::ok(QDialog* dialog)
 {
 // Try to create the composition with the provided name before accepting the dialog.
 CreateCompositionDialog* dlg = static_cast<CreateCompositionDialog*>(dialog);
 if (dlg) {
  // ensure any in-progress edit is committed to the EditableLabel
  if (compositionNameEdit_) compositionNameEdit_->finishEdit();
  QString name = dlg->compositionName();
  // suppress default creation triggered by projectCreated
  ArtifactProjectManager::getInstance().suppressDefaultCreate(true);
  if (!name.isEmpty()) {
    UniString u;
    u.setQString(name);
    ArtifactProjectManager::getInstance().createComposition(u);
  } else {
    // create with default params if no name provided
    ArtifactProjectManager::getInstance().createComposition();
  }
  ArtifactProjectManager::getInstance().suppressDefaultCreate(false);
 }
 dialog->accept();
 }

 void CreateCompositionDialog::Impl::cancel(QDialog* dialog)
 {
  dialog->reject();
 }

 W_OBJECT_IMPL(CreateCompositionDialog)

  CreateCompositionDialog::CreateCompositionDialog(QWidget* parent /*= nullptr*/) :QDialog(parent), impl_(new Impl(this))
 {
  
  setWindowTitle(u8"コンポジション設定");
  setWindowFlags(Qt::FramelessWindowHint);
  setAttribute(Qt::WA_NoChildEventsForParent);

  setFixedSize(600, 400);
  static int16_t beforeSelectHeight = 400;

  static QTime	beforeSelectTime(0, 0, 0);

  static QTime	beforeSelectDurationTime(0, 0, 30);

  static float	beforeSelectFrameRate = 30.0f;
  auto compositionNameEdit = impl_->compositionNameEdit_ = new EditableLabel();
  compositionNameEdit->setText("Comp1");

  impl_->pTabWidget = new QTabWidget(this);
  impl_->compositionSettingPage_ = new CompositionSettingPage();
  impl_->pTabWidget->addTab(impl_->compositionSettingPage_, "基本設定");
  // 拡張設定・オーディオ設定タブは必要に応じて追加

  QHBoxLayout* nameHLayout = new QHBoxLayout();
  nameHLayout->addWidget(compositionNameEdit);


  QDialogButtonBox* const pDialogButtonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);

  QVBoxLayout* const pVBoxLayout = new QVBoxLayout();
  pVBoxLayout->addWidget(compositionNameEdit);
  pVBoxLayout->addWidget(impl_->pTabWidget);
  pVBoxLayout->addWidget(pDialogButtonBox, 0, Qt::AlignRight);
  setLayout(pVBoxLayout);

  QObject::connect(pDialogButtonBox, &QDialogButtonBox::accepted, this, [this]() {
   impl_->ok(this);
   });
  QObject::connect(pDialogButtonBox, &QDialogButtonBox::rejected, this, [this]() {
   impl_->cancel(this);
   });


  auto style = getDCCStyleSheetPreset(DccStylePreset::ModoStyle);

  setStyleSheet(style);
 }

 CreateCompositionDialog::~CreateCompositionDialog()
 {

 }

 void CreateCompositionDialog::setCompositionName(const QString& compositionName)
 {
 if (impl_ && impl_->compositionNameEdit_) {
  impl_->compositionNameEdit_->setText(compositionName);
 }
 }

QString CreateCompositionDialog::compositionName() const
{
 if (impl_ && impl_->compositionNameEdit_) {
  return impl_->compositionNameEdit_->text();
 }
 return QString();
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

  if (impl_->m_showAnimation && impl_->m_showAnimation->state() == QAbstractAnimation::Running) {
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
  QPoint startPos = endPos;
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





};


