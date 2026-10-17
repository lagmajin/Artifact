module;

#include <wobjectdefs.h>

export module Artifact.Widgets.ViewportColorPipeline;

export namespace Artifact {

struct ViewportColorLutUpdate {
  const float *data = nullptr;
  int edge = 0;
  float inputMin = 0.0f;
  float inputMax = 4.0f;
  bool update = false;
  bool clear = false;
  bool active = false;
};

class ViewportColorPipeline {
public:
  ViewportColorPipeline();
  ~ViewportColorPipeline();

  ViewportColorLutUpdate prepare();
  void clear();

private:
  class Impl;
  Impl* impl_ = nullptr;
};

} // namespace Artifact
