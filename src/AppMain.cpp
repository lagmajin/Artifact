
#pragma push_macro("emit")
#pragma push_macro("event")
#undef emit
#include <tbb/tbb.h>
#pragma pop_macro("event")
#pragma pop_macro("emit")

#include <opencv2/opencv.hpp>

#include <QtCore/QtGlobal>
#include <QApplication>

#include <windows.h>
#include <QDateTime>
#include <QDir>
#include <QImage>

#include <QCommandLineOption>
#include <qthreadpool.h>
//#include <pybind11/pybind11.h>



import Transform;
import Draw;
import Glow;

import ImageProcessing;

import Artifact.MainWindow;

//import hostfxr;
//import HalideTest;
import Graphics;
import SearchImage;

import Test;

import ImageProcessing.SpectralGlow;

import Codec.Thumbnail.FFmpeg;

import Widgets.Render.Queue;
import IO.ImageExporter;

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


 /*
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


	*/
  //メソッドを呼び出す。
 //int result = reinterpret_cast<int(*)(int, int)>(method)(1, 2);
 //std::cout << result << std::endl;

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
 */cv::Size img_size(800, 600);
 // 背景色: 0.0-1.0の範囲でグレー (RGBA)
 /*
 cv::Scalar bg_color(0.2, 0.2, 0.2, 1.0);

 cv::Point center(400, 300);
 int size = 200;
 int radius = 40; // 角の丸め半径

  cv::Mat pentagon_no_fill = drawFilledRoundedPentagon(img_size, bg_color, center, size, radius,
  cv::Scalar(0.0, 0.0, 0.0, 0.0), // 完全に透明
  cv::Scalar(0.0, 1.0, 0.0, 1.0), // 不透明な緑
  2);

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
 */
 //auto testImage = findAndLoadImageInAppDir("test.jpg", CV_32FC4);

 //auto context=new GpuContext();

 //context->Initialize();

 //auto negateCS = new NegateCS(context->D3D12RenderDevice(),context->D3D12DeviceContext());

 //negateCS->loadShaderBinaryFromDirectory(QCoreApplication::applicationDirPath(), "Negate.cso");

 //negateCS->Process(testImage);

 //SpectralGlow glow;

 //glow.ElegantGlow(testImage);


 int width = 600;
 int height = 400;




 // 作成する単色の色 (例: 青)
 // QColor::blue() の代わりに QColor(255, 0, 0) (赤), QColor(0, 255, 0) (緑) なども指定できます。
 //QColor singleColor = QColor(0, 0, 255); // RGB (赤, 緑, 青) の値で青色を設定

 // QImageオブジェクトを作成
 // Format_RGB32 は、各ピクセルが32ビット（RGBA）で表現される形式です。
 // この形式は、多くのシステムで効率的に扱われます。
 //QImage image(width, height, QImage::Format_RGB32);

 // 画像を単色で塗りつぶす
 // fill() メソッドは、指定された色で画像全体を塗りつぶします。
 //image.fill(singleColor);

 //auto file= findFirstFileByLooseExtensionFromAppDir("mov");

 //FFmpegThumbnailExtractor extractor;

 //auto image=extractor.extractThumbnail(file);

 //QString exePath = QCoreApplication::applicationDirPath();

 // 適当なファイル名（例：thumb_1234.png）
 //QString fileName = "thumb_" + QString::number(QDateTime::currentMSecsSinceEpoch()) + ".png";

 // フルパスを作成
 //QString fullPath = QDir(exePath).filePath(fileName);



}

int main(int argc, char* argv[])
{
 tbb::global_control c(tbb::global_control::max_allowed_parallelism, std::thread::hardware_concurrency());

 AddDllDirectory(L"C:\\Users\\lagma\\Desktop\\Artifact\\Artifact\\App");
 SetDefaultDllDirectories(LOAD_LIBRARY_SEARCH_DEFAULT_DIRS | LOAD_LIBRARY_SEARCH_USER_DIRS);
 //qsetenv("QT_QPA_PLATFORM", "windows:darkmode=[1]");

 //QTextCodec::setCodecForLocale(QTextCodec::codecForName("Shift-JIS"));
 bool renderMode = false;
	

	if (renderMode)
	 {
		
		
	 }else
	 {
		 
	 }
	
	 QApplication a(argc, argv);
	 auto pool = QThreadPool::globalInstance();

	 pool->setMaxThreadCount(10);

	
 test();
 ImageExporter exp;
 exp.testWrite();
	ArtifactMainWindow mw;
 mw.show();
 return a.exec();

}