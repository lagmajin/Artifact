#include <opencv2/opencv.hpp>

#include <QtCore/QtGlobal>

#include <QtWidgets/QApplication>







import Transform;
import Draw;
import Glow;

import ImageProcessing;

import ArtifactMainWindow;

using namespace Artifact;
using namespace ArtifactCore;

void test()
{
 cv::Mat mat(400, 640, CV_8UC3, cv::Scalar(0, 0, 0));
 cv::Mat mask(400, 640, CV_8UC3, cv::Scalar(0, 0, 0));
 cv::Mat dst(400, 640, CV_8UC3, cv::Scalar(0, 0, 0));
 drawStar5(mat,
  cv::Scalar(0, 255, 255),  // edgeColor�i���j
  2,                        // edgeThickness
  cv::Scalar(-1, -1, -1),   // fillColor�i���������j
  0.8f);
 

 //cv::Mat mat2(400, 640, CV_32FC4, cv::Scalar(0, 0, 0));

 //auto test = halideTestMinimal2(mat2);

 //cv::Mat canvas = cv::Mat::zeros(400, 600, CV_8UC3); // ��600�A����400�̍����摜

 //unsigned int random_seed_line1 = 123;
 //unsigned int random_seed_line2 = 456;
 //long total_points_attempt = 1000; // ���s�񐔂𑝂₵�Ė��x���グ��

 // �����Ƀ����_���ȓ_�Q�̏c���C��
 //drawRandomDottedVerticalLine(canvas, 150, 40, total_points_attempt, random_seed_line1, cv::Scalar(0, 0, 255), 1);



 float threshold = 0.5f;   // ����臒l��薾�邢�s�N�Z�����O���[�̌��ɂȂ�
 int vertical_blur_radius = 80; // �c�����ڂ����̔��a (�傫���قǏc�ɐL�т�)
 float intensity = 1.1f;   // �O���[�̋��x

 cv::Mat glowed_image = applyVerticalGlow(mat, threshold, vertical_blur_radius, intensity);


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