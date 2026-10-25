export module Artifact.Application.Manager.Access;

export namespace Artifact
{
void* createApplicationGlobalEffectManager();
void destroyApplicationGlobalEffectManager(void* manager);
void* createApplicationTestProjectManager();
void destroyApplicationTestProjectManager(void* manager);
void* createApplicationToolManager();
void destroyApplicationToolManager(void* manager);
void* createApplicationMotionSketchTool();
void destroyApplicationMotionSketchTool(void* tool);
void* createApplicationPuppetTool();
void destroyApplicationPuppetTool(void* tool);
void* createApplicationBrushTool();
void destroyApplicationBrushTool(void* tool);
void* createApplicationEnvironmentVariableManager();
void destroyApplicationEnvironmentVariableManager(void* manager);
void* createApplicationCompositionManager();
void destroyApplicationCompositionManager(void* manager);
void* applicationProjectManagerOpaque();
void* applicationProjectServiceOpaque();
void* createApplicationActiveContextService();
void destroyApplicationActiveContextService(void* service);
void* applicationLayerSelectionManagerOpaque();
}
