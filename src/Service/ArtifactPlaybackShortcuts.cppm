module;

#include <QKeySequence>
#include <QDebug>
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
module Artifact.Service.PlaybackShortcuts;




import Frame.Position;
import Artifact.Composition.InOutPoints;
import Artifact.Composition.PlaybackController;
import Input.Operator;

namespace Artifact {

W_OBJECT_IMPL(ArtifactPlaybackShortcuts)

// ==================== Impl ====================

class ArtifactPlaybackShortcuts::Impl {
public:
    ArtifactCompositionPlaybackController* controller_ = nullptr;
    ArtifactInOutPoints* inOutPoints_ = nullptr;
    InputOperator* inputOperator_ = nullptr;
    bool enabled_ = true;
    QString activeContext_ = "Timeline";
    
    // Action IDs
    static inline const QString ACTION_PLAY_PAUSE = "playback.play_pause";
    static inline const QString ACTION_PLAY = "playback.play";
    static inline const QString ACTION_PAUSE = "playback.pause";
    static inline const QString ACTION_STOP = "playback.stop";
    static inline const QString ACTION_GOTO_START = "playback.goto_start";
    static inline const QString ACTION_GOTO_END = "playback.goto_end";
    static inline const QString ACTION_NEXT_FRAME = "playback.next_frame";
    static inline const QString ACTION_PREV_FRAME = "playback.prev_frame";
    
    static inline const QString ACTION_SET_IN_POINT = "playback.set_in_point";
    static inline const QString ACTION_SET_OUT_POINT = "playback.set_out_point";
    static inline const QString ACTION_CLEAR_IN_POINT = "playback.clear_in_point";
    static inline const QString ACTION_CLEAR_OUT_POINT = "playback.clear_out_point";
    static inline const QString ACTION_CLEAR_IN_OUT = "playback.clear_in_out";
    static inline const QString ACTION_GOTO_IN_POINT = "playback.goto_in_point";
    static inline const QString ACTION_GOTO_OUT_POINT = "playback.goto_out_point";
    
    static inline const QString ACTION_ADD_MARKER = "playback.add_marker";
    static inline const QString ACTION_ADD_CHAPTER = "playback.add_chapter";
    static inline const QString ACTION_NEXT_MARKER = "playback.next_marker";
    static inline const QString ACTION_PREV_MARKER = "playback.prev_marker";
    static inline const QString ACTION_NEXT_CHAPTER = "playback.next_chapter";
    static inline const QString ACTION_PREV_CHAPTER = "playback.prev_chapter";
    static inline const QString ACTION_DELETE_MARKER = "playback.delete_marker";
    
    static inline const QString ACTION_SPEED_NORMAL = "playback.speed_normal";
    static inline const QString ACTION_SPEED_HALF = "playback.speed_half";
    static inline const QString ACTION_SPEED_DOUBLE = "playback.speed_double";
    static inline const QString ACTION_TOGGLE_LOOP = "playback.toggle_loop";
};

// ==================== Implementation ====================

ArtifactPlaybackShortcuts::ArtifactPlaybackShortcuts(QObject* parent)
    : QObject(parent)
    , impl_(new Impl())
{
    // Get input operator instance
    impl_->inputOperator_ = InputOperator::instance();
    
    // Register all actions with ActionManager
    auto* am = ActionManager::instance();
    
    // Playback actions
    am->registerAction(Impl::ACTION_PLAY_PAUSE, "Play/Pause", "Toggle playback");
    am->registerAction(Impl::ACTION_PLAY, "Play", "Start playback");
    am->registerAction(Impl::ACTION_PAUSE, "Pause", "Pause playback");
    am->registerAction(Impl::ACTION_STOP, "Stop", "Stop playback and go to start");
    am->registerAction(Impl::ACTION_GOTO_START, "Go to Start", "Go to start frame");
    am->registerAction(Impl::ACTION_GOTO_END, "Go to End", "Go to end frame");
    am->registerAction(Impl::ACTION_NEXT_FRAME, "Next Frame", "Go to next frame");
    am->registerAction(Impl::ACTION_PREV_FRAME, "Previous Frame", "Go to previous frame");
    
    // In/Out point actions
    am->registerAction(Impl::ACTION_SET_IN_POINT, "Set In Point", "Set in point at current frame", "Markers");
    am->registerAction(Impl::ACTION_SET_OUT_POINT, "Set Out Point", "Set out point at current frame", "Markers");
    am->registerAction(Impl::ACTION_CLEAR_IN_POINT, "Clear In Point", "Clear in point", "Markers");
    am->registerAction(Impl::ACTION_CLEAR_OUT_POINT, "Clear Out Point", "Clear out point", "Markers");
    am->registerAction(Impl::ACTION_CLEAR_IN_OUT, "Clear In/Out Points", "Clear both points", "Markers");
    am->registerAction(Impl::ACTION_GOTO_IN_POINT, "Go to In Point", "Jump to in point", "Navigation");
    am->registerAction(Impl::ACTION_GOTO_OUT_POINT, "Go to Out Point", "Jump to out point", "Navigation");
    
    // Marker actions
    am->registerAction(Impl::ACTION_ADD_MARKER, "Add Marker", "Add marker at current frame", "Markers");
    am->registerAction(Impl::ACTION_ADD_CHAPTER, "Add Chapter", "Add chapter marker", "Markers");
    am->registerAction(Impl::ACTION_NEXT_MARKER, "Next Marker", "Go to next marker", "Navigation");
    am->registerAction(Impl::ACTION_PREV_MARKER, "Previous Marker", "Go to previous marker", "Navigation");
    am->registerAction(Impl::ACTION_NEXT_CHAPTER, "Next Chapter", "Go to next chapter", "Navigation");
    am->registerAction(Impl::ACTION_PREV_CHAPTER, "Previous Chapter", "Go to previous chapter", "Navigation");
    am->registerAction(Impl::ACTION_DELETE_MARKER, "Delete Marker", "Delete marker at current frame", "Markers");
    
    // Speed actions
    am->registerAction(Impl::ACTION_SPEED_NORMAL, "Normal Speed", "Set playback speed to 100%", "Speed");
    am->registerAction(Impl::ACTION_SPEED_HALF, "Half Speed", "Set playback speed to 50%", "Speed");
    am->registerAction(Impl::ACTION_SPEED_DOUBLE, "Double Speed", "Set playback speed to 200%", "Speed");
    am->registerAction(Impl::ACTION_TOGGLE_LOOP, "Toggle Loop", "Toggle loop playback", "Playback");
}

ArtifactPlaybackShortcuts::~ArtifactPlaybackShortcuts() = default;

void ArtifactPlaybackShortcuts::setup(ArtifactCompositionPlaybackController* controller,
                                      ArtifactInOutPoints* inOutPoints) {
    impl_->controller_ = controller;
    impl_->inOutPoints_ = inOutPoints;
    
    // Create keymap
    auto* keyMap = impl_->inputOperator_->addKeyMap("Playback", impl_->activeContext_);
    
    // Register default bindings
    registerDefaultBindings(keyMap);
}

void ArtifactPlaybackShortcuts::registerDefaultBindings(KeyMap* keyMap) {
    auto* am = ActionManager::instance();
    const auto kAlt = InputEvent::Modifiers(InputEvent::ModifierKey::LAlt)
                    | InputEvent::Modifiers(InputEvent::ModifierKey::RAlt);
    const auto kShift = InputEvent::Modifiers(InputEvent::ModifierKey::LShift)
                      | InputEvent::Modifiers(InputEvent::ModifierKey::RShift);
    const auto kCtrl = InputEvent::Modifiers(InputEvent::ModifierKey::LCtrl)
                     | InputEvent::Modifiers(InputEvent::ModifierKey::RCtrl);
    const auto kCtrlShift = kCtrl | kShift;
    
    // ==================== After Effects-like shortcuts ====================
    
    // Playback control
    keyMap->addBinding(Qt::Key_Space, InputEvent::Modifiers(), 
                      am->getAction(Impl::ACTION_PLAY_PAUSE), "Play/Pause");
    
    keyMap->addBinding(Qt::Key_Enter, InputEvent::Modifiers(),
                      am->getAction(Impl::ACTION_GOTO_END), "Go to End");
    
    // Frame by frame
    keyMap->addBinding(Qt::Key_Right, InputEvent::Modifiers(),
                      am->getAction(Impl::ACTION_NEXT_FRAME), "Next Frame");
    keyMap->addBinding(Qt::Key_Left, InputEvent::Modifiers(),
                      am->getAction(Impl::ACTION_PREV_FRAME), "Previous Frame");
    
    // J/K/L shuttle (After Effects style)
    keyMap->addBinding(Qt::Key_J, InputEvent::Modifiers(),
                      am->getAction(Impl::ACTION_PREV_FRAME), "Shuttle Left");
    keyMap->addBinding(Qt::Key_K, InputEvent::Modifiers(),
                      am->getAction(Impl::ACTION_PLAY_PAUSE), "Shuttle Stop");
    keyMap->addBinding(Qt::Key_L, InputEvent::Modifiers(),
                      am->getAction(Impl::ACTION_NEXT_FRAME), "Shuttle Right");
    
    // ==================== In/Out Points ====================
    
    keyMap->addBinding(Qt::Key_I, InputEvent::Modifiers(),
                      am->getAction(Impl::ACTION_SET_IN_POINT), "Set In Point");
    keyMap->addBinding(Qt::Key_O, InputEvent::Modifiers(),
                      am->getAction(Impl::ACTION_SET_OUT_POINT), "Set Out Point");
    
    keyMap->addBinding(Qt::Key_I, kAlt,
                      am->getAction(Impl::ACTION_CLEAR_IN_POINT), "Clear In Point");
    keyMap->addBinding(Qt::Key_O, kAlt,
                      am->getAction(Impl::ACTION_CLEAR_OUT_POINT), "Clear Out Point");
    
    // Go to in/out
    keyMap->addBinding(Qt::Key_Home, InputEvent::Modifiers(),
                      am->getAction(Impl::ACTION_GOTO_IN_POINT), "Go to In Point");
    keyMap->addBinding(Qt::Key_End, InputEvent::Modifiers(),
                      am->getAction(Impl::ACTION_GOTO_OUT_POINT), "Go to Out Point");
    
    // ==================== Markers ====================
    
    keyMap->addBinding(Qt::Key_M, InputEvent::Modifiers(),
                      am->getAction(Impl::ACTION_ADD_MARKER), "Add Marker");
    keyMap->addBinding(Qt::Key_M, kShift,
                      am->getAction(Impl::ACTION_NEXT_MARKER), "Next Marker");
    keyMap->addBinding(Qt::Key_M, kCtrlShift,
                      am->getAction(Impl::ACTION_PREV_MARKER), "Previous Marker");
    
    // Chapters
    keyMap->addBinding(Qt::Key_Return, kShift,
                      am->getAction(Impl::ACTION_NEXT_CHAPTER), "Next Chapter");

    // ==================== Playback Speed ====================

    keyMap->addBinding(Qt::Key_0, kCtrl,
                      am->getAction(Impl::ACTION_SPEED_NORMAL), "Normal Speed");
    keyMap->addBinding(Qt::Key_1, kCtrl,
                      am->getAction(Impl::ACTION_SPEED_HALF), "Half Speed");
    keyMap->addBinding(Qt::Key_2, kCtrl,
                      am->getAction(Impl::ACTION_SPEED_DOUBLE), "Double Speed");

    keyMap->addBinding(Qt::Key_L, kAlt,
                      am->getAction(Impl::ACTION_TOGGLE_LOOP), "Toggle Loop");
    
    // ==================== Scrubbing (drag on timeline) ====================
    // These would be bound to mouse drag events in actual implementation
    
    qDebug() << "Registered playback shortcuts";
}

void ArtifactPlaybackShortcuts::setEnabled(bool enabled) {
    impl_->enabled_ = enabled;
    if (impl_->inputOperator_) {
        impl_->inputOperator_->setEnabled(enabled);
    }
}

bool ArtifactPlaybackShortcuts::isEnabled() const {
    return impl_->enabled_;
}

void ArtifactPlaybackShortcuts::setActiveContext(const QString& context) {
    impl_->activeContext_ = context;
    if (impl_->inputOperator_) {
        impl_->inputOperator_->setActiveContext(context);
    }
}

QString ArtifactPlaybackShortcuts::activeContext() const {
    return impl_->activeContext_;
}

// ==================== Playback Actions ====================

void ArtifactPlaybackShortcuts::playPause() {
    if (!impl_->controller_) return;
    impl_->controller_->togglePlayPause();
    emit shortcutExecuted(Impl::ACTION_PLAY_PAUSE);
}

void ArtifactPlaybackShortcuts::play() {
    if (!impl_->controller_) return;
    impl_->controller_->play();
    emit shortcutExecuted(Impl::ACTION_PLAY);
}

void ArtifactPlaybackShortcuts::pause() {
    if (!impl_->controller_) return;
    impl_->controller_->pause();
    emit shortcutExecuted(Impl::ACTION_PAUSE);
}

void ArtifactPlaybackShortcuts::stop() {
    if (!impl_->controller_) return;
    impl_->controller_->stop();
    emit shortcutExecuted(Impl::ACTION_STOP);
}

void ArtifactPlaybackShortcuts::goToStart() {
    if (!impl_->controller_) return;
    impl_->controller_->goToStartFrame();
    emit shortcutExecuted(Impl::ACTION_GOTO_START);
}

void ArtifactPlaybackShortcuts::goToEnd() {
    if (!impl_->controller_) return;
    impl_->controller_->goToEndFrame();
    emit shortcutExecuted(Impl::ACTION_GOTO_END);
}

void ArtifactPlaybackShortcuts::nextFrame() {
    if (!impl_->controller_) return;
    impl_->controller_->goToNextFrame();
    emit shortcutExecuted(Impl::ACTION_NEXT_FRAME);
}

void ArtifactPlaybackShortcuts::previousFrame() {
    if (!impl_->controller_) return;
    impl_->controller_->goToPreviousFrame();
    emit shortcutExecuted(Impl::ACTION_PREV_FRAME);
}

void ArtifactPlaybackShortcuts::goToFrame(int frame) {
    if (!impl_->controller_) return;
    impl_->controller_->goToFrame(FramePosition(frame));
    emit shortcutExecuted("playback.goto_frame");
}

// ==================== In/Out Point Actions ====================

void ArtifactPlaybackShortcuts::setInPoint() {
    if (!impl_->inOutPoints_ || !impl_->controller_) return;
    auto frame = impl_->controller_->currentFrame();
    impl_->inOutPoints_->setInPoint(frame);
    // emit inPointSet(frame.value());
    emit shortcutExecuted(Impl::ACTION_SET_IN_POINT);
}

void ArtifactPlaybackShortcuts::setOutPoint() {
    if (!impl_->inOutPoints_ || !impl_->controller_) return;
    auto frame = impl_->controller_->currentFrame();
    impl_->inOutPoints_->setOutPoint(frame);
    // emit outPointSet(frame.value());
    emit shortcutExecuted(Impl::ACTION_SET_OUT_POINT);
}

void ArtifactPlaybackShortcuts::clearInPoint() {
    if (!impl_->inOutPoints_) return;
    impl_->inOutPoints_->clearInPoint();
    emit shortcutExecuted(Impl::ACTION_CLEAR_IN_POINT);
}

void ArtifactPlaybackShortcuts::clearOutPoint() {
    if (!impl_->inOutPoints_) return;
    impl_->inOutPoints_->clearOutPoint();
    emit shortcutExecuted(Impl::ACTION_CLEAR_OUT_POINT);
}

void ArtifactPlaybackShortcuts::clearInOutPoints() {
    if (!impl_->inOutPoints_) return;
    impl_->inOutPoints_->clearAllPoints();
    emit shortcutExecuted(Impl::ACTION_CLEAR_IN_OUT);
}

void ArtifactPlaybackShortcuts::goToInPoint() {
    if (!impl_->inOutPoints_ || !impl_->controller_) return;
    auto inPoint = impl_->inOutPoints_->inPoint();
    if (inPoint) {
        impl_->controller_->goToFrame(*inPoint);
    }
    emit shortcutExecuted(Impl::ACTION_GOTO_IN_POINT);
}

void ArtifactPlaybackShortcuts::goToOutPoint() {
    if (!impl_->inOutPoints_ || !impl_->controller_) return;
    auto outPoint = impl_->inOutPoints_->outPoint();
    if (outPoint) {
        impl_->controller_->goToFrame(*outPoint);
    }
    emit shortcutExecuted(Impl::ACTION_GOTO_OUT_POINT);
}

// ==================== Marker Actions ====================

void ArtifactPlaybackShortcuts::addMarker(const QString& comment) {
    if (!impl_->inOutPoints_ || !impl_->controller_) return;
    auto frame = impl_->controller_->currentFrame();
    auto* marker = impl_->inOutPoints_->addMarker(frame, comment, MarkerType::Comment);
    Q_UNUSED(marker);
    // emit markerAdded(frame.value(), comment);
    emit shortcutExecuted(Impl::ACTION_ADD_MARKER);
}

void ArtifactPlaybackShortcuts::addChapterMarker(const QString& name) {
    if (!impl_->inOutPoints_ || !impl_->controller_) return;
    auto frame = impl_->controller_->currentFrame();
    auto* marker = impl_->inOutPoints_->addMarker(frame, name, MarkerType::Chapter);
    Q_UNUSED(marker);
    // emit markerAdded(frame.value(), name);
    emit shortcutExecuted(Impl::ACTION_ADD_CHAPTER);
}

void ArtifactPlaybackShortcuts::goToNextMarker() {
    if (!impl_->controller_) return;
    impl_->controller_->goToNextMarker();
    emit shortcutExecuted(Impl::ACTION_NEXT_MARKER);
}

void ArtifactPlaybackShortcuts::goToPreviousMarker() {
    if (!impl_->controller_) return;
    impl_->controller_->goToPreviousMarker();
    emit shortcutExecuted(Impl::ACTION_PREV_MARKER);
}

void ArtifactPlaybackShortcuts::goToNextChapter() {
    if (!impl_->controller_) return;
    impl_->controller_->goToNextChapter();
    emit shortcutExecuted(Impl::ACTION_NEXT_CHAPTER);
}

void ArtifactPlaybackShortcuts::goToPreviousChapter() {
    if (!impl_->controller_) return;
    impl_->controller_->goToPreviousChapter();
    emit shortcutExecuted(Impl::ACTION_PREV_CHAPTER);
}

void ArtifactPlaybackShortcuts::deleteMarkerAtCurrent() {
    if (!impl_->inOutPoints_ || !impl_->controller_) return;
    auto frame = impl_->controller_->currentFrame();
    impl_->inOutPoints_->removeMarker(frame);
    emit shortcutExecuted(Impl::ACTION_DELETE_MARKER);
}

void ArtifactPlaybackShortcuts::clearAllMarkers() {
    if (!impl_->inOutPoints_) return;
    impl_->inOutPoints_->clearAllMarkers();
    emit shortcutExecuted("playback.clear_markers");
}

// ==================== Speed Actions ====================

void ArtifactPlaybackShortcuts::setSpeedNormal() {
    if (!impl_->controller_) return;
    impl_->controller_->setPlaybackSpeed(1.0f);
    emit shortcutExecuted(Impl::ACTION_SPEED_NORMAL);
}

void ArtifactPlaybackShortcuts::setSpeedHalf() {
    if (!impl_->controller_) return;
    impl_->controller_->setPlaybackSpeed(0.5f);
    emit shortcutExecuted(Impl::ACTION_SPEED_HALF);
}

void ArtifactPlaybackShortcuts::setSpeedDouble() {
    if (!impl_->controller_) return;
    impl_->controller_->setPlaybackSpeed(2.0f);
    emit shortcutExecuted(Impl::ACTION_SPEED_DOUBLE);
}

void ArtifactPlaybackShortcuts::toggleLoop() {
    if (!impl_->controller_) return;
    impl_->controller_->setLooping(!impl_->controller_->isLooping());
    emit shortcutExecuted(Impl::ACTION_TOGGLE_LOOP);
}

// ==================== Scrubbing ====================

void ArtifactPlaybackShortcuts::startScrub() {
    if (!impl_->controller_) return;
    // Pause and prepare for scrubbing
    if (impl_->controller_->isPlaying()) {
        impl_->controller_->pause();
    }
}

void ArtifactPlaybackShortcuts::scrubTo(int frame) {
    if (!impl_->controller_) return;
    impl_->controller_->goToFrame(FramePosition(frame));
}

void ArtifactPlaybackShortcuts::endScrub() {
    // Could start playing again if was playing before scrub
}

} // namespace Artifact
