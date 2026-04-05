module;
#include <QWidget>
#include <QPainter>
#include <QLinearGradient>
#include <wobjectimpl.h>
#include <cmath>
#include <algorithm>
#include <vector>

module Artifact.Widgets.SpectrumAnalyzer;

namespace Artifact {

W_OBJECT_IMPL(SpectrumAnalyzerWidget)

SpectrumAnalyzerWidget::SpectrumAnalyzerWidget(QWidget* parent)
 : QWidget(parent)
{
 setMinimumHeight(40);
 setFixedHeight(60);
 setMinimumWidth(60);
}

SpectrumAnalyzerWidget::~SpectrumAnalyzerWidget() = default;

void SpectrumAnalyzerWidget::setSpectrum(const std::vector<float>& spectrum)
{
 if (spectrum.empty()) return;

 if (spectrum_.size() != spectrum.size()) {
  spectrum_ = spectrum;
 } else {
  for (size_t i = 0; i < spectrum.size(); ++i) {
   if (spectrum[i] > spectrum_[i]) {
    spectrum_[i] = spectrum[i];
   } else {
    spectrum_[i] *= 0.85f;
   }
  }
 }
 update();
}

void SpectrumAnalyzerWidget::paintEvent(QPaintEvent* event)
{
 Q_UNUSED(event);
 QPainter painter(this);
 painter.setRenderHint(QPainter::Antialiasing);

 const int w = width();
 const int h = height();
 painter.fillRect(0, 0, w, h, QColor(10, 10, 10));

 if (spectrum_.empty()) return;

 const int numBins = static_cast<int>(spectrum_.size());
 const int displayBins = std::min(numBins, 32);
 const float barWidth = static_cast<float>(w) / displayBins;

 QLinearGradient grad(0, h, 0, 0);
 grad.setColorAt(0.0, QColor(0, 120, 255, 200));
 grad.setColorAt(0.6, QColor(0, 255, 120, 200));
 grad.setColorAt(1.0, QColor(255, 255, 0, 200));

 for (int i = 0; i < displayBins; ++i) {
  const float freqRatio = static_cast<float>(i) / displayBins;
  const int binIdx = static_cast<int>(std::pow(freqRatio, 1.5f) * (numBins - 1));
  const float val = spectrum_[binIdx];
  const float scaledVal = std::clamp((std::log10(val + 0.001f) + 3.0f) / 3.0f, 0.0f, 1.0f);
  const int barHeight = static_cast<int>(scaledVal * h);

  painter.fillRect(
   static_cast<int>(i * barWidth) + 1,
   h - barHeight,
   static_cast<int>(barWidth) - 1,
   barHeight,
   grad);
 }

 painter.setPen(QPen(QColor(255, 255, 255, 15), 0.5));
 painter.drawLine(0, h / 2, w, h / 2);
 painter.drawLine(0, h / 4, w, h / 4);
 painter.drawLine(0, h * 3 / 4, w, h * 3 / 4);
}

}
