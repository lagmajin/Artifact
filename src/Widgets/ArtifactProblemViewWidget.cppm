module;
#include <wobjectimpl.h>
#include <vector>
#include <QWidget>
#include <QFrame>
#include <QLabel>
#include <QApplication>
#include <QBrush>
#include <QClipboard>
#include <QDesktopServices>
#include <QFileInfo>
#include <QLineEdit>
#include <QMap>
#include <QRegularExpression>
#include <QStringList>
#include <QTreeWidget>
#include <QComboBox>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QColor>
#include <QFont>
#include <QPalette>
#include <QShowEvent>
#include <QStyle>
#include <QTreeWidgetItemIterator>
#include <QEvent>
#include <QTimer>
#include <QUrl>
module Artifact.Widgets.ProblemViewWidget;

import Widgets.Utils.CSS;
import Event.Bus;
import Artifact.Event.Types;
import Artifact.Project;
import Artifact.Project.Health;
import Artifact.Widgets.ProjectHealthSummary;
import Artifact.Service.Project;
import Utils.Id;

namespace Artifact {

W_OBJECT_IMPL(ArtifactProblemViewWidget)

class ArtifactProblemViewWidget::Impl {
public:
    // UI Components
    QLineEdit* searchFilter = nullptr;
    QTreeWidget* problemTree = nullptr;
    QComboBox* severityFilter = nullptr;
    QComboBox* categoryFilter = nullptr;
    QComboBox* groupingFilter = nullptr;
    QPushButton* refreshButton = nullptr;
    QPushButton* clearButton = nullptr;
    QLabel* summaryLabel = nullptr;
    QFrame* divider = nullptr;
    std::vector<ArtifactCore::ProjectDiagnostic> diagnostics;
    ArtifactProject* project = nullptr;
    ArtifactCore::EventBus eventBus_ = ArtifactCore::globalEventBus();
    std::vector<ArtifactCore::EventBus::Subscription> eventBusSubscriptions_;
};

namespace {
enum class ProblemGroupingMode {
    Flat,
    Severity,
    Category,
    Source
};

auto convertHealthReportToDiagnostics(const ProjectHealthReport& report)
    -> std::vector<ArtifactCore::ProjectDiagnostic>
{
    std::vector<ArtifactCore::ProjectDiagnostic> diagnostics;
    diagnostics.reserve(static_cast<size_t>(report.issues.size()));

    const auto fixActionForCategory = [](const QString& category) {
        if (category == QStringLiteral("MissingAsset")) {
            return QStringLiteral("Relink the missing asset or remove the footage entry");
        }
        if (category == QStringLiteral("BrokenReference")) {
            return QStringLiteral("Open the composition and replace or remove the broken reference");
        }
        if (category == QStringLiteral("CircularReference")) {
            return QStringLiteral("Break the composition nesting cycle");
        }
        if (category == QStringLiteral("FrameRange")) {
            return QStringLiteral("Normalize the composition or layer frame range");
        }
        if (category == QStringLiteral("Naming")) {
            return QStringLiteral("Rename the item to a production-safe label");
        }
        if (category == QStringLiteral("Spelling")) {
            return QStringLiteral("Review the suggested spelling correction");
        }
        return QStringLiteral("Inspect the reported issue");
    };

    for (const auto& issue : report.issues) {
        ArtifactCore::DiagnosticSeverity severity = ArtifactCore::DiagnosticSeverity::Info;
        ArtifactCore::DiagnosticCategory category = ArtifactCore::DiagnosticCategory::Custom;

        switch (issue.severity) {
        case HealthIssueSeverity::Error:
            severity = ArtifactCore::DiagnosticSeverity::Error;
            break;
        case HealthIssueSeverity::Warning:
            severity = ArtifactCore::DiagnosticSeverity::Warning;
            break;
        case HealthIssueSeverity::Info:
        default:
            severity = ArtifactCore::DiagnosticSeverity::Info;
            break;
        }

        if (issue.category == QStringLiteral("CircularReference")) {
            category = ArtifactCore::DiagnosticCategory::CircularDep;
        } else if (issue.category == QStringLiteral("MissingAsset")) {
            category = ArtifactCore::DiagnosticCategory::File;
        } else if (issue.category == QStringLiteral("FrameRange")) {
            category = ArtifactCore::DiagnosticCategory::Configuration;
        } else if (issue.category == QStringLiteral("BrokenReference")) {
            category = ArtifactCore::DiagnosticCategory::Reference;
        } else if (issue.category == QStringLiteral("Naming")) {
            category = ArtifactCore::DiagnosticCategory::Configuration;
        } else if (issue.category == QStringLiteral("Spelling")) {
            category = ArtifactCore::DiagnosticCategory::Custom;
        }

        ArtifactCore::ProjectDiagnostic diag(
            severity,
            category,
            issue.message);
        diag.setDescription(issue.message);
        diag.setSourceCompId(issue.targetName);
        diag.setFixAction(fixActionForCategory(issue.category));
        diagnostics.push_back(diag);
    }

    return diagnostics;
}

bool severityMatchesFilter(ArtifactCore::DiagnosticSeverity severity, int index)
{
    switch (index) {
    case 1:
        return severity == ArtifactCore::DiagnosticSeverity::Error;
    case 2:
        return severity == ArtifactCore::DiagnosticSeverity::Warning;
    case 3:
        return severity == ArtifactCore::DiagnosticSeverity::Info;
    default:
        return true;
    }
}

bool categoryMatchesFilter(ArtifactCore::DiagnosticCategory category, int index)
{
    switch (index) {
    case 1:
        return category == ArtifactCore::DiagnosticCategory::Reference;
    case 2:
        return category == ArtifactCore::DiagnosticCategory::Matte;
    case 3:
        return category == ArtifactCore::DiagnosticCategory::CircularDep;
    case 4:
        return category == ArtifactCore::DiagnosticCategory::Expression;
    case 5:
        return category == ArtifactCore::DiagnosticCategory::Performance;
    case 6:
        return category == ArtifactCore::DiagnosticCategory::File;
    case 7:
        return category == ArtifactCore::DiagnosticCategory::Configuration;
    case 8:
        return category == ArtifactCore::DiagnosticCategory::Custom;
    default:
        return true;
    }
}

QString severityText(ArtifactCore::DiagnosticSeverity severity)
{
    switch (severity) {
    case ArtifactCore::DiagnosticSeverity::Error:
        return QStringLiteral("Error");
    case ArtifactCore::DiagnosticSeverity::Warning:
        return QStringLiteral("Warning");
    case ArtifactCore::DiagnosticSeverity::Info:
    default:
        return QStringLiteral("Info");
    }
}

QString categoryText(ArtifactCore::DiagnosticCategory category)
{
    switch (category) {
    case ArtifactCore::DiagnosticCategory::Reference:
        return QStringLiteral("Reference");
    case ArtifactCore::DiagnosticCategory::Matte:
        return QStringLiteral("Matte");
    case ArtifactCore::DiagnosticCategory::CircularDep:
        return QStringLiteral("Circular");
    case ArtifactCore::DiagnosticCategory::Expression:
        return QStringLiteral("Expression");
    case ArtifactCore::DiagnosticCategory::Performance:
        return QStringLiteral("Performance");
    case ArtifactCore::DiagnosticCategory::File:
        return QStringLiteral("File");
    case ArtifactCore::DiagnosticCategory::Configuration:
        return QStringLiteral("Configuration");
    case ArtifactCore::DiagnosticCategory::Custom:
    default:
        return QStringLiteral("Custom");
    }
}

QString sourceLabelForDiagnostic(const ArtifactCore::ProjectDiagnostic& diag)
{
    const QString layerId = diag.getSourceLayerId().trimmed();
    if (!layerId.isEmpty()) {
        return QStringLiteral("Layer: %1").arg(layerId);
    }

    const QString compId = diag.getSourceCompId().trimmed();
    if (!compId.isEmpty()) {
        return QStringLiteral("Composition: %1").arg(compId);
    }

    const QString message = diag.getMessage().trimmed();
    if (message.startsWith(QStringLiteral("Missing asset file:"), Qt::CaseInsensitive)) {
        return QStringLiteral("Asset");
    }

    return QStringLiteral("Project");
}

QString assetPathFromDiagnostic(const ArtifactCore::ProjectDiagnostic& diag)
{
    const QString message = diag.getMessage().trimmed();
    static const QRegularExpression missingAssetPattern(
        QStringLiteral(R"(Missing asset file:\s*(.+)$)"),
        QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch match = missingAssetPattern.match(message);
    if (!match.hasMatch()) {
        return {};
    }

    return match.captured(1).trimmed();
}

QString searchHaystackForDiagnostic(const ArtifactCore::ProjectDiagnostic& diag)
{
    return QStringList{
        diag.getMessage(),
        diag.getDescription(),
        diag.getFixAction(),
        diag.getSourceLayerId(),
        diag.getSourceCompId(),
        sourceLabelForDiagnostic(diag),
        categoryText(diag.getCategory())
    }.join(QStringLiteral("\n"));
}

bool diagnosticMatchesSearch(const ArtifactCore::ProjectDiagnostic& diag, const QString& filterText)
{
    if (filterText.trimmed().isEmpty()) {
        return true;
    }

    const QString needle = filterText.trimmed().toCaseFolded();
    const QString haystack = searchHaystackForDiagnostic(diag).toCaseFolded();
    return haystack.contains(needle);
}

ProblemGroupingMode groupingModeFromIndex(int index)
{
    switch (index) {
    case 1:
        return ProblemGroupingMode::Severity;
    case 2:
        return ProblemGroupingMode::Category;
    case 3:
        return ProblemGroupingMode::Source;
    default:
        return ProblemGroupingMode::Flat;
    }
}

QString groupingLabel(ProblemGroupingMode mode)
{
    switch (mode) {
    case ProblemGroupingMode::Severity:
        return QStringLiteral("Severity");
    case ProblemGroupingMode::Category:
        return QStringLiteral("Category");
    case ProblemGroupingMode::Source:
        return QStringLiteral("Source");
    case ProblemGroupingMode::Flat:
    default:
        return QStringLiteral("Flat");
    }
}

QString groupKeyForDiagnostic(const ArtifactCore::ProjectDiagnostic& diag, ProblemGroupingMode mode)
{
    switch (mode) {
    case ProblemGroupingMode::Severity:
        return severityText(diag.getSeverity());
    case ProblemGroupingMode::Category:
        return categoryText(diag.getCategory());
    case ProblemGroupingMode::Source:
        return sourceLabelForDiagnostic(diag);
    case ProblemGroupingMode::Flat:
    default:
        return {};
    }
}

QColor severityColor(ArtifactCore::DiagnosticSeverity severity)
{
    switch (severity) {
    case ArtifactCore::DiagnosticSeverity::Error:
        return QColor(QStringLiteral("#F44336"));
    case ArtifactCore::DiagnosticSeverity::Warning:
        return QColor(QStringLiteral("#FF9800"));
    case ArtifactCore::DiagnosticSeverity::Info:
    default:
        return QColor(QStringLiteral("#4FA8FF"));
    }
}

QIcon severityIcon(QWidget* widget, ArtifactCore::DiagnosticSeverity severity)
{
    if (!widget) {
        return {};
    }
    switch (severity) {
    case ArtifactCore::DiagnosticSeverity::Error:
        return widget->style()->standardIcon(QStyle::SP_MessageBoxCritical);
    case ArtifactCore::DiagnosticSeverity::Warning:
        return widget->style()->standardIcon(QStyle::SP_MessageBoxWarning);
    case ArtifactCore::DiagnosticSeverity::Info:
    default:
        return widget->style()->standardIcon(QStyle::SP_MessageBoxInformation);
    }
}

void populateDiagnosticItem(QTreeWidgetItem* item, const ArtifactCore::ProjectDiagnostic& diag)
{
    if (!item) {
        return;
    }

    const QString sourceText = sourceLabelForDiagnostic(diag);
    const QString assetPath = assetPathFromDiagnostic(diag);
    const QString displaySource = !assetPath.isEmpty()
        ? QStringLiteral("Asset: %1").arg(QFileInfo(assetPath).fileName().isEmpty() ? assetPath : QFileInfo(assetPath).fileName())
        : sourceText;

    item->setIcon(0, severityIcon(item->treeWidget() ? item->treeWidget()->window() : nullptr, diag.getSeverity()));
    item->setText(0, severityText(diag.getSeverity()));
    item->setText(1, categoryText(diag.getCategory()));
    item->setText(2, diag.getMessage());
    item->setText(3, displaySource);
    item->setForeground(0, QBrush(severityColor(diag.getSeverity())));
    item->setForeground(1, QBrush(severityColor(diag.getSeverity()).lighter(120)));
    item->setToolTip(0, QStringLiteral("Severity: %1").arg(severityText(diag.getSeverity())));
    item->setToolTip(1, QStringLiteral("Category: %1").arg(categoryText(diag.getCategory())));
    item->setToolTip(2, diag.getDescription().isEmpty() ? diag.getMessage() : diag.getDescription());
    item->setToolTip(3, assetPath.isEmpty()
                            ? (diag.getFixAction().isEmpty() ? sourceText : diag.getFixAction())
                            : assetPath);
    item->setData(0, Qt::UserRole, diag.getId());
    item->setData(0, Qt::UserRole + 1, diag.getSourceCompId());
    item->setData(0, Qt::UserRole + 2, diag.getSourceLayerId());
    item->setData(0, Qt::UserRole + 3, diag.getFixAction());
    item->setData(0, Qt::UserRole + 4, assetPath);
    item->setData(0, Qt::UserRole + 5, static_cast<int>(diag.getSeverity()));
    item->setData(0, Qt::UserRole + 6, static_cast<int>(diag.getCategory()));
}

QTreeWidgetItem* ensureGroupItem(QTreeWidget* tree,
                                 QMap<QString, QTreeWidgetItem*>& groupItems,
                                 const QString& key,
                                 const QString& label,
                                 const QColor& color)
{
    if (groupItems.contains(key)) {
        return groupItems.value(key);
    }

    auto* groupItem = new QTreeWidgetItem(tree);
    groupItem->setFirstColumnSpanned(true);
    groupItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    groupItem->setChildIndicatorPolicy(QTreeWidgetItem::ShowIndicator);
    groupItem->setText(0, label);
    groupItem->setForeground(0, QBrush(color));
    groupItems.insert(key, groupItem);
    return groupItem;
}
} // namespace

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

    impl_->searchFilter = new QLineEdit(this);
    impl_->searchFilter->setPlaceholderText(QStringLiteral("Search diagnostics"));
    impl_->searchFilter->installEventFilter(this);
    headerLayout->addWidget(impl_->searchFilter, 2);

    impl_->severityFilter = new QComboBox(this);
    impl_->severityFilter->addItems({"All Severities", "Errors Only", "Warnings Only", "Info Only"});
    impl_->severityFilter->installEventFilter(this);
    headerLayout->addWidget(impl_->severityFilter, 1);

    impl_->categoryFilter = new QComboBox(this);
    impl_->categoryFilter->addItems({
        "All Categories",
        "References",
        "Mattes",
        "Circular Deps",
        "Expressions",
        "Performance",
        "Files",
        "Config",
        "Custom"
    });
    impl_->categoryFilter->installEventFilter(this);
    headerLayout->addWidget(impl_->categoryFilter, 1);

    impl_->groupingFilter = new QComboBox(this);
    impl_->groupingFilter->addItems({"Flat", "By Severity", "By Category", "By Source"});
    impl_->groupingFilter->installEventFilter(this);
    headerLayout->addWidget(impl_->groupingFilter, 1);

    root->addLayout(headerLayout);

    // Action buttons
    auto* actionLayout = new QHBoxLayout();
    actionLayout->setSpacing(8);

    impl_->refreshButton = new QPushButton("Refresh", this);
    impl_->clearButton = new QPushButton("Clear", this);
    impl_->refreshButton->installEventFilter(this);
    impl_->clearButton->installEventFilter(this);

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
    impl_->problemTree->setSortingEnabled(false);
    QObject::connect(impl_->problemTree, &QTreeWidget::itemDoubleClicked,
                     this, &ArtifactProblemViewWidget::onNavigateToProblem);

    root->addWidget(impl_->problemTree, 1);

    // Summary
    impl_->summaryLabel = new QLabel("goal: inspect project issues | now: 0 | warning: none | next: refresh", this);
    {
        QPalette summaryPal = impl_->summaryLabel->palette();
        summaryPal.setColor(QPalette::WindowText, QColor(ArtifactCore::currentDCCTheme().textColor).darker(130));
        summaryPal.setColor(QPalette::Text, QColor(ArtifactCore::currentDCCTheme().textColor).darker(130));
    impl_->summaryLabel->setPalette(summaryPal);
    }
    root->addWidget(impl_->summaryLabel);

    impl_->eventBusSubscriptions_.push_back(
        impl_->eventBus_.subscribe<ProjectChangedEvent>([this](const ProjectChangedEvent&) {
            if (!impl_) {
                return;
            }
            QTimer::singleShot(0, this, [this]() {
                if (impl_) {
                    refreshFromCurrentProject();
                }
            });
        }));
    impl_->eventBusSubscriptions_.push_back(
        impl_->eventBus_.subscribe<CurrentCompositionChangedEvent>([this](const CurrentCompositionChangedEvent&) {
            if (!impl_) {
                return;
            }
            QTimer::singleShot(0, this, [this]() {
                if (impl_) {
                    refreshFromCurrentProject();
                }
            });
        }));
    impl_->eventBusSubscriptions_.push_back(
        impl_->eventBus_.subscribe<LayerChangedEvent>([this](const LayerChangedEvent&) {
            if (!impl_) {
                return;
            }
            QTimer::singleShot(0, this, [this]() {
                if (impl_) {
                    refreshFromCurrentProject();
                }
            });
        }));
}

ArtifactProblemViewWidget::~ArtifactProblemViewWidget()
{
    delete impl_;
}

void ArtifactProblemViewWidget::setProject(ArtifactProject* project)
{
    if (!impl_) {
        return;
    }
    impl_->project = project;
}

void ArtifactProblemViewWidget::refreshFromCurrentProject()
{
    if (!impl_) {
        return;
    }

    ArtifactProject* project = impl_->project;
    if (!project) {
        loadDiagnostics(std::vector<ArtifactCore::ProjectDiagnostic>{});
        return;
    }

    if (auto* projectService = ArtifactProjectService::instance()) {
        loadDiagnostics(projectService->currentProjectDiagnostics());
        return;
    }

    // Fallback when service not available
    const auto healthReport = ArtifactProjectHealthChecker::check(project);
    const auto diagnostics = convertHealthReportToDiagnostics(healthReport);
    loadDiagnostics(diagnostics);
}

void ArtifactProblemViewWidget::loadDiagnostics(const std::vector<ArtifactCore::ProjectDiagnostic>& diagnostics)
{
    if (!impl_) {
        return;
    }

    impl_->diagnostics = diagnostics;
    rebuildProblemTree();
}

void ArtifactProblemViewWidget::rebuildProblemTree()
{
    if (!impl_ || !impl_->problemTree) {
        return;
    }

    impl_->problemTree->setUpdatesEnabled(false);
    impl_->problemTree->clear();
    impl_->problemTree->setSortingEnabled(false);

    const QString searchText = impl_->searchFilter ? impl_->searchFilter->text().trimmed() : QString();
    const auto groupingMode = groupingModeFromIndex(impl_->groupingFilter ? impl_->groupingFilter->currentIndex() : 0);
    std::vector<ArtifactCore::ProjectDiagnostic> visibleDiagnostics;
    visibleDiagnostics.reserve(impl_->diagnostics.size());
    QMap<QString, QTreeWidgetItem*> groupItems;
    QMap<QString, int> groupCounts;

    for (const auto& diag : impl_->diagnostics) {
        const int severityIndex = impl_->severityFilter ? impl_->severityFilter->currentIndex() : 0;
        const int categoryIndex = impl_->categoryFilter ? impl_->categoryFilter->currentIndex() : 0;
        if (!severityMatchesFilter(diag.getSeverity(), severityIndex)) {
            continue;
        }
        if (!categoryMatchesFilter(diag.getCategory(), categoryIndex)) {
            continue;
        }
        if (!diagnosticMatchesSearch(diag, searchText)) {
            continue;
        }

        QTreeWidgetItem* parentItem = nullptr;
        const QString groupKey = groupKeyForDiagnostic(diag, groupingMode);
        if (groupingMode != ProblemGroupingMode::Flat) {
            const QString label = groupKey.isEmpty() ? QStringLiteral("Uncategorized") : groupKey;
            parentItem = ensureGroupItem(
                impl_->problemTree,
                groupItems,
                label,
                label,
                severityColor(diag.getSeverity()).darker(120));
            groupCounts[label] += 1;
        }

        auto* item = parentItem ? new QTreeWidgetItem(parentItem) : new QTreeWidgetItem(impl_->problemTree);
        populateDiagnosticItem(item, diag);
        visibleDiagnostics.push_back(diag);
    }

    if (groupingMode != ProblemGroupingMode::Flat) {
        for (auto it = groupItems.begin(); it != groupItems.end(); ++it) {
            const QString key = it.key();
            auto* groupItem = it.value();
            const int count = groupCounts.value(key, 0);
            groupItem->setText(0, QStringLiteral("%1 (%2)").arg(key).arg(count));
            groupItem->setToolTip(0, QStringLiteral("%1 diagnostics grouped by %2").arg(count).arg(groupingLabel(groupingMode)));
            groupItem->setExpanded(true);
        }
    }

    updateSummary(visibleDiagnostics);
    impl_->problemTree->setUpdatesEnabled(true);
    impl_->problemTree->viewport()->update();
}

void ArtifactProblemViewWidget::onRefresh()
{
    refreshFromCurrentProject();
}

void ArtifactProblemViewWidget::onNavigateToProblem(QTreeWidgetItem* item, int column)
{
    Q_UNUSED(column);
    if (!item || !impl_) {
        return;
    }

    if (item->childCount() > 0) {
        item->setExpanded(!item->isExpanded());
        return;
    }

    const QString compIdText = item->data(0, Qt::UserRole + 1).toString();
    const QString layerIdText = item->data(0, Qt::UserRole + 2).toString();
    const QString assetPath = item->data(0, Qt::UserRole + 4).toString();
    const QString message = item->text(2);

    if (compIdText.isEmpty() && layerIdText.isEmpty() && assetPath.isEmpty()) {
        if (!message.isEmpty()) {
            if (auto* clipboard = QApplication::clipboard()) {
                clipboard->setText(message);
            }
        }
        return;
    }

    auto* projectService = ArtifactProjectService::instance();
    if (!compIdText.isEmpty() && projectService) {
        projectService->changeCurrentComposition(CompositionID(compIdText));
    }

    if (!layerIdText.isEmpty() && projectService) {
        projectService->selectLayer(LayerID(layerIdText));
    }

    if (!assetPath.isEmpty()) {
        const QFileInfo assetInfo(assetPath);
        if (assetInfo.exists()) {
            QDesktopServices::openUrl(QUrl::fromLocalFile(assetInfo.absoluteFilePath()));
        } else if (auto* clipboard = QApplication::clipboard()) {
            clipboard->setText(assetPath);
        }
    }
}

bool ArtifactProblemViewWidget::eventFilter(QObject* watched, QEvent* event)
{
    if (!impl_ || !watched || !event) {
        return QWidget::eventFilter(watched, event);
    }

    const bool isFilterWidget =
        watched == impl_->searchFilter ||
        watched == impl_->severityFilter ||
        watched == impl_->categoryFilter ||
        watched == impl_->groupingFilter ||
        watched == impl_->refreshButton ||
        watched == impl_->clearButton;
    if (!isFilterWidget) {
        return QWidget::eventFilter(watched, event);
    }

    switch (event->type()) {
    case QEvent::MouseButtonRelease:
    case QEvent::KeyRelease:
    case QEvent::Wheel:
    case QEvent::Hide:
        if (watched == impl_->refreshButton) {
            onRefresh();
            return true;
        }
        if (watched == impl_->clearButton) {
            loadDiagnostics(std::vector<ArtifactCore::ProjectDiagnostic>{});
            return true;
        }
        if (watched == impl_->searchFilter || watched == impl_->groupingFilter) {
            QTimer::singleShot(0, this, [this]() {
                rebuildProblemTree();
            });
            break;
        }
        QTimer::singleShot(0, this, [this]() {
            rebuildProblemTree();
        });
        break;
    default:
        break;
    }

    return QWidget::eventFilter(watched, event);
}

void ArtifactProblemViewWidget::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    refreshFromCurrentProject();
}

void ArtifactProblemViewWidget::updateSummary(const std::vector<ArtifactCore::ProjectDiagnostic>& diagnostics)
{
    if (!impl_ || !impl_->summaryLabel) {
        return;
    }

    int errors = 0;
    int warnings = 0;
    int infos = 0;
    for (const auto& diag : diagnostics) {
        if (diag.isError()) {
            ++errors;
        } else if (diag.isWarning()) {
            ++warnings;
        } else {
            ++infos;
        }
    }

    impl_->summaryLabel->setText(
        projectHealthSummaryText(static_cast<int>(diagnostics.size()), errors, warnings, infos, true));
}

} // namespace Artifact
