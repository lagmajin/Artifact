module;
export module Image.ImageF32x4_RGBA.Compat;

import Image.ImageF32xN;
import Image.ImageF32x4_RGBA;

export namespace ArtifactCore {

// Thin compatibility helpers between ImageF32xN and ImageF32x4_RGBA
inline ImageF32x4_RGBA ImageFromF32xN(const ImageF32xN& src) {
    ImageF32x4_RGBA out;
    // TODO: implement conversion (placeholder)
    (void)src;
    return out;
}

inline ImageF32xN ImageToF32xN(const ImageF32x4_RGBA& src) {
    ImageF32xN out;
    // TODO: implement conversion (placeholder)
    (void)src;
    return out;
}

}
