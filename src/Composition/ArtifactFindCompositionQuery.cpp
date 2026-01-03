module;

module Artifact.Composition.FindQuery;

import std;

namespace Artifact {

 class ArtifactCompositionQuery::Impl{
 private:

 public:
  Impl();
  ~Impl();
  bool containsIgnoreCase(const std::string& target) const {
   if (searchWord.empty()) return true; // 検索ワードが空なら常にヒット

   auto it = std::search(
	target.begin(), target.end(),
	searchWord.begin(), searchWord.end(),
	[](char a, char b) { return std::tolower(a) == std::tolower(b); }
   );
   return it != target.end();
  }
};


 ArtifactFindCompositionQuery::ArtifactFindCompositionQuery()
 {

 }

 ArtifactFindCompositionQuery::~ArtifactFindCompositionQuery()
 {

 }

};