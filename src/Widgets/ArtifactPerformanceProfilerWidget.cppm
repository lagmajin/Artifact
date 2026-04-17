module;
#include <QWidget>
#include <QTimer>
#include <QPainter>
#include <QLinearGradient>
#include <QFont>
#include <QFontMetrics>
#include <QPalette>
#include <QClipboard>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QPaintEvent>
#include <QResizeEvent>
#include <QRect>
#include <wobjectdefs.h>
#include <wobjectimpl.h>

#include <algorithm>
#include <cmath>
#include <map>
#include <string>
#include <utility>
#include <vector>

export module Artifact.Widgets.PerformanceProfilerWidget;

import ArtifactCore.Utils.PerformanceProfiler;
import Widgets.Utils.CSS;

namespace Artifact {

namespace {

struct ThemeColors {
    QColor bg0;
    QColor bg1;
    QColor bg2;
    QColor accent;
    QColor text;
    QColor textMuted;
    QColor border;
};

ThemeColors makeThemeColors()
{
    const auto& theme = ArtifactCore::currentDCCTheme();
    QColor bg0(theme.backgroundColor);
    QColor bg1(theme.secondaryBackgroundColor);
    QColor bg2 = bg1.lighter(108);
    QColor accent(theme.accentColor);
    QColor text(theme.textColor);
    QColor textMuted = text.darker(140);
    QColor border = textMuted;
    border.setAlpha(120);
    return {bg0, bg1, bg2, accent, text, textMuted, border};
}

QColor withAlpha(QColor color, int alpha)
{
    color.setAlpha(alpha);
    return color;
}

QColor severityColor(double ms)
{
    if (ms >= 33.3) {
        return QColor(239, 83, 80);
    }
    if (ms >= 16.6) {
        return QColor(255, 167, 38);
    }
    return QColor(76, 175, 80);
}

QString formatMs(double ms)
{
    return QString::asprintf("%.2f ms", ms);
}

QString formatCompactMs(double ms)
{
    return QString::asprintf("%.1f", ms);
}

QString formatPercent(double value)
{
    return QString::asprintf("%.0f%%", value * 100.0);
}

struct SampleRow {
    QString name;
    double durationMs = 0.0;
    double share = 0.0;
};

} // namespace

/**
 * @brief Widget for visualizing performance metrics in real-time
 */
export class ArtifactPerformanceProfilerWidget : public QWidget {
    W_OBJECT(ArtifactPerformanceProfilerWidget)

public:
    explicit ArtifactPerformanceProfilerWidget(QWidget* parent = nullptr)
        : QWidget(parent)
    {
        setAttribute(Qt::WA_OpaquePaintEvent, true);
        setAutoFillBackground(false);
        setMinimumSize(520, 320);

        timer_ = new QTimer(this);
        connect(timer_, &QTimer::timeout, this, &ArtifactPerformanceProfilerWidget::updateMetrics);
        timer_->start(100);
    }

    void setNamePrefixFilter(const QString& prefix)
    {
        if (namePrefixFilter_ == prefix) {
            return;
        }
        namePrefixFilter_ = prefix;
        updateMetrics();
    }

    QString namePrefixFilter() const
    {
        return namePrefixFilter_;
    }

    void setTitleText(const QString& text)
    {
        if (titleText_ == text) {
            return;
        }
        titleText_ = text;
        update();
    }

    void copyReportToClipboard()
    {
        const std::string report =
            ArtifactCore::Profiler::instance().generateDiagnosticReport(48);
        QGuiApplication::clipboard()->setText(QString::fromStdString(report));
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        const ThemeColors colors = makeThemeColors();

        const auto samples = filteredSamples();
        std::vector<SampleRow> rows;
        rows.reserve(samples.size());

        double totalMs = 0.0;
        double slowestMs = 0.0;

        for (const auto& [name, sample] : samples) {
            totalMs += sample.durationMs;
            if (sample.durationMs >= slowestMs) {
                slowestMs = sample.durationMs;
            }
        }

        for (const auto& [name, sample] : samples) {
            const double share = totalMs > 0.0001 ? sample.durationMs / totalMs : 0.0;
            rows.push_back({QString::fromStdString(name), sample.durationMs, share});
        }

        std::sort(rows.begin(), rows.end(), [](const SampleRow& a, const SampleRow& b) {
            return a.durationMs > b.durationMs;
        });

        const int visibleRows = std::min<int>(rows.size(), 8);
        const double totalBudgetMs = 16.67;
        const double rateHz = totalMs > 0.0001 ? 1000.0 / totalMs : 0.0;

        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);
        p.setRenderHint(QPainter::TextAntialiasing, true);

        const QRect bounds = rect().adjusted(12, 12, -12, -12);
        const int radius = 18;

        QLinearGradient bgGrad(bounds.topLeft(), bounds.bottomRight());
        bgGrad.setColorAt(0.0, colors.bg0.lighter(108));
        bgGrad.setColorAt(1.0, colors.bg0.darker(112));
        p.fillRect(rect(), bgGrad);

        p.setPen(Qt::NoPen);
        p.setBrush(QColor(0, 0, 0, 40));
        p.drawRoundedRect(bounds.translated(0, 2), radius, radius);
        p.setBrush(colors.bg1);
        p.drawRoundedRect(bounds, radius, radius);

        // Accent strip and inner border
        p.setBrush(colors.accent);
        p.drawRoundedRect(QRect(bounds.left(), bounds.top(), bounds.width(), 5), radius, radius);
        p.setPen(colors.border);
        p.setBrush(Qt::NoBrush);
        p.drawRoundedRect(bounds, radius, radius);

        const int contentLeft = bounds.left() + 20;
        const int contentRight = bounds.right() - 20;
        const int contentTop = bounds.top() + 18;
        const int contentWidth = bounds.width() - 40;

        QFont titleFont = font();
        titleFont.setFamily(QStringLiteral("Segoe UI"));
        titleFont.setPointSize(16);
        titleFont.setWeight(QFont::DemiBold);

        QFont subtitleFont = titleFont;
        subtitleFont.setPointSize(9);
        subtitleFont.setWeight(QFont::Normal);

        QFont metricFont = titleFont;
        metricFont.setPointSize(12);
        metricFont.setWeight(QFont::DemiBold);

        QFont rowNameFont = titleFont;
        rowNameFont.setPointSize(10);
        rowNameFont.setWeight(QFont::Medium);

        QFont monoFont(QStringLiteral("Consolas"));
        monoFont.setPointSize(9);

        QFontMetrics titleFm(titleFont);
        QFontMetrics subtitleFm(subtitleFont);
        QFontMetrics metricFm(metricFont);
        QFontMetrics rowNameFm(rowNameFont);
        QFontMetrics monoFm(monoFont);

        const bool hasData = !rows.empty();
        const bool healthy = totalMs <= totalBudgetMs;
        const QColor statusColor = hasData ? (healthy ? QColor(76, 175, 80) : severityColor(totalMs))
                                           : QColor(124, 124, 124);

        // Header
        p.setPen(colors.text);
        p.setFont(titleFont);
        p.drawText(contentLeft, contentTop + titleFm.ascent(), titleText_);

        p.setFont(subtitleFont);
        p.setPen(colors.textMuted);
        const QString subtitle = namePrefixFilter_.isEmpty()
            ? QStringLiteral("All profiled scopes")
            : QStringLiteral("Scope prefix: %1").arg(namePrefixFilter_);
        p.drawText(contentLeft, contentTop + titleFm.height() + 8 + subtitleFm.ascent(), subtitle);

        const QString statusText = hasData
            ? QStringLiteral("LIVE  %1").arg(formatMs(totalMs))
            : QStringLiteral("IDLE");
        const int statusW = monoFm.horizontalAdvance(statusText) + 22;
        const int statusH = 28;
        const QRect statusRect(contentRight - statusW, contentTop, statusW, statusH);
        p.setPen(Qt::NoPen);
        p.setBrush(withAlpha(statusColor, 38));
        p.drawRoundedRect(statusRect, 14, 14);
        p.setPen(statusColor);
        p.setFont(monoFont);
        p.drawText(statusRect, Qt::AlignCenter, statusText);

        // Secondary chips
        const int chipsY = contentTop + titleFm.height() + subtitleFm.height() + 18;
        auto drawChip = [&](const QRect& r, const QString& label, const QString& value, const QColor& accent) {
            p.setPen(Qt::NoPen);
            p.setBrush(withAlpha(accent, 28));
            p.drawRoundedRect(r, 12, 12);
            p.setPen(accent);
            p.setFont(subtitleFont);
            p.drawText(r.adjusted(12, 4, -12, -r.height() / 2), Qt::AlignLeft | Qt::AlignVCenter, label);
            p.setFont(metricFont);
            p.setPen(colors.text);
            p.drawText(r.adjusted(12, r.height() / 2 - 2, -12, -4), Qt::AlignLeft | Qt::AlignVCenter, value);
        };

        const int chipH = 62;
        const int chipGap = 10;
        const int chipW = (contentWidth - chipGap * 3) / 4;
        const QColor accentA = colors.accent;
        const QColor accentB = severityColor(slowestMs);
        const QColor accentC = healthy ? QColor(76, 175, 80) : QColor(255, 167, 38);
        const QColor accentD = QColor(96, 165, 250);

        drawChip(QRect(contentLeft, chipsY, chipW, chipH), QStringLiteral("Scopes"),
                 QString::number(rows.size()), accentD);
        drawChip(QRect(contentLeft + (chipW + chipGap), chipsY, chipW, chipH), QStringLiteral("Total"),
                 formatCompactMs(totalMs) + QStringLiteral(" ms"), accentA);
        drawChip(QRect(contentLeft + (chipW + chipGap) * 2, chipsY, chipW, chipH), QStringLiteral("Slowest"),
                 formatCompactMs(slowestMs) + QStringLiteral(" ms"), accentB);
        drawChip(QRect(contentLeft + (chipW + chipGap) * 3, chipsY, chipW, chipH), QStringLiteral("Throughput"),
                 QStringLiteral("%1 Hz").arg(QString::number(rateHz, 'f', 1)), accentC);

        const int listTop = chipsY + chipH + 18;
        const QRect listRect(contentLeft, listTop, contentWidth, bounds.bottom() - listTop - 20);

        p.setPen(colors.border);
        p.setBrush(colors.bg2);
        p.drawRoundedRect(listRect, 16, 16);

        if (!hasData) {
            p.setPen(colors.textMuted);
            p.setFont(metricFont);
            const QString emptyTitle = QStringLiteral("No scopes recorded yet");
            const QString emptyBody = QStringLiteral("Run audio playback or enable profiling to populate this panel.");
            const int cy = listRect.center().y();
            p.drawText(QRect(listRect.left(), cy - 32, listRect.width(), 24),
                       Qt::AlignHCenter | Qt::AlignVCenter, emptyTitle);
            p.setFont(subtitleFont);
            p.drawText(QRect(listRect.left() + 20, cy, listRect.width() - 40, 32),
                       Qt::AlignHCenter | Qt::AlignVCenter, emptyBody);
            return;
        }

        // Table header
        const int headerH = 28;
        const QRect headerRect(listRect.left() + 1, listRect.top() + 1, listRect.width() - 2, headerH);
        p.setBrush(withAlpha(colors.accent, 24));
        p.setPen(Qt::NoPen);
        p.drawRoundedRect(headerRect.adjusted(0, 0, 0, 0), 16, 16);

        p.setFont(subtitleFont);
        p.setPen(colors.textMuted);
        const int nameCol = headerRect.left() + 14;
        const int lastCol = headerRect.right() - 220;
        const int avgCol = headerRect.right() - 155;
        const int pctCol = headerRect.right() - 92;
        const int barCol = headerRect.right() - 70;
        p.drawText(nameCol, headerRect.center().y() + subtitleFm.ascent() / 2, QStringLiteral("Scope"));
        p.drawText(lastCol, headerRect.center().y() + subtitleFm.ascent() / 2, QStringLiteral("last"));
        p.drawText(avgCol, headerRect.center().y() + subtitleFm.ascent() / 2, QStringLiteral("avg"));
        p.drawText(pctCol, headerRect.center().y() + subtitleFm.ascent() / 2, QStringLiteral("%"));
        p.drawText(barCol, headerRect.center().y() + subtitleFm.ascent() / 2, QStringLiteral("share"));

        const int rowTop = headerRect.bottom() + 6;
        const int rowH = 30;
        const int rowsToDraw = std::min(visibleRows, 8);
        const double maxDuration = std::max(1.0, rows.front().durationMs);
        for (int i = 0; i < rowsToDraw; ++i) {
            const SampleRow& row = rows[static_cast<size_t>(i)];
            const QRect rowRect(listRect.left() + 8, rowTop + i * (rowH + 6), listRect.width() - 16, rowH);

            QColor rowBg = QColor(255, 255, 255, 0);
            if (i == 0) {
                rowBg = withAlpha(colors.accent, 20);
            } else if ((i % 2) == 0) {
                rowBg = withAlpha(colors.text, 6);
            }

            p.setPen(Qt::NoPen);
            p.setBrush(rowBg);
            p.drawRoundedRect(rowRect, 12, 12);

            const QColor rowAccent = severityColor(row.durationMs);
            p.fillRect(QRect(rowRect.left(), rowRect.top(), 4, rowRect.height()), rowAccent);

            p.setFont(rowNameFont);
            p.setPen(colors.text);
            const QString clippedName = rowNameFm.elidedText(row.name, Qt::ElideRight, lastCol - nameCol - 24);
            p.drawText(nameCol + 10, rowRect.center().y() + rowNameFm.ascent() / 2, clippedName);

            p.setFont(monoFont);
            p.setPen(colors.textMuted);
            p.drawText(lastCol, rowRect.center().y() + monoFm.ascent() / 2, formatCompactMs(row.durationMs));
            p.drawText(avgCol, rowRect.center().y() + monoFm.ascent() / 2, formatCompactMs(row.durationMs));
            p.drawText(pctCol, rowRect.center().y() + monoFm.ascent() / 2, formatPercent(row.share));

            const QRect barBg(barCol, rowRect.top() + 8, rowRect.right() - barCol - 14, rowRect.height() - 16);
            p.setBrush(withAlpha(colors.text, 9));
            p.setPen(Qt::NoPen);
            p.drawRoundedRect(barBg, 8, 8);

            const int barW = std::max(4, static_cast<int>(barBg.width() * (row.durationMs / maxDuration)));
            const QRect barFill(barBg.left(), barBg.top(), barW, barBg.height());
            QLinearGradient fillGrad(barFill.topLeft(), barFill.topRight());
            fillGrad.setColorAt(0.0, rowAccent.lighter(110));
            fillGrad.setColorAt(1.0, rowAccent.darker(110));
            p.setBrush(fillGrad);
            p.drawRoundedRect(barFill, 8, 8);
        }

        if (static_cast<int>(rows.size()) > rowsToDraw) {
            p.setFont(subtitleFont);
            p.setPen(colors.textMuted);
            const QString tail = QStringLiteral("+ %1 more").arg(rows.size() - rowsToDraw);
            p.drawText(QRect(listRect.right() - 120, listRect.bottom() - 28, 108, 18),
                       Qt::AlignRight | Qt::AlignVCenter, tail);
        }
    }

    void resizeEvent(QResizeEvent* e) override
    {
        QWidget::resizeEvent(e);
        update();
    }

    void keyPressEvent(QKeyEvent* event) override
    {
        if (event && event->matches(QKeySequence::Copy)) {
            copyReportToClipboard();
            event->accept();
            return;
        }
        QWidget::keyPressEvent(event);
    }

private:
    std::map<std::string, ArtifactCore::PerformanceSample> filteredSamples() const
    {
        auto samples = ArtifactCore::PerformanceRegistry::instance().getLatestSamples();
        if (namePrefixFilter_.isEmpty()) {
            return samples;
        }

        std::map<std::string, ArtifactCore::PerformanceSample> filtered;
        const std::string prefix = namePrefixFilter_.toStdString();
        for (const auto& [name, sample] : samples) {
            if (name.rfind(prefix, 0) == 0) {
                filtered.emplace(name, sample);
            }
        }
        return filtered;
    }

    void updateMetrics()
    {
        update();
    }

    QTimer* timer_ = nullptr;
    QString titleText_ = QStringLiteral("Performance Profiler");
    QString namePrefixFilter_;
};

W_OBJECT_IMPL(ArtifactPerformanceProfilerWidget)

} // namespace Artifact
