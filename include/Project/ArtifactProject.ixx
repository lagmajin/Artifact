﻿module;

//#include <memory>
#include <QObject>
#include <QPointer>

#include <wobjectdefs.h>
#include <QJsonObject>

#include <boost/signals2.hpp>

export module Project;

export import Project.Settings;



import std;

import Utils;

import Composition.Settings;

export namespace Artifact {

 using namespace ArtifactCore; 

 class ArtifactProjectSignalHelper {
 private:

 public:
  ArtifactProjectSignalHelper();
  ~ArtifactProjectSignalHelper();
 };

 
	

 class ArtifactProject :public QObject{
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
  std::optional<Id> createComposition(const CompositionSettings& settings);
  void addAssetFile();
  void addAssetFromPath(const QString& filepath);
  bool isNull() const;

 public:
	//signals
  void projectChanged()
   W_SIGNAL(projectChanged)
   void compositionCreated(const QString& id)
   W_SIGNAL(compositionCreated, id)

	

 };

 typedef std::shared_ptr<ArtifactProject> ArtifactProjectPtr;
 typedef std::weak_ptr<ArtifactProject>	 ArtifactProjectWeakPtr;

}