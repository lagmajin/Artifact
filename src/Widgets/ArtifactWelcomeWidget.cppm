module;
#include <wobjectimpl.h>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QListWidget>
#include <QFileInfo>
#include <QDir>
#include <QApplication>
#include <QPalette>
#include <QFont>
#include <QStyle>
module Artifact.Widgets.Welcome;

import Widgets.Utils.CSS;
import Application.AppSettings;

namespace Artifact {

W_OBJECT_IMPL(ArtifactWelcomeWidget)

class ArtifactWelcomeWidget::Impl {
public:
    QLabel* titleLabel = nullptr;
    QLabel* subtitleLabel = nullptr;
    QListWidget* recentList = nullptr;
    QPushButton* openProjectBtn = nullptr;
    QPushButton* newCompBtn = nullptr;
    QPushButton* importBtn = nullptr;
    QLabel* emptyRecentLabel = nullptr;
};

ArtifactWelcomeWidget::ArtifactWelcomeWidget(QWidget* parent)
    : QWidget(parent), impl_(new Impl())
{
    setObjectName(QStringLiteral("welcomeWidget"));
    setAutoFillBackground(true);
    const auto& theme = ArtifactCore::currentDCCTheme();

    QPalette pal = palette();
    pal.setColor(QPalette::Window, QColor(theme.backgroundColor));
    pal.setColor(QPalette::WindowText, QColor(theme.textColor));
    pal.setColor(QPalette::Base, QColor(theme.secondaryBackgroundColor));
    pal.setColor(QPalette::Text, QColor(theme.textColor));
    pal.setColor(QPalette::Button, QColor(theme.secondaryBackgroundColor));
    pal.setColor(QPalette::ButtonText, QColor(theme.textColor));
    pal.setColor(QPalette::Highlight, QColor(theme.accentColor));
    pal.setColor(QPalette::HighlightedText, QColor(theme.textColor));
    setPalette(pal);

    auto* root = new QVBoxLayout(this);
    root->setAlignment(Qt::AlignCenter);
    root->setContentsMargins(80, 40, 80, 40);
    root->setSpacing(16);

    auto* centerWidget = new QWidget(this);
    centerWidget->setFixedWidth(480);
    centerWidget->setPalette(pal);
    centerWidget->setAutoFillBackground(true);
    auto* center = new QVBoxLayout(centerWidget);
    center->setAlignment(Qt::AlignCenter);
    center->setSpacing(12);

    impl_->titleLabel = new QLabel(QStringLiteral("Artifact"), centerWidget);
    QFont titleFont = impl_->titleLabel->font();
    titleFont.setPointSize(28);
    titleFont.setBold(true);
    impl_->titleLabel->setFont(titleFont);
    impl_->titleLabel->setAlignment(Qt::AlignCenter);
    impl_->titleLabel->setStyleSheet(QStringLiteral("color: %1;").arg(theme.accentColor));
    center->addWidget(impl_->titleLabel);

    impl_->subtitleLabel = new QLabel(QStringLiteral("Start a new project or open an existing one."), centerWidget);
    QFont subFont = impl_->subtitleLabel->font();
    subFont.setPointSize(11);
    impl_->subtitleLabel->setFont(subFont);
    impl_->subtitleLabel->setAlignment(Qt::AlignCenter);
    impl_->subtitleLabel->setStyleSheet(QStringLiteral("color: %1;").arg(QColor(theme.textColor).darker(120).name()));
    center->addWidget(impl_->subtitleLabel);

    center->addSpacing(8);

    auto* recentLabel = new QLabel(QStringLiteral("Recent Projects"), centerWidget);
    QFont recentFont = recentLabel->font();
    recentFont.setPointSize(10);
    recentFont.setBold(true);
    recentLabel->setFont(recentFont);
    recentLabel->setStyleSheet(QStringLiteral("color: %1;").arg(QColor(theme.textColor).lighter(130).name()));
    center->addWidget(recentLabel);

    impl_->recentList = new QListWidget(centerWidget);
    impl_->recentList->setMinimumHeight(120);
    impl_->recentList->setMaximumHeight(200);
    impl_->recentList->setObjectName(QStringLiteral("welcomeRecentList"));
    impl_->recentList->setStyleSheet(QStringLiteral(
        "QListWidget { background: %1; border: 1px solid %2; border-radius: 4px; padding: 4px; }"
        "QListWidget::item { padding: 6px 8px; border-radius: 3px; }"
        "QListWidget::item:hover { background: %3; }"
        "QListWidget::item:selected { background: %4; color: %5; }")
        .arg(theme.secondaryBackgroundColor)
        .arg(theme.borderColor)
        .arg(QColor(theme.selectionColor).lighter(150).name())
        .arg(theme.selectionColor)
        .arg(theme.textColor));
    QObject::connect(impl_->recentList, &QListWidget::itemActivated, this, [this](QListWidgetItem* item) {
        const QString path = item->data(Qt::UserRole).toString();
        if (!path.isEmpty()) {
            Q_EMIT openRecentProject(path);
        }
    });
    center->addWidget(impl_->recentList);

    impl_->emptyRecentLabel = new QLabel(QStringLiteral("No recent projects"), centerWidget);
    impl_->emptyRecentLabel->setAlignment(Qt::AlignCenter);
    impl_->emptyRecentLabel->setStyleSheet(QStringLiteral("color: %1;").arg(QColor(theme.textColor).darker(140).name()));
    impl_->emptyRecentLabel->setMinimumHeight(80);
    impl_->emptyRecentLabel->hide();
    center->addWidget(impl_->emptyRecentLabel);

    center->addSpacing(8);

    auto* btnRow = new QHBoxLayout();
    btnRow->setSpacing(12);

    auto makeButton = [&](const QString& text) -> QPushButton* {
        auto* btn = new QPushButton(text, centerWidget);
        btn->setMinimumHeight(36);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setStyleSheet(QStringLiteral(
            "QPushButton { background: %1; color: %2; border: 1px solid %3; border-radius: 4px; padding: 8px 16px; font-size: 12px; }"
            "QPushButton:hover { background: %4; }"
            "QPushButton:pressed { background: %5; }")
        .arg(theme.secondaryBackgroundColor)
        .arg(theme.textColor)
        .arg(theme.borderColor)
        .arg(theme.selectionColor)
        .arg(QColor(theme.selectionColor).darker(120).name()));
        return btn;
    };

    impl_->newCompBtn = makeButton(QStringLiteral("New Composition"));
    QObject::connect(impl_->newCompBtn, &QPushButton::clicked, this, &ArtifactWelcomeWidget::createNewComposition);
    btnRow->addWidget(impl_->newCompBtn);

    impl_->importBtn = makeButton(QStringLiteral("Import Asset"));
    QObject::connect(impl_->importBtn, &QPushButton::clicked, this, &ArtifactWelcomeWidget::importAsset);
    btnRow->addWidget(impl_->importBtn);

    impl_->openProjectBtn = makeButton(QStringLiteral("Open Project"));
    QObject::connect(impl_->openProjectBtn, &QPushButton::clicked, this, &ArtifactWelcomeWidget::openProject);
    btnRow->addWidget(impl_->openProjectBtn);

    center->addLayout(btnRow);
    root->addWidget(centerWidget, 0, Qt::AlignCenter);

    refreshRecentProjects();
}

ArtifactWelcomeWidget::~ArtifactWelcomeWidget()
{
    delete impl_;
}

void ArtifactWelcomeWidget::refreshRecentProjects()
{
    auto* settings = ArtifactCore::ArtifactAppSettings::instance();
    if (!settings) return;

    const QStringList recent = settings->recentProjectPaths();
    impl_->recentList->clear();

    if (recent.isEmpty()) {
        impl_->recentList->hide();
        impl_->emptyRecentLabel->show();
        return;
    }

    impl_->emptyRecentLabel->hide();
    impl_->recentList->show();

    for (const auto& path : recent) {
        QFileInfo fi(path);
        if (!fi.exists()) continue;

        auto* item = new QListWidgetItem(impl_->recentList);
        item->setText(fi.fileName());
        item->setToolTip(path);
        item->setData(Qt::UserRole, path);
    }
}

} // namespace Artifact
