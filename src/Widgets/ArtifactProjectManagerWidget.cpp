module;
#include <QWidget>
#include <wobjectimpl.h>
module ArtifactProjectManagerWidget;


import HeadPanel;

namespace Artifact {

 class ArtifactProjectManagerWidget::Impl {
 public:

  
 };

	W_OBJECT_IMPL(ArtifactProjectManagerWidget)

 ArtifactProjectManagerWidget::ArtifactProjectManagerWidget(QWidget* parent /*= nullptr*/)
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

 }

 ArtifactProjectManagerWidget::~ArtifactProjectManagerWidget()
 {

 }

 void ArtifactProjectManagerWidget::dropEvent(QDropEvent* event)
 {

 }

 void ArtifactProjectManagerWidget::triggerUpdate()
 {

 }

 void ArtifactProjectManagerWidget::dragEnterEvent(QDragEnterEvent* event)
 {

 }

}
