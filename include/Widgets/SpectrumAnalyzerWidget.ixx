module;
#include <QWidget>
#include <vector>

export module Artifact.Widgets.SpectrumAnalyzer;

export namespace Artifact {

/**
 * @brief リアルタイム・スペクトラム表示ウィジェット
 * 音声の周波数成分を棒グラフや波形で表示します。
 */
class SpectrumAnalyzerWidget : public QWidget {
    Q_OBJECT
public:
    SpectrumAnalyzerWidget(QWidget* parent = nullptr);
    virtual ~SpectrumAnalyzerWidget();

    /**
     * @brief スペクトラムデータの更新
     * @param spectrum 0.0 ~ 1.0 の強度が並んだ配列
     */
    void setSpectrum(const std::vector<float>& spectrum);

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    std::vector<float> spectrum_;
};

} // namespace Artifact
