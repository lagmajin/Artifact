module;

#include <cmath>
#include <algorithm>

#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <unordered_map>
#include <set>
#include <unordered_set>
#include <memory>
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
#include <QDebug>
#include <wobjectimpl.h>
module Artifact.Color.Wheels;




import ColorCollection.ColorGrading;

namespace Artifact {

// ==================== ColorWheelsProcessor::Impl ====================

class ColorWheelsProcessor::Impl {
public:
    ArtifactCore::ColorWheelsProcessor core_;
};

// ==================== ColorCurves::Impl ====================

class ColorCurves::Impl {
public:
    ArtifactCore::ColorCurves core_;
};

// ==================== ColorGrader::Impl ====================

class ColorGrader::Impl {
public:
    ArtifactCore::ColorGrader core_;
};

// ==================== ColorWheelsProcessor ====================

ColorWheelsProcessor::ColorWheelsProcessor(QObject* parent)
    : QObject(parent)
    , impl_(new Impl())
{
}
{
}

ColorWheelsProcessor::~ColorWheelsProcessor() = default;

void ColorWheelsProcessor::setWheelType(ArtifactCore::ColorWheelType type) {
    impl_->core_.setWheelType(type);
    emit paramsChanged();
}

void ColorWheelsProcessor::setLift(float r, float g, float b) {
    impl_->core_.setLift(r, g, b);
    emit paramsChanged();
}

void ColorWheelsProcessor::setGamma(float r, float g, float b) {
    impl_->core_.setGamma(r, g, b);
    emit paramsChanged();
}

void ColorWheelsProcessor::setGain(float r, float g, float b) {
    impl_->core_.setGain(r, g, b);
    emit paramsChanged();
}

void ColorWheelsProcessor::setOffset(float r, float g, float b) {
    impl_->core_.setOffset(r, g, b);
    emit paramsChanged();
}

void ColorWheelsProcessor::process(float* pixels, int width, int height) {
    impl_->core_.process(pixels, width, height);
}

ColorWheelsProcessor* ColorWheelsProcessor::createWarmLook() {
    auto* proc = new ColorWheelsProcessor();
    proc->impl_->core_ = ArtifactCore::ColorWheelsProcessor::createWarmLook();
    return proc;
}

ColorWheelsProcessor* ColorWheelsProcessor::createCoolLook() {
    auto* proc = new ColorWheelsProcessor();
    proc->impl_->core_ = ArtifactCore::ColorWheelsProcessor::createCoolLook();
    return proc;
}

ColorWheelsProcessor* ColorWheelsProcessor::createHighContrast() {
    auto* proc = new ColorWheelsProcessor();
    proc->impl_->core_ = ArtifactCore::ColorWheelsProcessor::createHighContrast();
    return proc;
}

ColorWheelsProcessor* ColorWheelsProcessor::createFade() {
    auto* proc = new ColorWheelsProcessor();
    proc->impl_->core_ = ArtifactCore::ColorWheelsProcessor::createFade();
    return proc;
}

// ==================== ColorCurves ====================

ColorCurves::ColorCurves(QObject* parent)
    : QObject(parent)
    , impl_(new Impl())
{
}

ColorCurves::~ColorCurves() = default;

void ColorCurves::setMasterCurve(const std::vector<CurvePoint>& points) {
    impl_->core_.setMasterCurve(points);
    emit curveChanged();
}

void ColorCurves::setRedCurve(const std::vector<CurvePoint>& points) {
    impl_->core_.setRedCurve(points);
    emit curveChanged();
}

void ColorCurves::setGreenCurve(const std::vector<CurvePoint>& points) {
    impl_->core_.setGreenCurve(points);
    emit curveChanged();
}

void ColorCurves::setBlueCurve(const std::vector<CurvePoint>& points) {
    impl_->core_.setBlueCurve(points);
    emit curveChanged();
}

void ColorCurves::applySCurve() {
    impl_->core_.applySCurve();
    emit curveChanged();
}

void ColorCurves::applyFadeIn() {
    impl_->core_.applyFadeIn();
    emit curveChanged();
}

void ColorCurves::applyFadeOut() {
    impl_->core_.applyFadeOut();
    emit curveChanged();
}

void ColorCurves::applyInvert() {
    impl_->core_.applyInvert();
    emit curveChanged();
}

void ColorCurves::applyPosterize(int levels) {
    impl_->core_.applyPosterize(levels);
    emit curveChanged();
}

void ColorCurves::process(float* pixels, int width, int height) {
    impl_->core_.process(pixels, width, height);
}

void ColorCurves::buildLUT() {
    impl_->core_.buildLUT();
}

void ColorCurves::reset() {
    impl_->core_.reset();
    emit curveChanged();
}

// ==================== ColorGrader ====================

ColorGrader::ColorGrader(QObject* parent)
    : QObject(parent)
    , impl_(new Impl())
{
    wheelsProcessor_ = new ColorWheelsProcessor(this);
    curvesProcessor_ = new ColorCurves(this);
}

ColorGrader::~ColorGrader() = default;

void ColorGrader::setEnabled(bool e) {
    impl_->core_.setEnabled(e);
    emit changed();
}

void ColorGrader::setIntensity(float i) {
    impl_->core_.setIntensity(i);
    emit changed();
}

void ColorGrader::process(float* pixels, int width, int height) {
    impl_->core_.process(pixels, width, height);
}

ColorGrader* ColorGrader::createFilmLook() {
    auto* grader = new ColorGrader();
    grader->impl_->core_ = ArtifactCore::ColorGrader::createFilmLook();
    return grader;
}

ColorGrader* ColorGrader::createCinematic() {
    auto* grader = new ColorGrader();
    grader->impl_->core_ = ArtifactCore::ColorGrader::createCinematic();
    return grader;
}

ColorGrader* ColorGrader::createNoir() {
    auto* grader = new ColorGrader();
    grader->impl_->core_ = ArtifactCore::ColorGrader::createNoir();
    return grader;
}

} // namespace Artifact
