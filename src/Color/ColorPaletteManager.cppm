module;

#include <QColor>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QFile>

module Artifact.Color.Palette;

import std;

namespace Artifact {

QJsonObject ColorPalette::toJson() const {
    QJsonObject obj;
    obj["palette_name"] = name;
    QJsonArray colorsArray;
    for (const auto& nc : colors) {
        QJsonObject colorItem;
        colorItem["name"] = nc.name;
        colorItem["color_hex"] = nc.color.name(QColor::HexArgb);
        colorsArray.append(colorItem);
    }
    obj["colors"] = colorsArray;
    return obj;
}

ColorPalette ColorPalette::fromJson(const QJsonObject& json) {
    ColorPalette cp;
    cp.name = json["palette_name"].toString();
    QJsonArray colorsArray = json["colors"].toArray();
    for (int i = 0; i < colorsArray.size(); ++i) {
        QJsonObject colorItem = colorsArray[i].toObject();
        NamedColor nc;
        nc.name = colorItem["name"].toString();
        nc.color = QColor::fromString(colorItem["color_hex"].toString());
        cp.colors.append(nc);
    }
    return cp;
}

bool ColorPaletteManager::addPalette(const ColorPalette& palette) {
    if (palette.name.isEmpty()) return false;
    palettes_[palette.name] = palette;
    return true;
}

bool ColorPaletteManager::removePalette(const QString& name) {
    return palettes_.remove(name) > 0;
}

ColorPalette* ColorPaletteManager::getPalette(const QString& name) {
    if (palettes_.contains(name)) return &palettes_[name];
    return nullptr;
}

QStringList ColorPaletteManager::paletteNames() const {
    return palettes_.keys();
}

bool ColorPaletteManager::loadFromFile(const QString& filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) return false;

    QByteArray data = file.readAll();
    file.close();

    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (doc.isNull() || !doc.isArray()) return false;

    QJsonArray root = doc.array();
    for (int i = 0; i < root.size(); ++i) {
        addPalette(ColorPalette::fromJson(root.at(i).toObject()));
    }
    return true;
}

bool ColorPaletteManager::saveToFile(const QString& filePath) const {
    QJsonArray root;
    for (const auto& cp : palettes_) {
        root.append(cp.toJson());
    }

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) return false;

    QJsonDocument doc(root);
    file.write(doc.toJson());
    file.close();
    return true;
}

} // namespace Artifact
