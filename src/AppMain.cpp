#include <QtWidgets/QApplication>
#include "../include/Widgets/ArtifactMainWindow.hpp"

using namespace Artifact;

int main(int argc, char* argv[])
{
 QApplication a(argc, argv);

 ArtifactMainWindow mw;
 mw.show();
 return a.exec();

}