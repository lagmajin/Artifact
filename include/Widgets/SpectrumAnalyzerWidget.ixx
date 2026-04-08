module;
#include <utility>
#include <vector>

#include <wobjectdefs.h>
#include <QWidget>
export module Artifact.Widgets.SpectrumAnalyzer;



export namespace Artifact {

class SpectrumAnalyzerWidget : public QWidget {
 W_OBJECT(SpectrumAnalyzerWidget)
public:
 SpectrumAnalyzerWidget(QWidget* parent = nullptr);
 virtual ~SpectrumAnalyzerWidget();

 void setSpectrum(const std::vector<float>& spectrum);

protected:
 void paintEvent(QPaintEvent* event) override;

private:
 std::vector<float> spectrum_;
};

}
