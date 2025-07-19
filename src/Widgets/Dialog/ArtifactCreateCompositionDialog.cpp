module;
#include <QTime>
#include <QWidget>
#include <QDialog>
#include <QTabWidget>
#include <QDialogButtonBox>
#include <QBoxLayout>

#include <QEvent>
#include <QKeyEvent>
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
  QTabWidget* pTabWidget = nullptr;

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

 CompositionExtendSettingPage::CompositionExtendSettingPage(QWidget* parent /*= nullptr*/) :QWidget(parent)
 {

 }

 CompositionExtendSettingPage::~CompositionExtendSettingPage()
 {

 }



}


