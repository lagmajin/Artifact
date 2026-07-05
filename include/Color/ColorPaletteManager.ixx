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
#include <QColor>
#include <QList>
#include <QMap>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QStringList>
#include <wobjectimpl.h>
export module Artifact.Color.Palette;

import Color.Float;





export namespace ArtifactCore::Color {

struct NamedColor {
    QString name;
    ArtifactCore::FloatColor color;
};

class ColorPalette {
public:
    int schemaVersion = 1;
    QString name;
    QList<NamedColor> colors;

    QJsonObject toJson() const;
    static ColorPalette fromJson(const QJsonObject& json);
};

class ColorPaletteManager {
private:
    QMap<QString, ColorPalette> palettes_;
    QString lastError_;

public:
    bool addPalette(const ColorPalette& palette);
    bool removePalette(const QString& name);
    ColorPalette* getPalette(const QString& name);
    QStringList paletteNames() const;

    bool loadFromFile(const QString& filePath);
    bool saveToFile(const QString& filePath) const;

    QString lastError() const { return lastError_; }
    void setLastError(const QString& error) { lastError_ = error; }
};

} // namespace ArtifactCore::Color

namespace Artifact {
using ArtifactCore::Color::NamedColor;
using ArtifactCore::Color::ColorPalette;
using ArtifactCore::Color::ColorPaletteManager;
}

W_REGISTER_ARGTYPE(Artifact::ColorPalette)
