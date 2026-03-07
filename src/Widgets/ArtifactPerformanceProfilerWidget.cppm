module;
#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QTimer>
#include <QPainter>
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
            painter.setBrush(QColor(50, 50, 50));
            painter.drawRect(x, y, maxBarWidth, barHeight);
            
            // Value bar
            int barWidth = static_cast<int>((sample.durationMs / maxTime) * maxBarWidth);
            QColor barColor = sample.durationMs > 16.6 ? QColor(255, 100, 100) : QColor(100, 255, 100);
            painter.setBrush(barColor);
            painter.drawRect(x, y, barWidth, barHeight);

            // Label
            painter.setPen(QColor(220, 220, 220));
            painter.drawText(10, y + 15, QString::fromStdString(name));
            painter.drawText(x + maxBarWidth + 10, y + 15, QString("%1 ms").arg(sample.durationMs, 0, 'f', 2));

            count++;
        }
    }

private:
    void updateMetrics() {
        auto samples = ArtifactCore::PerformanceRegistry::instance().getLatestSamples();
        double totalTime = 0;
        for (const auto& [name, sample] : samples) {
            totalTime += sample.durationMs;
        }
        
        double fps = 1000.0 / (totalTime + 0.001);
        fpsLabel_->setText(QString("FPS: %1").arg(fps, 0, 'f', 1));
        
        if (fps < 30) fpsLabel_->setStyleSheet("color: #F44336; font-size: 24px; font-weight: bold;");
        else if (fps < 55) fpsLabel_->setStyleSheet("color: #FF9800; font-size: 24px; font-weight: bold;");
        else fpsLabel_->setStyleSheet("color: #4CAF50; font-size: 24px; font-weight: bold;");
        
        update(); // Force repaint
    }

    void setupUI() {
        auto layout = new QVBoxLayout(this);
        layout->setContentsMargins(15, 15, 15, 15);
        layout->setSpacing(10);

        auto header = new QHBoxLayout();
        auto title = new QLabel("Performance Profiler");
        title->setStyleSheet("color: #aaa; font-size: 14px; font-weight: bold;");
        
        fpsLabel_ = new QLabel("FPS: --");
        fpsLabel_->setStyleSheet("color: #4CAF50; font-size: 24px; font-weight: bold;");

        header->addWidget(title);
        header->addStretch();
        header->addWidget(fpsLabel_);
        layout->addLayout(header);

        layout->addSpacing(20);
        layout->addStretch(); // Placeholder for painted bars

        setStyleSheet("background-color: #1a1a1a;");
        setMinimumSize(400, 300);
    }

    QTimer* timer_ = nullptr;
    QLabel* fpsLabel_ = nullptr;
};

W_OBJECT_IMPL(ArtifactPerformanceProfilerWidget)

} // namespace Artifact
