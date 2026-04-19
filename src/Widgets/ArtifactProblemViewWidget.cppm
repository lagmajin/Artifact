module;
#include <wobjectimpl.h>
#include <vector>
#include <QWidget>
#include <QFrame>
#include <QLabel>
#include <QApplication>
#include <QBrush>
#include <QClipboard>
#include <QTreeWidget>
#include <QComboBox>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QColor>
#include <QFont>
#include <QPalette>
#include <QStyle>
#include <QTreeWidgetItemIterator>
#include <QEvent>
#include <QTimer>
module Artifact.Widgets.ProblemViewWidget;

import Artifact.Widgets.ProblemViewWidget;
import Widgets.Utils.CSS;
import Artifact.Project;
import Artifact.Project.Health;
import Artifact.Service.Project;
import Utils.Id;

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
    std::vector<ArtifactCore::ProjectDiagnostic> diagnostics;
    ArtifactProject* project = nullptr;
};

namespace {
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

    impl_->severityFilter = new QComboBox(this);
    impl_->severityFilter->addItems({"All Severities", "Errors Only", "Warnings Only", "Info Only"});
    impl_->severityFilter->installEventFilter(this);
    headerLayout->addWidget(impl_->severityFilter, 1);

    impl_->categoryFilter = new QComboBox(this);
    impl_->categoryFilter->addItems({"All Categories", "References", "Mattes", "Circular Deps", "Expressions", "Performance"});
    impl_->categoryFilter->installEventFilter(this);
    headerLayout->addWidget(impl_->categoryFilter, 1);

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
    impl_->problemTree->setSortingEnabled(true);
    QObject::connect(impl_->problemTree, &QTreeWidget::itemDoubleClicked,
                     this, &ArtifactProblemViewWidget::onNavigateToProblem);

    root->addWidget(impl_->problemTree, 1);

    // Summary
    impl_->summaryLabel = new QLabel("0 Errors, 0 Warnings, 0 Info", this);
    {
        QPalette summaryPal = impl_->summaryLabel->palette();
        summaryPal.setColor(QPalette::WindowText, QColor(ArtifactCore::currentDCCTheme().textColor).darker(130));
        summaryPal.setColor(QPalette::Text, QColor(ArtifactCore::currentDCCTheme().textColor).darker(130));
        impl_->summaryLabel->setPalette(summaryPal);
    }
    root->addWidget(impl_->summaryLabel);
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

    std::vector<ArtifactCore::ProjectDiagnostic> diagnostics;
    const auto issues = project->validate();
    diagnostics.reserve(issues.size());

    for (const auto& issue : issues) {
        ArtifactCore::DiagnosticSeverity severity = ArtifactCore::DiagnosticSeverity::Info;
        ArtifactCore::DiagnosticCategory category = ArtifactCore::DiagnosticCategory::Custom;

        switch (issue.severity) {
        case ProjectValidationIssue::Severity::Error:
            severity = ArtifactCore::DiagnosticSeverity::Error;
            break;
        case ProjectValidationIssue::Severity::Warning:
            severity = ArtifactCore::DiagnosticSeverity::Warning;
            break;
        case ProjectValidationIssue::Severity::Info:
        default:
            severity = ArtifactCore::DiagnosticSeverity::Info;
            break;
        }

        if (issue.field.startsWith(QStringLiteral("composition."))) {
            category = ArtifactCore::DiagnosticCategory::Configuration;
        } else if (issue.field.startsWith(QStringLiteral("layer."))) {
            category = ArtifactCore::DiagnosticCategory::Reference;
        } else if (issue.field.startsWith(QStringLiteral("footage."))) {
            category = ArtifactCore::DiagnosticCategory::File;
        }

        ArtifactCore::ProjectDiagnostic diag(severity, category, issue.message);
        diag.setDescription(issue.suggestion);
        diag.setSourceCompId(issue.field);
        diagnostics.push_back(diag);
    }

    loadDiagnostics(diagnostics);
}

void ArtifactProblemViewWidget::loadProjectHealth(const ProjectHealthReport& report)
{
    std::vector<ArtifactCore::ProjectDiagnostic> diagnostics;
    diagnostics.reserve(static_cast<size_t>(report.issues.size()));

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
        }

        ArtifactCore::ProjectDiagnostic diag(
            severity,
            category,
            issue.message);
        diag.setDescription(issue.message);
        diag.setSourceCompId(issue.targetName);
        diagnostics.push_back(diag);
    }

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

    impl_->problemTree->clear();
    std::vector<ArtifactCore::ProjectDiagnostic> visibleDiagnostics;
    visibleDiagnostics.reserve(impl_->diagnostics.size());

    for (const auto& diag : impl_->diagnostics) {
        const int severityIndex = impl_->severityFilter ? impl_->severityFilter->currentIndex() : 0;
        const int categoryIndex = impl_->categoryFilter ? impl_->categoryFilter->currentIndex() : 0;
        if (!severityMatchesFilter(diag.getSeverity(), severityIndex)) {
            continue;
        }
        if (!categoryMatchesFilter(diag.getCategory(), categoryIndex)) {
            continue;
        }

        auto* item = new QTreeWidgetItem(impl_->problemTree);
        item->setIcon(0, severityIcon(this, diag.getSeverity()));
        item->setText(0, severityText(diag.getSeverity()));
        item->setText(1, categoryText(diag.getCategory()));
        item->setText(2, diag.getMessage());
        item->setText(3, diag.getSourceLayerId().isEmpty()
                              ? diag.getSourceCompId()
                              : diag.getSourceLayerId());
        item->setForeground(0, QBrush(severityColor(diag.getSeverity())));
        item->setForeground(1, QBrush(severityColor(diag.getSeverity()).lighter(120)));
        item->setToolTip(2, diag.getDescription().isEmpty()
                                ? diag.getMessage()
                                : diag.getDescription());
        item->setToolTip(3, diag.getFixAction().isEmpty()
                                ? QStringLiteral("Double-click to navigate")
                                : diag.getFixAction());
        item->setData(0, Qt::UserRole, diag.getId());
        item->setData(0, Qt::UserRole + 1, diag.getSourceCompId());
        item->setData(0, Qt::UserRole + 2, diag.getSourceLayerId());
        item->setData(0, Qt::UserRole + 3, diag.getFixAction());
        visibleDiagnostics.push_back(diag);
    }

    updateSummary(visibleDiagnostics);
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

    const QString compIdText = item->data(0, Qt::UserRole + 1).toString();
    const QString layerIdText = item->data(0, Qt::UserRole + 2).toString();
    const QString message = item->text(2);

    if (compIdText.isEmpty() && layerIdText.isEmpty()) {
        if (!message.isEmpty()) {
            if (auto* clipboard = QApplication::clipboard()) {
                clipboard->setText(message);
            }
        }
        return;
    }

    auto* projectService = ArtifactProjectService::instance();
    if (!projectService) {
        return;
    }

    if (!compIdText.isEmpty()) {
        projectService->changeCurrentComposition(CompositionID(compIdText));
    }

    if (!layerIdText.isEmpty()) {
        projectService->selectLayer(LayerID(layerIdText));
    }
}

bool ArtifactProblemViewWidget::eventFilter(QObject* watched, QEvent* event)
{
    if (!impl_ || !watched || !event) {
        return QWidget::eventFilter(watched, event);
    }

    const bool isFilterWidget =
        watched == impl_->severityFilter ||
        watched == impl_->categoryFilter ||
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
        QTimer::singleShot(0, this, [this]() {
            rebuildProblemTree();
        });
        break;
    default:
        break;
    }

    return QWidget::eventFilter(watched, event);
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
        QStringLiteral("%1 Errors, %2 Warnings, %3 Info")
            .arg(errors)
            .arg(warnings)
            .arg(infos));
}

} // namespace Artifact
