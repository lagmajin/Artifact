module;
#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>
#include <wobjectimpl.h>

#include <QAbstractItemView>
#include <QBoxLayout>
#include <QCheckBox>
#include <QColor>
#include <QFont>
#include <QFontMetrics>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QPushButton>
#include <QRect>
#include <QResizeEvent>
#include <QScrollArea>
#include <QString>
#include <QTabWidget>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTimerEvent>
#include <QWidget>

module Artifact.Widgets.EventBusDebugger;

import ArtifactCore.Event.EventBusDebugger;
import ArtifactCore.Event.Bus;

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
        const int cardH = 48;
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

        const int cardH  = 48;
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
            const QRect nameRect(card.left() + 8, card.top() + 6, card.width() - 16, 18);
            p.drawText(nameRect, Qt::AlignLeft | Qt::AlignVCenter,
                       QString::fromStdString(info.eventName));

            // Sub count
            p.setFont(fBody);
            p.setPen(kTextN);
            const QRect subRect(card.left() + 8, card.top() + 26, card.width() - 16, 16);
            p.drawText(subRect, Qt::AlignLeft | Qt::AlignVCenter,
                       QString("Subscribers: %1").arg(info.activeCount));

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
        setMinimumHeight(static_cast<int>(data_.size()) * (rowH + gap) + gap + 24 /*header*/);
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

            // fires/s value
            p.setPen(kTextH);
            p.drawText(QRect(width() - 110, y, 60, rowH),
                       Qt::AlignRight | Qt::AlignVCenter,
                       QString::number(fe.firesPerSec, 'f', 1));

            // HIGH / OK badge
            if (fe.isHighFreq) {
                const QRect badge(width() - 48, y + 6, 42, rowH - 12);
                p.fillRect(badge, kTagHigh);
                p.setFont(fHdr);
                p.setPen(Qt::white);
                p.drawText(badge, Qt::AlignCenter, "HIGH");
            } else {
                const QRect badge(width() - 48, y + 6, 42, rowH - 12);
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
    // Tab 1 — Fire Log
    QTableWidget*  logTable   = nullptr;
    QLineEdit*     nameFilter = nullptr;
    QCheckBox*     dupesOnly  = nullptr;
    QCheckBox*     pauseCk    = nullptr;
    QPushButton*   clearBtn   = nullptr;

    // Tab 2
    SubscriberCanvas* subCanvas = nullptr;

    // Tab 3
    FrequencyCanvas* freqCanvas = nullptr;

    QTabWidget* tabs = nullptr;
    int         timerId = 0;
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
    resize(640, 480);

    // Attach debugger
    ArtifactCore::EventBusDebugger::instance().attach(ArtifactCore::globalEventBus());

    // ---------- Tab 1: Fire Log ----------
    auto* tab1 = new QWidget(this);
    {
        impl_->logTable = new QTableWidget(0, 5, tab1);
        impl_->logTable->setHorizontalHeaderLabels(
            {"Time (ms)", "Event", "Subs", "Dur µs", "DUP"});
        impl_->logTable->horizontalHeader()->setStretchLastSection(false);
        impl_->logTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
        impl_->logTable->setSelectionBehavior(QAbstractItemView::SelectRows);
        impl_->logTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
        impl_->logTable->setAlternatingRowColors(true);
        impl_->logTable->verticalHeader()->hide();
        impl_->logTable->setColumnWidth(0, 80);
        impl_->logTable->setColumnWidth(2, 40);
        impl_->logTable->setColumnWidth(3, 60);
        impl_->logTable->setColumnWidth(4, 36);

        impl_->nameFilter = new QLineEdit(tab1);
        impl_->nameFilter->setPlaceholderText("Filter event name…");
        impl_->nameFilter->setFixedHeight(24);

        impl_->dupesOnly = new QCheckBox("Dupes only", tab1);
        impl_->pauseCk   = new QCheckBox("Pause", tab1);
        impl_->clearBtn  = new QPushButton("Clear", tab1);
        impl_->clearBtn->setFixedWidth(60);

        auto* toolbar = new QHBoxLayout();
        toolbar->setContentsMargins(0, 0, 0, 0);
        toolbar->setSpacing(6);
        toolbar->addWidget(impl_->nameFilter, 1);
        toolbar->addWidget(impl_->dupesOnly);
        toolbar->addWidget(impl_->pauseCk);
        toolbar->addWidget(impl_->clearBtn);

        auto* layout = new QVBoxLayout(tab1);
        layout->setContentsMargins(4, 4, 4, 4);
        layout->setSpacing(4);
        layout->addLayout(toolbar);
        layout->addWidget(impl_->logTable, 1);

        // Wire pause checkbox → debugger (Qt internal connection – local widget only)
        QObject::connect(impl_->pauseCk, &QCheckBox::toggled, tab1, [](bool checked) {
            ArtifactCore::EventBusDebugger::instance().setPaused(checked);
        });
        // Clear button
        QObject::connect(impl_->clearBtn, &QPushButton::clicked, tab1, []() {
            ArtifactCore::EventBusDebugger::instance().clearLog();
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

    // ---------- Assemble ----------
    impl_->tabs = new QTabWidget(this);
    impl_->tabs->addTab(tab1, "Fire Log");
    impl_->tabs->addTab(tab2, "Subscribers");
    impl_->tabs->addTab(tab3, "Frequency");

    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->addWidget(impl_->tabs);

    impl_->timerId = startTimer(200);
}

EventBusDebuggerWidget::~EventBusDebuggerWidget()
{
    if (impl_->timerId != 0) killTimer(impl_->timerId);
    ArtifactCore::EventBusDebugger::instance().detach();
    delete impl_;
}

// ---------------------------------------------------------------------------
// timerEvent — refresh active tab
// ---------------------------------------------------------------------------
void EventBusDebuggerWidget::timerEvent(QTimerEvent*)
{
    if (!isVisible()) return;
    const int tab = impl_->tabs->currentIndex();

    if (tab == 0) {
        // Fire Log
        const bool dupesOnly = impl_->dupesOnly->isChecked();
        const QString filter = impl_->nameFilter->text().toLower();
        auto entries = ArtifactCore::EventBusDebugger::instance().fireLog(dupesOnly);

        // Filter by name
        if (!filter.isEmpty()) {
            entries.erase(std::remove_if(entries.begin(), entries.end(),
                [&](const ArtifactCore::FireEntry& e) {
                    return !QString::fromStdString(e.eventName).toLower().contains(filter);
                }), entries.end());
        }

        // Only repopulate if count changed (minor perf guard)
        const int rowCount = static_cast<int>(entries.size());
        impl_->logTable->setRowCount(rowCount);
        for (int i = 0; i < rowCount; ++i) {
            const auto& e = entries[static_cast<std::size_t>(i)];
            impl_->logTable->setItem(i, 0, new QTableWidgetItem(
                QString::number(e.timestampMs, 'f', 1)));
            impl_->logTable->setItem(i, 1, new QTableWidgetItem(
                QString::fromStdString(e.eventName)));
            impl_->logTable->setItem(i, 2, new QTableWidgetItem(
                QString::number(static_cast<int>(e.subscriberCount))));
            impl_->logTable->setItem(i, 3, new QTableWidgetItem(
                QString::number(e.durationUs)));
            impl_->logTable->setItem(i, 4, new QTableWidgetItem(
                e.isDuplicate ? "●" : ""));
            if (e.isDuplicate) {
                for (int c = 0; c < 5; ++c) {
                    if (auto* item = impl_->logTable->item(i, c)) {
                        item->setBackground(kBgDup);
                    }
                }
            }
        }
        // Scroll to bottom
        if (rowCount > 0) impl_->logTable->scrollToBottom();

    } else if (tab == 1) {
        // Subscribers
        auto subs = ArtifactCore::EventBusDebugger::instance().subscriberSnapshot();
        impl_->subCanvas->setData(std::move(subs));

    } else if (tab == 2) {
        // Frequency
        auto freq = ArtifactCore::EventBusDebugger::instance().frequencySnapshot();
        impl_->freqCanvas->setData(std::move(freq));
    }
}

void EventBusDebuggerWidget::closeEvent(QCloseEvent* event)
{
    QWidget::closeEvent(event);
}

} // namespace Artifact
