#include <cmath>
#include <cstdint>
#include <fstream>
#include <string>
#include <vector>
 
static void WriteU16(std::ofstream& out, uint16_t v) {
    out.put(static_cast<char>(v & 0xFF));
    out.put(static_cast<char>((v >> 8) & 0xFF));
}
 
static void WriteU32(std::ofstream& out, uint32_t v) {
    out.put(static_cast<char>(v & 0xFF));
    out.put(static_cast<char>((v >> 8) & 0xFF));
    out.put(static_cast<char>((v >> 16) & 0xFF));
    out.put(static_cast<char>((v >> 24) & 0xFF));
}
 
static uint32_t ARGB(uint8_t a, uint8_t r, uint8_t g, uint8_t b) {
    return (static_cast<uint32_t>(a) << 24) | (static_cast<uint32_t>(r) << 16) | (static_cast<uint32_t>(g) << 8) | static_cast<uint32_t>(b);
}
 
static uint32_t Lerp(uint32_t c0, uint32_t c1, float t) {
    auto a0 = static_cast<uint8_t>((c0 >> 24) & 0xFF);
    auto r0 = static_cast<uint8_t>((c0 >> 16) & 0xFF);
    auto g0 = static_cast<uint8_t>((c0 >> 8) & 0xFF);
    auto b0 = static_cast<uint8_t>(c0 & 0xFF);
    auto a1 = static_cast<uint8_t>((c1 >> 24) & 0xFF);
    auto r1 = static_cast<uint8_t>((c1 >> 16) & 0xFF);
    auto g1 = static_cast<uint8_t>((c1 >> 8) & 0xFF);
    auto b1 = static_cast<uint8_t>(c1 & 0xFF);
 
    auto lerp8 = [t](uint8_t x, uint8_t y) -> uint8_t {
        const float v = static_cast<float>(x) + (static_cast<float>(y) - static_cast<float>(x)) * t;
        if (v < 0.0f) return 0;
        if (v > 255.0f) return 255;
        return static_cast<uint8_t>(v + 0.5f);
    };
 
    return ARGB(lerp8(a0, a1), lerp8(r0, r1), lerp8(g0, g1), lerp8(b0, b1));
}
 
int main(int argc, char** argv) {
    if (argc < 2) return 2;
    const std::string outPath = argv[1];
 
    const int w = 64;
    const int h = 64;
 
    // New logo colors: deep purple gradient with glow
    const uint32_t glowRing = ARGB(76, 100, 80, 220);      // Outer breathing glow (30% alpha)
    const uint32_t purpleOuter = ARGB(255, 45, 30, 110);   // Deep indigo
    const uint32_t purpleInner = ARGB(255, 70, 50, 160);   // Vivid purple
    const uint32_t highlight = ARGB(60, 120, 100, 220);    // Top-left 3D highlight
    const uint32_t borderRing = ARGB(120, 140, 120, 255);  // Thin bright border
    const uint32_t arrowColor = ARGB(255, 237, 247, 255);  // Bright cyan-white arrow
 
    std::vector<uint32_t> pixels(static_cast<size_t>(w) * static_cast<size_t>(h), 0);
 
    const float cx = 32.0f;
    const float cy = 32.0f;
    const float rGlow = 33.0f;      // Outer glow ring (110% of main)
    const float rOuter = 30.0f;     // Main circle outer (92% radius)
    const float rInner = 25.5f;     // Inner gradient (78% radius)
    const float rHighlight = 12.5f; // Top-left highlight (38% radius)
    const float rBorder = 30.0f;    // Border ring (same as outer)
 
    // Draw layers from back to front
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            const float fx = static_cast<float>(x) + 0.5f;
            const float fy = static_cast<float>(y) + 0.5f;
            const float dx = fx - cx;
            const float dy = fy - cy;
            const float d = sqrtf(dx * dx + dy * dy);
            
            // Layer 1: Outer glow ring
            if (d <= rGlow) {
                if (d > rOuter) {
                    pixels[static_cast<size_t>(y) * static_cast<size_t>(w) + static_cast<size_t>(x)] = glowRing;
                } else if (d <= rInner) {
                    // Layer 2: Inner purple gradient
                    pixels[static_cast<size_t>(y) * static_cast<size_t>(w) + static_cast<size_t>(x)] = purpleInner;
                } else {
                    // Layer 3: Outer purple gradient
                    pixels[static_cast<size_t>(y) * static_cast<size_t>(w) + static_cast<size_t>(x)] = purpleOuter;
                }
                
                // Layer 4: Top-left highlight for 3D feel
                const float hx = fx - (cx - rOuter * 0.18f);
                const float hy = fy - (cy - rOuter * 0.18f);
                const float hd = sqrtf(hx * hx + hy * hy);
                if (hd <= rHighlight && d <= rOuter) {
                    // Blend highlight over existing color
                    const uint32_t base = pixels[static_cast<size_t>(y) * static_cast<size_t>(w) + static_cast<size_t>(x)];
                    pixels[static_cast<size_t>(y) * static_cast<size_t>(w) + static_cast<size_t>(x)] = Lerp(base, highlight, 0.3f);
                }
            } else {
                // Transparent background
                pixels[static_cast<size_t>(y) * static_cast<size_t>(w) + static_cast<size_t>(x)] = 0;
            }
        }
    }
 
    auto insideTri = [](float px, float py, float ax, float ay, float bx, float by, float cx, float cy) -> bool {
        const float v0x = cx - ax;
        const float v0y = cy - ay;
        const float v1x = bx - ax;
        const float v1y = by - ay;
        const float v2x = px - ax;
        const float v2y = py - ay;
 
        const float dot00 = v0x * v0x + v0y * v0y;
        const float dot01 = v0x * v1x + v0y * v1y;
        const float dot02 = v0x * v2x + v0y * v2y;
        const float dot11 = v1x * v1x + v1y * v1y;
        const float dot12 = v1x * v2x + v1y * v2y;
 
        const float invDen = 1.0f / (dot00 * dot11 - dot01 * dot01);
        const float u = (dot11 * dot02 - dot01 * dot12) * invDen;
        const float v = (dot00 * dot12 - dot01 * dot02) * invDen;
        return (u >= 0.0f) && (v >= 0.0f) && (u + v <= 1.0f);
    };
 
    // Triangle arrow pointing upper-left (like DrawAnimatedCursor)
    // Arrow size: 42% of radius
    const float sz = rOuter * 0.42f;
    const float ax = cx - sz * 0.50f;  // Tip upper-left
    const float ay = cy - sz * 0.55f;
    const float bx = cx - sz * 0.50f;  // Bottom-left
    const float by = cy + sz * 0.60f;
    const float cx2 = cx + sz * 0.55f; // Bottom-right
    const float cy2 = cy + sz * 0.10f;
    
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            const float fx = static_cast<float>(x) + 0.5f;
            const float fy = static_cast<float>(y) + 0.5f;
            if (insideTri(fx, fy, ax, ay, bx, by, cx2, cy2)) {
                pixels[static_cast<size_t>(y) * static_cast<size_t>(w) + static_cast<size_t>(x)] = arrowColor;
            }
        }
    }
    
    // Add thin bright border ring on top
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            const float fx = static_cast<float>(x) + 0.5f;
            const float fy = static_cast<float>(y) + 0.5f;
            const float dx = fx - cx;
            const float dy = fy - cy;
            const float d = sqrtf(dx * dx + dy * dy);
            
            // Draw border ring (1-2 pixel width at radius)
            if (d >= rBorder - 1.0f && d <= rBorder + 0.5f) {
                pixels[static_cast<size_t>(y) * static_cast<size_t>(w) + static_cast<size_t>(x)] = borderRing;
            }
        }
    }
 
    std::ofstream out(outPath, std::ios::binary);
    if (!out.is_open()) return 3;
 
    const uint32_t biSize = 40;
    const uint32_t biWidth = static_cast<uint32_t>(w);
    const uint32_t biHeight = static_cast<uint32_t>(h * 2);
    const uint16_t biPlanes = 1;
    const uint16_t biBitCount = 32;
    const uint32_t biCompression = 0;
    const uint32_t biSizeImage = static_cast<uint32_t>(w * h * 4);
 
    const uint32_t andStride = static_cast<uint32_t>(((w + 31) / 32) * 4);
    const uint32_t andSize = andStride * static_cast<uint32_t>(h);
 
    const uint32_t imageBytes = biSize + biSizeImage + andSize;
    const uint32_t imageOffset = 6 + 16;
 
    WriteU16(out, 0);
    WriteU16(out, 1);
    WriteU16(out, 1);
 
    out.put(static_cast<char>(w));
    out.put(static_cast<char>(h));
    out.put(static_cast<char>(0));
    out.put(static_cast<char>(0));
    WriteU16(out, 1);
    WriteU16(out, 32);
    WriteU32(out, imageBytes);
    WriteU32(out, imageOffset);
 
    WriteU32(out, biSize);
    WriteU32(out, biWidth);
    WriteU32(out, biHeight);
    WriteU16(out, biPlanes);
    WriteU16(out, biBitCount);
    WriteU32(out, biCompression);
    WriteU32(out, biSizeImage);
    WriteU32(out, 0);
    WriteU32(out, 0);
    WriteU32(out, 0);
    WriteU32(out, 0);
 
    for (int y = h - 1; y >= 0; --y) {
        for (int x = 0; x < w; ++x) {
            const uint32_t c = pixels[static_cast<size_t>(y) * static_cast<size_t>(w) + static_cast<size_t>(x)];
            const uint8_t a = static_cast<uint8_t>((c >> 24) & 0xFF);
            const uint8_t r = static_cast<uint8_t>((c >> 16) & 0xFF);
            const uint8_t g = static_cast<uint8_t>((c >> 8) & 0xFF);
            const uint8_t b = static_cast<uint8_t>(c & 0xFF);
            out.put(static_cast<char>(b));
            out.put(static_cast<char>(g));
            out.put(static_cast<char>(r));
            out.put(static_cast<char>(a));
        }
    }
 
    std::vector<uint8_t> andMask(andSize, 0x00);
    out.write(reinterpret_cast<const char*>(andMask.data()), static_cast<std::streamsize>(andMask.size()));
    out.close();
    return 0;
}

