module;
#include <QVector>
#include <QWidget>
#include <wobjectimpl.h>
#include <QBoxLayout>
#include <QLabel>
#include <QClipboard>
#include <QEvent>
#include <QMimeData>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QLineEdit>

#include <QMenu>
module Artifact.Widgets.ProjectManagerWidget;


//import HeadPanel;

import Project.Manager;

import Artifact.Service.Project;


namespace Artifact {
 W_OBJECT_IMPL(ArtifactProjectManagerWidget)

  class ArtifactProjectView::Impl
 {
 private:
 	
 public:
  void handleDefaultKeyPressEvent(QKeyEvent* ev);
  void handleDefaultKeyReleaseEvent(QKeyEvent* ev);

 };

 void ArtifactProjectView::Impl::handleDefaultKeyPressEvent(QKeyEvent* ev)
 {

 }

 void ArtifactProjectView::Impl::handleDefaultKeyReleaseEvent(QKeyEvent* ev)
 {

 }

 ArtifactProjectView::ArtifactProjectView(QWidget* parent /*= nullptr*/) :QTreeView(parent)
 {
  setFrameShape(QFrame::NoFrame);
  // setSelectionMode(QAbstractItemView::SingleSelection);
  setDragEnabled(true);
  setAcceptDrops(true);
  setDropIndicatorShown(true);

 }

 ArtifactProjectView::~ArtifactProjectView()
 {

 }


 class ArtifactProjectManagerWidget::Impl {
 public:

  ArtifactProjectView* projectView_ = nullptr;
  Impl();
  ~Impl();
  QLabel* projectNameLabel = nullptr;
  QLineEdit* searchWidget_ = nullptr;
  void handleSearchTextChanged();
  void handleFileDrop(const QString& str);
  void rebuildMenu();

  QMenu* contextMenu = nullptr;
  QAction* createDirectoryAction = nullptr;
  QAction* addFileAction = nullptr;
  QAction* duplicateAction = nullptr;

  void showContextMenu(ArtifactProjectManagerWidget* widget, const QPoint& globalPos);
 };

 ArtifactProjectManagerWidget::Impl::Impl()
 {
  contextMenu = new QMenu();

  createDirectoryAction = new QAction();
  createDirectoryAction->setText("test");

  contextMenu->addAction(createDirectoryAction);

  addFileAction = new QAction();
  addFileAction->setText("Add file");

  contextMenu->addAction(addFileAction);

 }

 ArtifactProjectManagerWidget::Impl::~Impl()
 {

 }

 void ArtifactProjectManagerWidget::Impl::showContextMenu(ArtifactProjectManagerWidget* widget, const QPoint& globalPos)
 {
  contextMenu->show();


 }

 void ArtifactProjectManagerWidget::Impl::rebuildMenu()
 {

 }

 void ArtifactProjectManagerWidget::Impl::handleFileDrop(const QString& str)
 {
  auto& manager = ArtifactProjectManager::getInstance();

  qDebug() << "File:" << str;
  manager.addAssetFromFilePath(str);

 }

 ArtifactProjectManagerWidget::ArtifactProjectManagerWidget(QWidget* parent /*= nullptr*/) :QWidget(parent), impl_(new Impl())
 {
  setWindowTitle("Project");
  setAttribute(Qt::WA_StyledBackground);
  setStyleSheet(R"(
    QWidget {
        background-color: #2e2e2e;
        color: #e0e0e0;
        font-family: 'Segoe UI', 'Meiryo', sans-serif;
        font-size: 11pt;
    }

    QLineEdit, QTextEdit, QListWidget, QTreeWidget {
        background-color: #1e1e1e;
        border: 1px solid #3a3a3a;
        color: #ffffff;
        selection-background-color: #2979ff;
        selection-color: #ffffff;
    }

    QHeaderView::section {
        background-color: #3a3a3a;
        color: #cccccc;
        padding: 4px;
        border: 1px solid #444;
    }

    QToolButton {
        background-color: transparent;
        border: none;
        color: #cccccc;
    }

    QToolButton:hover {
        background-color: #3a3a3a;
        color: #ffffff;
    }

    QScrollBar:vertical, QScrollBar:horizontal {
        background: #1e1e1e;
        width: 12px;
        margin: 0px;
    }

    QScrollBar::handle {
        background: #555;
        border-radius: 6px;
    }

    QScrollBar::handle:hover {
        background: #888;
    }

    QTabBar::tab {
        background: #2e2e2e;
        padding: 6px 12px;
        border: 1px solid #444;
        color: #cccccc;
    }

    QTabBar::tab:selected {
        background: #1e1e1e;
        color: #ffffff;
    }
)");

  setEnabled(false);

  auto projectNameLabel=impl_->projectNameLabel = new QLabel();
  projectNameLabel->setText("Project name:");

  impl_->projectView_ = new ArtifactProjectView();

  auto searchWidget = impl_->searchWidget_ = new QLineEdit();
  searchWidget->setPlaceholderText("...");
  searchWidget->setAlignment(Qt::AlignLeft);
  searchWidget->setEchoMode(QLineEdit::Normal);

  auto layout = new QVBoxLayout();
  layout->addWidget(projectNameLabel);
  layout->addWidget(searchWidget);
  layout->addWidget(impl_->projectView_);
  setLayout(layout);

  setContextMenuPolicy(Qt::CustomContextMenu);
 }

 ArtifactProjectManagerWidget::~ArtifactProjectManagerWidget()
 {
  delete impl_;
 }

 void ArtifactProjectManagerWidget::triggerUpdate()
 {

 }

 void ArtifactProjectManagerWidget::dragEnterEvent(QDragEnterEvent* event)
 {
  const QMimeData* mimeData = event->mimeData();

  if (mimeData->hasUrls()) {
   for (const QUrl& url : mimeData->urls()) {
	QString filePath = url.toLocalFile().toLower();

	if (filePath.endsWith(".png") ||
	 filePath.endsWith(".jpg") ||
	 filePath.endsWith(".jpeg") ||
	 filePath.endsWith(".bmp") ||
	 filePath.endsWith(".gif") ||
	 filePath.endsWith(".mp4") ||
	 filePath.endsWith(".avi") ||
	 filePath.endsWith(".mov") ||
	 filePath.endsWith(".mkv")) {
	 event->acceptProposedAction(); // OK!
	 return;
	}
   }
  }

  event->ignore(); // 拒否
 }
 void ArtifactProjectManagerWidget::dropEvent(QDropEvent* event)
 {
  const QMimeData* mimeData = event->mimeData();

  if (mimeData->hasUrls()) {
   QList<QUrl> urlList = mimeData->urls();

   for (const QUrl& url : urlList) {
	QString filePath = url.toLocalFile().toLower();

	// 対象拡張子チェック
	if (filePath.endsWith(".png") ||
	 filePath.endsWith(".jpg") ||
	 filePath.endsWith(".jpeg") ||
	 filePath.endsWith(".bmp") ||
	 filePath.endsWith(".gif") ||
	 filePath.endsWith(".mp4") ||
	 filePath.endsWith(".avi") ||
	 filePath.endsWith(".mov") ||
	 filePath.endsWith(".mkv")) {

	 // ✔️ 実際の処理：ここで何かする（読み込み/表示/保存など）
	 qDebug() << "Accepted file dropped:" << filePath;

	 impl_->handleFileDrop(filePath);
	}
   }

   event->acceptProposedAction();  // ドロップ操作を受理
  }
  else {
   event->ignore();  // 無効なドロップ
  }
 }

 QSize ArtifactProjectManagerWidget::sizeHint() const
 {
  return QSize(QWidget::sizeHint().width(), 600);
 }

 void ArtifactProjectManagerWidget::contextMenuEvent(QContextMenuEvent* event)
 {
  impl_->showContextMenu(this, event->globalPos());


 }

 void ArtifactProjectManagerWidget::updateRequested()
 {
  auto& projectManager=ArtifactProjectManager::getInstance();
 	
 	
 	
 }

}
