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
#include <QDrag>
#include <QDropEvent>
#include <QLineEdit>
#include <QMouseEvent>
#include <QVariant>
#include <QStandardItem>
#include <QModelIndex>

#include <QMenu>
#include <QPixmap>
#include <QStringList>
#include <QTimer>
#include <QInputDialog>
#include <QContextMenuEvent>
#include <QLineEdit>
#include <QDesktopServices>
module Artifact.Widgets.ProjectManagerWidget;

import std;
import Utils.String.UniString;
import Utils.Id;
import Artifact.Project.Manager;
import Artifact.Service.Project;
import Artifact.Project.Model;
import Artifact.Project.Items;
import Artifact.Project.Roles;
import Artifact.Widgets.LayerPanelWidget;



namespace Artifact {

 using namespace ArtifactCore;

 W_OBJECT_IMPL(ArtifactProjectManagerWidget)

class HoverThumbnailPopupWidget::Impl {
 public:
  Impl() : thumbnailLabel(nullptr), layout(nullptr) {}
  QLabel* thumbnailLabel;
  QVector<QLabel*> infoLabels;
  QVBoxLayout* layout;
};

HoverThumbnailPopupWidget::HoverThumbnailPopupWidget(QWidget* parent /*= nullptr*/) : QWidget(parent), impl_(new Impl()) {
  setWindowFlags(Qt::ToolTip | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
  setAttribute(Qt::WA_TranslucentBackground);
  setAttribute(Qt::WA_ShowWithoutActivating);
  // ensure the popup has a visible background and rounded corners
  setStyleSheet("background-color: rgba(24,24,24,230); border-radius:8px;");

  impl_->layout = new QVBoxLayout(this);
  impl_->layout->setContentsMargins(8,8,8,8);
  impl_->layout->setSpacing(4);

  impl_->thumbnailLabel = new QLabel(this);
  impl_->thumbnailLabel->setFixedSize(200, 112); // 16:9 preview
  impl_->thumbnailLabel->setScaledContents(true);
  impl_->thumbnailLabel->setStyleSheet("background-color: #222; border-radius: 4px;");
  impl_->layout->addWidget(impl_->thumbnailLabel, 0, Qt::AlignCenter);

  // default a couple of info labels
  for (int i = 0; i < 3; ++i) {
    QLabel* l = new QLabel(this);
    l->setText("");
    l->setStyleSheet("color: white; background: transparent;");
    impl_->infoLabels.append(l);
    impl_->layout->addWidget(l);
  }

  setLayout(impl_->layout);
}

HoverThumbnailPopupWidget::~HoverThumbnailPopupWidget() {
  delete impl_;
}

void ArtifactProjectView::handleItemDoubleClicked(const QModelIndex& index)
{
  if (!index.isValid()) return;

  // DisplayRole の取得（デバッグ用）
  QVariant v = index.data(Qt::DisplayRole);
  if (v.isValid() && v.canConvert<QString>()) {
    qDebug() << "ArtifactProjectView: item double-clicked:" << v.toString();
  }

  // Composition ID を ProjectItemDataRole enum を使って取得
  // 注意: internalPointer() は QStandardItem* であり、ProjectItem* ではない
  // First try to check the stored item type to determine if this is a composition
  QVariant typeVar = index.data(Qt::UserRole + static_cast<int>(Artifact::ProjectItemDataRole::ProjectItemType));
  if (typeVar.isValid() && typeVar.canConvert<int>()) {
    int t = typeVar.toInt();
    // compare with enum value for Composition (eProjectItemType)
    if (t != static_cast<int>(eProjectItemType::Composition)) {
      qDebug() << "Item is not a composition type, ignoring double-click/context actions.";
      return;
    }
  }
  QVariant idVar = index.data(Qt::UserRole + static_cast<int>(Artifact::ProjectItemDataRole::CompositionId));
  if (!idVar.isValid() || !idVar.canConvert<QString>()) {
    qDebug() << "  No composition ID found for this item.";
    return;
  }

  QString idStr = idVar.toString();
  if (idStr.isEmpty()) {
    qDebug() << "  Composition ID is empty; not a composition item.";
    return;
  }

  qDebug() << "  Opening composition id=" << idStr;
  CompositionID cid(idStr);
  ArtifactLayerTimelinePanelWrapper* panel = new ArtifactLayerTimelinePanelWrapper(cid);
  panel->setAttribute(Qt::WA_DeleteOnClose);
  panel->show();
}

void ArtifactProjectView::mouseDoubleClickEvent(QMouseEvent* event)
{
  QModelIndex idx = indexAt(event->position().toPoint());
  if (idx.isValid()) {
    // dispatch to public handler
    handleItemDoubleClicked(idx);
  }
  QTreeView::mouseDoubleClickEvent(event);
}



  class ArtifactProjectView::Impl
 {
 private:
 	
 public:
 	void handleFileDrop(const QString& str);
 	void handleDefaultKeyPressEvent(QKeyEvent* ev);
 	void handleDefaultKeyReleaseEvent(QKeyEvent* ev);
 	void handleDoubleClicked(const QModelIndex& index);
 	QTimer* hoverTimer = nullptr;
 	QModelIndex hoverIndex;
 	HoverThumbnailPopupWidget* hoverPopup = nullptr;
 	QPoint lastMousePos;
  QPoint pos_;
 };

 void ArtifactProjectView::Impl::handleDefaultKeyPressEvent(QKeyEvent* ev)
 {

 }

void HoverThumbnailPopupWidget::setThumbnail(const QPixmap& pixmap)
{
  if (!impl_ || !impl_->thumbnailLabel) return;
  impl_->thumbnailLabel->setPixmap(pixmap);
}

void HoverThumbnailPopupWidget::setLabels(const QStringList& labels)
{
  if (!impl_) return;
  for (int i = 0; i < impl_->infoLabels.size() && i < labels.size(); ++i) {
    impl_->infoLabels[i]->setText(labels.at(i));
  }
}

void HoverThumbnailPopupWidget::setLabel(int idx, const QString& text)
{
  if (!impl_) return;
  if (idx >= 0 && idx < impl_->infoLabels.size()) impl_->infoLabels[idx]->setText(text);
}

void HoverThumbnailPopupWidget::showAt(const QPoint& globalPos)
{
  // place the popup at the given global position and show above other windows
  move(globalPos);
  show();
  raise();
  // do not steal focus
  setAttribute(Qt::WA_ShowWithoutActivating);
  // auto-hide after a short delay for convenience
  QTimer::singleShot(5000, this, [this]() { this->hide(); });
}

 void ArtifactProjectView::Impl::handleDefaultKeyReleaseEvent(QKeyEvent* ev)
 {

 }

void ArtifactProjectView::Impl::handleDoubleClicked(const QModelIndex& index)
{
  // Default behavior: print info about the clicked item. Consumers can
  // subclass ArtifactProjectView or access the model to implement real
  // functionality (open composition, expand, etc.).
  if (!index.isValid()) return;
  QStandardItem* item = nullptr;
  qDebug() << "Impl::handleDoubleClicked: index valid=" << index.isValid()
           << " row=" << index.row() << " col=" << index.column();
  QVariant v = index.data(Qt::DisplayRole);
  qDebug() << "  DisplayRole: valid=" << v.isValid() << " typeName=" << v.typeName();
  if (v.isValid() && v.canConvert<QString>()) qDebug() << "    toString=" << v.toString();
}

 void ArtifactProjectView::Impl::handleFileDrop(const QString& str)
 {
    // Convert to UniString and delegate to project service to register asset
    UniString u;
    u.setQString(str);
    ArtifactProjectService::instance()->addAssetFromPath(u);
 }

ArtifactProjectView::ArtifactProjectView(QWidget* parent /*= nullptr*/) :QTreeView(parent), impl_(new Impl())
 {
  // enable mouse move events even when no button is pressed
  setMouseTracking(true);
  if (viewport()) viewport()->setMouseTracking(true);
  setFrameShape(QFrame::NoFrame);
  // setSelectionMode(QAbstractItemView::SingleSelection);
  setDragEnabled(true);
  setAcceptDrops(true);
  setDropIndicatorShown(true);
  setRootIsDecorated(false);

  // Visual aid for debugging: set a distinct background color so we can verify the view is visible
  setStyleSheet("QTreeView { background-color: #1e2230; color: #ffffff; }");

  
 }

 ArtifactProjectView::~ArtifactProjectView()
 {
  delete impl_;
  impl_ = nullptr;
 }
 void ArtifactProjectView::mousePressEvent(QMouseEvent* event)
 {
  if (event->button() == Qt::LeftButton)
  {
	  
  }
  QTreeView::mousePressEvent(event);
 }
 void ArtifactProjectView::mouseMoveEvent(QMouseEvent* event)
 {
  // start/stop hover timer depending on whether the mouse moved over a new index
  QPoint p = event->position().toPoint();
  QModelIndex idx = indexAt(p);
  if (impl_) {
    if (!impl_->hoverTimer) {
      impl_->hoverTimer = new QTimer(this);
      impl_->hoverTimer->setSingleShot(true);
      connect(impl_->hoverTimer, &QTimer::timeout, this, [this]() {
        if (!impl_) return;
        if (impl_->hoverIndex.isValid()) {
          // populate and show popup
          if (!impl_->hoverPopup) impl_->hoverPopup = new HoverThumbnailPopupWidget();
          QString text = impl_->hoverIndex.data(Qt::DisplayRole).toString();
          impl_->hoverPopup->setLabels(QStringList() << text << "" << "");
          impl_->hoverPopup->setThumbnail(QPixmap());
          QPoint globalPos = this->viewport()->mapToGlobal(this->visualRect(impl_->hoverIndex).topRight());
          impl_->hoverPopup->showAt(globalPos + QPoint(8,8));
        }
      });
    }
    if (idx != impl_->hoverIndex) {
      // hide existing popup when moving to a different index
      if (impl_->hoverPopup && impl_->hoverPopup->isVisible()) impl_->hoverPopup->hide();
      impl_->hoverIndex = idx;
      impl_->hoverTimer->stop();
      if (idx.isValid()) {
        impl_->hoverTimer->start(500); // 0.5s
      } else {
        if (impl_->hoverPopup) impl_->hoverPopup->hide();
      }
    }
  }

  QTreeView::mouseMoveEvent(event);
 }

// contextMenuEvent implementation moved later in file (kept a single implementation)

 void ArtifactProjectView::dropEvent(QDropEvent* event)
 {
  event->setDropAction(Qt::CopyAction);
 	
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

void ArtifactProjectView::mouseReleaseEvent(QMouseEvent* event)
{
  // stop hover timer when mouse released
  if (impl_ && impl_->hoverTimer) impl_->hoverTimer->stop();
  QTreeView::mouseReleaseEvent(event);
}

 void ArtifactProjectView::dragEnterEvent(QDragEnterEvent* event)
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

 QSize ArtifactProjectView::sizeHint() const
 {
  return QSize(400, 400);
 }

 void ArtifactProjectView::dragMoveEvent(QDragMoveEvent* event)
 {
  const QMimeData* mimeData = event->mimeData();
  if (mimeData->hasUrls()) {
   for (const QUrl& url : mimeData->urls()) {
	QString filePath = url.toLocalFile().toLower();
	if (filePath.endsWith(".png") || filePath.endsWith(".jpg")) {
	 event->acceptProposedAction(); // OK!
	 return;
	}
   }
  }
  event->ignore();
 }

 void ArtifactProjectView::contextMenuEvent(QContextMenuEvent* event)
 {
  QModelIndex idx = indexAt(event->pos());
  if (!idx.isValid()) return;
  // Use the typed roles
  QVariant typeVar = idx.data(Qt::UserRole + static_cast<int>(Artifact::ProjectItemDataRole::ProjectItemType));
  qDebug() << "[contextMenuEvent] typeVar valid=" << typeVar.isValid() << ", value=" << typeVar << ", int=" << typeVar.toInt();
  QVariant compIdVar = idx.data(Qt::UserRole + static_cast<int>(Artifact::ProjectItemDataRole::CompositionId));
  QMenu menu(this);
  QAction* explorerAction = nullptr;
  if (typeVar.isValid() && typeVar.toInt() == static_cast<int>(eProjectItemType::Footage)) {
    explorerAction = menu.addAction(tr("エクスプローラーで開く"));
  }
  if (compIdVar.isValid() && !compIdVar.toString().isEmpty()) {
    QAction* openAction = menu.addAction(tr("Open"));
    QAction* renameAction = menu.addAction(tr("Rename"));
    QAction* deleteAction = menu.addAction(tr("Delete"));
    QAction* selected = menu.exec(event->globalPos());
    if (!selected) return;
    if (selected == explorerAction) {
      // FootageItemのfilePath取得
  // FilePath用のUserRoleはenum未定義のため直接値指定（例: Qt::UserRole+10）
  constexpr int filePathRole = Qt::UserRole + 10;
  QVariant filePathVar = idx.data(filePathRole);
      QString filePath;
      if (filePathVar.isValid() && filePathVar.canConvert<QString>()) {
        filePath = filePathVar.toString();
      } else {
        // モデルからProjectItemを取得してfilePathを参照
        auto model = this->model();
        if (model) {
          ProjectItem* item = static_cast<ProjectItem*>(model->data(idx, Qt::UserRole).value<void*>());
          if (item && item->type() == eProjectItemType::Footage) {
            filePath = static_cast<FootageItem*>(item)->filePath;
          }
        }
      }
      if (!filePath.isEmpty()) {
        QDesktopServices::openUrl(QUrl::fromLocalFile(filePath));
      }
      return;
    }
    if (selected == openAction) {
      handleItemDoubleClicked(idx);
    } else if (selected == renameAction) {
      QString currentName = idx.data(Qt::DisplayRole).toString();
      bool ok = false;
      QString newName = QInputDialog::getText(this, tr("Rename"), tr("New name:"), QLineEdit::Normal, currentName, &ok);
      if (ok && !newName.isEmpty()) {
      // Update model display value
      if (this->model()) {
        this->model()->setData(idx, newName, Qt::DisplayRole);
      }
      // Update underlying project via service using composition id
      QVariant idVar = idx.data(Qt::UserRole + static_cast<int>(Artifact::ProjectItemDataRole::CompositionId));
      if (idVar.isValid() && idVar.canConvert<QString>()) {
        QString idStr = idVar.toString();
        if (!idStr.isEmpty()) {
          // construct UniString from QString for public API
          UniString u;
          u.setQString(newName);
          ArtifactProjectService::instance()->renameComposition(CompositionID(idStr), u);
        }
      }
    }
    } else if (selected == deleteAction) {
      // TODO: implement deletion via ProjectService/Manager with confirmation
    }
  }


 }

 class ArtifactProjectManagerWidget::Impl {
 public:
  ArtifactProjectView* projectView_ = nullptr;
  ArtifactProjectModel* projectModel_ = nullptr;
  Impl();
  ~Impl();
 
  void handleSearchTextChanged();
  void handleFileDrop(const QString& str);
  void rebuildMenu();
  void update();
  QLabel* projectNameLabel = nullptr;
  QLineEdit* searchWidget_ = nullptr;
  QMenu* contextMenu = nullptr;
  QAction* createDirectoryAction = nullptr;
  QAction* addFileAction = nullptr;
  QAction* duplicateAction = nullptr;

  ArtifactProjectManagerToolBox* toolBox_ = nullptr;
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
  // create the model for the project view
  projectModel_ = nullptr;



 }

 ArtifactProjectManagerWidget::Impl::~Impl()
 {

 }

 void ArtifactProjectManagerWidget::Impl::showContextMenu(ArtifactProjectManagerWidget* widget, const QPoint& globalPos)
 {
  if (contextMenu) contextMenu->exec(globalPos);


 }

 void ArtifactProjectManagerWidget::Impl::rebuildMenu()
 {

 }

 void ArtifactProjectManagerWidget::Impl::handleFileDrop(const QString& str)
 {
  // Use ProjectService instead of direct ProjectManager access
  qDebug() << "File:" << str;
  UniString u;
  u.setQString(str);
  ArtifactProjectService::instance()->addAssetFromPath(u);

 }

 void ArtifactProjectManagerWidget::Impl::update()
 {
   // bind current project to model and assign to view via ProjectService
   auto shared = ArtifactProjectService::instance()->getCurrentProjectSharedPtr();
  if (!projectModel_) {
    projectModel_ = new ArtifactProjectModel();
  }
  projectModel_->setProject(shared);
  if (projectView_) {
    projectView_->setModel(projectModel_);
  }
 }

 ArtifactProjectManagerWidget::ArtifactProjectManagerWidget(QWidget* parent /*= nullptr*/) :QWidget(parent), impl_(new Impl())
 {
  setWindowTitle("Artifact.Project");
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
  projectNameLabel->setText("Artifact.Project name:");

  impl_->projectView_ = new ArtifactProjectView();
  // attach model to view
  // attach model to view (ensure model exists)
  if (!impl_->projectModel_) {
    impl_->projectModel_ = new ArtifactProjectModel(this);
  }
  // bind current project to model and assign to view
  impl_->projectModel_->setProject(ArtifactProjectManager::getInstance().getCurrentProjectSharedPtr());
  impl_->projectView_->setModel(impl_->projectModel_);

  auto searchWidget = impl_->searchWidget_ = new QLineEdit();
  searchWidget->setPlaceholderText("...");
  searchWidget->setAlignment(Qt::AlignLeft);
  searchWidget->setEchoMode(QLineEdit::Normal);
 	
  auto toolBox =impl_->toolBox_= new ArtifactProjectManagerToolBox();

  auto layout = new QVBoxLayout();
  layout->addWidget(projectNameLabel);
  layout->addWidget(searchWidget);
  layout->addWidget(impl_->projectView_);
  layout->addWidget(toolBox);
  setLayout(layout);

  setContextMenuPolicy(Qt::CustomContextMenu);
 	
  auto projectService = ArtifactProjectService::instance();
 	
  // connect to project service signals to update UI; use single-shot update to avoid duplicate
  connect(projectService, &ArtifactProjectService::projectCreated, this, [this]() { QMetaObject::invokeMethod(this, "updateRequested", Qt::QueuedConnection); });
  connect(projectService, &ArtifactProjectService::projectChanged, this, [this]() { QMetaObject::invokeMethod(this, "updateRequested", Qt::QueuedConnection); });
  
 }

 ArtifactProjectManagerWidget::~ArtifactProjectManagerWidget()
 {
  delete impl_;
 }

 void ArtifactProjectManagerWidget::triggerUpdate()
 {
  impl_->update();
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
  auto projectService = ArtifactProjectService::instance();
 	
  // refresh model/view when project is created/changed
  impl_->update();
  this->setEnabled(true);
 }

 class ArtifactProjectManagerToolBox::Impl
 {
 public:
  QToolButton* createCompositionButton = nullptr;
 };
	
 ArtifactProjectManagerToolBox::ArtifactProjectManagerToolBox(QWidget* widget/*=nullptr*/)
 {
  setMinimumHeight(20);
 	
 	
 }

 ArtifactProjectManagerToolBox::~ArtifactProjectManagerToolBox()
 {

 }

 void ArtifactProjectManagerToolBox::resizeEvent(QResizeEvent* event)
 {
  //throw std::logic_error("The method or operation is not implemented.");
 }



}
