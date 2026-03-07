module;

#include <algorithm>
#include <optional>
#include <vector>
#include <map>
#include <QString>
#include <QColor>
#include <QXmlStreamWriter>
#include <QXmlStreamReader>

module Artifact.Composition.InOutPoints;

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



import Frame.Position;
import Frame.Range;
import Artifact.Composition.InOutPoints;

namespace Artifact {

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
        emit positionChanged(position);
        emit markerChanged();
    }
}

QString ArtifactMarker::comment() const {
    return comment_;
}

void ArtifactMarker::setComment(const QString& comment) {
    if (comment_ != comment) {
        comment_ = comment;
        emit commentChanged(comment);
        emit markerChanged();
    }
}

MarkerType ArtifactMarker::type() const {
    return type_;
}

void ArtifactMarker::setType(MarkerType type) {
    if (type_ != type) {
        type_ = type;
        emit typeChanged(type);
        emit markerChanged();
    }
}

QColor ArtifactMarker::color() const {
    return color_;
}

void ArtifactMarker::setColor(const QColor& color) {
    if (color_ != color) {
        color_ = color;
        emit colorChanged(color);
        emit markerChanged();
    }
}

QString ArtifactMarker::webLink() const {
    return webLink_;
}

void ArtifactMarker::setWebLink(const QString& link) {
    if (webLink_ != link) {
        webLink_ = link;
        emit webLinkChanged(link);
        emit markerChanged();
    }
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
    std::optional<FramePosition> inPoint_;
    std::optional<FramePosition> outPoint_;
    
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
    emit inPointChanged(impl_->inPoint_);
}

void ArtifactInOutPoints::clearInPoint() {
    impl_->inPoint_ = std::nullopt;
    emit inPointChanged(impl_->inPoint_);
}

std::optional<FramePosition> ArtifactInOutPoints::inPoint() const {
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
    emit outPointChanged(impl_->outPoint_);
}

void ArtifactInOutPoints::clearOutPoint() {
    impl_->outPoint_ = std::nullopt;
    emit outPointChanged(impl_->outPoint_);
}

std::optional<FramePosition> ArtifactInOutPoints::outPoint() const {
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
    impl_->inPoint_ = std::nullopt;
    impl_->outPoint_ = std::nullopt;
    emit pointsCleared();
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
        emit markerChanged(it->second);
        return it->second;
    }
    
    // Create new marker
    auto* marker = new ArtifactMarker(position, comment, type, this);
    impl_->markers_[position] = marker;
    impl_->chaptersDirty_ = true;
    
    emit markerAdded(marker);
    return marker;
}

bool ArtifactInOutPoints::removeMarker(const FramePosition& position) {
    auto it = impl_->markers_.find(position);
    if (it != impl_->markers_.end()) {
        auto* marker = it->second;
        impl_->markers_.erase(it);
        impl_->chaptersDirty_ = true;
        
        emit markerRemoved(marker);
        delete marker;
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
        
        emit markerRemoved(marker);
        delete marker;
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
        emit markerRemoved(marker);
        delete marker;
    }
    impl_->markers_.clear();
    impl_->chapters_.clear();
    impl_->chaptersDirty_ = false;
    
    emit allMarkersCleared();
}

size_t ArtifactInOutPoints::markerCount() const {
    return impl_->markers_.size();
}

// Navigation
std::optional<FramePosition> ArtifactInOutPoints::nextMarker(const FramePosition& current) const {
    auto it = impl_->markers_.upper_bound(current);
    if (it != impl_->markers_.end()) {
        return it->first;
    }
    return std::nullopt;
}

std::optional<FramePosition> ArtifactInOutPoints::previousMarker(const FramePosition& current) const {
    auto it = impl_->markers_.lower_bound(current);
    if (it != impl_->markers_.begin()) {
        --it;
        return it->first;
    }
    return std::nullopt;
}

std::optional<FramePosition> ArtifactInOutPoints::nextChapter(const FramePosition& current) const {
    impl_->updateChapters();
    
    for (auto* chapter : impl_->chapters_) {
        if (chapter->position() > current) {
            return chapter->position();
        }
    }
    return std::nullopt;
}

std::optional<FramePosition> ArtifactInOutPoints::previousChapter(const FramePosition& current) const {
    impl_->updateChapters();
    
    std::optional<FramePosition> result;
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
                QStringRef name = reader.name();
                
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
                        
                        addMarker(FramePosition(frame), comment, type);
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

QString ArtifactInOutPoints::exportToXML() const {
    QString xml;
    QXmlStreamWriter writer(&xml);
    
    writer.setAutoFormatting(true);
    writer.writeStartDocument();
    writer.writeStartElement("InOutPoints");
    
    // Write in point
    if (hasInPoint()) {
        writer.writeStartElement("InPoint");
        writer.writeAttribute("frame", QString::number(inPoint()->value()));
        writer.writeEndElement();
    }
    
    // Write out point
    if (hasOutPoint()) {
        writer.writeStartElement("OutPoint");
        writer.writeAttribute("frame", QString::number(outPoint()->value()));
        writer.writeEndElement();
    }
    
    // Write markers
    for (auto* marker : allMarkers()) {
        writer.writeStartElement("Marker");
        writer.writeAttribute("frame", QString::number(marker->position().value()));
        writer.writeAttribute("comment", marker->comment());
        
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
