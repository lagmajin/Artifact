module;
#include <optional>
#include <vector>

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
#include <wobjectdefs.h>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QColor>
export module Artifact.Composition.InOutPoints;


import std;
import Frame.Position;
import Frame.Range;

W_REGISTER_ARGTYPE(ArtifactCore::FramePosition)

export namespace Artifact {

using namespace ArtifactCore;

/**
 * @brief Marker types for timeline markers
 */
enum class MarkerType {
    Comment,      // General comment marker
    Chapter,      // Chapter marker for navigation
    Flash,        // Flash frame marker
    WebLink,      // Web link marker
    Color         // Color correction point
};

// Register MarkerType for use as signal/slot parameter

/**
 * @brief Timeline marker with position, comment, and type
 */
class ArtifactMarker : public QObject {
    W_OBJECT(ArtifactMarker)
private:
    FramePosition position_;
    QString comment_;
    MarkerType type_;
    QColor color_;
    QString webLink_;
    QStringList tags_;
    
public:
    explicit ArtifactMarker(const FramePosition& position, 
                           const QString& comment = QString(),
                           MarkerType type = MarkerType::Comment,
                           QObject* parent = nullptr);
    
    // Position
    FramePosition position() const;
    void setPosition(const FramePosition& position);
    
    // Comment
    QString comment() const;
    void setComment(const QString& comment);
    
    // Type
    MarkerType type() const;
    void setType(MarkerType type);
    
    // Color
    QColor color() const;
    void setColor(const QColor& color);
    
    // Web link
    QString webLink() const;
    void setWebLink(const QString& link);

    // Tags
    QStringList tags() const;
    void setTags(const QStringList& tags);
    void addTag(const QString& tag);
    void removeTag(const QString& tag);
    bool hasTag(const QString& tag) const;
    
    // Operators
    bool operator<(const ArtifactMarker& other) const;
    bool operator==(const ArtifactMarker& other) const;
    
signals:
    void positionChanged(FramePosition position) W_SIGNAL(positionChanged, position);
    void commentChanged(QString comment) W_SIGNAL(commentChanged, comment);
    void typeChanged(MarkerType type) W_SIGNAL(typeChanged, type);
    void colorChanged(QColor color) W_SIGNAL(colorChanged, color);
    void webLinkChanged(QString link) W_SIGNAL(webLinkChanged, link);
    void tagsChanged(QStringList tags) W_SIGNAL(tagsChanged, tags);
    void markerChanged() W_SIGNAL(markerChanged);
};

/**
 * @brief In/Out points and Markers manager for composition playback
 * 
 * Manages:
 * - In point (start of playback range)
 * - Out point (end of playback range)  
 * - Timeline markers with comments, chapters, etc.
 */
class ArtifactInOutPoints : public QObject {
    W_OBJECT(ArtifactInOutPoints)
private:
    class Impl;
    Impl* impl_;
    
public:
    explicit ArtifactInOutPoints(QObject* parent = nullptr);
    ~ArtifactInOutPoints();
    
    // ==================== In Point ====================
    
    /**
     * @brief Set in point at specified frame
     * @param frame Frame position for in point
     */
    void setInPoint(const FramePosition& frame);
    
    /**
     * @brief Clear in point (use start of composition)
     */
    void clearInPoint();
    
    /**
     * @brief Get in point position
     * @return In point frame, or std::nullopt if not set
     */
    std::optional<FramePosition> inPoint() const;
    
    /**
     * @brief Check if in point is set
     */
    bool hasInPoint() const;
    
    /**
     * @brief Set in point at current frame
     */
    void setInPointAtCurrent(const FramePosition& currentFrame);
    
    // ==================== Out Point ====================
    
    /**
     * @brief Set out point at specified frame
     * @param frame Frame position for out point
     */
    void setOutPoint(const FramePosition& frame);
    
    /**
     * @brief Clear out point (use end of composition)
     */
    void clearOutPoint();
    
    /**
     * @brief Get out point position
     * @return Out point frame, or std::nullopt if not set
     */
    std::optional<FramePosition> outPoint() const;
    
    /**
     * @brief Check if out point is set
     */
    bool hasOutPoint() const;
    
    /**
     * @brief Set out point at current frame
     */
    void setOutPointAtCurrent(const FramePosition& currentFrame);
    
    // ==================== Playback Range ====================
    
    /**
     * @brief Get effective playback range considering in/out points
     * @param compStart Start frame of composition
     * @param compEnd End frame of composition
     * @return Effective playback range
     */
    FrameRange effectiveRange(FramePosition compStart, FramePosition compEnd) const;
    
    /**
     * @brief Check if playback is limited to in/out range
     */
    bool isRangeLimited() const;
    
    /**
     * @brief Clear both in and out points
     */
    void clearAllPoints();
    
    // ==================== Markers ====================
    
    /**
     * @brief Add a marker at specified position
     * @param position Frame position for marker
     * @param comment Marker comment
     * @param type Marker type
     * @return Pointer to created marker
     */
    ArtifactMarker* addMarker(const FramePosition& position,
                              const QString& comment = QString(),
                              MarkerType type = MarkerType::Comment);
    
    /**
     * @brief Remove marker at position
     * @param position Position of marker to remove
     * @return true if marker was removed
     */
    bool removeMarker(const FramePosition& position);
    
    /**
     * @brief Remove marker by pointer
     * @param marker Pointer to marker to remove
     */
    void removeMarker(ArtifactMarker* marker);
    
    /**
     * @brief Get marker at position
     * @param position Frame position
     * @return Marker at position or nullptr
     */
    ArtifactMarker* getMarkerAt(const FramePosition& position) const;
    
    /**
     * @brief Get all markers
     * @return Vector of all markers (sorted by position)
     */
    std::vector<ArtifactMarker*> allMarkers() const;
    
    /**
     * @brief Get markers of specific type
     * @param type Marker type to filter
     * @return Vector of markers of specified type
     */
    std::vector<ArtifactMarker*> markersByType(MarkerType type) const;
    std::vector<ArtifactMarker*> markersByTag(const QString& tag) const;
    std::vector<ArtifactMarker*> searchMarkers(const QString& text) const;
    
    /**
     * @brief Get markers within range
     * @param range Frame range
     * @return Vector of markers in range
     */
    std::vector<ArtifactMarker*> markersInRange(const FrameRange& range) const;
    
    /**
     * @brief Get chapter markers (for navigation)
     * @return Vector of chapter markers sorted by position
     */
    std::vector<ArtifactMarker*> chapterMarkers() const;
    
    /**
     * @brief Clear all markers
     */
    void clearAllMarkers();
    
    /**
     * @brief Get marker count
     */
    size_t markerCount() const;
    
    // ==================== Navigation ====================
    
    /**
     * @brief Go to next marker
     * @param current Current frame position
     * @return Next marker position or std::nullopt
     */
    std::optional<FramePosition> nextMarker(const FramePosition& current) const;
    
    /**
     * @brief Go to previous marker
     * @param current Current frame position
     * @return Previous marker position or std::nullopt
     */
    std::optional<FramePosition> previousMarker(const FramePosition& current) const;
    
    /**
     * @brief Go to next chapter marker
     * @param current Current frame position
     * @return Next chapter position or std::nullopt
     */
    std::optional<FramePosition> nextChapter(const FramePosition& current) const;
    
    /**
     * @brief Go to previous chapter marker
     * @param current Current frame position
     * @return Previous chapter position or std::nullopt
     */
    std::optional<FramePosition> previousChapter(const FramePosition& current) const;
    
    // ==================== Import/Export ====================
    
    /**
     * @brief Import markers from XML
     * @param xml XML string containing marker data
     * @return true if import successful
     */
    bool importFromXML(const QString& xml);
    
    /**
     * @brief Export markers to XML
     * @return XML string with marker data
     */
    QString exportToXML() const;
    
public: // signals
    
    // In/Out point signals
    void inPointChanged(std::optional<FramePosition> inPoint) 
        W_SIGNAL(inPointChanged, inPoint);
    void outPointChanged(std::optional<FramePosition> outPoint) 
        W_SIGNAL(outPointChanged, outPoint);
    void pointsCleared() W_SIGNAL(pointsCleared);
    
    // Marker signals
    void markerAdded(ArtifactMarker* marker) W_SIGNAL(markerAdded, marker);
    void markerRemoved(ArtifactMarker* marker) W_SIGNAL(markerRemoved, marker);
    void markerChanged(ArtifactMarker* marker) W_SIGNAL(markerChanged, marker);
    void allMarkersCleared() W_SIGNAL(allMarkersCleared);
    
    // Navigation signals
    void navigatedToMarker(FramePosition position) W_SIGNAL(navigatedToMarker, position);
};

} // namespace Artifact

W_REGISTER_ARGTYPE(Artifact::MarkerType)
W_REGISTER_ARGTYPE(std::optional<ArtifactCore::FramePosition>)
W_REGISTER_ARGTYPE(Artifact::ArtifactMarker*)
