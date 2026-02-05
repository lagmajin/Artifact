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
#include <QProgressBar>
module Artifact.Widgets.RenderQueueJobPanel;
import std;
import Widgets.Utils.CSS;
import Widgets.EditableLabel;
import Artifact.Render.Queue.Service;

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
	
	W_OBJECT_IMPL(ColumnWidthManager)

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
  QProgressBar* jobProgressBar = nullptr;
  QLabel* statusLabel = nullptr;

  //QLabel* outputPathLabel = nullptr

  QWidget* headerDetailPanel = nullptr;   // 1行目の詳細
  QWidget* outputDetailPanel = nullptr;

  void updateJobStatus(int status);
  void updateJobProgress(int progress);
 };

 RenderQueueJobWidget::Impl::Impl()
 {

 }

 RenderQueueJobWidget::Impl::~Impl()
 {

 }

 void RenderQueueJobWidget::Impl::updateJobStatus(int status)
 {
  QString statusText;

  // Status values: 0=Pending, 1=Rendering, 2=Completed, 3=Failed, 4=Canceled
  switch (status) {
    case 0: // Pending
      statusText = "待機中";
      break;
    case 1: // Rendering
      statusText = "レンダリング中";
      break;
    case 2: // Completed
      statusText = "完了";
      break;
    case 3: // Failed
      statusText = "失敗";
      break;
    case 4: // Canceled
      statusText = "キャンセル";
      break;
    default:
      statusText = "不明";
      break;
  }

  statusLabel->setText(statusText);
 }

 void RenderQueueJobWidget::Impl::updateJobProgress(int progress)
 {
  jobProgressBar->setValue(progress);
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

  impl_->statusLabel = new QLabel("待機中");
  impl_->jobProgressBar = new QProgressBar();
  impl_->jobProgressBar->setRange(0, 100);
  impl_->jobProgressBar->setValue(0);

  auto layout = new QHBoxLayout();
  layout->addWidget(impl_->toggleButton);
  layout->addWidget(impl_->compositionNameLabel);

  QHBoxLayout* outputLayout = new QHBoxLayout();
  impl_->renderingStartButton = new QToolButton();
  impl_->renderingStartButton->setText("▶");
  outputLayout->addWidget(impl_->renderingStartButton);
  outputLayout->addWidget(impl_->statusLabel);
  outputLayout->addWidget(impl_->jobProgressBar);
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
