
module;

#define _CRT_SECURE_NO_WARNINGS
#pragma warning(disable : 4996)
#pragma push_macro("emit")
#pragma push_macro("event")
#undef emit
#include <tbb/tbb.h>
#pragma pop_macro("event")
#pragma pop_macro("emit")

#include <clocale>
#include <cstdio>
#include <fcntl.h>
#include <io.h>
#include <iostream>
#include <windows.h>

// #include <pybind11/pybind11.h>
#include <QAbstractButton>
#include <QAbstractScrollArea>
#include <QApplication>
#include <QColor>
#include <QCommandLineOption>
#include <QDateTime>
#include <QDebug>
#include <QDesktopServices>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QFileInfoList>
#include <QFont>
#include <QIcon>
#include <QImage>
#include <QImageReader>
#include <QJsonDocument>
#include <QLoggingCategory>
#include <QMainWindow>
#include <QMessageBox>
#include <QMetaType>
#include <QPainter>
#include <QPixmap>
#include <QPushButton>
#include <QRectF>
#include <QSettings>
#include <QSortFilterProxyModel>
#include <QStandardPaths>
#include <QStyleFactory>
#include <QTimer>
#include <QUrl>
#include <QtCore/QtGlobal>
#include <ads_globals.h>
#include <filesystem>
#include <qthreadpool.h>

#include <QByteArray>
#include <opencv2/opencv.hpp>
#include <string>
module Artifact.AppMain;

import std;
import Core.AI.Context;
import Core.AI.McpBridge;

import Application.AppSettings;
import Artifact.Widgets.PlaybackControlWidget;
import Transform;
import Draw;
import Glow;

import ImageProcessing;

// import hostfxr;
// import HalideTest;
import Graphics;
import SearchImage;
import UI.Layout.State;
import Core.FastSettingsStore;
import Artifact.AI.WorkspaceAutomation;

import Artifact.TestRunner;

import ImageProcessing.SpectralGlow;

import Codec.Thumbnail.FFmpeg;
import AI.Client;

import Widgets.Render.Queue;
import Widgets.Utils.CSS;
import Widgets.CommonStyle;
import IO.ImageExporter;
import ArtifactStatusBar;
import Artifact.Application.Manager;
import Artifact.PythonAPI;
import Script.Python.Engine;
import Diagnostics.CrashHandler;
import Translation.Manager;
import Artifact.Diagnostics.AppValidationRules;
import Artifact.Layers.Selection.Manager;
import Artifact.Service.Playback;
import Artifact.Service.Project;
import Artifact.Project.Roles;
import EnvironmentVariable;
import Core.Localization;
import Artifact.Widgets.UndoHistoryWidget;
import Artifact.Widgets.PythonHookManagerWidget;
import Artifact.Widgets.ProjectManagerWidget;
import Artifact.Widgets.CompositionAudioMixer;
import Artifact.Widgets.Timeline;
import Artifact.Widgets.AI.ArtifactAICloudWidget;
import Artifact.Widgets.CompositionEditor;
import Artifact.Widgets.RenderLayerEditor;
import Artifact.Widgets.SoftwareRenderInspectors;
import Artifact.Widgets.MarkdownNoteEditorWidget;
import Artifact.Widgets.Render.QueueManager;
import Artifact.Widgets.RenderCenterWindow;
import Artifact.Contents.Viewer;
import Widgets.Inspector;
import Widgets.AssetBrowser;
import Artifact.Widgets.ArtifactPropertyWidget;
import Artifact.MainWindow;
import Artifact.Project.Manager;
import Artifact.Project.AutoSaveManager;
import Artifact.Script.Hooks;
import Artifact.Widgets.Test.ScrollPoC;

import Diagnostics.Logger;
import Artifact.Widgets.DebugConsoleWidget;
import Widgets.AIChatWidget;
import Event.Bus;
import Artifact.Event.Types;

using namespace Artifact;
using namespace ArtifactCore;

namespace {
constexpr int kMainWindowLayoutVersion = 8;

void suppressScrollBarsForViewportWidget(QWidget *widget) {
  if (!widget) {
    return;
  }

  const auto apply = [widget]() {
    QWidget *cursor = widget->parentWidget();
    while (cursor) {
      if (auto *scrollArea = qobject_cast<QAbstractScrollArea *>(cursor)) {
        scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
      }
      cursor = cursor->parentWidget();
    }
  };

  apply();
  QTimer::singleShot(0, widget, apply);
}

quint64 processWorkingSetMB() {
#if defined(_WIN32)
  MEMORYSTATUSEX memStatus{};
  memStatus.dwLength = sizeof(memStatus);
  if (GlobalMemoryStatusEx(&memStatus)) {
    const quint64 totalPhys = static_cast<quint64>(memStatus.ullTotalPhys);
    const quint64 availPhys = static_cast<quint64>(memStatus.ullAvailPhys);
    return (totalPhys - availPhys) / (1024ull * 1024ull);
  }
#endif
  return 0;
}

void bootstrapPythonScripts() {
  auto &py = PythonEngine::instance();
  if (!py.initialize()) {
    return;
  }

  ArtifactPythonAPI::registerAll();

  const QString appDir = QCoreApplication::applicationDirPath();
  const QStringList scriptDirs = {
      QDir(appDir).filePath("scripts"),
      QDir(QDir::currentPath()).filePath("scripts")};

  for (const QString &dirPath : scriptDirs) {
    QDir dir(dirPath);
    if (!dir.exists()) {
      continue;
    }

    py.addSearchPath(dir.absolutePath().toStdString());
    const QFileInfoList files =
        dir.entryInfoList(QStringList() << "*.py", QDir::Files, QDir::Name);
    for (const QFileInfo &fileInfo : files) {
      py.executeFile(fileInfo.absoluteFilePath().toStdString());
    }
  }
}

void configureQtPluginPaths() {
  const QString appDir = QCoreApplication::applicationDirPath();
  const QStringList candidates = {
      appDir,
      QDir(appDir).filePath(QStringLiteral("plugins")),
      QDir(appDir).filePath(
          QStringLiteral("../vcpkg_installed/x64-windows/Qt6/plugins")),
      QDir(appDir).filePath(
          QStringLiteral("../vcpkg_installed/x64-windows/debug/Qt6/plugins")),
      QDir(appDir).filePath(
          QStringLiteral("../vcpkg_installed/x64-windows/Qt6/plugins")),
      QDir(appDir).filePath(
          QStringLiteral("../vcpkg_installed/x64-windows/debug/Qt6/plugins"))};

  for (const QString &path : candidates) {
    if (QDir(path).exists()) {
      QCoreApplication::addLibraryPath(path);
    }
  }

  qDebug() << "[QtPluginPaths] libraryPaths="
           << QCoreApplication::libraryPaths();
  qDebug() << "[QtPluginPaths] supportedImageFormats="
           << QImageReader::supportedImageFormats();
}

QByteArray currentProjectSnapshotJson() {
  auto project =
      ArtifactProjectManager::getInstance().getCurrentProjectSharedPtr();
  if (!project) {
    return {};
  }
  const QJsonDocument doc(project->toJson());
  return doc.toJson(QJsonDocument::Indented);
}

QString sessionStateFilePath() {
  const QString appDataDir =
      QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
  QDir dataDir(appDataDir);
  if (!dataDir.exists()) {
    dataDir.mkpath(QStringLiteral("."));
  }
  return dataDir.filePath(QStringLiteral("session_state.cbor"));
}

QString recoveryDirectoryPath() {
  const QString appDataDir =
      QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
  QDir dataDir(appDataDir);
  if (!dataDir.exists()) {
    dataDir.mkpath(QStringLiteral("."));
  }
  return dataDir.filePath(QStringLiteral("Recovery"));
}

bool isStartupDialogSuppressed() {
  ArtifactCore::FastSettingsStore sessionStore(sessionStateFilePath());
  const QString suppressUntilIso =
      sessionStore
          .value(QStringLiteral("Session/startupDialogSuppressUntil"),
                 QString())
          .toString();
  if (suppressUntilIso.isEmpty()) {
    return false;
  }

  const QDateTime suppressUntil =
      QDateTime::fromString(suppressUntilIso, Qt::ISODate);
  if (!suppressUntil.isValid()) {
    return false;
  }
  return QDateTime::currentDateTime() < suppressUntil;
}

void suppressStartupDialogForDays(int days) {
  ArtifactCore::FastSettingsStore sessionStore(sessionStateFilePath());
  const QDateTime suppressUntil =
      QDateTime::currentDateTime().addDays(std::max(1, days));
  sessionStore.setValue(QStringLiteral("Session/startupDialogSuppressUntil"),
                        suppressUntil.toString(Qt::ISODate));
  sessionStore.sync();
}

void sanitizeSessionStateStore() {
  ArtifactCore::FastSettingsStore sessionStore(sessionStateFilePath());
  const QVariant running =
      sessionStore.value(QStringLiteral("Session/isRunning"), false);
  if (running.isValid() && running.typeId() != QMetaType::Bool) {
    sessionStore.setValue(QStringLiteral("Session/isRunning"), false);
  }

  const QVariant pid = sessionStore.value(QStringLiteral("Session/pid"));
  if (pid.isValid() && !pid.canConvert<qlonglong>()) {
    sessionStore.remove(QStringLiteral("Session/pid"));
  }

  const QVariant startTs =
      sessionStore.value(QStringLiteral("Session/startTimestamp"));
  if (startTs.isValid() && startTs.typeId() != QMetaType::QString) {
    sessionStore.remove(QStringLiteral("Session/startTimestamp"));
  }

  const QVariant cleanTs =
      sessionStore.value(QStringLiteral("Session/lastCleanExitTimestamp"));
  if (cleanTs.isValid() && cleanTs.typeId() != QMetaType::QString) {
    sessionStore.remove(QStringLiteral("Session/lastCleanExitTimestamp"));
  }

  const QVariant layoutAttempted =
      sessionStore.value(QStringLiteral("Session/layoutRestoreAttempted"));
  if (layoutAttempted.isValid() &&
      layoutAttempted.typeId() != QMetaType::Bool) {
    sessionStore.remove(QStringLiteral("Session/layoutRestoreAttempted"));
  }

  const QVariant layoutGeomRestored =
      sessionStore.value(QStringLiteral("Session/layoutGeometryRestored"));
  if (layoutGeomRestored.isValid() &&
      layoutGeomRestored.typeId() != QMetaType::Bool) {
    sessionStore.remove(QStringLiteral("Session/layoutGeometryRestored"));
  }

  const QVariant layoutStateRestored =
      sessionStore.value(QStringLiteral("Session/layoutStateRestored"));
  if (layoutStateRestored.isValid() &&
      layoutStateRestored.typeId() != QMetaType::Bool) {
    sessionStore.remove(QStringLiteral("Session/layoutStateRestored"));
  }

  const QVariant layoutResetApplied =
      sessionStore.value(QStringLiteral("Session/layoutResetApplied"));
  if (layoutResetApplied.isValid() &&
      layoutResetApplied.typeId() != QMetaType::Bool) {
    sessionStore.remove(QStringLiteral("Session/layoutResetApplied"));
  }

  const QVariant layoutRestoreTs =
      sessionStore.value(QStringLiteral("Session/layoutRestoreTimestamp"));
  if (layoutRestoreTs.isValid() &&
      layoutRestoreTs.typeId() != QMetaType::QString) {
    sessionStore.remove(QStringLiteral("Session/layoutRestoreTimestamp"));
  }

  const QVariant startupDialogSuppressUntil =
      sessionStore.value(QStringLiteral("Session/startupDialogSuppressUntil"));
  if (startupDialogSuppressUntil.isValid() &&
      startupDialogSuppressUntil.typeId() != QMetaType::QString) {
    sessionStore.remove(QStringLiteral("Session/startupDialogSuppressUntil"));
  }
  sessionStore.sync();
}

void recordLayoutRestoreResult(bool attempted, bool geometryRestored,
                               bool stateRestored, bool resetApplied) {
  ArtifactCore::FastSettingsStore sessionStore(sessionStateFilePath());
  sessionStore.setValue(QStringLiteral("Session/layoutRestoreAttempted"),
                        attempted);
  sessionStore.setValue(QStringLiteral("Session/layoutGeometryRestored"),
                        geometryRestored);
  sessionStore.setValue(QStringLiteral("Session/layoutStateRestored"),
                        stateRestored);
  sessionStore.setValue(QStringLiteral("Session/layoutResetApplied"),
                        resetApplied);
  sessionStore.setValue(QStringLiteral("Session/layoutRestoreTimestamp"),
                        QDateTime::currentDateTime().toString(Qt::ISODate));
  sessionStore.sync();
}

void sanitizeLayoutStore(ArtifactCore::FastSettingsStore &layoutStore) {
  const QVariant geometry =
      layoutStore.value(QStringLiteral("MainWindow/geometry"));
  if (geometry.isValid() && !geometry.canConvert<QByteArray>()) {
    layoutStore.remove(QStringLiteral("MainWindow/geometry"));
  }

  const QVariant state = layoutStore.value(QStringLiteral("MainWindow/state"));
  if (state.isValid() && !state.canConvert<QByteArray>()) {
    layoutStore.remove(QStringLiteral("MainWindow/state"));
  }

  const QVariant version =
      layoutStore.value(QStringLiteral("MainWindow/version"));
  if (version.isValid() && version.typeId() != QMetaType::QString) {
    layoutStore.remove(QStringLiteral("MainWindow/version"));
  }
  layoutStore.sync();
}

QString buildWindowTitle() {
  QString title = QStringLiteral("Artifact %1")
                      .arg(QStringLiteral(ARTIFACT_VERSION_STRING));

  const QString buildHash = QStringLiteral(ARTIFACT_BUILD_GIT_HASH);
  const QString buildStamp = QStringLiteral(ARTIFACT_BUILD_TIMESTAMP);
  const QString buildDirty = QStringLiteral(ARTIFACT_BUILD_DIRTY);

  title += QStringLiteral(" | ");
  title += buildHash;
  if (buildDirty == QStringLiteral("dirty")) {
    title += QStringLiteral("*");
  }
  title += QStringLiteral(" | ");
  title += buildStamp;
  return title;
}

QIcon buildTemporaryAppIcon() {
  QPixmap pix(256, 256);
  pix.fill(QColor(28, 28, 32));

  QPainter painter(&pix);
  painter.setRenderHint(QPainter::Antialiasing, true);
  painter.setPen(Qt::NoPen);
  painter.setBrush(QColor(212, 125, 50));
  painter.drawRoundedRect(QRectF(28.0, 28.0, 200.0, 200.0), 44.0, 44.0);

  QFont font;
  font.setBold(true);
  font.setPointSize(120);
  painter.setFont(font);
  painter.setPen(Qt::white);
  painter.drawText(pix.rect(), Qt::AlignCenter, QStringLiteral("A"));

  return QIcon(pix);
}

bool markSessionStartAndDetectUncleanExit() {
  sanitizeSessionStateStore();
  ArtifactCore::FastSettingsStore sessionStore(sessionStateFilePath());
  const bool wasRunning =
      sessionStore.value(QStringLiteral("Session/isRunning"), false).toBool();
  sessionStore.setValue(QStringLiteral("Session/isRunning"), true);
  sessionStore.setValue(QStringLiteral("Session/startTimestamp"),
                        QDateTime::currentDateTime().toString(Qt::ISODate));
  sessionStore.setValue(
      QStringLiteral("Session/pid"),
      static_cast<qlonglong>(QCoreApplication::applicationPid()));
  sessionStore.sync();
  return wasRunning;
}

void markSessionEndClean() {
  ArtifactCore::FastSettingsStore sessionStore(sessionStateFilePath());
  sessionStore.setValue(QStringLiteral("Session/isRunning"), false);
  sessionStore.setValue(QStringLiteral("Session/lastCleanExitTimestamp"),
                        QDateTime::currentDateTime().toString(Qt::ISODate));
  sessionStore.sync();
}

void showUncleanExitNoticeIfNeeded(bool hadUncleanExit, QWidget *parent) {
  if (!hadUncleanExit) {
    return;
  }

  QMessageBox box(parent);
  box.setIcon(QMessageBox::Warning);
  box.setWindowTitle(QStringLiteral("Previous Session Did Not Exit Cleanly"));
  box.setText(QStringLiteral("前回セッションが正常終了していません。"));
  box.setInformativeText(
      QStringLiteral("復旧スナップショットの場所を開きますか？"));
  auto *openFolder = box.addButton(QStringLiteral("Open Recovery Folder"),
                                   QMessageBox::ActionRole);
  box.addButton(QStringLiteral("Continue"), QMessageBox::AcceptRole);
  box.exec();

  if (box.clickedButton() == openFolder) {
    const QString recoveryDir = recoveryDirectoryPath();
    QDir dir(recoveryDir);
    if (!dir.exists()) {
      dir.mkpath(QStringLiteral("."));
    }
    QDesktopServices::openUrl(QUrl::fromLocalFile(recoveryDir));
  }
}

bool showRecoveryPrompt(ArtifactAutoSaveManager &autoSave, QWidget *parent) {
  if (!autoSave.hasRecoveryPoint()) {
    return false;
  }

  QMessageBox box(parent);
  box.setIcon(QMessageBox::Warning);
  box.setWindowTitle("Recovery Snapshot Found");
  box.setText("A crash recovery snapshot was found.");
  box.setInformativeText("Recover the latest snapshot now?");
  auto *recover = box.addButton("Recover", QMessageBox::AcceptRole);
  box.addButton("Ignore", QMessageBox::RejectRole);
  box.exec();

  if (box.clickedButton() != recover) {
    return false;
  }

  std::string recoveredJson;
  std::string sourcePath;
  if (!autoSave.loadLatestRecoveryPoint(&recoveredJson, &sourcePath) ||
      recoveredJson.empty()) {
    return false;
  }

  const QString tempRoot =
      QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
  QDir tempDir(tempRoot);
  tempDir.mkpath(".");
  const QString recoveredPath =
      tempDir.filePath("RecoveredProject.artifact.json");
  QFile out(recoveredPath);
  if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
    return false;
  }
  out.write(QByteArray::fromStdString(recoveredJson));
  out.close();

  ArtifactProjectManager::getInstance().loadFromFile(recoveredPath);
  return true;
}
} // namespace

void test() {
  cv::Mat mat(400, 640, CV_8UC3, cv::Scalar(0, 0, 0));
  cv::Mat mask(400, 640, CV_8UC3, cv::Scalar(0, 0, 0));
  cv::Mat dst(400, 640, CV_8UC3, cv::Scalar(0, 0, 0));
  drawStar5(mat, cv::Scalar(0, 255, 255), // edgeColor（黄）
            2,                            // edgeThickness
            cv::Scalar(-1, -1, -1),       // fillColor（無視される）
            0.8f);

  float threshold = 0.5f;        // この閾値より明るいピクセルがグローの元になる
  int vertical_blur_radius = 80; // 縦方向ぼかしの半径 (大きいほど縦に伸びる)
  float intensity = 1.1f;        // グローの強度

  cv::Mat glowed_image =
      applyVerticalGlow(mat, threshold, vertical_blur_radius, intensity);
  SetEnvironmentVariableW(L"COREHOST_TRACE", L"1");
  SetEnvironmentVariableW(L"COREHOST_TRACEFILE",
                          L"C:\\temp\\hostfxr_trace.log");

  /*
  DotnetRuntimeHost host;

  // ① .NET SDKのルート or dotnet.exeのルートを指定（例: "C:/Program
  Files/dotnet"） if (!host.initialize("C:/Program Files/dotnet")) { qWarning()
  << "hostfxr 初期化失敗"; return;
  }

  // ② アセンブリパスを指定（.dll）→ ここでは MyApp.dll を仮定
  QString dllPath =
  "C:/Users/lagma/Desktop/Artifact/Artifact/App/Debug/net9.0/ArtifactScriptRunner.dll";

  // 呼び出し先の型名とメソッド名（C#側と一致させる）
  if (!host.loadAssembly(dllPath))
  {
   qWarning() << "アセンブリのロード失敗";
   return;
  }

  void* method = nullptr;
  if (!(method =
  host.getFunctionPointer("ArtifactScriptRunner.ArtifactScriptRunner,ArtifactScriptRunner",
  "Add"))) {
   //std::cerr << hostfxr.getError() << std::endl;
   //return 1;
  }


         */
  // メソッドを呼び出す。
  // int result = reinterpret_cast<int(*)(int, int)>(method)(1, 2);
  // std::cout << result << std::endl;

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
  cv::Size img_size(800, 600);
  // 背景色: 0.0-1.0の範囲でグレー (RGBA)
  /*
  cv::Scalar bg_color(0.2, 0.2, 0.2, 1.0);

  cv::Point center(400, 300);
  int size = 200;
  int radius = 40; // 角の丸め半径

   cv::Mat pentagon_no_fill = drawFilledRoundedPentagon(img_size, bg_color,
  center, size, radius, cv::Scalar(0.0, 0.0, 0.0, 0.0), // 完全に透明
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
  // auto testImage = findAndLoadImageInAppDir("test.jpg", CV_32FC4);

  // auto context=new GpuContext();

  // context->Initialize();

  // auto negateCS = new
  // NegateCS(context->D3D12RenderDevice(),context->D3D12DeviceContext());

  // negateCS->loadShaderBinaryFromDirectory(QCoreApplication::applicationDirPath(),
  // "Negate.cso");

  // negateCS->Process(testImage);

  // SpectralGlow glow;

  // glow.ElegantGlow(testImage);

  int width = 600;
  int height = 400;

  // 作成する単色の色 (例: 青)
  // QColor::blue() の代わりに QColor(255, 0, 0) (赤), QColor(0, 255, 0) (緑)
  // なども指定できます。
  // QColor singleColor = QColor(0, 0, 255); // RGB (赤, 緑, 青)
  // の値で青色を設定

  // QImageオブジェクトを作成
  // Format_RGB32 は、各ピクセルが32ビット（RGBA）で表現される形式です。
  // この形式は、多くのシステムで効率的に扱われます。
  // QImage image(width, height, QImage::Format_RGB32);

  // 画像を単色で塗りつぶす
  // fill() メソッドは、指定された色で画像全体を塗りつぶします。
  // image.fill(singleColor);

  // auto file= findFirstFileByLooseExtensionFromAppDir("mov");

  // FFmpegThumbnailExtractor extractor;

  // auto image=extractor.extractThumbnail(file);

  // QString exePath = QCoreApplication::applicationDirPath();

  // 適当なファイル名（例：thumb_1234.png）
  // QString fileName = "thumb_" +
  // QString::number(QDateTime::currentMSecsSinceEpoch()) + ".png";

  // フルパスを作成
  // QString fullPath = QDir(exePath).filePath(fileName);
}

static void configureQtPaths() {
  // TODO: Configure Qt plugin paths and environment as needed
}

#if defined(_WIN32)
static void configureWindowsUtf8Console() {
  if (GetConsoleWindow() != nullptr) {
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
  }

  std::setlocale(LC_ALL, ".UTF-8");

  const int stdoutFd = _fileno(stdout);
  if (stdoutFd >= 0 && _isatty(stdoutFd)) {
    _setmode(stdoutFd, _O_U8TEXT);
  }

  const int stderrFd = _fileno(stderr);
  if (stderrFd >= 0 && _isatty(stderrFd)) {
    _setmode(stderrFd, _O_U8TEXT);
  }

  const int stdinFd = _fileno(stdin);
  if (stdinFd >= 0 && _isatty(stdinFd)) {
    _setmode(stdinFd, _O_U8TEXT);
  }
}

static void configureWindowsBinaryConsole() {
  const int stdinFd = _fileno(stdin);
  if (stdinFd >= 0) {
    _setmode(stdinFd, _O_BINARY);
  }
  const int stdoutFd = _fileno(stdout);
  if (stdoutFd >= 0) {
    _setmode(stdoutFd, _O_BINARY);
  }
}
#else
static void configureWindowsUtf8Console() {}
static void configureWindowsBinaryConsole() {}
#endif

static int runMcpServerMode() {
  configureWindowsBinaryConsole();

  QByteArray inputBuffer;
  char chunk[4096];
  while (true) {
    std::cin.read(chunk, sizeof(chunk));
    const std::streamsize readCount = std::cin.gcount();
    if (readCount <= 0) {
      break;
    }
    inputBuffer.append(chunk, static_cast<int>(readCount));

    QJsonObject request;
    while (ArtifactCore::McpBridge::tryPopFrame(&inputBuffer, &request)) {
      const QJsonObject response = ArtifactCore::McpBridge::handleRequest(
          request, ArtifactCore::AIContext());
      const QByteArray frame = ArtifactCore::McpBridge::encodeFrame(response);
      std::cout.write(frame.constData(),
                      static_cast<std::streamsize>(frame.size()));
      std::cout.flush();
    }
  }

  return 0;
}

int main(int argc, char *argv[]) {
  configureWindowsUtf8Console();
  ArtifactCore::CrashHandler::install();
  ArtifactCore::Logger::instance()->install();

  qDebug() << "Artifact Debug Console Initialized. Hello ArtifactStudio!";

  tbb::global_control c(tbb::global_control::max_allowed_parallelism,
                        std::thread::hardware_concurrency());

  AddDllDirectory(L"C:\\Users\\lagma\\Desktop\\Artifact\\Artifact\\App");
  SetDefaultDllDirectories(LOAD_LIBRARY_SEARCH_DEFAULT_DIRS |
                           LOAD_LIBRARY_SEARCH_USER_DIRS);
  // qsetenv("QT_QPA_PLATFORM", "windows:darkmode=[1]");

  // QTextCodec::setCodecForLocale(QTextCodec::codecForName("Shift-JIS"));
  bool renderMode = false;

  if (renderMode) {

  } else {
  }

  QStringList appArgs;
  for (int i = 0; i < argc; ++i) {
    appArgs << QString::fromLocal8Bit(argv[i]);
  }
  if (appArgs.contains(QStringLiteral("--mcp-server"))) {
    return runMcpServerMode();
  }

  // ============================================================
  // 起動言語オプションの処理
  // --lang ja/en/zh
  // ============================================================
  {
    int langIndex = appArgs.indexOf(QStringLiteral("--lang"));
    if (langIndex >= 0 && langIndex + 1 < appArgs.size()) {
      QString langCode = appArgs[langIndex + 1].toLower();
      ArtifactCore::LocaleLanguage targetLang = ArtifactCore::LocaleLanguage::Auto;
      
      if (langCode == QStringLiteral("ja") || langCode == QStringLiteral("japanese")) {
        targetLang = ArtifactCore::LocaleLanguage::Japanese;
        qInfo() << "[AppMain] Language set to Japanese via --lang";
      } else if (langCode == QStringLiteral("en") || langCode == QStringLiteral("english")) {
        targetLang = ArtifactCore::LocaleLanguage::English;
        qInfo() << "[AppMain] Language set to English via --lang";
      } else if (langCode.startsWith("zh-tw") || langCode == QStringLiteral("chinese-traditional")) {
        targetLang = ArtifactCore::LocaleLanguage::ChineseTraditional;
        qInfo() << "[AppMain] Language set to Traditional Chinese via --lang";
      } else if (langCode.startsWith("zh") || langCode == QStringLiteral("chinese-simplified")) {
        targetLang = ArtifactCore::LocaleLanguage::ChineseSimplified;
        qInfo() << "[AppMain] Language set to Simplified Chinese via --lang";
      } else {
        qWarning() << "[AppMain] Unknown language code:" << langCode;
      }
      
      if (targetLang != ArtifactCore::LocaleLanguage::Auto) {
        ArtifactCore::LocalizationManager::instance().setLanguage(targetLang);
      }
    }
  }

  QApplication a(argc, argv);
  configureQtPaths();
  Artifact::WorkspaceAutomation::ensureRegistered();

  // ============================================================
  // 翻訳システムの初期化 (LocalizationManager へ統合)
  // ============================================================
  {
    auto &loc = ArtifactCore::LocalizationManager::instance();
    const QString translationsDir =
        QDir(QCoreApplication::applicationDirPath())
            .filePath(QStringLiteral("translations"));
    if (QDir(translationsDir).exists()) {
      loc.loadFromDirectory(translationsDir);
      qDebug() << "[AppMain] Localization loaded, locale:" << loc.languageCode();
    } else {
      qDebug() << "[AppMain] Translations directory not found:" << translationsDir;
    }
  }

  // ============================================================
  // Problem View 検証ルールの登録
  // ============================================================
  {
    // ルールエンジンとルールのインスタンスを作成
    // 実際にはシングルトンかグローバルな場所に保持する必要があるが、
    // ここではデモ用に main 関数スコープで登録する（本来は App クラスなどで管理）
    // TODO: 診断エンジンをグローバルに保持する仕組みを用意する
    
    // 簡易的にルールのインスタンス化だけ行っておく
    // validateAll() はプロジェクト読み込み時などに呼び出す
    auto missingFileRule = std::make_unique<ArtifactMissingFileRule>();
    auto perfRule = std::make_unique<ArtifactPerformanceRule>();
    
    // デバッグ出力
    qInfo() << "[AppMain] Validation Rules initialized:" 
            << QString::fromStdString(missingFileRule->name().toStdString()) 
            << QString::fromStdString(perfRule->name().toStdString());
  }

  if (qEnvironmentVariableIsSet("ARTIFACT_RUN_BUILTIN_TESTS")) {
    const int builtinTestFailures = Artifact::runAllTests();
    if (builtinTestFailures != 0) {
      return builtinTestFailures;
    }
  }

  // Initialize environment variable manager
  auto *envManager = ArtifactCore::EnvironmentVariableManager::instance();
  qDebug() << "[AppMain] Environment variables loaded:"
           << envManager->variableNames().size();

  QLoggingCategory::setFilterRules(
      QStringLiteral("artifact.compositionview.debug=false\n"
                     "artifact.layer.video.debug=true"));
  auto *settings = ArtifactCore::ArtifactAppSettings::instance();
  auto applyThemeFromSettings = [&a, settings]() {
    if (!settings) {
      return;
    }
    ArtifactCore::DccStyleTheme theme = ArtifactCore::getDCCTheme(
        ArtifactCore::themePresetFromName(settings->themeName()));
    const QString themePresetPath = settings->themePresetPath().trimmed();
    if (!themePresetPath.isEmpty()) {
      QString loadError;
      ArtifactCore::DccStyleTheme loadedTheme;
      if (ArtifactCore::loadDCCThemePresetFromFile(themePresetPath,
                                                   &loadedTheme, &loadError)) {
        theme = loadedTheme;
      } else {
        qWarning() << "[AppMain] Failed to load theme preset:"
                   << themePresetPath << loadError;
      }
    }
    ArtifactCore::applyDCCTheme(a, theme);
    if (auto *style = a.style()) {
      style->polish(&a);
      const auto widgets = QApplication::allWidgets();
      for (QWidget *widget : widgets) {
        if (!widget) {
          continue;
        }
        style->unpolish(widget);
        style->polish(widget);
        widget->update();

        const QString className =
            QString::fromLatin1(widget->metaObject()->className());
        const QColor clear(theme.backgroundColor);
        if (className ==
            QStringLiteral("Artifact::ArtifactCompositionEditor")) {
          auto *compositionEditor =
              static_cast<ArtifactCompositionEditor *>(widget);
          compositionEditor->setClearColor(
              {clear.redF(), clear.greenF(), clear.blueF(), 1.0f});
        } else if (className ==
                   QStringLiteral("Artifact::ArtifactRenderLayerEditor")) {
          auto *layerEditor = static_cast<ArtifactRenderLayerEditor *>(widget);
          if (auto *view = layerEditor->view()) {
            view->setClearColor(
                {clear.redF(), clear.greenF(), clear.blueF(), 1.0f});
          }
        }
      }
    }
  };
  applyThemeFromSettings();
  QApplication::setStyle(
      new ArtifactCommonStyle(QStyleFactory::create(QStringLiteral("Fusion"))));
  QObject::connect(settings,
                   &ArtifactCore::ArtifactAppSettings::settingsChanged, &a,
                   applyThemeFromSettings);

  {
    QSettings aiSettings;
    const bool autoInitialize =
        aiSettings.value(QStringLiteral("AI/AutoInitialize"), false).toBool();
    const QString provider =
        aiSettings.value(QStringLiteral("AI/Provider"), QStringLiteral("local"))
            .toString()
            .trimmed();
    QString modelPath =
        aiSettings.value(QStringLiteral("AI/ModelPath")).toString().trimmed();
    if (modelPath.isEmpty()) {
      const QString normalizedProvider = provider.toLower();
      if (normalizedProvider == QStringLiteral("onnx") ||
          normalizedProvider == QStringLiteral("onnx-dml") ||
          normalizedProvider == QStringLiteral("onnxdml") ||
          normalizedProvider == QStringLiteral("directml")) {
        modelPath = QStringLiteral("models/onnx/model.onnx");
      } else {
        modelPath = QStringLiteral("models/llama-3.2-1b-instruct.q4_k_m.gguf");
      }
    }
    if (autoInitialize) {
      const QString normalizedProvider = provider.toLower();
      if (!provider.isEmpty() &&
          normalizedProvider != QStringLiteral("local") &&
          normalizedProvider != QStringLiteral("llama") &&
          normalizedProvider != QStringLiteral("onnx-dml") &&
          normalizedProvider != QStringLiteral("onnx") &&
          normalizedProvider != QStringLiteral("onnxdml") &&
          normalizedProvider != QStringLiteral("directml")) {
        qWarning()
            << "[AppMain] Auto AI initialization skipped: unsupported provider:"
            << provider;
      } else if (modelPath.isEmpty()) {
        qWarning()
            << "[AppMain] Auto AI initialization skipped: model path is empty";
      } else if (!QFileInfo::exists(modelPath)) {
        qWarning()
            << "[AppMain] Auto AI initialization skipped: model not found:"
            << modelPath;
      } else {
        auto *client = AIClient::instance();
        if (client->isInitializing()) {
          qInfo() << "[AppMain] Auto AI initialization skipped: backend is "
                     "already loading";
        } else {
          std::thread([client, modelPath]() {
            if (!client->initialize(modelPath)) {
              if (client->isInitializing()) {
                qInfo() << "[AppMain] Auto AI initialization is already in "
                           "progress:"
                        << modelPath;
              } else {
                qWarning()
                    << "[AppMain] Auto AI initialization failed for model:"
                    << modelPath;
              }
            } else {
              qDebug() << "[AppMain] Auto AI initialized from settings:"
                       << modelPath;
            }
          }).detach();
        }
      }
    }
  }

  auto pool = QThreadPool::globalInstance();

  pool->setMaxThreadCount(10);

  bootstrapPythonScripts();
  ArtifactPythonHookManager::runHook(QStringLiteral("on_startup"));
  const bool hadUncleanExit = markSessionStartAndDetectUncleanExit();
  const QIcon appIcon = buildTemporaryAppIcon();
  QApplication::setWindowIcon(appIcon);
  using namespace Artifact;
  auto *mw = new ArtifactMainWindow();
  mw->setObjectName("ArtifactMainWindow");
  mw->setWindowTitle(buildWindowTitle());
  mw->setWindowIcon(appIcon);
  auto *status = new ArtifactStatusBar(mw);
  mw->setStatusBar(status);
  status->showReadyMessage();
  status->setProjectText("Loaded");
  auto *projectService = ArtifactProjectService::instance();
  auto *playbackService = ArtifactPlaybackService::instance();
  // Enable output monitoring for debugging
  if (playbackService->controller()) {
    playbackService->controller()->enableOutputMonitoring(true);
    playbackService->controller()->setOutputMonitorCallback(
        [mw](bool audioOk, bool videoOk, const QString &context) {
          if (!audioOk || !videoOk) {
            auto *aiWidget = mw->aiCloudWidget();
            if (aiWidget) {
              QString prompt =
                  QString("プレビュー再生中に問題が発生しました。音: %1, 映像: "
                          "%2, コンテキスト: %3。原因を分析してください。")
                      .arg(audioOk ? "OK" : "NG")
                      .arg(videoOk ? "OK" : "NG")
                      .arg(context);
              aiWidget->startChatRequest(
                  prompt,
                  "あなたはAfter "
                  "Effectsのような動画編集アプリのデバッグアシスタントです。");
            }
          }
        });
  }
  auto *autoSaveManager = new ArtifactAutoSaveManager();
  QPointer<ArtifactRenderCenterWindow> renderCenterWindow;
  const QString recoveryDir =
      QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation))
          .filePath("Recovery");
  QTimer::singleShot(0, mw, [=, &renderCenterWindow]() {
    QSettings aiSettings;
    const QString aiProvider =
        aiSettings.value(QStringLiteral("AI/Provider"), QStringLiteral("local"))
            .toString()
            .trimmed();
    mw->addLazyDockedWidgetFloating(
        QStringLiteral("Playback Control"), QStringLiteral("PlaybackControl"),
        [mw]() -> QWidget * { return new ArtifactPlaybackControlWidget(mw); },
        QRect(120, 828, 720, 96));
    mw->addLazyDockedWidgetFloating(
        QStringLiteral("Debug Console"), QStringLiteral("DebugConsole"),
        [mw]() -> QWidget * { return new ArtifactDebugConsoleWidget(mw); },
        QRect(200, 200, 800, 400));
    mw->addLazyDockedWidgetFloating(
        QStringLiteral("AI Chat"), QStringLiteral("AIChat"),
        [mw, aiProvider]() -> QWidget * {
          auto *widget = new AIChatWidget(mw);
          widget->setProvider(UniString(aiProvider));
          return widget;
        },
        QRect(1040, 200, 760, 520));
    mw->setDockVisible(QStringLiteral("AI Chat"), false);
    auto *compositionEditor = new ArtifactCompositionEditor(mw);
    compositionEditor->setSizePolicy(QSizePolicy::Expanding,
                                     QSizePolicy::Expanding);
    suppressScrollBarsForViewportWidget(compositionEditor);
    mw->addDockedWidget(QStringLiteral("Composition Viewer"),
                        ads::CenterDockWidgetArea, compositionEditor);

    QObject::connect(compositionEditor,
                     &ArtifactCompositionEditor::videoDebugMessage, status,
                     &ArtifactStatusBar::setTimelineDebugText);

    // Update StatusBar console summary
    auto updateStatusConsole = [status, mw]() {
      if (!status)
        return;
      auto logs = Logger::instance()->getLogs();
      int errors = 0;
      int warnings = 0;
      for (const auto &log : logs) {
        if (log.level == LogLevel::Warning)
          warnings++;
        else if (log.level == LogLevel::Error || log.level == LogLevel::Fatal)
          errors++;
      }
      // status->setConsoleSummary(errors, warnings); // We can use this if the
      // previous edit definitely worked If setConsoleSummary failed to compile,
      // we can manually set text for now to avoid block
      status->setProjectText(
          QString("Logs: %1E %2W").arg(errors).arg(warnings));
    };
    QObject::connect(Logger::instance(), &Logger::logAdded, mw,
                     updateStatusConsole);
    QObject::connect(Logger::instance(), &Logger::logsCleared, mw,
                     updateStatusConsole);
    mw->addLazyDockedWidgetTabbedWithId(
        QStringLiteral("Composition View (Software)"),
        QStringLiteral("Composition View (Software)"),
        ads::CenterDockWidgetArea,
        [mw]() -> QWidget * {
          return new ArtifactSoftwareCompositionTestWidget(mw);
        },
        QStringLiteral("Composition Viewer"));
    auto *layerViewEditor = new ArtifactRenderLayerEditor(mw);
    layerViewEditor->setSizePolicy(QSizePolicy::Expanding,
                                   QSizePolicy::Expanding);
    suppressScrollBarsForViewportWidget(layerViewEditor);
    mw->addDockedWidgetTabbed(QStringLiteral("Layer View (Diligent)"),
                              ads::CenterDockWidgetArea, layerViewEditor,
                              QStringLiteral("Composition Viewer"));
    mw->addLazyDockedWidgetTabbedWithId(
        QStringLiteral("Layer View (Software)"),
        QStringLiteral("Layer View (Software)"), ads::CenterDockWidgetArea,
        [mw]() -> QWidget * { return new ArtifactSoftwareLayerTestWidget(mw); },
        QStringLiteral("Layer View (Diligent)"));
    auto *projectManagerWidget = new ArtifactProjectManagerWidget(mw);
    projectManagerWidget->setMinimumWidth(240);
    mw->addDockedWidget(QStringLiteral("Project"), ads::LeftDockWidgetArea,
                        projectManagerWidget);
    auto *assetBrowser = new ArtifactAssetBrowser(mw);
    mw->addDockedWidgetTabbed(QStringLiteral("Asset Browser"),
                              ads::LeftDockWidgetArea, assetBrowser,
                              QStringLiteral("Project"));
    auto *contentsViewer = new ArtifactContentsViewer(mw);
    mw->addDockedWidgetTabbed(QStringLiteral("Contents Viewer"),
                              ads::CenterDockWidgetArea, contentsViewer,
                              QStringLiteral("Composition Viewer"));
    mw->setDockVisible(QStringLiteral("Contents Viewer"), false);
    QObject::connect(assetBrowser, &ArtifactAssetBrowser::itemDoubleClicked, mw,
                     [mw, contentsViewer](const QString &itemPath) {
                       if (!contentsViewer || itemPath.isEmpty()) {
                         return;
                       }
                       if (!QFileInfo(itemPath).isFile()) {
                         return;
                       }
                       contentsViewer->setFilePath(itemPath);
                       mw->setDockVisible(QStringLiteral("Contents Viewer"),
                                          true);
                       mw->activateDock(QStringLiteral("Contents Viewer"));
                     });
    if (auto *projectView = projectManagerWidget
                                ? projectManagerWidget->projectView()
                                : nullptr) {
      QObject::connect(
          projectView, &ArtifactProjectView::itemSelected, mw,
          [mw, assetBrowser, contentsViewer](const QModelIndex &index) {
            if (!index.isValid()) {
              return;
            }
            QModelIndex sourceIdx = index;
            if (auto proxy = qobject_cast<const QSortFilterProxyModel *>(
                    index.model())) {
              sourceIdx = proxy->mapToSource(index);
            }
            const QVariant typeVar = sourceIdx.data(
                Qt::UserRole +
                static_cast<int>(
                    Artifact::ProjectItemDataRole::ProjectItemType));
            const QVariant ptrVar = sourceIdx.data(
                Qt::UserRole +
                static_cast<int>(
                    Artifact::ProjectItemDataRole::ProjectItemPtr));
            if (!typeVar.isValid()) {
              return;
            }

            if (typeVar.toInt() ==
                static_cast<int>(eProjectItemType::Composition)) {
              const QVariant idVar = sourceIdx.data(
                  Qt::UserRole +
                  static_cast<int>(
                      Artifact::ProjectItemDataRole::CompositionId));
              if (!idVar.isValid()) {
                return;
              }
              if (auto *service = ArtifactProjectService::instance()) {
                service->changeCurrentComposition(
                    CompositionID(idVar.toString()));
              }
              return;
            }

            if (typeVar.toInt() !=
                    static_cast<int>(eProjectItemType::Footage) ||
                !ptrVar.isValid() || !contentsViewer) {
              return;
            }
            ProjectItem *item =
                reinterpret_cast<ProjectItem *>(ptrVar.value<quintptr>());
            auto *footage = item && item->type() == eProjectItemType::Footage
                                ? static_cast<FootageItem *>(item)
                                : nullptr;
            if (!footage || footage->filePath.isEmpty() ||
                !QFileInfo(footage->filePath).isFile()) {
              return;
            }
            if (assetBrowser) {
              assetBrowser->selectAssetPaths(QStringList{footage->filePath});
            }
            contentsViewer->setFilePath(footage->filePath);
            mw->setDockVisible(QStringLiteral("Contents Viewer"), true);
            mw->activateDock(QStringLiteral("Contents Viewer"));
          });
    }
    QObject::connect(
        projectManagerWidget, &ArtifactProjectManagerWidget::itemDoubleClicked,
        mw, [mw, contentsViewer, assetBrowser](const QModelIndex &index) {
          if (!index.isValid()) {
            return;
          }
          QModelIndex sourceIdx = index;
          if (auto proxy =
                  qobject_cast<const QSortFilterProxyModel *>(index.model())) {
            sourceIdx = proxy->mapToSource(index);
          }
          const QVariant typeVar = sourceIdx.data(
              Qt::UserRole +
              static_cast<int>(Artifact::ProjectItemDataRole::ProjectItemType));
          const QVariant ptrVar = sourceIdx.data(
              Qt::UserRole +
              static_cast<int>(Artifact::ProjectItemDataRole::ProjectItemPtr));
          if (!typeVar.isValid()) {
            return;
          }
          if (typeVar.toInt() ==
              static_cast<int>(eProjectItemType::Composition)) {
            const QVariant idVar = sourceIdx.data(
                Qt::UserRole +
                static_cast<int>(Artifact::ProjectItemDataRole::CompositionId));
            if (!idVar.isValid()) {
              return;
            }
            const CompositionID compId(idVar.toString());
            if (auto *service = ArtifactProjectService::instance()) {
              service->changeCurrentComposition(compId);
            }
            mw->setDockVisible(QStringLiteral("Composition View (Software)"),
                               true);
            mw->activateDock(QStringLiteral("Composition View (Software)"));
            return;
          }
          if (typeVar.toInt() != static_cast<int>(eProjectItemType::Footage) ||
              !ptrVar.isValid() || !contentsViewer) {
            return;
          }
          ProjectItem *item =
              reinterpret_cast<ProjectItem *>(ptrVar.value<quintptr>());
          auto *footage = item && item->type() == eProjectItemType::Footage
                              ? static_cast<FootageItem *>(item)
                              : nullptr;
          if (!footage || footage->filePath.isEmpty() ||
              !QFileInfo(footage->filePath).isFile()) {
            return;
          }
          if (assetBrowser) {
            assetBrowser->selectAssetPaths(QStringList{footage->filePath});
          }
          contentsViewer->setFilePath(footage->filePath);
          mw->setDockVisible(QStringLiteral("Contents Viewer"), true);
          mw->activateDock(QStringLiteral("Contents Viewer"));
        });
    static ArtifactCore::EventBus appEventBus = ArtifactCore::globalEventBus();
    static std::vector<ArtifactCore::EventBus::Subscription>
        appEventSubscriptions;
    auto selectionSyncGuard = std::make_shared<bool>(false);
    appEventSubscriptions.push_back(
        appEventBus.subscribe<CurrentCompositionChangedEvent>(
            [projectManagerWidget](
                const CurrentCompositionChangedEvent &event) {
              const CompositionID compId(event.compositionId);
              auto *projectView = projectManagerWidget
                                      ? projectManagerWidget->projectView()
                                      : nullptr;
              if (!projectView || compId.isNil() || !projectView->model()) {
                return;
              }
              auto *model = projectView->model();
              const int rowCount = model->rowCount();
              for (int row = 0; row < rowCount; ++row) {
                const QModelIndex index = model->index(row, 0);
                if (!index.isValid()) {
                  continue;
                }
                const QModelIndex sourceIdx =
                    qobject_cast<const QSortFilterProxyModel *>(index.model())
                        ? qobject_cast<const QSortFilterProxyModel *>(
                              index.model())
                              ->mapToSource(index)
                        : index;
                const QVariant typeVar = sourceIdx.data(
                    Qt::UserRole +
                    static_cast<int>(
                        Artifact::ProjectItemDataRole::ProjectItemType));
                if (!typeVar.isValid() ||
                    typeVar.toInt() !=
                        static_cast<int>(eProjectItemType::Composition)) {
                  continue;
                }
                const QVariant idVar = sourceIdx.data(
                    Qt::UserRole +
                    static_cast<int>(
                        Artifact::ProjectItemDataRole::CompositionId));
                if (!idVar.isValid()) {
                  continue;
                }
                if (CompositionID(idVar.toString()) == compId) {
                  projectView->setCurrentIndex(index);
                  projectView->ensureIndexVisible(index);
                  return;
                }
              }
            }));
    QObject::connect(assetBrowser, &ArtifactAssetBrowser::selectionChanged, mw,
                     [projectManagerWidget,
                      selectionSyncGuard](const QStringList &selectedFiles) {
                       if (projectManagerWidget && selectionSyncGuard &&
                           !*selectionSyncGuard) {
                         *selectionSyncGuard = true;
                         projectManagerWidget->selectItemsByFilePaths(
                             selectedFiles);
                         *selectionSyncGuard = false;
                       }
                     });
    if (auto *projectView = projectManagerWidget
                                ? projectManagerWidget->projectView()
                                : nullptr) {
      QObject::connect(
          projectView, &ArtifactProjectView::itemSelected, mw,
          [assetBrowser, projectView,
           selectionSyncGuard](const QModelIndex &idx) {
            if (!assetBrowser || !selectionSyncGuard || *selectionSyncGuard ||
                !idx.isValid()) {
              return;
            }
            const auto *selectionModel =
                projectView ? projectView->selectionModel() : nullptr;
            if (!selectionModel) {
              return;
            }
            QStringList footagePaths;
            const auto rows = selectionModel->selectedRows(0);
            footagePaths.reserve(rows.size());
            for (const QModelIndex &row : rows) {
              QModelIndex sourceIdx = row;
              if (auto proxy = qobject_cast<const QSortFilterProxyModel *>(
                      sourceIdx.model())) {
                sourceIdx = proxy->mapToSource(sourceIdx).siblingAtColumn(0);
              }
              const QVariant typeVar = sourceIdx.data(
                  Qt::UserRole +
                  static_cast<int>(
                      Artifact::ProjectItemDataRole::ProjectItemType));
              const QVariant ptrVar = sourceIdx.data(
                  Qt::UserRole +
                  static_cast<int>(
                      Artifact::ProjectItemDataRole::ProjectItemPtr));
              if (!typeVar.isValid() ||
                  typeVar.toInt() !=
                      static_cast<int>(eProjectItemType::Footage) ||
                  !ptrVar.isValid()) {
                continue;
              }
              ProjectItem *item =
                  reinterpret_cast<ProjectItem *>(ptrVar.value<quintptr>());
              auto *footage = item && item->type() == eProjectItemType::Footage
                                  ? static_cast<FootageItem *>(item)
                                  : nullptr;
              if (footage && !footage->filePath.isEmpty()) {
                footagePaths.append(footage->filePath);
              }
            }
            footagePaths.removeDuplicates();
            if (footagePaths.isEmpty()) {
              return;
            }
            *selectionSyncGuard = true;
            assetBrowser->selectAssetPaths(footagePaths);
            *selectionSyncGuard = false;
          });
    }
    auto *inspectorWidget = new ArtifactInspectorWidget(mw);
    inspectorWidget->setMinimumWidth(240);
    mw->addDockedWidget(QStringLiteral("Inspector"), ads::RightDockWidgetArea,
                        inspectorWidget);
    auto *compositionNoteWidget = new ArtifactMarkdownNoteEditorWidget(
        MarkdownNoteTarget::Composition, mw);
    mw->addDockedWidgetTabbed(QStringLiteral("Composition Note"),
                              ads::RightDockWidgetArea, compositionNoteWidget,
                              QStringLiteral("Inspector"));
    auto *layerNoteWidget =
        new ArtifactMarkdownNoteEditorWidget(MarkdownNoteTarget::Layer, mw);
    mw->addDockedWidgetTabbed(QStringLiteral("Layer Note"),
                              ads::RightDockWidgetArea, layerNoteWidget,
                              QStringLiteral("Inspector"));
    auto *propertyPanel = new ArtifactPropertyWidget(mw);
    mw->addDockedWidgetTabbed(QStringLiteral("Properties"),
                              ads::RightDockWidgetArea, propertyPanel,
                              QStringLiteral("Inspector"));
    mw->addDockedWidget(QStringLiteral("Audio Mixer"), ads::RightDockWidgetArea,
                        new ArtifactCompositionAudioMixerWidget(mw));
    mw->addDockedWidget(QStringLiteral("AI Cloud"), ads::RightDockWidgetArea,
                        new ArtifactAICloudWidget(mw));
    renderCenterWindow = new ArtifactRenderCenterWindow();
    mw->setDockVisible(QStringLiteral("Audio Mixer"), false);
    mw->setDockVisible(QStringLiteral("Composition View (Software)"), false);
    mw->setDockVisible(QStringLiteral("Layer View (Diligent)"), false);
    mw->setDockVisible(QStringLiteral("Layer View (Software)"), false);

    autoSaveManager->initialize(
        std::filesystem::path("ArtifactProject"),
        std::filesystem::path(recoveryDir.toStdWString()));
    autoSaveManager->start();
    if (!isStartupDialogSuppressed()) {
      const bool hasRecoveryPoint = autoSaveManager->hasRecoveryPoint();
      showUncleanExitNoticeIfNeeded(hadUncleanExit, mw);
      showRecoveryPrompt(*autoSaveManager, mw);
      if (hadUncleanExit || hasRecoveryPoint) {
        suppressStartupDialogForDays(3);
      }
    }

    if (projectService) {
      if (auto *selectionManager = ArtifactApplicationManager::instance()
                                       ? ArtifactApplicationManager::instance()
                                             ->layerSelectionManager()
                                       : nullptr) {
        const auto syncSelectedLayerUi = [layerViewEditor, propertyPanel,
                                          projectService, selectionManager](
                                             const LayerID &layerId) {
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
            } else if (projectService) {
              auto current = selectionManager ? selectionManager->currentLayer()
                                              : ArtifactAbstractLayerPtr{};
              if (!current || current->id() != layerId) {
                const auto comp = projectService->currentComposition().lock();
                current = comp ? comp->layerById(layerId)
                               : ArtifactAbstractLayerPtr{};
              }
              if (current) {
                propertyPanel->setLayer(current);
              } else {
                propertyPanel->clear();
              }
            } else {
              propertyPanel->clear();
            }
          }
        };
        QObject::connect(
            selectionManager, &ArtifactLayerSelectionManager::selectionChanged,
            mw, [selectionManager, syncSelectedLayerUi]() {
              if (!selectionManager) {
                return;
              }
              const ArtifactAbstractLayerPtr current =
                  selectionManager->currentLayer();
              syncSelectedLayerUi(current ? current->id() : LayerID::Nil());
            });
      }
      appEventSubscriptions.push_back(
          appEventBus.subscribe<ProjectChangedEvent>(
              [status, autoSaveManager](const ProjectChangedEvent &) {
                if (status) {
                  status->setProjectText("Modified");
                }
                ArtifactPythonHookManager::runHook(
                    QStringLiteral("project_changed"));
                if (autoSaveManager) {
                  autoSaveManager->markDirty();
                }
              }));
      appEventSubscriptions.push_back(appEventBus.subscribe<LayerChangedEvent>(
          [status, autoSaveManager](const LayerChangedEvent &event) {
            if (event.changeType == LayerChangedEvent::ChangeType::Created) {
              if (status) {
                status->setProjectText("Layer Added");
              }
              ArtifactPythonHookManager::runHook(
                  QStringLiteral("layer_added"),
                  QStringList() << event.compositionId << event.layerId);
            } else if (event.changeType ==
                       LayerChangedEvent::ChangeType::Removed) {
              if (status) {
                status->setProjectText("Layer Removed");
              }
              ArtifactPythonHookManager::runHook(
                  QStringLiteral("layer_removed"),
                  QStringList() << event.compositionId << event.layerId);
            }
            if (autoSaveManager) {
              autoSaveManager->markDirty();
            }
          }));
      appEventSubscriptions.push_back(
          appEventBus.subscribe<LayerSelectionChangedEvent>(
              [layerViewEditor, propertyPanel, status,
               projectService](const LayerSelectionChangedEvent &event) {
                const LayerID layerId(event.layerId);
                // Guard: if this is a nil event but the selection manager still
                // has a valid current layer, skip the clear. Prevents spurious
                // property-edit notifications from blanking the Inspector.
                if (layerId.isNil()) {
                  auto *app = ArtifactApplicationManager::instance();
                  auto *sel = app ? app->layerSelectionManager() : nullptr;
                  if (sel && sel->currentLayer()) {
                    return;
                  }
                }
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
                  } else {
                    ArtifactAbstractLayerPtr current;
                    if (auto *selectionManager =
                            ArtifactApplicationManager::instance()
                                ? ArtifactApplicationManager::instance()
                                      ->layerSelectionManager()
                                : nullptr) {
                      current = selectionManager->currentLayer();
                    }
                    if (!current || current->id() != layerId) {
                      if (auto comp =
                              projectService
                                  ? projectService->currentComposition().lock()
                                  : ArtifactCompositionPtr{}) {
                        current = comp ? comp->layerById(layerId)
                                       : ArtifactAbstractLayerPtr{};
                      }
                    }
                    if (current) {
                      propertyPanel->setLayer(current);
                    } else {
                      propertyPanel->clear();
                    }
                  }
                }
                if (status) {
                  if (layerId.isNil()) {
                    status->setLayerText("None");
                  } else if (auto comp =
                                 projectService
                                     ? projectService->currentComposition()
                                           .lock()
                                     : ArtifactCompositionPtr{}) {
                    if (auto layer = comp->layerById(layerId)) {
                      const QString name = layer->layerName().trimmed();
                      status->setLayerText(name.isEmpty() ? layerId.toString()
                                                          : name);
                    } else {
                      status->setLayerText(layerId.toString());
                    }
                  } else {
                    status->setLayerText(layerId.toString());
                  }
                }
              }));
      appEventSubscriptions.push_back(
          appEventBus.subscribe<CurrentCompositionChangedEvent>(
              [compositionEditor, projectService, propertyPanel,
               layerViewEditor,
               status](const CurrentCompositionChangedEvent &event) {
                const CompositionID compId(event.compositionId);
                if (compositionEditor && projectService) {
                  const auto found = projectService->findComposition(compId);
                  if (found.success && !found.ptr.expired()) {
                    auto comp = found.ptr.lock();
                    compositionEditor->setComposition(comp);
                    const QString compName =
                        comp->settings().compositionName().toQString();
                    if (!compName.isEmpty()) {
                      compositionEditor->setWindowTitle(compName);
                    }
                  }
                }
                if (propertyPanel) {
                  propertyPanel->setFocusedEffectId(QString());
                  propertyPanel->clear();
                }
                if (layerViewEditor) {
                  layerViewEditor->view()->clearTargetLayer();
                }
                if (status) {
                  if (compId.isNil()) {
                    status->setLayerText("None");
                    status->setCompositionInfo("NO COMPOSITION", 0, 0, 0);
                  } else if (auto comp =
                                 projectService
                                     ? projectService->currentComposition()
                                           .lock()
                                     : ArtifactCompositionPtr{}) {
                    status->setLayerText(
                        comp->allLayer().isEmpty()
                            ? "None"
                            : QStringLiteral("(composition active)"));
                    const auto &settings = comp->settings();
                    status->setCompositionInfo(
                        settings.compositionName().toQString(),
                        settings.compositionSize().width(),
                        settings.compositionSize().height(),
                        comp->frameRate().framerate());
                  }
                }
              }));
      const auto timelineDockTitle =
          [projectService](const CompositionID &compId) {
            QString compositionLabel = compId.toString();
            if (projectService) {
              const auto found = projectService->findComposition(compId);
              if (found.success) {
                if (auto composition = found.ptr.lock()) {
                  const QString liveName = composition->settings()
                                               .compositionName()
                                               .toQString()
                                               .trimmed();
                  if (!liveName.isEmpty()) {
                    compositionLabel = liveName;
                  }
                }
              }
            }
            return compositionLabel;
          };
      const auto timelineDockObjectId = [](const CompositionID &compId) {
        return QStringLiteral("timeline::%1").arg(compId.toString());
      };
      appEventSubscriptions.push_back(
          appEventBus.subscribe<CompositionCreatedEvent>(
              [mw, timelineDockTitle, timelineDockObjectId,
               status](const CompositionCreatedEvent &event) {
                const CompositionID compId(event.compositionId);
                ArtifactPythonHookManager::runHook(
                    QStringLiteral("composition_created"),
                    QStringList() << event.compositionId);
                QTimer::singleShot(
                    0, mw,
                    [mw, compId, timelineDockTitle, timelineDockObjectId,
                     status]() {
                      const QString dockTitle = timelineDockTitle(compId);
                      const QString dockId = timelineDockObjectId(compId);
                      if (mw->hasDock(dockId)) {
                        mw->activateDock(dockId);
                        return;
                      }
                      auto *panel = new ArtifactTimelineWidget(mw);
                      panel->setMinimumHeight(200);
                      panel->resize(1200, 350);
                      panel->setComposition(compId);
                      panel->setWindowTitle(dockTitle);

                      QObject::connect(
                          panel, &ArtifactTimelineWidget::zoomLevelChanged,
                          status, &ArtifactStatusBar::setZoomPercent);
                      QObject::connect(
                          panel, &ArtifactTimelineWidget::timelineDebugMessage,
                          status, &ArtifactStatusBar::setTimelineDebugText);

                      mw->addDockedWidgetTabbedWithId(
                          dockTitle, dockId, ads::BottomDockWidgetArea, panel,
                          QStringLiteral("timeline::"));
                      // タイムライン追加後に縦スプリッターを調整して初期高さを確保
                      QTimer::singleShot(0, mw, [mw, dockId]() {
                        mw->setDockSplitterSizes(dockId, {700, 350});
                      });
                      QTimer::singleShot(
                          0, mw, [mw, dockId]() { mw->activateDock(dockId); });
                      QTimer::singleShot(0, mw, [mw, dockId, panel]() {
                        if (panel) {
                          panel->setFocus(Qt::OtherFocusReason);
                        }
                        mw->activateDock(dockId);
                      });
                    });
              }));
      appEventSubscriptions.push_back(
          appEventBus.subscribe<CompositionRemovedEvent>(
              [mw, timelineDockObjectId](const CompositionRemovedEvent &event) {
                mw->closeDock(
                    timelineDockObjectId(CompositionID(event.compositionId)));
              }));
      appEventSubscriptions.push_back(
          appEventBus.subscribe<ProjectCreatedEvent>(
              [](const ProjectCreatedEvent &) {
                ArtifactPythonHookManager::runHook(
                    QStringLiteral("project_opened"));
              }));
    }

    if (projectService && compositionEditor) {
      if (auto current = projectService->currentComposition().lock()) {
        compositionEditor->setComposition(current);
      }
    }

    auto latestFrame = std::make_shared<std::atomic<long long>>(0);
    auto hasFrameUpdate = std::make_shared<std::atomic_bool>(false);
    auto frameCounter = std::make_shared<std::atomic<int>>(0);

    auto *uiTimer = new QTimer(mw);
    uiTimer->setInterval(33); // ~30Hz UI update
    QObject::connect(uiTimer, &QTimer::timeout, mw,
                     [status, latestFrame, hasFrameUpdate]() {
                       if (hasFrameUpdate->exchange(false)) {
                         status->setFrame(latestFrame->load());
                       }
                     });
    uiTimer->start();

    auto *statsTimer = new QTimer(mw);
    statsTimer->setInterval(500);
    auto fpsElapsed = std::make_shared<QElapsedTimer>();
    fpsElapsed->start();
    QObject::connect(
        statsTimer, &QTimer::timeout, mw, [status, fpsElapsed, frameCounter]() {
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

    auto *recoveryTimer = new QTimer(mw);
    recoveryTimer->setInterval(120000);
    QObject::connect(recoveryTimer, &QTimer::timeout, mw, [autoSaveManager]() {
      if (!autoSaveManager || !autoSaveManager->isDirty()) {
        return;
      }
      const QByteArray snapshot = currentProjectSnapshotJson();
      if (!snapshot.isEmpty()) {
        autoSaveManager->createRecoveryPoint(snapshot.toStdString());
      }
    });
    recoveryTimer->start();

    if (playbackService) {
      QObject::connect(playbackService, &ArtifactPlaybackService::frameChanged,
                       mw,
                       [latestFrame, hasFrameUpdate,
                        frameCounter](const FramePosition &position) {
                         latestFrame->store(position.framePosition());
                         hasFrameUpdate->store(true);
                         frameCounter->fetch_add(1);
                       });
    }

    {
      const QString appDataDir =
          QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
      QDir dataDir(appDataDir);
      if (!dataDir.exists()) {
        dataDir.mkpath(QStringLiteral("."));
      }
      ArtifactCore::FastSettingsStore layoutStore(
          dataDir.filePath(QStringLiteral("main_window_layout.cbor")));
      sanitizeLayoutStore(layoutStore);
      if (!layoutStore.contains(QStringLiteral("MainWindow/layoutKey")) &&
          !layoutStore.contains(QStringLiteral("MainWindow/geometry")) &&
          !layoutStore.contains(QStringLiteral("MainWindow/state"))) {
        // Backward compatibility: import once from legacy QSettings.
        QSettings legacy(QStringLiteral("ArtifactStudio"),
                         QStringLiteral("Artifact"));
        auto legacyState = UiLayoutState::loadFromSettings(
            legacy, QStringLiteral("MainWindow"));
        if (!legacyState.isEmpty()) {
          legacyState.saveToStore(layoutStore, QStringLiteral("MainWindow"));
          layoutStore.sync();
        }
      }
      auto layoutState =
          UiLayoutState::loadFromStore(layoutStore, "MainWindow");
      if (layoutState.version != kMainWindowLayoutVersion) {
        layoutStore.remove("MainWindow/layoutKey");
        layoutStore.remove("MainWindow/version");
        layoutStore.remove("MainWindow/geometry");
        layoutStore.remove("MainWindow/state");
        layoutStore.sync();
        layoutState = UiLayoutState("ArtifactMainWindow");
      }
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
      recordLayoutRestoreResult(hasGeometry || hasState, geometryRestored,
                                stateRestored, resetApplied);
    }
    mw->setDockVisible(QStringLiteral("Layer View (Diligent)"), true);
    mw->activateDock(QStringLiteral("Layer View (Diligent)"));
  });

  QObject::connect(&a, &QCoreApplication::aboutToQuit, [mw]() {
    const QString appDataDir =
        QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir dataDir(appDataDir);
    if (!dataDir.exists()) {
      dataDir.mkpath(QStringLiteral("."));
    }
    ArtifactCore::FastSettingsStore layoutStore(
        dataDir.filePath(QStringLiteral("main_window_layout.cbor")));
    UiLayoutState layoutState("ArtifactMainWindow");
    layoutState.version = kMainWindowLayoutVersion;
    layoutState.geometry = mw->saveGeometry();
    layoutState.state = mw->saveState();
    layoutState.saveToStore(layoutStore, "MainWindow");
    layoutStore.sync();
  });
  QObject::connect(&a, &QCoreApplication::aboutToQuit, [&renderCenterWindow]() {
    if (renderCenterWindow) {
      renderCenterWindow->deleteLater();
    }
  });
  QObject::connect(&a, &QCoreApplication::aboutToQuit, mw,
                   &QObject::deleteLater);
  QObject::connect(&a, &QCoreApplication::aboutToQuit, [&]() {
    if (autoSaveManager) {
      if (autoSaveManager->isDirty()) {
        const QByteArray snapshot = currentProjectSnapshotJson();
        if (!snapshot.isEmpty()) {
          autoSaveManager->createRecoveryPoint(snapshot.toStdString());
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
