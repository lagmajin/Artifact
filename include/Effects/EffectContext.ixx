module;
#include <utility>
#include <cstdint>
#include <QRectF>
#include <QString>
#include <QByteArray>
#include <DeviceContext.h>
export module Artifact.Effect.Context;

import Artifact.Render.ROI;
import Image.ImageF32x4RGBAWithCache;
import Color.ColorSpace;

export namespace Artifact
{
 using namespace Diligent;

 class IEffectFrameSampler
 {
 public:
  virtual ~IEffectFrameSampler() = default;

  virtual bool sampleCurrentLayerFrame(
      std::int64_t compositionFrame,
      ArtifactCore::ImageF32x4RGBAWithCache& out) = 0;

  virtual bool sampleCurrentLayerFrameRelative(
      std::int64_t frameOffset,
      ArtifactCore::ImageF32x4RGBAWithCache& out) = 0;

  virtual bool sampleNamedInput(
      const QString& inputId,
      std::int64_t compositionFrame,
      ArtifactCore::ImageF32x4RGBAWithCache& out) = 0;
 };
	
 struct EffectContext
 {
  IDeviceContext* pDeviceContext = nullptr;
  QRectF roi;
  bool isInteractive = true;
  std::int64_t compositionFrame = 0;
  std::int64_t layerFrame = 0;
  double frameRate = 30.0;
  double timeSeconds = 0.0;
  float resolutionScale = 1.0f;
  float effectStrength = 1.0f;
  // Explicit color-space boundary for CPU/GPU parity and cache identity.
  ArtifactCore::ColorSpace inputColorSpace = ArtifactCore::ColorSpace::Linear;
  ArtifactCore::ColorSpace workingColorSpace = ArtifactCore::ColorSpace::Linear;
  ArtifactCore::ColorSpace outputColorSpace = ArtifactCore::ColorSpace::Linear;
  QString colorConfigIdentity;
  QByteArray evaluationCacheKey;
  IEffectFrameSampler* sampler = nullptr;
 };
};
