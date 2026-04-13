module;
#include <utility>
#include <QBoxLayout>
#include <QAbstractItemView>
#include <QItemSelection>
#include <QMouseEvent>
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
#include <QDateTimeEdit>
#include <QComboBox>
#include <QIcon>
#include <QColor>
#include <QPalette>
#include <QBrush>
#include <QVariant>
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
#include <QRegularExpression>
#include <algorithm>
#include <wobjectimpl.h>

module Artifact.Widgets.DebugConsoleWidget;

import Diagnostics.Logger;
import Utils;
import Widgets.Utils.CSS;
import Artifact.Service.Playback;

namespace {

QIcon loadIcon(const QString& path) {
    if (path.trimmed().isEmpty()) {
        return QIcon();
    }
    const QString resPath = ArtifactCore::resolveIconResourcePath(path);
    QIcon icon(resPath);
    if (!icon.isNull()) return icon;
    const QString filePath = ArtifactCore::resolveIconPath(path);
    if (filePath.trimmed().isEmpty()) {
        return QIcon();
    }
    return QIcon(filePath);
}

} // namespace

namespace Artifact {

using namespace ArtifactCore;

W_OBJECT_IMPL(ArtifactDebugConsoleWidget)

class DebugConsoleLogListWidget final : public QListWidget {
public:
    explicit DebugConsoleLogListWidget(QWidget* parent = nullptr)
        : QListWidget(parent) {}

protected:
    void mousePressEvent(QMouseEvent* event) override {
        if (event && event->button() == Qt::LeftButton &&
            event->modifiers().testFlag(Qt::ShiftModifier)) {
            const QModelIndex clicked = indexAt(event->pos());
            if (clicked.isValid() && selectionModel()) {
                const QModelIndex anchor =
                    currentIndex().isValid() ? currentIndex() : clicked;
                const int firstRow = std::min(anchor.row(), clicked.row());
                const int lastRow = std::max(anchor.row(), clicked.row());
                const QModelIndex top = model()->index(firstRow, 0);
                const QModelIndex bottom = model()->index(lastRow, 0);
                QItemSelection range(top, bottom);
                selectionModel()->select(
                    range, QItemSelectionModel::ClearAndSelect |
                               QItemSelectionModel::Rows);
                selectionModel()->setCurrentIndex(clicked,
                                                  QItemSelectionModel::NoUpdate);
                event->accept();
                return;
            }
        }
        QListWidget::mousePressEvent(event);
    }
};

class ArtifactDebugConsoleWidget::Impl {
public:
    struct DisplayLogEntry {
        LogLevel level;
        QString message;
        QString context;
        QDateTime timestamp;
        int count = 1;
    };

    ArtifactDebugConsoleWidget* owner_;
    
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
    QDateTimeEdit* startTimeEdit_ = nullptr;
    QDateTimeEdit* endTimeEdit_ = nullptr;
    QCheckBox* timeFilterCheck_ = nullptr;
    QComboBox* contextFilterCombo_ = nullptr;
    QToolButton* saveFiltersBtn_ = nullptr;
    QToolButton* loadFiltersBtn_ = nullptr;
    QLabel* fontSizeLabel_ = nullptr;
    QSpinBox* fontSizeSpin_ = nullptr;
    QToolButton* debugFilterBtn_ = nullptr;
    QToolButton* infoFilterBtn_ = nullptr;
    QToolButton* warningFilterBtn_ = nullptr;
    QToolButton* errorFilterBtn_ = nullptr;
    QListWidget* logList_ = nullptr;
    QPlainTextEdit* detailView_ = nullptr;
    QLabel* statusLabel_ = nullptr;
    QLabel* filterSummaryLabel_ = nullptr;
    QColor infoColor_;
    QColor warningColor_;
    QColor dangerColor_;

    bool showDebug_ = true;
    bool showInfo_ = true;
    bool showWarning_ = true;
    bool showError_ = true;
    bool paused_ = false;
    bool autoScroll_ = true;
    bool collapse_ = true;
    bool importantOnly_ = false;
    QString searchFilter_;
    QRegularExpression regexFilter_;
    QDateTime startTime_;
    QDateTime endTime_;
    bool useTimeFilter_ = false;
    QStringList contextFilters_;
    QStringList availableContexts_;
    bool useContextFilter_ = false;
    int consoleFontPointSize_ = 12;
    int pendingWhilePaused_ = 0;
    int totalDebugCount_ = 0;
    int totalInfoCount_ = 0;
    int totalWarningCount_ = 0;
    int totalErrorCount_ = 0;
    std::vector<DisplayLogEntry> displayEntries_;

    Impl(ArtifactDebugConsoleWidget* owner) : owner_(owner) {
        loadSettings();
        // Initialize time range to last hour
        endTime_ = QDateTime::currentDateTime();
        startTime_ = endTime_.addSecs(-3600);
    }

    static constexpr const char* kFontPointSizeKey = "ui/debugConsole/fontPointSize";
    static constexpr const char* kLegacyFontPointSizeKey = "ui/console/fontPointSize";

    void setupUI() {
        auto* layout = new QVBoxLayout(owner_);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(0);

        const QColor background = QColor(ArtifactCore::currentDCCTheme().backgroundColor);
        const QColor surface = QColor(ArtifactCore::currentDCCTheme().secondaryBackgroundColor);
        const QColor text = QColor(ArtifactCore::currentDCCTheme().textColor);
        const QColor muted = text.darker(130);
        const QColor accent = QColor(ArtifactCore::currentDCCTheme().accentColor);
        const QColor danger = QColor(QStringLiteral("#FF4B4B"));
        const QColor warning = QColor(QStringLiteral("#FFD700"));
        const QColor info = QColor(QStringLiteral("#E0E0E0"));
        dangerColor_ = danger;
        warningColor_ = warning;
        infoColor_ = info;

        owner_->setAutoFillBackground(true);
        QPalette ownerPalette = owner_->palette();
        ownerPalette.setColor(QPalette::Window, background);
        ownerPalette.setColor(QPalette::WindowText, text);
        owner_->setPalette(ownerPalette);

        // Toolbar
        auto* toolbarLayout = new QHBoxLayout();
        toolbarLayout->setContentsMargins(4, 4, 4, 4);
        toolbarLayout->setSpacing(8);

        clearBtn_ = createToolButton("MaterialVS/colored/E3E3E3/clear.svg", "Clear Logs");
        toolbarLayout->addWidget(clearBtn_);

        clearOnPlayCheck_ = new QCheckBox("Clear on Play");
        clearOnPlayCheck_->setPalette([&]() {
            QPalette pal = clearOnPlayCheck_->palette();
            pal.setColor(QPalette::WindowText, muted);
            return pal;
        }());
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
        {
            QPalette pal = searchEdit_->palette();
            pal.setColor(QPalette::Base, surface);
            pal.setColor(QPalette::Text, text);
            pal.setColor(QPalette::PlaceholderText, muted.darker(110));
            searchEdit_->setPalette(pal);
        }
        toolbarLayout->addWidget(searchEdit_);

        timeFilterCheck_ = new QCheckBox("Time Filter");
        timeFilterCheck_->setPalette([&]() {
            QPalette pal = timeFilterCheck_->palette();
            pal.setColor(QPalette::WindowText, muted);
            return pal;
        }());
        toolbarLayout->addWidget(timeFilterCheck_);

        startTimeEdit_ = new QDateTimeEdit(startTime_);
        startTimeEdit_->setDisplayFormat("yyyy-MM-dd hh:mm:ss");
        startTimeEdit_->setCalendarPopup(true);
        {
            QPalette pal = startTimeEdit_->palette();
            pal.setColor(QPalette::Base, surface);
            pal.setColor(QPalette::Text, text);
            pal.setColor(QPalette::WindowText, text);
            startTimeEdit_->setPalette(pal);
        }
        toolbarLayout->addWidget(startTimeEdit_);

        QLabel* toLabel = new QLabel("to");
        toLabel->setPalette([&]() {
            QPalette pal = toLabel->palette();
            pal.setColor(QPalette::WindowText, muted);
            return pal;
        }());
        toolbarLayout->addWidget(toLabel);

        endTimeEdit_ = new QDateTimeEdit(endTime_);
        endTimeEdit_->setDisplayFormat("yyyy-MM-dd hh:mm:ss");
        endTimeEdit_->setCalendarPopup(true);
        {
            QPalette pal = endTimeEdit_->palette();
            pal.setColor(QPalette::Base, surface);
            pal.setColor(QPalette::Text, text);
            pal.setColor(QPalette::WindowText, text);
            endTimeEdit_->setPalette(pal);
        }
        toolbarLayout->addWidget(endTimeEdit_);

        QLabel* contextLabel = new QLabel("Context:");
        contextLabel->setPalette([&]() {
            QPalette pal = contextLabel->palette();
            pal.setColor(QPalette::WindowText, muted);
            return pal;
        }());
        toolbarLayout->addWidget(contextLabel);

        contextFilterCombo_ = new QComboBox();
        contextFilterCombo_->addItem("All", QString());
        {
            QPalette pal = contextFilterCombo_->palette();
            pal.setColor(QPalette::Base, surface);
            pal.setColor(QPalette::Button, surface);
            pal.setColor(QPalette::Text, text);
            contextFilterCombo_->setPalette(pal);
        }
        toolbarLayout->addWidget(contextFilterCombo_);

        saveFiltersBtn_ = createToolButton("MaterialVS/colored/E3E3E3/save.svg", "Save current filters");
        toolbarLayout->addWidget(saveFiltersBtn_);

        loadFiltersBtn_ = createToolButton("MaterialVS/colored/E3E3E3/folder_open.svg", "Load saved filters");
        toolbarLayout->addWidget(loadFiltersBtn_);

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
        fontSizeSpin_->setToolTip(QStringLiteral("Debug console font size"));
        toolbarLayout->addWidget(fontSizeSpin_);

        debugFilterBtn_ = createToolButton("MaterialVS/colored/4CAF50/bug_report.svg", "Toggle Debug");
        debugFilterBtn_->setCheckable(true);
        debugFilterBtn_->setChecked(true);
        toolbarLayout->addWidget(debugFilterBtn_);

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

        auto* summaryLayout = new QHBoxLayout();
        summaryLayout->setContentsMargins(8, 0, 8, 4);
        summaryLayout->setSpacing(0);
        filterSummaryLabel_ = new QLabel();
        filterSummaryLabel_->setWordWrap(false);
        {
            QPalette pal = filterSummaryLabel_->palette();
            pal.setColor(QPalette::WindowText, muted);
            filterSummaryLabel_->setPalette(pal);
        }
        summaryLayout->addWidget(filterSummaryLabel_, 1);
        layout->addLayout(summaryLayout);

        statusLabel_ = new QLabel(owner_);
        statusLabel_->setPalette([&]() {
            QPalette pal = statusLabel_->palette();
            pal.setColor(QPalette::WindowText, muted);
            return pal;
        }());
        layout->addWidget(statusLabel_);

        auto* splitter = new QSplitter(Qt::Vertical, owner_);

        logList_ = new DebugConsoleLogListWidget();
        logList_->setAlternatingRowColors(true);
        logList_->setSelectionMode(QAbstractItemView::ExtendedSelection);
        logList_->setSelectionBehavior(QAbstractItemView::SelectRows);
        {
            QPalette pal = logList_->palette();
            pal.setColor(QPalette::Base, background);
            pal.setColor(QPalette::Window, background);
            pal.setColor(QPalette::Text, text);
            pal.setColor(QPalette::AlternateBase, background.darker(110));
            pal.setColor(QPalette::Highlight, accent);
            pal.setColor(QPalette::HighlightedText, QColor(QStringLiteral("#FFFFFF")));
            logList_->setPalette(pal);
        }
        splitter->addWidget(logList_);

        detailView_ = new QPlainTextEdit(owner_);
        detailView_->setReadOnly(true);
        detailView_->setMinimumHeight(110);
        {
            QPalette pal = detailView_->palette();
            pal.setColor(QPalette::Base, surface);
            pal.setColor(QPalette::Window, surface);
            pal.setColor(QPalette::Text, text);
            detailView_->setPalette(pal);
        }
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
            saveFilterPreset();
            updateStatus();
        });

        QObject::connect(autoScrollBtn_, &QToolButton::toggled, owner_, [this](bool checked) {
            autoScroll_ = checked;
            saveFilterPreset();
            updateStatus();
        });

        QObject::connect(collapseBtn_, &QToolButton::toggled, owner_, [this](bool checked) {
            collapse_ = checked;
            saveFilterPreset();
            refreshList();
        });

        QObject::connect(importantOnlyBtn_, &QToolButton::toggled, owner_, [this](bool checked) {
            importantOnly_ = checked;
            saveFilterPreset();
            refreshList();
        });

        QObject::connect(searchEdit_, &QLineEdit::textChanged, owner_, [this](const QString& text) {
            searchFilter_ = text;
            regexFilter_.setPattern(text);
            regexFilter_.setPatternOptions(QRegularExpression::CaseInsensitiveOption);
            saveFilterPreset();
            refreshList();
        });

        QObject::connect(timeFilterCheck_, &QCheckBox::toggled, owner_, [this](bool checked) {
            useTimeFilter_ = checked;
            saveFilterPreset();
            refreshList();
        });

        QObject::connect(startTimeEdit_, &QDateTimeEdit::dateTimeChanged, owner_, [this](const QDateTime& dt) {
            startTime_ = dt;
            saveFilterPreset();
            if (useTimeFilter_) refreshList();
        });

        QObject::connect(endTimeEdit_, &QDateTimeEdit::dateTimeChanged, owner_, [this](const QDateTime& dt) {
            endTime_ = dt;
            saveFilterPreset();
            if (useTimeFilter_) refreshList();
        });

        QObject::connect(contextFilterCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged), owner_, [this](int index) {
            QString selected = contextFilterCombo_->itemData(index).toString();
            useContextFilter_ = !selected.isEmpty();
            contextFilters_ = useContextFilter_ ? QStringList{selected} : QStringList{};
            saveFilterPreset();
            refreshList();
        });

        QObject::connect(saveFiltersBtn_, &QToolButton::clicked, owner_, [this]() {
            saveFilterPreset();
        });

        QObject::connect(loadFiltersBtn_, &QToolButton::clicked, owner_, [this]() {
            loadFilterPreset();
        });

        QObject::connect(debugFilterBtn_, &QToolButton::toggled, owner_, [this](bool checked) {
            showDebug_ = checked;
            saveFilterPreset();
            refreshList();
        });

        QObject::connect(infoFilterBtn_, &QToolButton::toggled, owner_, [this](bool checked) {
            showInfo_ = checked;
            saveFilterPreset();
            refreshList();
        });
        QObject::connect(warningFilterBtn_, &QToolButton::toggled, owner_, [this](bool checked) {
            showWarning_ = checked;
            saveFilterPreset();
            refreshList();
        });
        QObject::connect(errorFilterBtn_, &QToolButton::toggled, owner_, [this](bool checked) {
            showError_ = checked;
            saveFilterPreset();
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
            const QListWidgetItem* current = logList_->currentItem();
            const QString currentContext = current ? current->data(Qt::UserRole + 2).toString() : QString();

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

            if (!currentContext.isEmpty()) {
                menu.addSeparator();

                QAction* filterToContextAction = menu.addAction(QStringLiteral("Show Only Context: %1").arg(currentContext));
                QObject::connect(filterToContextAction, &QAction::triggered, owner_, [this, currentContext]() {
                    setContextFilter(QStringList{currentContext});
                });

                QAction* clearContextAction = menu.addAction("Show All Contexts");
                QObject::connect(clearContextAction, &QAction::triggered, owner_, [this]() {
                    clearContextFilter();
                });
            }

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
            totalDebugCount_ = 0;
            totalInfoCount_ = 0;
            totalWarningCount_ = 0;
            totalErrorCount_ = 0;
            updateFilterSummary();
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

        connectFontControls();
        applyFontPointSize(consoleFontPointSize_, false);
        loadFilterPreset();
    }

    QToolButton* createToolButton(const QString& iconPath, const QString& tooltip) {
        auto* btn = new QToolButton();
        btn->setIcon(loadIcon(iconPath));
        btn->setIconSize(QSize(18, 18));
        btn->setToolTip(tooltip);
        btn->setMinimumHeight(28);
        btn->setAutoRaise(true);
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
        const QVariant storedValue = settings.value(QString::fromLatin1(kFontPointSizeKey));
        const QVariant legacyStoredValue = settings.value(QString::fromLatin1(kLegacyFontPointSizeKey));
        const int stored = storedValue.isValid() ? storedValue.toInt() : legacyStoredValue.toInt();
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
        if (filterSummaryLabel_) filterSummaryLabel_->setFont(textFont);
        if (statusLabel_) statusLabel_->setFont(textFont);

        const QFont monospaceFont = monoFont();
        if (logList_) logList_->setFont(monospaceFont);
        if (detailView_) detailView_->setFont(monospaceFont);

        if (persist) {
            saveSettings();
        }
        updateStatus();
    }

    bool shouldShowLog(LogLevel level, const QString& message, const QDateTime& timestamp, const QString& context) const {
        if (!searchFilter_.isEmpty() && !regexFilter_.match(message).hasMatch()) {
            return false;
        }
        if (useTimeFilter_ && (timestamp < startTime_ || timestamp > endTime_)) {
            return false;
        }
        if (useContextFilter_ && !contextFilters_.contains(context)) {
            return false;
        }
        if (importantOnly_ && !(level == LogLevel::Warning || level == LogLevel::Error || level == LogLevel::Fatal)) {
            return false;
        }
        if (level == LogLevel::Debug) return showDebug_;
        if (level == LogLevel::Info) return showInfo_;
        if (level == LogLevel::Warning) return showWarning_;
        if (level == LogLevel::Error || level == LogLevel::Fatal) return showError_;
        return true;
    }

    std::vector<DisplayLogEntry> buildDisplayEntries() const {
        const auto logs = Logger::instance()->getLogs();
        std::vector<DisplayLogEntry> entries;
        entries.reserve(logs.size());
        for (const auto& log : logs) {
            if (!shouldShowLog(log.level, log.message, log.timestamp, log.context)) {
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
        totalDebugCount_ = 0;
        totalInfoCount_ = 0;
        totalWarningCount_ = 0;
        totalErrorCount_ = 0;
        availableContexts_.clear();
        const auto logs = Logger::instance()->getLogs();
        for (const auto& log : logs) {
            if (!log.context.isEmpty() && !availableContexts_.contains(log.context)) {
                availableContexts_.append(log.context);
            }
            if (log.level == LogLevel::Debug) {
                ++totalDebugCount_;
            } else if (log.level == LogLevel::Info) {
                ++totalInfoCount_;
            } else if (log.level == LogLevel::Warning) {
                ++totalWarningCount_;
            } else if (log.level == LogLevel::Error || log.level == LogLevel::Fatal) {
                ++totalErrorCount_;
            }
        }
        updateContextCombo();
    }

    void updateContextCombo() {
        if (!contextFilterCombo_) return;
        const QSignalBlocker blocker(contextFilterCombo_);
        contextFilterCombo_->clear();
        contextFilterCombo_->addItem("All", QString());
        for (const QString& context : availableContexts_) {
            contextFilterCombo_->addItem(context, context);
        }
    }

    void setContextFilter(const QStringList& contexts) {
        contextFilters_ = contexts;
        useContextFilter_ = !contextFilters_.isEmpty();
        if (contextFilterCombo_) {
            const QSignalBlocker blocker(contextFilterCombo_);
            if (useContextFilter_) {
                const int index = contextFilterCombo_->findData(contextFilters_.first());
                if (index >= 0) {
                    contextFilterCombo_->setCurrentIndex(index);
                }
            } else {
                contextFilterCombo_->setCurrentIndex(0);
            }
        }
        updateFilterSummary();
        refreshList();
    }

    void clearContextFilter() {
        setContextFilter(QStringList{});
    }

    void saveFilterPreset() {
        QSettings settings;
        settings.beginGroup("DebugConsoleFilters");
        settings.setValue("clearOnPlay", clearOnPlayCheck_ ? clearOnPlayCheck_->isChecked() : false);
        settings.setValue("autoScroll", autoScroll_);
        settings.setValue("collapse", collapse_);
        settings.setValue("showDebug", showDebug_);
        settings.setValue("showInfo", showInfo_);
        settings.setValue("showWarning", showWarning_);
        settings.setValue("showError", showError_);
        settings.setValue("useTimeFilter", useTimeFilter_);
        settings.setValue("startTime", startTime_);
        settings.setValue("endTime", endTime_);
        settings.setValue("useContextFilter", useContextFilter_);
        settings.setValue("contextFilters", contextFilters_);
        settings.setValue("importantOnly", importantOnly_);
        settings.setValue("searchFilter", searchFilter_);
        settings.endGroup();
        updateFilterSummary();
    }

    void loadFilterPreset() {
        QSettings settings;
        settings.beginGroup("DebugConsoleFilters");
        const auto legacy = [&settings](const char* key, const QVariant& fallback) -> QVariant {
            const QString legacyKey = QStringLiteral("ConsoleFilters/%1").arg(QString::fromLatin1(key));
            return settings.value(QString::fromLatin1(key), settings.value(legacyKey, fallback));
        };
        const bool clearOnPlay = legacy("clearOnPlay", false).toBool();
        autoScroll_ = legacy("autoScroll", true).toBool();
        collapse_ = legacy("collapse", true).toBool();
        showDebug_ = legacy("showDebug", true).toBool();
        showInfo_ = legacy("showInfo", true).toBool();
        showWarning_ = legacy("showWarning", true).toBool();
        showError_ = legacy("showError", true).toBool();
        useTimeFilter_ = legacy("useTimeFilter", false).toBool();
        startTime_ = legacy("startTime", QDateTime::currentDateTime().addSecs(-3600)).toDateTime();
        endTime_ = legacy("endTime", QDateTime::currentDateTime()).toDateTime();
        useContextFilter_ = legacy("useContextFilter", false).toBool();
        contextFilters_ = legacy("contextFilters", QStringList{}).toStringList();
        importantOnly_ = legacy("importantOnly", false).toBool();
        searchFilter_ = legacy("searchFilter", QString{}).toString();
        regexFilter_.setPattern(searchFilter_);
        regexFilter_.setPatternOptions(QRegularExpression::CaseInsensitiveOption);
        settings.endGroup();

        // Update UI
        if (clearOnPlayCheck_) {
            const QSignalBlocker blocker(clearOnPlayCheck_);
            clearOnPlayCheck_->setChecked(clearOnPlay);
        }
        if (autoScrollBtn_) {
            const QSignalBlocker blocker(autoScrollBtn_);
            autoScrollBtn_->setChecked(autoScroll_);
        }
        if (collapseBtn_) {
            const QSignalBlocker blocker(collapseBtn_);
            collapseBtn_->setChecked(collapse_);
        }
        if (debugFilterBtn_) {
            const QSignalBlocker blocker(debugFilterBtn_);
            debugFilterBtn_->setChecked(showDebug_);
        }
        if (infoFilterBtn_) {
            const QSignalBlocker blocker(infoFilterBtn_);
            infoFilterBtn_->setChecked(showInfo_);
        }
        if (warningFilterBtn_) {
            const QSignalBlocker blocker(warningFilterBtn_);
            warningFilterBtn_->setChecked(showWarning_);
        }
        if (errorFilterBtn_) {
            const QSignalBlocker blocker(errorFilterBtn_);
            errorFilterBtn_->setChecked(showError_);
        }
        if (timeFilterCheck_) {
            const QSignalBlocker blocker(timeFilterCheck_);
            timeFilterCheck_->setChecked(useTimeFilter_);
        }
        if (startTimeEdit_) {
            const QSignalBlocker blocker(startTimeEdit_);
            startTimeEdit_->setDateTime(startTime_);
        }
        if (endTimeEdit_) {
            const QSignalBlocker blocker(endTimeEdit_);
            endTimeEdit_->setDateTime(endTime_);
        }
        if (importantOnlyBtn_) {
            const QSignalBlocker blocker(importantOnlyBtn_);
            importantOnlyBtn_->setChecked(importantOnly_);
        }
        if (searchEdit_) {
            const QSignalBlocker blocker(searchEdit_);
            searchEdit_->setText(searchFilter_);
        }
        // Context filter update
        if (contextFilterCombo_) {
            const QSignalBlocker blocker(contextFilterCombo_);
            if (!contextFilters_.isEmpty()) {
                int index = contextFilterCombo_->findData(contextFilters_.first());
                if (index >= 0) contextFilterCombo_->setCurrentIndex(index);
            } else {
                contextFilterCombo_->setCurrentIndex(0);
            }
        }

        refreshList();
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
        updateFilterSummary();
        updateStatus();
    }

    void onLogAdded(LogLevel level, const QString& message, const QString& context, const QDateTime& ts) {
        if (level == LogLevel::Debug) {
            ++totalDebugCount_;
        } else if (level == LogLevel::Warning) {
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
        if (!shouldShowLog(level, message, ts, context)) {
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
            item->setForeground(QBrush(warningColor_));
        } else if (entry.level == LogLevel::Error || entry.level == LogLevel::Fatal) {
            item->setForeground(QBrush(dangerColor_));
        } else {
            item->setForeground(QBrush(infoColor_));
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
        const int totalCount = totalDebugCount_ + totalInfoCount_ + totalWarningCount_ + totalErrorCount_;
        statusLabel_->setText(
            QStringLiteral("Shown %1 / Total %2   Debug %3   Info %4   Warning %5   Error %6%7%8")
                .arg(static_cast<int>(displayEntries_.size()))
                .arg(totalCount)
                .arg(totalDebugCount_)
                .arg(totalInfoCount_)
                .arg(totalWarningCount_)
                .arg(totalErrorCount_)
                .arg(paused_ ? QStringLiteral("   Paused") : QString())
                .arg(pendingWhilePaused_ > 0 ? QStringLiteral(" (+%1 queued)").arg(pendingWhilePaused_) : QString()));
    }

    void updateFilterSummary() {
        if (!filterSummaryLabel_) {
            return;
        }

        QStringList parts;
        parts << QStringLiteral("Search: %1").arg(searchFilter_.isEmpty() ? QStringLiteral("off") : searchFilter_);
        parts << QStringLiteral("Time: %1").arg(useTimeFilter_
            ? QStringLiteral("%1 - %2").arg(startTime_.toString("yyyy-MM-dd hh:mm:ss"), endTime_.toString("yyyy-MM-dd hh:mm:ss"))
            : QStringLiteral("off"));
        parts << QStringLiteral("Context: %1").arg(useContextFilter_ && !contextFilters_.isEmpty() ? contextFilters_.join(", ") : QStringLiteral("all"));
        parts << QStringLiteral("Levels: %1%2%3%4")
            .arg(showDebug_ ? QStringLiteral("D") : QStringLiteral("-"))
            .arg(showInfo_ ? QStringLiteral("I") : QStringLiteral("-"))
            .arg(showWarning_ ? QStringLiteral("W") : QStringLiteral("-"))
            .arg(showError_ ? QStringLiteral("E") : QStringLiteral("-"));
        parts << QStringLiteral("Mode: %1").arg(importantOnly_ ? QStringLiteral("important-only") : QStringLiteral("all"));
        filterSummaryLabel_->setText(parts.join(QStringLiteral("   ")));
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
            QStringLiteral("Save Debug Console Logs"),
            QStringLiteral("artifact_debug_console_logs.txt"),
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

ArtifactDebugConsoleWidget::ArtifactDebugConsoleWidget(QWidget* parent)
    : QWidget(parent), impl_(new Impl(this)) {
    impl_->setupUI();
}

ArtifactDebugConsoleWidget::~ArtifactDebugConsoleWidget() {
    delete impl_;
}

int ArtifactDebugConsoleWidget::debugConsoleFontPointSize() const {
    return impl_ ? impl_->fontPointSize() : 12;
}

void ArtifactDebugConsoleWidget::setDebugConsoleFontPointSize(int pointSize) {
    if (!impl_) {
        return;
    }
    impl_->applyFontPointSize(pointSize, true);
}

} // namespace Artifact
