#pragma once
namespace Artifact {
    class ArtifactTestRenderQueue {
    public:
        ArtifactTestRenderQueue();
        ~ArtifactTestRenderQueue();
        void runAllTests();

    private:
        class Impl;
        Impl* impl_;
    };
} // namespace Artifact