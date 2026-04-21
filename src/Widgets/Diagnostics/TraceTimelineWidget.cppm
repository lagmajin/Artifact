module;
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <QColor>
#include <QHeaderView>
#include <QLabel>
#include <QHash>
#include <QPaintEvent>
#include <QPainter>
#include <QPlainTextEdit>
#include <QSplitter>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QVector>
#include <QStringList>
#include <vector>
#include <wobjectimpl.h>

module Artifact.Widgets.TraceTimelineWidget;

namespace Artifact {

W_OBJECT_IMPL(TraceTimelineWidget)

static QString traceDomainName(ArtifactCore::TraceDomain domain)
{
    switch (domain) {
    case ArtifactCore::TraceDomain::Crash: return QStringLiteral("Crash");
    case ArtifactCore::TraceDomain::Scope: return QStringLiteral("Scope");
    case ArtifactCore::TraceDomain::Timeline: return QStringLiteral("Timeline");
    case ArtifactCore::TraceDomain::Thread: return QStringLiteral("Thread");
    case ArtifactCore::TraceDomain::Event: return QStringLiteral("Event");
    case ArtifactCore::TraceDomain::Render: return QStringLiteral("Render");
    case ArtifactCore::TraceDomain::Decode: return QStringLiteral("Decode");
    case ArtifactCore::TraceDomain::UI: return QStringLiteral("UI");
    }
    return QStringLiteral("Scope");
}

static QColor traceDomainColor(ArtifactCore::TraceDomain domain)
{
    switch (domain) {
    case ArtifactCore::TraceDomain::Render: return QColor(110, 150, 255);
    case ArtifactCore::TraceDomain::Decode: return QColor(110, 220, 150);
    case ArtifactCore::TraceDomain::UI: return QColor(255, 170, 80);
    case ArtifactCore::TraceDomain::Event: return QColor(235, 120, 165);
    case ArtifactCore::TraceDomain::Thread: return QColor(150, 150, 150);
    case ArtifactCore::TraceDomain::Crash: return QColor(255, 90, 90);
    case ArtifactCore::TraceDomain::Timeline: return QColor(180, 120, 255);
    case ArtifactCore::TraceDomain::Scope:
    default: return QColor(180, 180, 180);
    }
}

class TraceTimelineCanvas : public QWidget {
public:
    explicit TraceTimelineCanvas(QWidget* parent = nullptr)
        : QWidget(parent)
    {
        setMinimumHeight(220);
    }

    void setSnapshot(const ArtifactCore::TraceSnapshot& snapshot)
    {
        snapshot_ = snapshot;
        update();
    }

    void setFocusedThreadId(std::uint64_t threadId)
    {
        focusThreadId_ = threadId;
        update();
    }

    void setFocusedMutexName(const QString& mutexName)
    {
        focusMutexName_ = mutexName;
        update();
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter painter(this);
        painter.fillRect(rect(), QColor(22, 22, 28));
        painter.setRenderHint(QPainter::Antialiasing, true);

        const QRect content = rect().adjusted(8, 8, -8, -8);
        painter.setPen(QColor(60, 60, 80));
        painter.drawRect(content);

        if (snapshot_.events.isEmpty()) {
            painter.setPen(QColor(190, 190, 200));
            painter.drawText(content, Qt::AlignCenter, QStringLiteral("No trace events yet"));
            return;
        }

        qint64 minNs = snapshot_.events.first().startNs;
        qint64 maxNs = snapshot_.events.first().endNs;
        for (const auto& event : snapshot_.events) {
            minNs = std::min(minNs, event.startNs);
            maxNs = std::max(maxNs, std::max(event.startNs, event.endNs));
        }
        const qint64 spanNs = std::max<qint64>(1, maxNs - minNs);

        QVector<std::uint64_t> threadOrder;
        threadOrder.reserve(snapshot_.threads.size());
        for (const auto& thread : snapshot_.threads) {
            threadOrder.push_back(thread.threadId);
        }
        if (threadOrder.isEmpty()) {
            for (const auto& event : snapshot_.events) {
                if (event.threadId != 0 && !threadOrder.contains(event.threadId)) {
                    threadOrder.push_back(event.threadId);
                }
            }
        }
        std::sort(threadOrder.begin(), threadOrder.end());

        const int labelWidth = 150;
        const int barLeft = content.left() + labelWidth;
        const int barWidth = std::max(1, content.width() - labelWidth - 12);
        const int rowHeight = 24;

        painter.setPen(QColor(220, 220, 220));
        painter.drawText(content.left(), content.top() - 2,
                         QStringLiteral("events=%1 threads=%2 spanNs=%3")
                             .arg(snapshot_.events.size())
                             .arg(snapshot_.threads.size())
                             .arg(spanNs));

        int visibleRow = 0;
        for (int i = 0; i < threadOrder.size(); ++i) {
            const std::uint64_t threadId = threadOrder[static_cast<std::size_t>(i)];
            const bool isFocusedThread = focusThreadId_ != 0 && threadId == focusThreadId_;
            if (focusThreadId_ != 0 && !isFocusedThread) {
                continue;
            }
            const QRect rowRect(content.left(), content.top() + visibleRow * rowHeight, content.width(), rowHeight - 2);
            ++visibleRow;
            painter.fillRect(rowRect, isFocusedThread ? QColor(36, 36, 50) : QColor(28, 28, 34));
            painter.setPen(QColor(65, 65, 76));
            painter.drawRect(rowRect.adjusted(0, 0, -1, -1));

            QString threadName = QStringLiteral("thread");
            int scopeCount = 0;
            int lockCount = 0;
            int crashCount = 0;
            int lockDepth = 0;
            QString lastMutex;
            for (const auto& thread : snapshot_.threads) {
                if (thread.threadId == threadId) {
                    threadName = thread.threadName.isEmpty()
                        ? QStringLiteral("thread-%1").arg(QString::number(static_cast<unsigned long long>(thread.threadId)))
                        : thread.threadName;
                    scopeCount = thread.scopeCount;
                    lockCount = thread.lockCount;
                    crashCount = thread.crashCount;
                    lockDepth = thread.lockDepth;
                    lastMutex = thread.lastMutexName;
                    break;
                }
            }

            painter.setPen(isFocusedThread ? QColor(255, 235, 170) : QColor(220, 220, 230));
            painter.drawText(QRect(content.left() + 4, rowRect.top(), labelWidth - 8, rowRect.height()),
                             Qt::AlignLeft | Qt::AlignVCenter,
                             QStringLiteral("%1 [0x%2]%3")
                                 .arg(threadName)
                                 .arg(QString::number(static_cast<unsigned long long>(threadId), 16))
                                 .arg(isFocusedThread ? QStringLiteral(" *") : QString()));

            const auto events = eventsForThread(threadId);
            for (const auto& event : events) {
                if (!focusMutexName_.isEmpty() && event.kind == ArtifactCore::TraceEventKind::Lock) {
                    if (event.name != focusMutexName_) {
                        continue;
                    }
                }
                const qreal startT = qreal(event.startNs - minNs) / qreal(spanNs);
                const qreal endT = qreal(std::max(event.endNs, event.startNs + 1) - minNs) / qreal(spanNs);
                const int x = barLeft + static_cast<int>(startT * barWidth);
                const int w = std::max(2, static_cast<int>((endT - startT) * barWidth));
                QRect barRect(x, rowRect.top() + 5, w, rowRect.height() - 10);
                QColor color = traceDomainColor(event.domain);
                if (event.kind == ArtifactCore::TraceEventKind::Lock) {
                    color = event.acquired ? QColor(130, 210, 255) : QColor(255, 140, 110);
                }
                if (event.kind == ArtifactCore::TraceEventKind::Crash) {
                    color = QColor(255, 90, 90);
                }
                painter.fillRect(barRect, color);
                painter.setPen(color.lighter(145));
                painter.drawRect(barRect.adjusted(0, 0, -1, -1));
            }

            painter.setPen(QColor(140, 140, 160));
            painter.drawText(QRect(barLeft + barWidth - 172, rowRect.top(), 168, rowRect.height()),
                             Qt::AlignRight | Qt::AlignVCenter,
                             QStringLiteral("sc:%1 lk:%2 cr:%3 d:%4 %5")
                                 .arg(scopeCount)
                                 .arg(lockCount)
                                 .arg(crashCount)
                                 .arg(lockDepth)
                                 .arg(lastMutex.isEmpty() ? QStringLiteral("-") : lastMutex.left(12)));
        }
    }

private:
    QVector<ArtifactCore::TraceEventRecord> eventsForThread(std::uint64_t threadId) const
    {
        QVector<ArtifactCore::TraceEventRecord> result;
        for (const auto& event : snapshot_.events) {
            if (event.threadId == threadId) {
                if (!focusMutexName_.isEmpty() && event.kind == ArtifactCore::TraceEventKind::Lock && event.name != focusMutexName_) {
                    continue;
                }
                result.push_back(event);
            }
        }
        std::sort(result.begin(), result.end(), [](const auto& a, const auto& b) {
            if (a.startNs == b.startNs) {
                return a.endNs < b.endNs;
            }
            return a.startNs < b.startNs;
        });
        return result;
    }

    ArtifactCore::TraceSnapshot snapshot_;
    std::uint64_t focusThreadId_ = 0;
    QString focusMutexName_;
};

class TraceTimelineWidget::Impl {
public:
    TraceTimelineWidget* owner_ = nullptr;
    QLabel* summary_ = nullptr;
    TraceTimelineCanvas* canvas_ = nullptr;
    QPlainTextEdit* list_ = nullptr;
    std::uint64_t focusThreadId_ = 0;
    QString focusMutexName_;

    explicit Impl(TraceTimelineWidget* owner)
        : owner_(owner)
    {}

    void setupUI()
    {
        auto* layout = new QVBoxLayout(owner_);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(0);

        summary_ = new QLabel(owner_);
        summary_->setTextFormat(Qt::PlainText);
        layout->addWidget(summary_);

        auto* splitter = new QSplitter(Qt::Vertical, owner_);
        canvas_ = new TraceTimelineCanvas(splitter);
        list_ = new QPlainTextEdit(splitter);
        list_->setReadOnly(true);
        list_->setLineWrapMode(QPlainTextEdit::NoWrap);
        splitter->addWidget(canvas_);
        splitter->addWidget(list_);
        splitter->setStretchFactor(0, 3);
        splitter->setStretchFactor(1, 1);
        layout->addWidget(splitter);
    }

    void showSnapshot(const ArtifactCore::TraceSnapshot& snapshot)
    {
        if (summary_) {
            int openLocks = 0;
            QHash<QString, int> lockBalance;
            for (const auto& lock : snapshot.locks) {
                const QString key = lock.mutexName.isEmpty() ? QStringLiteral("<unnamed-mutex>") : lock.mutexName;
                if (lock.acquired) {
                    ++lockBalance[key];
                } else if (lockBalance.value(key) > 0) {
                    --lockBalance[key];
                }
            }
            for (auto it = lockBalance.cbegin(); it != lockBalance.cend(); ++it) {
                openLocks += it.value();
            }
            QStringList summaryParts;
            summaryParts << QStringLiteral("events=%1").arg(snapshot.events.size());
            summaryParts << QStringLiteral("scopes=%1").arg(snapshot.scopes.size());
            summaryParts << QStringLiteral("locks=%1").arg(snapshot.locks.size());
            summaryParts << QStringLiteral("crashes=%1").arg(snapshot.crashes.size());
            summaryParts << QStringLiteral("frames=%1").arg(snapshot.frames.size());
            summaryParts << QStringLiteral("threads=%1").arg(snapshot.threads.size());
            summaryParts << QStringLiteral("openLocks=%1").arg(openLocks);
            if (focusThreadId_ != 0) {
                summaryParts << QStringLiteral("focusThread=0x%1")
                                    .arg(QString::number(static_cast<unsigned long long>(focusThreadId_), 16));
            }
            if (!focusMutexName_.isEmpty()) {
                summaryParts << QStringLiteral("focusMutex=%1").arg(focusMutexName_);
            }
            summary_->setText(summaryParts.join(QStringLiteral("  ")));
        }
        if (canvas_) {
            canvas_->setSnapshot(snapshot);
        }
        if (list_) {
            QStringList lines;
            lines << QStringLiteral("Trace Timeline");
            if (focusThreadId_ != 0 || !focusMutexName_.isEmpty()) {
                lines << QStringLiteral("Focus:");
                if (focusThreadId_ != 0) {
                    lines << QStringLiteral("  thread=0x%1")
                                  .arg(QString::number(static_cast<unsigned long long>(focusThreadId_), 16));
                }
                if (!focusMutexName_.isEmpty()) {
                    lines << QStringLiteral("  mutex=%1").arg(focusMutexName_);
                }
                lines << QString();
            }
            lines << QStringLiteral("Hot threads:");
            QVector<ArtifactCore::TraceThreadRecord> hotThreads = snapshot.threads;
            std::sort(hotThreads.begin(), hotThreads.end(), [](const auto& a, const auto& b) {
                if (a.lockDepth == b.lockDepth) {
                    return a.lockCount > b.lockCount;
                }
                return a.lockDepth > b.lockDepth;
            });
            const int hotRows = std::min(static_cast<int>(hotThreads.size()), 4);
            for (int i = 0; i < hotRows; ++i) {
                const auto& thread = hotThreads[static_cast<std::size_t>(i)];
                lines << QStringLiteral("  %1 depth=%2 locks=%3 last=%4")
                              .arg(thread.threadName.isEmpty() ? QStringLiteral("<unnamed>") : thread.threadName)
                              .arg(thread.lockDepth)
                              .arg(thread.lockCount)
                              .arg(thread.lastMutexName.isEmpty() ? QStringLiteral("-") : thread.lastMutexName);
            }
            if (hotRows == 0) {
                lines << QStringLiteral("  <none>");
            }
            lines << QString();
            lines << QStringLiteral("Domain totals:");
            QHash<QString, qint64> domainDurations;
            QHash<QString, int> domainCounts;
            for (const auto& event : snapshot.events) {
                const QString domain = traceDomainName(event.domain);
                domainDurations[domain] += std::max<qint64>(0, event.endNs - event.startNs);
                ++domainCounts[domain];
            }
            if (domainDurations.isEmpty()) {
                lines << QStringLiteral("  <none>");
            } else {
                QStringList domains = domainDurations.keys();
                std::sort(domains.begin(), domains.end(), [&domainDurations](const QString& a, const QString& b) {
                    if (domainDurations.value(a) == domainDurations.value(b)) {
                        return a < b;
                    }
                    return domainDurations.value(a) > domainDurations.value(b);
                });
                const int domainRows = std::min(static_cast<int>(domains.size()), 6);
                for (int i = 0; i < domainRows; ++i) {
                    const auto& domain = domains[static_cast<std::size_t>(i)];
                    lines << QStringLiteral("  %1 ns=%2 count=%3")
                                  .arg(domain)
                                  .arg(domainDurations.value(domain))
                                  .arg(domainCounts.value(domain));
                }
            }
            lines << QString();
            lines << QStringLiteral("Mutex chains:");
            struct MutexChainRow {
                QString name;
                int balance = 0;
                std::uint64_t lastThreadId = 0;
                qint64 lastNs = 0;
                bool lastAcquire = false;
            };
            QHash<QString, MutexChainRow> mutexChains;
            for (const auto& lock : snapshot.locks) {
                const QString key = lock.mutexName.isEmpty() ? QStringLiteral("<unnamed-mutex>") : lock.mutexName;
                auto& row = mutexChains[key];
                row.name = key;
                row.lastThreadId = lock.threadId;
                row.lastNs = lock.timestampNs;
                row.lastAcquire = lock.acquired;
                if (lock.acquired) {
                    ++row.balance;
                } else if (row.balance > 0) {
                    --row.balance;
                }
            }
            std::vector<MutexChainRow> chainRows;
            chainRows.reserve(mutexChains.size());
            for (auto it = mutexChains.cbegin(); it != mutexChains.cend(); ++it) {
                chainRows.push_back(it.value());
            }
            std::sort(chainRows.begin(), chainRows.end(), [](const auto& a, const auto& b) {
                if (a.balance == b.balance) {
                    return a.lastNs > b.lastNs;
                }
                return a.balance > b.balance;
            });
            const int chainRowsCount = std::min(static_cast<int>(chainRows.size()), 8);
            for (int i = 0; i < chainRowsCount; ++i) {
                const auto& row = chainRows[static_cast<std::size_t>(i)];
                lines << QStringLiteral("  %1 bal=%2 last=%3 [0x%4] %5")
                              .arg(row.name)
                              .arg(row.balance)
                              .arg(row.lastAcquire ? QStringLiteral("acquire") : QStringLiteral("release"))
                              .arg(QString::number(static_cast<unsigned long long>(row.lastThreadId), 16))
                              .arg(QString::number(row.lastNs));
            }
            if (chainRowsCount == 0) {
                lines << QStringLiteral("  <none>");
            }
            lines << QString();
            lines << QStringLiteral("Recent crashes:");
            const int crashRows = std::min(static_cast<int>(snapshot.crashes.size()), 3);
            if (crashRows == 0) {
                lines << QStringLiteral("  <none>");
            } else {
                for (int i = 0; i < crashRows; ++i) {
                    const auto& crash = snapshot.crashes[static_cast<std::size_t>(snapshot.crashes.size() - 1 - i)];
                    lines << QStringLiteral("  %1 thread=%2 [0x%3]")
                                  .arg(crash.summary.isEmpty() ? QStringLiteral("<no-summary>") : crash.summary.left(48))
                                  .arg(crash.threadName.isEmpty() ? QStringLiteral("<unnamed>") : crash.threadName)
                                  .arg(QString::number(static_cast<unsigned long long>(crash.threadId), 16));
                    if (!crash.stack.isEmpty()) {
                        lines << QStringLiteral("    %1").arg(crash.stack.left(120));
                    }
                }
            }
            lines << QString();
            const int eventRows = std::min(static_cast<int>(snapshot.events.size()), 24);
            for (int i = 0; i < eventRows; ++i) {
                const auto& event = snapshot.events[static_cast<std::size_t>(i)];
                QString line = QStringLiteral("%1  %2  [0x%3]  %4  %5  frame=%6");
                line = line.arg(traceDomainName(event.domain));
                line = line.arg(event.name.isEmpty() ? QStringLiteral("<unnamed>") : event.name);
                line = line.arg(QString::number(static_cast<unsigned long long>(event.threadId), 16));
                line = line.arg(QString::number(event.startNs));
                line = line.arg(event.detail.isEmpty() ? QStringLiteral("<no-detail>") : event.detail);
                line = line.arg(QString::number(event.frameIndex));
                if (event.kind == ArtifactCore::TraceEventKind::Lock) {
                    line.append(event.acquired ? QStringLiteral("  [acquire]") : QStringLiteral("  [release]"));
                }
                lines << line;
            }
            if (snapshot.events.isEmpty()) {
                lines << QStringLiteral("<no events>");
            }

            lines << QString();
            lines << QStringLiteral("Lock balance:");
            QHash<QString, int> lockBalance;
            for (const auto& lock : snapshot.locks) {
                const QString key = lock.mutexName.isEmpty() ? QStringLiteral("<unnamed-mutex>") : lock.mutexName;
                if (lock.acquired) {
                    ++lockBalance[key];
                } else if (lockBalance.value(key) > 0) {
                    --lockBalance[key];
                }
            }
            if (lockBalance.isEmpty()) {
                lines << QStringLiteral("<no locks>");
            } else {
                const QStringList keys = lockBalance.keys();
                for (const auto& key : keys) {
                    lines << QStringLiteral("%1 balance=%2")
                                  .arg(key)
                                  .arg(lockBalance.value(key));
                }
            }
            list_->setPlainText(lines.join(QStringLiteral("\n")));
        }
    }

    void setFocusedThreadId(std::uint64_t threadId)
    {
        focusThreadId_ = threadId;
        if (canvas_) {
            canvas_->setFocusedThreadId(threadId);
        }
    }

    void setFocusedMutexName(const QString& mutexName)
    {
        focusMutexName_ = mutexName;
        if (canvas_) {
            canvas_->setFocusedMutexName(mutexName);
        }
    }
};

TraceTimelineWidget::TraceTimelineWidget(QWidget* parent)
    : QWidget(parent), impl_(new Impl(this))
{
    impl_->setupUI();
}

TraceTimelineWidget::~TraceTimelineWidget()
{
    delete impl_;
}

void TraceTimelineWidget::setTraceSnapshot(const ArtifactCore::TraceSnapshot& snapshot)
{
    if (!impl_) {
        return;
    }
    impl_->showSnapshot(snapshot);
}

void TraceTimelineWidget::setFocusedThreadId(std::uint64_t threadId)
{
    if (!impl_) {
        return;
    }
    impl_->setFocusedThreadId(threadId);
}

void TraceTimelineWidget::setFocusedMutexName(const QString& mutexName)
{
    if (!impl_) {
        return;
    }
    impl_->setFocusedMutexName(mutexName);
}

} // namespace Artifact
