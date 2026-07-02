module;

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVector>
#include <memory>

export module Artifact.Render.Batch;

import Utils.Id;
import Artifact.Render.Queue.Service;
import Artifact.Render.Queue.Presets;
import Artifact.Project.Manager;
import Artifact.Project.Items;

export namespace Artifact {

struct BatchTemplate {
    QString name;
    QString outputDirectory;
    QString fileNamePattern;      // %compName%_%date%_%time%
    QString presetId;             // ArtifactRenderFormatPreset id
    int overrideWidth = 0;        // 0 = comp default
    int overrideHeight = 0;
    double overrideFps = 0.0;
    int startFrame = -1;          // -1 = comp default
    int endFrame = -1;
    // 明示的な出力設定（空文字列 = preset の値を使う）
    QString format;                // "MP4", "PNG", "EXR" など
    QString codec;                 // "H.264", "ProRes", "PNG" など
    QString codecProfile;          // "hq", "4444", "high" など
    int overrideBitrate = 0;       // 0 = preset の値を使う
    int framePadding = 4;          // 画像シーケンスのフレーム番号ゼロ埋め桁数
};

class ArtifactBatchRenderer : public QObject {
public:
    explicit ArtifactBatchRenderer(QObject* parent = nullptr);
    ~ArtifactBatchRenderer();

    static ArtifactBatchRenderer* instance();

    // Phase 1: 一括ジョブ追加
    int addAllCompositions(const QString& outputDir,
                           const QString& fileNamePattern = "%compName%_%date%");
    int addCompositions(const QVector<ArtifactCore::CompositionID>& ids,
                        const QString& outputDir,
                        const QString& fileNamePattern = "%compName%_%date%");
    int addCompositionsWithTemplate(const QVector<ArtifactCore::CompositionID>& ids,
                                    const BatchTemplate& tmpl);

    // Phase 3: バッチテンプレート
    QVector<BatchTemplate> availableTemplates() const;
    bool saveTemplate(const BatchTemplate& tmpl);
    bool deleteTemplate(const QString& name);
    BatchTemplate defaultTemplate() const;

    // ファイル名パターン補完
    static QString resolveFileNamePattern(const QString& pattern,
                                           const QString& compName,
                                           int frameNumber = -1);

    void batchJobsAdded(int count);

private:
    class Impl;
    Impl* impl_;
    QString resolveTemplateDir() const;
};

} // namespace Artifact
