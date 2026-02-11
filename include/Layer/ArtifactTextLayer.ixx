module;
export module Artifact.Layer.Text;

import std;
import Artifact.Layer.Abstract;
import Utils.String.UniString;
import FloatRGBA;
import Artifact.Layers;

export namespace Artifact {

class ArtifactTextLayer : public ArtifactAbstractLayer {
private:
    class Impl;
    Impl* impl_;
public:
    ArtifactTextLayer();
    ~ArtifactTextLayer();

    void setText(const UniString& text);
    UniString text() const;

    void setFontSize(float size);
    float fontSize() const;

    void setFontFamily(const UniString& family);
    UniString fontFamily() const;

    void setTextColor(const FloatRGBA& color);
    FloatRGBA textColor() const;

    // Trigger update of internal image
    void updateImage();

    void draw() override;
};

}
