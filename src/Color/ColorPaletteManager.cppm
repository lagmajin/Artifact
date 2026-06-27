module;

#include <QColor>
#include <QDebug>
#include <QFile>
#include <QSaveFile>
#include <QStandardPaths>
#include <QUuid>

#include <algorithm>

module Artifact.Color.Palette;

import Color.Float;

namespace ArtifactCore::Color {

namespace {

constexpr int kSchemaVersion = 1;

FloatColor clampColor(const FloatColor& color)
{
    return FloatColor(std::clamp(color.r(), 0.0f, 1.0f),
                      std::clamp(color.g(), 0.0f, 1.0f),
                      std::clamp(color.b(), 0.0f, 1.0f),
                      std::clamp(color.a(), 0.0f, 1.0f));
}

FloatColor colorFromJsonObject(const QJsonObject& obj, bool* ok = nullptr)
{
    const bool hasComponents = obj.contains(QStringLiteral("r")) &&
                               obj.contains(QStringLiteral("g")) &&
                               obj.contains(QStringLiteral("b"));
    if (!hasComponents) {
        if (ok) *ok = false;
        return FloatColor(1.0f, 0.0f, 1.0f, 1.0f);
    }

    if (ok) *ok = true;
    return clampColor(FloatColor(static_cast<float>(obj.value(QStringLiteral("r")).toDouble(0.0)),
                                 static_cast<float>(obj.value(QStringLiteral("g")).toDouble(0.0)),
                                 static_cast<float>(obj.value(QStringLiteral("b")).toDouble(0.0)),
                                 static_cast<float>(obj.value(QStringLiteral("a")).toDouble(1.0))));
}

QJsonObject colorToJsonObject(const FloatColor& color)
{
    QJsonObject obj;
    obj[QStringLiteral("r")] = color.r();
    obj[QStringLiteral("g")] = color.g();
    obj[QStringLiteral("b")] = color.b();
    obj[QStringLiteral("a")] = color.a();
    return obj;
}

FloatColor colorFromLegacyHex(const QString& hex, bool* ok = nullptr)
{
    QColor q = QColor::fromString(hex);
    if (!q.isValid()) {
        if (ok) *ok = false;
        return FloatColor(1.0f, 0.0f, 1.0f, 1.0f);
    }

    if (ok) *ok = true;
    return clampColor(FloatColor(q.redF(), q.greenF(), q.blueF(), q.alphaF()));
}

QString generateId()
{
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

} // namespace

QJsonObject ColorPalette::toJson() const
{
    QJsonObject obj;
    obj[QStringLiteral("schema_version")] = schemaVersion;
    obj[QStringLiteral("palette_name")] = name;

    QJsonArray colorsArray;
    for (const auto& nc : colors) {
        QJsonObject colorItem;
        colorItem[QStringLiteral("id")] = nc.name;
        colorItem[QStringLiteral("name")] = nc.name;
        colorItem[QStringLiteral("color")] = colorToJsonObject(clampColor(nc.color));
        colorsArray.append(colorItem);
    }
    obj[QStringLiteral("entries")] = colorsArray;
    return obj;
}

ColorPalette ColorPalette::fromJson(const QJsonObject& json)
{
    ColorPalette cp;
    cp.schemaVersion = json.value(QStringLiteral("schema_version")).toInt(kSchemaVersion);
    cp.name = json.value(QStringLiteral("palette_name")).toString();

    const QJsonArray entries = json.contains(QStringLiteral("entries"))
                                 ? json.value(QStringLiteral("entries")).toArray()
                                 : json.value(QStringLiteral("colors")).toArray();

    for (int i = 0; i < entries.size(); ++i) {
        const QJsonObject entry = entries.at(i).toObject();
        NamedColor nc;
        nc.name = entry.value(QStringLiteral("name")).toString();
        if (nc.name.isEmpty()) {
            nc.name = entry.value(QStringLiteral("id")).toString();
        }
        if (nc.name.isEmpty()) {
            nc.name = QStringLiteral("Color %1").arg(i + 1);
        }
        if (nc.name.isEmpty()) {
            nc.name = generateId();
        }

        bool ok = false;
        if (entry.contains(QStringLiteral("color")) && entry.value(QStringLiteral("color")).isObject()) {
            nc.color = colorFromJsonObject(entry.value(QStringLiteral("color")).toObject(), &ok);
        } else if (entry.contains(QStringLiteral("color_hex"))) {
            nc.color = colorFromLegacyHex(entry.value(QStringLiteral("color_hex")).toString(), &ok);
        } else {
            ok = false;
            nc.color = FloatColor(1.0f, 0.0f, 1.0f, 1.0f);
        }

        if (!ok) {
            qWarning() << "[ColorPalette] invalid color entry"
                       << "palette=" << cp.name
                       << "name=" << nc.name;
        }
        cp.colors.append(nc);
    }
    return cp;
}

bool ColorPaletteManager::addPalette(const ColorPalette& palette)
{
    if (palette.name.isEmpty()) {
        lastError_ = QStringLiteral("Palette name is empty");
        return false;
    }
    palettes_[palette.name] = palette;
    return true;
}

bool ColorPaletteManager::removePalette(const QString& name)
{
    return palettes_.remove(name) > 0;
}

ColorPalette* ColorPaletteManager::getPalette(const QString& name)
{
    if (palettes_.contains(name)) return &palettes_[name];
    return nullptr;
}

QStringList ColorPaletteManager::paletteNames() const
{
    return palettes_.keys();
}

bool ColorPaletteManager::loadFromFile(const QString& filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        lastError_ = QStringLiteral("Failed to open palette file for reading");
        return false;
    }

    const QByteArray data = file.readAll();
    const QJsonDocument doc = QJsonDocument::fromJson(data);
    if (doc.isNull()) {
        lastError_ = QStringLiteral("Invalid JSON");
        return false;
    }

    palettes_.clear();
    if (doc.isArray()) {
        for (const auto& value : doc.array()) {
            if (!value.isObject()) continue;
            const ColorPalette palette = ColorPalette::fromJson(value.toObject());
            if (!palette.name.isEmpty()) {
                palettes_[palette.name] = palette;
            }
        }
        return true;
    }

    if (doc.isObject()) {
        const auto root = doc.object();
        const ColorPalette palette = ColorPalette::fromJson(root);
        if (!palette.name.isEmpty()) {
            palettes_[palette.name] = palette;
            return true;
        }
    }

    lastError_ = QStringLiteral("Unsupported palette document format");
    return false;
}

bool ColorPaletteManager::saveToFile(const QString& filePath) const
{
    QJsonArray root;
    for (const auto& cp : palettes_) {
        root.append(cp.toJson());
    }

    QSaveFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }

    const QJsonDocument doc(root);
    if (file.write(doc.toJson(QJsonDocument::Indented)) < 0) {
        return false;
    }
    return file.commit();
}

} // namespace ArtifactCore::Color
