module;

#include <QString>
#include <QColor>
#include <QList>
#include <QMap>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>

export module Artifact.Color.Palette;

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




export namespace Artifact {

struct NamedColor {
    QString name;
    QColor color;
};

class ColorPalette {
public:
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
};

} // namespace Artifact
