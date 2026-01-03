module ;
#include <QSize>
#include <QString>
#include "../ArtifactCore/include/Define/DllExportMacro.hpp"


export module Composition.Settings;

import std;


import Color.Float;
import Core.AspectRatio;
import Utils.String.UniString;


export namespace ArtifactCore {


 class LIBRARY_DLL_API  CompositionSettings {
 private:
  class Impl;
  Impl* impl_;
 public:
  CompositionSettings();
  CompositionSettings(const CompositionSettings& settings);
  ~CompositionSettings();
  UniString compositionName() const;
  void setCompositionName(const UniString& string);
  QSize compositionSize() const;
  void setCompositionSize(const QSize& size);


 };







};