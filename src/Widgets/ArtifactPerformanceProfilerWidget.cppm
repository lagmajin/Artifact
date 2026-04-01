module;
#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QTimer>
#include <QPainter>
#include <QColor>
#include <QFont>
#include <QPalette>
#include <QMap>
#include <wobjectdefs.h>
#include <wobjectimpl.h>

#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <unordered_map>
#include <set>
#include <unordered_set>
#include <memory>
#include <algorithm>
#include <cmath>
#include <functional>
#include <optional>
#include <utility>
#include <array>
#include <mutex>
#include <thread>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <variant>
#include <any>
#include <atomic>
#include <condition_variable>
#include <queue>
#include <deque>
#include <list>
#include <tuple>
#include <numeric>
#include <regex>
#include <random>
export module Artifact.Widgets.PerformanceProfilerWidget;




import ArtifactCore.Utils.PerformanceProfiler;
import Widgets.Utils.CSS;

namespace Artifact {

/**
 * @brief Widget for visualizing performance metrics in real-time
 */
export class ArtifactPerformanceProfilerWidget : public QWidget {
    W_OBJECT(ArtifactPerformanceProfilerWidget)

public:
    explicit ArtifactPerformanceProfilerWidget(QWidget* parent = nullptr)
        : QWidget(parent) 
    {
        setupUI();
        
        timer_ = new QTimer(this);
        connect(timer_, &QTimer::timeout, this, &ArtifactPerformanceProfilerWidget::updateMetrics);
        timer_->start(100); // Update at 10Hz
    }

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);

        auto samples = ArtifactCore::PerformanceRegistry::instance().getLatestSamples();
        if (samples.empty()) return;

        int yOffset = 100;
        int barHeight = 20;
        int maxBarWidth = width() - 200;
        double maxTime = 16.6; // Target 60fps limit initially

        // Draw individual bars for components
        for (const auto& [name, sample] : samples) {
            maxTime = std::max(maxTime, sample.durationMs);
        }

        int count = 0;
        for (const auto& [name, sample] : samples) {
            int x = 120;
            int y = yOffset + count * (barHeight + 10);
            
            // Background
            painter.setPen(Qt::NoPen);
            painter.setBrush(QColor(ArtifactCore::currentDCCTheme().secondaryBackgroundColor));
            painter.drawRect(x, y, maxBarWidth, barHeight);
            
            // Value bar
            int barWidth = static_cast<int>((sample.durationMs / maxTime) * maxBarWidth);
            QColor barColor = sample.durationMs > 16.6 ? QColor(QStringLiteral("#F44336")) : QColor(QStringLiteral("#4CAF50"));
            painter.setBrush(barColor);
            painter.drawRect(x, y, barWidth, barHeight);

            // Label
            painter.setPen(QColor(ArtifactCore::currentDCCTheme().textColor));
            painter.drawText(10, y + 15, QString::fromStdString(name));
            painter.drawText(x + maxBarWidth + 10, y + 15, QString("%1 ms").arg(sample.durationMs, 0, 'f', 2));

            count++;
        }
    }

private:
    void updateMetrics() {
        auto samples = ArtifactCore::PerformanceRegistry::instance().getLatestSamples();
        double totalTime = 0;
        double queueWaitTime = 0; // If it exists, otherwise 0

        std::vector<std::pair<std::string, double>> sortedSamples;

        for (const auto& [name, sample] : samples) {
            totalTime += sample.durationMs;
            if (name.find("Queue") != std::string::npos || name.find("Wait") != std::string::npos) {
                queueWaitTime += sample.durationMs;
            }
            sortedSamples.push_back({name, sample.durationMs});
        }
        
        double fps = 1000.0 / (totalTime + 0.001);
        fpsLabel_->setText(QString("FPS: %1\nFrame: %2 ms\nQ-Wait: %3 ms")
            .arg(fps, 0, 'f', 1)
            .arg(totalTime, 0, 'f', 1)
            .arg(queueWaitTime, 0, 'f', 2));
        
        {
            QPalette pal = fpsLabel_->palette();
            if (fps < 30) pal.setColor(QPalette::WindowText, QColor(QStringLiteral("#F44336")));
            else if (fps < 55) pal.setColor(QPalette::WindowText, QColor(QStringLiteral("#FF9800")));
            else pal.setColor(QPalette::WindowText, QColor(QStringLiteral("#4CAF50")));
            fpsLabel_->setPalette(pal);
        }

        // 重い処理上位3件のログ出力 (1秒に1回)
        static int logCounter = 0;
        if (++logCounter >= 10) {
            std::sort(sortedSamples.begin(), sortedSamples.end(), [](const auto& a, const auto& b) {
                return a.second > b.second;
            });
            qDebug() << "--- Top 3 Heaviest Processes ---";
            for (size_t i = 0; i < std::min<size_t>(3, sortedSamples.size()); ++i) {
                qDebug() << i + 1 << ":" << QString::fromStdString(sortedSamples[i].first) << "-" << QString::number(sortedSamples[i].second, 'f', 2) << "ms";
            }
            logCounter = 0;
        }
        
        update(); // Force repaint
    }

    void setupUI() {
        auto layout = new QVBoxLayout(this);
        layout->setContentsMargins(15, 15, 15, 15);
        layout->setSpacing(10);

        setAutoFillBackground(true);
        QPalette widgetPalette = palette();
        widgetPalette.setColor(QPalette::Window, QColor(ArtifactCore::currentDCCTheme().backgroundColor));
        widgetPalette.setColor(QPalette::WindowText, QColor(ArtifactCore::currentDCCTheme().textColor));
        setPalette(widgetPalette);

        auto header = new QHBoxLayout();
        auto title = new QLabel("Performance Profiler");
        {
            QFont f = title->font();
            f.setBold(true);
            f.setPointSize(14);
            title->setFont(f);
            QPalette pal = title->palette();
            pal.setColor(QPalette::WindowText, QColor(ArtifactCore::currentDCCTheme().textColor).darker(130));
            title->setPalette(pal);
        }
        
        fpsLabel_ = new QLabel("FPS: --");
        {
            QFont f = fpsLabel_->font();
            f.setBold(true);
            f.setPointSize(24);
            fpsLabel_->setFont(f);
            QPalette pal = fpsLabel_->palette();
            pal.setColor(QPalette::WindowText, QColor(QStringLiteral("#4CAF50")));
            fpsLabel_->setPalette(pal);
        }

        header->addWidget(title);
        header->addStretch();
        header->addWidget(fpsLabel_);
        layout->addLayout(header);

        layout->addSpacing(20);
        layout->addStretch(); // Placeholder for painted bars

        setMinimumSize(400, 300);
    }

    QTimer* timer_ = nullptr;
    QLabel* fpsLabel_ = nullptr;
};

W_OBJECT_IMPL(ArtifactPerformanceProfilerWidget)

} // namespace Artifact
