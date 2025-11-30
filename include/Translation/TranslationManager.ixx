module;
export module Translation.Manager;

export namespace Artifact
{

 class TranslationManager
 {
 private:
  class Impl;
  Impl* impl_;
 public:
  TranslationManager();
  ~TranslationManager();
 };



};