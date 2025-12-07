module;
export module Artifact.Generator.Abstract;

export namespace Artifact
{
 class Generator
 {
 private:
  class Impl;
  Impl* impl_;
 public:
  Generator();
  ~Generator();
 };
	
	

};