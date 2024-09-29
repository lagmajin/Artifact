#include <QtCore/QtGlobal>

#include <QtWidgets/QApplication>
#include "../include/Widgets/ArtifactMainWindow.hpp"



using namespace Artifact;

int main(int argc, char* argv[])
{
 //qsetenv("QT_QPA_PLATFORM", "windows:darkmode=[1]");

 

 QApplication a(argc, argv);

 ArtifactMainWindow mw;
 mw.show();
 return a.exec();

}