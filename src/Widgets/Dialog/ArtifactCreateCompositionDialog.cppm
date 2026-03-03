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
#include <QColorDialog>
#include <QPushButton>
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
 public:
  Impl();
  
  QComboBox* resolutionCombobox_ = nullptr;
  QComboBox* fpsCombo_ = nullptr;
  QComboBox* pixelAspectCombo_ = nullptr;
  DragSpinBox* widthSpinBox = nullptr;
  DragSpinBox* heightSpinBox = nullptr;
  DoubleDragSpinBox* durationSpinBox = nullptr;
  QLineEdit* startTimecodeEdit = nullptr;
  // Use a simple QPushButton for color picking to avoid dependency on an incomplete widget
  QPushButton* bgColorButton = nullptr;
  QColor bgColor = QColor(0, 0, 0, 255);
 };

 CompositionSettingPage::Impl::Impl()
 {
 }

 CompositionSettingPage::CompositionSettingPage(QWidget* parent /*= nullptr*/) :QWidget(parent),impl_(new Impl)
 {
  impl_->widthSpinBox = new DragSpinBox();
  impl_->widthSpinBox->setRange(1, 16384);
  impl_->heightSpinBox = new DragSpinBox();
  impl_->heightSpinBox->setRange(1, 16384);
  
  impl_->resolutionCombobox_ = new QComboBox();
  impl_->resolutionCombobox_->addItem("1920x1080 (FHD)", QVariant::fromValue(QSize(1920, 1080)));
  impl_->resolutionCombobox_->addItem("1280x720 (HD)", QVariant::fromValue(QSize(1280, 720)));
  impl_->resolutionCombobox_->addItem("3840x2160 (4K)", QVariant::fromValue(QSize(3840, 2160)));
  impl_->resolutionCombobox_->addItem("カスタム", QVariant::fromValue(QSize(-1, -1)));

  impl_->pixelAspectCombo_ = new QComboBox();
  impl_->pixelAspectCombo_->addItem("正方形ピクセル (1.0)", QVariant::fromValue(1.0));
  impl_->pixelAspectCombo_->addItem("D1/DV NTSC (0.91)", QVariant::fromValue(0.9091));
  impl_->pixelAspectCombo_->addItem("D1/DV PAL (1.09)", QVariant::fromValue(1.0940));

  impl_->fpsCombo_ = new QComboBox();
  impl_->fpsCombo_->addItem("23.976", QVariant::fromValue(23.976));
  impl_->fpsCombo_->addItem("24", QVariant::fromValue(24.0));
  impl_->fpsCombo_->addItem("25", QVariant::fromValue(25.0));
  impl_->fpsCombo_->addItem("29.97", QVariant::fromValue(29.97));
  impl_->fpsCombo_->addItem("30", QVariant::fromValue(30.0));
  impl_->fpsCombo_->addItem("60", QVariant::fromValue(60.0));
  impl_->fpsCombo_->setCurrentIndex(4); // default 30 fps

  impl_->startTimecodeEdit = new QLineEdit("00:00:00:00");
  
  impl_->durationSpinBox = new DoubleDragSpinBox();
  impl_->durationSpinBox->setRange(0.1, 3600.0);
  impl_->durationSpinBox->setValue(10.0); // 10 seconds default

  impl_->bgColorButton = new QPushButton();
  impl_->bgColorButton->setFixedSize(60, 24);
  impl_->bgColorButton->setStyleSheet("background-color: rgb(0, 0, 0); border: 1px solid #555;");
  
  auto line1 = new QFrame();
  line1->setFrameShape(QFrame::HLine);
  line1->setFrameShadow(QFrame::Sunken);

  auto line2 = new QFrame();
  line2->setFrameShape(QFrame::HLine);
  line2->setFrameShadow(QFrame::Sunken);

  auto vboxLayout = new QFormLayout();
  vboxLayout->addRow("プリセット:", impl_->resolutionCombobox_);
  vboxLayout->addRow("幅 (px):", impl_->widthSpinBox);
  vboxLayout->addRow("高さ (px):", impl_->heightSpinBox);
  vboxLayout->addRow("ピクセル縦横比:", impl_->pixelAspectCombo_);
  vboxLayout->addRow("フレームレート:", impl_->fpsCombo_);
  vboxLayout->addRow(line1);
  vboxLayout->addRow("開始タイムコード:", impl_->startTimecodeEdit);
  vboxLayout->addRow("デュレーション (秒):", impl_->durationSpinBox);
  vboxLayout->addRow(line2);
  vboxLayout->addRow("背景色:", impl_->bgColorButton);
  
  setLayout(vboxLayout);

  // Sync combo boxes with spin boxes
  QObject::connect(impl_->resolutionCombobox_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) {
    QSize sz = impl_->resolutionCombobox_->currentData().toSize();
    if (sz.width() > 0 && sz.height() > 0) {
      impl_->widthSpinBox->setValue(sz.width());
      impl_->heightSpinBox->setValue(sz.height());
    }
  });
  
  // Custom size logic
  auto forceCustom = [this]() {
      impl_->resolutionCombobox_->blockSignals(true);
      impl_->resolutionCombobox_->setCurrentIndex(impl_->resolutionCombobox_->count() - 1); // Custom
      impl_->resolutionCombobox_->blockSignals(false);
  };
  QObject::connect(impl_->widthSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, forceCustom);
  QObject::connect(impl_->heightSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, forceCustom);

  // Background color picker
  QObject::connect(impl_->bgColorButton, &QPushButton::clicked, this, [this]() {
      QColor c = QColorDialog::getColor(impl_->bgColor, this, "背景色を選択");
      if (c.isValid()) {
          impl_->bgColor = c;
          QString style = QString("background-color: %1; border: 1px solid #555;").arg(c.name());
          impl_->bgColorButton->setStyleSheet(style);
      }
  });

  impl_->resolutionCombobox_->setCurrentIndex(0); // This triggers the connected lambda to set 1920x1080

  auto style = getDCCStyleSheetPreset(DccStylePreset::ModoStyle);
  setStyleSheet(style);
 }

 CompositionSettingPage::~CompositionSettingPage()
 {
 }

 ArtifactCompositionInitParams CompositionSettingPage::getInitParams(const QString& name) const
 {
     ArtifactCompositionInitParams params;
     UniString u;
     u.setQString(name);
     params.setCompositionName(u);
     params.setResolution(impl_->widthSpinBox->value(), impl_->heightSpinBox->value());
     
     double fps = impl_->fpsCombo_->currentData().toDouble();
     params.setFrameRate(fps);

     // Currently AspectRatio accepts rational (width, height), approximate float for now
     double aspect = impl_->pixelAspectCombo_->currentData().toDouble();
     params.setPixelAspectRatio(AspectRatio(static_cast<int>(aspect * 10000), 10000));
     
     params.setDurationSeconds(impl_->durationSpinBox->value());
     
     // Background color
     QColor c = impl_->bgColor;
     params.setBackgroundColor(FloatColor(c.redF(), c.greenF(), c.blueF(), c.alphaF()));
     
     return params;
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
  bool okCalled_ = false;  // 二重呼び出し防止フラグ
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
  // 二重呼び出し防止
  if (okCalled_) {
   qDebug() << "[CreateCompositionDialog] ok() already called, skipping duplicate";
   return;
  }
  okCalled_ = true;

  // Try to create the composition with the provided name before accepting the dialog.
  CreateCompositionDialog* dlg = static_cast<CreateCompositionDialog*>(dialog);
  if (dlg) {
   // ensure any in-progress edit is committed to the EditableLabel
   if (compositionNameEdit_) compositionNameEdit_->finishEdit();
   QString name = dlg->compositionName();
   // suppress default creation triggered by projectCreated
   ArtifactProjectManager::getInstance().suppressDefaultCreate(true);
   
   if (compositionSettingPage_) {
     ArtifactCompositionInitParams params = compositionSettingPage_->getInitParams(name);
     ArtifactProjectManager::getInstance().createComposition(params);
   } else {
     if (!name.isEmpty()) {
       UniString u;
       u.setQString(name);
       ArtifactProjectManager::getInstance().createComposition(u);
     } else {
       // create with default params if no name provided
       ArtifactProjectManager::getInstance().createComposition();
     }
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
   event->accept();
   return;
  }

  // Use default dialog handling for Enter/Return to avoid double OK invocation
  QDialog::keyPressEvent(event);
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
  const float offsetFactor = 0.08f; // 動きの量 (ダイアログの高さに対する割合)
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



