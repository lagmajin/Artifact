module;
#include <QString>
#include <QListWidget>
#include <QStackedWidget>


 module ApplicationSettingDialog;

import Artifact.Application.Service;

namespace ArtifactCore {


 LabelColorSettingWidget::LabelColorSettingWidget(const QString& labelname, const QColor& color, QWidget* parent /*= NULL*/)
 {

 }



 LabelColorSettingWidget::~LabelColorSettingWidget()
 {

 }
	
 MemoryAndCpuSettingPage::MemoryAndCpuSettingPage(QWidget* parent /*= nullptr*/)
 {

 }

 MemoryAndCpuSettingPage::~MemoryAndCpuSettingPage()
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
  setWindowTitle("Title");
 }

 ApplicationSettingDialog::~ApplicationSettingDialog()
 {
  delete impl_;
 }



};