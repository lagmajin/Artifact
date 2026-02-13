module;
#include <QString>
#include <QColor>
#include <QHash>

export module Generator.Effector;

import std;
import Utils.String.UniString;
import Artifact.Layer.Abstract;
import Image.ImageF32x4RGBAWithCache;

export namespace Artifact
{
  using namespace ArtifactCore;

  // Generator Effector のベースクラス
  class AbstractGeneratorEffector
  {
  private:
    class Impl;
    Impl* impl_;

  protected:
    // サブクラスが実装する生成ロジック
    virtual void generateContent(ImageF32x4RGBAWithCache& dst, 
                                 int frameNumber, 
                                 int width, 
                                 int height) = 0;

  public:
    AbstractGeneratorEffector();
    virtual ~AbstractGeneratorEffector();

    // レイヤーに適用
    void apply();
    void applyToLayer(std::shared_ptr<ArtifactAbstractLayer> layer);

    // 基本情報
    void setName(const UniString& name);
    UniString name() const;

    // パラメータ管理
    void setParameter(const UniString& paramName, float value);
    float getParameter(const UniString& paramName) const;

    void setParameterColor(const UniString& paramName, const QColor& color);
    QColor getParameterColor(const UniString& paramName) const;

    // 出力サイズ
    void setOutputSize(int width, int height);
    int outputWidth() const;
    int outputHeight() const;

    // フレーム範囲
    void setFrameRange(int startFrame, int endFrame);
    int startFrame() const;
    int endFrame() const;

    // 現在のフレーム
    void setCurrentFrame(int frameNumber);
    int currentFrame() const;

    // 有効/無効
    void setEnabled(bool enabled);
    bool isEnabled() const;
  };

  // ==================== 具体的なジェネレータの実装 ====================

  // 単色生成エフェクト
  class SolidGeneratorEffector : public AbstractGeneratorEffector {
  private:
    QColor solidColor_;

  protected:
    void generateContent(ImageF32x4RGBAWithCache& dst, 
                        int frameNumber, 
                        int width, 
                        int height) override;

  public:
    SolidGeneratorEffector();
    ~SolidGeneratorEffector();

    void setSolidColor(const QColor& color);
    QColor solidColor() const;
  };

  // グラデーション生成エフェクト
  class GradientGeneratorEffector : public AbstractGeneratorEffector {
  public:
    enum GradientType {
        Linear,
        Radial,
        Conic
    };

  private:
    GradientType gradientType_;
    QColor startColor_;
    QColor endColor_;

  protected:
    void generateContent(ImageF32x4RGBAWithCache& dst, 
                        int frameNumber, 
                        int width, 
                        int height) override;

  public:
    GradientGeneratorEffector();
    ~GradientGeneratorEffector();

    void setGradientType(GradientType type);
    GradientType gradientType() const;

    void setStartColor(const QColor& color);
    void setEndColor(const QColor& color);
    QColor startColor() const;
    QColor endColor() const;
  };

  // ノイズ生成エフェクト
  class NoiseGeneratorEffector : public AbstractGeneratorEffector {
  public:
    enum NoiseType {
        Perlin,
        Simplex,
        Voronoi,
        WhiteNoise
    };

  private:
    NoiseType noiseType_;
    float scale_;
    float amplitude_;
    int octaves_;

  protected:
    void generateContent(ImageF32x4RGBAWithCache& dst, 
                        int frameNumber, 
                        int width, 
                        int height) override;

  public:
    NoiseGeneratorEffector();
    ~NoiseGeneratorEffector();

    void setNoiseType(NoiseType type);
    NoiseType noiseType() const;

    void setScale(float scale);
    float scale() const;

    void setAmplitude(float amplitude);
    float amplitude() const;

    void setOctaves(int octaves);
    int octaves() const;
  };

  // シェイプ生成エフェクト
  class ShapeGeneratorEffector : public AbstractGeneratorEffector {
  public:
    enum ShapeType {
        Rectangle,
        Circle,
        Triangle,
        Polygon
    };

  private:
    ShapeType shapeType_;
    QColor shapeColor_;
    QColor backgroundColor_;
    float shapeSize_;

  protected:
    void generateContent(ImageF32x4RGBAWithCache& dst, 
                        int frameNumber, 
                        int width, 
                        int height) override;

  public:
    ShapeGeneratorEffector();
    ~ShapeGeneratorEffector();

    void setShapeType(ShapeType type);
    ShapeType shapeType() const;

    void setShapeColor(const QColor& color);
    void setBackgroundColor(const QColor& color);
    QColor shapeColor() const;
    QColor backgroundColor() const;

    void setShapeSize(float size);
    float shapeSize() const;
  };

};
