
import re

path = r'c:\Users\lagma\Desktop\Artifact\Artifact\src\Widgets\ArtifactMainWindow.cpp'
with open(path, 'r', encoding='utf-8') as f:
    content = f.read()

# Define the new status bar setup
# This adds a status dot and container
new_status_setup = """
  // Setup status bar with multiple widgets
  auto statusbar = statusBar();
  statusbar->setStyleSheet(R"(
      QStatusBar {
          background: #181818;
          border-top: 1px solid #2d2d2d;
          min-height: 24px;
          color: #888;
      }
      QStatusBar::item { border: none; }
  )");
  
  // Create status bar labels
  auto statusContainer = new QWidget();
  auto statusLayout = new QHBoxLayout(statusContainer);
  statusLayout->setContentsMargins(8, 0, 8, 0);
  statusLayout->setSpacing(6);
  
  auto statusDot = new QLabel();
  statusDot->setFixedSize(6, 6);
  statusDot->setStyleSheet("background-color: #4CAF50; border-radius: 3px;");
  
  auto statusLabel = new QLabel("READY");
  statusLabel->setStyleSheet("color: #ccc; font-size: 10px; font-weight: bold; font-family: 'Segoe UI';");
  statusLabel->setObjectName("statusLabel");
  
  statusLayout->addWidget(statusDot);
  statusLayout->addWidget(statusLabel);
  statusLayout->addStretch();

  auto coordinatesLabel = new QLabel("X: 0 | Y: 0");
  coordinatesLabel->setMinimumWidth(100);
  coordinatesLabel->setAlignment(Qt::AlignCenter);
  coordinatesLabel->setObjectName("coordinatesLabel");
  coordinatesLabel->setStyleSheet("color: #888; border-left: 1px solid #2d2d2d; font-size: 10px; padding: 0 10px; font-family: 'Segoe UI';");
  
  auto zoomLabel = new QLabel("ZOOM: 100%");
  zoomLabel->setMinimumWidth(80);
  zoomLabel->setAlignment(Qt::AlignCenter);
  zoomLabel->setObjectName("zoomLabel");
  zoomLabel->setStyleSheet("color: #888; border-left: 1px solid #2d2d2d; font-size: 10px; padding: 0 10px; font-family: 'Segoe UI';");
  
  auto memoryLabel = new QLabel("MEM: 0 MB");
  memoryLabel->setMinimumWidth(90);
  memoryLabel->setAlignment(Qt::AlignCenter);
  memoryLabel->setObjectName("memoryLabel");
  memoryLabel->setStyleSheet("color: #888; border-left: 1px solid #2d2d2d; font-size: 10px; padding: 0 10px; font-family: 'Segoe UI';");
  
  auto fpsLabel = new QLabel("FPS: 00.0");
  fpsLabel->setMinimumWidth(80);
  fpsLabel->setAlignment(Qt::AlignCenter);
  fpsLabel->setObjectName("fpsLabel");
  fpsLabel->setStyleSheet("color: #888; border-left: 1px solid #2d2d2d; font-size: 10px; padding: 0 10px; font-family: 'Segoe UI';");
  
  // Add widgets to status bar
  statusbar->addWidget(statusContainer, 1);
  statusbar->addPermanentWidget(fpsLabel);
  statusbar->addPermanentWidget(memoryLabel);
  statusbar->addPermanentWidget(zoomLabel);
  statusbar->addPermanentWidget(coordinatesLabel);
"""

# Replace the existing status bar block (lines 185 to 224 roughly)
pattern = r'//\s*Setup\s+status\s+bar\s+with\s+multiple\s+widgets.*statusbar->addPermanentWidget\(coordinatesLabel\);'
content = re.sub(pattern, new_status_setup, content, flags=re.DOTALL)

# Also ensure setStatusReady sets text to "READY" all-caps
content = re.sub(r'void\s+ArtifactMainWindow::setStatusReady\(\)\s*\{\s*if\s*\(impl_->statusLabel_\)\s*\{\s*impl_->statusLabel_->setText\(QString\("Ready"\)\);\s*\}\s*\}',
                 'void ArtifactMainWindow::setStatusReady() { if (impl_->statusLabel_) impl_->statusLabel_->setText("READY"); }', content, flags=re.DOTALL)

with open(path, 'w', encoding='utf-8') as f:
    f.write(content)
print("Ultra-Premium StatusBar applied")
