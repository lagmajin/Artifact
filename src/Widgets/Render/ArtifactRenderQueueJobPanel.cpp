module;
#include <QVector>
#include <QObject>
#include <wobjectimpl.h>
#include <QLineEdit>
#include <QLabel>
#include <QToolButton>
#include <QPushButton>
#include <QBoxLayout>
#include <QFileDialog>
module Artifact.Widgets.RenderQueueJobPanel;
import std;
import Widgets.Utils.CSS;
import Widgets.EditableLabel;

namespace ArtifactCore {}//;

namespace Artifact
{
 using namespace ArtifactCore;
 using namespace ArtifactWidgets;

 class ColumnWidthManager::Impl
 {
 public:
  QVector<int> widths;


 };

 ColumnWidthManager::ColumnWidthManager(QObject* parent /*= nullptr*/) :QObject(parent), impl_(new Impl())
 {

 }

 ColumnWidthManager::~ColumnWidthManager()
 {
  delete impl_;
 }

 W_OBJECT_IMPL(RenderQueueManagerJobDetailWidget)

  class RenderQueueManagerJobDetailWidget::Impl
 {
 private:

 public:
  Impl(RenderQueueManagerJobDetailWidget* q);
  ~Impl();
  void handleShowSaveDialog();


 };

 RenderQueueManagerJobDetailWidget::Impl::Impl(RenderQueueManagerJobDetailWidget* q)
 {

 }

 RenderQueueManagerJobDetailWidget::Impl::~Impl()
 {

 }

 RenderQueueManagerJobDetailWidget::RenderQueueManagerJobDetailWidget(QWidget* parent /*= nullptr*/) : QWidget(parent)
  , impl_(new Impl(this))
 {
  auto style = getDCCStyleSheetPreset(DccStylePreset::ModoStyle);

  setStyleSheet(style);
 }

 RenderQueueManagerJobDetailWidget::~RenderQueueManagerJobDetailWidget()
 {
  delete impl_;
 }

 W_OBJECT_IMPL(RenderQueueJobWidget)

  class RenderQueueJobWidget::Impl
 {
 public:
  Impl();
  ~Impl();
  QToolButton* toggleButton = nullptr;
  EditableLabel* compositionNameLabel = nullptr;
  QToolButton* renderingStartButton = nullptr;
  EditableLabel* directryLabel = nullptr;
 	
  //QLabel* outputPathLabel = nullptr
 	
  QWidget* headerDetailPanel = nullptr;   // 1行目の詳細
  QWidget* outputDetailPanel = nullptr;
 };

 RenderQueueJobWidget::Impl::Impl()
 {

 }

 RenderQueueJobWidget::Impl::~Impl()
 {

 }

 RenderQueueJobWidget::RenderQueueJobWidget(QWidget* parent /*= nullptr*/) :QWidget(parent), impl_(new Impl())
 {
  auto style = getDCCStyleSheetPreset(DccStylePreset::ModoStyle);

  setStyleSheet(style);

  setWindowTitle("RenderQueueManagerHeaderWidget");
  QVBoxLayout* mainLayout = new QVBoxLayout(this);
  impl_->toggleButton = new QToolButton(this);
  impl_->toggleButton->setCheckable(true);
  impl_->toggleButton->setChecked(true);       // 初期状態: 開いてる
  impl_->toggleButton->setText("▼");

  impl_->compositionNameLabel = new EditableLabel();
  impl_->compositionNameLabel->setText("Comp1");

  auto layout = new QHBoxLayout();
  layout->addWidget(impl_->toggleButton);
  layout->addWidget(impl_->compositionNameLabel);
 	
  QHBoxLayout* outputLayout = new QHBoxLayout();
  impl_->renderingStartButton = new QToolButton();
  impl_->renderingStartButton->setText("▶");
  outputLayout->addWidget(impl_->renderingStartButton);
  mainLayout->addLayout(layout);
  mainLayout->addLayout(outputLayout);
 	
  connect(impl_->toggleButton, &QToolButton::toggled, this, [this](bool checked) {
   if (checked) {
	impl_->compositionNameLabel->show();
	impl_->toggleButton->setText("▼"); // 開いている時のアイコン
   }
   else {
	impl_->compositionNameLabel->hide();
	impl_->toggleButton->setText("▶"); // 閉じている時のアイコン
   }
   });
  setLayout(layout);
 }
 RenderQueueJobWidget::~RenderQueueJobWidget()
 {
  delete impl_;
 }



};
