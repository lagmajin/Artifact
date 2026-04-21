module;
#include <utility>
#include <QRectF>
#include <DeviceContext.h>
export module Artifact.Effect.Context;

import Artifact.Render.ROI;

export namespace Artifact
{
 using namespace Diligent;
	
 struct EffectContext
 {
  IDeviceContext* pDeviceContext = nullptr;
  QRectF roi;
  bool isInteractive = true;
 };
};
