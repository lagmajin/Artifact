module;
#include "../Define/DllExportMacro.hpp"
#include <wobjectdefs.h>
#include <QWidget>
#include <QJsonObject>
#include <QJsonArray>

export module Artifact.Widgets.ReportDashboard;

import std;

export namespace Artifact {

/**
 * @brief レポートセクションの種類
 */
enum class ReportSectionType {
    Chart,   // Chart.js グラフ (折れ線/棒/円/散布図/レーダー)
    Table,   // テーブル
    Text,    // テキスト/HTML
    Image,   // Base64画像
    Html     // 生HTML
};

/**
 * @brief グラフの種類
 */
enum class ChartType {
    Line, Bar, Pie, Scatter, Radar, Doughnut, Area
};

/**
 * @brief データセット定義
 */
struct LIBRARY_DLL_API ChartDataset {
    std::string label;
    std::vector<double> data;
    std::string color = "#3b82f6";     // 線/塗りの色
    std::string backgroundColor;        // 塗りつぶし色 (省略時はcolorから自動)
    bool fill = false;                  // 面塗りつぶし
    float tension = 0.3f;              // 曲線の滑らかさ
};

/**
 * @brief レポートの1セクション
 */
struct LIBRARY_DLL_API ReportSection {
    ReportSectionType type = ReportSectionType::Text;
    std::string title;
    // Chart用
    ChartType chartType = ChartType::Line;
    std::vector<std::string> labels;
    std::vector<ChartDataset> datasets;
    // Table用
    std::vector<std::string> columns;
    std::vector<std::vector<std::string>> rows;
    // Text/Image/Html用
    std::string content;     // テキスト or HTML or base64 image
    int imageWidth = 800;    // Image の表示幅
    int imageHeight = 600;
};

/**
 * @brief Webベースのグラフィカルレポートダッシュボード
 *
 * ArtifactWebUIHost + QWebChannel を利用し、C++側からJSONで
 * レポート定義を送信するとChromiumベースのWebViewでグラフや表を描画する。
 *
 * 使い方:
 *   auto* dash = new ReportDashboard(webHost);
 *   dash->setTitle("パフォーマンスレポート");
 *
 *   ReportSection sec;
 *   sec.type = ReportSectionType::Chart;
 *   sec.title = "フレーム時間";
 *   sec.chartType = ChartType::Line;
 *   sec.labels = {"0", "1", "2", "3", "4"};
 *   ChartDataset ds; ds.label = "ms"; ds.data = {16.6, 18.2, 15.1, 22.3, 17.0};
 *   sec.datasets.push_back(ds);
 *   dash->addSection(sec);
 *
 *   dash->render();
 */
class LIBRARY_DLL_API ReportDashboard : public QObject {
    W_OBJECT(ReportDashboard)

public:
    explicit ReportDashboard(class ArtifactWebUIHost* host, QObject* parent = nullptr);
    ~ReportDashboard();

    /// レポートタイトル
    void setTitle(const std::string& title);
    /// サブタイトル
    void setSubtitle(const std::string& subtitle);
    /// セクションを追加
    void addSection(const ReportSection& section);
    /// 全セクションをクリア
    void clear();

    /// JSONをビルドしてWebViewに送信し描画
    void render();

    // ── エクスポート ──

    /// 標準JSON文字列
    std::string toJson() const;

    /// JSON5形式（コメント付き・末尾カンマ許容・キー引用符不要）
    std::string toJson5() const;

    /// スタンドアロンHTMLファイル（ブラウザで単体表示可能）
    std::string toStandaloneHtml() const;

    /// Markdownテーブル（Graph/Tableセクションのみ）
    std::string toMarkdown() const;

    /// CSV形式（Tableセクションのみ）
    std::string toCsv() const;

    /// ファイル保存（拡張子から形式を自動判定: .json .json5 .html .md .csv）
    bool saveToFile(const std::string& filepath) const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace Artifact
