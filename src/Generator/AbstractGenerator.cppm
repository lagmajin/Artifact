module;
#include <utility>


module Artifact.Generator.Abstract;

namespace Artifact
{

 class Generator::Impl {
 public:
  Impl() = default;
  ~Impl() = default;
 };

	 Generator::Generator()
	  : impl_(new Impl())
	 {

	 }

	 Generator::~Generator()
	 {
	  delete impl_;
	  impl_ = nullptr;
	 }

};
