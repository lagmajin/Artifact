
module;
#include <QString>
#include "..\ArtifactWidgets\include\Define\DllExportMacro.hpp"
export module Composition.Settings;

import std;

import Color.Float;

export namespace ArtifactCore {


 class LIBRARY_DLL_API  CompositionSettings {
 private:
  class Impl;
  Impl* impl_;
 public:
  CompositionSettings();
  CompositionSettings(const CompositionSettings& settings);
  ~CompositionSettings();
  void setCompositionName(const QString& string);
 };







};