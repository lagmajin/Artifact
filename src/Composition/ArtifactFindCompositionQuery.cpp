module;
#include <QString>
module Artifact.Composition.FindQuery;

import std;

namespace Artifact {

 class ArtifactFindCompositionQuery::Impl{
 private:
  QString searchWord;
 public:
  Impl();
  ~Impl();
  bool containsIgnoreCase(const std::string& target) const {

   return false;
  }
};


 ArtifactFindCompositionQuery::ArtifactFindCompositionQuery()
 {

 }

 ArtifactFindCompositionQuery::~ArtifactFindCompositionQuery()
 {

 }

};