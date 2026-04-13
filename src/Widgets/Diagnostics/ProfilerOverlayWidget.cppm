module;
#include <algorithm>
#include <cmath>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include <wobjectimpl.h>

#include <QColor>
#include <QFont>
#include <QFontMetrics>
#include <QPainter>
#include <QResizeEvent>
#include <QTimer>
#include <QWidget>

module Artifact.Widgets.ProfilerOverlay;

import Diagnostics.Profiler;

namespace Artifact {

W_OBJECT_IMPL(ProfilerOverlayWidget)

namespace {
const QColor kBgColor         {0, 0, 0, 190};
const QColor kBgBottleneck    {60, 0, 0, 200};
const QColor kBarGreen        {80, 200, 80};
const QColor kBarYellow       {220, 190, 60};
const QColor kBarRed          {220, 60, 60};
const QColor kGridLine        {80, 80, 80, 120};
const QColor kTextNormal      {210, 210, 210};
const QColor kTextDim         {140, 140, 140};
const QColor kTextWarn        {240, 150, 60};
const QColor kTextCrit        {255, 80, 80};
const QColor kSeparator       {70, 70, 70};
const QColor kHighlight       {255, 200, 60, 50};

constexpr double k60fpsBudgetMs = 16.667;
constexpr double k30fpsBudgetMs = 33.333;

const QColor kCatColors[] = {
    {100, 180, 255},  // Render
    {180, 120, 255},  // Composite
    {100, 220, 180},  // UI
    {255, 180,  80},  // EventBus
    {150, 200,  80},  // IO
    {255, 120, 120},  // Animation
    {160, 160, 160},  // Other
};

// Strip "class Foo::BarEvent" → "BarEvent"
std::string cleanName(const std::string& raw) {
    std::string s = raw;
    for (const auto* prefix : {"class ", "struct "}) {
        const std::string p(prefix);
        if (s.substr(0, p.size()) == p) { s = s.substr(p.size()); break; }
    }
    const auto pos = s.rfind("::");
    if (pos != std::string::npos) s = s.substr(pos + 2);
    return s;
}
} // namespace

class ProfilerOverlayWidget::Impl {
public:
    QTimer*  refreshTimer = nullptr;
    int      barFrameCount = 60;

    static constexpr int kPad        = 8;
    static constexpr int kBarWidth   = 5;
    static constexpr int kBarSpacing = 1;
    static constexpr int kBarHeight  = 60;
    static constexpr int kGraphTop   = 24;
    static constexpr int kScopeRows  = 8;
    static constexpr int kEventRows  = 6;
    static constexpr int kRowH       = 16;
    // Wider panel to fit avg + % columns
    static constexpr int kPanelWidth = 360;

    int calcWidth() const  { return kPanelWidth; }
    int calcHeight() const {
        // bar graph + scope section (header+rows) + event section (header+rows) + footer + padding
        return kPad                                    // top pad
               + kGraphTop + kBarHeight                // bar graph
               + kPad + (1 + kScopeRows) * kRowH      // separator + scope header + scope rows
               + kPad + (1 + kEventRows) * kRowH      // separator + event header + event rows
               + kPad + kRowH                          // footer hint
               + kPad;                                 // bottom pad
    }
};

ProfilerOverlayWidget::ProfilerOverlayWidget(QWidget* parent)
    : QWidget(parent), impl_(new Impl())
{
    setAttribute(Qt::WA_TransparentForMouseEvents, true);
    setAttribute(Qt::WA_NoSystemBackground, true);

    impl_->refreshTimer = new QTimer(this);
    impl_->refreshTimer->setInterval(150);
    connect(impl_->refreshTimer, &QTimer::timeout, this,
            static_cast<void (QWidget::*)()>(&QWidget::update));
    impl_->refreshTimer->start();

    setFixedSize(impl_->calcWidth(), impl_->calcHeight());
}

ProfilerOverlayWidget::~ProfilerOverlayWidget() { delete impl_; }

void ProfilerOverlayWidget::setBarGraphFrameCount(int n) {
    impl_->barFrameCount = std::max(10, std::min(n, 120));
    update();
}

int ProfilerOverlayWidget::barGraphFrameCount() const {
    return impl_->barFrameCount;
}

void ProfilerOverlayWidget::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
}

void ProfilerOverlayWidget::paintEvent(QPaintEvent*) {
    using ArtifactCore::Profiler;
    using ArtifactCore::ProfileCategory;

    auto& prof = Profiler::instance();
    if (!prof.isEnabled()) return;

    const int histN  = impl_->barFrameCount;
    const auto last  = prof.lastFrameSnapshot();
    const auto fstat = prof.computeFrameStats(histN);

    const double lastMs = static_cast<double>(last.frameDurationNs) / 1e6;

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, false);
    p.fillRect(rect(), kBgColor);

    QFont font;
    font.setFamily(QStringLiteral("Consolas"));
    font.setPointSize(8);
    p.setFont(font);
    QFontMetrics fm(font);

    const int pad  = Impl::kPad;
    const int W    = width();
    const int bw   = Impl::kBarWidth;
    const int bs   = Impl::kBarSpacing;
    const int bh   = Impl::kBarHeight;
    const int gtop = pad + Impl::kGraphTop;
    const int rowH = Impl::kRowH;

    // --- Header ---
    {
        const QColor hdrClr = (lastMs > k30fpsBudgetMs) ? kTextCrit
                            : (lastMs > k60fpsBudgetMs) ? kTextWarn
                                                        : kTextNormal;
        p.setPen(hdrClr);
        const QString hdr = QString::asprintf(
            "Profiler  last=%.1fms  avg=%.1f  p95=%.1f  %s  %dx%d",
            lastMs, fstat.avgMs, fstat.p95Ms,
            last.isPlayback ? "PLAY" : "IDLE",
            last.canvasWidth, last.canvasHeight);
        p.drawText(pad, pad + fm.ascent(), hdr);
    }

    // --- Budget lines ---
    const auto msToBarY = [&](double ms) -> int {
        const double clamped = std::min(ms, k30fpsBudgetMs * 2.0);
        return gtop + bh - static_cast<int>(clamped / (k30fpsBudgetMs * 2.0) * bh);
    };

    {
        p.setPen(QPen(kGridLine, 1, Qt::DashLine));
        p.drawLine(pad, msToBarY(k60fpsBudgetMs), W - pad, msToBarY(k60fpsBudgetMs));
        p.drawLine(pad, msToBarY(k30fpsBudgetMs), W - pad, msToBarY(k30fpsBudgetMs));
        p.setPen(kTextDim);
        p.drawText(W - pad - fm.horizontalAdvance(QStringLiteral("60fps")) - 2,
                   msToBarY(k60fpsBudgetMs) - 2, QStringLiteral("60fps"));
        p.drawText(W - pad - fm.horizontalAdvance(QStringLiteral("30fps")) - 2,
                   msToBarY(k30fpsBudgetMs) - 2, QStringLiteral("30fps"));
    }

    // --- Frame bars ---
    {
        const auto frames = prof.frameHistory(histN);
        const int maxBars = std::min(static_cast<int>(frames.size()),
                                     (W - pad * 2) / (bw + bs));
        int x = pad;
        for (int i = 0; i < maxBars; ++i) {
            const double ms = static_cast<double>(frames[i].frameDurationNs) / 1e6;
            const QColor clr = (ms < k60fpsBudgetMs) ? kBarGreen
                             : (ms < k30fpsBudgetMs) ? kBarYellow
                                                     : kBarRed;
            const int barPx = std::max(1, msToBarY(0) - msToBarY(ms));
            p.fillRect(x, gtop + bh - barPx, bw, barPx, clr);
            x += bw + bs;
        }
    }

    // Separator
    int curY = gtop + bh + pad;
    p.setPen(kSeparator);
    p.drawLine(pad, curY, W - pad, curY);
    curY += pad;

    // --- Scope table ---
    {
        auto scopes = last.scopes;
        std::sort(scopes.begin(), scopes.end(),
                  [](const ArtifactCore::ScopeRecord& a,
                     const ArtifactCore::ScopeRecord& b) {
                      return a.durationNs > b.durationNs;
                  });

        // Column positions
        const int colName = pad + 10;
        const int colLast = W - 120;
        const int colAvg  = W - 80;
        const int colPct  = W - 38;

        // Header row
        p.setPen(kTextDim);
        p.drawText(colName, curY + fm.ascent(),
                   QStringLiteral("Scope"));
        p.drawText(colLast, curY + fm.ascent(), QStringLiteral("last"));
        p.drawText(colAvg,  curY + fm.ascent(), QStringLiteral("avg"));
        p.drawText(colPct,  curY + fm.ascent(), QStringLiteral("%fr"));
        curY += rowH;

        const int rows = std::min(static_cast<int>(scopes.size()), Impl::kScopeRows);
        for (int i = 0; i < rows; ++i, curY += rowH) {
            const auto& s    = scopes[i];
            const double sMs = static_cast<double>(s.durationNs) / 1e6;
            const double pct = (lastMs > 0.001) ? sMs / lastMs * 100.0 : 0.0;
            const auto   sst = prof.computeScopeStats(s.name, histN);

            // Highlight the top bottleneck (worst scope, first row after sort)
            if (i == 0 && sMs > k60fpsBudgetMs * 0.5) {
                p.fillRect(0, curY, W, rowH, kHighlight);
            }

            const int catIdx = std::min(static_cast<int>(s.category), 6);
            p.fillRect(pad, curY + 2, 6, rowH - 4, kCatColors[catIdx]);

            const bool isWarn = sMs > prof.scopeWarningThresholdMs();
            p.setPen(isWarn ? kTextWarn : kTextNormal);

            const std::string indent(static_cast<std::size_t>(s.depth * 2), ' ');
            const QString name = QString::fromStdString(indent + s.name).left(18);
            p.drawText(colName, curY + fm.ascent(), name);
            p.drawText(colLast, curY + fm.ascent(),
                       QString::asprintf("%5.1f", sMs));
            p.setPen(kTextDim);
            p.drawText(colAvg,  curY + fm.ascent(),
                       QString::asprintf("%5.1f", sst.avgMs));
            p.drawText(colPct,  curY + fm.ascent(),
                       QString::asprintf("%4.0f%%", pct));
        }
        if (rows == 0) {
            p.setPen(kTextDim);
            p.drawText(pad, curY + fm.ascent(),
                       QStringLiteral("(no scopes — add ProfileScope markers)"));
            curY += rowH;
        }
    }

    // Separator
    curY += pad;
    p.setPen(kSeparator);
    p.drawLine(pad, curY, W - pad, curY);
    curY += pad;

    // --- EventBus table ---
    {
        const int threshold = prof.eventSpamThreshold();
        std::vector<std::pair<std::string, int>> sorted(
            last.eventCounts.begin(), last.eventCounts.end());
        std::sort(sorted.begin(), sorted.end(),
                  [](const auto& a, const auto& b){ return a.second > b.second; });

        const int colEvt = pad;
        const int colN   = W - 28;

        p.setPen(kTextDim);
        p.drawText(colEvt, curY + fm.ascent(), QStringLiteral("EventBus"));
        p.drawText(colN,   curY + fm.ascent(), QStringLiteral("n"));
        curY += rowH;

        const int rows = std::min(static_cast<int>(sorted.size()), Impl::kEventRows);
        for (int i = 0; i < rows; ++i, curY += rowH) {
            const auto& [rawName, cnt] = sorted[i];
            const bool  spam = cnt > threshold;
            const QString nice = QString::fromStdString(cleanName(rawName)).left(30);
            p.setPen(spam ? kTextWarn : kTextNormal);
            p.drawText(colEvt, curY + fm.ascent(), nice);
            p.drawText(colN,   curY + fm.ascent(), QString::number(cnt));
            if (spam) {
                p.setPen(kTextCrit);
                p.drawText(colN + fm.horizontalAdvance(QString::number(cnt)) + 4,
                           curY + fm.ascent(), QStringLiteral("⚠"));
            }
        }
        if (rows == 0) {
            p.setPen(kTextDim);
            p.drawText(pad, curY + fm.ascent(), QStringLiteral("(none)"));
            curY += rowH;
        }
    }

    // --- Footer hint ---
    curY += pad;
    p.setPen(kTextDim);
    p.drawText(pad, curY + fm.ascent(),
               QStringLiteral("Ctrl+Shift+C: copy report  Ctrl+Shift+P: hide"));
}

} // namespace Artifact
