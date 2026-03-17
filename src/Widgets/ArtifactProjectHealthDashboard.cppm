module;
#include <QWidget>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QIcon>
#include <QPixmap>
#include <QPainter>
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
import Utils.Path;
import Utils.String.UniString;

namespace Artifact {

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
            statusLabel_->setText("No project loaded.");
            statusLabel_->setStyleSheet("color: #888; font-weight: bold;");
            issuesTree_->clear();
            return;
        }

        issuesTree_->clear();
        ArtifactProjectHealthChecker checker;
        ProjectHealthReport report = checker.check(project_);
        lastReport_ = report;

        // Update overall status
        if (report.isHealthy) {
            if (report.issues.isEmpty()) {
                statusLabel_->setText("Project is Healthy");
                statusLabel_->setStyleSheet("color: #4CAF50; font-weight: bold; font-size: 14px;");
            } else {
                statusLabel_->setText("Project has Minor Warnings");
                statusLabel_->setStyleSheet("color: #FF9800; font-weight: bold; font-size: 14px;");
            }
        } else {
            statusLabel_->setText("Project has Critical Issues");
            statusLabel_->setStyleSheet("color: #F44336; font-weight: bold; font-size: 14px;");
        }

        // Add issues to tree
        for (const auto& issue : report.issues) {
            auto item = new QTreeWidgetItem(issuesTree_);
            
            // Set Icon based on severity
            QIcon icon;
            if (issue.severity == HealthIssueSeverity::Error) {
                icon = loadHealthSeverityIcon(issue.severity);
                item->setForeground(0, Qt::red);
            } else if (issue.severity == HealthIssueSeverity::Warning) {
                icon = loadHealthSeverityIcon(issue.severity);
                item->setForeground(0, QColor(255, 165, 0)); // Orange
            } else {
                icon = loadHealthSeverityIcon(issue.severity);
                item->setForeground(0, QColor(79, 193, 255)); // Blue
            }
            item->setIcon(0, icon);
            
            item->setText(1, issue.message);
            item->setText(2, issue.targetName);
            item->setText(3, issue.category);
            
            // Add tooltip for full message
            item->setToolTip(1, issue.message);
        }

        issuesTree_->header()->setSectionResizeMode(1, QHeaderView::Stretch);
        if (fixBtn_) {
            fixBtn_->setEnabled(!report.isHealthy || !report.issues.isEmpty());
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
        auto mainLayout = new QVBoxLayout(this);
        mainLayout->setContentsMargins(15, 15, 15, 15);
        mainLayout->setSpacing(12);

        // Header Panel
        auto headerLayout = new QHBoxLayout();
        statusLabel_ = new QLabel("Loading...");
        statusLabel_->setStyleSheet("font-weight: bold; font-size: 14px;");
        
        auto refreshBtn = new QPushButton("Scan Project");
        refreshBtn->setFixedWidth(120);
        refreshBtn->setStyleSheet(R"(
            QPushButton {
                background-color: #3e3e42;
                color: #eee;
                border: 1px solid #555;
                padding: 6px 12px;
                border-radius: 4px;
            }
            QPushButton:hover {
                background-color: #4e4e52;
                border: 1px solid #777;
            }
        )");
        connect(refreshBtn, &QPushButton::clicked, this, &ArtifactProjectHealthDashboard::refresh);

        headerLayout->addWidget(statusLabel_);
        headerLayout->addStretch();
        headerLayout->addWidget(refreshBtn);
        mainLayout->addLayout(headerLayout);

        // Issues Tree
        issuesTree_ = new QTreeWidget();
        QStringList headers = {"", "Issue Description", "Target Object", "Category"};
        issuesTree_->setHeaderLabels(headers);
        issuesTree_->setColumnWidth(0, 30);
        issuesTree_->setColumnWidth(2, 120);
        issuesTree_->setColumnWidth(3, 100);
        issuesTree_->setAlternatingRowColors(true);
        issuesTree_->setIndentation(0);
        issuesTree_->setStyleSheet(R"(
            QTreeWidget {
                background-color: #1e1e1e;
                color: #ccc;
                border: 1px solid #333;
                selection-background-color: #3f3f46;
                alternate-background-color: #252526;
            }
            QHeaderView::section {
                background-color: #2d2d2d;
                color: #aaa;
                padding: 4px;
                border: 1px solid #1a1a1a;
            }
        )");
        mainLayout->addWidget(issuesTree_);

        // Footer Actions
        auto footerLayout = new QHBoxLayout();
        fixBtn_ = new QPushButton("Auto Repair");
        fixBtn_->setEnabled(false);
        fixBtn_->setStyleSheet(R"(
            QPushButton {
                background-color: #007acc;
                color: white;
                border: none;
                padding: 8px 16px;
                border-radius: 4px;
            }
            QPushButton:disabled {
                background-color: #444;
                color: #888;
            }
        )");
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

        // Set modern dark theme appearance
        setStyleSheet("background-color: #1e1e1e; color: #ccc;");
    }

    ArtifactProject* project_ = nullptr;
    QTreeWidget* issuesTree_ = nullptr;
    QLabel* statusLabel_ = nullptr;
    QPushButton* fixBtn_ = nullptr;
    ProjectHealthReport lastReport_;
};

W_OBJECT_IMPL(ArtifactProjectHealthDashboard)

} // namespace Artifact

