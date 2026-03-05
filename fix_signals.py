
import re

# Fix ArtifactProject.ixx
ixx_path = r'c:\Users\lagma\Desktop\Artifact\Artifact\include\Project\ArtifactProject.ixx'
with open(ixx_path, 'r', encoding='utf-8') as f:
    ixx_content = f.read()

ixx_content = re.sub(r'void\s+layerCreated\(\)\s*W_SIGNAL\(layerCreated\);', 
                     'void layerCreated(const LayerID& id)\n   W_SIGNAL(layerCreated, id);', ixx_content)

with open(ixx_path, 'w', encoding='utf-8') as f:
    f.write(ixx_content)
print("Updated ArtifactProject.ixx")

# Fix ArtifactProject.cppm
cppm_path = r'c:\Users\lagma\Desktop\Artifact\Artifact\src\Project\ArtifactProject.cppm'
with open(cppm_path, 'r', encoding='utf-8') as f:
    cppm_content = f.read()

# Update ArtifactProject::createLayerAndAddToComposition to emit signal
new_method = """
  ArtifactLayerResult ArtifactProject::createLayerAndAddToComposition(const CompositionID& compositionId, ArtifactLayerInitParams& params)
  {
   auto result = impl_->createLayerAndAddToComposition(compositionId, params);
   if (result.success && result.layer) {
    layerCreated(result.layer->id());
   }
   return result;
  }
"""
cppm_content = re.sub(r'ArtifactLayerResult\s+ArtifactProject::createLayerAndAddToComposition\(const\s+CompositionID&\s+compositionId,\s+ArtifactLayerInitParams&\s+params\)\s*\{.*?\}', 
                      new_method, cppm_content, flags=re.DOTALL)

with open(cppm_path, 'w', encoding='utf-8') as f:
    f.write(cppm_content)
print("Updated ArtifactProject.cppm")
