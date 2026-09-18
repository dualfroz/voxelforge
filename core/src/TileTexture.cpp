#include "vf/TileTexture.h"

#include <algorithm>
#include <cmath>

namespace vf {

namespace {

constexpr float twoPi = 6.28318530718f;

float clamp01(float v) {
    if (v < 0.0f) return 0.0f;
    if (v > 1.0f) return 1.0f;
    return v;
}

uint8_t toByte(float v) {
    if (v < 0.0f) return 0;
    if (v > 255.0f) return 255;
    return static_cast<uint8_t>(v + 0.5f);
}

uint32_t scramble(uint32_t v) {
    v ^= v >> 16;
    v *= 0x7feb352du;
    v ^= v >> 15;
    v *= 0x846ca68bu;
    v ^= v >> 16;
    return v;
}

uint32_t gridHash(int tileSalt, int x, int y, uint32_t salt, uint32_t seed) {
    uint32_t h = static_cast<uint32_t>(tileSalt) * 374761393u +
                 static_cast<uint32_t>(x) * 668265263u +
                 static_cast<uint32_t>(y) * 2246822519u + salt * 2654435761u + seed;
    return scramble(h);
}

float gridValue(int tileSalt, int x, int y, uint32_t salt, uint32_t seed) {
    return static_cast<float>(gridHash(tileSalt, x, y, salt, seed) & 0xffffu) / 65535.0f;
}

float easeCurve(float t) { return t * t * (3.0f - 2.0f * t); }

float valueNoise(int tileSalt, float fx, float fy, uint32_t salt, uint32_t seed) {
    float baseX = std::floor(fx);
    float baseY = std::floor(fy);
    int x0 = static_cast<int>(baseX);
    int y0 = static_cast<int>(baseY);
    float tx = easeCurve(fx - baseX);
    float ty = easeCurve(fy - baseY);
    float corner00 = gridValue(tileSalt, x0, y0, salt, seed);
    float corner10 = gridValue(tileSalt, x0 + 1, y0, salt, seed);
    float corner01 = gridValue(tileSalt, x0, y0 + 1, salt, seed);
    float corner11 = gridValue(tileSalt, x0 + 1, y0 + 1, salt, seed);
    float top = corner00 + (corner10 - corner00) * tx;
    float bottom = corner01 + (corner11 - corner01) * tx;
    return top + (bottom - top) * ty;
}

float fractalNoise(int tileSalt, float fx, float fy, uint32_t salt, uint32_t seed) {
    float coarse = valueNoise(tileSalt, fx, fy, salt, seed);
    float fine = valueNoise(tileSalt, fx * 2.17f + 3.1f, fy * 2.17f - 1.7f, salt + 101u, seed);
    return coarse * 0.66f + fine * 0.34f;
}

float lineNoise(int tileSalt, float along, uint32_t salt, uint32_t seed) {
    return valueNoise(tileSalt, along, 0.5f, salt, seed);
}

TileRgb mixRgb(TileRgb from, TileRgb to, float t) {
    float k = clamp01(t);
    TileRgb out;
    out.r = static_cast<uint8_t>(from.r + (to.r - from.r) * k + 0.5f);
    out.g = static_cast<uint8_t>(from.g + (to.g - from.g) * k + 0.5f);
    out.b = static_cast<uint8_t>(from.b + (to.b - from.b) * k + 0.5f);
    return out;
}

struct PatternSample {
    float accentWeight = 0.0f;
    float shade = 1.0f;
    bool hole = false;
};

PatternSample solidPattern(const TileSpec& spec, int tileSalt, int x, int y, int size,
                           uint32_t seed) {
    float cells = 2.0f + spec.scale * 4.0f;
    float blob = fractalNoise(tileSalt, x * cells / size, y * cells / size, 11u, seed);
    PatternSample out;
    out.accentWeight = clamp01((blob - 0.34f) * 2.0f) * spec.density;
    return out;
}

PatternSample orePattern(const TileSpec& spec, int tileSalt, int x, int y, int size,
                         uint32_t seed) {
    float cells = 6.5f - spec.scale * 3.5f;
    float blob = fractalNoise(tileSalt, x * cells / size, y * cells / size, 23u, seed);
    float threshold = 1.0f - spec.density * 1.15f;
    PatternSample out;
    if (blob > threshold) {
        out.accentWeight = clamp01((blob - threshold) * 9.0f);
        out.shade = 1.0f + 0.10f * out.accentWeight;
    } else if (blob > threshold - 0.05f) {
        out.shade = 0.78f;
    }
    return out;
}

PatternSample brickPattern(const TileSpec& spec, int tileSalt, int x, int y, int size,
                           uint32_t seed) {
    int courseHeight = std::max(2, static_cast<int>(std::lround(size * (0.16f + spec.scale * 0.2f))));
    int brickWidth = std::max(3, courseHeight * 2);
    int course = y / courseHeight;
    int shift = (course % 2 == 0) ? 0 : brickWidth / 2;
    int localY = y - course * courseHeight;
    int localX = (x + shift) % brickWidth;

    PatternSample out;
    if (localY == 0 || localX == 0) {
        out.accentWeight = 1.0f;
        out.shade = 0.95f;
        return out;
    }
    int brickIndex = (x + shift) / brickWidth;
    out.shade = 0.93f + 0.14f * gridValue(tileSalt, brickIndex, course, 29u, seed);
    if (localY == 1) out.shade *= 1.05f;
    if (localY == courseHeight - 1) out.shade *= 0.93f;
    return out;
}

PatternSample plankPattern(const TileSpec& spec, int tileSalt, int x, int y, int size,
                           uint32_t seed) {
    int across = spec.vertical ? x : y;
    int along = spec.vertical ? y : x;
    int boardWidth =
        std::max(3, static_cast<int>(std::lround(size * (0.16f + spec.scale * 0.30f))));
    int board = across / boardWidth;
    int localAcross = across - board * boardWidth;

    PatternSample out;
    if (localAcross == 0) {
        out.accentWeight = 1.0f;
        out.shade = 0.88f;
        return out;
    }
    out.shade = 0.94f + 0.12f * gridValue(tileSalt, board, spec.flipped ? 5 : 0, 31u, seed);
    if (localAcross == boardWidth - 1) out.shade *= 0.94f;
    float grain = lineNoise(tileSalt, along * (2.0f + spec.density * 5.0f) / size + board * 7.0f,
                            33u, seed);
    out.accentWeight = clamp01((grain - 0.52f) * 3.2f) * 0.7f;
    return out;
}

PatternSample latticePattern(const TileSpec& spec, int tileSalt, int x, int y, int size,
                             uint32_t seed) {
    int barWidth = std::max(1, static_cast<int>(std::lround(size * (0.06f + spec.scale * 0.14f))));
    int spacing = std::max(barWidth * 2 + 1,
                           static_cast<int>(std::lround(size * (0.22f + (1.0f - spec.density) * 0.3f))));
    int across = spec.vertical ? x : y;
    int along = spec.vertical ? y : x;

    bool onRail = false;
    bool onCross = false;
    if (spec.flipped) {
        onRail = (across % spacing) < barWidth;
        onCross = (along % spacing) < barWidth;
    } else {
        int inset = std::max(0, size / 8);
        onRail = (across >= inset && across < inset + barWidth) ||
                 (across >= size - inset - barWidth && across < size - inset);
        bool betweenRails = across >= inset && across < size - inset;
        onCross = betweenRails && (along % spacing) < barWidth;
    }

    PatternSample out;
    if (onRail || onCross) {
        out.accentWeight = (onCross && !onRail) ? 1.0f : 0.0f;
        out.shade = 0.90f + 0.18f * gridValue(tileSalt, x, y, 41u, seed);
        return out;
    }
    out.hole = true;
    out.shade = 0.42f;
    return out;
}

PatternSample edgeBandPattern(const TileSpec& spec, int tileSalt, int x, int y, int size,
                              uint32_t seed) {
    float thickness = size * (0.15f + spec.scale * 0.35f);
    float depth = spec.flipped ? static_cast<float>(size - 1 - y) : static_cast<float>(y);
    float fringe = (lineNoise(tileSalt, x * 4.0f / size, 43u, seed) - 0.5f) * size * 0.16f;
    float edge = thickness + fringe;

    PatternSample out;
    if (depth < edge) {
        out.accentWeight = 1.0f;
        out.shade = 1.0f - 0.10f * (depth / std::max(1.0f, edge));
        return out;
    }
    if (depth < edge + 1.0f) out.shade = 0.84f;
    out.accentWeight = 0.0f;
    return out;
}

PatternSample growthRingsPattern(const TileSpec& spec, int tileSalt, int x, int y, int size,
                                 uint32_t seed) {
    float centre = size * 0.5f;
    float dx = x + 0.5f - centre;
    float dy = y + 0.5f - centre;
    float radius = std::sqrt(dx * dx + dy * dy);
    float spacing = std::max(1.2f, size * (0.05f + spec.scale * 0.16f));
    float warp = (fractalNoise(tileSalt, x * 3.0f / size, y * 3.0f / size, 47u, seed) - 0.5f) *
                 spacing * 0.7f;
    float phase = (radius + warp) / spacing;
    float ring = std::fabs(phase - std::floor(phase) - 0.5f) * 2.0f;

    PatternSample out;
    out.accentWeight = clamp01((1.0f - ring) * 1.4f) * spec.density;
    if (radius < spacing * 0.55f) out.accentWeight = spec.density;
    out.shade = 1.0f - 0.10f * clamp01(radius / (size * 0.7f));
    return out;
}

PatternSample dotsPattern(const TileSpec& spec, int tileSalt, int x, int y, int size,
                          uint32_t seed) {
    float cells = 2.0f + (1.0f - spec.scale) * 4.0f;
    float cellSize = size / cells;
    float fx = (x + 0.5f) / cellSize;
    float fy = (y + 0.5f) / cellSize;
    int cx = static_cast<int>(std::floor(fx));
    int cy = static_cast<int>(std::floor(fy));

    float nearest = 8.0f;
    for (int oy = -1; oy <= 1; ++oy) {
        for (int ox = -1; ox <= 1; ++ox) {
            int gx = cx + ox;
            int gy = cy + oy;
            if (gridValue(tileSalt, gx, gy, 53u, seed) > spec.density) continue;
            float jitterX = gx + 0.25f + 0.5f * gridValue(tileSalt, gx, gy, 51u, seed);
            float jitterY = gy + 0.25f + 0.5f * gridValue(tileSalt, gx, gy, 52u, seed);
            float dx = fx - jitterX;
            float dy = fy - jitterY;
            float distance = std::sqrt(dx * dx + dy * dy);
            if (distance < nearest) nearest = distance;
        }
    }

    float radius = 0.16f + spec.scale * 0.30f;
    PatternSample out;
    if (nearest < radius) {
        out.accentWeight = clamp01((1.0f - nearest / radius) * 1.7f);
        out.shade = 1.0f + 0.10f * out.accentWeight;
    }
    return out;
}

PatternSample strataPattern(const TileSpec& spec, int tileSalt, int x, int y, int size,
                            uint32_t seed) {
    float across = spec.vertical ? static_cast<float>(x) : static_cast<float>(y);
    float along = spec.vertical ? static_cast<float>(y) : static_cast<float>(x);
    float bands = 3.0f + spec.scale * 6.0f;
    float warp = (lineNoise(tileSalt, along * 3.0f / size, 59u, seed) - 0.5f) * size * 0.12f;
    float phase = (across + warp) * bands / size;
    float within = phase - std::floor(phase);

    PatternSample out;
    out.accentWeight = within < spec.density ? 1.0f : 0.0f;
    out.shade = 1.0f + 0.07f * std::sin(phase * twoPi);
    if (within < 0.10f) out.shade *= 0.88f;
    return out;
}

PatternSample splatterPattern(const TileSpec& spec, int tileSalt, int x, int y, int size,
                              uint32_t seed) {
    float coarseCells = 2.0f + spec.scale * 3.0f;
    float fineCells = coarseCells * 2.6f;
    float coarse = fractalNoise(tileSalt, x * coarseCells / size, y * coarseCells / size, 61u, seed);
    float fine = fractalNoise(tileSalt, x * fineCells / size, y * fineCells / size, 67u, seed);
    float field = coarse * 0.66f + fine * 0.34f;
    float threshold = 1.0f - spec.density * 0.9f;

    PatternSample out;
    if (field > threshold) {
        out.accentWeight = clamp01((field - threshold) * 6.0f);
    } else if (field > threshold - 0.05f) {
        out.shade = 0.80f;
    }
    out.shade *= 0.95f + 0.10f * fine;
    return out;
}

PatternSample liquidPattern(const TileSpec& spec, int tileSalt, int x, int y, int size,
                            uint32_t seed) {
    float drift = lineNoise(tileSalt, x * 2.0f / size, 71u, seed);
    float crest = std::sin((y * (1.5f + spec.scale * 2.5f) / size + drift * 0.7f) * twoPi);
    float ripple = fractalNoise(tileSalt, x * 3.0f / size, y * 3.0f / size, 73u, seed);

    PatternSample out;
    out.accentWeight = clamp01((0.5f + 0.5f * crest) * spec.density * (0.55f + 0.7f * ripple));
    out.shade = 0.95f + 0.12f * ripple;
    out.hole = out.accentWeight < 0.16f && ripple < 0.50f;
    return out;
}

PatternSample crossPattern(const TileSpec& spec, int tileSalt, int x, int y, int size,
                           uint32_t seed) {
    float centre = (size - 1) * 0.5f;
    PatternSample out;

    if (!spec.vertical) {
        float armHalf = std::max(0.6f, size * (0.05f + spec.scale * 0.09f));
        float dx = std::fabs(x - centre);
        float dy = std::fabs(y - centre);
        if (dx <= armHalf || dy <= armHalf) {
            float edge = std::min(dx, dy) / std::max(1.0f, armHalf);
            out.accentWeight = clamp01(1.0f - edge) * spec.density;
            out.shade = 0.92f + 0.16f * gridValue(tileSalt, x, y, 79u, seed);
            return out;
        }
        out.hole = true;
        out.shade = 0.34f;
        return out;
    }

    float sway = (lineNoise(tileSalt, y * 3.0f / size, 79u, seed) - 0.5f) * size * 0.16f;
    float stalkHalf = std::max(0.6f, size * (0.05f + spec.scale * 0.07f));
    float dx = std::fabs(x - centre - sway);
    if (dx <= stalkHalf) {
        float headDepth = size * 0.30f;
        out.accentWeight = clamp01((headDepth - y) / headDepth);
        out.shade = 1.02f - 0.22f * (dx / std::max(1.0f, stalkHalf));
        return out;
    }

    float leafField = fractalNoise(tileSalt, x * 4.0f / size, y * 4.0f / size, 83u, seed);
    float reach = stalkHalf + size * (0.10f + spec.density * 0.24f) * leafField;
    if (dx <= reach && leafField > 0.42f) {
        out.accentWeight = 1.0f;
        out.shade = 0.90f + 0.20f * leafField;
        return out;
    }
    out.hole = true;
    out.shade = 0.34f;
    return out;
}

PatternSample webPattern(const TileSpec& spec, int tileSalt, int x, int y, int size,
                         uint32_t seed) {
    float centre = (size - 1) * 0.5f;
    float dx = x - centre;
    float dy = y - centre;
    float pixels = std::sqrt(dx * dx + dy * dy);
    float radius = pixels / (size * 0.5f);
    float spokes = 8.0f;
    float angle = std::atan2(dy, dx) / twoPi + 0.5f;
    float spokePhase = angle * spokes;
    float spokeOffset = std::fabs(spokePhase - std::floor(spokePhase + 0.5f));
    float spokeDistance = spokeOffset * (twoPi / spokes) * pixels;

    PatternSample out;
    float rings = 2.0f + spec.scale * 4.0f;
    float ringPhase = radius * rings;
    float ringOffset = std::fabs(ringPhase - std::floor(ringPhase + 0.5f));
    float ringDistance = ringOffset * (size * 0.5f) / rings;

    bool onSpoke = spokeDistance < 0.60f && radius <= 1.02f;
    bool onRing = ringDistance < 0.45f && radius <= 0.98f;
    if (onSpoke || onRing) {
        out.accentWeight = onRing && !onSpoke ? 1.0f : 0.0f;
        out.shade = 0.92f + 0.16f * gridValue(tileSalt, x, y, 89u, seed);
        return out;
    }
    out.hole = true;
    out.shade = 0.30f;
    return out;
}

PatternSample panePattern(const TileSpec& spec, int tileSalt, int x, int y, int size,
                          uint32_t seed) {
    int border = std::max(1, static_cast<int>(std::lround(size * (0.04f + spec.scale * 0.06f))));
    bool onFrame = x < border || y < border || x >= size - border || y >= size - border;
    bool onCorner = (x < border * 3 && y < border * 3) ||
                    (x >= size - border * 3 && y >= size - border * 3);

    PatternSample out;
    if (onFrame || onCorner) {
        out.accentWeight = onCorner && !onFrame ? 1.0f : 0.0f;
        out.shade = 0.94f + 0.12f * gridValue(tileSalt, x, y, 97u, seed);
        return out;
    }
    int quarter = std::max(2, size / 4);
    if (x - y == quarter || y - x == quarter) {
        out.accentWeight = spec.density;
        out.shade = 1.10f;
        return out;
    }
    out.hole = true;
    out.shade = 0.55f;
    return out;
}

PatternSample samplePattern(const TileSpec& spec, int tileSalt, int x, int y, int size,
                            uint32_t seed) {
    switch (spec.pattern) {
        case TilePattern::Solid: return solidPattern(spec, tileSalt, x, y, size, seed);
        case TilePattern::OreSpeckle: return orePattern(spec, tileSalt, x, y, size, seed);
        case TilePattern::Brick: return brickPattern(spec, tileSalt, x, y, size, seed);
        case TilePattern::Plank: return plankPattern(spec, tileSalt, x, y, size, seed);
        case TilePattern::Lattice: return latticePattern(spec, tileSalt, x, y, size, seed);
        case TilePattern::EdgeBand: return edgeBandPattern(spec, tileSalt, x, y, size, seed);
        case TilePattern::GrowthRings: return growthRingsPattern(spec, tileSalt, x, y, size, seed);
        case TilePattern::Dots: return dotsPattern(spec, tileSalt, x, y, size, seed);
        case TilePattern::Strata: return strataPattern(spec, tileSalt, x, y, size, seed);
        case TilePattern::Splatter: return splatterPattern(spec, tileSalt, x, y, size, seed);
        case TilePattern::Liquid: return liquidPattern(spec, tileSalt, x, y, size, seed);
        case TilePattern::Cross: return crossPattern(spec, tileSalt, x, y, size, seed);
        case TilePattern::Web: return webPattern(spec, tileSalt, x, y, size, seed);
        case TilePattern::Pane: return panePattern(spec, tileSalt, x, y, size, seed);
        case TilePattern::Count: break;
    }
    return PatternSample{};
}

float materialGrain(const TileSpec& spec, int tileSalt, int x, int y, int size, uint32_t seed) {
    float amount = spec.noise;
    if (amount <= 0.0f) return 1.0f;

    float shape = 0.0f;
    switch (spec.material) {
        case TileMaterial::Stone: {
            float speck = gridValue(tileSalt, x, y, 101u, seed) * 2.0f - 1.0f;
            float mottle =
                fractalNoise(tileSalt, x * 4.0f / size, y * 4.0f / size, 103u, seed) * 2.0f - 1.0f;
            shape = speck * 0.70f + mottle * 0.55f;
            break;
        }
        case TileMaterial::Soil: {
            float clump =
                fractalNoise(tileSalt, x * 5.0f / size, y * 5.0f / size, 107u, seed) * 2.0f - 1.0f;
            float crumb = gridValue(tileSalt, x, y, 109u, seed) * 2.0f - 1.0f;
            shape = clump * 0.90f + crumb * 0.45f;
            break;
        }
        case TileMaterial::Wood: {
            float across = spec.vertical ? static_cast<float>(x) : static_cast<float>(y);
            float along = spec.vertical ? static_cast<float>(y) : static_cast<float>(x);
            float ring = lineNoise(tileSalt, across * 6.0f / size, 113u, seed) * 2.0f - 1.0f;
            float fibre =
                valueNoise(tileSalt, along * 3.0f / size, across * 9.0f / size, 127u, seed) * 2.0f -
                1.0f;
            shape = ring * 0.65f + fibre * 0.55f;
            break;
        }
        case TileMaterial::Foliage: {
            float cluster = fractalNoise(tileSalt, x * 6.0f / size, y * 6.0f / size, 131u, seed);
            float fleck = gridValue(tileSalt, x, y, 137u, seed) * 2.0f - 1.0f;
            shape = (cluster < 0.44f ? -0.95f : 0.40f) + fleck * 0.45f;
            break;
        }
        case TileMaterial::Metal: {
            float sheen = std::cos((x + y) * 0.5f / size * twoPi) * 0.85f;
            float scratch = gridValue(tileSalt, x, y * 3, 139u, seed) > 0.88f ? -0.95f : 0.0f;
            shape = sheen + scratch + (gridValue(tileSalt, x, y, 149u, seed) * 2.0f - 1.0f) * 0.25f;
            break;
        }
        case TileMaterial::Crystal: {
            float facet =
                valueNoise(tileSalt, (x + y) * 2.5f / size, (x - y) * 2.5f / size, 151u, seed);
            shape = (facet - 0.5f) * 2.4f;
            break;
        }
        case TileMaterial::Fabric: {
            float weave = ((x / 2) + (y / 2)) % 2 == 0 ? 0.75f : -0.75f;
            shape = weave + (gridValue(tileSalt, x, y, 157u, seed) * 2.0f - 1.0f) * 0.40f;
            break;
        }
        case TileMaterial::Fluid: {
            float swell = std::sin((y * 2.0f / size + x * 0.35f / size) * twoPi);
            shape = swell * 0.80f + (gridValue(tileSalt, x, y, 163u, seed) * 2.0f - 1.0f) * 0.35f;
            break;
        }
        case TileMaterial::Light: {
            float half = size * 0.5f;
            float dx = (x + 0.5f - half) / half;
            float dy = (y + 0.5f - half) / half;
            float glow = 1.0f - clamp01(std::sqrt(dx * dx + dy * dy));
            shape = glow * 1.7f - 0.65f + (gridValue(tileSalt, x, y, 167u, seed) * 2.0f - 1.0f) * 0.3f;
            break;
        }
        case TileMaterial::Organic: {
            float soft = fractalNoise(tileSalt, x * 3.0f / size, y * 3.0f / size, 173u, seed);
            float pore = gridValue(tileSalt, x, y, 179u, seed);
            shape = (soft - 0.5f) * 1.9f + (pore > 0.92f ? -0.85f : 0.0f);
            break;
        }
        case TileMaterial::Count: break;
    }
    return std::max(0.0f, 1.0f + amount * shape);
}

}

TileSampler::TileSampler(const TileTextureParams& params) : settings(params) {}

TileSampler::TileSampler(const TileTextureParams& params, const TilePalette& palette)
    : settings(params), table(palette) {}

TilePixel TileSampler::pixelOf(const TileSpec& spec, int tileSalt, int x, int y) const {
    int size = settings.tileSize;
    PatternSample pattern = samplePattern(spec, tileSalt, x, y, size, settings.noiseSeed);
    TileRgb blended = mixRgb(spec.base, spec.accent, pattern.accentWeight);

    float brightness = pattern.shade * materialGrain(spec, tileSalt, x, y, size, settings.noiseSeed);
    if (x == 0 || y == 0) brightness *= 0.86f;

    float red = blended.r * brightness;
    float green = blended.g * brightness;
    float blue = blended.b * brightness;

    red = spec.base.r + (red - spec.base.r) * settings.contrast;
    green = spec.base.g + (green - spec.base.g) * settings.contrast;
    blue = spec.base.b + (blue - spec.base.b) * settings.contrast;

    TilePixel out;
    out.r = toByte(red);
    out.g = toByte(green);
    out.b = toByte(blue);
    out.a = (spec.cutout && pattern.hole) ? 0 : 255;
    return out;
}

TilePixel TileSampler::pixelAt(int tile, int x, int y) const {
    return pixelOf(table.specFor(tile), tile, x, y);
}

void TileSampler::writeTile(int tile, uint8_t* rgba) const {
    const TileSpec& spec = table.specFor(tile);
    int size = settings.tileSize;
    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            TilePixel texel = pixelOf(spec, tile, x, y);
            std::size_t offset = (static_cast<std::size_t>(y) * size + x) * 4;
            rgba[offset + 0] = texel.r;
            rgba[offset + 1] = texel.g;
            rgba[offset + 2] = texel.b;
            rgba[offset + 3] = texel.a;
        }
    }
}

}
