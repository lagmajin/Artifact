module;
#include <QString>

export module Generator.Effector;

import std;
import Artifact.Layers.Abstract;

export namespace Artifact
{
 class AbstractGeneratorEffector
 {
 private:
  class Impl;
  Impl* impl_;
 public:
  void apply();

 };

};