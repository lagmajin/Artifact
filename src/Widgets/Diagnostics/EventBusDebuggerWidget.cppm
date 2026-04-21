module;
#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>
#include <wobjectimpl.h>

#include <QAbstractItemView>
#include <QApplication>
#include <QBoxLayout>
#include <QCheckBox>
#include <QClipboard>
#include <QColor>
#include <QFile>
#include <QFileDialog>
#include <QFont>
#include <QFontMetrics>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QPainter>
#include <QPen>
#include <QPushButton>
#include <QRect>
#include <QResizeEvent>
#include <QScrollArea>
#include <QString>
#include <QTabWidget>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTextStream>
#include <QTimerEvent>
#include <QWidget>

module Artifact.Widgets.EventBusDebugger;

import ArtifactCore.Event.EventBusDebugger;
import Event.Bus;

namespace Artifact {

W_OBJECT_IMPL(EventBusDebuggerWidget)

// ---------------------------------------------------------------------------
// Palette
// ---------------------------------------------------------------------------
namespace {

const QColor kBg        {22,  22,  28,  242};
const QColor kBgHdr     {40,  40,  52,  255};
const QColor kBgRow0    {28,  28,  36,  255};
const QColor kBgRow1    {33,  33,  42,  255};
const QColor kBgDup     {60,  20,  20,  200};
const QColor kBgSlow    {60,  35,  10,  200};
const QColor kBgBurst   {45,  20,  60,  200};
const QColor kBgCard    {35,  35,  45,  255};
const QColor kBorderRed {200, 55,  55,  220};
const QColor kBorderNrm {60,  60,  80,  180};
const QColor kBarBlue   {80,  140, 220};
const QColor kBarRed    {220, 60,  60};
const QColor kTextH     {240, 240, 255};
const QColor kTextN     {195, 195, 210};
const QColor kTextD     {120, 120, 140};
const QColor kTextBadge {240, 80,  80};
const QColor kTagHigh   {220, 60,  60,  200};
const QColor kTagNormal {55,  130, 55,  200};

QString shortSourceFile(const char* fileName)
{
    if (!fileName || !*fileName) {
        return QStringLiteral("(unknown)");
    }
    QString path = QString::fromUtf8(fileName);
    const int slash = path.lastIndexOf('/');
    const int backslash = path.lastIndexOf('\\');
    const int pos = std::max(slash, backslash);
    return pos >= 0 ? path.mid(pos + 1) : path;
}

QString fireFlagsText(const ArtifactCore::FireEntry& entry)
{
    QString flags;
    if (entry.isDuplicate) flags += "D";
    if (entry.isSlow)  { if (!flags.isEmpty()) flags += "|"; flags += "S"; }
    if (entry.isBurst) { if (!flags.isEmpty()) flags += "|"; flags += "B"; }
    return flags;
}

QString fireSourceText(const ArtifactCore::FireEntry& entry)
{
    return QString("%1:%2")
        .arg(shortSourceFile(entry.origin.file_name()))
        .arg(static_cast<qulonglong>(entry.origin.line()));
}

QString fireSourceTooltip(const ArtifactCore::FireEntry& entry)
{
    const QString file = QString::fromUtf8(entry.origin.file_name() ? entry.origin.file_name() : "");
    const QString func = QString::fromUtf8(entry.origin.function_name() ? entry.origin.function_name() : "");
    return QString("%1\n%2:%3:%4")
        .arg(func.isEmpty() ? QStringLiteral("(unknown)") : func)
        .arg(file.isEmpty() ? QStringLiteral("(unknown)") : file)
        .arg(static_cast<qulonglong>(entry.origin.line()))
        .arg(static_cast<qulonglong>(entry.origin.column()));
}

} // namespace

// ===========================================================================
// Tab 2: Subscribers canvas
// ===========================================================================
class SubscriberCanvas : public QWidget {
public:
    explicit SubscriberCanvas(QWidget* parent = nullptr) : QWidget(parent) {}

    void setData(std::vector<ArtifactCore::SubscriberInfo> data)
    {
        data_ = std::move(data);
        const int cardH = 64;
        const int gap   = 6;
        setMinimumHeight(static_cast<int>(data_.size()) * (cardH + gap) + gap);
        update();
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        p.fillRect(rect(), kBg);

        const int cardH  = 64;
        const int gap    = 6;
        const int margin = 8;
        QFont fHdr  = font(); fHdr.setPointSize(9); fHdr.setBold(true);
        QFont fBody = font(); fBody.setPointSize(8);

        int y = gap;
        for (const auto& info : data_) {
            const QRect card(margin, y, width() - margin * 2, cardH);
            p.fillRect(card, kBgCard);

            const QColor borderClr = info.neverFired ? kBorderRed : kBorderNrm;
            p.setPen(QPen(borderClr, info.neverFired ? 2 : 1));
            p.drawRect(card);

            // Event name
            p.setFont(fHdr);
            p.setPen(kTextH);
            const QRect nameRect(card.left() + 8, card.top() + 5, card.width() - 16, 18);
            p.drawText(nameRect, Qt::AlignLeft | Qt::AlignVCenter,
                       QString::fromStdString(info.eventName));

            // Subs + fires
            p.setFont(fBody);
            p.setPen(kTextN);
            const QRect subRect(card.left() + 8, card.top() + 25,
                                (card.width() - 16) * 2 / 3, 14);
            p.drawText(subRect, Qt::AlignLeft | Qt::AlignVCenter,
                       QString("Subs: %1  |  Fires: %2")
                           .arg(static_cast<qulonglong>(info.activeCount))
                           .arg(static_cast<qulonglong>(info.totalFires)));

            // Avg duration
            if (info.avgDurationUs > 0) {
                const QRect durRect(card.left() + 8, card.top() + 41, card.width() - 16, 14);
                p.setPen(kTextD);
                p.drawText(durRect, Qt::AlignLeft | Qt::AlignVCenter,
                           QString("Avg: %1 µs").arg(info.avgDurationUs));
            }

            // Never-fired badge
            if (info.neverFired) {
                p.setFont(fBody);
                p.setPen(kTextBadge);
                p.drawText(subRect, Qt::AlignRight | Qt::AlignVCenter,
                           QStringLiteral("● never fired"));
            }

            y += cardH + gap;
        }
    }

private:
    std::vector<ArtifactCore::SubscriberInfo> data_;
};

// ===========================================================================
// Tab 3: Frequency canvas
// ===========================================================================
class FrequencyCanvas : public QWidget {
public:
    explicit FrequencyCanvas(QWidget* parent = nullptr) : QWidget(parent) {}

    void setData(std::vector<ArtifactCore::FrequencyEntry> data)
    {
        data_ = std::move(data);
        const int rowH = 28;
        const int gap  = 4;
        setMinimumHeight(static_cast<int>(data_.size()) * (rowH + gap) + gap + 24);
        update();
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.fillRect(rect(), kBg);

        QFont fHdr  = font(); fHdr.setPointSize(9); fHdr.setBold(true);
        QFont fBody = font(); fBody.setPointSize(8);

        // Column header
        p.setFont(fHdr);
        p.setPen(kTextD);
        p.fillRect(QRect(0, 0, width(), 22), kBgHdr);
        p.drawText(QRect(8,  2, 200, 18), Qt::AlignLeft  | Qt::AlignVCenter, "Event");
        p.drawText(QRect(width() - 175, 2, 60, 18), Qt::AlignRight | Qt::AlignVCenter, "total");
        p.drawText(QRect(width() - 110, 2, 60, 18), Qt::AlignRight | Qt::AlignVCenter, "fires/s");
        p.drawText(QRect(width() - 50,  2, 42, 18), Qt::AlignRight | Qt::AlignVCenter, "tag");

        const int rowH    = 28;
        const int gap     = 4;
        const int barLeft = 8;
        const int barMax  = width() - 200;
        const double maxFps = data_.empty() ? 1.0
            : std::max(1.0, data_.front().firesPerSec);

        int y = 24;
        for (std::size_t i = 0; i < data_.size(); ++i) {
            const auto& fe = data_[i];
            const QRect row(0, y, width(), rowH);
            p.fillRect(row, (i % 2 == 0) ? kBgRow0 : kBgRow1);

            // Bar
            const int barW = static_cast<int>(barMax * (fe.firesPerSec / maxFps));
            const QColor barClr = fe.isHighFreq ? kBarRed : kBarBlue;
            p.fillRect(QRect(barLeft, y + 6, std::max(2, barW), rowH - 12), barClr);

            // Name
            p.setFont(fBody);
            p.setPen(kTextN);
            p.drawText(QRect(barLeft, y, 200, rowH), Qt::AlignLeft | Qt::AlignVCenter,
                       QString::fromStdString(fe.eventName));

            // Total fires
            p.setPen(kTextD);
            p.drawText(QRect(width() - 175, y, 60, rowH),
                       Qt::AlignRight | Qt::AlignVCenter,
                       QString::number(static_cast<qulonglong>(fe.totalFires)));

            // fires/s value
            p.setPen(kTextH);
            p.drawText(QRect(width() - 110, y, 60, rowH),
                       Qt::AlignRight | Qt::AlignVCenter,
                       QString::number(fe.firesPerSec, 'f', 1));

            // HIGH / OK badge
            const QRect badge(width() - 48, y + 6, 42, rowH - 12);
            if (fe.isHighFreq) {
                p.fillRect(badge, kTagHigh);
                p.setFont(fHdr);
                p.setPen(Qt::white);
                p.drawText(badge, Qt::AlignCenter, "HIGH");
            } else {
                p.fillRect(badge, kTagNormal);
                p.setFont(fHdr);
                p.setPen(Qt::white);
                p.drawText(badge, Qt::AlignCenter, "OK");
            }

            y += rowH + gap;
        }
    }

private:
    std::vector<ArtifactCore::FrequencyEntry> data_;
};

// ===========================================================================
// Impl
// ===========================================================================
class EventBusDebuggerWidget::Impl {
public:
    QLabel*        overviewBar   = nullptr;
    QLabel*        legendBar     = nullptr;

    // Tab 1 — Fire Log
    QTableWidget*  logTable      = nullptr;
    QLineEdit*     nameFilter    = nullptr;
    QCheckBox*     dupesOnly     = nullptr;
    QCheckBox*     slowOnly      = nullptr;
    QCheckBox*     burstOnly     = nullptr;
    QCheckBox*     pauseCk       = nullptr;
    QCheckBox*     scrollLockCk  = nullptr;
    QPushButton*   clearBtn      = nullptr;
    QPushButton*   copyBtn       = nullptr;
    QPushButton*   saveCsvBtn    = nullptr;

    // Tab 2
    SubscriberCanvas* subCanvas  = nullptr;

    // Tab 3
    FrequencyCanvas*  freqCanvas = nullptr;

    // Tab 4 — Per-Event Stats
    QTableWidget*  statsTable    = nullptr;

    // Status bar
    QLabel*        statusBar     = nullptr;

    QTabWidget* tabs    = nullptr;
    int         timerId = 0;
    std::vector<ArtifactCore::FireEntry> visibleLogEntries;

    void copySelectedLogRowsToClipboard() const;
};

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------
EventBusDebuggerWidget::EventBusDebuggerWidget(QWidget* parent)
    : QWidget(parent, Qt::Tool | Qt::Window),
      impl_(new Impl())
{
    setAttribute(Qt::WA_DeleteOnClose, false);
    setWindowTitle("EventBus Debugger");
    resize(720, 540);

    ArtifactCore::EventBusDebugger::instance().attach(ArtifactCore::globalEventBus());

    // ---------- Tab 1: Fire Log ----------
    auto* tab1 = new QWidget(this);
    {
        impl_->logTable = new QTableWidget(0, 6, tab1);
        impl_->logTable->setHorizontalHeaderLabels(
            {"Time (ms)", "Event", "Subs", "Dur µs", "Flags", "Source"});
        impl_->logTable->horizontalHeader()->setStretchLastSection(false);
        impl_->logTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
        impl_->logTable->setSelectionBehavior(QAbstractItemView::SelectRows);
        impl_->logTable->setSelectionMode(QAbstractItemView::ExtendedSelection);
        impl_->logTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
        impl_->logTable->setAlternatingRowColors(true);
        impl_->logTable->verticalHeader()->hide();
        impl_->logTable->setColumnWidth(0, 80);
        impl_->logTable->setColumnWidth(2, 40);
        impl_->logTable->setColumnWidth(3, 65);
        impl_->logTable->setColumnWidth(4, 58);
        impl_->logTable->setColumnWidth(5, 180);
        impl_->logTable->setContextMenuPolicy(Qt::CustomContextMenu);

        impl_->nameFilter   = new QLineEdit(tab1);
        impl_->nameFilter->setPlaceholderText("Filter event name…");
        impl_->nameFilter->setFixedHeight(24);

        impl_->dupesOnly    = new QCheckBox("Dupes only",  tab1);
        impl_->slowOnly     = new QCheckBox("Slow only",   tab1);
        impl_->burstOnly    = new QCheckBox("Burst only",  tab1);
        impl_->pauseCk      = new QCheckBox("Pause",      tab1);
        impl_->scrollLockCk = new QCheckBox("Lock scroll", tab1);

        impl_->clearBtn   = new QPushButton("Clear",   tab1);
        impl_->copyBtn    = new QPushButton("Copy",    tab1);
        impl_->saveCsvBtn = new QPushButton("CSV…",   tab1);
        impl_->clearBtn->setFixedWidth(50);
        impl_->copyBtn->setFixedWidth(50);
        impl_->saveCsvBtn->setFixedWidth(50);

        auto* toolbar = new QHBoxLayout();
        toolbar->setContentsMargins(0, 0, 0, 0);
        toolbar->setSpacing(5);
        toolbar->addWidget(impl_->nameFilter, 1);
        toolbar->addWidget(impl_->dupesOnly);
        toolbar->addWidget(impl_->slowOnly);
        toolbar->addWidget(impl_->burstOnly);
        toolbar->addWidget(impl_->pauseCk);
        toolbar->addWidget(impl_->scrollLockCk);
        toolbar->addStretch();
        toolbar->addWidget(impl_->clearBtn);
        toolbar->addWidget(impl_->copyBtn);
        toolbar->addWidget(impl_->saveCsvBtn);

        auto* layout = new QVBoxLayout(tab1);
        layout->setContentsMargins(4, 4, 4, 4);
        layout->setSpacing(4);
        impl_->overviewBar = new QLabel(tab1);
        impl_->overviewBar->setWordWrap(true);
        impl_->overviewBar->setText(
            "Overview: waiting for event data");
        impl_->legendBar = new QLabel(tab1);
        impl_->legendBar->setWordWrap(true);
        impl_->legendBar->setText(
            "Legend: D=duplicate S=slow B=burst  |  red border = never fired");
        layout->addLayout(toolbar);
        layout->addWidget(impl_->overviewBar);
        layout->addWidget(impl_->legendBar);
        layout->addWidget(impl_->logTable, 1);

        QObject::connect(impl_->pauseCk, &QCheckBox::toggled, tab1, [](bool checked) {
            ArtifactCore::EventBusDebugger::instance().setPaused(checked);
        });
        QObject::connect(impl_->clearBtn, &QPushButton::clicked, tab1, []() {
            ArtifactCore::EventBusDebugger::instance().clearLog();
        });
        QObject::connect(impl_->copyBtn, &QPushButton::clicked, tab1, [this]() {
            const auto& entries = (!impl_->visibleLogEntries.empty())
                ? impl_->visibleLogEntries
                : ArtifactCore::EventBusDebugger::instance().fireLog();
            QString text = "Time(ms)\tEvent\tSubs\tDur µs\tFlags\tSource\n";
            for (const auto& e : entries) {
                text += QString("%1\t%2\t%3\t%4\t%5\t%6\n")
                    .arg(QString::number(e.timestampMs, 'f', 1))
                    .arg(QString::fromStdString(e.eventName))
                    .arg(static_cast<int>(e.subscriberCount))
                    .arg(e.durationUs)
                    .arg(fireFlagsText(e))
                    .arg(fireSourceText(e));
            }
            QApplication::clipboard()->setText(text);
        });
        QObject::connect(impl_->logTable, &QTableWidget::customContextMenuRequested, tab1,
                         [this](const QPoint& pos) {
                             if (!impl_ || !impl_->logTable) {
                                 return;
                             }
                             QMenu menu(impl_->logTable);
                             QAction* copySelected = menu.addAction(QStringLiteral("Copy selected rows"));
                             QAction* copyAll = menu.addAction(QStringLiteral("Copy all rows"));
                             QAction* chosen = menu.exec(impl_->logTable->viewport()->mapToGlobal(pos));
                             if (chosen == copySelected) {
                                impl_->copySelectedLogRowsToClipboard();
                            } else if (chosen == copyAll) {
                                const auto entries = ArtifactCore::EventBusDebugger::instance().fireLog();
                                QString text = "Time(ms)\tEvent\tSubs\tDur µs\tFlags\tSource\n";
                                for (const auto& e : entries) {
                                    text += QString("%1\t%2\t%3\t%4\t%5\t%6\n")
                                                .arg(QString::number(e.timestampMs, 'f', 1))
                                                .arg(QString::fromStdString(e.eventName))
                                                .arg(static_cast<int>(e.subscriberCount))
                                                .arg(e.durationUs)
                                                .arg(fireFlagsText(e))
                                                .arg(fireSourceText(e));
                                }
                                QApplication::clipboard()->setText(text);
                            }
                         });
        QObject::connect(impl_->saveCsvBtn, &QPushButton::clicked, this, [this]() {
            const QString path = QFileDialog::getSaveFileName(
                this, "Save Fire Log", QString(), "CSV files (*.csv);;All files (*)");
            if (path.isEmpty()) return;
            QFile file(path);
            if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) return;
            QTextStream out(&file);
            out << "Time(ms),Event,Subscribers,Duration_us,Flags,Source\n";
            const auto entries = ArtifactCore::EventBusDebugger::instance().fireLog();
            for (const auto& e : entries) {
                out << QString::number(e.timestampMs, 'f', 1) << ','
                    << QString::fromStdString(e.eventName) << ','
                    << static_cast<int>(e.subscriberCount) << ','
                    << e.durationUs << ','
                    << fireFlagsText(e) << ','
                    << fireSourceText(e) << '\n';
            }
        });
    }

    // ---------- Tab 2: Subscribers ----------
    auto* tab2 = new QWidget(this);
    {
        impl_->subCanvas = new SubscriberCanvas();

        auto* scrollArea = new QScrollArea(tab2);
        scrollArea->setWidget(impl_->subCanvas);
        scrollArea->setWidgetResizable(true);
        scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

        auto* layout = new QVBoxLayout(tab2);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->addWidget(scrollArea);
    }

    // ---------- Tab 3: Frequency ----------
    auto* tab3 = new QWidget(this);
    {
        impl_->freqCanvas = new FrequencyCanvas();

        auto* scrollArea = new QScrollArea(tab3);
        scrollArea->setWidget(impl_->freqCanvas);
        scrollArea->setWidgetResizable(true);
        scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

        auto* layout = new QVBoxLayout(tab3);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->addWidget(scrollArea);
    }

    // ---------- Tab 4: Per-Event Stats ----------
    auto* tab4 = new QWidget(this);
    {
        impl_->statsTable = new QTableWidget(0, 6, tab4);
        impl_->statsTable->setHorizontalHeaderLabels(
            {"Event", "Total", "fires/s", "Avg µs", "Max µs", "Min µs"});
        impl_->statsTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
        impl_->statsTable->horizontalHeader()->setStretchLastSection(false);
        impl_->statsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
        impl_->statsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
        impl_->statsTable->setAlternatingRowColors(true);
        impl_->statsTable->verticalHeader()->hide();
        impl_->statsTable->setColumnWidth(1, 55);
        impl_->statsTable->setColumnWidth(2, 60);
        impl_->statsTable->setColumnWidth(3, 65);
        impl_->statsTable->setColumnWidth(4, 65);
        impl_->statsTable->setColumnWidth(5, 65);

        auto* layout = new QVBoxLayout(tab4);
        layout->setContentsMargins(4, 4, 4, 4);
        layout->addWidget(impl_->statsTable, 1);
    }

    // ---------- Status bar ----------
    impl_->statusBar = new QLabel(this);
    {
        QFont f = impl_->statusBar->font();
        f.setPointSize(7);
        impl_->statusBar->setFont(f);
        impl_->statusBar->setContentsMargins(6, 2, 6, 2);
        impl_->statusBar->setText("—");
    }

    // ---------- Assemble ----------
    impl_->tabs = new QTabWidget(this);
    impl_->tabs->addTab(tab1, "Fire Log");
    impl_->tabs->addTab(tab2, "Subscribers");
    impl_->tabs->addTab(tab3, "Frequency");
    impl_->tabs->addTab(tab4, "Stats");

    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);
    rootLayout->addWidget(impl_->tabs, 1);
    rootLayout->addWidget(impl_->statusBar);

    impl_->timerId = startTimer(200);
}

EventBusDebuggerWidget::~EventBusDebuggerWidget()
{
    if (impl_->timerId != 0) killTimer(impl_->timerId);
    ArtifactCore::EventBusDebugger::instance().detach();
    delete impl_;
}

// ---------------------------------------------------------------------------
// timerEvent — refresh active tab + status bar
// ---------------------------------------------------------------------------
void EventBusDebuggerWidget::timerEvent(QTimerEvent*)
{
    if (!isVisible()) return;

    // Always refresh the status bar
    {
        const auto gs = ArtifactCore::EventBusDebugger::instance().globalStats();
        QString overview = QString("Events: %1  |  Rate: %2/s  |  Uptime: %3s")
            .arg(static_cast<qulonglong>(gs.totalEventsFired))
            .arg(gs.overallEventsPerSec, 0, 'f', 1)
            .arg(gs.uptimeSec, 0, 'f', 1);
        if (!gs.slowestEventName.empty()) {
            overview += QString("  |  Slowest: %1 (%2 µs)")
                .arg(QString::fromStdString(gs.slowestEventName))
                .arg(gs.slowestMaxUs);
        }
        if (impl_->overviewBar) {
            impl_->overviewBar->setText(overview);
        }

        if (impl_->legendBar) {
            const auto subs = ArtifactCore::EventBusDebugger::instance().subscriberSnapshot();
            std::size_t neverFired = 0;
            for (const auto& s : subs) {
                if (s.neverFired) {
                    ++neverFired;
                }
            }
            const auto freq = ArtifactCore::EventBusDebugger::instance().frequencySnapshot();
            std::size_t highFreq = 0;
            for (const auto& f : freq) {
                if (f.isHighFreq) {
                    ++highFreq;
                }
            }
            impl_->legendBar->setText(QString("Never fired: %1  |  High freq: %2")
                                          .arg(static_cast<qulonglong>(neverFired))
                                          .arg(static_cast<qulonglong>(highFreq)));
        }
    }

    const int tab = impl_->tabs->currentIndex();

    if (tab == 0) {
        // Fire Log
        const bool filterDupes = impl_->dupesOnly->isChecked();
        const bool filterSlow  = impl_->slowOnly->isChecked();
        const bool filterBurst = impl_->burstOnly->isChecked();
        const QString filter   = impl_->nameFilter->text().toLower();

        auto entries = ArtifactCore::EventBusDebugger::instance().fireLog(filterDupes);

        if (filterSlow) {
            entries.erase(std::remove_if(entries.begin(), entries.end(),
                [](const ArtifactCore::FireEntry& e) { return !e.isSlow; }), entries.end());
        }
        if (filterBurst) {
            entries.erase(std::remove_if(entries.begin(), entries.end(),
                [](const ArtifactCore::FireEntry& e) { return !e.isBurst; }), entries.end());
        }
        if (!filter.isEmpty()) {
            entries.erase(std::remove_if(entries.begin(), entries.end(),
                [&](const ArtifactCore::FireEntry& e) {
                    return !QString::fromStdString(e.eventName).toLower().contains(filter);
                }), entries.end());
        }

        impl_->visibleLogEntries = entries;

        const int rowCount = static_cast<int>(entries.size());
        impl_->logTable->setRowCount(rowCount);
        for (int i = 0; i < rowCount; ++i) {
            const auto& e = entries[static_cast<std::size_t>(i)];

            QString flags;
            if (e.isDuplicate) flags += "D";
            if (e.isSlow)  { if (!flags.isEmpty()) flags += " "; flags += "S"; }
            if (e.isBurst) { if (!flags.isEmpty()) flags += " "; flags += "B"; }

            impl_->logTable->setItem(i, 0, new QTableWidgetItem(
                QString::number(e.timestampMs, 'f', 1)));
            impl_->logTable->setItem(i, 1, new QTableWidgetItem(
                QString::fromStdString(e.eventName)));
            impl_->logTable->setItem(i, 2, new QTableWidgetItem(
                QString::number(static_cast<int>(e.subscriberCount))));
            impl_->logTable->setItem(i, 3, new QTableWidgetItem(
                QString::number(e.durationUs)));
            impl_->logTable->setItem(i, 4, new QTableWidgetItem(flags));
            auto* sourceItem = new QTableWidgetItem(fireSourceText(e));
            sourceItem->setToolTip(fireSourceTooltip(e));
            impl_->logTable->setItem(i, 5, sourceItem);

            // Row color: burst > dup > slow
            const bool hasBgColor = e.isBurst || e.isDuplicate || e.isSlow;
            if (hasBgColor) {
                const QColor& bg = e.isBurst ? kBgBurst
                                 : e.isDuplicate ? kBgDup
                                 : kBgSlow;
                for (int c = 0; c < 6; ++c) {
                    if (auto* item = impl_->logTable->item(i, c))
                        item->setBackground(bg);
                }
            }
        }

        if (!impl_->scrollLockCk->isChecked() && rowCount > 0)
            impl_->logTable->scrollToBottom();

    } else if (tab == 1) {
        auto subs = ArtifactCore::EventBusDebugger::instance().subscriberSnapshot();
        impl_->subCanvas->setData(std::move(subs));

    } else if (tab == 2) {
        auto freq = ArtifactCore::EventBusDebugger::instance().frequencySnapshot();
        impl_->freqCanvas->setData(std::move(freq));

    } else if (tab == 3) {
        const auto stats = ArtifactCore::EventBusDebugger::instance().perEventStats();
        const int rowCount = static_cast<int>(stats.size());
        impl_->statsTable->setRowCount(rowCount);
        for (int i = 0; i < rowCount; ++i) {
            const auto& s = stats[static_cast<std::size_t>(i)];
            impl_->statsTable->setItem(i, 0, new QTableWidgetItem(
                QString::fromStdString(s.eventName)));
            impl_->statsTable->setItem(i, 1, new QTableWidgetItem(
                QString::number(static_cast<qulonglong>(s.totalFires))));
            impl_->statsTable->setItem(i, 2, new QTableWidgetItem(
                QString::number(s.firesPerSec, 'f', 1)));
            impl_->statsTable->setItem(i, 3, new QTableWidgetItem(
                QString::number(s.avgDurationUs)));
            impl_->statsTable->setItem(i, 4, new QTableWidgetItem(
                QString::number(s.maxDurationUs)));
            impl_->statsTable->setItem(i, 5, new QTableWidgetItem(
                QString::number(s.minDurationUs)));
            if (s.isSlowAvg) {
                for (int c = 3; c <= 5; ++c) {
                    if (auto* item = impl_->statsTable->item(i, c))
                        item->setBackground(kBgSlow);
                }
            }
        }
    }
}

void EventBusDebuggerWidget::closeEvent(QCloseEvent* event)
{
    QWidget::closeEvent(event);
}

void EventBusDebuggerWidget::Impl::copySelectedLogRowsToClipboard() const
{
    if (!logTable) {
        return;
    }

    const auto selected = logTable->selectionModel()
                              ? logTable->selectionModel()->selectedRows()
                              : QList<QModelIndex>{};
    if (selected.isEmpty()) {
        return;
    }

    std::vector<int> rows;
    rows.reserve(selected.size());
    for (const auto& index : selected) {
        rows.push_back(index.row());
    }
    std::sort(rows.begin(), rows.end());

    const auto& entries = visibleLogEntries.empty()
        ? ArtifactCore::EventBusDebugger::instance().fireLog()
        : visibleLogEntries;
    QString text = "Time(ms)\tEvent\tSubs\tDur µs\tFlags\tSource\n";
    for (int row : rows) {
        if (row < 0 || row >= static_cast<int>(entries.size())) {
            continue;
        }
        const auto& e = entries[static_cast<std::size_t>(row)];
        text += QString("%1\t%2\t%3\t%4\t%5\t%6\n")
                    .arg(QString::number(e.timestampMs, 'f', 1))
                    .arg(QString::fromStdString(e.eventName))
                    .arg(static_cast<int>(e.subscriberCount))
                    .arg(e.durationUs)
                    .arg(fireFlagsText(e))
                    .arg(fireSourceText(e));
    }
    QApplication::clipboard()->setText(text);
}

} // namespace Artifact
