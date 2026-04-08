module;
#include <utility>

#include <memory>
#include <string>
#include <vector>
#include <wobjectdefs.h>

#include <QColor>
#include <QObject>
export module Color.ScienceManager;

import Color.Float;
import Color.LUT;
import Color.ACES;
import Color.ColorSpace;

export namespace Artifact {

using namespace ArtifactCore;

class ArtifactComposition;

enum class ColorScienceMode {
  None,    // No color management
  Basic,   // Simple color space conversion
  Advanced // Full ACES/LUT pipeline
};

enum class ColorSpacePreset {
  sRGB,
  Rec709,
  Rec2020,
  DCI_P3,
  ACEScg,
  ACES2065_1,
  Custom
};

struct ColorScienceSettings {
  ColorScienceMode mode = ColorScienceMode::None;
  ArtifactCore::ColorSpace inputSpace = ArtifactCore::ColorSpace::sRGB;
  ArtifactCore::ColorSpace workingSpace = ArtifactCore::ColorSpace::ACES_AP0;
  ArtifactCore::ColorSpace outputSpace = ArtifactCore::ColorSpace::sRGB;
  std::string lutPath;
  float lutIntensity = 1.0f;
  bool enableHDR = false;
};

// Per-composition color settings
struct CompositionColorSettings {
  std::string compositionId;
  ColorScienceSettings colorSettings;
  bool useGlobalSettings = true; // If false, use composition-specific settings
};

class ArtifactColorScienceManager : public QObject {
  W_OBJECT(ArtifactColorScienceManager)

private:
  class Impl;
  Impl *impl_;

public:
  ArtifactColorScienceManager();
  ~ArtifactColorScienceManager();

  // Settings management
  void setSettings(const ColorScienceSettings &settings);
  ColorScienceSettings getSettings() const;

  // LUT operations
  bool loadLUT(const std::string &path);
  void setLUTIntensity(float intensity);
  float getLUTIntensity() const;
  void clearLUT();

  // Color space operations
  ArtifactCore::FloatColor convertColor(const ArtifactCore::FloatColor &color,
                                        ArtifactCore::ColorSpace from,
                                        ArtifactCore::ColorSpace to) const;

  // HDR operations
  bool isHDREnabled() const;
  void setHDREnabled(bool enabled);

  // Composition-specific settings
  void setCompositionSettings(const std::string &compositionId,
                              const CompositionColorSettings &settings);
  CompositionColorSettings
  getCompositionSettings(const std::string &compositionId) const;
  void removeCompositionSettings(const std::string &compositionId);
  std::vector<std::string> getCompositionsWithCustomSettings() const;

  // Get effective settings (composition-specific or global)
  ColorScienceSettings
  getEffectiveSettings(const std::string &compositionId) const;

  // Utility functions
  std::vector<std::string> getAvailableLUTs() const;
  std::vector<ArtifactCore::ColorSpace> getSupportedColorSpaces() const;

  // Signals
  void settingsChanged() W_SIGNAL(settingsChanged);
  void lutChanged() W_SIGNAL(lutChanged);
  void compositionSettingsChanged(const QString &compositionId)
      W_SIGNAL(compositionSettingsChanged, compositionId);

private:
  void applySettings();
};

} // namespace Artifact
