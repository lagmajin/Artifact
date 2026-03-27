module;
#include <QBoxLayout>
#include <QToolButton>
#include <QListWidget>
#include <QLineEdit>
#include <QCheckBox>
#include <QLabel>
#include <QPlainTextEdit>
#include <QSpinBox>
#include <QSettings>
#include <QFont>
#include <QSignalBlocker>
#include <QDateTime>
#include <QIcon>
#include <QString>
#include <QPainter>
#include <QMenu>
#include <QAction>
#include <QSplitter>
#include <QFileDialog>
#include <QFile>
#include <QTextStream>
#include <QClipboard>
#include <QApplication>
#include <algorithm>
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
    struct DisplayLogEntry {
        LogLevel level;
        QString message;
        QString context;
        QDateTime timestamp;
        int count = 1;
    };

    ArtifactConsoleWidget* owner_;
    
    QToolButton* clearBtn_ = nullptr;
    QCheckBox* clearOnPlayCheck_ = nullptr;
    QToolButton* pauseBtn_ = nullptr;
    QToolButton* autoScrollBtn_ = nullptr;
    QToolButton* collapseBtn_ = nullptr;
    QToolButton* importantOnlyBtn_ = nullptr;
    QToolButton* copySelectedBtn_ = nullptr;
    QToolButton* copyVisibleBtn_ = nullptr;
    QToolButton* saveVisibleBtn_ = nullptr;
    QLineEdit* searchEdit_ = nullptr;
    QLabel* fontSizeLabel_ = nullptr;
    QSpinBox* fontSizeSpin_ = nullptr;
    QToolButton* infoFilterBtn_ = nullptr;
    QToolButton* warningFilterBtn_ = nullptr;
    QToolButton* errorFilterBtn_ = nullptr;
    QListWidget* logList_ = nullptr;
    QPlainTextEdit* detailView_ = nullptr;
    QLabel* statusLabel_ = nullptr;

    bool showInfo_ = true;
    bool showWarning_ = true;
    bool showError_ = true;
    bool paused_ = false;
    bool autoScroll_ = true;
    bool collapse_ = true;
    bool importantOnly_ = false;
    QString searchFilter_;
    int consoleFontPointSize_ = 12;
    int pendingWhilePaused_ = 0;
    int totalInfoCount_ = 0;
    int totalWarningCount_ = 0;
    int totalErrorCount_ = 0;
    std::vector<DisplayLogEntry> displayEntries_;

    Impl(ArtifactConsoleWidget* owner) : owner_(owner) {
        loadSettings();
    }

    static constexpr const char* kFontPointSizeKey = "ui/console/fontPointSize";

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
        clearOnPlayCheck_->setStyleSheet("color: #bbbbbb;");
        toolbarLayout->addWidget(clearOnPlayCheck_);

        pauseBtn_ = createToolButton("MaterialVS/colored/E3E3E3/pause.svg", "Pause incoming logs");
        pauseBtn_->setCheckable(true);
        pauseBtn_->setText(QStringLiteral("Pause"));
        toolbarLayout->addWidget(pauseBtn_);

        autoScrollBtn_ = createToolButton("MaterialVS/colored/E3E3E3/down.svg", "Auto scroll");
        autoScrollBtn_->setCheckable(true);
        autoScrollBtn_->setChecked(true);
        autoScrollBtn_->setText(QStringLiteral("Follow"));
        toolbarLayout->addWidget(autoScrollBtn_);

        collapseBtn_ = createToolButton("MaterialVS/colored/E3E3E3/list.svg", "Collapse duplicate logs");
        collapseBtn_->setCheckable(true);
        collapseBtn_->setChecked(true);
        collapseBtn_->setText(QStringLiteral("Collapse"));
        toolbarLayout->addWidget(collapseBtn_);

        importantOnlyBtn_ = createToolButton("MaterialVS/colored/E3E3E3/filter.svg", "Show warnings and errors only");
        importantOnlyBtn_->setCheckable(true);
        importantOnlyBtn_->setText(QStringLiteral("Important"));
        toolbarLayout->addWidget(importantOnlyBtn_);

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

        copySelectedBtn_ = createToolButton("MaterialVS/colored/E3E3E3/copy.svg", "Copy selected log");
        copySelectedBtn_->setText(QStringLiteral("Copy Selected"));
        toolbarLayout->addWidget(copySelectedBtn_);

        copyVisibleBtn_ = createToolButton("MaterialVS/colored/E3E3E3/copy.svg", "Copy visible logs");
        copyVisibleBtn_->setText(QStringLiteral("Copy Visible"));
        toolbarLayout->addWidget(copyVisibleBtn_);

        saveVisibleBtn_ = createToolButton("MaterialVS/colored/E3E3E3/save.svg", "Save visible logs to file");
        saveVisibleBtn_->setText(QStringLiteral("Save Visible"));
        toolbarLayout->addWidget(saveVisibleBtn_);

        toolbarLayout->addSpacing(8);
        fontSizeLabel_ = new QLabel(QStringLiteral("Font"));
        toolbarLayout->addWidget(fontSizeLabel_);

        fontSizeSpin_ = new QSpinBox();
        fontSizeSpin_->setRange(8, 24);
        fontSizeSpin_->setSuffix(QStringLiteral(" pt"));
        fontSizeSpin_->setFixedWidth(84);
        fontSizeSpin_->setToolTip(QStringLiteral("Console font size"));
        toolbarLayout->addWidget(fontSizeSpin_);

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

        statusLabel_ = new QLabel(owner_);
        statusLabel_->setStyleSheet("color: #9a9a9a; padding: 2px 8px;");
        layout->addWidget(statusLabel_);

        auto* splitter = new QSplitter(Qt::Vertical, owner_);

        logList_ = new QListWidget();
        logList_->setAlternatingRowColors(true);
        logList_->setStyleSheet(R"(
            QListWidget {
                background-color: #1e1e1e;
                color: #e0e0e0;
                border: none;
                font-family: Consolas, monospace;
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
        splitter->addWidget(logList_);

        detailView_ = new QPlainTextEdit(owner_);
        detailView_->setReadOnly(true);
        detailView_->setMinimumHeight(110);
        detailView_->setStyleSheet(R"(
            QPlainTextEdit {
                background-color: #171717;
                color: #d8d8d8;
                border: none;
                border-top: 1px solid #2d2d2d;
                font-family: Consolas, monospace;
                padding: 6px;
            }
        )");
        splitter->addWidget(detailView_);
        splitter->setStretchFactor(0, 4);
        splitter->setStretchFactor(1, 1);
        layout->addWidget(splitter);

        // Connect Buttons
        QObject::connect(clearBtn_, &QToolButton::clicked, owner_, [this]() {
            Logger::instance()->clearLogs();
        });

        QObject::connect(pauseBtn_, &QToolButton::toggled, owner_, [this](bool checked) {
            paused_ = checked;
            if (!paused_ && pendingWhilePaused_ > 0) {
                refreshList();
                pendingWhilePaused_ = 0;
            }
            updateStatus();
        });

        QObject::connect(autoScrollBtn_, &QToolButton::toggled, owner_, [this](bool checked) {
            autoScroll_ = checked;
            updateStatus();
        });

        QObject::connect(collapseBtn_, &QToolButton::toggled, owner_, [this](bool checked) {
            collapse_ = checked;
            refreshList();
        });

        QObject::connect(importantOnlyBtn_, &QToolButton::toggled, owner_, [this](bool checked) {
            importantOnly_ = checked;
            refreshList();
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

        QObject::connect(logList_, &QListWidget::currentItemChanged, owner_, [this](QListWidgetItem* current) {
            showItemDetails(current);
        });

        QObject::connect(copySelectedBtn_, &QToolButton::clicked, owner_, [this]() {
            copySelectedToClipboard();
        });

        QObject::connect(copyVisibleBtn_, &QToolButton::clicked, owner_, [this]() {
            QApplication::clipboard()->setText(visibleLogText());
        });

        QObject::connect(saveVisibleBtn_, &QToolButton::clicked, owner_, [this]() {
            saveVisibleLogsToFile();
        });

        // Context menu
        logList_->setContextMenuPolicy(Qt::CustomContextMenu);
        QObject::connect(logList_, &QListWidget::customContextMenuRequested, owner_, [this](const QPoint& pos) {
            QMenu menu;

            QAction* copyAction = menu.addAction("Copy");
            copyAction->setEnabled(!logList_->selectedItems().isEmpty());
            QObject::connect(copyAction, &QAction::triggered, owner_, [this]() { copySelectedToClipboard(); });

            QAction* copyAllAction = menu.addAction("Copy All");
            QObject::connect(copyAllAction, &QAction::triggered, owner_, [this]() {
                QApplication::clipboard()->setText(visibleLogText());
            });

            QAction* copyDetailsAction = menu.addAction("Copy Details");
            copyDetailsAction->setEnabled(logList_->currentItem() != nullptr);
            QObject::connect(copyDetailsAction, &QAction::triggered, owner_, [this]() {
                if (detailView_) {
                    QApplication::clipboard()->setText(detailView_->toPlainText());
                }
            });

            menu.addSeparator();

            QAction* clearAction = menu.addAction("Clear");
            QObject::connect(clearAction, &QAction::triggered, owner_, [this]() {
                Logger::instance()->clearLogs();
            });

            QAction* saveVisibleAction = menu.addAction("Save Visible...");
            QObject::connect(saveVisibleAction, &QAction::triggered, owner_, [this]() {
                saveVisibleLogsToFile();
            });

            menu.exec(logList_->viewport()->mapToGlobal(pos));
        });

        // Initialize Logger Signal
        QObject::connect(Logger::instance(), &Logger::logAdded, owner_, [this](int level, const QString& msg, const QString& context, const QDateTime& ts) {
            onLogAdded(static_cast<LogLevel>(level), msg, context, ts);
        });

        QObject::connect(Logger::instance(), &Logger::logsCleared, owner_, [this]() {
            logList_->clear();
            displayEntries_.clear();
            if (detailView_) {
                detailView_->clear();
            }
            pendingWhilePaused_ = 0;
            totalInfoCount_ = 0;
            totalWarningCount_ = 0;
            totalErrorCount_ = 0;
            updateStatus();
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

        connectFontControls();
        applyFontPointSize(consoleFontPointSize_, false);
        refreshList();
    }

    QToolButton* createToolButton(const QString& iconPath, const QString& tooltip) {
        auto* btn = new QToolButton();
        btn->setIcon(loadIcon(iconPath));
        btn->setIconSize(QSize(18, 18));
        btn->setToolTip(tooltip);
        btn->setMinimumHeight(28);
        return btn;
    }

    QFont uiFont() const {
        QFont font = owner_ ? owner_->font() : QFont();
        font.setPointSize(consoleFontPointSize_);
        return font;
    }

    QFont monoFont() const {
        QFont font(QStringLiteral("Consolas"));
        font.setStyleHint(QFont::Monospace);
        font.setPointSize(consoleFontPointSize_);
        return font;
    }

    void loadSettings() {
        QSettings settings;
        const int stored = settings.value(QString::fromLatin1(kFontPointSizeKey), 12).toInt();
        consoleFontPointSize_ = std::clamp(stored, 8, 24);
    }

    void saveSettings() const {
        QSettings settings;
        settings.setValue(QString::fromLatin1(kFontPointSizeKey), consoleFontPointSize_);
    }

    int fontPointSize() const {
        return consoleFontPointSize_;
    }

    void connectFontControls() {
        if (!fontSizeSpin_) {
            return;
        }
        fontSizeSpin_->setValue(consoleFontPointSize_);
        QObject::connect(fontSizeSpin_, QOverload<int>::of(&QSpinBox::valueChanged), owner_, [this](int value) {
            applyFontPointSize(value, true);
        });
    }

    void applyFontPointSize(int pointSize, bool persist) {
        consoleFontPointSize_ = std::clamp(pointSize, 8, 24);

        if (fontSizeSpin_) {
            const QSignalBlocker blocker(fontSizeSpin_);
            fontSizeSpin_->setValue(consoleFontPointSize_);
        }

        const QFont textFont = uiFont();
        if (owner_) owner_->setFont(textFont);
        if (clearOnPlayCheck_) clearOnPlayCheck_->setFont(textFont);
        if (pauseBtn_) pauseBtn_->setFont(textFont);
        if (autoScrollBtn_) autoScrollBtn_->setFont(textFont);
        if (collapseBtn_) collapseBtn_->setFont(textFont);
        if (importantOnlyBtn_) importantOnlyBtn_->setFont(textFont);
        if (copySelectedBtn_) copySelectedBtn_->setFont(textFont);
        if (copyVisibleBtn_) copyVisibleBtn_->setFont(textFont);
        if (saveVisibleBtn_) saveVisibleBtn_->setFont(textFont);
        if (searchEdit_) searchEdit_->setFont(textFont);
        if (fontSizeLabel_) fontSizeLabel_->setFont(textFont);
        if (fontSizeSpin_) fontSizeSpin_->setFont(textFont);
        if (infoFilterBtn_) infoFilterBtn_->setFont(textFont);
        if (warningFilterBtn_) warningFilterBtn_->setFont(textFont);
        if (errorFilterBtn_) errorFilterBtn_->setFont(textFont);
        if (statusLabel_) statusLabel_->setFont(textFont);

        const QFont monospaceFont = monoFont();
        if (logList_) logList_->setFont(monospaceFont);
        if (detailView_) detailView_->setFont(monospaceFont);

        if (persist) {
            saveSettings();
        }
        updateStatus();
    }

    bool shouldShowLog(LogLevel level, const QString& message) const {
        if (!searchFilter_.isEmpty() && !message.contains(searchFilter_, Qt::CaseInsensitive)) {
            return false;
        }
        if (importantOnly_ && !(level == LogLevel::Warning || level == LogLevel::Error || level == LogLevel::Fatal)) {
            return false;
        }
        if (level == LogLevel::Debug || level == LogLevel::Info) return showInfo_;
        if (level == LogLevel::Warning) return showWarning_;
        if (level == LogLevel::Error || level == LogLevel::Fatal) return showError_;
        return true;
    }

    std::vector<DisplayLogEntry> buildDisplayEntries() const {
        const auto logs = Logger::instance()->getLogs();
        std::vector<DisplayLogEntry> entries;
        entries.reserve(logs.size());
        for (const auto& log : logs) {
            if (!shouldShowLog(log.level, log.message)) {
                continue;
            }
            if (collapse_ && !entries.empty()) {
                auto& last = entries.back();
                if (last.level == log.level &&
                    last.message == log.message &&
                    last.context == log.context) {
                    ++last.count;
                    last.timestamp = log.timestamp;
                    continue;
                }
            }
            entries.push_back(DisplayLogEntry{log.level, log.message, log.context, log.timestamp, 1});
        }
        return entries;
    }

    void recountTotalsFromLogger() {
        totalInfoCount_ = 0;
        totalWarningCount_ = 0;
        totalErrorCount_ = 0;
        const auto logs = Logger::instance()->getLogs();
        for (const auto& log : logs) {
            if (log.level == LogLevel::Warning) {
                ++totalWarningCount_;
            } else if (log.level == LogLevel::Error || log.level == LogLevel::Fatal) {
                ++totalErrorCount_;
            } else {
                ++totalInfoCount_;
            }
        }
    }

    void refreshList() {
        recountTotalsFromLogger();
        displayEntries_ = buildDisplayEntries();
        const auto previousText = logList_->currentItem()
            ? logList_->currentItem()->data(Qt::UserRole + 5).toString()
            : QString();
        logList_->clear();
        for (const auto& entry : displayEntries_) {
            appendLogItem(entry);
        }
        if (!previousText.isEmpty()) {
            for (int i = 0; i < logList_->count(); ++i) {
                auto* item = logList_->item(i);
                if (item && item->data(Qt::UserRole + 5).toString() == previousText) {
                    logList_->setCurrentItem(item);
                    break;
                }
            }
        }
        if (!logList_->currentItem()) {
            showItemDetails(nullptr);
        }
        updateStatus();
    }

    void onLogAdded(LogLevel level, const QString& message, const QString& context, const QDateTime& ts) {
        if (level == LogLevel::Warning) {
            ++totalWarningCount_;
        } else if (level == LogLevel::Error || level == LogLevel::Fatal) {
            ++totalErrorCount_;
        } else {
            ++totalInfoCount_;
        }
        if (paused_) {
            ++pendingWhilePaused_;
            updateStatus();
            return;
        }
        if (!shouldShowLog(level, message)) {
            updateStatus();
            return;
        }
        DisplayLogEntry entry{level, message, context, ts, 1};
        if (collapse_ && !displayEntries_.empty()) {
            auto& last = displayEntries_.back();
            if (last.level == entry.level &&
                last.message == entry.message &&
                last.context == entry.context) {
                ++last.count;
                last.timestamp = entry.timestamp;
                if (logList_ && logList_->count() > 0) {
                    if (auto* item = logList_->item(logList_->count() - 1)) {
                        updateListItem(item, last);
                        if (logList_->currentItem() == item) {
                            showItemDetails(item);
                        }
                    }
                }
                updateStatus();
                return;
            }
        }
        displayEntries_.push_back(entry);
        appendLogItem(entry);
        updateStatus();
    }

    void appendLogItem(const DisplayLogEntry& entry) {
        QListWidgetItem* item = new QListWidgetItem();
        updateListItem(item, entry);
        logList_->addItem(item);
        if (autoScroll_) {
            logList_->scrollToBottom();
        }
    }

    void updateListItem(QListWidgetItem* item, const DisplayLogEntry& entry) {
        if (!item) {
            return;
        }
        QIcon icon;
        if (entry.level == LogLevel::Warning) {
            icon = loadIcon("MaterialVS/colored/FFD700/warning_active.svg");
        } else if (entry.level == LogLevel::Error || entry.level == LogLevel::Fatal) {
            icon = loadIcon("MaterialVS/colored/FF4B4B/error_active.svg");
        } else {
            icon = loadIcon("MaterialVS/colored/4EA0FF/info_active.svg");
        }
        item->setIcon(icon);
        
        const QString repeatSuffix = entry.count > 1 ? QStringLiteral("  (x%1)").arg(entry.count) : QString();
        QString text = QString("[%1] %2%3").arg(entry.timestamp.toString("hh:mm:ss"), entry.message, repeatSuffix);
        item->setToolTip(entry.context);
        item->setText(text);
        item->setData(Qt::UserRole, static_cast<int>(entry.level));
        item->setData(Qt::UserRole + 1, entry.message);
        item->setData(Qt::UserRole + 2, entry.context);
        item->setData(Qt::UserRole + 3, entry.timestamp);
        item->setData(Qt::UserRole + 4, entry.count);
        item->setData(Qt::UserRole + 5, buildFullText(entry));
        
        if (entry.level == LogLevel::Warning) {
            item->setForeground(QBrush(QColor(255, 215, 0))); // Gold
        } else if (entry.level == LogLevel::Error || entry.level == LogLevel::Fatal) {
            item->setForeground(QBrush(QColor(255, 75, 75))); // Red
        } else {
            item->setForeground(QBrush(QColor(224, 224, 224)));
        }
    }

    QString buildFullText(const DisplayLogEntry& entry) const {
        QStringList lines;
        lines << QStringLiteral("[%1] %2").arg(entry.timestamp.toString(Qt::ISODateWithMs), entry.message);
        lines << QStringLiteral("Level: %1").arg(levelText(entry.level));
        if (entry.count > 1) {
            lines << QStringLiteral("Count: %1").arg(entry.count);
        }
        if (!entry.context.isEmpty()) {
            lines << QStringLiteral("Context: %1").arg(entry.context);
        }
        return lines.join('\n');
    }

    QString levelText(LogLevel level) const {
        switch (level) {
        case LogLevel::Debug: return QStringLiteral("Debug");
        case LogLevel::Info: return QStringLiteral("Info");
        case LogLevel::Warning: return QStringLiteral("Warning");
        case LogLevel::Error: return QStringLiteral("Error");
        case LogLevel::Fatal: return QStringLiteral("Fatal");
        }
        return QStringLiteral("Unknown");
    }

    void showItemDetails(QListWidgetItem* item) {
        if (!detailView_) {
            return;
        }
        if (!item) {
            detailView_->clear();
            return;
        }
        detailView_->setPlainText(item->data(Qt::UserRole + 5).toString());
    }

    void updateStatus() {
        if (!statusLabel_) {
            return;
        }
        const int totalCount = totalInfoCount_ + totalWarningCount_ + totalErrorCount_;
        statusLabel_->setText(
            QStringLiteral("Shown %1 / Total %2   Info %3   Warning %4   Error %5%6%7")
                .arg(static_cast<int>(displayEntries_.size()))
                .arg(totalCount)
                .arg(totalInfoCount_)
                .arg(totalWarningCount_)
                .arg(totalErrorCount_)
                .arg(paused_ ? QStringLiteral("   Paused") : QString())
                .arg(pendingWhilePaused_ > 0 ? QStringLiteral(" (+%1 queued)").arg(pendingWhilePaused_) : QString()));
    }

    QString visibleLogText() const {
        QStringList lines;
        if (!logList_) {
            return {};
        }
        for (int i = 0; i < logList_->count(); ++i) {
            if (auto* item = logList_->item(i)) {
                lines.append(item->data(Qt::UserRole + 5).toString());
            }
        }
        return lines.join(QStringLiteral("\n\n"));
    }

    void copySelectedToClipboard() const {
        if (!logList_) {
            return;
        }
        QStringList lines;
        for (auto* item : logList_->selectedItems()) {
            lines.append(item->data(Qt::UserRole + 5).toString());
        }
        QApplication::clipboard()->setText(lines.join(QStringLiteral("\n\n")));
    }

    void saveVisibleLogsToFile() const {
        const QString path = QFileDialog::getSaveFileName(
            owner_,
            QStringLiteral("Save Console Logs"),
            QStringLiteral("artifact_console_logs.txt"),
            QStringLiteral("Text Files (*.txt);;Log Files (*.log);;All Files (*.*)"));
        if (path.isEmpty()) {
            return;
        }
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
            return;
        }
        QTextStream stream(&file);
        stream << visibleLogText();
    }
};

ArtifactConsoleWidget::ArtifactConsoleWidget(QWidget* parent)
    : QWidget(parent), impl_(new Impl(this)) {
    impl_->setupUI();
}

ArtifactConsoleWidget::~ArtifactConsoleWidget() {
    delete impl_;
}

int ArtifactConsoleWidget::consoleFontPointSize() const {
    return impl_ ? impl_->fontPointSize() : 12;
}

void ArtifactConsoleWidget::setConsoleFontPointSize(int pointSize) {
    if (!impl_) {
        return;
    }
    impl_->applyFontPointSize(pointSize, true);
}

} // namespace Artifact
