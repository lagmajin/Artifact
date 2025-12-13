module;

//#include <memory>
#include <QObject>
#include <QPointer>

#include <wobjectdefs.h>
#include <QJsonObject>

#include <boost/signals2.hpp>

export module Artifact.Project;

export import Artifact.Project.Settings;



import std;

import Utils;


import Composition.Settings;
import Artifact.Composition.Result;

W_REGISTER_ARGTYPE(ArtifactCore::CompositionID)

export namespace Artifact {

 using namespace ArtifactCore; 

 class ArtifactProjectSignalHelper {
 private:

 public:
  ArtifactProjectSignalHelper();
  ~ArtifactProjectSignalHelper();
 };

 
	

 class ArtifactProject final :public QObject{
  W_OBJECT(ArtifactProject)
 private:
  class Impl;
  Impl* impl_;

 public:
  ArtifactProject();
  ArtifactProject(const QString& name);
  ArtifactProject(const ArtifactProjectSettings& setting);
  ~ArtifactProject();
  ArtifactProjectSettings settings() const;
  void createComposition(const QString&name);
  template <typename NameT, typename SizeT>
   requires StringLike<NameT>&& SizeLike<SizeT>
  void createComposition(NameT name, SizeT size);
  CompositionResult createComposition(const CompositionSettings& settings);
  void addAssetFile();
  void addAssetFromPath(const QString& filepath);
  bool isNull() const;
  bool removeCompositionById(const CompositionID& id);
  void removeAllCompositions();
 	
  QJsonObject toJson() const;
 public:
	//signals
  void projectChanged()
   W_SIGNAL(projectChanged)
   void compositionCreated(const QString& id)
   W_SIGNAL(compositionCreated, id)

   void preRemoveAllCompositions()
   W_SIGNAL(preRemoveAllCompositions)

   void layerCreated()
   W_SIGNAL(layerCreated)

 	
 };


 typedef std::shared_ptr<ArtifactProject> ArtifactProjectPtr;
 typedef std::weak_ptr<ArtifactProject>	 ArtifactProjectWeakPtr;

}