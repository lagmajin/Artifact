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
#include <QSortFilterProxyModel>
#include <QMenu>
#include <QPixmap>
#include <QStringList>
#include <QTimer>
#include <QInputDialog>
#include <QContextMenuEvent>
#include <QDesktopServices>
#include <QHeaderView>
#include <QPushButton>

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
 W_OBJECT_IMPL(ArtifactProjectView)

// --- Preview/Info Panel at top ---
class ProjectInfoPanel : public QWidget {
public:
    QLabel* thumbnail;
    QLabel* titleLabel;
    QLabel* detailsLabel;

    ProjectInfoPanel(QWidget* parent = nullptr) : QWidget(parent) {
        setFixedHeight(90);
        auto layout = new QHBoxLayout(this);
        layout->setContentsMargins(12, 8, 12, 8);
        layout->setSpacing(15);

        thumbnail = new QLabel();
        thumbnail->setFixedSize(120, 68);
        thumbnail->setAlignment(Qt::AlignCenter);
        thumbnail->setText("PREVIEW");
        thumbnail->setStyleSheet(R"(
            QLabel {
                background-color: #111;
                border: 1px solid #3e3e42;
                border-radius: 3px;
                color: #444;
                font-size: 9px;
                font-weight: bold;
            }
        )");

        auto infoLayout = new QVBoxLayout();
        infoLayout->setSpacing(2);
        infoLayout->setContentsMargins(0, 5, 0, 5);

        titleLabel = new QLabel("Project");
        titleLabel->setStyleSheet("color: #eee; font-weight: bold; font-size: 13px;");

        detailsLabel = new QLabel("Select an item to see details");
        detailsLabel->setStyleSheet("color: #888; font-size: 11px;");

        infoLayout->addWidget(titleLabel);
        infoLayout->addWidget(detailsLabel);
        infoLayout->addStretch();

        layout->addWidget(thumbnail);
        layout->addLayout(infoLayout);
        layout->addStretch();

        setStyleSheet("background-color: #252526; border-bottom: 2px solid #1a1a1a;");
    }

    void updateInfo(const QModelIndex& index) {
        if (!index.isValid()) {
            titleLabel->setText("Project");
            detailsLabel->setText("No selection");
            return;
        }
        QString name = index.data(Qt::DisplayRole).toString();
        titleLabel->setText(name);

        QString size = index.siblingAtColumn(1).data(Qt::DisplayRole).toString();
        QString duration = index.siblingAtColumn(2).data(Qt::DisplayRole).toString();
        QString fps = index.siblingAtColumn(3).data(Qt::DisplayRole).toString();

        if (!size.isEmpty() || !duration.isEmpty()) {
            detailsLabel->setText(QString("%1\n%2, %3").arg(size).arg(duration).arg(fps));
        } else {
            detailsLabel->setText("Folder or Footage Asset");
        }
    }
};

// --- Hover Popup ---
class HoverThumbnailPopupWidget::Impl {
 public:
  Impl() : thumbnailLabel(nullptr) {}
  QLabel* thumbnailLabel;
  QVector<QLabel*> infoLabels;
  QVBoxLayout* layout;
};

HoverThumbnailPopupWidget::HoverThumbnailPopupWidget(QWidget* parent) : QWidget(parent), impl_(new Impl()) {
  setWindowFlags(Qt::ToolTip | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
  setAttribute(Qt::WA_TranslucentBackground);
  setAttribute(Qt::WA_ShowWithoutActivating);
  setStyleSheet("background-color: rgba(30, 30, 30, 240); border: 1px solid #444; border-radius: 6px;");

  impl_->layout = new QVBoxLayout(this);
  impl_->layout->setContentsMargins(10, 10, 10, 10);
  impl_->layout->setSpacing(6);

  impl_->thumbnailLabel = new QLabel(this);
  impl_->thumbnailLabel->setFixedSize(200, 112);
  impl_->thumbnailLabel->setScaledContents(true);
  impl_->thumbnailLabel->setStyleSheet("background-color: #000; border-radius: 4px;");
  impl_->layout->addWidget(impl_->thumbnailLabel, 0, Qt::AlignCenter);

  for (int i = 0; i < 3; ++i) {
    QLabel* l = new QLabel(this);
    l->setStyleSheet("color: #ccc; font-size: 11px; font-family: 'Segoe UI';");
    impl_->infoLabels.append(l);
    impl_->layout->addWidget(l);
  }
}

HoverThumbnailPopupWidget::~HoverThumbnailPopupWidget() { delete impl_; }
void HoverThumbnailPopupWidget::setThumbnail(const QPixmap& px) { if(impl_->thumbnailLabel) impl_->thumbnailLabel->setPixmap(px); }
void HoverThumbnailPopupWidget::setLabels(const QStringList& ls) { for(int i=0; i<impl_->infoLabels.size() && i<ls.size(); ++i) impl_->infoLabels[i]->setText(ls[i]); }
void HoverThumbnailPopupWidget::setLabel(int idx, const QString& t) { if(idx>=0 && idx<impl_->infoLabels.size()) impl_->infoLabels[idx]->setText(t); }
void HoverThumbnailPopupWidget::showAt(const QPoint& p) { move(p); show(); raise(); QTimer::singleShot(5000, this, &QWidget::hide); }

// --- Project View (Tree) ---
class ArtifactProjectView::Impl {
public:
    QTimer* hoverTimer = nullptr;
    QModelIndex hoverIndex;
    HoverThumbnailPopupWidget* hoverPopup = nullptr;
    void handleFileDrop(const QString& str) {
        UniString u; u.setQString(str);
        ArtifactProjectService::instance()->addAssetFromPath(u);
    }
};

ArtifactProjectView::ArtifactProjectView(QWidget* parent) : QTreeView(parent), impl_(new Impl()) {
    setMouseTracking(true);
    if(viewport()) viewport()->setMouseTracking(true);
    setFrameShape(QFrame::NoFrame);
    setDragEnabled(true);
    setAcceptDrops(true);
    setDropIndicatorShown(true);
    setRootIsDecorated(true);
    setIndentation(15);
    setAlternatingRowColors(true);
    setAnimated(true);

    header()->setStyleSheet(R"(
        QHeaderView::section {
            background-color: #2D2D30;
            color: #888;
            padding: 4px 10px;
            border: none;
            border-right: 1px solid #1a1a1a;
            font-size: 10px;
            font-weight: bold;
            text-transform: uppercase;
        }
    )");

    setStyleSheet(R"(
        QTreeView {
            background-color: #1E1E1E;
            color: #CCC;
            alternate-background-color: #252526;
            selection-background-color: #094771;
            selection-color: white;
            outline: none;
            border: none;
        }
        QTreeView::item {
            padding: 5px;
            border-bottom: 1px solid #1a1a1a;
        }
        QTreeView::item:hover {
            background-color: #2D2D30;
        }

        /* --- Modern ScrollBar Style --- */
        QScrollBar:vertical {
            border: none;
            background: #1e1e1e;
            width: 8px;
            margin: 0px;
        }
        QScrollBar::handle:vertical {
            background: #3e3e42;
            min-height: 20px;
            border-radius: 4px;
            margin: 2px;
        }
        QScrollBar::handle:vertical:hover {
            background: #505050;
        }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
            height: 0px;
        }
        QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical {
            background: none;
        }

        QScrollBar:horizontal {
            border: none;
            background: #1e1e1e;
            height: 8px;
            margin: 0px;
        }
        QScrollBar::handle:horizontal {
            background: #3e3e42;
            min-width: 20px;
            border-radius: 4px;
            margin: 2px;
        }
        QScrollBar::handle:horizontal:hover {
            background: #505050;
        }
        QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal {
            width: 0px;
        }
        QScrollBar::add-page:horizontal, QScrollBar::sub-page:horizontal {
            background: none;
        }
    )");
}

ArtifactProjectView::~ArtifactProjectView() { delete impl_; }

void ArtifactProjectView::handleItemDoubleClicked(const QModelIndex& index) {
    if (!index.isValid()) return;
    QModelIndex actualIdx = index;
    if (auto proxy = qobject_cast<const QSortFilterProxyModel*>(index.model())) {
        actualIdx = proxy->mapToSource(index);
    }

    QVariant typeVar = actualIdx.data(Qt::UserRole + static_cast<int>(Artifact::ProjectItemDataRole::ProjectItemType));
    if (typeVar.isValid()) {
        if (typeVar.toInt() == static_cast<int>(eProjectItemType::Composition)) {
             QVariant idVar = actualIdx.data(Qt::UserRole + static_cast<int>(Artifact::ProjectItemDataRole::CompositionId));
             if (idVar.isValid()) {
                 CompositionID cid(idVar.toString());
                 ArtifactLayerTimelinePanelWrapper* panel = new ArtifactLayerTimelinePanelWrapper(cid);
                 panel->setAttribute(Qt::WA_DeleteOnClose);
                 panel->show();
             }
        }
    }
}

void ArtifactProjectView::mouseDoubleClickEvent(QMouseEvent* event) {
    QModelIndex idx = indexAt(event->position().toPoint());
    if (idx.isValid()) handleItemDoubleClicked(idx);
    QTreeView::mouseDoubleClickEvent(event);
}

void ArtifactProjectView::mouseMoveEvent(QMouseEvent* event) {
    QModelIndex idx = indexAt(event->position().toPoint());
    if (!impl_->hoverTimer) {
        impl_->hoverTimer = new QTimer(this);
        impl_->hoverTimer->setSingleShot(true);
        connect(impl_->hoverTimer, &QTimer::timeout, this, [this]() {
            if (impl_->hoverIndex.isValid()) {
                if (!impl_->hoverPopup) impl_->hoverPopup = new HoverThumbnailPopupWidget();
                QString text = impl_->hoverIndex.data(Qt::DisplayRole).toString();
                impl_->hoverPopup->setLabels(QStringList() << text << "Metadata Info" << "");
                impl_->hoverPopup->showAt(QCursor::pos() + QPoint(15, 15));
            }
        });
    }
    if (idx != impl_->hoverIndex) {
        if (impl_->hoverPopup) impl_->hoverPopup->hide();
        impl_->hoverIndex = idx;
        impl_->hoverTimer->stop();
        if (idx.isValid()) impl_->hoverTimer->start(600);
    }
    QTreeView::mouseMoveEvent(event);
}

void ArtifactProjectView::contextMenuEvent(QContextMenuEvent* event) {
    QModelIndex idx = indexAt(event->pos());
    if(!idx.isValid()) return;
    QMenu menu(this);
    menu.addAction("Open", [this, idx]() { handleItemDoubleClicked(idx); });
    menu.addAction("Rename", [this, idx]() {
        bool ok;
        QString name = QInputDialog::getText(this, "Rename", "New Name:", QLineEdit::Normal, idx.data().toString(), &ok);
        if(ok && !name.isEmpty()) {
             QModelIndex actualIdx = idx;
             if(auto proxy = qobject_cast<QSortFilterProxyModel*>(model())) actualIdx = proxy->mapToSource(idx);
             model()->setData(idx, name, Qt::EditRole);
        }
    });
    menu.exec(event->globalPos());
}

void ArtifactProjectView::dropEvent(QDropEvent* event) {
    event->setDropAction(Qt::CopyAction);
    const QMimeData* mimeData = event->mimeData();
    if (mimeData->hasUrls()) {
        for (const QUrl& url : mimeData->urls()) {
            QString filePath = url.toLocalFile().toLower();
            if (filePath.endsWith(".png") || filePath.endsWith(".jpg") || filePath.endsWith(".jpeg") ||
                filePath.endsWith(".bmp") || filePath.endsWith(".gif") || filePath.endsWith(".mp4") ||
                filePath.endsWith(".avi") || filePath.endsWith(".mov") || filePath.endsWith(".mkv")) {
                impl_->handleFileDrop(filePath);
            }
        }
        event->acceptProposedAction();
    } else {
        event->ignore();
    }
}

void ArtifactProjectView::dragEnterEvent(QDragEnterEvent* event) {
    if (event->mimeData()->hasUrls()) event->acceptProposedAction();
    else event->ignore();
}

void ArtifactProjectView::dragMoveEvent(QDragMoveEvent* event) {
    if (event->mimeData()->hasUrls()) event->acceptProposedAction();
    else event->ignore();
}

void ArtifactProjectView::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
         QModelIndex idx = indexAt(event->position().toPoint());
         if (idx.isValid()) itemSelected(idx);
    }
    QTreeView::mousePressEvent(event);
}

void ArtifactProjectView::mouseReleaseEvent(QMouseEvent* event) {
    if (impl_ && impl_->hoverTimer) impl_->hoverTimer->stop();
    QTreeView::mouseReleaseEvent(event);
}

QSize ArtifactProjectView::sizeHint() const { return QSize(400, 400); }

// --- Main Widget Implementation ---
class ArtifactProjectManagerWidget::Impl {
public:
    ArtifactProjectView* projectView_ = nullptr;
    ArtifactProjectModel* projectModel_ = nullptr;
    QSortFilterProxyModel* proxyModel_ = nullptr;
    ProjectInfoPanel* infoPanel_ = nullptr;
    QLineEdit* searchBar = nullptr;
    ArtifactProjectManagerToolBox* toolBox = nullptr;
    QLabel* projectNameLabel = nullptr;

    void update() {
        auto shared = ArtifactProjectService::instance()->getCurrentProjectSharedPtr();
        if (!projectModel_) projectModel_ = new ArtifactProjectModel();
        projectModel_->setProject(shared);

        if (!proxyModel_) {
            proxyModel_ = new QSortFilterProxyModel();
            proxyModel_->setSourceModel(projectModel_);
            proxyModel_->setFilterCaseSensitivity(Qt::CaseInsensitive);
            proxyModel_->setRecursiveFilteringEnabled(true);
        }
        if (projectView_) projectView_->setModel(proxyModel_);
    }

    void handleSearch(const QString& text) {
        if(proxyModel_) proxyModel_->setFilterFixedString(text);
    }
};

ArtifactProjectManagerWidget::ArtifactProjectManagerWidget(QWidget* parent)
    : QWidget(parent), impl_(new Impl())
{
    auto mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    impl_->infoPanel_ = new ProjectInfoPanel(this);
    mainLayout->addWidget(impl_->infoPanel_);

    impl_->projectNameLabel = new QLabel("PROJECT");
    impl_->projectNameLabel->setStyleSheet("color: #888; font-size: 10px; padding: 5px 10px; font-weight: bold;");
    mainLayout->addWidget(impl_->projectNameLabel);

    impl_->searchBar = new QLineEdit();
    impl_->searchBar->setPlaceholderText("Search project...");
    impl_->searchBar->setStyleSheet(R"(
        QLineEdit {
            background-color: #1E1E1E;
            color: #AAA;
            border: 1px solid #333;
            border-radius: 4px;
            padding: 5px 10px;
            margin: 0 10px 8px 10px;
            font-size: 11px;
        }
        QLineEdit:focus { border: 1px solid #007ACC; color: white; }
    )");
    mainLayout->addWidget(impl_->searchBar);

    impl_->projectView_ = new ArtifactProjectView();
    mainLayout->addWidget(impl_->projectView_);

    impl_->toolBox = new ArtifactProjectManagerToolBox();
    mainLayout->addWidget(impl_->toolBox);

    connect(impl_->searchBar, &QLineEdit::textChanged, [this](const QString& t) { impl_->handleSearch(t); });
    connect(impl_->projectView_, &ArtifactProjectView::itemSelected, [this](const QModelIndex& idx) {
        if (impl_->proxyModel_) impl_->infoPanel_->updateInfo(impl_->proxyModel_->mapToSource(idx));
    });

    auto svc = ArtifactProjectService::instance();
    connect(svc, &ArtifactProjectService::projectChanged, this, [this]() { updateRequested(); });
    connect(svc, &ArtifactProjectService::projectCreated, this, [this]() { updateRequested(); });

    impl_->update();
}

ArtifactProjectManagerWidget::~ArtifactProjectManagerWidget() { delete impl_; }

void ArtifactProjectManagerWidget::updateRequested() {
    impl_->update();
    setEnabled(true);
}

void ArtifactProjectManagerWidget::triggerUpdate() { impl_->update(); }
void ArtifactProjectManagerWidget::setFilter() {}
void ArtifactProjectManagerWidget::setThumbnailEnabled(bool) {}
void ArtifactProjectManagerWidget::dropEvent(QDropEvent* event) { /* Handled by child view but kept for API */ }
void ArtifactProjectManagerWidget::dragEnterEvent(QDragEnterEvent* event) { /* Handled by child view but kept for API */ }
void ArtifactProjectManagerWidget::contextMenuEvent(QContextMenuEvent* event) { /* Handled by child view but kept for API */ }
QSize ArtifactProjectManagerWidget::sizeHint() const { return QSize(300, 600); }

// --- ToolBox Implementation ---
ArtifactProjectManagerToolBox::ArtifactProjectManagerToolBox(QWidget* parent) : QWidget(parent) {
    setFixedHeight(32);
    setStyleSheet("background-color: #2D2D30; border-top: 1px solid #1a1a1a;");
    auto layout = new QHBoxLayout(this);
    layout->setContentsMargins(10, 0, 10, 0);
    layout->setSpacing(10);

    auto createBtn = [](const QString& icon, const QString& tip) {
        auto b = new QPushButton(icon);
        b->setFixedSize(24, 24);
        b->setToolTip(tip);
        b->setStyleSheet(R"(
            QPushButton { background: transparent; border: none; font-size: 14px; color: #888; border-radius: 3px; }
            QPushButton:hover { background: #444; color: white; }
        )");
        return b;
    };

    auto btnNew = createBtn("📜", "New Composition");
    auto btnFolder = createBtn("📁", "New Folder");
    auto btnDel = createBtn("🗑️", "Delete");

    layout->addWidget(btnNew);
    layout->addWidget(btnFolder);
    layout->addStretch();
    layout->addWidget(btnDel);

    connect(btnNew, &QPushButton::clicked, []() { ArtifactProjectService::instance()->createComposition(UniString("Composition")); });
}

ArtifactProjectManagerToolBox::~ArtifactProjectManagerToolBox() {}
void ArtifactProjectManagerToolBox::resizeEvent(QResizeEvent*) {}

}
