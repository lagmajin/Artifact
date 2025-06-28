#include <opencv2/opencv.hpp>

#include <QtCore/QtGlobal>

#include <QApplication>

#include <windows.h>







import Transform;
import Draw;
import Glow;

import ImageProcessing;

import ArtifactMainWindow;

import hostfxr;
import HalideTest;
import Graphics;

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
 



 float threshold = 0.5f;   // ����臒l��薾�邢�s�N�Z�����O���[�̌��ɂȂ�
 int vertical_blur_radius = 80; // �c�����ڂ����̔��a (�傫���قǏc�ɐL�т�)
 float intensity = 1.1f;   // �O���[�̋��x

 cv::Mat glowed_image = applyVerticalGlow(mat, threshold, vertical_blur_radius, intensity);
 SetEnvironmentVariableW(L"COREHOST_TRACE", L"1");
 SetEnvironmentVariableW(L"COREHOST_TRACEFILE", L"C:\\temp\\hostfxr_trace.log");

 DotnetRuntimeHost host;

 // �@ .NET SDK�̃��[�g or dotnet.exe�̃��[�g���w��i��: "C:/Program Files/dotnet"�j
 if (!host.initialize("C:/Program Files/dotnet")) {
  qWarning() << "hostfxr ���������s";
  return;
 }

 // �A �A�Z���u���p�X���w��i.dll�j�� �����ł� MyApp.dll ������
 QString dllPath = "C:/Users/lagma/Desktop/Artifact/Artifact/App/Debug/net9.0/ArtifactScriptRunner.dll";

 // �Ăяo����̌^���ƃ��\�b�h���iC#���ƈ�v������j
 if (!host.loadAssembly(dllPath))
 {
  qWarning() << "�A�Z���u���̃��[�h���s";
  return;
 }

 void* method = nullptr;
 if (!(method = host.getFunctionPointer("ArtifactScriptRunner.ArtifactScriptRunner,ArtifactScriptRunner", "Add"))) {
  //std::cerr << hostfxr.getError() << std::endl;
  //return 1;
 }

  //���\�b�h���Ăяo���B
 int result = reinterpret_cast<int(*)(int, int)>(method)(1, 2);
 std::cout << result << std::endl;

 /*
 int test_width = 100;
 int test_height = 50;
 cv::Mat input_test_mat(test_height, test_width, CV_32FC4);

 // �T���v���f�[�^��Mat�𖄂߂� (BGRA��)
 // �Ⴆ�΁A����͐A�E��͐ԁA�����͗΁A�E���͔�
 for (int y = 0; y < test_height; ++y) {
  for (int x = 0; x < test_width; ++x) {
   // OpenCV Vec4f: [Blue, Green, Red, Alpha]
   float B = (float)x / test_width;         // x���i�ނɂ�Đ������Ȃ�
   float G = (float)y / test_height;        // y���i�ނɂ�ė΂������Ȃ�
   float R = 1.0f - (float)x / test_width;  // x���i�ނɂ�ĐԂ��キ�Ȃ�
   float A = 1.0f;                          // �A���t�@�͏��1.0 (�s����)

   input_test_mat.at<cv::Vec4f>(y, x) = cv::Vec4f(B, G, R, A);
  }
 }

 cv::Mat output_result_mat = process_bgra_mat_with_halide_gpu(input_test_mat);
 */

 auto context=new GpuContext();

 context->Initialize();

 auto negateCS = new NegateCS(context->D3D12RenderDevice());




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