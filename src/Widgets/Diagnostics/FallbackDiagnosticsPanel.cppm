module;
#include <QHeaderView>
#include <QDateTime>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QComboBox>
#include <QPushButton>
#include <QTreeWidget>
#include <wobjectimpl.h>

module Artifact.Widgets.Diagnostics.FallbackPanel;

W_OBJECT_IMPL(FallbackDiagnosticsPanel)

namespace {

QString fallbackCategoryText(ArtifactCore::FallbackCategory cat) {
    switch (cat) {
    case ArtifactCore::FallbackCategory::Font: return QStringLiteral("Font");
    case ArtifactCore::FallbackCategory::Image: return QStringLiteral("Image");
    case ArtifactCore::FallbackCategory::Color: return QStringLiteral("Color");
    case ArtifactCore::FallbackCategory::Effect: return QStringLiteral("Effect");
    case ArtifactCore::FallbackCategory::Asset: return QStringLiteral("Asset");
    default: return QStringLiteral("Other");
    }
}

QString fallbackActionText(ArtifactCore::FallbackAction action) {
    switch (action) {
    case ArtifactCore::FallbackAction::Fallback: return QStringLiteral("Fallback");
    case ArtifactCore::FallbackAction::Bypass: return QStringLiteral("Bypass");
    case ArtifactCore::FallbackAction::Warning: return QStringLiteral("Warning");
    case ArtifactCore::FallbackAction::Strict: return QStringLiteral("Strict");
    case ArtifactCore::FallbackAction::Ignore: return QStringLiteral("Ignore");
    default: return QStringLiteral("Unknown");
    }
}

}

FallbackDiagnosticsPanel::FallbackDiagnosticsPanel(QWidget* parent)
    : QWidget(parent)
{
    setupUi();
    refresh();
}

void FallbackDiagnosticsPanel::setupUi()
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(4);

    auto* header = new QHBoxLayout;
    auto* titleLabel = new QLabel(QStringLiteral("Fallback Diagnostics"));
    QFont titleFont = titleLabel->font();
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);
    header->addWidget(titleLabel);

    header->addStretch();

    filterCombo_ = new QComboBox;
    filterCombo_->addItem(QStringLiteral("All"));
    filterCombo_->addItem(QStringLiteral("Font"));
    filterCombo_->addItem(QStringLiteral("Image"));
    filterCombo_->addItem(QStringLiteral("Color"));
    filterCombo_->addItem(QStringLiteral("Effect"));
    filterCombo_->addItem(QStringLiteral("Asset"));
    filterCombo_->addItem(QStringLiteral("Other"));
    header->addWidget(filterCombo_);

    refreshButton_ = new QPushButton(QStringLiteral("Refresh"));
    header->addWidget(refreshButton_);

    clearButton_ = new QPushButton(QStringLiteral("Clear"));
    header->addWidget(clearButton_);

    layout->addLayout(header);

    tree_ = new QTreeWidget;
    tree_->setColumnCount(5);
    tree_->setHeaderLabels({
        QStringLiteral("Time"),
        QStringLiteral("Category"),
        QStringLiteral("Action"),
        QStringLiteral("Original"),
        QStringLiteral("Resolved")
    });
    tree_->setRootIsDecorated(false);
    tree_->setAlternatingRowColors(true);
    tree_->header()->setStretchLastSection(true);
    tree_->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
    layout->addWidget(tree_, 1);

    summaryLabel_ = new QLabel;
    layout->addWidget(summaryLabel_);

    connect(refreshButton_, &QPushButton::clicked, this, &FallbackDiagnosticsPanel::refresh);
    connect(clearButton_, &QPushButton::clicked, this, [this]() {
        ArtifactCore::FallbackTracker::instance()->clear();
        populateTree();
        Q_EMIT cleared();
    });
    connect(filterCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int) { populateTree(); });
}

void FallbackDiagnosticsPanel::refresh()
{
    populateTree();
}

void FallbackDiagnosticsPanel::populateTree()
{
    tree_->clear();

    auto* tracker = ArtifactCore::FallbackTracker::instance();
    auto events = tracker->getEvents();

    const int filterIdx = filterCombo_->currentIndex();
    if (filterIdx > 0) {
        const auto filterCat = static_cast<ArtifactCore::FallbackCategory>(filterIdx - 1);
        events = tracker->getEventsByCategory(filterCat);
    }

    int warnCount = 0;
    for (const auto& e : events) {
        auto* item = new QTreeWidgetItem(tree_);
        item->setText(0, e.timestamp.toString(QStringLiteral("HH:mm:ss.zzz")));
        item->setText(1, fallbackCategoryText(e.category));
        item->setText(2, fallbackActionText(e.action));
        item->setText(3, e.originalId);
        item->setText(4, e.resolvedId);
        if (e.isWarning) {
            for (int i = 0; i < 5; ++i) {
                item->setForeground(i, QColor(255, 200, 100));
            }
            ++warnCount;
        }
    }

    summaryLabel_->setText(QStringLiteral("Total: %1 | Warnings: %2")
        .arg(events.size()).arg(warnCount));
}

QString FallbackDiagnosticsPanel::categoryText(ArtifactCore::FallbackCategory cat) const {
    return fallbackCategoryText(cat);
}

QString FallbackDiagnosticsPanel::actionText(ArtifactCore::FallbackAction action) const {
    return fallbackActionText(action);
}
