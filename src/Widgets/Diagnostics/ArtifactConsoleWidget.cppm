module;
#include <QBoxLayout>
#include <QToolButton>
#include <QListWidget>
#include <QLineEdit>
#include <QCheckBox>
#include <QLabel>
#include <QDateTime>
#include <QIcon>
#include <QString>
#include <QPainter>
#include <QMenu>
#include <QAction>
#include <QClipboard>
#include <QApplication>
#include <wobjectimpl.h>

module Artifact.Widgets.ConsoleWidget;

import Diagnostics.Logger;
import Utils;
import Widgets.Utils.CSS;
import Artifact.Service.Playback;

namespace {

QIcon loadIcon(const QString& path) {
    const QString resPath = ArtifactCore::resolveIconResourcePath(path);
    QIcon icon(resPath);
    if (!icon.isNull()) return icon;
    return QIcon(ArtifactCore::resolveIconPath(path));
}

} // namespace

namespace Artifact {

using namespace ArtifactCore;

W_OBJECT_IMPL(ArtifactConsoleWidget)

class ArtifactConsoleWidget::Impl {
public:
    ArtifactConsoleWidget* owner_;
    
    QToolButton* clearBtn_ = nullptr;
    QCheckBox* clearOnPlayCheck_ = nullptr;
    QLineEdit* searchEdit_ = nullptr;
    QToolButton* infoFilterBtn_ = nullptr;
    QToolButton* warningFilterBtn_ = nullptr;
    QToolButton* errorFilterBtn_ = nullptr;
    QListWidget* logList_ = nullptr;

    bool showInfo_ = true;
    bool showWarning_ = true;
    bool showError_ = true;
    QString searchFilter_;

    Impl(ArtifactConsoleWidget* owner) : owner_(owner) {}

    void setupUI() {
        auto* layout = new QVBoxLayout(owner_);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(0);

        // Toolbar
        auto* toolbarLayout = new QHBoxLayout();
        toolbarLayout->setContentsMargins(4, 4, 4, 4);
        toolbarLayout->setSpacing(8);

        clearBtn_ = createToolButton("MaterialVS/colored/E3E3E3/clear.svg", "Clear Logs");
        toolbarLayout->addWidget(clearBtn_);

        clearOnPlayCheck_ = new QCheckBox("Clear on Play");
        clearOnPlayCheck_->setStyleSheet("color: #bbbbbb; font-size: 11px;");
        toolbarLayout->addWidget(clearOnPlayCheck_);

        toolbarLayout->addSpacing(8);
        searchEdit_ = new QLineEdit();
        searchEdit_->setPlaceholderText("Search...");
        searchEdit_->setStyleSheet(R"(
            QLineEdit {
                background-color: #3c3c3c;
                color: #e0e0e0;
                border: 1px solid #555555;
                border-radius: 4px;
                padding: 2px 8px;
                height: 22px;
            }
        )");
        toolbarLayout->addWidget(searchEdit_);

        toolbarLayout->addStretch();

        infoFilterBtn_ = createToolButton("MaterialVS/colored/E3E3E3/info.svg", "Toggle Information");
        infoFilterBtn_->setCheckable(true);
        infoFilterBtn_->setChecked(true);
        toolbarLayout->addWidget(infoFilterBtn_);

        warningFilterBtn_ = createToolButton("MaterialVS/colored/E3E3E3/warning.svg", "Toggle Warnings");
        warningFilterBtn_->setCheckable(true);
        warningFilterBtn_->setChecked(true);
        toolbarLayout->addWidget(warningFilterBtn_);

        errorFilterBtn_ = createToolButton("MaterialVS/colored/E3E3E3/error.svg", "Toggle Errors");
        errorFilterBtn_->setCheckable(true);
        errorFilterBtn_->setChecked(true);
        toolbarLayout->addWidget(errorFilterBtn_);

        layout->addLayout(toolbarLayout);

        // List
        logList_ = new QListWidget();
        logList_->setAlternatingRowColors(true);
        logList_->setStyleSheet(R"(
            QListWidget {
                background-color: #1e1e1e;
                color: #e0e0e0;
                border: none;
                font-family: Consolas, monospace;
                font-size: 11px;
            }
            QListWidget::item {
                padding: 2px 4px;
                border-bottom: 1px solid #2d2d2d;
            }
            QListWidget::item:alternate {
                background-color: #252526;
            }
            QListWidget::item:selected {
                background-color: #37373d;
                color: white;
            }
        )");
        layout->addWidget(logList_);

        // Connect Buttons
        QObject::connect(clearBtn_, &QToolButton::clicked, owner_, [this]() {
            Logger::instance()->clearLogs();
        });

        QObject::connect(searchEdit_, &QLineEdit::textChanged, owner_, [this](const QString& text) {
            searchFilter_ = text;
            refreshList();
        });

        QObject::connect(infoFilterBtn_, &QToolButton::toggled, owner_, [this](bool checked) {
            showInfo_ = checked;
            refreshList();
        });
        QObject::connect(warningFilterBtn_, &QToolButton::toggled, owner_, [this](bool checked) {
            showWarning_ = checked;
            refreshList();
        });
        QObject::connect(errorFilterBtn_, &QToolButton::toggled, owner_, [this](bool checked) {
            showError_ = checked;
            refreshList();
        });

        // Context menu
        logList_->setContextMenuPolicy(Qt::CustomContextMenu);
        QObject::connect(logList_, &QListWidget::customContextMenuRequested, owner_, [this](const QPoint& pos) {
            QMenu menu;

            QAction* copyAction = menu.addAction("Copy");
            copyAction->setEnabled(!logList_->selectedItems().isEmpty());
            QObject::connect(copyAction, &QAction::triggered, owner_, [this]() {
                QStringList lines;
                for (auto* item : logList_->selectedItems()) {
                    lines.append(item->text());
                }
                QApplication::clipboard()->setText(lines.join("\n"));
            });

            QAction* copyAllAction = menu.addAction("Copy All");
            QObject::connect(copyAllAction, &QAction::triggered, owner_, [this]() {
                QStringList lines;
                for (int i = 0; i < logList_->count(); ++i) {
                    lines.append(logList_->item(i)->text());
                }
                QApplication::clipboard()->setText(lines.join("\n"));
            });

            menu.addSeparator();

            QAction* clearAction = menu.addAction("Clear");
            QObject::connect(clearAction, &QAction::triggered, owner_, [this]() {
                Logger::instance()->clearLogs();
            });

            menu.exec(logList_->viewport()->mapToGlobal(pos));
        });

        // Initialize Logger Signal
        QObject::connect(Logger::instance(), &Logger::logAdded, owner_, [this](int level, const QString& msg, const QString& context, const QDateTime& ts) {
            appendLogItem(static_cast<LogLevel>(level), msg, context, ts);
        });

        QObject::connect(Logger::instance(), &Logger::logsCleared, owner_, [this]() {
            logList_->clear();
        });

        // Playback Service for "Clear on Play"
        if (auto* svc = ArtifactPlaybackService::instance()) {
            QObject::connect(svc, &ArtifactPlaybackService::playbackStateChanged, owner_, [this](::Artifact::PlaybackState state) {
                if (state == ::Artifact::PlaybackState::Playing && clearOnPlayCheck_->isChecked()) {
                    Logger::instance()->clearLogs();
                }
            });
        }

        owner_->setStyleSheet(R"(
            ArtifactConsoleWidget {
                background-color: #2d2d2d;
            }
            QToolButton {
                background-color: transparent;
                border: 1px solid transparent;
                border-radius: 4px;
                padding: 4px;
            }
            QToolButton:hover {
                background-color: rgba(255, 255, 255, 0.1);
            }
            QToolButton:checked {
                background-color: rgba(100, 150, 255, 0.3);
                border: 1px solid rgba(100, 150, 255, 0.5);
            }
        )");

        refreshList();
    }

    QToolButton* createToolButton(const QString& iconPath, const QString& tooltip) {
        auto* btn = new QToolButton();
        btn->setIcon(loadIcon(iconPath));
        btn->setIconSize(QSize(18, 18));
        btn->setToolTip(tooltip);
        btn->setFixedSize(28, 28);
        return btn;
    }

    bool shouldShowLog(LogLevel level, const QString& message) const {
        if (!searchFilter_.isEmpty() && !message.contains(searchFilter_, Qt::CaseInsensitive)) {
            return false;
        }
        if (level == LogLevel::Debug || level == LogLevel::Info) return showInfo_;
        if (level == LogLevel::Warning) return showWarning_;
        if (level == LogLevel::Error || level == LogLevel::Fatal) return showError_;
        return true;
    }

    void refreshList() {
        logList_->clear();
        auto logs = Logger::instance()->getLogs();
        for (const auto& log : logs) {
            appendLogItem(log.level, log.message, log.context, log.timestamp);
        }
    }

    void appendLogItem(LogLevel level, const QString& message, const QString& context, const QDateTime& ts) {
        if (!shouldShowLog(level, message)) return;

        QListWidgetItem* item = new QListWidgetItem();
        
        QIcon icon;
        if (level == LogLevel::Warning) {
            icon = loadIcon("MaterialVS/colored/FFD700/warning_active.svg");
        } else if (level == LogLevel::Error || level == LogLevel::Fatal) {
            icon = loadIcon("MaterialVS/colored/FF4B4B/error_active.svg");
        } else {
            icon = loadIcon("MaterialVS/colored/4EA0FF/info_active.svg");
        }
        item->setIcon(icon);
        
        QString text = QString("[%1] %2").arg(ts.toString("hh:mm:ss"), message);
        if (!context.isEmpty()) {
            item->setToolTip(context);
        }
        item->setText(text);
        
        if (level == LogLevel::Warning) {
            item->setForeground(QBrush(QColor(255, 215, 0))); // Gold
        } else if (level == LogLevel::Error || level == LogLevel::Fatal) {
            item->setForeground(QBrush(QColor(255, 75, 75))); // Red
        }

        logList_->addItem(item);
        logList_->scrollToBottom();
    }
};

ArtifactConsoleWidget::ArtifactConsoleWidget(QWidget* parent)
    : QWidget(parent), impl_(new Impl(this)) {
    impl_->setupUI();
}

ArtifactConsoleWidget::~ArtifactConsoleWidget() {
    delete impl_;
}

} // namespace Artifact
