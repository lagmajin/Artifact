module;
#include <wobjectimpl.h>
#include <QWidget>
#include <QFrame>
#include <QLabel>
#include <QTreeWidget>
#include <QComboBox>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QColor>
#include <QFont>
#include <QPalette>
module Artifact.Widgets.ProblemViewWidget;

import Artifact.Widgets.ProblemViewWidget;
import Artifact.Core.Theme;

namespace Artifact {

W_OBJECT_IMPL(ArtifactProblemViewWidget)

class ArtifactProblemViewWidget::Impl {
public:
    // UI Components
    QTreeWidget* problemTree = nullptr;
    QComboBox* severityFilter = nullptr;
    QComboBox* categoryFilter = nullptr;
    QPushButton* refreshButton = nullptr;
    QPushButton* clearButton = nullptr;
    QLabel* summaryLabel = nullptr;
    QFrame* divider = nullptr;
};

ArtifactProblemViewWidget::ArtifactProblemViewWidget(QWidget* parent)
    : QWidget(parent), impl_(new Impl())
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(8, 8, 8, 8);
    root->setSpacing(8);

    // Theme setup
    setAutoFillBackground(true);
    QPalette widgetPalette = palette();
    widgetPalette.setColor(QPalette::Window, QColor(ArtifactCore::currentDCCTheme().backgroundColor));
    widgetPalette.setColor(QPalette::WindowText, QColor(ArtifactCore::currentDCCTheme().textColor));
    setPalette(widgetPalette);

    // Header: Filters
    auto* headerLayout = new QHBoxLayout();
    headerLayout->setSpacing(12);

    impl_->severityFilter = new QComboBox(this);
    impl_->severityFilter->addItems({"All Severities", "Errors Only", "Warnings Only", "Info Only"});
    headerLayout->addWidget(impl_->severityFilter, 1);

    impl_->categoryFilter = new QComboBox(this);
    impl_->categoryFilter->addItems({"All Categories", "References", "Mattes", "Circular Deps", "Expressions", "Performance"});
    headerLayout->addWidget(impl_->categoryFilter, 1);

    root->addLayout(headerLayout);

    // Action buttons
    auto* actionLayout = new QHBoxLayout();
    actionLayout->setSpacing(8);

    impl_->refreshButton = new QPushButton("Refresh", this);
    impl_->clearButton = new QPushButton("Clear", this);

    actionLayout->addWidget(impl_->refreshButton);
    actionLayout->addWidget(impl_->clearButton);
    actionLayout->addStretch();

    root->addLayout(actionLayout);

    // Divider
    impl_->divider = new QFrame(this);
    impl_->divider->setFrameShape(QFrame::HLine);
    impl_->divider->setFrameShadow(QFrame::Sunken);
    root->addWidget(impl_->divider);

    // Problem tree
    impl_->problemTree = new QTreeWidget(this);
    impl_->problemTree->setHeaderLabels({"Severity", "Category", "Message", "Source"});
    impl_->problemTree->setColumnWidth(0, 80);
    impl_->problemTree->setColumnWidth(1, 100);
    impl_->problemTree->setColumnWidth(2, 400);
    impl_->problemTree->setColumnWidth(3, 150);
    impl_->problemTree->setAlternatingRowColors(true);
    impl_->problemTree->setSelectionMode(QTreeWidget::SingleSelection);
    impl_->problemTree->setSortingEnabled(true);

    root->addWidget(impl_->problemTree, 1);

    // Summary
    impl_->summaryLabel = new QLabel("0 Errors, 0 Warnings, 0 Info", this);
    impl_->summaryLabel->setStyleSheet("color: #888;");
    root->addWidget(impl_->summaryLabel);

    // TODO: Connect signals
    // connect(impl_->refreshButton, &QPushButton::clicked, this, &ArtifactProblemViewWidget::onRefresh);
    // connect(impl_->problemTree, &QTreeWidget::itemDoubleClicked, this, &ArtifactProblemViewWidget::onNavigateToProblem);
}

ArtifactProblemViewWidget::~ArtifactProblemViewWidget()
{
    delete impl_;
}

void ArtifactProblemViewWidget::loadDiagnostics(const std::vector<ArtifactCore::ProjectDiagnostic>& diagnostics)
{
    // TODO: 問題一覧をツリーにロード
    // impl_->problemTree->clear();
    // for (const auto& diag : diagnostics) {
    //     auto* item = new QTreeWidgetItem(impl_->problemTree);
    //     item->setText(0, getSeverityIcon(diag.getSeverity()));
    //     item->setText(1, getCategoryName(diag.getCategory()));
    //     item->setText(2, diag.getMessage());
    //     item->setText(3, diag.getSourceLayerId());
    // }
    // updateSummary(diagnostics);
}

void ArtifactProblemViewWidget::onRefresh()
{
    // TODO: 再検証を実行
}

void ArtifactProblemViewWidget::onNavigateToProblem(QTreeWidgetItem* item, int column)
{
    // TODO: 問題箇所へジャンプ
}

void ArtifactProblemViewWidget::updateSummary(const std::vector<ArtifactCore::ProjectDiagnostic>& diagnostics)
{
    // TODO: サマリー更新
    // int errors = 0, warnings = 0, infos = 0;
    // for (const auto& diag : diagnostics) {
    //     if (diag.isError()) ++errors;
    //     else if (diag.isWarning()) ++warnings;
    //     else ++infos;
    // }
    // impl_->summaryLabel->setText(QString("%1 Errors, %2 Warnings, %3 Info")
    //     .arg(errors).arg(warnings).arg(infos));
}

} // namespace Artifact
