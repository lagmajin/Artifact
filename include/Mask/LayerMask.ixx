module;
export module Artifact.Mask.LayerMask;

import std;
import Artifact.Mask.Path;

export namespace Artifact {

/// レイヤーに付属するマスクコンテナ
/// 複数の MaskPath を保持し、合成してアルファマスクを生成する
class LayerMask {
private:
    class Impl;
    Impl* impl_;
public:
    LayerMask();
    ~LayerMask();
    LayerMask(const LayerMask& other);
    LayerMask& operator=(const LayerMask& other);

    // マスクパスの管理
    void addMaskPath(const MaskPath& path);
    void removeMaskPath(int index);
    void setMaskPath(int index, const MaskPath& path);
    MaskPath maskPath(int index) const;
    int maskPathCount() const;
    void clearMaskPaths();

    // マスク全体の有効/無効
    bool isEnabled() const;
    void setEnabled(bool enabled);

    /// 全マスクパスを合成して単一アルファマスク (CV_32FC1) を生成
    /// outMat は cv::Mat* を void* として渡す
    void compositeAlphaMask(int width, int height, void* outMat) const;

    /// RGBA画像のアルファチャンネルにマスクを乗算適用
    /// imageMat は CV_32FC4 の cv::Mat* を void* として渡す
    void applyToImage(int width, int height, void* imageMat) const;
};

}
