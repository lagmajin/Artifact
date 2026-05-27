module;
#include <utility>
#include <QWidget>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QIcon>
#include <QBrush>
#include <QColor>
#include <QFont>
#include <QPixmap>
#include <QPainter>
#include <QPalette>
#include <QStringList>
#include <QRegularExpression>
#include <QtSVG/QSvgRenderer>
#include <QHeaderView>
#include <QStyle>
#include <QMessageBox>
#include <wobjectdefs.h>
#include <wobjectimpl.h>

export module Artifact.Widgets.ProjectHealthDashboard;
import std;




import Artifact.Project;
import Artifact.Project.Health;
import Artifact.Service.Project;
import Core.Diagnostics.ProjectDiagnostic;
import Widgets.Utils.CSS;
import Utils.Path;
import Utils.String.UniString;

namespace Artifact {

namespace {
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
    static const QRegularExpression missingAssetPattern(
        QStringLiteral(R"(Missing asset file:\s*(.+)$)"),
        QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch match = missingAssetPattern.match(diag.getMessage().trimmed());
    if (!match.hasMatch()) {
        return {};
    }
    return match.captured(1).trimmed();
}

std::vector<ArtifactCore::ProjectDiagnostic> diagnosticsFromHealthReport(const ProjectHealthReport& report)
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
        } else if (issue.category == QStringLiteral("Naming")) {
            category = ArtifactCore::DiagnosticCategory::Configuration;
        }

        ArtifactCore::ProjectDiagnostic diag(severity, category, issue.message);
        diag.setDescription(issue.message);
        diag.setSourceCompId(issue.targetName);
        diagnostics.push_back(diag);
    }

    return diagnostics;
}
} // namespace

/**
 * @brief Artifact Project Health Dashboard
 * 
 * A standalone widget to visualize project health issues detected by ArtifactProjectHealthChecker.
 */
export class ArtifactProjectHealthDashboard : public QWidget {
    W_OBJECT(ArtifactProjectHealthDashboard)

public:
    explicit ArtifactProjectHealthDashboard(ArtifactProject* project = nullptr, QWidget* parent = nullptr)
        : QWidget(parent), project_(project) 
    {
        setupUI();
        if (project_) {
            refresh();
        }
    }

    void setProject(ArtifactProject* project) {
        project_ = project;
        refresh();
    }

    /**
     * @brief Run the health check and update the UI
     */
    void refresh() {
        if (!project_) {
            statusLabel_->setText("Open a project to inspect health.");
            applyStatusColor(QColor(ArtifactCore::currentDCCTheme().textColor).darker(130));
            issuesTree_->clear();
            lastReport_ = {};
            return;
        }

        issuesTree_->clear();
        std::vector<ArtifactCore::ProjectDiagnostic> diagnostics;
        if (ArtifactProjectService::instance()) {
            diagnostics = ArtifactProjectService::instance()->currentProjectDiagnostics();
            lastReport_ = ArtifactProjectService::instance()->currentProjectHealthReport();
        } else {
            lastReport_ = ArtifactProjectHealthChecker::check(project_);
            diagnostics = diagnosticsFromHealthReport(lastReport_);
        }

        // Update overall status
        if (lastReport_.isHealthy) {
            if (lastReport_.issues.isEmpty()) {
                statusLabel_->setText("Project is Healthy");
                applyStatusColor(QColor(QStringLiteral("#4CAF50")));
            } else {
                statusLabel_->setText("Project has Minor Warnings");
                applyStatusColor(QColor(QStringLiteral("#FF9800")));
            }
        } else {
            statusLabel_->setText("Project has Critical Issues");
            applyStatusColor(QColor(QStringLiteral("#F44336")));
        }

        // Add issues to tree
        for (const auto& diagnostic : diagnostics) {
            auto item = new QTreeWidgetItem(issuesTree_);
            
            // Set Icon based on severity
            QIcon icon;
            const auto severity = diagnostic.getSeverity();
            if (severity == ArtifactCore::DiagnosticSeverity::Error) {
                icon = loadHealthSeverityIcon(HealthIssueSeverity::Error);
                item->setForeground(0, QBrush(QColor(QStringLiteral("#F44336"))));
            } else if (severity == ArtifactCore::DiagnosticSeverity::Warning) {
                icon = loadHealthSeverityIcon(HealthIssueSeverity::Warning);
                item->setForeground(0, QBrush(QColor(QStringLiteral("#FF9800"))));
            } else {
                icon = loadHealthSeverityIcon(HealthIssueSeverity::Info);
                item->setForeground(0, QBrush(QColor(QStringLiteral("#4FA8FF"))));
            }
            item->setIcon(0, icon);
            
            item->setText(1, diagnostic.getMessage());
            item->setText(2, sourceLabelForDiagnostic(diagnostic));
            item->setText(3, categoryText(diagnostic.getCategory()));
            item->setData(0, Qt::UserRole, diagnostic.getId());
            item->setData(0, Qt::UserRole + 1, diagnostic.getSourceCompId());
            item->setData(0, Qt::UserRole + 2, diagnostic.getSourceLayerId());
            item->setData(0, Qt::UserRole + 3, assetPathFromDiagnostic(diagnostic));
            
            // Add tooltip for full message
            item->setToolTip(1, diagnostic.getDescription().isEmpty()
                                    ? diagnostic.getMessage()
                                    : diagnostic.getDescription());
            item->setToolTip(2, item->data(0, Qt::UserRole + 3).toString().isEmpty()
                                    ? QStringLiteral("Double-click to inspect")
                                    : item->data(0, Qt::UserRole + 3).toString());
        }

        issuesTree_->header()->setSectionResizeMode(1, QHeaderView::Stretch);
        issuesTree_->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
        issuesTree_->header()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
        if (fixBtn_) {
            fixBtn_->setEnabled(!lastReport_.isHealthy || !lastReport_.issues.isEmpty());
        }
    }

private:
    QIcon loadHealthSeverityIcon(HealthIssueSeverity severity) const {
        QString relativePath;
        QStyle::StandardPixmap fallback = QStyle::SP_MessageBoxInformation;
        switch (severity) {
            case HealthIssueSeverity::Error:
                relativePath = QStringLiteral("MaterialVS/red/error.svg");
                fallback = QStyle::SP_MessageBoxCritical;
                break;
            case HealthIssueSeverity::Warning:
                relativePath = QStringLiteral("MaterialVS/yellow/warning.svg");
                fallback = QStyle::SP_MessageBoxWarning;
                break;
            case HealthIssueSeverity::Info:
            default:
                relativePath = QStringLiteral("MaterialVS/blue/info.svg");
                fallback = QStyle::SP_MessageBoxInformation;
                break;
        }

        auto tryLoadSvgIcon = [](const QString& path) -> QIcon {
            if (path.isEmpty()) return QIcon();
            if (path.endsWith(QStringLiteral(".svg"), Qt::CaseInsensitive)) {
                QSvgRenderer renderer(path);
                if (renderer.isValid()) {
                    QPixmap pixmap(16, 16);
                    pixmap.fill(Qt::transparent);
                    QPainter painter(&pixmap);
                    renderer.render(&painter);
                    painter.end();
                    if (!pixmap.isNull()) return QIcon(pixmap);
                }
                return QIcon();
            }
            return QIcon(path);
        };

        QIcon icon = tryLoadSvgIcon(ArtifactCore::resolveIconResourcePath(relativePath));
        if (!icon.isNull()) {
            return icon;
        }

        icon = tryLoadSvgIcon(ArtifactCore::resolveIconPath(relativePath));
        if (!icon.isNull()) {
            return icon;
        }

        return style()->standardIcon(fallback);
    }

    void setupUI() {
        const QColor background = QColor(ArtifactCore::currentDCCTheme().backgroundColor);
        const QColor surface = QColor(ArtifactCore::currentDCCTheme().secondaryBackgroundColor);
        const QColor text = QColor(ArtifactCore::currentDCCTheme().textColor);
        const QColor muted = text.darker(130);
        const QColor accent = QColor(ArtifactCore::currentDCCTheme().accentColor);

        auto mainLayout = new QVBoxLayout(this);
        mainLayout->setContentsMargins(15, 15, 15, 15);
        mainLayout->setSpacing(12);

        setAutoFillBackground(true);
        QPalette widgetPalette = palette();
        widgetPalette.setColor(QPalette::Window, background);
        widgetPalette.setColor(QPalette::WindowText, text);
        setPalette(widgetPalette);

        // Header Panel
        auto headerLayout = new QHBoxLayout();
        statusLabel_ = new QLabel("Loading...");
        {
            QFont f = statusLabel_->font();
            f.setBold(true);
            f.setPointSize(14);
            statusLabel_->setFont(f);
            QPalette pal = statusLabel_->palette();
            pal.setColor(QPalette::WindowText, muted);
            statusLabel_->setPalette(pal);
        }
        
        auto refreshBtn = new QPushButton("Scan Project");
        refreshBtn->setFixedWidth(120);
        {
            QPalette pal = refreshBtn->palette();
            pal.setColor(QPalette::Button, surface);
            pal.setColor(QPalette::ButtonText, text);
            refreshBtn->setPalette(pal);
        }
        connect(refreshBtn, &QPushButton::clicked, this, &ArtifactProjectHealthDashboard::refresh);

        headerLayout->addWidget(statusLabel_);
        headerLayout->addStretch();
        headerLayout->addWidget(refreshBtn);
        mainLayout->addLayout(headerLayout);

        // Issues Tree
        issuesTree_ = new QTreeWidget();
        QStringList headers = {"", "Issue Description", "Source", "Category"};
        issuesTree_->setHeaderLabels(headers);
        issuesTree_->setColumnWidth(0, 30);
        issuesTree_->setColumnWidth(2, 120);
        issuesTree_->setColumnWidth(3, 100);
        issuesTree_->setAlternatingRowColors(true);
        issuesTree_->setIndentation(0);
        {
            QPalette pal = issuesTree_->palette();
            pal.setColor(QPalette::Window, background);
            pal.setColor(QPalette::Base, surface);
            pal.setColor(QPalette::AlternateBase, background.darker(110));
            pal.setColor(QPalette::Text, text);
            pal.setColor(QPalette::WindowText, text);
            pal.setColor(QPalette::Highlight, accent);
            pal.setColor(QPalette::HighlightedText, QColor(QStringLiteral("#FFFFFF")));
            pal.setColor(QPalette::Button, surface);
            pal.setColor(QPalette::ButtonText, text);
            issuesTree_->setPalette(pal);
        }
        mainLayout->addWidget(issuesTree_);

        // Footer Actions
        auto footerLayout = new QHBoxLayout();
        fixBtn_ = new QPushButton("Auto Repair");
        fixBtn_->setEnabled(false);
        {
            QPalette pal = fixBtn_->palette();
            pal.setColor(QPalette::Button, accent);
            pal.setColor(QPalette::ButtonText, QColor(QStringLiteral("#FFFFFF")));
            fixBtn_->setPalette(pal);
        }
        connect(fixBtn_, &QPushButton::clicked, this, [this]() {
            if (!project_) return;
            AutoRepairOptions options;
            options.repairFrameRanges = true;
            options.normalizeCompositionRanges = true;
            options.removeMissingAssets = true;

            AutoRepairResult repaired = ArtifactProjectHealthChecker::checkAndRepair(project_, options);
            QMessageBox::information(this,
                                     "Project Auto Repair",
                                     QString("Fixed: %1\nSkipped: %2").arg(repaired.fixedCount).arg(repaired.skippedCount));
            refresh();
        });

        footerLayout->addStretch();
        footerLayout->addWidget(fixBtn_);
        mainLayout->addLayout(footerLayout);
    }

    void applyStatusColor(const QColor& color) {
        if (!statusLabel_) {
            return;
        }
        QFont f = statusLabel_->font();
        f.setBold(true);
        f.setPointSize(14);
        statusLabel_->setFont(f);
        QPalette pal = statusLabel_->palette();
        pal.setColor(QPalette::WindowText, color);
        statusLabel_->setPalette(pal);
    }

    ArtifactProject* project_ = nullptr;
    QTreeWidget* issuesTree_ = nullptr;
    QLabel* statusLabel_ = nullptr;
    QPushButton* fixBtn_ = nullptr;
    ProjectHealthReport lastReport_;
};

W_OBJECT_IMPL(ArtifactProjectHealthDashboard)

} // namespace Artifact

