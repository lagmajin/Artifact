module;
#include <wobjectimpl.h>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>
#include <QDebug>
#include <sstream>
#include <fstream>
#include <filesystem>

module Artifact.Widgets.ReportDashboard;

import Artifact.Widgets.WebUIHost;

namespace Artifact {

    W_OBJECT_IMPL(ReportDashboard)

    static const char* chartTypeName(ChartType t) {
        switch (t) {
            case ChartType::Line: return "line"; case ChartType::Bar: return "bar";
            case ChartType::Pie: return "pie"; case ChartType::Scatter: return "scatter";
            case ChartType::Radar: return "radar"; case ChartType::Doughnut: return "doughnut";
            case ChartType::Area: return "line"; default: return "bar";
        }
    }

    static const char* sectionTypeName(ReportSectionType t) {
        switch (t) {
            case ReportSectionType::Chart: return "chart";
            case ReportSectionType::Table: return "table";
            case ReportSectionType::Text:  return "text";
            case ReportSectionType::Image: return "image";
            case ReportSectionType::Html:  return "html";
            default: return "text";
        }
    }

    struct ReportDashboard::Impl {
        ArtifactWebUIHost* host = nullptr;
        std::string title, subtitle;
        std::vector<ReportSection> sections;
        bool templateLoaded = false;

        QJsonObject buildSectionJson(const ReportSection& sec) const {
            QJsonObject obj;
            obj["type"] = sectionTypeName(sec.type);
            obj["title"] = QString::fromStdString(sec.title);
            if (sec.type == ReportSectionType::Chart) {
                obj["chartType"] = chartTypeName(sec.chartType);
                QJsonArray labelsArr;
                for (auto& l : sec.labels) labelsArr.append(QString::fromStdString(l));
                obj["labels"] = labelsArr;
                QJsonArray datasetsArr;
                for (auto& ds : sec.datasets) {
                    QJsonObject dsObj;
                    dsObj["label"] = QString::fromStdString(ds.label);
                    QJsonArray dataArr;
                    for (auto v : ds.data) dataArr.append(v);
                    dsObj["data"] = dataArr;
                    dsObj["borderColor"] = QString::fromStdString(ds.color);
                    dsObj["backgroundColor"] = ds.backgroundColor.empty()
                        ? QJsonValue(QString::fromStdString(ds.color) + "33")
                        : QJsonValue(QString::fromStdString(ds.backgroundColor));
                    dsObj["fill"] = (sec.chartType == ChartType::Area) ? true : ds.fill;
                    dsObj["tension"] = ds.tension;
                    datasetsArr.append(dsObj);
                }
                obj["datasets"] = datasetsArr;
            } else if (sec.type == ReportSectionType::Table) {
                QJsonArray colsArr;
                for (auto& c : sec.columns) colsArr.append(QString::fromStdString(c));
                obj["columns"] = colsArr;
                QJsonArray rowsArr;
                for (auto& row : sec.rows) {
                    QJsonArray r;
                    for (auto& cell : row) r.append(QString::fromStdString(cell));
                    rowsArr.append(r);
                }
                obj["rows"] = rowsArr;
            } else if (sec.type == ReportSectionType::Image) {
                obj["imageWidth"] = sec.imageWidth;
                obj["imageHeight"] = sec.imageHeight;
                obj["content"] = QString::fromStdString(sec.content);
            } else {
                obj["content"] = QString::fromStdString(sec.content);
            }
            return obj;
        }
    };

    ReportDashboard::ReportDashboard(ArtifactWebUIHost* host, QObject* parent)
        : QObject(parent), impl_(std::make_unique<Impl>()) { impl_->host = host; }
    ReportDashboard::~ReportDashboard() = default;

    void ReportDashboard::setTitle(const std::string& t) { impl_->title = t; }
    void ReportDashboard::setSubtitle(const std::string& s) { impl_->subtitle = s; }
    void ReportDashboard::addSection(const ReportSection& s) { impl_->sections.push_back(s); }
    void ReportDashboard::clear() { impl_->sections.clear(); }

    void ReportDashboard::render() {
        if (!impl_->host) { qWarning() << "[ReportDashboard] No host"; return; }
        if (!impl_->templateLoaded) {
            impl_->host->loadUrl(QStringLiteral("qrc:/webui/report/template.html"));
            impl_->templateLoaded = true;
        }
        QJsonObject root;
        root["title"] = QString::fromStdString(impl_->title);
        root["subtitle"] = QString::fromStdString(impl_->subtitle);
        QJsonArray sectionsArr;
        for (auto& sec : impl_->sections) sectionsArr.append(impl_->buildSectionJson(sec));
        root["sections"] = sectionsArr;
        QString json = QJsonDocument(root).toJson(QJsonDocument::Compact);
        QString js = QStringLiteral("if(window.renderReport)renderReport(%1);else window.__pendingReport=%1;").arg(json);
        impl_->host->runJavaScript(js);
        qDebug() << "[ReportDashboard] Rendered" << impl_->sections.size() << "sections";
    }

    std::string ReportDashboard::toJson() const {
        QJsonObject root;
        root["title"] = QString::fromStdString(impl_->title);
        QJsonArray sectionsArr;
        for (auto& sec : impl_->sections) sectionsArr.append(impl_->buildSectionJson(sec));
        root["sections"] = sectionsArr;
        return QJsonDocument(root).toJson(QJsonDocument::Indented).toStdString();
    }


    std::string ReportDashboard::toJson5() const {
        std::ostringstream ss;
        ss << "// Report: " << impl_->title << "\n";
        ss << "// Subtitle: " << impl_->subtitle << "\n";
        ss << "// Sections: " << impl_->sections.size() << "\n";
        ss << "{\n";
        ss << "  title: \"" << impl_->title << "\",\n";
        ss << "  subtitle: \"" << impl_->subtitle << "\",\n";
        ss << "  sections: [\n";
        for (size_t si = 0; si < impl_->sections.size(); ++si) {
            auto& sec = impl_->sections[si];
            ss << "    {\n";
            ss << "      type: '" << sectionTypeName(sec.type) << "',\n";
            ss << "      title: '" << sec.title << "',\n";
            if (sec.type == ReportSectionType::Chart) {
                ss << "      chartType: '" << chartTypeName(sec.chartType) << "',\n";
                ss << "      labels: [";
                for (size_t i = 0; i < sec.labels.size(); ++i) { ss << "'" << sec.labels[i] << "'"; if (i+1<sec.labels.size()) ss << ", "; }
                ss << "],\n";
                ss << "      datasets: [\n";
                for (size_t di=0;di<sec.datasets.size();++di){auto&ds=sec.datasets[di];
                    ss<<"        {label:'"<<ds.label<<"', data:[";
                    for(size_t j=0;j<ds.data.size();++j){ss<<ds.data[j];if(j+1<ds.data.size())ss<<", ";}
                    ss<<"], color:'"<<ds.color<<"'}"; if(di+1<sec.datasets.size())ss<<","; ss<<"\n";}
                ss << "      ],\n";
            } else if (sec.type == ReportSectionType::Table) {
                ss << "      columns: [";
                for (size_t i = 0; i < sec.columns.size(); ++i) { ss << "'" << sec.columns[i] << "'"; if (i+1<sec.columns.size()) ss << ", "; }
                ss << "],\n";
                ss << "      rows: [\n";
                for (size_t ri=0;ri<sec.rows.size();++ri){ss<<"        [";
                    for(size_t ci=0;ci<sec.rows[ri].size();++ci){ss<<"'"<<sec.rows[ri][ci]<<"'";
                    if(ci+1<sec.rows[ri].size())ss<<", ";} ss<<"]";if(ri+1<sec.rows.size())ss<<",";ss<<"\n";}
                ss << "      ],\n";
            }
            ss << "    }"; if (si+1 < impl_->sections.size()) ss << ","; ss << "\n";
        }
        ss << "  ],\n";
        ss << "}\n";
        return ss.str();
    }


    std::string ReportDashboard::toStandaloneHtml() const {
        std::string json = toJson();
        std::ostringstream ss;
        ss << "<!DOCTYPE html>\n<html lang=\"en\">\n<head>\n<meta charset=\"UTF-8\">\n";
        ss << "<meta name=\"viewport\"content=\"width=device-width,initial-scale=1.0\">\n";
        ss << "<title>" << impl_->title << "</title>\n";
        ss << "<script src=\"https://cdn.jsdelivr.net/npm/chart.js@4.4.0/dist/chart.umd.min.js\"><\\/script>\n";
        ss << "<style>\n:root{--bg:#1e1e2e;--card:#252536;--text:#cdd6f4;--muted:#6c7086;";
        ss << "--accent:#89b4fa;--border:#313244}\n*{margin:0;padding:0;box-sizing:border-box}\n";
        ss << "body{font-family:'Segoe UI',system-ui,sans-serif;background:var(--bg);color:var(--text);padding:24px}\n";
        ss << ".header{text-align:center;margin-bottom:32px}\n";
        ss << ".header h1{font-size:28px;font-weight:600;color:var(--accent)}\n";
        ss << ".header p{font-size:14px;color:var(--muted)}\n";
        ss << ".grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(460px,1fr));gap:20px}\n";
        ss << ".card{background:var(--card);border:1px solid var(--border);border-radius:12px;padding:20px}\n";
        ss << ".card h3{font-size:15px;font-weight:600;color:var(--muted);text-transform:uppercase;margin-bottom:12px}\n";
        ss << ".chart-wrap canvas{max-height:320px}\n";
        ss << "table{width:100%;border-collapse:collapse;font-size:13px}\n";
        ss << "th{text-align:left;color:var(--muted);padding:8px 12px;border-bottom:2px solid var(--border)}\n";
        ss << "td{padding:8px 12px;border-bottom:1px solid var(--border)}\n";
        ss << "</style>\n</head>\n<body>\n";
        ss << "<div class=\"header\"><h1>" << impl_->title << "</h1>";
        if (!impl_->subtitle.empty()) ss << "<p>" << impl_->subtitle << "</p>";
        ss << "</div>\n<div class=\"grid\"id=\"grid\"></div>\n<script>\nconst DATA=" << json << ";";
        ss << "\ndocument.addEventListener('DOMContentLoaded',function(){var g=document.getElementById('grid');";
        ss << "DATA.sections.forEach(function(s){var c=document.createElement('div');c.className='card';";
        ss << "c.innerHTML='<h3>'+s.title+'</h3>';";
        ss << "if(s.type==='chart'){var w=document.createElement('div');w.className='chart-wrap';";
        ss << "var cv=document.createElement('canvas');w.appendChild(cv);c.appendChild(w);";
        ss << "new Chart(cv,{type:s.chartType,data:{labels:s.labels,datasets:s.datasets.map(function(d){return{label:d.label,data:d.data,borderColor:d.borderColor||'#89b4fa',backgroundColor:d.backgroundColor||(d.borderColor||'#89b4fa')+'33',fill:d.fill||false,tension:0.3,pointRadius:2,borderWidth:2}})},options:{responsive:true,maintainAspectRatio:false,scales:s.chartType==='pie'||s.chartType==='doughnut'?{}:{x:{ticks:{color:'#6c7086'}},y:{ticks:{color:'#6c7086'},beginAtZero:true}}}})}";
        ss << "else if(s.type==='table'){var h='<table><thead><tr>';s.columns.forEach(function(c){h+='<th>'+c+'</th>'});h+='</tr></thead><tbody>';s.rows.forEach(function(r){h+='<tr>';r.forEach(function(c){h+='<td>'+c+'</td>'});h+='</tr>'});h+='</tbody></table>';c.innerHTML+=h}";
        ss << "else{c.innerHTML+='<div style=\"font-size:14px;line-height:1.7\">'+s.content+'</div>'}";
        ss << "g.appendChild(c)});});\n</script>\n</body>\n</html>";
        return ss.str();
    }


    std::string ReportDashboard::toMarkdown() const {
        std::ostringstream ss;
        ss << "# " << impl_->title << "\n\n";
        if (!impl_->subtitle.empty()) ss << "> " << impl_->subtitle << "\n\n";
        for (auto& sec : impl_->sections) {
            ss << "## " << sec.title << "\n\n";
            if (sec.type == ReportSectionType::Table) {
                ss << "|"; for (auto& c : sec.columns) ss << " " << c << " |"; ss << "\n";
                ss << "|"; for (size_t i=0;i<sec.columns.size();++i) ss << " --- |"; ss << "\n";
                for (auto& r : sec.rows) {
                    ss << "|"; for (auto& c : r) ss << " " << c << " |"; ss << "\n";
                }
                ss << "\n";
            } else if (sec.type == ReportSectionType::Text) {
                ss << sec.content << "\n\n";
            } else {
                ss << "(" << sectionTypeName(sec.type) << " section)\n\n";
            }
        }
        return ss.str();
    }

    std::string ReportDashboard::toCsv() const {
        std::ostringstream ss;
        for (auto& sec : impl_->sections) {
            if (sec.type != ReportSectionType::Table) continue;
            ss << "# " << sec.title << "\n";
            for (size_t i = 0; i < sec.columns.size(); ++i) { if (i > 0) ss << ","; ss << sec.columns[i]; }
            ss << "\n";
            for (auto& r : sec.rows) {
                for (size_t i = 0; i < r.size(); ++i) { if (i > 0) ss << ","; ss << r[i]; }
                ss << "\n";
            }
            ss << "\n";
        }
        return ss.str();
    }

    bool ReportDashboard::saveToFile(const std::string& filepath) const {
        namespace fs = std::filesystem;
        auto ext = fs::path(filepath).extension().string();
        std::string content;
        if (ext == ".json5") content = toJson5();
        else if (ext == ".html" || ext == ".htm") content = toStandaloneHtml();
        else if (ext == ".md") content = toMarkdown();
        else if (ext == ".csv") content = toCsv();
        else content = toJson();

        std::ofstream out(filepath, std::ios::binary);
        if (!out) return false;
        out.write(content.data(), content.size());
        return out.good();
    }

} // namespace Artifact
