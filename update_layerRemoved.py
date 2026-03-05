
import re

# 1. ArtifactProject.ixx
path = r'c:\Users\lagma\Desktop\Artifact\Artifact\include\Project\ArtifactProject.ixx'
with open(path, 'r', encoding='utf-8') as f:
    content = f.read()

content = re.sub(r'void\s+layerRemoved\(const\s+LayerID&\s+id\)\s*W_SIGNAL\(layerRemoved,\s+id\);',
                 'void layerRemoved(const CompositionID& compId, const LayerID& id)\n   W_SIGNAL(layerRemoved, compId, id);', content)

with open(path, 'w', encoding='utf-8') as f:
    f.write(content)


# 2. ArtifactProject.cppm
path = r'c:\Users\lagma\Desktop\Artifact\Artifact\src\Project\ArtifactProject.cppm'
with open(path, 'r', encoding='utf-8') as f:
    content = f.read()

content = re.sub(r'if\s*\(ok\)\s*layerRemoved\(layerId\);',
                 'if (ok) layerRemoved(compositionId, layerId);', content)

with open(path, 'w', encoding='utf-8') as f:
    f.write(content)


# 3. ArtifactProjectManager.ixx
path = r'c:\Users\lagma\Desktop\Artifact\Artifact\include\Project\ArtifactProjectManager.ixx'
with open(path, 'r', encoding='utf-8') as f:
    content = f.read()

content = re.sub(r'void\s+layerRemoved\(const\s+LayerID&\s+id\)\s*W_SIGNAL\(layerRemoved,\s+id\);',
                 'void layerRemoved(const CompositionID& compId, const LayerID& id)\n   W_SIGNAL(layerRemoved, compId, id);', content)

with open(path, 'w', encoding='utf-8') as f:
    f.write(content)


# 4. ArtifactProjectManager.cppm
path = r'c:\Users\lagma\Desktop\Artifact\Artifact\src\Project\ArtifactProjectManager.cppm'
with open(path, 'r', encoding='utf-8') as f:
    content = f.read()

# Replace connect for layerRemoved
content = re.sub(r'connect\(shared\.get\(\),\s+&ArtifactProject::layerRemoved,\s+this,\s+\[weakProj,\s+this\]\(const\s+LayerID&\s+id\)\s*\{\s*if\s*\(weakProj\.lock\(\)\)\s*layerRemoved\(id\);\s*\}\);',
                 'connect(shared.get(), &ArtifactProject::layerRemoved, this, [weakProj, this](const CompositionID& cid, const LayerID& lid) {\n      if (weakProj.lock()) layerRemoved(cid, lid);\n    });', content)

with open(path, 'w', encoding='utf-8') as f:
    f.write(content)


# 5. ArtifactProjectService.ixx
path = r'c:\Users\lagma\Desktop\Artifact\Artifact\include\Service\ArtifactProjectService.ixx'
with open(path, 'r', encoding='utf-8') as f:
    content = f.read()

content = re.sub(r'void\s+layerRemoved\(const\s+LayerID&\s+id\)\s*W_SIGNAL\(layerRemoved,\s+id\);',
                 'void layerRemoved(const CompositionID& compId, const LayerID& layerId)\n   W_SIGNAL(layerRemoved, compId, layerId);', content)

with open(path, 'w', encoding='utf-8') as f:
    f.write(content)


# 6. ArtifactProjectService.cpp
path = r'c:\Users\lagma\Desktop\Artifact\Artifact\src\Service\ArtifactProjectService.cpp'
with open(path, 'r', encoding='utf-8') as f:
    content = f.read()

content = re.sub(r'connect\(mgr,\s+&ArtifactProjectManager::layerRemoved,\s+this,\s+\[this\]\(const\s+LayerID&\s+id\)\s*\{\s*layerRemoved\(id\);\s*\}\);',
                 'connect(mgr, &ArtifactProjectManager::layerRemoved, this, [this](const CompositionID& cid, const LayerID& lid) {\n    layerRemoved(cid, lid);\n  });', content)

with open(path, 'w', encoding='utf-8') as f:
    f.write(content)

print("Updated layerRemoved across chain")
