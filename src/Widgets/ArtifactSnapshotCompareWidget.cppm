module;
#include <utility>
#include <functional>

#include <QWidget>
#include <QFrame>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QSplitter>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QComboBox>
#include <QListWidgetItem>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonDocument>
#include <QMouseEvent>
#include <QStringList>
#include <QVector>
#include <QColor>
#include <QFont>
#include <QPalette>
#include <wobjectimpl.h>
module Artifact.Widgets.SnapshotCompareWidget;

import std;
import Artifact.Project.RevisionService;
import Widgets.Utils.CSS;

namespace Artifact {

namespace {

class SnapshotActionButton final : public QPushButton {
public:
    using Callback = std::function<void()>;

    explicit SnapshotActionButton(const QString& text, QWidget* parent = nullptr)
        : QPushButton(text, parent) {}

    void setCallback(Callback callback) { callback_ = std::move(callback); }

protected:
    void mouseReleaseEvent(QMouseEvent* event) override {
        QPushButton::mouseReleaseEvent(event);
        if (!event || !isEnabled() || event->button() != Qt::LeftButton ||
            !rect().contains(event->pos())) {
            return;
        }
        if (callback_) {
            callback_();
        }
    }

private:
    Callback callback_;
};

QString snapshotDisplayName(const ProjectRevisionRecord& record) {
    const QString shortId = record.id.left(8);
    const QString message =
        record.message.isEmpty() ? QStringLiteral("Snapshot") : record.message;
    return QStringLiteral("%1 [%2]").arg(message, shortId);
}

QString resolveSnapshotId(const QString& rawValue,
                          const QVector<ProjectRevisionRecord>& revisions) {
    const QString trimmed = rawValue.trimmed();
    if (trimmed.isEmpty()) {
        return {};
    }

    for (const auto& revision : revisions) {
        if (revision.id == trimmed || revision.message == trimmed) {
            return revision.id;
        }
    }
    return trimmed;
}

QString selectedSnapshotId(QComboBox* combo) {
    if (!combo) {
        return {};
    }
    return combo->currentData().toString();
}

int findSnapshotIndex(QComboBox* combo, const QString& snapshotId) {
    if (!combo || snapshotId.isEmpty()) {
        return -1;
    }
    int index = combo->findData(snapshotId);
    if (index >= 0) {
        return index;
    }
    index = combo->findText(snapshotId, Qt::MatchContains);
    return index;
}

QString joinChangedPaths(const QJsonArray& changes) {
    QStringList paths;
    for (const auto& value : changes) {
        if (!value.isObject()) {
            continue;
        }
        const QString path =
            value.toObject().value(QStringLiteral("path")).toString();
        if (!path.isEmpty()) {
            paths.push_back(path);
        }
    }
    return paths.join(QStringLiteral(", "));
}

void populateComparisonLists(QListWidget* leftList, QListWidget* rightList,
                             const QJsonArray& changes) {
    if (leftList) {
        leftList->clear();
    }
    if (rightList) {
        rightList->clear();
    }

    if (changes.isEmpty()) {
        if (leftList) {
            leftList->addItem(QStringLiteral("No changes found"));
        }
        if (rightList) {
            rightList->addItem(QStringLiteral("No changes found"));
        }
        return;
    }

    for (const auto& value : changes) {
        if (!value.isObject()) {
            continue;
        }
        const QJsonObject change = value.toObject();
        const QString path = change.value(QStringLiteral("path")).toString();
        const QString changeType = change.value(QStringLiteral("change")).toString();
        const QString beforeText =
            change.value(QStringLiteral("beforeText")).toString();
        const QString afterText =
            change.value(QStringLiteral("afterText")).toString();

        const QString pathText =
            path.isEmpty() ? QStringLiteral("<root>") : path;
        if (leftList) {
            leftList->addItem(QStringLiteral("%1 [%2] %3")
                                  .arg(pathText, changeType, beforeText));
        }
        if (rightList) {
            rightList->addItem(QStringLiteral("%1 [%2] %3")
                                   .arg(pathText, changeType, afterText));
        }
    }
}

} // namespace

W_OBJECT_IMPL(ArtifactSnapshotCompareWidget)

class ArtifactSnapshotCompareWidget::Impl {
public:
    // UI Components
    QComboBox* snapshotASelector = nullptr;
    QComboBox* snapshotBSelector = nullptr;

    QListWidget* snapshotAList = nullptr;
    QListWidget* snapshotBList = nullptr;

    SnapshotActionButton* compareButton = nullptr;
    SnapshotActionButton* branchButton = nullptr;
    SnapshotActionButton* restoreAButton = nullptr;
    SnapshotActionButton* restoreBButton = nullptr;
    SnapshotActionButton* diffButton = nullptr;

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

    impl_->compareButton = new SnapshotActionButton("Compare", this);
    impl_->restoreAButton = new SnapshotActionButton("Restore A", this);
    impl_->restoreBButton = new SnapshotActionButton("Restore B", this);
    impl_->branchButton = new SnapshotActionButton("Branch", this);
    impl_->diffButton = new SnapshotActionButton("Diff", this);

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

    impl_->compareButton->setCallback([this]() { onCompare(); });
    impl_->restoreAButton->setCallback([this]() { onRestoreA(); });
    impl_->restoreBButton->setCallback([this]() { onRestoreB(); });
    impl_->branchButton->setCallback([this]() { onBranch(); });
    impl_->diffButton->setCallback([this]() { onDiff(); });

    impl_->compareButton->setToolTip(QStringLiteral("Compare the selected snapshots"));
    impl_->restoreAButton->setToolTip(QStringLiteral("Restore Snapshot A to the current project"));
    impl_->restoreBButton->setToolTip(QStringLiteral("Restore Snapshot B to the current project"));
    impl_->branchButton->setToolTip(QStringLiteral("Restore the selected snapshot and commit a branch snapshot"));
    impl_->diffButton->setToolTip(QStringLiteral("Show a concise diff summary for the selected snapshots"));
}

ArtifactSnapshotCompareWidget::~ArtifactSnapshotCompareWidget()
{
    delete impl_;
}

void ArtifactSnapshotCompareWidget::loadSnapshots(const std::vector<QString>& snapshotNames)
{
    const auto* service = ArtifactRevisionService::instance();
    const QVector<ProjectRevisionRecord> revisions =
        service ? service->revisions() : QVector<ProjectRevisionRecord>{};

    impl_->availableSnapshots.clear();

    impl_->snapshotASelector->clear();
    impl_->snapshotBSelector->clear();

    const auto appendSnapshot = [&](const QString& rawValue) {
        const QString snapshotId = resolveSnapshotId(rawValue, revisions);
        if (snapshotId.isEmpty()) {
            return;
        }

        QString displayText = snapshotId;
        for (const auto& record : revisions) {
            if (record.id == snapshotId) {
                displayText = snapshotDisplayName(record);
                break;
            }
        }

        impl_->availableSnapshots.push_back(snapshotId);
        impl_->snapshotASelector->addItem(displayText, snapshotId);
        impl_->snapshotBSelector->addItem(displayText, snapshotId);
    };

    if (snapshotNames.empty()) {
        for (const auto& record : revisions) {
            appendSnapshot(record.id);
        }
    } else {
        for (const auto& name : snapshotNames) {
            appendSnapshot(name);
        }
    }

    if (impl_->snapshotASelector->count() > 0) {
        impl_->snapshotASelector->setCurrentIndex(0);
    }
    if (impl_->snapshotBSelector->count() > 1) {
        impl_->snapshotBSelector->setCurrentIndex(1);
    } else if (impl_->snapshotBSelector->count() > 0) {
        impl_->snapshotBSelector->setCurrentIndex(0);
    }

    impl_->currentSnapshotA = selectedSnapshotId(impl_->snapshotASelector);
    impl_->currentSnapshotB = selectedSnapshotId(impl_->snapshotBSelector);

    if (impl_->currentSnapshotA.isEmpty() || impl_->currentSnapshotB.isEmpty()) {
        impl_->statusLabel->setText(QStringLiteral("No snapshots selected"));
    } else {
        impl_->statusLabel->setText(QStringLiteral("Ready to compare %1 vs %2")
                                        .arg(impl_->currentSnapshotA.left(8),
                                             impl_->currentSnapshotB.left(8)));
    }
}

void ArtifactSnapshotCompareWidget::onCompare()
{
    const auto* service = ArtifactRevisionService::instance();
    if (!service) {
        impl_->statusLabel->setText(QStringLiteral("Snapshot service unavailable"));
        return;
    }

    const QString leftId = selectedSnapshotId(impl_->snapshotASelector);
    const QString rightId = selectedSnapshotId(impl_->snapshotBSelector);
    if (leftId.isEmpty() || rightId.isEmpty()) {
        impl_->statusLabel->setText(QStringLiteral("Select two snapshots first"));
        return;
    }

    impl_->currentSnapshotA = leftId;
    impl_->currentSnapshotB = rightId;

    const QJsonObject diff = service->diffRevisions(leftId, rightId);
    const QJsonArray changes = diff.value(QStringLiteral("changes")).toArray();
    populateComparisonLists(impl_->snapshotAList, impl_->snapshotBList, changes);

    const int changeCount = diff.value(QStringLiteral("changeCount")).toInt(changes.size());
    impl_->statusLabel->setText(QStringLiteral("Compared %1 vs %2: %3 change(s)")
                                    .arg(leftId.left(8), rightId.left(8))
                                    .arg(changeCount));
}

void ArtifactSnapshotCompareWidget::onRestoreA()
{
    auto* service = ArtifactRevisionService::instance();
    if (!service) {
        impl_->statusLabel->setText(QStringLiteral("Snapshot service unavailable"));
        return;
    }

    const QString leftId = selectedSnapshotId(impl_->snapshotASelector);
    if (leftId.isEmpty()) {
        impl_->statusLabel->setText(QStringLiteral("Choose Snapshot A first"));
        return;
    }

    if (service->restoreRevision(leftId)) {
        impl_->statusLabel->setText(QStringLiteral("Restored Snapshot A: %1").arg(leftId.left(8)));
    } else {
        impl_->statusLabel->setText(QStringLiteral("Failed to restore Snapshot A"));
    }
}

void ArtifactSnapshotCompareWidget::onRestoreB()
{
    auto* service = ArtifactRevisionService::instance();
    if (!service) {
        impl_->statusLabel->setText(QStringLiteral("Snapshot service unavailable"));
        return;
    }

    const QString rightId = selectedSnapshotId(impl_->snapshotBSelector);
    if (rightId.isEmpty()) {
        impl_->statusLabel->setText(QStringLiteral("Choose Snapshot B first"));
        return;
    }

    if (service->restoreRevision(rightId)) {
        impl_->statusLabel->setText(QStringLiteral("Restored Snapshot B: %1").arg(rightId.left(8)));
    } else {
        impl_->statusLabel->setText(QStringLiteral("Failed to restore Snapshot B"));
    }
}

void ArtifactSnapshotCompareWidget::onBranch()
{
    auto* service = ArtifactRevisionService::instance();
    if (!service) {
        impl_->statusLabel->setText(QStringLiteral("Snapshot service unavailable"));
        return;
    }

    const QString sourceId = !selectedSnapshotId(impl_->snapshotASelector).isEmpty()
                                 ? selectedSnapshotId(impl_->snapshotASelector)
                                 : selectedSnapshotId(impl_->snapshotBSelector);
    if (sourceId.isEmpty()) {
        impl_->statusLabel->setText(QStringLiteral("Choose a snapshot to branch from"));
        return;
    }

    if (!service->restoreRevision(sourceId)) {
        impl_->statusLabel->setText(QStringLiteral("Failed to restore source snapshot"));
        return;
    }

    const QString message = QStringLiteral("Branch from snapshot %1").arg(sourceId.left(8));
    if (service->commitCurrentProject(message, QString(), {QStringLiteral("branch"), sourceId.left(8)})) {
        impl_->statusLabel->setText(QStringLiteral("Created branch from %1").arg(sourceId.left(8)));
    } else {
        impl_->statusLabel->setText(QStringLiteral("Branch commit failed"));
    }
}

void ArtifactSnapshotCompareWidget::onDiff()
{
    const auto* service = ArtifactRevisionService::instance();
    if (!service) {
        impl_->statusLabel->setText(QStringLiteral("Snapshot service unavailable"));
        return;
    }

    const QString leftId = selectedSnapshotId(impl_->snapshotASelector);
    const QString rightId = selectedSnapshotId(impl_->snapshotBSelector);
    if (leftId.isEmpty() || rightId.isEmpty()) {
        impl_->statusLabel->setText(QStringLiteral("Select two snapshots first"));
        return;
    }

    const QJsonObject diff = service->diffRevisions(leftId, rightId);
    const QJsonArray changes = diff.value(QStringLiteral("changes")).toArray();
    populateComparisonLists(impl_->snapshotAList, impl_->snapshotBList, changes);

    const QString changedPaths = joinChangedPaths(changes);
    if (changedPaths.isEmpty()) {
        impl_->statusLabel->setText(QStringLiteral("No differences between %1 and %2")
                                        .arg(leftId.left(8), rightId.left(8)));
    } else {
        impl_->statusLabel->setText(QStringLiteral("Diff %1 → %2: %3")
                                        .arg(leftId.left(8), rightId.left(8), changedPaths));
    }
}

void ArtifactSnapshotCompareWidget::setSnapshotA(const QString& name)
{
    impl_->currentSnapshotA = name;
    const auto* service = ArtifactRevisionService::instance();
    const QVector<ProjectRevisionRecord> revisions =
        service ? service->revisions() : QVector<ProjectRevisionRecord>{};
    const int index = findSnapshotIndex(impl_->snapshotASelector, resolveSnapshotId(name, revisions));
    if (index >= 0) {
        impl_->snapshotASelector->setCurrentIndex(index);
    }
}

void ArtifactSnapshotCompareWidget::setSnapshotB(const QString& name)
{
    impl_->currentSnapshotB = name;
    const auto* service = ArtifactRevisionService::instance();
    const QVector<ProjectRevisionRecord> revisions =
        service ? service->revisions() : QVector<ProjectRevisionRecord>{};
    const int index = findSnapshotIndex(impl_->snapshotBSelector, resolveSnapshotId(name, revisions));
    if (index >= 0) {
        impl_->snapshotBSelector->setCurrentIndex(index);
    }
}

} // namespace Artifact
