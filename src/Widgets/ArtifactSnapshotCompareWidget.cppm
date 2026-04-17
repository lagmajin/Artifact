module;
#include <wobjectimpl.h>
#include <QWidget>
#include <QFrame>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QSplitter>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QComboBox>
#include <QColor>
#include <QFont>
#include <QPalette>
module Artifact.Widgets.SnapshotCompareWidget;

import Artifact.Widgets.SnapshotCompareWidget;
import Widgets.Utils.CSS;

namespace Artifact {

W_OBJECT_IMPL(ArtifactSnapshotCompareWidget)

class ArtifactSnapshotCompareWidget::Impl {
public:
    // UI Components
    QComboBox* snapshotASelector = nullptr;
    QComboBox* snapshotBSelector = nullptr;
    
    QListWidget* snapshotAList = nullptr;
    QListWidget* snapshotBList = nullptr;
    
    QPushButton* compareButton = nullptr;
    QPushButton* branchButton = nullptr;
    QPushButton* restoreAButton = nullptr;
    QPushButton* restoreBButton = nullptr;
    QPushButton* diffButton = nullptr;
    
    QLabel* statusLabel = nullptr;
    QFrame* divider = nullptr;
    
    // State
    QString currentSnapshotA;
    QString currentSnapshotB;
    std::vector<QString> availableSnapshots;
};

ArtifactSnapshotCompareWidget::ArtifactSnapshotCompareWidget(QWidget* parent)
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
    
    // Header: Snapshot selectors
    auto* headerLayout = new QHBoxLayout();
    headerLayout->setSpacing(12);
    
    impl_->snapshotASelector = new QComboBox(this);
    impl_->snapshotASelector->setPlaceholderText("Snapshot A");
    headerLayout->addWidget(impl_->snapshotASelector, 1);
    
    impl_->snapshotBSelector = new QComboBox(this);
    impl_->snapshotBSelector->setPlaceholderText("Snapshot B");
    headerLayout->addWidget(impl_->snapshotBSelector, 1);
    
    root->addLayout(headerLayout);
    
    // Action buttons
    auto* actionLayout = new QHBoxLayout();
    actionLayout->setSpacing(8);
    
    impl_->compareButton = new QPushButton("Compare", this);
    impl_->restoreAButton = new QPushButton("Restore A", this);
    impl_->restoreBButton = new QPushButton("Restore B", this);
    impl_->branchButton = new QPushButton("Branch", this);
    impl_->diffButton = new QPushButton("Diff", this);
    
    actionLayout->addWidget(impl_->compareButton);
    actionLayout->addWidget(impl_->restoreAButton);
    actionLayout->addWidget(impl_->restoreBButton);
    actionLayout->addWidget(impl_->branchButton);
    actionLayout->addWidget(impl_->diffButton);
    
    root->addLayout(actionLayout);
    
    // Divider
    impl_->divider = new QFrame(this);
    impl_->divider->setFrameShape(QFrame::HLine);
    impl_->divider->setFrameShadow(QFrame::Sunken);
    root->addWidget(impl_->divider);
    
    // Main content: Split view
    auto* splitter = new QSplitter(Qt::Horizontal, this);
    
    // Left: Snapshot A details
    auto* leftWidget = new QWidget(splitter);
    auto* leftLayout = new QVBoxLayout(leftWidget);
    leftLayout->setContentsMargins(4, 4, 4, 4);
    leftLayout->setSpacing(4);
    
    auto* labelA = new QLabel("Snapshot A", leftWidget);
    labelA->setFont(QFont(labelA->font().family(), -1, QFont::Bold));
    leftLayout->addWidget(labelA);
    
    impl_->snapshotAList = new QListWidget(leftWidget);
    impl_->snapshotAList->setAlternatingRowColors(true);
    leftLayout->addWidget(impl_->snapshotAList);
    
    // Right: Snapshot B details
    auto* rightWidget = new QWidget(splitter);
    auto* rightLayout = new QVBoxLayout(rightWidget);
    rightLayout->setContentsMargins(4, 4, 4, 4);
    rightLayout->setSpacing(4);
    
    auto* labelB = new QLabel("Snapshot B", rightWidget);
    labelB->setFont(QFont(labelB->font().family(), -1, QFont::Bold));
    rightLayout->addWidget(labelB);
    
    impl_->snapshotBList = new QListWidget(rightWidget);
    impl_->snapshotBList->setAlternatingRowColors(true);
    rightLayout->addWidget(impl_->snapshotBList);
    
    splitter->addWidget(leftWidget);
    splitter->addWidget(rightWidget);
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 1);
    
    root->addWidget(splitter, 1);
    
    // Status bar
    impl_->statusLabel = new QLabel("No snapshots selected", this);
    {
        QPalette statusPal = impl_->statusLabel->palette();
        statusPal.setColor(QPalette::WindowText, QColor(ArtifactCore::currentDCCTheme().textColor).darker(130));
        statusPal.setColor(QPalette::Text, QColor(ArtifactCore::currentDCCTheme().textColor).darker(130));
        impl_->statusLabel->setPalette(statusPal);
    }
    root->addWidget(impl_->statusLabel);
    
    // TODO: Connect signals
    // connect(impl_->compareButton, &QPushButton::clicked, this, &ArtifactSnapshotCompareWidget::onCompare);
    // connect(impl_->restoreAButton, &QPushButton::clicked, this, &ArtifactSnapshotCompareWidget::onRestoreA);
    // ...
}

ArtifactSnapshotCompareWidget::~ArtifactSnapshotCompareWidget()
{
    delete impl_;
}

// Placeholder methods for future implementation

void ArtifactSnapshotCompareWidget::loadSnapshots(const std::vector<QString>& snapshotNames)
{
    // TODO: Load snapshot list from manager
    impl_->availableSnapshots = snapshotNames;
    
    impl_->snapshotASelector->clear();
    impl_->snapshotBSelector->clear();
    
    for (const auto& name : snapshotNames) {
        impl_->snapshotASelector->addItem(name);
        impl_->snapshotBSelector->addItem(name);
    }
}

void ArtifactSnapshotCompareWidget::onCompare()
{
    // TODO: Compare two snapshots
    // impl_->snapshotAList->clear();
    // impl_->snapshotBList->clear();
    // Load differences into lists
}

void ArtifactSnapshotCompareWidget::onRestoreA()
{
    // TODO: Restore snapshot A to current composition
}

void ArtifactSnapshotCompareWidget::onRestoreB()
{
    // TODO: Restore snapshot B to current composition
}

void ArtifactSnapshotCompareWidget::onBranch()
{
    // TODO: Create branch from selected snapshot
}

void ArtifactSnapshotCompareWidget::onDiff()
{
    // TODO: Show diff between snapshot A and B
}

void ArtifactSnapshotCompareWidget::setSnapshotA(const QString& name)
{
    impl_->currentSnapshotA = name;
    // TODO: Update UI
}

void ArtifactSnapshotCompareWidget::setSnapshotB(const QString& name)
{
    impl_->currentSnapshotB = name;
    // TODO: Update UI
}

} // namespace Artifact
