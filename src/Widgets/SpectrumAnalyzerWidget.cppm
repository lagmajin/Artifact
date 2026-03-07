module;
#include <QWidget>
#include <QPainter>
#include <QLinearGradient>
#include <cmath>
#include <algorithm>
#include <vector>

module Artifact.Widgets.SpectrumAnalyzer;

namespace Artifact {

SpectrumAnalyzerWidget::SpectrumAnalyzerWidget(QWidget* parent)
    : QWidget(parent) {
    
    setMinimumHeight(40);
    setFixedHeight(60);
    setMinimumWidth(60);
    
    setStyleSheet("background: #121212; border-bottom: 2px solid #333;");
}

SpectrumAnalyzerWidget::~SpectrumAnalyzerWidget() = default;

void SpectrumAnalyzerWidget::setSpectrum(const std::vector<float>& spectrum) {
    if (spectrum.empty()) return;
    
    // 簡易的な平滑化 (Fall-down)
    if (spectrum_.size() != spectrum.size()) {
        spectrum_ = spectrum;
    } else {
        for (size_t i = 0; i < spectrum.size(); ++i) {
            // 前のフレームより低ければ少し下げる、高ければ即反映
            if (spectrum[i] > spectrum_[i]) {
                spectrum_[i] = spectrum[i];
            } else {
                spectrum_[i] *= 0.85f; // 毎フレーム 15% 減衰
            }
        }
    }
    update();
}

void SpectrumAnalyzerWidget::paintEvent(QPaintEvent* event) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    int w = width();
    int h = height();

    // BG (暗いガラス感)
    painter.fillRect(0, 0, w, h, QColor(10, 10, 10));

    if (spectrum_.empty()) return;

    // スペクトラム描画 (棒グラフ)
    int numBins = static_cast<int>(spectrum_.size());
    // 周波数解像度が高すぎる場合は平均化して表示数を抑える
    int displayBins = std::min(numBins, 32); 
    float barWidth = static_cast<float>(w) / displayBins;

    QLinearGradient grad(0, h, 0, 0);
    grad.setColorAt(0.0, QColor(0, 120, 255, 200));  // Blue
    grad.setColorAt(0.6, QColor(0, 255, 120, 200));  // Green
    grad.setColorAt(1.0, QColor(255, 255, 0, 200));  // Yellow

    for (int i = 0; i < displayBins; ++i) {
        // 対数的な周波数分布をシミュレート (低域を広く)
        float freqRatio = static_cast<float>(i) / displayBins;
        int binIdx = static_cast<int>(std::pow(freqRatio, 1.5f) * (numBins - 1));
        
        float val = spectrum_[binIdx];
        
        // デシベル表示に近くするために対数スケール化 (0.01~1.0 -> 0~1.0)
        float scaledVal = std::clamp((std::log10(val + 0.001f) + 3.0f) / 3.0f, 0.0f, 1.0f);
        int barHeight = static_cast<int>(scaledVal * h);

        painter.fillRect(
                static_cast<int>(i * barWidth) + 1,
                h - barHeight,
                static_cast<int>(barWidth) - 1,
                barHeight,
                grad
        );
    }

    // グリッド線
    painter.setPen(QPen(QColor(255, 255, 255, 15), 0.5));
    painter.drawLine(0, h/2, w, h/2);
    painter.drawLine(0, h/4, w, h/4);
    painter.drawLine(0, h*3/4, w, h*3/4);
}

} // namespace Artifact
