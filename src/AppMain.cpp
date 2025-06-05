#include <opencv2/opencv.hpp>

#include <QtCore/QtGlobal>

#include <QtWidgets/QApplication>
#include "../include/Widgets/ArtifactMainWindow.hpp"


import Scale2D;
import Draw;
import Glow;

using namespace Artifact;
using namespace ArtifactCore;

void test()
{
 cv::Mat mat(400, 640, CV_8UC3, cv::Scalar(0, 0, 0));
 cv::Mat mask(400, 640, CV_8UC3, cv::Scalar(0, 0, 0));
 cv::Mat dst(400, 640, CV_8UC3, cv::Scalar(0, 0, 0));
 drawStar5(mat,
  cv::Scalar(0, 255, 255),  // edgeColor（黄）
  2,                        // edgeThickness
  cv::Scalar(-1, -1, -1),   // fillColor（無視される）
  0.8f);
 
 applySimpleGlow(mat, cv::Mat(), dst, cv::Scalar(200, 200, 255), 1.6, 4, 5.0f, 1.8f, 0.3f, 0.6f, true, true);



}

int main(int argc, char* argv[])
{
 //qsetenv("QT_QPA_PLATFORM", "windows:darkmode=[1]");

 test();

 QApplication a(argc, argv);

 ArtifactMainWindow mw;
 mw.show();
 return a.exec();

}