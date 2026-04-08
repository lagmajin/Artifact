module;
#include <utility>
#include <QMainWindow>
#include <QColor>
#include <QPalette>
#include <QStatusBar>
#include <QWidget>
#include <QVBoxLayout>
#include <wobjectimpl.h>
#if defined(_WIN32)
#include <windows.h>
#endif

module Artifact.Widgets.RenderCenterWindow;

import Artifact.Widgets.RenderCenterWindow;
import Artifact.Widgets.Render.QueueManager;
import Widgets.Utils.CSS;

namespace Artifact {

W_OBJECT_IMPL(ArtifactRenderCenterWindow)

class ArtifactRenderCenterWindow::Impl {
public:
 RenderQueueManagerWidget* queueWidget = nullptr;
};

namespace {
using DwmSetWindowAttributeFn = HRESULT(WINAPI*)(HWND, DWORD, LPCVOID, DWORD);
void applyDarkNativeTitleBar(QWidget* widget)
{
 if (!widget) return;
 const HWND hwnd = reinterpret_cast<HWND>(widget->winId());
 if (!hwnd) return;
 static HMODULE dwmModule = ::LoadLibraryW(L"dwmapi.dll");
 if (!dwmModule) return;
 static const auto setWindowAttribute =
  reinterpret_cast<DwmSetWindowAttributeFn>(::GetProcAddress(dwmModule, "DwmSetWindowAttribute"));
 if (!setWindowAttribute) return;
 const BOOL darkModeEnabled = TRUE;
 const DWORD darkModeAttributes[] = {20u, 19u};
 for (const DWORD attribute : darkModeAttributes) {
  setWindowAttribute(hwnd, attribute, &darkModeEnabled, sizeof(darkModeEnabled));
 }
 const COLORREF captionColor = RGB(40, 40, 40);
 const COLORREF textColor = RGB(187, 187, 187);
 const COLORREF borderColor = RGB(24, 24, 24);
 setWindowAttribute(hwnd, 35u, &captionColor, sizeof(captionColor));
 setWindowAttribute(hwnd, 36u, &textColor, sizeof(textColor));
 setWindowAttribute(hwnd, 34u, &borderColor, sizeof(borderColor));
 ::SetWindowPos(hwnd, nullptr, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
}
}

ArtifactRenderCenterWindow::ArtifactRenderCenterWindow(QWidget* parent)
 : QMainWindow(parent), impl_(new Impl())
{
 setObjectName(QStringLiteral("ArtifactRenderCenterWindow"));
 setWindowTitle(QStringLiteral("Render Queue - Artifact Studio"));
 resize(1100, 800);

// Apply native dark title bar on Windows
#if defined(_WIN32)
 applyDarkNativeTitleBar(this);
#endif

 auto* host = new QWidget(this);
 host->setAutoFillBackground(true);
 {
  QPalette pal = host->palette();
  pal.setColor(QPalette::Window, QColor(ArtifactCore::currentDCCTheme().backgroundColor));
  host->setPalette(pal);
 }
 auto* hostLayout = new QVBoxLayout(host);
 hostLayout->setContentsMargins(0, 0, 0, 0);
 hostLayout->setSpacing(0);

 impl_->queueWidget = new RenderQueueManagerWidget(host);
 hostLayout->addWidget(impl_->queueWidget);

 setCentralWidget(host);
 
 if (statusBar()) {
  QPalette pal = statusBar()->palette();
  pal.setColor(QPalette::Window, QColor(ArtifactCore::currentDCCTheme().secondaryBackgroundColor));
  pal.setColor(QPalette::WindowText, QColor(ArtifactCore::currentDCCTheme().textColor).darker(150));
  statusBar()->setAutoFillBackground(true);
  statusBar()->setPalette(pal);
  statusBar()->showMessage(QStringLiteral("Render Center Ready"));
 }
}

ArtifactRenderCenterWindow::~ArtifactRenderCenterWindow()
{
 delete impl_;
}

void ArtifactRenderCenterWindow::present()
{
 show();
 if (isMinimized()) {
  showNormal();
 }
 raise();
 activateWindow();
}

}
