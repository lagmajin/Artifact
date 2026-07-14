module;
#include <QColor>
#include <QGroupBox>
#include <QLabel>
#include <QLayout>
#include <QLineEdit>
#include <QInputDialog>
#include <QMouseEvent>
#include <QPalette>
#include <QSettings>
#include <QString>
#include <QStringList>
#include <QToolButton>
#include <QVariant>
#include <algorithm>
#include <memory>
#include <utility>

module Artifact.Widgets.ArtifactPropertyWidget;

import std;
import Property;
import Property.Abstract;
import Property.Group;
import Widgets.Utils.CSS;
import Artifact.Widgets.Timeline;
import Artifact.Widgets.LayerPanelWidget;
import Artifact.Widgets.PropertyEditor;
import Artifact.Widgets.ExpressionCopilotWidget;
import Artifact.Layer.Abstract;
import Artifact.Composition.Abstract;
import Artifact.Layer.Text;
import Artifact.Layer.InitParams;
import Artifact.Layers.Selection.Manager;
import Application.AppSettings;
import Artifact.Service.Playback;
import Event.Bus;
import Time.Rational;
import Settings.Accessibility;
import Undo.UndoManager;

namespace Artifact {
namespace detail {

constexpr int kPropertyLabelMinWidth = 132;
constexpr int kPropertyLabelMaxWidth = 184;


QColor propertyWidgetThemeColor(const QString &value, const QColor &fallback) {
  const QColor color(value);
  return color.isValid() ? color : fallback;
}

QColor propertyWidgetBlendColor(const QColor &a, const QColor &b, const qreal t) {
  const qreal clamped = std::clamp(t, 0.0, 1.0);
  return QColor::fromRgbF(a.redF() * (1.0 - clamped) + b.redF() * clamped,
                          a.greenF() * (1.0 - clamped) + b.greenF() * clamped,
                          a.blueF() * (1.0 - clamped) + b.blueF() * clamped,
                          a.alphaF() * (1.0 - clamped) + b.alphaF() * clamped);
}



bool shouldHideInspectorPropertyGroup(const QString &groupName);

ArtifactTimelineWidget *activeTimelineWidget(QWidget *root) {
  if (!root) {
    return nullptr;
  }
  const auto widgets = root->window()->findChildren<ArtifactTimelineWidget *>();
  for (auto *widget : widgets) {
    if (widget && widget->hasFocus()) {
      return widget;
    }
  }
  for (auto *widget : widgets) {
    if (widget && widget->isVisible()) {
      return widget;
    }
  }
  return widgets.isEmpty() ? nullptr : widgets.front();
}

std::shared_ptr<ArtifactCore::AbstractProperty>
findPropertyByName(const std::vector<std::shared_ptr<ArtifactCore::AbstractProperty>> &properties,
                   const QString &name) {
  auto it = std::find_if(
      properties.begin(), properties.end(),
      [&name](const std::shared_ptr<ArtifactCore::AbstractProperty> &property) {
        return property &&
               property->getName().compare(name, Qt::CaseInsensitive) == 0;
      });
  return it != properties.end() ? *it : nullptr;
}

bool boolPropertyValue(
    const std::vector<std::shared_ptr<ArtifactCore::AbstractProperty>> &properties,
    const QString &name, const bool fallback = false) {
  if (const auto property = findPropertyByName(properties, name)) {
    return property->getValue().toBool();
  }
  return fallback;
}

bool timelineAutoKeyEnabled() {
  if (auto *settings = ArtifactCore::ArtifactAppSettings::instance()) {
    return settings->timelineAutoKeyEnabled();
  }
  return false;
}

bool timelineAutoKeyAppliesToLayer(const ArtifactAbstractLayerPtr &layer) {
  if (!layer) {
    return false;
  }
  const auto *settings = ArtifactCore::ArtifactAppSettings::instance();
  if (!settings) {
    return true;
  }
  const QString scope = settings->timelineAutoKeyScopeText();
  if (scope.compare(QStringLiteral("Current Layer"), Qt::CaseInsensitive) == 0) {
    if (auto *selection = ArtifactLayerSelectionManager::instance()) {
      return selection->currentLayer() == layer;
    }
    return false;
  }
  if (scope.compare(QStringLiteral("Selected Layers"), Qt::CaseInsensitive) == 0) {
    if (auto *selection = ArtifactLayerSelectionManager::instance()) {
      return selection->isSelected(layer);
    }
    return false;
  }
  return true;
}

bool timelinePropertyAllowedByKeyingSet(const ArtifactCore::AbstractProperty &property) {
  const auto *settings = ArtifactCore::ArtifactAppSettings::instance();
  if (!settings) {
    return true;
  }

  const QString mode = settings->timelineKeyingSetModeText();
  const QString propertyName = property.getName().trimmed();

  if (mode.compare(QStringLiteral("Transform Only"), Qt::CaseInsensitive) == 0) {
    return propertyName.startsWith(QStringLiteral("transform."), Qt::CaseInsensitive) ||
           propertyName.compare(QStringLiteral("layer.opacity"), Qt::CaseInsensitive) == 0;
  }

  if (mode.compare(QStringLiteral("Custom"), Qt::CaseInsensitive) == 0) {
    const QStringList customPaths = settings->timelineCustomKeyingSetPropertyPaths();
    if (customPaths.isEmpty()) {
      return propertyName.startsWith(QStringLiteral("transform."), Qt::CaseInsensitive) ||
             propertyName.compare(QStringLiteral("layer.opacity"), Qt::CaseInsensitive) == 0;
    }
    return std::any_of(customPaths.begin(), customPaths.end(),
                       [&propertyName](const QString &path) {
                         return propertyName.compare(path.trimmed(), Qt::CaseInsensitive) == 0;
                       });
  }

  return true;
}

int intPropertyValue(
    const std::vector<std::shared_ptr<ArtifactCore::AbstractProperty>> &properties,
    const QString &name, const int fallback = 0) {
  if (const auto property = findPropertyByName(properties, name)) {
    return property->getValue().toInt();
  }
  return fallback;
}

bool isComponentActivationProperty(const QString &propertyName) {
  static const QStringList activationProperties = {
      QStringLiteral("physics.enabled"),
      QStringLiteral("component.script.enabled"),
      QStringLiteral("component.layout.enabled"),
      QStringLiteral("component.cloner.enabled"),
      QStringLiteral("component.collision.enabled"),
      QStringLiteral("component.crowd.enabled"),
      QStringLiteral("component.particleEmitter.enabled"),
      QStringLiteral("component.fluid.enabled"),
  };
  return activationProperties.contains(propertyName, Qt::CaseInsensitive);
}

std::vector<std::shared_ptr<ArtifactCore::AbstractProperty>> filteredGroupProperties(
    const ArtifactAbstractLayerPtr &layer, const QString &groupName,
    const std::vector<std::shared_ptr<ArtifactCore::AbstractProperty>> &properties) {
  const QString normalizedGroup = groupName.trimmed();
  const auto layerBool = [&layer](const QString &name, const bool fallback = false) {
    if (!layer) {
      return fallback;
    }
    const auto property = layer->getProperty(name);
    return property ? property->getValue().toBool() : fallback;
  };
  const auto layerInt = [&layer](const QString &name, const int fallback = 0) {
    if (!layer) {
      return fallback;
    }
    const auto property = layer->getProperty(name);
    return property ? property->getValue().toInt() : fallback;
  };
  const auto groupBool = [&properties, &layerBool](const QString &name,
                                                   const bool fallback = false) {
    if (const auto property = findPropertyByName(properties, name)) {
      return property->getValue().toBool();
    }
    return layerBool(name, fallback);
  };
  const auto groupInt = [&properties, &layerInt](const QString &name,
                                                 const int fallback = 0) {
    if (const auto property = findPropertyByName(properties, name)) {
      return property->getValue().toInt();
    }
    return layerInt(name, fallback);
  };

  if (normalizedGroup.compare(QStringLiteral("Physics"), Qt::CaseInsensitive) == 0 &&
      !groupBool(QStringLiteral("physics.enabled"))) {
    return {};
  }

  if (shouldHideInspectorPropertyGroup(normalizedGroup)) {
    return {};
  }

  // Component activation is controlled by the Components tab item itself.
  // Showing the same On/Off row above its settings duplicates that control.
  std::vector<std::shared_ptr<ArtifactCore::AbstractProperty>> visibleProperties;
  visibleProperties.reserve(properties.size());
  for (const auto &property : properties) {
    if (property && !isComponentActivationProperty(property->getName())) {
      visibleProperties.push_back(property);
    }
  }

  if (normalizedGroup.compare(QStringLiteral("Layer"), Qt::CaseInsensitive) == 0) {
    std::vector<std::shared_ptr<ArtifactCore::AbstractProperty>> filtered;
    filtered.reserve(visibleProperties.size());
    for (const auto &property : visibleProperties) {
      if (!property) {
        continue;
      }
      const QString name = property->getName();
      const bool isStateProperty =
          name == QStringLiteral("layer.visible") ||
          name == QStringLiteral("layer.locked") ||
          name == QStringLiteral("layer.selectionLocked") ||
          name == QStringLiteral("layer.transformLocked") ||
          name == QStringLiteral("layer.timingLocked") ||
          name == QStringLiteral("layer.guide") ||
          name == QStringLiteral("layer.solo") ||
          name == QStringLiteral("layer.shy");
      if (isStateProperty) {
        continue;
      }
      filtered.push_back(property);
    }
    return filtered;
  }

  if (normalizedGroup.compare(QStringLiteral("Layout"), Qt::CaseInsensitive) == 0 &&
      !groupBool(QStringLiteral("component.layout.enabled"))) {
    return {};
  }

  if (normalizedGroup.compare(QStringLiteral("Crowd"), Qt::CaseInsensitive) == 0 &&
      !groupBool(QStringLiteral("component.crowd.enabled"))) {
    return {};
  }

  if (normalizedGroup.compare(QStringLiteral("Particle Emitter"),
                              Qt::CaseInsensitive) == 0 &&
      !groupBool(QStringLiteral("component.particleEmitter.enabled"))) {
    return {};
  }

  if (normalizedGroup.compare(QStringLiteral("Cloner"), Qt::CaseInsensitive) == 0) {
    if (!groupBool(QStringLiteral("component.cloner.enabled"))) {
      return {};
    }
    const int mode = groupInt(QStringLiteral("component.cloner.mode"), 0);
    std::vector<std::shared_ptr<ArtifactCore::AbstractProperty>> filtered;
    filtered.reserve(visibleProperties.size());
    for (const auto &property : visibleProperties) {
      if (!property) {
        continue;
      }
      const QString name = property->getName();
      const bool isLinearOnly =
          name == QStringLiteral("component.cloner.cloneCount") ||
          name == QStringLiteral("component.cloner.offsetX") ||
          name == QStringLiteral("component.cloner.offsetY") ||
          name == QStringLiteral("component.cloner.offsetZ");
      const bool isGridOnly =
          name == QStringLiteral("component.cloner.columns") ||
          name == QStringLiteral("component.cloner.rows") ||
          name == QStringLiteral("component.cloner.depth") ||
          name == QStringLiteral("component.cloner.spacingX") ||
          name == QStringLiteral("component.cloner.spacingY") ||
          name == QStringLiteral("component.cloner.spacingZ");
      const bool isRadialOnly =
          name == QStringLiteral("component.cloner.radialCount") ||
          name == QStringLiteral("component.cloner.radius") ||
          name == QStringLiteral("component.cloner.startAngle") ||
          name == QStringLiteral("component.cloner.endAngle");
      const bool isRandomOnly =
          name == QStringLiteral("component.cloner.cloneCount") ||
          name == QStringLiteral("component.cloner.offsetX") ||
          name == QStringLiteral("component.cloner.offsetY") ||
          name == QStringLiteral("component.cloner.offsetZ") ||
          name == QStringLiteral("component.cloner.jitterX") ||
          name == QStringLiteral("component.cloner.jitterY") ||
          name == QStringLiteral("component.cloner.jitterZ") ||
          name == QStringLiteral("component.cloner.seed");
      const bool isSplineOnly =
          name == QStringLiteral("component.cloner.cloneCount") ||
          name == QStringLiteral("component.cloner.radius") ||
          name == QStringLiteral("component.cloner.startAngle") ||
          name == QStringLiteral("component.cloner.endAngle");

      if ((mode == 0 && (isGridOnly || isRadialOnly || isRandomOnly || isSplineOnly)) ||
          (mode == 1 && (isGridOnly || isRadialOnly || isSplineOnly)) ||
          (mode == 2 && (isGridOnly || isRadialOnly || isRandomOnly)) ||
          (mode == 3 && (isLinearOnly || isGridOnly || isRadialOnly || isSplineOnly)) ||
          (mode == 4 && (isLinearOnly || isGridOnly || isRadialOnly || isRandomOnly)) ||
          (mode == 5 && (isLinearOnly || isRadialOnly || isRandomOnly || isSplineOnly)) ||
          (mode == 6 && (isLinearOnly || isGridOnly || isRandomOnly || isSplineOnly))) {
        continue;
      }
      filtered.push_back(property);
    }
    return filtered;
  }

  if (normalizedGroup.compare(QStringLiteral("Solid"), Qt::CaseInsensitive) == 0) {
    const int fillType = groupInt(QStringLiteral("solid.fillType"),
                                 static_cast<int>(ArtifactSolidFillType::Solid));
    if (fillType == static_cast<int>(ArtifactSolidFillType::Solid)) {
      std::vector<std::shared_ptr<ArtifactCore::AbstractProperty>> filtered;
      filtered.reserve(properties.size());
      for (const auto &property : properties) {
        if (!property) {
          continue;
        }
        const QString name = property->getName();
        const bool isGradientOnly =
            name == QStringLiteral("solid.gradientStartColor") ||
            name == QStringLiteral("solid.gradientEndColor") ||
            name == QStringLiteral("solid.gradientAngleDegrees") ||
            name == QStringLiteral("solid.gradientReverse") ||
            name == QStringLiteral("solid.gradientCenterX") ||
            name == QStringLiteral("solid.gradientCenterY") ||
            name == QStringLiteral("solid.gradientScale") ||
            name == QStringLiteral("solid.gradientOffset");
        if (isGradientOnly) {
          continue;
        }
        filtered.push_back(property);
      }
      return filtered;
    }
  }

  // Shape Appearance group: hide gradient properties when fillType == Solid
  if (normalizedGroup.compare(QStringLiteral("Appearance"), Qt::CaseInsensitive) == 0) {
    const int fillType = groupInt(QStringLiteral("shape.fillType"),
                                 static_cast<int>(ArtifactSolidFillType::Solid));
    if (fillType == static_cast<int>(ArtifactSolidFillType::Solid)) {
      std::vector<std::shared_ptr<ArtifactCore::AbstractProperty>> filtered;
      filtered.reserve(properties.size());
      for (const auto &property : properties) {
        if (!property) {
          continue;
        }
        const QString name = property->getName();
        const bool isGradientOnly =
            name == QStringLiteral("shape.fillGradientStartColor") ||
            name == QStringLiteral("shape.fillGradientEndColor") ||
            name == QStringLiteral("shape.fillGradientAngle") ||
            name == QStringLiteral("shape.fillGradientCenterX") ||
            name == QStringLiteral("shape.fillGradientCenterY") ||
            name == QStringLiteral("shape.fillGradientRadius");
        if (isGradientOnly) {
          continue;
        }
        filtered.push_back(property);
      }
      return filtered;
    }
  }

  return visibleProperties;
}

int64_t playbackFrameRateValue(ArtifactPlaybackService *playback) {
  const auto frameRate = playback ? playback->frameRate() : FrameRate(30.0f);
  return std::max<int64_t>(
      1, static_cast<int64_t>(std::llround(frameRate.framerate())));
}

int64_t compositionFrameRateValue(
    const ArtifactAbstractComposition *composition) {
  if (!composition) {
    return 30;
  }
  return std::max<int64_t>(
      1, static_cast<int64_t>(
             std::llround(composition->frameRate().framerate())));
}

RationalTime currentPlaybackTime(ArtifactPlaybackService *playback) {
  const auto frame = playback ? playback->currentFrame() : FramePosition(0);
  return RationalTime(frame.framePosition(), playbackFrameRateValue(playback));
}

RationalTime currentPlaybackTime(ArtifactPlaybackService *playback,
                                 const ArtifactAbstractLayerPtr &layer) {
  if (layer) {
    if (auto *composition = static_cast<ArtifactAbstractComposition *>(
            layer->composition())) {
      return RationalTime(composition->framePosition().framePosition(),
                          compositionFrameRateValue(composition));
    }
  }
  return currentPlaybackTime(playback);
}

void applyThemeTextPalette(QWidget *widget, int shade) {
  if (!widget) {
    return;
  }
  const auto &theme = ArtifactCore::currentDCCTheme();
  QPalette pal = widget->palette();
  const QColor textColor(
      propertyWidgetThemeColor(theme.textColor, QColor(QStringLiteral("#E3E7EC"))));
  pal.setColor(QPalette::WindowText, textColor.darker(shade));
  pal.setColor(QPalette::Text, textColor.darker(shade));
  widget->setPalette(pal);
}

void applyPropertyPanelPalette(QWidget *widget, const bool elevated) {
  if (!widget) {
    return;
  }
  const auto &theme = ArtifactCore::currentDCCTheme();
  const QColor background =
      propertyWidgetThemeColor(theme.backgroundColor, QColor(QStringLiteral("#20242A")));
  const QColor surface = propertyWidgetThemeColor(theme.secondaryBackgroundColor,
                                    QColor(QStringLiteral("#2B3038")));
  const QColor text =
      propertyWidgetThemeColor(theme.textColor, QColor(QStringLiteral("#E3E7EC")));
  const QColor selection =
      propertyWidgetThemeColor(theme.selectionColor, QColor(QStringLiteral("#3C5B76")));
  const QColor border =
      propertyWidgetThemeColor(theme.borderColor, QColor(QStringLiteral("#404754")));
  const QColor accent =
      propertyWidgetThemeColor(theme.accentColor, QColor(QStringLiteral("#5E94C7")));

  widget->setAttribute(Qt::WA_StyledBackground, true);
  widget->setAutoFillBackground(true);
  QPalette pal = widget->palette();
  const QColor window =
      propertyWidgetBlendColor(background, surface, elevated ? 0.72 : 0.58);
  pal.setColor(QPalette::Window, window);
  pal.setColor(QPalette::WindowText, text);
  pal.setColor(QPalette::Base, surface);
  pal.setColor(QPalette::AlternateBase, propertyWidgetBlendColor(window, surface, 0.16));
  pal.setColor(QPalette::Text, text);
  pal.setColor(QPalette::Button, window);
  pal.setColor(QPalette::ButtonText, text);
  pal.setColor(QPalette::Highlight, selection);
  pal.setColor(QPalette::HighlightedText, background);
  pal.setColor(QPalette::Mid, border);
  pal.setColor(QPalette::Light, accent.lighter(120));
  widget->setPalette(pal);
}

void registerCurrentLayerPropertySnapshot(const ArtifactAbstractLayerPtr& layer,
                                          const QString& focusedEffectId)
{
  auto& registry = ArtifactCore::globalPropertyRegistry();
  registry.clear();

  if (!layer) {
    return;
  }

  const QString layerOwnerPath = QStringLiteral("layer:%1").arg(layer->id().toString());
  const auto layerGroups = layer->getLayerPropertyGroups();
  for (const auto& groupDef : layerGroups) {
    const QString groupName = groupDef.name().trimmed().isEmpty()
        ? QStringLiteral("Layer")
        : groupDef.name().trimmed();
    const QString ownerPath = ArtifactCore::propertyPathJoin(layerOwnerPath, groupName);
    registry.registerOwnerSnapshot(ownerPath,
                                   groupName,
                                   QStringLiteral("LayerGroup"),
                                   groupDef,
                                   true);
  }

  const auto effects = layer->getEffects();
  for (const auto& effect : effects) {
    if (!effect) {
      continue;
    }
    if (!focusedEffectId.trimmed().isEmpty() &&
        effect->effectID().toQString() != focusedEffectId.trimmed()) {
      continue;
    }

    ArtifactCore::PropertyGroup effectGroup(effect->displayName().toQString());
    for (const auto& property : effect->getProperties()) {
      effectGroup.addProperty(std::make_shared<ArtifactCore::AbstractProperty>(property));
    }

    const QString ownerPath = ArtifactCore::propertyPathJoin(
        layerOwnerPath,
        QStringLiteral("effect:%1").arg(effect->effectID().toQString()));
    registry.registerOwnerSnapshot(ownerPath,
                                   effect->displayName().toQString(),
                                   QStringLiteral("EffectGroup"),
                                   effectGroup,
                                   true);
  }
}

void applyPropertySearchPalette(QLineEdit *edit) {
  applyPropertyPanelPalette(edit, true);
  if (!edit) {
    return;
  }
  QFont font = edit->font();
  font.setPointSize(font.pointSize() > 0 ? std::max(font.pointSize(), 10) : 10);
  edit->setFont(font);
}

void applyPropertySectionLabel(QLabel *label, const bool prominent) {
  if (!label) {
    return;
  }
  const auto &theme = ArtifactCore::currentDCCTheme();
  const QColor text =
      propertyWidgetThemeColor(theme.textColor, QColor(QStringLiteral("#E3E7EC")));
  const QColor accent =
      propertyWidgetThemeColor(theme.accentColor, QColor(QStringLiteral("#5E94C7")));
  QPalette pal = label->palette();
  pal.setColor(QPalette::WindowText, prominent ? accent : text);
  label->setPalette(pal);
}

void applyPresentationToneLabel(QLabel *label,
                                LayerPresentationBadgeTone tone,
                                const bool prominent) {
  if (!label) {
    return;
  }
  const auto &theme = ArtifactCore::currentDCCTheme();
  const QColor text =
      propertyWidgetThemeColor(theme.textColor, QColor(QStringLiteral("#E3E7EC")));
  const QColor accent =
      propertyWidgetThemeColor(theme.accentColor, QColor(QStringLiteral("#5E94C7")));
  QColor toneColor = text;
  switch (tone) {
  case LayerPresentationBadgeTone::Container:
    toneColor = accent;
    break;
  case LayerPresentationBadgeTone::Media:
    toneColor = propertyWidgetBlendColor(text, accent, 0.22);
    break;
  case LayerPresentationBadgeTone::Motion:
    toneColor = accent.lighter(115);
    break;
  case LayerPresentationBadgeTone::Special:
    toneColor = accent.darker(110);
    break;
  case LayerPresentationBadgeTone::Neutral:
  default:
    toneColor = text;
    break;
  }
  QPalette pal = label->palette();
  pal.setColor(QPalette::WindowText, prominent ? toneColor : toneColor.darker(110));
  label->setPalette(pal);
}

void applyPropertySectionBox(QGroupBox *box) {
  if (!box) {
    return;
  }
  applyPropertyPanelPalette(box, true);
  QFont font = box->font();
  font.setPointSize(10);
  font.setWeight(QFont::DemiBold);
  box->setFont(font);
  box->setFlat(false);
}

bool isExpandedInspectorSection(const QString &groupName) {
  return isInspectorExpandedByDefaultLayerPropertyGroup(groupName);
}

bool shouldHideInspectorPropertyGroup(const QString &groupName);

bool shouldHideInspectorPropertyGroup(const QString &groupName) {
  return isInspectorHiddenLayerPropertyGroup(groupName);
}

bool isClonerSection(const QString &groupName) {
  return isClonerLayerPropertyGroup(groupName);
}

bool isSourceReframeSection(const QString &groupName) {
  return isSourceReframeLayerPropertyGroup(groupName);
}

std::vector<std::shared_ptr<ArtifactCore::AbstractProperty>>
applyFavoriteFilter(
    const std::vector<std::shared_ptr<ArtifactCore::AbstractProperty>> &properties,
    const bool favoriteOnly) {
  if (!favoriteOnly) {
    return properties;
  }

  QSettings settings(QStringLiteral("Artifact"),
                     QStringLiteral("PropertyFavorites"));
  const auto favs = settings.value(QStringLiteral("favorites")).toStringList();
  if (favs.isEmpty()) {
    return {};
  }

  std::vector<std::shared_ptr<ArtifactCore::AbstractProperty>> filtered;
  filtered.reserve(properties.size());
  for (const auto &property : properties) {
    if (property && favs.contains(property->getName(), Qt::CaseInsensitive)) {
      filtered.push_back(property);
    }
  }
  return filtered;
}

void clearLayoutRecursive(QLayout *layout) {
  if (!layout) {
    return;
  }
  while (QLayoutItem *item = layout->takeAt(0)) {
    if (QLayout *childLayout = item->layout()) {
      clearLayoutRecursive(childLayout);
    }
    if (QWidget *widget = item->widget()) {
      widget->hide();
      widget->deleteLater();
    }
    delete item;
  }
}



QString humanizePropertyLabel(QString name) {
  static const QHash<QString, QString> explicitLabels = {
      {QStringLiteral("transform.position.x"), QStringLiteral("Position X")},
      {QStringLiteral("transform.position.y"), QStringLiteral("Position Y")},
      {QStringLiteral("transform.scale.x"), QStringLiteral("Scale X")},
      {QStringLiteral("transform.scale.y"), QStringLiteral("Scale Y")},
      {QStringLiteral("transform.rotation"), QStringLiteral("Rotation")},
      {QStringLiteral("transform.anchor.x"), QStringLiteral("Anchor X")},
      {QStringLiteral("transform.anchor.y"), QStringLiteral("Anchor Y")},
      {QStringLiteral("layer.opacity"), QStringLiteral("Opacity")},
      {QStringLiteral("time.inPoint"), QStringLiteral("In Point")},
      {QStringLiteral("time.outPoint"), QStringLiteral("Out Point")},
      {QStringLiteral("time.startTime"), QStringLiteral("Start Time")}};
  if (const auto it = explicitLabels.constFind(name);
      it != explicitLabels.constEnd()) {
    return it.value();
  }

  const auto parts = name.split(QLatin1Char('.'), Qt::SkipEmptyParts);
  if (!parts.isEmpty()) {
    const bool isMaskPath =
        parts.front().compare(QStringLiteral("mask"), Qt::CaseInsensitive) == 0;
    const bool isRotoPath =
        parts.front().compare(QStringLiteral("roto"), Qt::CaseInsensitive) == 0;
    if (isMaskPath || isRotoPath) {
      const QString entryLabel =
          isRotoPath ? QStringLiteral("Roto") : QStringLiteral("Mask");
      if (parts.size() == 3 &&
          parts[2].compare(QStringLiteral("enabled"), Qt::CaseInsensitive) == 0) {
        return QStringLiteral("%1 %2 / Enabled")
            .arg(entryLabel)
            .arg(parts[1].toInt() + 1);
      }
      if (parts.size() == 5 &&
          parts[2].compare(QStringLiteral("path"), Qt::CaseInsensitive) == 0) {
        const QString pathLabel = QStringLiteral("%1 %2 / Path %3")
                                      .arg(entryLabel)
                                      .arg(parts[1].toInt() + 1)
                                      .arg(parts[3].toInt() + 1);
        const QString field = parts[4];
        if (field.compare(QStringLiteral("closed"), Qt::CaseInsensitive) == 0) {
          return pathLabel + QStringLiteral(" / Closed");
        }
        if (field.compare(QStringLiteral("opacity"), Qt::CaseInsensitive) == 0) {
          return pathLabel + QStringLiteral(" / Opacity");
        }
        if (field.compare(QStringLiteral("feather"), Qt::CaseInsensitive) == 0) {
          return pathLabel + QStringLiteral(" / Feather");
        }
        if (field.compare(QStringLiteral("featherHorizontal"), Qt::CaseInsensitive) == 0) {
          return pathLabel + QStringLiteral(" / Feather H");
        }
        if (field.compare(QStringLiteral("featherVertical"), Qt::CaseInsensitive) == 0) {
          return pathLabel + QStringLiteral(" / Feather V");
        }
        if (field.compare(QStringLiteral("featherInner"), Qt::CaseInsensitive) == 0) {
          return pathLabel + QStringLiteral(" / Feather Inner");
        }
        if (field.compare(QStringLiteral("featherOuter"), Qt::CaseInsensitive) == 0) {
          return pathLabel + QStringLiteral(" / Feather Outer");
        }
        if (field.compare(QStringLiteral("expansion"), Qt::CaseInsensitive) == 0) {
          return pathLabel + QStringLiteral(" / Expansion");
        }
        if (field.compare(QStringLiteral("inverted"), Qt::CaseInsensitive) == 0) {
          return pathLabel + QStringLiteral(" / Inverted");
        }
        if (field.compare(QStringLiteral("mode"), Qt::CaseInsensitive) == 0) {
          return pathLabel + QStringLiteral(" / Mode");
        }
        if (field.compare(QStringLiteral("name"), Qt::CaseInsensitive) == 0) {
          return pathLabel + QStringLiteral(" / Name");
        }
      }
    }
  }

  const int dot = name.lastIndexOf('.');
  if (dot >= 0 && dot + 1 < name.size()) {
    name = name.mid(dot + 1);
  }

  QString out;
  out.reserve(name.size() * 2);
  for (int i = 0; i < name.size(); ++i) {
    const QChar ch = name.at(i);
    if (ch == '_' || ch == '-') {
      out += ' ';
      continue;
    }
    if (i > 0 && ch.isUpper() && name.at(i - 1).isLetterOrNumber()) {
      out += ' ';
    }
    out += ch;
  }

  bool cap = true;
  for (int i = 0; i < out.size(); ++i) {
    if (out.at(i).isSpace()) {
      cap = true;
      continue;
    }
    if (cap) {
      out[i] = out.at(i).toUpper();
      cap = false;
    }
  }
  return out;
}

bool propertyMatchesFilter(const ArtifactCore::AbstractProperty &property,
                           const QString &filterText) {
  const QString query = filterText.trimmed();
  if (query.isEmpty()) {
    return true;
  }

  const QStringList alternatives = query.split(QLatin1Char('|'),
                                               Qt::SkipEmptyParts);
  if (alternatives.size() > 1) {
    for (const QString &alternative : alternatives) {
      if (propertyMatchesFilter(property, alternative.trimmed())) {
        return true;
      }
    }
    return false;
  }

  const QString key = property.getName();
  const QString friendly = humanizePropertyLabel(key);
  return key.contains(query, Qt::CaseInsensitive) ||
         friendly.contains(query, Qt::CaseInsensitive);
}

void notifyLayerPropertyAnimationChanged(const ArtifactAbstractLayerPtr &layer);

bool shouldHideInspectorProperty(const QString &propertyName) {
  return false;
}

void applyInspectorPropertyPresentation(
    const std::shared_ptr<ArtifactCore::AbstractProperty> &property) {
  if (!property) {
    return;
  }
  const QString propertyName = property->getName();
  if (propertyName.compare(QStringLiteral("transform.scale.x"),
                           Qt::CaseInsensitive) == 0 ||
      propertyName.compare(QStringLiteral("transform.scale.y"),
                           Qt::CaseInsensitive) == 0) {
    property->setUnit(QStringLiteral("%"));
  }
}

QString scaleSupplementaryText(const ArtifactAbstractLayerPtr &layer,
                               const QString &propertyName,
                               const QVariant &value) {
  if (!layer) {
    return {};
  }

  const auto sourceSize = layer->sourceSize();
  const bool isScaleX =
      propertyName.compare(QStringLiteral("transform.scale.x"),
                           Qt::CaseInsensitive) == 0;
  const bool isScaleY =
      propertyName.compare(QStringLiteral("transform.scale.y"),
                           Qt::CaseInsensitive) == 0;
  if (!isScaleX && !isScaleY) {
    return {};
  }

  const int baseSize = isScaleX ? sourceSize.width : sourceSize.height;
  if (baseSize <= 0) {
    return {};
  }

  const double scale = value.toDouble();
  const int actualSize = std::max(0, static_cast<int>(std::lround(
                                    static_cast<double>(baseSize) * scale)));
  return QStringLiteral("%1 px → %2 px")
      .arg(baseSize)
      .arg(actualSize);
}

void updateScaleSupplementaryText(
    ArtifactPropertyEditorRowWidget *row, const ArtifactAbstractLayerPtr &layer,
    const std::shared_ptr<ArtifactCore::AbstractProperty> &property,
    const QVariant &value) {
  if (!row || !property) {
    return;
  }
  row->setSupplementaryText(
      scaleSupplementaryText(layer, property->getName(), value));
}

std::vector<std::shared_ptr<ArtifactCore::AbstractProperty>>
inspectorProperties(
    const std::vector<std::shared_ptr<ArtifactCore::AbstractProperty>>
        &properties) {
  std::vector<std::shared_ptr<ArtifactCore::AbstractProperty>> filtered;
  filtered.reserve(properties.size());
  for (const auto &property : properties) {
    if (!property || shouldHideInspectorProperty(property->getName())) {
      continue;
    }
    applyInspectorPropertyPresentation(property);
    filtered.push_back(property);
  }
  return filtered;
}

QStringList loadFavoriteProperties() {
  QSettings settings(QStringLiteral("Artifact"),
                     QStringLiteral("PropertyFavorites"));
  return settings.value(QStringLiteral("favorites")).toStringList();
}

void saveFavoriteProperties(const QStringList &favorites) {
  QSettings settings(QStringLiteral("Artifact"),
                     QStringLiteral("PropertyFavorites"));
  settings.setValue(QStringLiteral("favorites"), favorites);
}

bool isFavorite(const QString &propertyPath) {
  return loadFavoriteProperties().contains(propertyPath,
                                           Qt::CaseInsensitive);
}

void toggleFavorite(const QString &propertyPath) {
  auto favs = loadFavoriteProperties();
  const int idx = favs.indexOf(propertyPath, Qt::CaseInsensitive);
  if (idx >= 0) {
    favs.removeAt(idx);
  } else {
    favs.push_back(propertyPath);
  }
  saveFavoriteProperties(favs);
}

void notifyLayerPropertyAnimationChanged(const ArtifactAbstractLayerPtr &layer) {
  if (!layer) {
    return;
  }
  auto *composition =
      static_cast<ArtifactAbstractComposition *>(layer->composition());
  layer->changed();
  ArtifactCore::globalEventBus().publish(LayerChangedEvent{
      composition ? composition->id().toString() : QString{},
      layer->id().toString(), LayerChangedEvent::ChangeType::Modified});
}

void launchExpressionCopilot(
    QWidget *parent, const QString &propertyName,
    const std::shared_ptr<ArtifactCore::AbstractProperty> &propertyPtr,
    const QString &initialExpression,
    const ArtifactAbstractLayerPtr &layer,
    const RationalTime &currentTime,
    const std::function<void(const QString &)> &applyHandler = {}) {
  auto *copilot = new ArtifactExpressionCopilotWidget(parent);
  copilot->setWindowFlags(Qt::Window | Qt::WindowStaysOnTopHint | Qt::Tool);
  copilot->setWindowTitle(
      QStringLiteral("Expression Copilot: %1").arg(propertyName));
  copilot->setExpressionText(initialExpression);
  if (layer) {
    auto *composition =
        static_cast<ArtifactAbstractComposition *>(layer->composition());
    QStringList layerNames;
    int currentLayerIndex = -1;
    if (composition) {
      const auto layers = composition->allLayerRef();
      layerNames.reserve(layers.size());
      for (int i = 0; i < layers.size(); ++i) {
        const auto &candidate = layers.at(i);
        if (!candidate) {
          continue;
        }
        layerNames.push_back(candidate->layerName());
        if (candidate->id() == layer->id()) {
          currentLayerIndex = i;
        }
      }
    }
    copilot->setPreviewContext(
        composition ? composition->settings().compositionName().toQString()
                    : QString(),
        composition ? composition->settings().compositionSize() : QSize(),
        layerNames,
        currentLayerIndex,
        layer->layerName(),
        propertyPtr ? propertyPtr->getValue() : QVariant(),
        currentTime.toDouble());
  } else {
    copilot->clearPreviewContext();
  }
  if (propertyPtr && applyHandler) {
    copilot->setApplyHandler([propertyPtr, applyHandler](const QString &expr) {
      propertyPtr->setExpression(expr);
      applyHandler(expr);
    });
  }
  copilot->setAttribute(Qt::WA_DeleteOnClose);
  copilot->move(QCursor::pos() - QPoint(150, 200));
  copilot->show();
}

ArtifactPropertyEditorRowWidget *createPropertyRow(
    QWidget *parent,
    const std::shared_ptr<ArtifactCore::AbstractProperty> &propertyPtr,
    const std::function<void(const QString &, const QVariant &)> &commitValue,
    const std::function<void(const QString &, const QVariant &)> &previewValue =
        {},
    const std::function<RationalTime()> &currentTimeProvider = {},
    const std::function<void(const QString &)> &keyframeChanged = {},
    const ArtifactAbstractLayerPtr &layer = {},
    const QString &contextLabel = {},
    const std::function<void(
        ArtifactPropertyEditorRowWidget *,
        const std::shared_ptr<ArtifactCore::AbstractProperty> &,
        const QVariant &)> &rowValueChanged = {}) {
  if (!propertyPtr)
    return nullptr;
  const auto &property = *propertyPtr;

  auto *editor = createPropertyEditorWidget(property, parent);
  if (!editor) {
    return nullptr;
  }

  if (auto *colorEditor = qobject_cast<ArtifactTextAnimatorColorEditor *>(editor)) {
    if (layer) {
      if (auto *textLayer = dynamic_cast<ArtifactTextLayer *>(layer.get())) {
        colorEditor->setLayer(textLayer);
      }
    }
  }

  const auto meta = property.metadata();
  const QString labelText = meta.displayLabel.isEmpty()
                                ? humanizePropertyLabel(property.getName())
                                : meta.displayLabel;
  const QString rowLabel = labelText;
  auto *row = new ArtifactPropertyEditorRowWidget(rowLabel, editor,
                                                  property.getName(), parent);
  QString activeStateOverrideName;
  QString audioReactiveSource;
  if (layer) {
    const auto *composition =
        dynamic_cast<const ArtifactAbstractComposition *>(layer->compositionObject());
    if (composition && !composition->activeStateVariantId().isEmpty()) {
      const QString activeStateId = composition->activeStateVariantId();
      const auto states = composition->stateVariants();
      const auto activeState = std::find_if(
          states.cbegin(), states.cend(), [&activeStateId](const auto &state) {
            return state.stateId == activeStateId;
          });
      if (activeState != states.cend() &&
          activeState->hasOverride(layer->id(), property.getName())) {
        activeStateOverrideName = activeState->displayName.trimmed().isEmpty()
                                      ? activeState->stateId
                                      : activeState->displayName;
      }
    }
    if (composition) {
      const auto bindings = composition->audioReactiveBindings();
      const auto binding = std::find_if(
          bindings.cbegin(), bindings.cend(), [&layer, &property](const auto &item) {
            return item.enabled && item.layerId == layer->id() &&
                   item.propertyPath == property.getName();
          });
      if (binding != bindings.cend()) {
        audioReactiveSource = binding->source;
      }
    }
  }
  if (!contextLabel.isEmpty()) {
    row->setProperty("propertyScope", contextLabel);
  }

  auto *playback = ArtifactPlaybackService::instance();

  const auto applyPreviewValue =
      [handler = previewValue ? previewValue : commitValue, propertyPtr,
       propertyName = property.getName(), row, rowValueChanged](const QVariant &value) {
        if (propertyPtr) {
          propertyPtr->setValue(value);
        }
        if (rowValueChanged) {
          rowValueChanged(row, propertyPtr, value);
        }
        handler(propertyName, value);
      };
  const auto applyCommitValue =
      [commitValue, propertyPtr, playback, currentTimeProvider,
       propertyName = property.getName(), row, rowValueChanged,
       keyframeChanged, layer](const QVariant &value) {
        if (propertyPtr) {
          propertyPtr->setValue(value);
          if (row && layer) {
            if (auto *timeline = activeTimelineWidget(row)) {
              timeline->applyValueToSelectedKeyframeArea(layer->id(), propertyName, value);
            }
          }
        }
        if (rowValueChanged) {
          rowValueChanged(row, propertyPtr, value);
        }
        const bool autoKey =
            timelineAutoKeyEnabled() && timelineAutoKeyAppliesToLayer(layer);
        const bool keyframeMode = row && row->isKeyframeModeEnabled();
        const bool allowedByKeyingSet =
            propertyPtr ? timelinePropertyAllowedByKeyingSet(*propertyPtr) : false;
        if (propertyPtr && (autoKey || keyframeMode) &&
            allowedByKeyingSet && (playback || currentTimeProvider)) {
          const auto nowTime = currentTimeProvider
                                   ? currentTimeProvider()
                                   : currentPlaybackTime(playback);
          propertyPtr->setAnimatable(true);
          propertyPtr->addKeyFrame(nowTime, value);
          if (row) {
            row->setKeyframeChecked(propertyPtr->hasKeyFrameAt(nowTime));
            row->setNavigationEnabled(true);
            if (autoKey) {
              row->setKeyframeModeEnabled(true);
            }
          }
          if (keyframeChanged) {
            keyframeChanged(propertyName);
          }
        }
        commitValue(propertyName, value);
      };
  editor->setPreviewHandler(applyPreviewValue);
  editor->setCommitHandler(applyCommitValue);

  QString editorTooltip = meta.tooltip;
  if (property.getName().compare(QStringLiteral("text.value"), Qt::CaseInsensitive) == 0) {
    const QString sourceTextHint =
        QStringLiteral("Source Text keyframes can be added from the timeline at the playhead, or edited from this row's menu.");
    editorTooltip = editorTooltip.isEmpty()
                        ? sourceTextHint
                        : QStringLiteral("%1\n%2").arg(editorTooltip, sourceTextHint);
    row->setSupplementaryText(
        activeStateOverrideName.isEmpty() && audioReactiveSource.isEmpty()
            ? QStringLiteral("Source Text uses timeline keyframes at the playhead. Use the row menu to edit the current text.")
            : QStringLiteral("%1%2Source Text uses timeline keyframes at the playhead.")
                  .arg(activeStateOverrideName.isEmpty()
                           ? QString()
                           : QStringLiteral("STATE · "),
                       audioReactiveSource.isEmpty()
                           ? QString()
                           : QStringLiteral("AUDIO · ")));
    row->setAuxAction(
        [layer, currentTimeProvider, playback, editor, row]() {
          const auto textProperty = layer ? layer->getProperty(QStringLiteral("text.value"))
                                          : std::shared_ptr<ArtifactCore::AbstractProperty>{};
          const auto textLayer = std::dynamic_pointer_cast<ArtifactTextLayer>(layer);
          if (!textLayer) {
            return;
          }
          const auto nowTime = currentTimeProvider ? currentTimeProvider()
                                                   : currentPlaybackTime(playback);
          const qint64 frame = nowTime.value();
          bool ok = false;
          const QString currentText = textLayer->sourceTextAtFrame(frame);
          const auto beforeKeyframes =
              textProperty ? textProperty->getKeyFrames()
                           : std::vector<ArtifactCore::KeyFrame>{};
          const QString editedText = QInputDialog::getMultiLineText(
              row, QStringLiteral("Edit Source Text"),
              QStringLiteral("Source text at the playhead:"), currentText, &ok);
          if (!ok) {
            return;
          }
          auto afterKeyframes = beforeKeyframes;
          ArtifactCore::KeyFrame editedKeyframe;
          editedKeyframe.time = nowTime;
          editedKeyframe.value = editedText;
          editedKeyframe.interpolation = ArtifactCore::InterpolationType::Constant;
          auto existing = std::find_if(afterKeyframes.begin(), afterKeyframes.end(),
                                       [nowTime](const ArtifactCore::KeyFrame &keyframe) {
                                         return keyframe.time == nowTime;
                                       });
          if (existing != afterKeyframes.end()) {
            *existing = editedKeyframe;
          } else {
            afterKeyframes.push_back(editedKeyframe);
          }
          if (auto *mgr = UndoManager::instance()) {
            mgr->push(std::make_unique<SetLayerPropertyKeyframesCommand>(
                layer, QStringLiteral("text.value"), beforeKeyframes,
                afterKeyframes, QStringLiteral("Edit Source Text")));
          } else if (textLayer) {
            textLayer->setSourceTextAtFrame(frame, editedText);
          }
          editor->setValueFromVariant(editedText);
          row->setKeyframeModeEnabled(true);
          row->setKeyframeChecked(true);
          row->setNavigationEnabled(true);
        },
        QStringLiteral("Set Source Text at Playhead..."));
  }
  if (!activeStateOverrideName.isEmpty() || !audioReactiveSource.isEmpty()) {
    if (property.getName().compare(QStringLiteral("text.value"),
                                   Qt::CaseInsensitive) != 0) {
      QStringList badges;
      if (!activeStateOverrideName.isEmpty()) {
        badges.append(QStringLiteral("STATE"));
      }
      if (!audioReactiveSource.isEmpty()) {
        badges.append(QStringLiteral("AUDIO"));
      }
      row->setSupplementaryText(badges.join(QStringLiteral(" · ")));
    }
    QStringList bindingHints;
    if (!activeStateOverrideName.isEmpty()) {
      bindingHints.append(QStringLiteral("Overridden by composition state: %1")
                              .arg(activeStateOverrideName));
    }
    if (!audioReactiveSource.isEmpty()) {
      bindingHints.append(QStringLiteral("Audio reactive source: %1")
                              .arg(audioReactiveSource));
    }
    const QString bindingHint = bindingHints.join(QStringLiteral("\n"));
    editorTooltip = editorTooltip.isEmpty()
                        ? bindingHint
                        : QStringLiteral("%1\n%2").arg(editorTooltip, bindingHint);
  }
  if (!editorTooltip.isEmpty()) {
    row->setEditorToolTip(editorTooltip);
  }

  const QVariant defaultValue = property.getDefaultValue();
  // ✅
  // 設定で有効化されている場合、またはデフォルト値が存在する場合にリセットボタンを表示
  const bool showResetButton =
      Artifact::artifactShouldShowPropertyResetButtons() ||
      defaultValue.isValid();
  row->setShowResetButton(showResetButton);
  if (defaultValue.isValid()) {
    row->setResetHandler([editor, defaultValue]() {
      editor->setValueFromVariant(defaultValue);
      editor->commitCurrentValue();
    });
  }

  const bool animatable = property.isAnimatable();
  row->setShowKeyframeButton(animatable);
  row->setNavigationEnabled(animatable);
  if (animatable) {
    const QString propertyName = property.getName();
    const auto track = propertyPtr->getKeyFrames();
    row->setNavigationEnabled(true);
    if (playback || currentTimeProvider) {
      const auto now = currentTimeProvider ? currentTimeProvider()
                                           : currentPlaybackTime(playback);
      row->setKeyframeChecked(propertyPtr->hasKeyFrameAt(now));
      row->setKeyframeModeEnabled(propertyPtr->hasKeyFrameAt(now));
      const QVariant animatedValue = propertyPtr->interpolateValue(now);
      if (animatedValue.isValid()) {
        editor->setValueFromVariant(animatedValue);
      }
    } else {
      row->setKeyframeChecked(!track.empty());
      row->setKeyframeModeEnabled(!track.empty());
    }

    // キーフレームトグル (◆ボタン)
    row->setKeyframeHandler(
        [propertyPtr, playback, row, editor, keyframeChanged,
         currentTimeProvider, propertyName](bool checked) {
          if (!propertyPtr)
            return;
          if (checked && !timelinePropertyAllowedByKeyingSet(*propertyPtr)) {
            row->setKeyframeChecked(propertyPtr->hasKeyFrameAt(
                currentTimeProvider ? currentTimeProvider() : currentPlaybackTime(playback)));
            return;
          }
          const auto nowTime = currentTimeProvider
                                   ? currentTimeProvider()
                                   : currentPlaybackTime(playback);

          if (checked) {
            propertyPtr->setAnimatable(true);
            propertyPtr->addKeyFrame(nowTime, editor->value());
          } else {
            propertyPtr->removeKeyFrame(nowTime);
          }
          row->setKeyframeModeEnabled(checked);
          row->setKeyframeChecked(propertyPtr->hasKeyFrameAt(nowTime));
          row->setNavigationEnabled(true);
          if (keyframeChanged) {
            keyframeChanged(propertyName);
          }
        });

    row->setKeyframeAnchorHandler([propertyPtr, keyframeChanged, propertyName](
                                      ArtifactCore::KeyFrame::Anchor anchor) {
      if (!propertyPtr) {
        return;
      }
      const auto keyframes = propertyPtr->getKeyFrames();
      for (const auto &keyframe : keyframes) {
        propertyPtr->setKeyFrameAnchorAt(keyframe.time, anchor);
      }
      if (keyframeChanged) {
        keyframeChanged(propertyName);
      }
    });

    row->setKeyframeColorLabelHandler(
        [propertyPtr, keyframeChanged, propertyName](
            ArtifactCore::KeyFrame::ColorLabel label) {
          if (!propertyPtr) {
            return;
          }
          const auto keyframes = propertyPtr->getKeyFrames();
          for (const auto &keyframe : keyframes) {
            propertyPtr->setKeyFrameColorLabelAt(keyframe.time, label);
          }
          if (keyframeChanged) {
            keyframeChanged(propertyName);
          }
        });

    // ナビゲーション (◀ ▶ボタン)
    row->setNavigationHandler([propertyPtr, playback, currentTimeProvider](
                                  int direction) {
      if (!playback || !propertyPtr)
        return;
      auto track = propertyPtr->getKeyFrames();
      if (track.empty())
        return;
      std::sort(track.begin(), track.end(),
                [](const auto &lhs, const auto &rhs) {
                  return lhs.time < rhs.time;
                });

      const auto nowTime = currentTimeProvider
                               ? currentTimeProvider()
                               : currentPlaybackTime(playback);
      const int64_t fps_val = std::max<int64_t>(1, nowTime.scale());
      if (direction > 0) {
        // 次のキーフレームへ
        for (const auto &kf : track) {
          if (kf.time > nowTime) {
            playback->goToFrame(
                FramePosition(static_cast<int>(kf.time.rescaledTo(fps_val))));
            break;
          }
        }
      } else {
        // 前のキーフレームへ
        for (auto it = track.rbegin(); it != track.rend(); ++it) {
          if (it->time < nowTime) {
            playback->goToFrame(
                FramePosition(static_cast<int>(it->time.rescaledTo(fps_val))));
            break;
          }
        }
      }
    });
  }

  const bool showExpressionButton = animatable;
  row->setShowExpressionButton(showExpressionButton);
  if (showExpressionButton) {
    const QString initialExpression = propertyPtr && propertyPtr->hasExpression()
        ? propertyPtr->getExpression()
        : (editor ? editor->value().toString() : QString{});
    row->setExpressionHandler(
        [parent, propertyName = property.getName(), propertyPtr, layer, keyframeChanged,
         initialExpression, currentTimeProvider, playback]() {
          const auto nowTime = currentTimeProvider
                                   ? currentTimeProvider()
                                   : currentPlaybackTime(playback);
          launchExpressionCopilot(
              parent,
              propertyName,
              propertyPtr,
              initialExpression,
              layer,
              nowTime,
              [layer, keyframeChanged, propertyName](const QString &) {
                if (keyframeChanged) {
                  keyframeChanged(propertyName);
                }
                if (layer) {
                  notifyLayerPropertyAnimationChanged(layer);
                }
              });
        });
  }

  return row;
}

void alignPropertyRowLabels(
    const std::vector<ArtifactPropertyEditorRowWidget *> &rows,
    const int minimumLabelWidth = kPropertyLabelMinWidth,
    const int maximumLabelWidth = kPropertyLabelMaxWidth) {
  const float accelScale = Artifact::Accessibility::targetScale();
  const int minW = static_cast<int>(static_cast<float>(minimumLabelWidth) * accelScale + 0.5f);
  const int maxW = static_cast<int>(static_cast<float>(maximumLabelWidth) * accelScale + 0.5f);
  int labelWidth = minW;
  for (auto *row : rows) {
    if (!row || !row->label()) {
      continue;
    }
    labelWidth = std::max(labelWidth, row->label()->sizeHint().width());
  }
  labelWidth = std::clamp(labelWidth, minW, maxW);

  for (auto *row : rows) {
    if (!row || !row->label()) {
      continue;
    }
    row->label()->setMinimumWidth(labelWidth);
    row->label()->setMaximumWidth(labelWidth);
    row->label()->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    row->label()->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);
  }
}

void addRowsFromProperties(
    QWidget *parent, QVBoxLayout *layout,
    const std::vector<std::shared_ptr<ArtifactCore::AbstractProperty>>
        &properties,
    const QString &filterText,
    const std::function<void(const QString &, const QVariant &)> &commitValue,
    const std::function<void(const QString &, const QVariant &)> &previewValue,
    const std::function<RationalTime()> &currentTimeProvider,
    const std::function<void(const QString &)> &keyframeChanged,
    const ArtifactAbstractLayerPtr &layer,
    bool *addedAny,
    const QString &registryScope = {},
    QMultiHash<QString, ArtifactPropertyEditorRowWidget *> *registry = nullptr,
    std::vector<ArtifactPropertyEditorRowWidget *> *collectedRows = nullptr,
    const std::function<void(ArtifactPropertyEditorRowWidget *,
                             const std::shared_ptr<ArtifactCore::AbstractProperty> &)>
        &decorateRow = {},
    const std::function<void(
        ArtifactPropertyEditorRowWidget *,
        const std::shared_ptr<ArtifactCore::AbstractProperty> &,
        const QVariant &)> &rowValueChanged = {}) {
  for (const auto &ptr : properties) {
    if (!ptr || !propertyMatchesFilter(*ptr, filterText)) {
      continue;
    }
    if (auto *row =
            createPropertyRow(parent, ptr, commitValue, previewValue,
                              currentTimeProvider,
                              keyframeChanged, layer, registryScope,
                              rowValueChanged)) {
      if (decorateRow) {
        decorateRow(row, ptr);
      }
      layout->addWidget(row);
      if (registry) {
        const QString key = registryScope.isEmpty()
                                ? ptr->getName()
                                : QStringLiteral("%1/%2")
                                      .arg(registryScope, ptr->getName());
        registry->insert(key, row);
      }
      if (collectedRows) {
        collectedRows->push_back(row);
      }
      if (addedAny) {
        *addedAny = true;
      }
    }
  }
}

std::vector<std::shared_ptr<ArtifactCore::AbstractProperty>>
prioritizedSummaryProperties(
    const std::vector<std::shared_ptr<ArtifactCore::AbstractProperty>>
        &properties,
    const std::unordered_set<std::string> &preferredKeys,
    const std::size_t maxCount) {
  std::vector<std::shared_ptr<ArtifactCore::AbstractProperty>> preferred;
  std::vector<std::shared_ptr<ArtifactCore::AbstractProperty>> fallback;
  preferred.reserve(properties.size());
  fallback.reserve(properties.size());

  for (const auto &property : properties) {
    if (!property) {
      continue;
    }
    const auto key = property->getName().toStdString();
    if (preferredKeys.contains(key)) {
      preferred.push_back(property);
    } else {
      fallback.push_back(property);
    }
  }

  preferred.insert(preferred.end(), fallback.begin(), fallback.end());
  if (preferred.size() > maxCount) {
    preferred.resize(maxCount);
  }
  return preferred;
}



} // namespace detail
} // namespace Artifact
