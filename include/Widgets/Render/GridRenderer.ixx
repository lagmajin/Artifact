module;

#include <BasicMath.hpp>
export module GridRenderer;



export namespace Artifact {

 class GridRenderer {
 private:
  class Impl;
  Impl* impl_;
 public:
  GridRenderer();
  ~GridRenderer();
  void Initialize();
 };









};