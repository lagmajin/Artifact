module;
#include <wobjectdefs.h>
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
 };







}