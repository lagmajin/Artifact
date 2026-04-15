module;
#include <algorithm>
#include <cmath>
#include <string>
#include <utility>
#include <vector>
#include <wobjectimpl.h>

#include <QClipboard>
#include <QColor>
#include <QFont>
#include <QFontMetrics>
#include <QGuiApplication>
#include <QMouseEvent>
#include <QPainter>
#include <QRect>
#include <QResizeEvent>
#include <QString>
#include <QTimer>
#include <QWidget>

module Artifact.Widgets.ProfilerPanel;

import ArtifactCore.Utils.PerformanceProfiler;

namespace Artifact {

W_OBJECT_IMPL(ProfilerPanelWidget)

// ---------------------------------------------------------------------------
// Palette
// ---------------------------------------------------------------------------
namespace {

const QColor kBg          {22, 22, 28, 242};
const QColor kBgSection   {30, 30, 38, 255};
const QColor kBgHdr       {40, 40, 52, 255};
const QColor kBgHighlight {60, 30, 10, 200};
const QColor kBgBtn       {50, 50, 65, 255};
const QColor kBgBtnHover  {70, 70, 90, 255};
const QColor kBorder      {60, 60, 80, 180};
const QColor kBarGreen    {70, 195, 70};
const QColor kBarYellow   {220, 190, 50};
const QColor kBarRed      {220, 55, 55};
const QColor kBarMini     {80, 80, 140, 200};
const QColor kBarMiniTop  {140, 100, 220, 220};
const QColor kTextH       {240, 240, 255};
const QColor kTextN       {195, 195, 210};
const QColor kTextD       {120, 120, 140};
const QColor kTextW       {240, 155, 55};
const QColor kTextCrit    {255, 75, 75};
const QColor kTextGreen   {80, 200, 80};
const QColor kGridLine    {60, 60, 80, 100};

const QColor kCatColors[] = {
    {100, 180, 255},  // Render
    {180, 120, 255},  // Composite
    {100, 220, 180},  // UI
    {255, 180,  80},  // EventBus
    {150, 200,  80},  // IO
    {255, 120, 120},  // Animation
    {160, 160, 160},  // Other
};

constexpr double k60fps = 16.667;
constexpr double k30fps = 33.333;

std::string cleanEventName(const std::string& raw) {
    std::string s = raw;
    for (const auto* pfx : {"class ", "struct "}) {
        if (s.substr(0, std::string(pfx).size()) == pfx) {
            s = s.substr(std::string(pfx).size());
            break;
        }
    }
    const auto pos = s.rfind("::");
    if (pos != std::string::npos) s = s.substr(pos + 2);
    return s;
}

// Minimal-bar painter — fills a horizontal bar of given fraction in a rect.
void drawMiniBar(QPainter& p, const QRect& r, double fraction,
                 const QColor& clr)
{
    const int w = static_cast<int>(std::max(2.0, r.width() * std::min(fraction, 1.0)));
    p.fillRect(r.x(), r.y(), w, r.height(), clr);
}

} // namespace

// ---------------------------------------------------------------------------
// Impl
// ---------------------------------------------------------------------------
class ProfilerPanelWidget::Impl {
public:
    QTimer* timer      = nullptr;
    int     histN      = 90;
    bool    btnHovered = false;
    QRect   btnRect;

    static constexpr int kW         = 520;
    static constexpr int kPad       = 10;
    static constexpr int kRowH      = 17;
    static constexpr int kHdrH      = 20;
    static constexpr int kGraphH    = 80;
    static constexpr int kScopeRows = 10;
    static constexpr int kTimerRows = 6;
    static constexpr int kEventRows = 8;

    static constexpr int kColName   = kPad + 8;
    static constexpr int kColLast   = kW - 246;
    static constexpr int kColAvg    = kW - 194;
    static constexpr int kColP95    = kW - 142;
    static constexpr int kColPct    = kW - 90;
    static constexpr int kColBar    = kW - 60;
    static constexpr int kBarW      = 50;

    int calcHeight() const {
        return kPad                              // top
             + kHdrH                            // title bar
             + kPad + kGraphH                   // bar graph
             + kPad + kHdrH                     // scopes header
             + (kScopeRows + 1) * kRowH         // scope rows + col header
             + kPad + kHdrH                     // ui timers header
             + (kTimerRows + 1) * kRowH         // timer rows + col header
             + kPad + kHdrH                     // events header
             + (kEventRows + 1) * kRowH         // event rows + col header
             + kPad * 2;                        // bottom
    }
};

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------
ProfilerPanelWidget::ProfilerPanelWidget(QWidget* parent)
    : QWidget(parent, Qt::Tool | Qt::FramelessWindowHint),
      impl_(new Impl())
{
    setAttribute(Qt::WA_NoSystemBackground, true);
    setFixedSize(Impl::kW, impl_->calcHeight());

    impl_->timer = new QTimer(this);
    impl_->timer->setInterval(200);
    connect(impl_->timer, &QTimer::timeout, this,
            static_cast<void (QWidget::*)()>(&QWidget::update));
    impl_->timer->start();
}

ProfilerPanelWidget::~ProfilerPanelWidget() { delete impl_; }

void ProfilerPanelWidget::setHistoryFrames(int n) {
    impl_->histN = std::max(10, std::min(n, 120));
    update();
}
int ProfilerPanelWidget::historyFrames() const { return impl_->histN; }

void ProfilerPanelWidget::copyReportToClipboard() {
    const std::string report =
        ArtifactCore::Profiler::instance().generateDiagnosticReport(impl_->histN);
    QGuiApplication::clipboard()->setText(QString::fromStdString(report));
}

void ProfilerPanelWidget::mousePressEvent(QMouseEvent* event) {
    if (impl_->btnRect.contains(event->pos())) {
        copyReportToClipboard();
    }
    QWidget::mousePressEvent(event);
}

void ProfilerPanelWidget::resizeEvent(QResizeEvent* e) {
    QWidget::resizeEvent(e);
}

// ---------------------------------------------------------------------------
// Paint
// ---------------------------------------------------------------------------
void ProfilerPanelWidget::paintEvent(QPaintEvent*) {
    using ArtifactCore::Profiler;
    auto& prof = Profiler::instance();

    const int histN = impl_->histN;
    const auto last  = prof.lastFrameSnapshot();
    const auto fstat = prof.computeFrameStats(histN);
    const double lastMs = static_cast<double>(last.frameDurationNs) / 1e6;

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, false);
    p.fillRect(rect(), kBg);
    p.setPen(kBorder);
    p.drawRect(rect().adjusted(0,0,-1,-1));

    QFont fontMono, fontBold;
    fontMono.setFamily(QStringLiteral("Consolas"));
    fontMono.setPointSize(8);
    fontBold.setFamily(QStringLiteral("Consolas"));
    fontBold.setPointSize(9);
    fontBold.setBold(true);

    p.setFont(fontMono);
    QFontMetrics fm(fontMono);
    QFontMetrics fmB(fontBold);

    const int W   = width();
    const int pad = Impl::kPad;
    int curY = 0;

    // =======================================================================
    // Title bar
    // =======================================================================
    const int titleH = Impl::kHdrH + pad;
    p.fillRect(0, curY, W, titleH, kBgHdr);
    p.setFont(fontBold);
    p.setPen(kTextH);
    p.drawText(pad, curY + fmB.ascent() + 4,
               QStringLiteral("Performance Profiler"));

    // Copy button
    const QString btnTxt = QStringLiteral("📋 Copy Report");
    const int btnW = fmB.horizontalAdvance(btnTxt) + 16;
    impl_->btnRect = QRect(W - btnW - pad, curY + 4, btnW, titleH - 8);
    p.fillRect(impl_->btnRect, impl_->btnHovered ? kBgBtnHover : kBgBtn);
    p.setPen(kBorder);
    p.drawRect(impl_->btnRect);
    p.setPen(kTextN);
    p.setFont(fontMono);
    p.drawText(impl_->btnRect,
               Qt::AlignCenter | Qt::AlignVCenter, btnTxt);

    // Enabled/status dot
    const bool enabled = prof.isEnabled();
    p.setBrush(enabled ? kBarGreen : kBarRed);
    p.setPen(Qt::NoPen);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.drawEllipse(pad + fmB.horizontalAdvance(QStringLiteral("Performance Profiler")) + 8,
                  curY + 9, 6, 6);
    p.setRenderHint(QPainter::Antialiasing, false);

    if (!enabled) {
        p.setPen(kTextD);
        p.setFont(fontMono);
        p.drawText(pad, curY + titleH + fm.ascent() + 4,
                   QStringLiteral("Profiler disabled — press Ctrl+Shift+P in CompositeEditor"));
        return;
    }

    curY += titleH;

    // =======================================================================
    // Frame-time bar graph
    // =======================================================================
    {
        curY += pad;
        const int graphTop = curY;
        const int graphH   = Impl::kGraphH;

        // Axis labels
        const auto msToY = [&](double ms) -> int {
            const double clamped = std::min(ms, k30fps * 2.0);
            return graphTop + graphH - static_cast<int>(clamped / (k30fps * 2.0) * graphH);
        };

        // Grid lines
        p.setPen(QPen(kGridLine, 1, Qt::DashLine));
        p.drawLine(pad, msToY(k60fps), W - pad, msToY(k60fps));
        p.drawLine(pad, msToY(k30fps), W - pad, msToY(k30fps));
        p.setPen(kTextD);
        p.drawText(W - pad - fm.horizontalAdvance(QStringLiteral("60")), msToY(k60fps) - 2,
                   QStringLiteral("60"));
        p.drawText(W - pad - fm.horizontalAdvance(QStringLiteral("30")), msToY(k30fps) - 2,
                   QStringLiteral("30"));

        // Bars
        const auto frames = prof.frameHistory(histN);
        const int bw = std::max(2, (W - pad * 2) / std::max(1, static_cast<int>(frames.size())));
        int x = pad;
        for (const auto& fr : frames) {
            const double ms = static_cast<double>(fr.frameDurationNs) / 1e6;
            const QColor clr = (ms < k60fps) ? kBarGreen
                             : (ms < k30fps) ? kBarYellow
                                             : kBarRed;
            const int barPx = std::max(1, msToY(0) - msToY(ms));
            p.fillRect(x, graphTop + graphH - barPx, bw - 1, barPx, clr);
            x += bw;
            if (x > W - pad) break;
        }

        // Stats line
        const QColor statClr = (fstat.avgMs > k30fps) ? kTextCrit
                             : (fstat.avgMs > k60fps) ? kTextW
                                                      : kTextGreen;
        p.setPen(statClr);
        p.drawText(pad, graphTop + graphH + fm.height(),
                   QString::asprintf("last=%.1fms  avg=%.1f  p95=%.1f  max=%.1f  [%d frames]",
                                     lastMs, fstat.avgMs, fstat.p95Ms, fstat.maxMs, fstat.samples));
        curY = graphTop + graphH + fm.height() + 4;
    }

    // Helper: draw a column-header row
    const auto drawColHeader = [&](int y) {
        p.setPen(kTextD);
        p.setFont(fontMono);
        p.drawText(Impl::kColName, y + fm.ascent(), QStringLiteral("Name"));
        p.drawText(Impl::kColLast, y + fm.ascent(), QStringLiteral("last"));
        p.drawText(Impl::kColAvg,  y + fm.ascent(), QStringLiteral("avg"));
        p.drawText(Impl::kColP95,  y + fm.ascent(), QStringLiteral("p95"));
        p.drawText(Impl::kColPct,  y + fm.ascent(), QStringLiteral("%fr"));
        p.drawText(Impl::kColBar,  y + fm.ascent(), QStringLiteral("share"));
    };

    // =======================================================================
    // Render Scopes
    // =======================================================================
    {
        curY += pad;
        p.fillRect(0, curY, W, Impl::kHdrH, kBgSection);
        p.setPen(kBorder);
        p.drawLine(0, curY, W, curY);
        p.setFont(fontBold);
        p.setPen(kTextH);
        p.drawText(pad, curY + fmB.ascent() + 2, QStringLiteral("Render Scopes"));
        curY += Impl::kHdrH;

        drawColHeader(curY);
        curY += Impl::kRowH;

        // Sort last-frame scopes by duration desc
        auto scopes = last.scopes;
        std::sort(scopes.begin(), scopes.end(),
                  [](const ArtifactCore::ScopeRecord& a,
                     const ArtifactCore::ScopeRecord& b) {
                      return a.durationNs > b.durationNs;
                  });

        const int rows = std::min(static_cast<int>(scopes.size()), Impl::kScopeRows);
        for (int i = 0; i < rows; ++i, curY += Impl::kRowH) {
            const auto& s    = scopes[i];
            const double sMs = static_cast<double>(s.durationNs) / 1e6;
            const double pct = (lastMs > 0.001) ? sMs / lastMs : 0.0;
            const auto   sst = prof.computeScopeStats(s.name, histN);

            // Highlight biggest bottleneck
            if (i == 0 && sMs > k60fps * 0.3) {
                p.fillRect(0, curY, W, Impl::kRowH, kBgHighlight);
            }

            const int catIdx = std::min(static_cast<int>(s.category), 6);
            p.fillRect(pad, curY + 3, 5, Impl::kRowH - 6, kCatColors[catIdx]);

            const bool isWarn = sMs > prof.scopeWarningThresholdMs();
            p.setPen(isWarn ? kTextW : kTextN);
            p.setFont(fontMono);

            const std::string indent(static_cast<std::size_t>(s.depth * 2), ' ');
            const QString name = QString::fromStdString(indent + s.name).left(20);
            p.drawText(Impl::kColName + 8, curY + fm.ascent(), name);

            p.drawText(Impl::kColLast, curY + fm.ascent(),
                       QString::asprintf("%5.1f", sMs));
            p.setPen(kTextD);
            p.drawText(Impl::kColAvg, curY + fm.ascent(),
                       QString::asprintf("%5.1f", sst.avgMs));
            p.drawText(Impl::kColP95, curY + fm.ascent(),
                       QString::asprintf("%5.1f", sst.p95Ms));
            p.setPen(isWarn ? kTextW : kTextN);
            p.drawText(Impl::kColPct, curY + fm.ascent(),
                       QString::asprintf("%4.0f%%", pct * 100.0));

            // Mini bar
            const QRect barRect(Impl::kColBar, curY + 3, Impl::kBarW, Impl::kRowH - 6);
            p.fillRect(barRect, QColor(40, 40, 55));
            drawMiniBar(p, barRect, sst.avgMs / std::max(fstat.avgMs, 0.001),
                        (i == 0) ? kBarMiniTop : kBarMini);
        }
        if (rows == 0) {
            p.setPen(kTextD);
            p.drawText(pad, curY + fm.ascent(),
                       QStringLiteral("No scopes yet — enable profiler with Ctrl+Shift+P"));
            curY += Impl::kRowH;
        }
    }

    // =======================================================================
    // UI Timers (ambient ProfileTimer sources)
    // =======================================================================
    {
        curY += pad;
        p.fillRect(0, curY, W, Impl::kHdrH, kBgSection);
        p.setPen(kBorder);
        p.drawLine(0, curY, W, curY);
        p.setFont(fontBold);
        p.setPen(kTextH);
        p.drawText(pad, curY + fmB.ascent() + 2, QStringLiteral("UI Timers  (paint/update)"));
        curY += Impl::kHdrH;

        // Column header (no %fr column for timers)
        p.setPen(kTextD);
        p.setFont(fontMono);
        p.drawText(Impl::kColName, curY + fm.ascent(), QStringLiteral("Name"));
        p.drawText(Impl::kColAvg,  curY + fm.ascent(), QStringLiteral("avg"));
        p.drawText(Impl::kColP95,  curY + fm.ascent(), QStringLiteral("p95"));
        p.drawText(Impl::kColBar,  curY + fm.ascent(), QStringLiteral("share"));
        curY += Impl::kRowH;

        const auto timerNames = prof.knownTimerNames();
        // Sort by avg descending
        struct TS { std::string name; ArtifactCore::ScopeStats st; };
        std::vector<TS> timers;
        for (const auto& n : timerNames)
            timers.push_back({n, prof.timerStats(n, histN)});
        std::sort(timers.begin(), timers.end(),
                  [](const TS& a, const TS& b) { return a.st.avgMs > b.st.avgMs; });

        // Find max for bar scaling
        const double timerMax = timers.empty() ? 1.0 : std::max(timers.front().st.avgMs, 0.001);

        const int rows = std::min(static_cast<int>(timers.size()), Impl::kTimerRows);
        for (int i = 0; i < rows; ++i, curY += Impl::kRowH) {
            const auto& [name, st] = timers[i];
            const bool warn = st.avgMs > 8.0;
            p.setPen(warn ? kTextW : kTextN);
            p.setFont(fontMono);
            p.drawText(Impl::kColName, curY + fm.ascent(),
                       QString::fromStdString(name).left(24));
            p.setPen(warn ? kTextW : kTextN);
            p.drawText(Impl::kColAvg, curY + fm.ascent(),
                       QString::asprintf("%5.1f", st.avgMs));
            p.setPen(kTextD);
            p.drawText(Impl::kColP95, curY + fm.ascent(),
                       QString::asprintf("%5.1f", st.p95Ms));
            const QRect barRect(Impl::kColBar, curY + 3, Impl::kBarW, Impl::kRowH - 6);
            p.fillRect(barRect, QColor(40, 40, 55));
            drawMiniBar(p, barRect, st.avgMs / timerMax,
                        warn ? QColor(220, 140, 50, 200) : kBarMini);
        }
        if (rows == 0) {
            p.setPen(kTextD);
            p.drawText(pad, curY + fm.ascent(),
                       QStringLiteral("No UI timers yet — add ProfileTimer to paint events"));
            curY += Impl::kRowH;
        }
    }

    // =======================================================================
    // EventBus
    // =======================================================================
    {
        curY += pad;
        p.fillRect(0, curY, W, Impl::kHdrH, kBgSection);
        p.setPen(kBorder);
        p.drawLine(0, curY, W, curY);
        p.setFont(fontBold);
        p.setPen(kTextH);

        // Count total dispatches
        int totalEvt = 0;
        for (const auto& [k, v] : last.eventCounts) totalEvt += v;
        p.drawText(pad, curY + fmB.ascent() + 2,
                   QString::asprintf("EventBus  (%d dispatches this frame)", totalEvt));
        curY += Impl::kHdrH;

        p.setPen(kTextD);
        p.setFont(fontMono);
        p.drawText(Impl::kColName, curY + fm.ascent(), QStringLiteral("Event"));
        p.drawText(Impl::kColAvg,  curY + fm.ascent(), QStringLiteral("n"));
        p.drawText(Impl::kColP95,  curY + fm.ascent(), QStringLiteral("subs"));
        curY += Impl::kRowH;

        const int spamT = prof.eventSpamThreshold();
        std::vector<std::pair<std::string, int>> sorted(
            last.eventCounts.begin(), last.eventCounts.end());
        std::sort(sorted.begin(), sorted.end(),
                  [](const auto& a, const auto& b) { return a.second > b.second; });

        const int rows = std::min(static_cast<int>(sorted.size()), Impl::kEventRows);
        for (int i = 0; i < rows; ++i, curY += Impl::kRowH) {
            const auto& [rawName, cnt] = sorted[i];
            const bool spam = cnt > spamT;
            const QString nice = QString::fromStdString(
                cleanEventName(rawName)).left(30);
            const int subs = [&]() -> int {
                auto it = last.eventSubscriberPeak.find(rawName);
                return it != last.eventSubscriberPeak.end() ? it->second : 0;
            }();
            p.setPen(spam ? kTextW : kTextN);
            p.drawText(Impl::kColName, curY + fm.ascent(), nice);
            p.drawText(Impl::kColAvg,  curY + fm.ascent(), QString::number(cnt));
            p.setPen(kTextD);
            p.drawText(Impl::kColP95,  curY + fm.ascent(), QString::number(subs));
            if (spam) {
                p.setPen(kTextCrit);
                p.drawText(Impl::kColP95 + fm.horizontalAdvance(QString::number(subs)) + 6,
                           curY + fm.ascent(), QStringLiteral("⚠ SPAM"));
            }
        }
        if (rows == 0) {
            p.setPen(kTextD);
            p.drawText(pad, curY + fm.ascent(), QStringLiteral("(none this frame)"));
            curY += Impl::kRowH;
        }
    }

    // =======================================================================
    // Bottom padding
    // =======================================================================
    curY += pad;
}

} // namespace Artifact
