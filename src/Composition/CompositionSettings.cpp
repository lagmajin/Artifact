module ;


module Composition.Settings;

import std;
import Utils;

namespace ArtifactCore{

 using namespace ArtifactCore;

 class CompositionSettings::Impl{
 private:
  QString compositionName_;


 public:
  Impl();
  ~Impl();
  QString compositionName() const;
  void setCompositionName(StringLike auto name);

 };
 
 QString CompositionSettings::Impl::compositionName() const
 {
  return compositionName_;
 }

 void CompositionSettings::Impl::setCompositionName(StringLike auto name)
 {
  compositionName_ = toQString(name);
  
 }

 CompositionSettings::Impl::Impl()
 {

 }

 CompositionSettings::Impl::~Impl()
 {

 }

 CompositionSettings::CompositionSettings()
 {

 }

 CompositionSettings::CompositionSettings(const CompositionSettings& settings)
 {

 }

 CompositionSettings::~CompositionSettings()
 {

 }

}