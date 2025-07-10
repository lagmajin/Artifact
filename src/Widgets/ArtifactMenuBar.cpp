module;
#include <QString>
module Menu.MenuBar;

import Menu;





namespace Artifact {

 class ArtifactMenuBar::Impl {
 private:

 public:
  QMenuBar* menuBar = nullptr;
  QMenu* fileMenu=nullptr;
  QMenu* compositionMenu=nullptr;
  QMenu* layerMenu = nullptr;
  QMenu* viewMenu = nullptr;
  Impl(QMenuBar*menu);
  ~Impl();
  //void setUpUI(QMenu* menu);
 };

 ArtifactMenuBar::Impl::Impl(QMenuBar* menu)
 {

 }

 ArtifactMenuBar::Impl::~Impl()
 {

 }

 ArtifactMenuBar::ArtifactMenuBar(QWidget* parent) :QMenuBar(parent),impl(new Impl(this))
 {
  impl->fileMenu = new ArtifactFileMenu(this);


  impl->compositionMenu = new ArtifactCompositionMenu(this);
  impl->layerMenu = new ArtifactLayerMenu(this);
  impl->viewMenu = new ArtifactViewMenu(this);


  addMenu(impl->fileMenu);
  addMenu(impl->compositionMenu);
  addMenu(impl->viewMenu);
  QString styleSheet = R"(
        QMenuBar {
            background-color: #333333; /* �_�[�N�Ȕw�i�F */
            color: #FFFFFF; /* ���邢�e�L�X�g�F */
            border: 1px solid #555555; /* �킸���ȃ{�[�_�[ */
        }

        QMenuBar::item {
            spacing: 3px; /* ���j���[�A�C�e���Ԃ̃X�y�[�X */
            padding: 5px 10px; /* �p�f�B���O */
            background-color: transparent; /* �f�t�H���g�œ����Ȕw�i */
            color: #FFFFFF; /* ���邢�e�L�X�g�F */
        }

        QMenuBar::item:selected {
            background-color: #555555; /* �z�o�[���̔w�i�F */
        }

        QMenuBar::item:pressed {
            background-color: #222222; /* �N���b�N���̔w�i�F */
        }

        QMenu {
            background-color: #444444; /* �h���b�v�_�E�����j���[�̔w�i�F */
            color: #FFFFFF; /* �h���b�v�_�E�����j���[�̃e�L�X�g�F */
            border: 1px solid #666666; /* �h���b�v�_�E�����j���[�̃{�[�_�[ */
        }

        QMenu::item {
            padding: 5px 20px 5px 20px; /* �h���b�v�_�E�����j���[�A�C�e���̃p�f�B���O */
            color: #FFFFFF; /* �h���b�v�_�E�����j���[�A�C�e���̃e�L�X�g�F */
        }

        QMenu::item:selected {
            background-color: #666666; /* �h���b�v�_�E�����j���[�A�C�e���̃z�o�[���̔w�i�F */
        }

        QMenu::separator {
            height: 1px;
            background-color: #666666; /* �Z�p���[�^�̐F */
            margin-left: 10px;
            margin-right: 10px;
        }
    )";

  setStyleSheet(styleSheet);

 }

 ArtifactMenuBar::~ArtifactMenuBar()
 {
  delete impl;
 }


};