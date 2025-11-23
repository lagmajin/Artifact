module;
#include <QString>
#include <QListWidget>
#include <QStackedWidget>


 module ApplicationSettingDialog;


namespace ArtifactCore {


 LabelColorSettingWidget::LabelColorSettingWidget(const QString& labelname, const QColor& color, QWidget* parent /*= NULL*/)
 {

 }



 LabelColorSettingWidget::~LabelColorSettingWidget()
 {

 }

 class ApplicationSettingDialog::Impl
 {
 private:

 public:
  Impl();
  ~Impl();
  QListWidget* tabList;
  QStackedWidget* pages;
 };

 ApplicationSettingDialog::Impl::Impl()
 {

 }

 ApplicationSettingDialog::Impl::~Impl()
 {

 }

 ApplicationSettingDialog::ApplicationSettingDialog(QWidget* parent /*= nullptr*/):QDialog(parent),impl_(new Impl)
 {

 }

 ApplicationSettingDialog::~ApplicationSettingDialog()
 {
  delete impl_;
 }

};