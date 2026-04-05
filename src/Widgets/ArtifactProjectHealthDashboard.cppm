module;
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
import Widgets.Utils.CSS;
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
            applyStatusColor(QColor(ArtifactCore::currentDCCTheme().textColor).darker(130));
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
        for (const auto& issue : report.issues) {
            auto item = new QTreeWidgetItem(issuesTree_);
            
            // Set Icon based on severity
            QIcon icon;
            if (issue.severity == HealthIssueSeverity::Error) {
                icon = loadHealthSeverityIcon(issue.severity);
                item->setForeground(0, QBrush(QColor(QStringLiteral("#F44336"))));
            } else if (issue.severity == HealthIssueSeverity::Warning) {
                icon = loadHealthSeverityIcon(issue.severity);
                item->setForeground(0, QBrush(QColor(QStringLiteral("#FF9800"))));
            } else {
                icon = loadHealthSeverityIcon(issue.severity);
                item->setForeground(0, QBrush(QColor(QStringLiteral("#4FA8FF"))));
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
        QStringList headers = {"", "Issue Description", "Target Object", "Category"};
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

