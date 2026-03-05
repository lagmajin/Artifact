
import re

path = r'c:\Users\lagma\Desktop\Artifact\Artifact\src\Widgets\ArtifactMainWindow.cpp'
with open(path, 'r', encoding='utf-8') as f:
    content = f.read()

# Update labels creation
content = re.sub(r'auto\s+statusLabel\s+=\s+new\s+QLabel\("READY"\);', 
                 'auto statusLabel = new QLabel("READY");\n   statusLabel->setStyleSheet("color: #aaa; font-size: 10px; font-weight: bold; margin-left: 8px; font-family: \'Segoe UI\';");', content)

content = re.sub(r'auto\s+coordinatesLabel\s+=\s+new\s+QLabel\("X: 0 \| Y: 0"\);\s+coordinatesLabel->setMinimumWidth\(120\);',
                 'auto coordinatesLabel = new QLabel("X: 0 | Y: 0");\n   coordinatesLabel->setMinimumWidth(100);\n   coordinatesLabel->setStyleSheet("color: #888; border-left: 1px solid #333; font-size: 10px; margin: 3px 0;");', content)

content = re.sub(r'auto\s+zoomLabel\s+=\s+new\s+QLabel\("Zoom: 100%"\);\s+zoomLabel->setMinimumWidth\(100\);',
                 'auto zoomLabel = new QLabel("ZOOM: 100%");\n   zoomLabel->setMinimumWidth(100);\n   zoomLabel->setStyleSheet("color: #888; border-left: 1px solid #333; font-size: 10px; margin: 3px 0;");', content)

content = re.sub(r'auto\s+memoryLabel\s+=\s+new\s+QLabel\("Memory: 0 MB"\);\s+memoryLabel->setMinimumWidth\(120\);',
                 'auto memoryLabel = new QLabel("MEM: 0 MB");\n   memoryLabel->setMinimumWidth(100);\n   memoryLabel->setStyleSheet("color: #888; border-left: 1px solid #333; font-size: 10px; margin: 3px 0;");', content)

content = re.sub(r'auto\s+fpsLabel\s+=\s+new\s+QLabel\("FPS: 0"\);\s+fpsLabel->setMinimumWidth\(80\);',
                 'auto fpsLabel = new QLabel("FPS: 00.0");\n   fpsLabel->setMinimumWidth(80);\n   fpsLabel->setStyleSheet("color: #888; border-left: 1px solid #333; font-size: 10px; margin: 3px 0;");', content)

# Update setStatus* methods
content = re.sub(r'void\s+ArtifactMainWindow::setStatusZoomLevel\(float\s+zoomPercent\)\s*\{\s*if\s*\(impl_->zoomLabel_\)\s*\{\s*impl_->zoomLabel_->setText\(QString\("Zoom: %1%"\).arg\(static_cast<int>\(zoomPercent\)\)\);\s*\}\s*\}',
                 'void ArtifactMainWindow::setStatusZoomLevel(float zoomPercent) { if (impl_->zoomLabel_) impl_->zoomLabel_->setText(QString("ZOOM: %1%").arg(static_cast<int>(zoomPercent))); }', content, flags=re.DOTALL)

content = re.sub(r'void\s+ArtifactMainWindow::setStatusMemoryUsage\(uint64_t\s+memoryMB\)\s*\{\s*if\s*\(impl_->memoryLabel_\)\s*\{\s*impl_->memoryLabel_->setText\(QString\("Memory: %1 MB"\).arg\(memoryMB\)\);\s*\}\s*\}',
                 'void ArtifactMainWindow::setStatusMemoryUsage(uint64_t memoryMB) { if (impl_->memoryLabel_) impl_->memoryLabel_->setText(QString("MEM: %1 MB").arg(memoryMB)); }', content, flags=re.DOTALL)

content = re.sub(r'void\s+ArtifactMainWindow::setStatusFPS\(double\s+fps\)\s*\{\s*if\s*\(impl_->fpsLabel_\)\s*\{\s*impl_->fpsLabel_->setText\(QString\("FPS: %1"\).arg\(static_cast<int>\(fps\)\)\);\s*\}\s*\}',
                 'void ArtifactMainWindow::setStatusFPS(double fps) { if (impl_->fpsLabel_) impl_->fpsLabel_->setText(QString("FPS: %1").arg(QString::number(fps, \'f\', 1))); }', content, flags=re.DOTALL)

with open(path, 'w', encoding='utf-8') as f:
    f.write(content)
print("Updated MainWindow successfully")
