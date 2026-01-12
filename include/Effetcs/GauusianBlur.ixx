module;
export module Artifact.Effect.GauusianBlur;

export namespace Artifact
{
 class GauusianBlur
 {
 private:
  class Impl;
  Impl* impl_;
 public:
  GauusianBlur();
  ~GauusianBlur();
 };

};