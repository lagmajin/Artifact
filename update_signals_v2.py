
import re

# 1. Update ArtifactProject.ixx
ixx_path = r'c:\Users\lagma\Desktop\Artifact\Artifact\include\Project\ArtifactProject.ixx'
with open(ixx_path, 'r', encoding='utf-8') as f:
    content = f.read()

content = re.sub(r'void\s+layerCreated\(const\s+LayerID&\s+id\)\s*W_SIGNAL\(layerCreated,\s+id\);', 
                 'void layerCreated(const CompositionID& compId, const LayerID& layerId)\n   W_SIGNAL(layerCreated, compId, layerId);', content)

with open(ixx_path, 'w', encoding='utf-8') as f:
    f.write(content)
print("Updated ArtifactProject.ixx signals")

# 2. Update ArtifactProject.cppm
cppm_path = r'c:\Users\lagma\Desktop\Artifact\Artifact\src\Project\ArtifactProject.cppm'
with open(cppm_path, 'r', encoding='utf-8') as f:
    content = f.read()

new_method = """
  ArtifactLayerResult ArtifactProject::createLayerAndAddToComposition(const CompositionID& compositionId, ArtifactLayerInitParams& params)
  {
   auto result = impl_->createLayerAndAddToComposition(compositionId, params);
   if (result.success && result.layer) {
    layerCreated(compositionId, result.layer->id());
   }
   return result;
  }
"""
content = re.sub(r'ArtifactLayerResult\s+ArtifactProject::createLayerAndAddToComposition\(const\s+CompositionID&\s+compositionId,\s+ArtifactLayerInitParams&\s+params\)\s*\{.*?\}', 
                  new_method, content, flags=re.DOTALL)

with open(cppm_path, 'w', encoding='utf-8') as f:
    f.write(content)
print("Updated ArtifactProject.cppm signal emission")

# 3. Update ArtifactProjectManager.cppm
manager_path = r'c:\Users\lagma\Desktop\Artifact\Artifact\src\Project\ArtifactProjectManager.cppm'
with open(manager_path, 'r', encoding='utf-8') as f:
    content = f.read()

# Fix signal forwarding in createProject and loadFromFile
# We need to change the lambda to take two arguments
# First for createProject:
content = re.sub(r'connect\(shared\.get\(\),\s+&ArtifactProject::layerCreated,\s+this,\s+\[weakProj,\s+this\]\(const\s+LayerID&\s+id\)\s*\{',
                 'connect(shared.get(), &ArtifactProject::layerCreated, this, [weakProj, this](const CompositionID& cid, const LayerID& lid) {', content)
# And the call inside:
content = re.sub(r'layerCreated\(id\);', 'layerCreated(cid, lid);', content)

with open(manager_path, 'w', encoding='utf-8') as f:
    f.write(content)
print("Updated ArtifactProjectManager.cppm forwarding")

# 4. Update ArtifactProjectService.ixx
service_ixx_path = r'c:\Users\lagma\Desktop\Artifact\Artifact\include\Service\ArtifactProjectService.ixx'
with open(service_ixx_path, 'r', encoding='utf-8') as f:
    content = f.read()

content = re.sub(r'void\s+layerCreated\(const\s+LayerID&\s+id\)\s*W_SIGNAL\(layerCreated,\s+id\);',
                 'void layerCreated(const CompositionID& compId, const LayerID& layerId)\n    W_SIGNAL(layerCreated, compId, layerId);', content)

with open(service_ixx_path, 'w', encoding='utf-8') as f:
    f.write(content)
print("Updated ArtifactProjectService.ixx signals")
