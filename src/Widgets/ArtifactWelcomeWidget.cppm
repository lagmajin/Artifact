module;
#include <wobjectimpl.h>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QListWidget>
#include <QFrame>
#include <QFileInfo>
#include <QDateTime>
#include <QSize>
#include <QDir>
#include <QApplication>
#include <QPalette>
#include <QFont>
#include <QStyle>
module Artifact.Widgets.Welcome;

import Widgets.Utils.CSS;
import Application.AppSettings;
import Artifact.Event.Types;
import Event.Bus;

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
    root->setContentsMargins(56, 36, 56, 36);
    root->setSpacing(18);

    auto* centerWidget = new QWidget(this);
    centerWidget->setMinimumWidth(760);
    centerWidget->setMaximumWidth(1080);
    centerWidget->setPalette(pal);
    centerWidget->setAutoFillBackground(true);
    auto* center = new QVBoxLayout(centerWidget);
    center->setSpacing(14);

    impl_->titleLabel = new QLabel(QStringLiteral("Artifact"), centerWidget);
    QFont titleFont = impl_->titleLabel->font();
    titleFont.setPointSize(28);
    titleFont.setBold(true);
    impl_->titleLabel->setFont(titleFont);
    impl_->titleLabel->setAlignment(Qt::AlignCenter);
    {
      QPalette pal = impl_->titleLabel->palette();
      pal.setColor(QPalette::WindowText, QColor(theme.accentColor));
      impl_->titleLabel->setPalette(pal);
    }
    center->addWidget(impl_->titleLabel);

    impl_->subtitleLabel = new QLabel(QStringLiteral("Start a new project or open an existing one."), centerWidget);
    QFont subFont = impl_->subtitleLabel->font();
    subFont.setPointSize(11);
    impl_->subtitleLabel->setFont(subFont);
    impl_->subtitleLabel->setAlignment(Qt::AlignCenter);
    {
      QPalette pal = impl_->subtitleLabel->palette();
      pal.setColor(QPalette::WindowText, QColor(theme.textColor).darker(120));
      impl_->subtitleLabel->setPalette(pal);
    }
    center->addWidget(impl_->subtitleLabel);

    center->addSpacing(12);

    auto* contentLayout = new QHBoxLayout();
    contentLayout->setContentsMargins(0, 0, 0, 0);
    contentLayout->setSpacing(28);

    auto* startPanel = new QFrame(centerWidget);
    startPanel->setFrameShape(QFrame::NoFrame);
    startPanel->setMinimumWidth(240);
    startPanel->setMaximumWidth(300);
    auto* startLayout = new QVBoxLayout(startPanel);
    startLayout->setContentsMargins(0, 0, 0, 0);
    startLayout->setSpacing(10);
    auto* startLabel = new QLabel(QStringLiteral("Start"), startPanel);
    QFont startFont = startLabel->font();
    startFont.setPointSize(11);
    startFont.setBold(true);
    startLabel->setFont(startFont);
    startLayout->addWidget(startLabel);

    auto* recentPanel = new QFrame(centerWidget);
    recentPanel->setFrameShape(QFrame::NoFrame);
    auto* recentLayout = new QVBoxLayout(recentPanel);
    recentLayout->setContentsMargins(0, 0, 0, 0);
    recentLayout->setSpacing(10);
    auto* recentLabel = new QLabel(QStringLiteral("Recent Projects"), recentPanel);
    QFont recentFont = recentLabel->font();
    recentFont.setPointSize(10);
    recentFont.setBold(true);
    recentLabel->setFont(recentFont);
    {
      QPalette pal = recentLabel->palette();
      pal.setColor(QPalette::WindowText, QColor(theme.textColor).lighter(130));
      recentLabel->setPalette(pal);
    }
    recentLayout->addWidget(recentLabel);

    impl_->recentList = new QListWidget(recentPanel);
    impl_->recentList->setMinimumHeight(240);
    impl_->recentList->setMaximumHeight(360);
    impl_->recentList->setSpacing(1);
    impl_->recentList->setObjectName(QStringLiteral("welcomeRecentList"));
    {
      QPalette pal = impl_->recentList->palette();
      pal.setColor(QPalette::Base, QColor(theme.secondaryBackgroundColor));
      pal.setColor(QPalette::Text, QColor(theme.textColor));
      pal.setColor(QPalette::Highlight, QColor(theme.selectionColor));
      pal.setColor(QPalette::HighlightedText, QColor(theme.textColor));
      impl_->recentList->setPalette(pal);
    }
    QObject::connect(impl_->recentList, &QListWidget::itemActivated, this, [this](QListWidgetItem* item) {
        const QString path = item->data(Qt::UserRole).toString();
        if (!path.isEmpty()) {
            openRecentProject(path);
        }
    });
    recentLayout->addWidget(impl_->recentList, 1);

    impl_->emptyRecentLabel = new QLabel(QStringLiteral("No recent projects"), recentPanel);
    impl_->emptyRecentLabel->setAlignment(Qt::AlignCenter);
    {
      QPalette pal = impl_->emptyRecentLabel->palette();
      pal.setColor(QPalette::WindowText, QColor(theme.textColor).darker(140));
      impl_->emptyRecentLabel->setPalette(pal);
    }
    impl_->emptyRecentLabel->setMinimumHeight(80);
    impl_->emptyRecentLabel->hide();
    recentLayout->addWidget(impl_->emptyRecentLabel);

    auto makeButton = [&](const QString& text) -> QPushButton* {
        auto* btn = new QPushButton(text, startPanel);
        btn->setMinimumHeight(46);
        btn->setCursor(Qt::PointingHandCursor);
        {
          QPalette pal = btn->palette();
          pal.setColor(QPalette::Button, QColor(theme.secondaryBackgroundColor));
          pal.setColor(QPalette::ButtonText, QColor(theme.textColor));
          btn->setPalette(pal);
          btn->setAutoFillBackground(true);
        }
        return btn;
    };

    impl_->newCompBtn = makeButton(QStringLiteral("New Composition"));
    QObject::connect(impl_->newCompBtn, &QPushButton::clicked, this, &ArtifactWelcomeWidget::createNewComposition);
    startLayout->addWidget(impl_->newCompBtn);

    impl_->importBtn = makeButton(QStringLiteral("Import Asset"));
    QObject::connect(impl_->importBtn, &QPushButton::clicked, this, &ArtifactWelcomeWidget::importAsset);
    startLayout->addWidget(impl_->importBtn);

    impl_->openProjectBtn = makeButton(QStringLiteral("Open Project"));
    QObject::connect(impl_->openProjectBtn, &QPushButton::clicked, this, &ArtifactWelcomeWidget::openProject);
    startLayout->addWidget(impl_->openProjectBtn);
    startLayout->addStretch(1);

    contentLayout->addWidget(startPanel);
    contentLayout->addWidget(recentPanel, 1);
    center->addLayout(contentLayout, 1);
    root->addStretch(1);
    root->addWidget(centerWidget, 1, Qt::AlignHCenter);
    root->addStretch(1);

    refreshRecentProjects();
}

ArtifactWelcomeWidget::~ArtifactWelcomeWidget()
{
    delete impl_;
}

void ArtifactWelcomeWidget::createNewComposition()
{
    ArtifactCore::globalEventBus().publish<CreateCompositionRequestedEvent>({});
}

void ArtifactWelcomeWidget::openRecentProject(const QString& path)
{
    if (path.isEmpty()) {
        return;
    }
    ArtifactCore::globalEventBus().publish<OpenRecentProjectRequestedEvent>(
        OpenRecentProjectRequestedEvent{path});
}

void ArtifactWelcomeWidget::openProject()
{
    ArtifactCore::globalEventBus().publish<OpenProjectRequestedEvent>({});
}

void ArtifactWelcomeWidget::importAsset()
{
    ArtifactCore::globalEventBus().publish<ImportAssetsRequestedEvent>({});
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
        item->setText(QStringLiteral("%1\n%2  ·  %3")
                          .arg(fi.fileName(), fi.absolutePath(),
                               fi.lastModified().toString(
                                   QStringLiteral("yyyy-MM-dd  HH:mm"))));
        item->setSizeHint(QSize(0, 56));
        item->setToolTip(path);
        item->setData(Qt::UserRole, path);
    }
}

} // namespace Artifact
