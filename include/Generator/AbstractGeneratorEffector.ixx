module;
#include <QString>

export module Generator.Effector;

import std;
import Artifact.Layer.Abstract;

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