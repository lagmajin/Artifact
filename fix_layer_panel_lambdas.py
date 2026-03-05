
import re

path = r'c:\Users\lagma\Desktop\Artifact\Artifact\src\Widgets\Timeline\ArtifactLayerPanelWidget.cpp'
with open(path, 'r', encoding='utf-8') as f:
    content = f.read()

# Update signals connection in constructor to match the new 2-argument signal
content = re.sub(r'QObject::connect\(service,\s+&ArtifactProjectService::layerCreated,\s+this,\s+\[this\]\(const\s+LayerID&\)\s*\{',
                 'QObject::connect(service, &ArtifactProjectService::layerCreated, this, [this](const CompositionID& compId, const LayerID&) {', content)

# Also update layerRemoved if I changed it (yes, I did)
content = re.sub(r'QObject::connect\(service,\s+&ArtifactProjectService::layerRemoved,\s+this,\s+\[this\]\(const\s+LayerID&\)\s*\{',
                 'QObject::connect(service, &ArtifactProjectService::layerRemoved, this, [this](const CompositionID& compId, const LayerID&) {', content)

with open(path, 'w', encoding='utf-8') as f:
    f.write(content)
print("Updated ArtifactLayerPanelWidget signals")
