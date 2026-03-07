module;
#include <wobjectdefs.h>
#include <wobjectimpl.h>
#include <QObject>
export module Render;


export namespace Artifact {

 class ArtifactRenderer : public QObject {
    W_OBJECT(ArtifactRenderer)

 public:
    void renderingStarted() W_SIGNAL(renderingStarted);
    void renderingFinished() W_SIGNAL(renderingFinished);

 private:
    class Impl;
    Impl* impl_;

 public:
    ArtifactRenderer(QObject* parent = nullptr);
    ~ArtifactRenderer();

 private:
    class Impl {
    public:
        Impl();
        ~Impl();
    };
 };

 W_OBJECT_IMPL(ArtifactRenderer)

 inline ArtifactRenderer::ArtifactRenderer(QObject* parent)
    : QObject(parent), impl_(new Impl()) {
 }

 inline ArtifactRenderer::~ArtifactRenderer() {
    delete impl_;
 }

 inline ArtifactRenderer::Impl::Impl() {
 }

 inline ArtifactRenderer::Impl::~Impl() {
 }


}