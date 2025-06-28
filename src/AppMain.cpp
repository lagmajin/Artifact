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
  cv::Scalar(0, 255, 255),  // edgeColor（黄）
  2,                        // edgeThickness
  cv::Scalar(-1, -1, -1),   // fillColor（無視される）
  0.8f);
 



 float threshold = 0.5f;   // この閾値より明るいピクセルがグローの元になる
 int vertical_blur_radius = 80; // 縦方向ぼかしの半径 (大きいほど縦に伸びる)
 float intensity = 1.1f;   // グローの強度

 cv::Mat glowed_image = applyVerticalGlow(mat, threshold, vertical_blur_radius, intensity);
 SetEnvironmentVariableW(L"COREHOST_TRACE", L"1");
 SetEnvironmentVariableW(L"COREHOST_TRACEFILE", L"C:\\temp\\hostfxr_trace.log");

 DotnetRuntimeHost host;

 // ① .NET SDKのルート or dotnet.exeのルートを指定（例: "C:/Program Files/dotnet"）
 if (!host.initialize("C:/Program Files/dotnet")) {
  qWarning() << "hostfxr 初期化失敗";
  return;
 }

 // ② アセンブリパスを指定（.dll）→ ここでは MyApp.dll を仮定
 QString dllPath = "C:/Users/lagma/Desktop/Artifact/Artifact/App/Debug/net9.0/ArtifactScriptRunner.dll";

 // 呼び出し先の型名とメソッド名（C#側と一致させる）
 if (!host.loadAssembly(dllPath))
 {
  qWarning() << "アセンブリのロード失敗";
  return;
 }

 void* method = nullptr;
 if (!(method = host.getFunctionPointer("ArtifactScriptRunner.ArtifactScriptRunner,ArtifactScriptRunner", "Add"))) {
  //std::cerr << hostfxr.getError() << std::endl;
  //return 1;
 }

  //メソッドを呼び出す。
 int result = reinterpret_cast<int(*)(int, int)>(method)(1, 2);
 std::cout << result << std::endl;

 /*
 int test_width = 100;
 int test_height = 50;
 cv::Mat input_test_mat(test_height, test_width, CV_32FC4);

 // サンプルデータでMatを埋める (BGRA順)
 // 例えば、左上は青、右上は赤、左下は緑、右下は白
 for (int y = 0; y < test_height; ++y) {
  for (int x = 0; x < test_width; ++x) {
   // OpenCV Vec4f: [Blue, Green, Red, Alpha]
   float B = (float)x / test_width;         // xが進むにつれて青が強くなる
   float G = (float)y / test_height;        // yが進むにつれて緑が強くなる
   float R = 1.0f - (float)x / test_width;  // xが進むにつれて赤が弱くなる
   float A = 1.0f;                          // アルファは常に1.0 (不透明)

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