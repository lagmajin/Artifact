module;
#include <QObject>
#include <memory>
#include <wobjectdefs.h>

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
export module Artifact.Service.PlaybackShortcuts;




import Artifact.Composition.InOutPoints;
import Artifact.Composition.PlaybackController;
import Input.Operator;

export namespace Artifact {

using namespace ArtifactCore;

/**
 * @brief Playback-specific shortcut manager
 * 
 * Manages keyboard shortcuts for:
 * - Play/Pause/Stop
 * - In/Out points
 * - Markers
 * - Frame navigation
 * - Playback speed
 */
class ArtifactPlaybackShortcuts : public QObject {
    W_OBJECT(ArtifactPlaybackShortcuts)
private:
    class Impl;
    std::unique_ptr<Impl> impl_;
    
public:
    explicit ArtifactPlaybackShortcuts(QObject* parent = nullptr);
    ~ArtifactPlaybackShortcuts();
    
    // Setup - connect to playback controller and in/out points
    void setup(ArtifactCompositionPlaybackController* controller,
               ArtifactInOutPoints* inOutPoints);
    
    // Manual binding (for custom keymaps)
    void registerDefaultBindings(KeyMap* keyMap);
    
    // Enable/disable
    void setEnabled(bool enabled);
    bool isEnabled() const;
    
    // Current context
    void setActiveContext(const QString& context);
    QString activeContext() const;
    
public: // Actions - can be triggered manually
    
    // Playback
    void playPause();
    void play();
    void pause();
    void stop();
    void goToStart();
    void goToEnd();
    
    // Frame navigation
    void nextFrame();
    void previousFrame();
    void goToFrame(int frame);
    
    // In/Out points
    void setInPoint();
    void setOutPoint();
    void clearInPoint();
    void clearOutPoint();
    void clearInOutPoints();
    void goToInPoint();
    void goToOutPoint();
    
    // Markers
    void addMarker(const QString& comment = QString());
    void addChapterMarker(const QString& name = QString());
    void goToNextMarker();
    void goToPreviousMarker();
    void goToNextChapter();
    void goToPreviousChapter();
    void deleteMarkerAtCurrent();
    void clearAllMarkers();
    
    // Playback speed
    void setSpeedNormal();
    void setSpeedHalf();
    void setSpeedDouble();
    void toggleLoop();
    
    // Scrubbing
    void startScrub();
    void scrubTo(int frame);
    void endScrub();
    
signals:
    // Signals for UI feedback
    void inPointSet(int frame) W_SIGNAL(inPointSet, frame);
    void outPointSet(int frame) W_SIGNAL(outPointSet, frame);
    void markerAdded(int frame, const QString& comment) W_SIGNAL(markerAdded, frame, comment);
    void shortcutExecuted(const QString& actionId) W_SIGNAL(shortcutExecuted, actionId);
};

} // namespace Artifact
