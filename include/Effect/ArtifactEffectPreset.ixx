module;

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
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <variant>
#include <any>
#include <atomic>
#include <queue>
#include <deque>
#include <list>
#include <tuple>
#include <numeric>
#include <regex>
#include <random>
#include <QString>
#include <QVector>
#include <QJsonObject>
#include <QColor>
#include <QUuid>
export module Artifact.Effect.Preset;





import Artifact.Effect.Abstract;
import Serialization.ISerializable;

export namespace Artifact
{

// エフェクトプリセット（AfterEffectsの「エフェクトお気に入りに保存」一样的機能）
class ArtifactEffectPreset : public ArtifactCore::Serialization::ISerializable
{
public:
    using PresetID = QString;  // UUIDベースのID

private:
    class Impl;
    Impl* impl_;

public:
    ArtifactEffectPreset();
    explicit ArtifactEffectPreset(const QString& name);
    ~ArtifactEffectPreset();
    ArtifactEffectPreset(const ArtifactEffectPreset& other);
    ArtifactEffectPreset& operator=(const ArtifactEffectPreset& other);
    ArtifactEffectPreset(ArtifactEffectPreset&& other);
    ArtifactEffectPreset& operator=(ArtifactEffectPreset&& other);

    // プリセットID（UUID）
    PresetID id() const;
    void setId(const PresetID& id);

    // プリセット名
    QString name() const;
    void setName(const QString& name);

    // カテゴリ（分類）
    QString category() const;
    void setCategory(const QString& category);

    // 説明
    QString description() const;
    void setDescription(const QString& desc);

    // 保存されたエフェクトパラメータ
    void addParameter(const QString& paramName, float value);
    void addParameter(const QString& paramName, double value);
    void addParameter(const QString& paramName, int value);
    void addParameter(const QString& paramName, bool value);
    void addParameter(const QString& paramName, const QColor& color);
    void addParameter(const QString& paramName, const QString& value);

    float getFloatParameter(const QString& paramName) const;
    double getDoubleParameter(const QString& paramName) const;
    int getIntegerParameter(const QString& paramName) const;
    bool getBooleanParameter(const QString& paramName) const;
    QColor getColorParameter(const QString& paramName) const;
    QString getStringParameter(const QString& paramName) const;

    // すべてのパラメータ
    struct Parameter
    {
        enum Type {
            Float = 0,
            Color = 1,
            String = 2,
            Integer = 3,
            Boolean = 4,
            Double = 5
        };
        Type type;
        QString name;
        float floatValue = 0.0f;
        double doubleValue = 0.0;
        int integerValue = 0;
        bool booleanValue = false;
        QColor colorValue;
        QString stringValue;
    };
    QVector<Parameter> allParameters() const;

    // シリアライズ
    QJsonObject toJson() const;
    static ArtifactEffectPreset fromJson(const QJsonObject& json);
    QJsonObject serialize() const override { return toJson(); }
    bool deserialize(const QJsonObject& json) override;
    QString typeName() const override { return QStringLiteral("ArtifactEffectPreset"); }
    int schemaVersion() const override { return 2; }

    // 適用（既存のArtifactAbstractEffectにパラメータを適用）
    void applyTo(ArtifactAbstractEffect* effect) const;

    // サムネイル（オプション）
    QByteArray thumbnail() const;
    void setThumbnail(const QByteArray& data);
};

// エフェクトプリセットコレクション
class ArtifactEffectPresetCollection
{
private:
    class Impl;
    Impl* impl_;

public:
    ArtifactEffectPresetCollection();
    ~ArtifactEffectPresetCollection();

    // プリセット作成/削除
    ArtifactEffectPreset* createPreset(const QString& name);
    void deletePreset(const ArtifactEffectPreset::PresetID& id);

    // プリセット取得
    ArtifactEffectPreset* getPreset(const ArtifactEffectPreset::PresetID& id);
    const ArtifactEffectPreset* getPreset(const ArtifactEffectPreset::PresetID& id) const;

    // カテゴリ別取得
    QVector<ArtifactEffectPreset*> getPresetsByCategory(const QString& category) const;
    QStringList allCategories() const;

    // すべて取得
    QVector<ArtifactEffectPreset*> allPresets();
    QVector<const ArtifactEffectPreset*> allPresets() const;

    // プリセット数
    int presetCount() const;
    bool isEmpty() const;

    // ファイル保存/読込
    bool saveToFile(const QString& filePath) const;
    bool loadFromFile(const QString& filePath);

    // デフォルトプリセット読み込み
    void loadDefaultPresets();
};

// 組み込みプリセットカテゴリ
namespace PresetCategories
{
    constexpr const char* Blur = "Blur";
    constexpr const char* Color = "Color Correction";
    constexpr const char* Distort = "Distort";
    constexpr const char* Generate = "Generate";
    constexpr const char* Noise = "Noise";
    constexpr const char* Stylize = "Stylize";
    constexpr const char* Text = "Text";
    constexpr const char* Transition = "Transition";
    constexpr const char* Utility = "Utility";
    constexpr const char* Custom = "Custom";
}

} // namespace Artifact
