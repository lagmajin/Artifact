module;

#include <QString>
#include <QColor>
#include <QList>
#include <QMap>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>

export module Artifact.Color.Palette;

import std;

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
