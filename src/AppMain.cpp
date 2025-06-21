#include <opencv2/opencv.hpp>

#include <QtCore/QtGlobal>

#include <QtWidgets/QApplication>

#include <windows.h>







import Transform;
import Draw;
import Glow;

import ImageProcessing;

import ArtifactMainWindow;

import hostfxr;

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
 

 //cv::Mat mat2(400, 640, CV_32FC4, cv::Scalar(0, 0, 0));

 //auto test = halideTestMinimal2(mat2);

 //cv::Mat canvas = cv::Mat::zeros(400, 600, CV_8UC3); // 幅600、高さ400の黒い画像

 //unsigned int random_seed_line1 = 123;
 //unsigned int random_seed_line2 = 456;
 //long total_points_attempt = 1000; // 試行回数を増やして密度を上げる

 // 左側にランダムな点群の縦ライン
 //drawRandomDottedVerticalLine(canvas, 150, 40, total_points_attempt, random_seed_line1, cv::Scalar(0, 0, 255), 1);



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