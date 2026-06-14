#ifndef TEXTURE_UTIL_H
#define TEXTURE_UTIL_H

#include <raylib.h>
#include <cmath>

// Pixel-art sprites need nearest filtering and clamped UVs (especially on WebGL).
inline void ConfigurePixelArtTexture(Texture2D tex)
{
    SetTextureFilter(tex, TEXTURE_FILTER_POINT);
    SetTextureWrap(tex, TEXTURE_WRAP_CLAMP);
}

// Snap draw positions so scaled sprites stay on pixel boundaries.
inline float SnapPixel(float v)
{
    return floorf(v + 0.5f);
}

#endif
