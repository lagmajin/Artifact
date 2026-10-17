module;
#include <array>
#include <cstddef>
#include <functional>
#include <string>
#include <utility>
#include <QString>

export module Artifact.Render.CompositionPassExecutor;

export import Artifact.Render.CompositionRenderPassContext;
export import Artifact.Render.CompositionRenderPassPlan;
import Graphics.RenderGraph;

export namespace Artifact {

class RenderPass {
public:
  virtual ~RenderPass() = default;
  virtual bool setup(RenderPassResources& resources) = 0;
  virtual bool execute(RenderPassContext& context,
                       RenderPassResources& resources) = 0;
  virtual FrameRenderPassKind kind() const = 0;
  virtual QString name() const = 0;
};

class FunctionalRenderPass final : public RenderPass {
public:
  using Setup = std::function<bool(RenderPassResources&)>;
  using Execute = std::function<bool(RenderPassContext&,
                                     RenderPassResources&)>;

  FunctionalRenderPass(FrameRenderPassKind passKind, QString passName,
                       Setup setup, Execute execute)
      : kind_(passKind), name_(std::move(passName)),
        setup_(std::move(setup)), execute_(std::move(execute)) {}

  bool setup(RenderPassResources& resources) override {
    return !setup_ || setup_(resources);
  }
  bool execute(RenderPassContext& context,
               RenderPassResources& resources) override {
    return execute_ && execute_(context, resources);
  }
  FrameRenderPassKind kind() const override { return kind_; }
  QString name() const override { return name_; }

private:
  FrameRenderPassKind kind_ = FrameRenderPassKind::Setup;
  QString name_;
  Setup setup_;
  Execute execute_;
};

struct RenderPassExecutor {
  static bool run(RenderPass& pass, RenderPassContext& context,
                  RenderPassResources& resources) {
    if (!context.renderer || !pass.setup(resources)) {
      return false;
    }
    return pass.execute(context, resources);
  }

  template <std::size_t PassCount>
  static bool runAll(const std::array<RenderPass*, PassCount>& passes,
                     RenderPassContext& context,
                     RenderPassResources& resources) {
    for (RenderPass* pass : passes) {
      if (!pass || !run(*pass, context, resources)) {
        return false;
      }
    }
    return true;
  }

  template <std::size_t PassCount>
  static bool runAllWithRenderGraph(
      const std::array<RenderPass*, PassCount>& passes,
      RenderPassContext& context, RenderPassResources& resources) {
    static const auto graphAndCompiled = [] {
      ArtifactCore::RenderGraph graph;
      // These viewport passes blend or clear the same active target. Model
      // that write hazard explicitly so graph diagnostics report the real
      // shared output and the compiler preserves painter order.
      const auto activeColorTarget = graph.addResource({
          "Renderer.ActiveColorTarget",
          ArtifactCore::RenderResourceKind::Texture,
          ArtifactCore::RenderResourceLifetime::External, 0, 0, 1, 0, 0});
      for (std::size_t index = 0; index < PassCount; ++index) {
        graph.addPass({"RenderPass." + std::to_string(index),
                       ArtifactCore::RenderPassQueue::Graphics,
                       {}, {activeColorTarget}, true});
      }
      auto compiled = graph.compile();
      return std::pair<ArtifactCore::RenderGraph,
                       ArtifactCore::CompiledRenderGraph>{
          std::move(graph), std::move(compiled)};
    }();
    const auto& graph = graphAndCompiled.first;
    const auto& compiled = graphAndCompiled.second;
    if (!compiled.valid) return false;

    std::size_t nextPass = 0;
    std::string error;
    return graph.execute(
        compiled,
        [&](const ArtifactCore::RenderGraphExecutionContext&) {
          return nextPass < PassCount && passes[nextPass] != nullptr &&
                 run(*passes[nextPass++], context, resources);
        },
        &error);
  }

  static bool runWithRenderGraph(RenderPass& pass,
                                 RenderPassContext& context,
                                 RenderPassResources& resources) {
    const std::array<RenderPass*, 1> passes{&pass};
    return runAllWithRenderGraph(passes, context, resources);
  }
};

} // namespace Artifact
