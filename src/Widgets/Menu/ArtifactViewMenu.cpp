﻿module;
#include <QWidget>
#include <QMenu>

module Menu:View;

//#include "../../../include/Widgets/Menu/ArtifactViewMenu.hpp"





namespace Artifact {

 class ArtifactViewMenu::Impl {
 private:

 public:
  Impl();
  ~Impl();
 };

 ArtifactViewMenu::Impl::Impl()
 {

 }

 ArtifactViewMenu::Impl::~Impl()
 {

 }

 ArtifactViewMenu::ArtifactViewMenu(QWidget* parent/*=nullptr*/):QMenu(parent),impl_(new Impl())
 {
  setObjectName("ViewMenu");

  setTitle("View");
 }

 ArtifactViewMenu::~ArtifactViewMenu()
 {
  delete impl_;
 }

 void ArtifactViewMenu::registerView(const QString& name, QWidget* view)
 {

 }

};