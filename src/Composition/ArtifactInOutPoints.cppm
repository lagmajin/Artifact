module;

#include <algorithm>
#include <optional>
#include <vector>
#include <map>
#include <QString>
#include <QColor>
#include <QJsonObject>
#include <QJsonArray>
#include <QXmlStreamWriter>
#include <QXmlStreamReader>
#include <wobjectimpl.h>

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
module Artifact.Composition.InOutPoints;

import Serialization.Registry;
import Serialization.SchemaMigration;




import Frame.Position;
import Frame.Range;
import Event.Bus;
import Artifact.Event.Types;

namespace Artifact {

namespace {
const bool registeredInOutPoints = [] {
    ArtifactCore::Serialization::registerSerializableType<ArtifactInOutPoints>();
    ArtifactCore::Serialization::SchemaMigrationRegistry::instance().registerMigration(
        QStringLiteral("ArtifactInOutPoints"), 0, 1,
        [](const QJsonObject& legacy) { return legacy; });
    return true;
}();
}

W_OBJECT_IMPL(ArtifactMarker)
W_OBJECT_IMPL(ArtifactInOutPoints)

namespace {
// Model changes cross widget boundaries through the internal event bus.
// Keep Qt signal emission out of this domain-facing path.
void publishMarkerAttributeChanged(const ArtifactMarker* marker) {
    if (!marker) {
        return;
    }
    auto* points = qobject_cast<const ArtifactInOutPoints*>(marker->parent());
    if (!points) {
        return;
    }
    ArtifactCore::globalEventBus().publish<PlaybackInOutPointsChangedEvent>(
        PlaybackInOutPointsChangedEvent{points->hasInPoint(), points->hasOutPoint()});
}
}

// ==================== ArtifactMarker Implementation ====================

ArtifactMarker::ArtifactMarker(const FramePosition& position,
                               const QString& comment,
                               MarkerType type,
                               QObject* parent)
    : QObject(parent)
    , position_(position)
    , comment_(comment)
    , type_(type)
{
    // Default colors by type
    switch (type) {
        case MarkerType::Comment: color_ = QColor("#FFFF00"); break; // Yellow
        case MarkerType::Chapter: color_ = QColor("#00FF00"); break; // Green
        case MarkerType::Flash:   color_ = QColor("#FF0000"); break; // Red
        case MarkerType::WebLink:  color_ = QColor("#00FFFF"); break; // Cyan
        case MarkerType::Color:    color_ = QColor("#FF00FF"); break; // Magenta
    }
}

FramePosition ArtifactMarker::position() const {
    return position_;
}

void ArtifactMarker::setPosition(const FramePosition& position) {
    if (position_ != position) {
        position_ = position;
        publishMarkerAttributeChanged(this);
    }
}

QString ArtifactMarker::comment() const {
    return comment_;
}

void ArtifactMarker::setComment(const QString& comment) {
    if (comment_ != comment) {
        comment_ = comment;
        publishMarkerAttributeChanged(this);
    }
}

MarkerType ArtifactMarker::type() const {
    return type_;
}

void ArtifactMarker::setType(MarkerType type) {
    if (type_ != type) {
        type_ = type;
        publishMarkerAttributeChanged(this);
    }
}

QColor ArtifactMarker::color() const {
    return color_;
}

void ArtifactMarker::setColor(const QColor& color) {
    if (color_ != color) {
        color_ = color;
        publishMarkerAttributeChanged(this);
    }
}

QString ArtifactMarker::webLink() const {
    return webLink_;
}

void ArtifactMarker::setWebLink(const QString& link) {
    if (webLink_ != link) {
        webLink_ = link;
        publishMarkerAttributeChanged(this);
    }
}

QStringList ArtifactMarker::tags() const {
    return tags_;
}

void ArtifactMarker::setTags(const QStringList& tags) {
    QStringList normalized = tags;
    normalized.removeDuplicates();
    for (QString& t : normalized) {
        t = t.trimmed();
    }
    normalized.erase(std::remove_if(normalized.begin(), normalized.end(), [](const QString& t) {
        return t.isEmpty();
    }), normalized.end());

    if (tags_ != normalized) {
        tags_ = normalized;
        publishMarkerAttributeChanged(this);
    }
}

void ArtifactMarker::addTag(const QString& tag) {
    const QString trimmed = tag.trimmed();
    if (trimmed.isEmpty() || tags_.contains(trimmed, Qt::CaseInsensitive)) {
        return;
    }
    tags_.append(trimmed);
    publishMarkerAttributeChanged(this);
}

void ArtifactMarker::removeTag(const QString& tag) {
    const QString trimmed = tag.trimmed();
    if (trimmed.isEmpty()) return;
    const int before = tags_.size();
    tags_.erase(std::remove_if(tags_.begin(), tags_.end(), [&](const QString& v) {
        return v.compare(trimmed, Qt::CaseInsensitive) == 0;
    }), tags_.end());
    if (before != tags_.size()) {
        publishMarkerAttributeChanged(this);
    }
}

bool ArtifactMarker::hasTag(const QString& tag) const {
    const QString trimmed = tag.trimmed();
    if (trimmed.isEmpty()) return false;
    for (const QString& t : tags_) {
        if (t.compare(trimmed, Qt::CaseInsensitive) == 0) {
            return true;
        }
    }
    return false;
}

bool ArtifactMarker::operator<(const ArtifactMarker& other) const {
    return position_ < other.position_;
}

bool ArtifactMarker::operator==(const ArtifactMarker& other) const {
    return position_ == other.position_;
}

// ==================== ArtifactInOutPoints::Impl ====================

class ArtifactInOutPoints::Impl {
public:
    ArtifactCore::Optional<FramePosition> inPoint_;
    ArtifactCore::Optional<FramePosition> outPoint_;
    
    // Markers sorted by position for fast lookup
    std::map<FramePosition, ArtifactMarker*> markers_;
    
    // Chapter markers cache
    std::vector<ArtifactMarker*> chapters_;
    bool chaptersDirty_ = true;
    
    void updateChapters() {
        if (!chaptersDirty_) return;
        
        chapters_.clear();
        for (auto& [pos, marker] : markers_) {
            if (marker->type() == MarkerType::Chapter) {
                chapters_.push_back(marker);
            }
        }
        chaptersDirty_ = false;
    }
};

// ==================== ArtifactInOutPoints Implementation ====================

ArtifactInOutPoints::ArtifactInOutPoints(QObject* parent)
    : QObject(parent)
    , impl_(new Impl())
{
}

ArtifactInOutPoints::~ArtifactInOutPoints() = default;

// In Point
void ArtifactInOutPoints::setInPoint(const FramePosition& frame) {
    impl_->inPoint_ = frame;
    ArtifactCore::globalEventBus().publish<PlaybackInOutPointsChangedEvent>(
        PlaybackInOutPointsChangedEvent{impl_->inPoint_.has_value(), impl_->outPoint_.has_value()});
}

void ArtifactInOutPoints::clearInPoint() {
    impl_->inPoint_ = {};
    ArtifactCore::globalEventBus().publish<PlaybackInOutPointsChangedEvent>(
        PlaybackInOutPointsChangedEvent{impl_->inPoint_.has_value(), impl_->outPoint_.has_value()});
}

ArtifactCore::Optional<FramePosition> ArtifactInOutPoints::inPoint() const {
    return impl_->inPoint_;
}

bool ArtifactInOutPoints::hasInPoint() const {
    return impl_->inPoint_.has_value();
}

void ArtifactInOutPoints::setInPointAtCurrent(const FramePosition& currentFrame) {
    setInPoint(currentFrame);
}

// Out Point
void ArtifactInOutPoints::setOutPoint(const FramePosition& frame) {
    impl_->outPoint_ = frame;
    ArtifactCore::globalEventBus().publish<PlaybackInOutPointsChangedEvent>(
        PlaybackInOutPointsChangedEvent{impl_->inPoint_.has_value(), impl_->outPoint_.has_value()});
}

void ArtifactInOutPoints::clearOutPoint() {
    impl_->outPoint_ = {};
    ArtifactCore::globalEventBus().publish<PlaybackInOutPointsChangedEvent>(
        PlaybackInOutPointsChangedEvent{impl_->inPoint_.has_value(), impl_->outPoint_.has_value()});
}

ArtifactCore::Optional<FramePosition> ArtifactInOutPoints::outPoint() const {
    return impl_->outPoint_;
}

bool ArtifactInOutPoints::hasOutPoint() const {
    return impl_->outPoint_.has_value();
}

void ArtifactInOutPoints::setOutPointAtCurrent(const FramePosition& currentFrame) {
    setOutPoint(currentFrame);
}

// Playback Range
FrameRange ArtifactInOutPoints::effectiveRange(FramePosition compStart, FramePosition compEnd) const {
    FramePosition start = impl_->inPoint_.value_or(compStart);
    FramePosition end = impl_->outPoint_.value_or(compEnd);
    
    // Ensure start <= end
    if (start > end) {
        return FrameRange(end, start);
    }
    
    return FrameRange(start, end);
}

bool ArtifactInOutPoints::isRangeLimited() const {
    return impl_->inPoint_.has_value() || impl_->outPoint_.has_value();
}

void ArtifactInOutPoints::clearAllPoints() {
    impl_->inPoint_ = {};
    impl_->outPoint_ = {};
    ArtifactCore::globalEventBus().publish<PlaybackInOutPointsChangedEvent>(
        PlaybackInOutPointsChangedEvent{false, false});
}

// Markers
ArtifactMarker* ArtifactInOutPoints::addMarker(const FramePosition& position,
                                                const QString& comment,
                                                MarkerType type) {
    // Check if marker already exists at position
    auto it = impl_->markers_.find(position);
    if (it != impl_->markers_.end()) {
        // Update existing marker
        it->second->setComment(comment);
        it->second->setType(type);
        impl_->chaptersDirty_ = true;
        ArtifactCore::globalEventBus().publish<PlaybackInOutPointsChangedEvent>(
            PlaybackInOutPointsChangedEvent{impl_->inPoint_.has_value(),
                                            impl_->outPoint_.has_value()});
        return it->second;
    }
    
    // Create new marker
    auto* marker = new ArtifactMarker(position, comment, type, this);
    impl_->markers_[position] = marker;
    impl_->chaptersDirty_ = true;
    
    ArtifactCore::globalEventBus().publish<PlaybackInOutPointsChangedEvent>(
        PlaybackInOutPointsChangedEvent{impl_->inPoint_.has_value(),
                                        impl_->outPoint_.has_value()});
    return marker;
}

bool ArtifactInOutPoints::removeMarker(const FramePosition& position) {
    auto it = impl_->markers_.find(position);
    if (it != impl_->markers_.end()) {
        auto* marker = it->second;
        impl_->markers_.erase(it);
        impl_->chaptersDirty_ = true;
        
        delete marker;
        ArtifactCore::globalEventBus().publish<PlaybackInOutPointsChangedEvent>(
            PlaybackInOutPointsChangedEvent{impl_->inPoint_.has_value(),
                                            impl_->outPoint_.has_value()});
        return true;
    }
    return false;
}

void ArtifactInOutPoints::removeMarker(ArtifactMarker* marker) {
    if (!marker) return;
    
    auto it = impl_->markers_.find(marker->position());
    if (it != impl_->markers_.end() && it->second == marker) {
        impl_->markers_.erase(it);
        impl_->chaptersDirty_ = true;
        
        delete marker;
        ArtifactCore::globalEventBus().publish<PlaybackInOutPointsChangedEvent>(
            PlaybackInOutPointsChangedEvent{impl_->inPoint_.has_value(),
                                            impl_->outPoint_.has_value()});
    }
}

ArtifactMarker* ArtifactInOutPoints::getMarkerAt(const FramePosition& position) const {
    auto it = impl_->markers_.find(position);
    if (it != impl_->markers_.end()) {
        return it->second;
    }
    return nullptr;
}

std::vector<ArtifactMarker*> ArtifactInOutPoints::allMarkers() const {
    std::vector<ArtifactMarker*> result;
    result.reserve(impl_->markers_.size());
    
    for (auto& [pos, marker] : impl_->markers_) {
        result.push_back(marker);
    }
    
    // Sort by position
    std::sort(result.begin(), result.end(),
              [](ArtifactMarker* a, ArtifactMarker* b) {
                  return a->position() < b->position();
              });
    
    return result;
}

std::vector<ArtifactMarker*> ArtifactInOutPoints::markersByType(MarkerType type) const {
    std::vector<ArtifactMarker*> result;
    
    for (auto& [pos, marker] : impl_->markers_) {
        if (marker->type() == type) {
            result.push_back(marker);
        }
    }
    
    std::sort(result.begin(), result.end(),
              [](ArtifactMarker* a, ArtifactMarker* b) {
                  return a->position() < b->position();
              });
    
    return result;
}

std::vector<ArtifactMarker*> ArtifactInOutPoints::markersByTag(const QString& tag) const {
    std::vector<ArtifactMarker*> result;
    for (auto& [pos, marker] : impl_->markers_) {
        if (marker && marker->hasTag(tag)) {
            result.push_back(marker);
        }
    }
    std::sort(result.begin(), result.end(),
              [](ArtifactMarker* a, ArtifactMarker* b) {
                  return a->position() < b->position();
              });
    return result;
}

std::vector<ArtifactMarker*> ArtifactInOutPoints::searchMarkers(const QString& text) const {
    std::vector<ArtifactMarker*> result;
    const QString needle = text.trimmed();
    if (needle.isEmpty()) {
        return allMarkers();
    }

    for (auto& [pos, marker] : impl_->markers_) {
        if (!marker) continue;
        const bool inComment = marker->comment().contains(needle, Qt::CaseInsensitive);
        const bool inLink = marker->webLink().contains(needle, Qt::CaseInsensitive);
        bool inTags = false;
        for (const QString& tag : marker->tags()) {
            if (tag.contains(needle, Qt::CaseInsensitive)) {
                inTags = true;
                break;
            }
        }
        if (inComment || inLink || inTags) {
            result.push_back(marker);
        }
    }

    std::sort(result.begin(), result.end(),
              [](ArtifactMarker* a, ArtifactMarker* b) {
                  return a->position() < b->position();
              });
    return result;
}

std::vector<ArtifactMarker*> ArtifactInOutPoints::markersInRange(const FrameRange& range) const {
    std::vector<ArtifactMarker*> result;
    
    for (auto& [pos, marker] : impl_->markers_) {
        if (range.contains(pos)) {
            result.push_back(marker);
        }
    }
    
    std::sort(result.begin(), result.end(),
              [](ArtifactMarker* a, ArtifactMarker* b) {
                  return a->position() < b->position();
              });
    
    return result;
}

std::vector<ArtifactMarker*> ArtifactInOutPoints::chapterMarkers() const {
    impl_->updateChapters();
    return impl_->chapters_;
}

void ArtifactInOutPoints::clearAllMarkers() {
    for (auto& [pos, marker] : impl_->markers_) {
        delete marker;
    }
    impl_->markers_.clear();
    impl_->chapters_.clear();
    impl_->chaptersDirty_ = false;
    
    ArtifactCore::globalEventBus().publish<PlaybackInOutPointsChangedEvent>(
        PlaybackInOutPointsChangedEvent{impl_->inPoint_.has_value(),
                                        impl_->outPoint_.has_value()});
}

size_t ArtifactInOutPoints::markerCount() const {
    return impl_->markers_.size();
}

// Navigation
ArtifactCore::Optional<FramePosition> ArtifactInOutPoints::nextMarker(const FramePosition& current) const {
    auto it = impl_->markers_.upper_bound(current);
    if (it != impl_->markers_.end()) {
        return it->first;
    }
    return {};
}

ArtifactCore::Optional<FramePosition> ArtifactInOutPoints::previousMarker(const FramePosition& current) const {
    auto it = impl_->markers_.lower_bound(current);
    if (it != impl_->markers_.begin()) {
        --it;
        return it->first;
    }
    return {};
}

ArtifactCore::Optional<FramePosition> ArtifactInOutPoints::nextChapter(const FramePosition& current) const {
    impl_->updateChapters();
    
    for (auto* chapter : impl_->chapters_) {
        if (chapter->position() > current) {
            return chapter->position();
        }
    }
    return {};
}

ArtifactCore::Optional<FramePosition> ArtifactInOutPoints::previousChapter(const FramePosition& current) const {
    impl_->updateChapters();
    
    ArtifactCore::Optional<FramePosition> result;
    for (auto* chapter : impl_->chapters_) {
        if (chapter->position() < current) {
            result = chapter->position();
        } else {
            break;
        }
    }
    return result;
}

// Import/Export
bool ArtifactInOutPoints::importFromXML(const QString& xml) {
    QXmlStreamReader reader(xml);
    
    // Clear existing markers
    clearAllMarkers();
    clearAllPoints();

    while (reader.readNextStartElement()) {
        if (reader.name() == "InOutPoints") {
            while (reader.readNextStartElement()) {
                QStringView name = reader.name();

                if (name == "InPoint") {
                    QString frameStr = reader.attributes().value("frame").toString();
                    bool ok = false;
                    long long frame = frameStr.toLongLong(&ok);
                    if (ok) {
                        setInPoint(FramePosition(frame));
                    }
                    reader.skipCurrentElement();
                }
                else if (name == "OutPoint") {
                    QString frameStr = reader.attributes().value("frame").toString();
                    bool ok = false;
                    long long frame = frameStr.toLongLong(&ok);
                    if (ok) {
                        setOutPoint(FramePosition(frame));
                    }
                    reader.skipCurrentElement();
                }
                else if (name == "Marker") {
                    QString frameStr = reader.attributes().value("frame").toString();
                    QString comment = reader.attributes().value("comment").toString();
                    QString typeStr = reader.attributes().value("type").toString();
                    
                    bool ok = false;
                    long long frame = frameStr.toLongLong(&ok);
                    
                    if (ok) {
                        MarkerType type = MarkerType::Comment;
                        if (typeStr == "Chapter") type = MarkerType::Chapter;
                        else if (typeStr == "Flash") type = MarkerType::Flash;
                        else if (typeStr == "WebLink") type = MarkerType::WebLink;
                        else if (typeStr == "Color") type = MarkerType::Color;

                        ArtifactMarker* marker = addMarker(FramePosition(frame), comment, type);
                        if (marker) {
                            const QString tagsJoined = reader.attributes().value("tags").toString();
                            if (!tagsJoined.isEmpty()) {
                                marker->setTags(tagsJoined.split(';', Qt::SkipEmptyParts));
                            }
                        }
                    }
                    
                    reader.skipCurrentElement();
                }
                else {
                    reader.skipCurrentElement();
                }
            }
        }
    }
    
    return !reader.hasError();
}

QJsonObject ArtifactInOutPoints::toJson() const {
    QJsonObject object;
    if (impl_->inPoint_.has_value()) {
        object.insert(QStringLiteral("inPoint"), impl_->inPoint_->framePosition());
    }
    if (impl_->outPoint_.has_value()) {
        object.insert(QStringLiteral("outPoint"), impl_->outPoint_->framePosition());
    }
    QJsonArray markers;
    for (const auto& entry : impl_->markers_) {
        const auto* marker = entry.second;
        if (!marker) continue;
        QJsonObject item;
        item.insert(QStringLiteral("position"), marker->position().framePosition());
        item.insert(QStringLiteral("comment"), marker->comment());
        item.insert(QStringLiteral("type"), static_cast<int>(marker->type()));
        item.insert(QStringLiteral("color"), marker->color().name(QColor::HexArgb));
        item.insert(QStringLiteral("webLink"), marker->webLink());
        item.insert(QStringLiteral("tags"), QJsonArray::fromStringList(marker->tags()));
        markers.append(item);
    }
    object.insert(QStringLiteral("markers"), markers);
    return object;
}

bool ArtifactInOutPoints::fromJson(const QJsonObject& object) {
    const auto markersValue = object.value(QStringLiteral("markers"));
    if (!markersValue.isUndefined() && !markersValue.isArray()) {
        return false;
    }
    const auto inValue = object.value(QStringLiteral("inPoint"));
    const auto outValue = object.value(QStringLiteral("outPoint"));
    if ((!inValue.isUndefined() && !inValue.isDouble()) ||
        (!outValue.isUndefined() && !outValue.isDouble())) {
        return false;
    }
    clearAllPoints();
    clearAllMarkers();
    if (inValue.isDouble()) {
        setInPoint(FramePosition(inValue.toInt()));
    }
    if (outValue.isDouble()) {
        setOutPoint(FramePosition(outValue.toInt()));
    }
    const QJsonArray markers = markersValue.toArray();
    for (const auto& value : markers) {
        if (!value.isObject()) continue;
        const auto item = value.toObject();
        const auto positionValue = item.value(QStringLiteral("position"));
        if (!positionValue.isDouble()) {
            continue;
        }
        const int rawType = item.value(QStringLiteral("type")).toInt(0);
        const auto type = rawType >= static_cast<int>(MarkerType::Comment) &&
                                  rawType <= static_cast<int>(MarkerType::Color)
            ? static_cast<MarkerType>(rawType) : MarkerType::Comment;
        auto* marker = addMarker(
            FramePosition(positionValue.toInt()),
            item.value(QStringLiteral("comment")).toString(), type);
        if (!marker) continue;
        const QColor color(item.value(QStringLiteral("color")).toString());
        if (color.isValid()) marker->setColor(color);
        marker->setWebLink(item.value(QStringLiteral("webLink")).toString());
        QStringList tags;
        for (const auto& tagValue : item.value(QStringLiteral("tags")).toArray()) {
            const QString tag = tagValue.toString().trimmed();
            if (!tag.isEmpty()) tags.append(tag);
        }
        marker->setTags(tags);
    }
    return true;
}

QString ArtifactInOutPoints::exportToXML() const {
    QString xml;
    QXmlStreamWriter writer(&xml);
    
    writer.setAutoFormatting(true);
    writer.writeStartDocument();
    writer.writeStartElement("InOutPoints");
    
    // Write in point
    if (hasInPoint()) {
        writer.writeStartElement("InPoint");
        writer.writeAttribute("frame", QString::number(inPoint()->framePosition()));
        writer.writeEndElement();
    }

    // Write out point
    if (hasOutPoint()) {
        writer.writeStartElement("OutPoint");
        writer.writeAttribute("frame", QString::number(outPoint()->framePosition()));
        writer.writeEndElement();
    }

    // Write markers
    for (auto* marker : allMarkers()) {
        writer.writeStartElement("Marker");
        writer.writeAttribute("frame", QString::number(marker->position().framePosition()));
        writer.writeAttribute("comment", marker->comment());
        if (!marker->tags().isEmpty()) {
            writer.writeAttribute("tags", marker->tags().join(';'));
        }
        
        QString typeStr = "Comment";
        switch (marker->type()) {
            case MarkerType::Chapter: typeStr = "Chapter"; break;
            case MarkerType::Flash: typeStr = "Flash"; break;
            case MarkerType::WebLink: typeStr = "WebLink"; break;
            case MarkerType::Color: typeStr = "Color"; break;
            default: break;
        }
        writer.writeAttribute("type", typeStr);
        
        writer.writeEndElement();
    }
    
    writer.writeEndElement(); // InOutPoints
    writer.writeEndDocument();
    
    return xml;
}

} // namespace Artifact
