module;

export module ArtifactProject.ProjectManagerService;

import std;

export namespace ArtifactCore
{
 class ArtifactProjectManagerService {
 private:
  class Impl;
 	//std::unique_ptr
 public:
  ArtifactProjectManagerService();
  ~ArtifactProjectManagerService();
  bool openProject(const std::string& path);
  bool closeProject();
  // ...
 };












};