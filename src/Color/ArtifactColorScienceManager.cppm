module;

#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>
#include <wobjectimpl.h>

module Color.ScienceManager;

import Color.Float;
import Color.LUT;
import Color.ACES;
import Color.ColorSpace;

namespace Artifact {

using namespace ArtifactCore;

class ArtifactColorScienceManager::Impl {
public:
  ColorScienceSettings globalSettings_;
  std::unique_ptr<ColorLUT> currentLUT_;
  bool lutLoaded_ = false;

  // Per-composition settings
  std::map<std::string, CompositionColorSettings> compositionSettings_;

  // Cache for color space conversions
  mutable std::map<std::pair<ArtifactCore::ColorSpace, ArtifactCore::ColorSpace>,
                   std::function<FloatColor(const FloatColor &)>>
      conversionCache_;
};

ArtifactColorScienceManager::ArtifactColorScienceManager()
    : QObject(nullptr), impl_(new Impl()) {
  // Initialize with default global settings
  impl_->globalSettings_.mode = ColorScienceMode::Basic;
  impl_->globalSettings_.inputSpace = ArtifactCore::ColorSpace::sRGB;
  impl_->globalSettings_.workingSpace = ArtifactCore::ColorSpace::ACES_AP0;
  impl_->globalSettings_.outputSpace = ArtifactCore::ColorSpace::sRGB;
}

ArtifactColorScienceManager::~ArtifactColorScienceManager() { delete impl_; }

void ArtifactColorScienceManager::setSettings(
    const ColorScienceSettings &settings) {
  impl_->globalSettings_ = settings;
  applySettings();
  Q_EMIT settingsChanged();
}

ColorScienceSettings ArtifactColorScienceManager::getSettings() const {
  return impl_->globalSettings_;
}

bool ArtifactColorScienceManager::loadLUT(const std::string &path) {
  try {
    impl_->currentLUT_ =
        std::make_unique<ColorLUT>(QString::fromStdString(path));
    impl_->lutLoaded_ = true;
    impl_->globalSettings_.lutPath = path;
    Q_EMIT lutChanged();
    return true;
  } catch (const std::exception &e) {
    qWarning() << "Failed to load LUT:" << e.what();
  }
  return false;
}

void ArtifactColorScienceManager::setLUTIntensity(float intensity) {
  impl_->globalSettings_.lutIntensity = std::clamp(intensity, 0.0f, 1.0f);
  Q_EMIT settingsChanged();
}

float ArtifactColorScienceManager::getLUTIntensity() const {
  return impl_->globalSettings_.lutIntensity;
}

void ArtifactColorScienceManager::clearLUT() {
  impl_->currentLUT_.reset();
  impl_->lutLoaded_ = false;
  impl_->globalSettings_.lutPath.clear();
  impl_->globalSettings_.lutIntensity = 1.0f;
  Q_EMIT lutChanged();
}

ArtifactCore::FloatColor
ArtifactColorScienceManager::convertColor(const ArtifactCore::FloatColor &color,
                                          ArtifactCore::ColorSpace from,
                                          ArtifactCore::ColorSpace to) const {
  if (from == to)
    return color;

  // Check cache first
  auto key = std::make_pair(from, to);
  auto it = impl_->conversionCache_.find(key);
  if (it != impl_->conversionCache_.end()) {
    return it->second(color);
  }

  // Create conversion function
  std::function<ArtifactCore::FloatColor(const ArtifactCore::FloatColor &)>
      converter;

  // For now, implement basic conversions
  // In a full implementation, this would use the existing ColorSpace
  // infrastructure
  if (from == ArtifactCore::ColorSpace::sRGB &&
      to == ArtifactCore::ColorSpace::ACES_AP0) {
    converter =
        [](const ArtifactCore::FloatColor &c) -> ArtifactCore::FloatColor {
      // Simple sRGB to ACEScg approximation
      return FloatColor(c.r() * 0.6131f, c.g() * 0.6698f, c.b() * 0.5171f,
                        c.a());
    };
  } else if (from == ArtifactCore::ColorSpace::ACES_AP0 &&
             to == ArtifactCore::ColorSpace::sRGB) {
    converter =
        [](const ArtifactCore::FloatColor &c) -> ArtifactCore::FloatColor {
      // Simple ACEScg to sRGB approximation
      return FloatColor(c.r() * 1.7047f, c.g() * 1.4927f, c.b() * 1.9325f,
                        c.a());
    };
  } else {
    // Identity conversion for unsupported spaces
    converter = [](const ArtifactCore::FloatColor &c) { return c; };
  }

  // Cache the converter
  impl_->conversionCache_[key] = converter;
  return converter(color);
}

bool ArtifactColorScienceManager::isHDREnabled() const {
  return impl_->globalSettings_.enableHDR;
}

void ArtifactColorScienceManager::setHDREnabled(bool enabled) {
  impl_->globalSettings_.enableHDR = enabled;
  Q_EMIT settingsChanged();
}

std::vector<std::string> ArtifactColorScienceManager::getAvailableLUTs() const {
  std::vector<std::string> luts;

  // Search in standard LUT directories
  QStringList searchPaths = {
      QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) +
          "/LUTs",
      QDir::currentPath() + "/resources/luts", "./LUTs"};

  for (const QString &path : searchPaths) {
    QDir dir(path);
    if (dir.exists()) {
      QStringList filters = {"*.cube", "*.3dl", "*.lut"};
      QFileInfoList files = dir.entryInfoList(filters, QDir::Files);
      for (const QFileInfo &file : files) {
        luts.push_back(file.absoluteFilePath().toStdString());
      }
    }
  }

  return luts;
}

std::vector<ArtifactCore::ColorSpace>
ArtifactColorScienceManager::getSupportedColorSpaces() const {
  return {ArtifactCore::ColorSpace::Linear,
          ArtifactCore::ColorSpace::sRGB,
          ArtifactCore::ColorSpace::Rec709,
          ArtifactCore::ColorSpace::Rec2020,
          ArtifactCore::ColorSpace::P3,
          ArtifactCore::ColorSpace::ACES_AP0,
          ArtifactCore::ColorSpace::ACES_AP1};
}

void ArtifactColorScienceManager::applySettings() {
  // Apply current settings to the rendering pipeline
  // This would integrate with the rendering system
  qDebug() << "Applying color science settings:"
           << (int)impl_->globalSettings_.mode;
}

// Composition-specific settings implementation
void ArtifactColorScienceManager::setCompositionSettings(
    const std::string &compositionId,
    const CompositionColorSettings &settings) {
  impl_->compositionSettings_[compositionId] = settings;
  Q_EMIT compositionSettingsChanged(QString::fromStdString(compositionId));
}

CompositionColorSettings ArtifactColorScienceManager::getCompositionSettings(
    const std::string &compositionId) const {
  auto it = impl_->compositionSettings_.find(compositionId);
  if (it != impl_->compositionSettings_.end()) {
    return it->second;
  }
  // Return default settings
  return {compositionId, impl_->globalSettings_, true};
}

void ArtifactColorScienceManager::removeCompositionSettings(
    const std::string &compositionId) {
  auto it = impl_->compositionSettings_.find(compositionId);
  if (it != impl_->compositionSettings_.end()) {
    impl_->compositionSettings_.erase(it);
    Q_EMIT compositionSettingsChanged(QString::fromStdString(compositionId));
  }
}

std::vector<std::string>
ArtifactColorScienceManager::getCompositionsWithCustomSettings() const {
  std::vector<std::string> result;
  for (const auto &pair : impl_->compositionSettings_) {
    if (!pair.second.useGlobalSettings) {
      result.push_back(pair.first);
    }
  }
  return result;
}

ColorScienceSettings ArtifactColorScienceManager::getEffectiveSettings(
    const std::string &compositionId) const {
  auto compSettings = getCompositionSettings(compositionId);
  if (compSettings.useGlobalSettings) {
    return impl_->globalSettings_;
  }
  return compSettings.colorSettings;
}

} // namespace Artifact

W_OBJECT_IMPL(Artifact::ArtifactColorScienceManager)

// #include <moc_ArtifactColorScienceManager.cpp>
