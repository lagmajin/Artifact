
module;
#include <QtCore/QtGlobal>
#include <QApplication>
#include <QMainWindow>
#include <QDateTime>
#include <QDir>
#include <QSettings>
#include <QImage>
#include <QTimer>
#include <QElapsedTimer>
#include <QMessageBox>
#include <QAbstractButton>
#include <QPushButton>
#include <QStandardPaths>
#include <QJsonDocument>
#include <QFileInfo>
#include <QFile>
#include <QCommandLineOption>
#include <QMetaType>
#include <QDesktopServices>
#include <QUrl>
#include <qthreadpool.h>
#include <QFileInfoList>
#include <ads_globals.h>
#include <memory>
#include <atomic>

#define _CRT_SECURE_NO_WARNINGS
#pragma warning(disable:4996)
#pragma push_macro("emit")
#pragma push_macro("event")
#undef emit
#include <tbb/tbb.h>
#pragma pop_macro("event")
#pragma pop_macro("emit")

#include <opencv2/opencv.hpp>

#include <windows.h>

//#include <pybind11/pybind11.h>

module Artifact.AppMain;



import Transform;
import Draw;
import Glow;

import ImageProcessing;


//import hostfxr;
//import HalideTest;
import Graphics;
import SearchImage;
import UI.Layout.State;
import Core.FastSettingsStore;

import Test;

import ImageProcessing.SpectralGlow;

import Codec.Thumbnail.FFmpeg;

import Widgets.Render.Queue;
import Widgets.Utils.CSS;
import IO.ImageExporter;
import ArtifactStatusBar;
import Artifact.PythonAPI;
import Script.Python.Engine;
import Artifact.Service.Playback;
import Artifact.Service.Project;
import Artifact.Widgets.UndoHistoryWidget;
import Artifact.Widgets.PythonHookManagerWidget;
import Artifact.Widgets.ProjectManagerWidget;
import Artifact.Widgets.CompositionAudioMixer;
import Artifact.Widgets.Timeline;
import Artifact.Widgets.CompositionEditor;
import Artifact.Widgets.RenderLayerEditor;
import Artifact.Widgets.Render.QueueManager;
import Widgets.Inspector;
import Artifact.Widgets.ArtifactPropertyWidget;
import Artifact.MainWindow;
import Artifact.Project.Manager;
import Artifact.Project.AutoSaveManager;
import Artifact.Script.Hooks;

using namespace Artifact;
using namespace ArtifactCore;

namespace
{
 quint64 processWorkingSetMB()
 {
#if defined(_WIN32)
  MEMORYSTATUSEX memStatus{};
  memStatus.dwLength = sizeof(memStatus);
  if (GlobalMemoryStatusEx(&memStatus))
  {
   const quint64 totalPhys = static_cast<quint64>(memStatus.ullTotalPhys);
   const quint64 availPhys = static_cast<quint64>(memStatus.ullAvailPhys);
   return (totalPhys - availPhys) / (1024ull * 1024ull);
  }
#endif
  return 0;
 }

 void bootstrapPythonScripts()
 {
  auto& py = PythonEngine::instance();
  if (!py.initialize())
  {
   return;
  }

  ArtifactPythonAPI::registerAll();

  const QString appDir = QCoreApplication::applicationDirPath();
  const QStringList scriptDirs = {
   QDir(appDir).filePath("scripts"),
   QDir(QDir::currentPath()).filePath("scripts")
  };

  for (const QString& dirPath : scriptDirs)
  {
   QDir dir(dirPath);
   if (!dir.exists())
   {
    continue;
   }

   py.addSearchPath(dir.absolutePath().toStdString());
   const QFileInfoList files = dir.entryInfoList(QStringList() << "*.py", QDir::Files, QDir::Name);
   for (const QFileInfo& fileInfo : files)
   {
    py.executeFile(fileInfo.absoluteFilePath().toStdString());
   }
  }
 }

 QByteArray currentProjectSnapshotJson()
 {
  auto project = ArtifactProjectManager::getInstance().getCurrentProjectSharedPtr();
  if (!project)
  {
   return {};
  }
  const QJsonDocument doc(project->toJson());
  return doc.toJson(QJsonDocument::Indented);
 }

 QString sessionStateFilePath()
 {
  const QString appDataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
  QDir dataDir(appDataDir);
  if (!dataDir.exists())
  {
   dataDir.mkpath(QStringLiteral("."));
  }
  return dataDir.filePath(QStringLiteral("session_state.cbor"));
 }

QString recoveryDirectoryPath()
{
  const QString appDataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
  QDir dataDir(appDataDir);
  if (!dataDir.exists())
  {
   dataDir.mkpath(QStringLiteral("."));
  }
  return dataDir.filePath(QStringLiteral("Recovery"));
}

void sanitizeSessionStateStore()
{
  ArtifactCore::FastSettingsStore sessionStore(sessionStateFilePath());
  const QVariant running = sessionStore.value(QStringLiteral("Session/isRunning"), false);
  if (running.isValid() && running.typeId() != QMetaType::Bool)
  {
    sessionStore.setValue(QStringLiteral("Session/isRunning"), false);
  }

  const QVariant pid = sessionStore.value(QStringLiteral("Session/pid"));
  if (pid.isValid() && !pid.canConvert<qlonglong>())
  {
    sessionStore.remove(QStringLiteral("Session/pid"));
  }

  const QVariant startTs = sessionStore.value(QStringLiteral("Session/startTimestamp"));
  if (startTs.isValid() && startTs.typeId() != QMetaType::QString)
  {
    sessionStore.remove(QStringLiteral("Session/startTimestamp"));
  }

  const QVariant cleanTs = sessionStore.value(QStringLiteral("Session/lastCleanExitTimestamp"));
  if (cleanTs.isValid() && cleanTs.typeId() != QMetaType::QString)
  {
    sessionStore.remove(QStringLiteral("Session/lastCleanExitTimestamp"));
  }

  const QVariant layoutAttempted = sessionStore.value(QStringLiteral("Session/layoutRestoreAttempted"));
  if (layoutAttempted.isValid() && layoutAttempted.typeId() != QMetaType::Bool)
  {
    sessionStore.remove(QStringLiteral("Session/layoutRestoreAttempted"));
  }

  const QVariant layoutGeomRestored = sessionStore.value(QStringLiteral("Session/layoutGeometryRestored"));
  if (layoutGeomRestored.isValid() && layoutGeomRestored.typeId() != QMetaType::Bool)
  {
    sessionStore.remove(QStringLiteral("Session/layoutGeometryRestored"));
  }

  const QVariant layoutStateRestored = sessionStore.value(QStringLiteral("Session/layoutStateRestored"));
  if (layoutStateRestored.isValid() && layoutStateRestored.typeId() != QMetaType::Bool)
  {
    sessionStore.remove(QStringLiteral("Session/layoutStateRestored"));
  }

  const QVariant layoutResetApplied = sessionStore.value(QStringLiteral("Session/layoutResetApplied"));
  if (layoutResetApplied.isValid() && layoutResetApplied.typeId() != QMetaType::Bool)
  {
    sessionStore.remove(QStringLiteral("Session/layoutResetApplied"));
  }

  const QVariant layoutRestoreTs = sessionStore.value(QStringLiteral("Session/layoutRestoreTimestamp"));
  if (layoutRestoreTs.isValid() && layoutRestoreTs.typeId() != QMetaType::QString)
  {
    sessionStore.remove(QStringLiteral("Session/layoutRestoreTimestamp"));
  }
  sessionStore.sync();
}

void recordLayoutRestoreResult(
  bool attempted,
  bool geometryRestored,
  bool stateRestored,
  bool resetApplied)
{
  ArtifactCore::FastSettingsStore sessionStore(sessionStateFilePath());
  sessionStore.setValue(QStringLiteral("Session/layoutRestoreAttempted"), attempted);
  sessionStore.setValue(QStringLiteral("Session/layoutGeometryRestored"), geometryRestored);
  sessionStore.setValue(QStringLiteral("Session/layoutStateRestored"), stateRestored);
  sessionStore.setValue(QStringLiteral("Session/layoutResetApplied"), resetApplied);
  sessionStore.setValue(QStringLiteral("Session/layoutRestoreTimestamp"), QDateTime::currentDateTime().toString(Qt::ISODate));
  sessionStore.sync();
}

void sanitizeLayoutStore(ArtifactCore::FastSettingsStore& layoutStore)
{
  const QVariant geometry = layoutStore.value(QStringLiteral("MainWindow/geometry"));
  if (geometry.isValid() && !geometry.canConvert<QByteArray>())
  {
    layoutStore.remove(QStringLiteral("MainWindow/geometry"));
  }

  const QVariant state = layoutStore.value(QStringLiteral("MainWindow/state"));
  if (state.isValid() && !state.canConvert<QByteArray>())
  {
    layoutStore.remove(QStringLiteral("MainWindow/state"));
  }

  const QVariant version = layoutStore.value(QStringLiteral("MainWindow/version"));
  if (version.isValid() && version.typeId() != QMetaType::QString)
  {
    layoutStore.remove(QStringLiteral("MainWindow/version"));
  }
  layoutStore.sync();
}

bool markSessionStartAndDetectUncleanExit()
{
  sanitizeSessionStateStore();
  ArtifactCore::FastSettingsStore sessionStore(sessionStateFilePath());
  const bool wasRunning = sessionStore.value(QStringLiteral("Session/isRunning"), false).toBool();
  sessionStore.setValue(QStringLiteral("Session/isRunning"), true);
  sessionStore.setValue(QStringLiteral("Session/startTimestamp"), QDateTime::currentDateTime().toString(Qt::ISODate));
  sessionStore.setValue(QStringLiteral("Session/pid"), static_cast<qlonglong>(QCoreApplication::applicationPid()));
  sessionStore.sync();
  return wasRunning;
 }

 void markSessionEndClean()
 {
  ArtifactCore::FastSettingsStore sessionStore(sessionStateFilePath());
  sessionStore.setValue(QStringLiteral("Session/isRunning"), false);
  sessionStore.setValue(QStringLiteral("Session/lastCleanExitTimestamp"), QDateTime::currentDateTime().toString(Qt::ISODate));
  sessionStore.sync();
 }

 void showUncleanExitNoticeIfNeeded(bool hadUncleanExit, QWidget* parent)
 {
  if (!hadUncleanExit)
  {
   return;
  }

  QMessageBox box(parent);
  box.setIcon(QMessageBox::Warning);
  box.setWindowTitle(QStringLiteral("Previous Session Did Not Exit Cleanly"));
  box.setText(QStringLiteral("前回セッションが正常終了していません。"));
  box.setInformativeText(QStringLiteral("復旧スナップショットの場所を開きますか？"));
  auto* openFolder = box.addButton(QStringLiteral("Open Recovery Folder"), QMessageBox::ActionRole);
  box.addButton(QStringLiteral("Continue"), QMessageBox::AcceptRole);
  box.exec();

  if (box.clickedButton() == openFolder)
  {
   const QString recoveryDir = recoveryDirectoryPath();
   QDir dir(recoveryDir);
   if (!dir.exists())
   {
    dir.mkpath(QStringLiteral("."));
   }
   QDesktopServices::openUrl(QUrl::fromLocalFile(recoveryDir));
  }
 }

 bool showRecoveryPrompt(ArtifactAutoSaveManager& autoSave, QWidget* parent)
 {
  if (!autoSave.hasRecoveryPoint())
  {
   return false;
  }

  QMessageBox box(parent);
  box.setIcon(QMessageBox::Warning);
  box.setWindowTitle("Recovery Snapshot Found");
  box.setText("A crash recovery snapshot was found.");
  box.setInformativeText("Recover the latest snapshot now?");
  auto* recover = box.addButton("Recover", QMessageBox::AcceptRole);
  box.addButton("Ignore", QMessageBox::RejectRole);
  box.exec();

  if (box.clickedButton() != recover)
  {
   return false;
  }

  QByteArray recoveredJson;
  QString sourcePath;
  if (!autoSave.loadLatestRecoveryPoint(&recoveredJson, &sourcePath) || recoveredJson.isEmpty())
  {
   return false;
  }

  const QString tempRoot = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
  QDir tempDir(tempRoot);
  tempDir.mkpath(".");
  const QString recoveredPath = tempDir.filePath("RecoveredProject.artifact.json");
  QFile out(recoveredPath);
  if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate))
  {
   return false;
  }
  out.write(recoveredJson);
  out.close();

  ArtifactProjectManager::getInstance().loadFromFile(recoveredPath);
  return true;
 }
}

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
     a.setStyleSheet(getDCCStyleSheetPreset(DccStylePreset::ModoStyle));
	 auto pool = QThreadPool::globalInstance();

	 pool->setMaxThreadCount(10);

  bootstrapPythonScripts();
  ArtifactPythonHookManager::runHook(QStringLiteral("on_startup"));
    const bool hadUncleanExit = markSessionStartAndDetectUncleanExit();
    auto* mw = new ArtifactMainWindow();
    mw->setObjectName("ArtifactMainWindow");
    mw->setWindowTitle("Artifact");
    auto* status = new ArtifactStatusBar(mw);
    mw->setStatusBar(status);
    status->showReadyMessage();
    status->setProjectText("Loaded");
    auto* compositionEditor = new ArtifactCompositionEditor(mw);
    mw->addDockedWidget(QStringLiteral("Composition Viewer"), ads::CenterDockWidgetArea, compositionEditor);
    auto* layerViewEditor = new ArtifactRenderLayerEditor(mw);
    mw->addDockedWidget(QStringLiteral("Layer View (Diligent)"), ads::CenterDockWidgetArea, layerViewEditor);
    mw->addDockedWidgetTabbed(QStringLiteral("Render Queue"), ads::BottomDockWidgetArea, new RenderQueueManagerWidget(mw), QStringLiteral("Timeline - "));
    mw->addDockedWidget(QStringLiteral("Project"), ads::LeftDockWidgetArea, new ArtifactProjectManagerWidget(mw));
    mw->addDockedWidget(QStringLiteral("Inspector"), ads::RightDockWidgetArea, new ArtifactInspectorWidget(mw));
    auto* propertyPanel = new ArtifactPropertyWidget(mw);
    mw->addDockedWidget(QStringLiteral("Properties"), ads::RightDockWidgetArea, propertyPanel);
    mw->addDockedWidget(QStringLiteral("Audio Mixer"), ads::RightDockWidgetArea, new ArtifactCompositionAudioMixerWidget(mw));
    mw->addDockedWidget(QStringLiteral("Undo History"), ads::RightDockWidgetArea, new ArtifactUndoHistoryWidget(mw));
    mw->addDockedWidget(QStringLiteral("Python Hooks"), ads::RightDockWidgetArea, new ArtifactPythonHookManagerWidget(mw));
    mw->setDockVisible(QStringLiteral("Undo History"), false);
    mw->setDockVisible(QStringLiteral("Python Hooks"), false);
    mw->setDockVisible(QStringLiteral("Layer View (Diligent)"), true);

    auto* projectService = ArtifactProjectService::instance();
    auto* playbackService = ArtifactPlaybackService::instance();
    auto* autoSaveManager = new ArtifactAutoSaveManager();
    const QString recoveryDir = QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)).filePath("Recovery");
    autoSaveManager->initialize("ArtifactProject", recoveryDir);
    autoSaveManager->start();
    showUncleanExitNoticeIfNeeded(hadUncleanExit, mw);
    showRecoveryPrompt(*autoSaveManager, mw);

    if (projectService) {
        QObject::connect(projectService, &ArtifactProjectService::projectChanged, mw, [status]() {
            status->setProjectText("Modified");
        });
        QObject::connect(projectService, &ArtifactProjectService::projectChanged, mw, []() {
            ArtifactPythonHookManager::runHook(QStringLiteral("project_changed"));
        });
        QObject::connect(projectService, &ArtifactProjectService::projectChanged, mw, [autoSaveManager]() {
            if (autoSaveManager) autoSaveManager->markDirty();
        });
        QObject::connect(projectService, &ArtifactProjectService::layerCreated, mw, [status](const CompositionID&, const LayerID&) {
            status->setProjectText("Layer Added");
        });
        QObject::connect(projectService, &ArtifactProjectService::layerCreated, mw, [](const CompositionID& compId, const LayerID& layerId) {
            ArtifactPythonHookManager::runHook(QStringLiteral("layer_added"), QStringList() << compId.toString() << layerId.toString());
        });
        QObject::connect(projectService, &ArtifactProjectService::layerCreated, mw, [autoSaveManager](const CompositionID&, const LayerID&) {
            if (autoSaveManager) autoSaveManager->markDirty();
        });
        QObject::connect(projectService, &ArtifactProjectService::layerSelected, mw, [mw, layerViewEditor, propertyPanel, projectService](const LayerID& layerId) {
            if (layerViewEditor) {
                if (layerId.isNil()) {
                    layerViewEditor->view()->clearTargetLayer();
                } else {
                    layerViewEditor->setTargetLayer(layerId);
                }
            }
            if (propertyPanel) {
                propertyPanel->setFocusedEffectId(QString());
                if (layerId.isNil()) {
                    propertyPanel->clear();
                } else if (auto comp = projectService->currentComposition().lock()) {
                    propertyPanel->setLayer(comp->layerById(layerId));
                } else {
                    propertyPanel->clear();
                }
            }
            mw->activateDock(QStringLiteral("Layer View (Diligent)"));
        });
        QObject::connect(projectService, &ArtifactProjectService::currentCompositionChanged, mw, [mw, compositionEditor, projectService](const CompositionID& compId) {
            if (!compositionEditor || !projectService) {
                return;
            }
            const auto found = projectService->findComposition(compId);
            if (found.success && !found.ptr.expired()) {
                compositionEditor->setComposition(found.ptr.lock());
                mw->activateDock(QStringLiteral("Composition Viewer"));
            }
        });
        QObject::connect(projectService, &ArtifactProjectService::currentCompositionChanged, mw, [layerViewEditor]() {
            if (layerViewEditor) {
                layerViewEditor->view()->clearTargetLayer();
            }
        });
        QObject::connect(projectService, &ArtifactProjectService::currentCompositionChanged, mw, [propertyPanel]() {
            if (propertyPanel) {
                propertyPanel->setFocusedEffectId(QString());
                propertyPanel->clear();
            }
        });
        QObject::connect(projectService, &ArtifactProjectService::projectCreated, mw, [propertyPanel, layerViewEditor, compositionEditor]() {
            if (propertyPanel) {
                propertyPanel->setFocusedEffectId(QString());
                propertyPanel->clear();
            }
            if (layerViewEditor) {
                layerViewEditor->view()->clearTargetLayer();
            }
            if (compositionEditor) {
                compositionEditor->setComposition(nullptr);
            }
        });
        QObject::connect(projectService, &ArtifactProjectService::layerRemoved, mw, [status](const CompositionID&, const LayerID&) {
            status->setProjectText("Layer Removed");
        });
        QObject::connect(projectService, &ArtifactProjectService::layerRemoved, mw, [](const CompositionID& compId, const LayerID& layerId) {
            ArtifactPythonHookManager::runHook(QStringLiteral("layer_removed"), QStringList() << compId.toString() << layerId.toString());
        });
        QObject::connect(projectService, &ArtifactProjectService::layerRemoved, mw, [autoSaveManager](const CompositionID&, const LayerID&) {
            if (autoSaveManager) autoSaveManager->markDirty();
        });
        QObject::connect(projectService, &ArtifactProjectService::compositionCreated, mw, [](const CompositionID& compId) {
            ArtifactPythonHookManager::runHook(QStringLiteral("composition_created"), QStringList() << compId.toString());
        });
        QObject::connect(projectService, &ArtifactProjectService::compositionCreated, mw, [mw](const CompositionID& compId) {
            QTimer::singleShot(0, mw, [mw, compId]() {
                auto* panel = new ArtifactTimelineWidget(mw);
                panel->setComposition(compId);
                mw->addDockedWidgetTabbed(
                    QStringLiteral("Timeline - %1").arg(compId.toString()),
                    ads::BottomDockWidgetArea,
                    panel,
                    QStringLiteral("Timeline - "));
            });
        });
        QObject::connect(projectService, &ArtifactProjectService::projectCreated, mw, []() {
            ArtifactPythonHookManager::runHook(QStringLiteral("project_opened"));
        });
    }

    if (projectService && compositionEditor) {
        if (auto current = projectService->currentComposition().lock()) {
            compositionEditor->setComposition(current);
        }
    }

    auto latestFrame = std::make_shared<std::atomic<long long>>(0);
    auto hasFrameUpdate = std::make_shared<std::atomic_bool>(false);
    auto frameCounter = std::make_shared<std::atomic<int>>(0);

    auto* uiTimer = new QTimer(mw);
    uiTimer->setInterval(33); // ~30Hz UI update
    QObject::connect(uiTimer, &QTimer::timeout, mw, [status, latestFrame, hasFrameUpdate]() {
        if (hasFrameUpdate->exchange(false)) {
            status->setFrame(latestFrame->load());
        }
    });
    uiTimer->start();

    auto* statsTimer = new QTimer(mw);
    statsTimer->setInterval(500);
    auto fpsElapsed = std::make_shared<QElapsedTimer>();
    fpsElapsed->start();
    QObject::connect(statsTimer, &QTimer::timeout, mw, [status, fpsElapsed, frameCounter]() {
        status->setMemoryMB(processWorkingSetMB());
        const qint64 elapsedMs = fpsElapsed->elapsed();
        if (elapsedMs > 0) {
            const int frames = frameCounter->exchange(0);
            const double fps = frames * 1000.0 / static_cast<double>(elapsedMs);
            status->setFPS(fps);
        }
        fpsElapsed->restart();
    });
    statsTimer->start();

    auto* recoveryTimer = new QTimer(mw);
    recoveryTimer->setInterval(120000);
    QObject::connect(recoveryTimer, &QTimer::timeout, mw, [autoSaveManager]() {
        if (!autoSaveManager || !autoSaveManager->isDirty()) {
            return;
        }
        const QByteArray snapshot = currentProjectSnapshotJson();
        if (!snapshot.isEmpty()) {
            autoSaveManager->createRecoveryPoint(snapshot);
        }
    });
    recoveryTimer->start();

    if (playbackService) {
        QObject::connect(playbackService, &ArtifactPlaybackService::frameChanged, mw,
            [latestFrame, hasFrameUpdate, frameCounter](const FramePosition& position) {
                latestFrame->store(position.framePosition());
                hasFrameUpdate->store(true);
                frameCounter->fetch_add(1);
            });
    }

    {
        const QString appDataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        QDir dataDir(appDataDir);
        if (!dataDir.exists()) {
            dataDir.mkpath(QStringLiteral("."));
        }
        ArtifactCore::FastSettingsStore layoutStore(dataDir.filePath(QStringLiteral("main_window_layout.cbor")));
        sanitizeLayoutStore(layoutStore);
        if (!layoutStore.contains(QStringLiteral("MainWindow/layoutKey")) &&
            !layoutStore.contains(QStringLiteral("MainWindow/geometry")) &&
            !layoutStore.contains(QStringLiteral("MainWindow/state"))) {
            // Backward compatibility: import once from legacy QSettings.
            QSettings legacy(QStringLiteral("ArtifactStudio"), QStringLiteral("Artifact"));
            auto legacyState = UiLayoutState::loadFromSettings(legacy, QStringLiteral("MainWindow"));
            if (!legacyState.isEmpty()) {
                legacyState.saveToStore(layoutStore, QStringLiteral("MainWindow"));
                layoutStore.sync();
            }
        }
        auto layoutState = UiLayoutState::loadFromStore(layoutStore, "MainWindow");
        bool geometryRestored = true;
        bool stateRestored = true;
        const bool hasGeometry = !layoutState.geometry.isEmpty();
        const bool hasState = !layoutState.state.isEmpty();
        if (!layoutState.geometry.isEmpty()) {
            geometryRestored = mw->restoreGeometry(layoutState.geometry);
        }
        if (!layoutState.state.isEmpty()) {
            stateRestored = mw->restoreState(layoutState.state);
        }
        bool resetApplied = false;
        if ((!layoutState.geometry.isEmpty() && !geometryRestored) ||
            (!layoutState.state.isEmpty() && !stateRestored)) {
            // Saved layout is likely incompatible with current dock/widget set.
            layoutStore.remove("MainWindow/layoutKey");
            layoutStore.remove("MainWindow/version");
            layoutStore.remove("MainWindow/geometry");
            layoutStore.remove("MainWindow/state");
            layoutStore.sync();
            mw->resize(1600, 900);
            resetApplied = true;
        }
        recordLayoutRestoreResult(hasGeometry || hasState, geometryRestored, stateRestored, resetApplied);
    }
    mw->setDockVisible(QStringLiteral("Layer View (Diligent)"), true);

    QObject::connect(&a, &QCoreApplication::aboutToQuit, [mw]() {
        const QString appDataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        QDir dataDir(appDataDir);
        if (!dataDir.exists()) {
            dataDir.mkpath(QStringLiteral("."));
        }
        ArtifactCore::FastSettingsStore layoutStore(dataDir.filePath(QStringLiteral("main_window_layout.cbor")));
        UiLayoutState layoutState("ArtifactMainWindow");
        layoutState.geometry = mw->saveGeometry();
        layoutState.state = mw->saveState();
        layoutState.saveToStore(layoutStore, "MainWindow");
        layoutStore.sync();
    });
    QObject::connect(&a, &QCoreApplication::aboutToQuit, mw, &QObject::deleteLater);
    QObject::connect(&a, &QCoreApplication::aboutToQuit, [&]() {
        if (autoSaveManager) {
            if (autoSaveManager->isDirty()) {
                const QByteArray snapshot = currentProjectSnapshotJson();
                if (!snapshot.isEmpty()) {
                    autoSaveManager->createRecoveryPoint(snapshot);
                }
            }
            autoSaveManager->stop();
            delete autoSaveManager;
        }
        markSessionEndClean();
    });

    mw->show();
    return a.exec();

}
