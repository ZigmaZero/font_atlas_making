#include <iostream>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

#include <vector>
#include <unordered_map>
#include <fstream>
#include <cassert>
#include <filesystem>

struct Glyph {
    float u0, v0, u1, v1;
    int width, height;
    int bearingX, bearingY;
    float advance;
};

enum GenerateFontAtlasError {
    GenFontAtlasError_Success = 0,
    GenFontAtlasError_FontFileNotFound,
    GenFontAtlasError_FontFileCorrupted,
    GenFontAtlasError_FontAtlasTooSmall,
};

GenerateFontAtlasError GenerateFontAtlas(
    const char* fontPath,
    float pixelHeight,
    int atlasW,
    int atlasH,
    std::vector<unsigned char>& atlas,
    std::unordered_map<char, Glyph>& glyphs)
{
    // Load font file
    std::ifstream file(fontPath, std::ios::binary);
    if (!file) return GenFontAtlasError_FontFileNotFound;

    std::vector<unsigned char> fontBuffer(
        (std::istreambuf_iterator<char>(file)),
        std::istreambuf_iterator<char>());

    // Init font
    stbtt_fontinfo font;
    if (!stbtt_InitFont(&font, fontBuffer.data(), 0))
        return GenFontAtlasError_FontFileCorrupted;

    float scale = stbtt_ScaleForPixelHeight(&font, pixelHeight);

    int ascent, descent, lineGap;
    stbtt_GetFontVMetrics(&font, &ascent, &descent, &lineGap);

    float baseline = (float) ascent * scale;

    // Allocate atlas (grayscale)
    atlas.assign(atlasW * atlasH, 0);

    int penX = 0;
    int penY = 0;
    int rowHeight = 0;

    for (char c = 32; c < 127; c++)
    {
        int w, h, xoff, yoff;
        unsigned char* bitmap = stbtt_GetCodepointBitmap(
            &font,
            0,
            scale,
            c,
            &w,
            &h,
            &xoff,
            &yoff
        );

        // Metrics are needed even for whitespace characters like space,
        // which may have no visible bitmap.
        int advance, lsb;
        stbtt_GetCodepointHMetrics(&font, c, &advance, &lsb);

        // Handle glyphs with no bitmap (for example, space).
        if (!bitmap || w == 0 || h == 0) {
            Glyph g = {};
            g.u0 = 0.0f;
            g.v0 = 0.0f;
            g.u1 = 0.0f;
            g.v1 = 0.0f;
            g.width = 0;
            g.height = 0;
            g.bearingX = xoff;
            g.bearingY = -yoff;
            g.advance = (float)advance * scale;

            glyphs[c] = g;

            if (bitmap) {
                stbtt_FreeBitmap(bitmap, nullptr);
            }
            continue;
        }

        // Line wrap
        if (penX + w >= atlasW) {
            penX = 0;
            penY += rowHeight;
            rowHeight = 0;
        }

        // Bounds check
        if (penY + h >= atlasH) {
            stbtt_FreeBitmap(bitmap, nullptr);
            return GenFontAtlasError_FontAtlasTooSmall; // atlas too small
        }

        // Copy into atlas
        for (int y = 0; y < h; y++) {
            for (int x = 0; x < w; x++) {
                int dst = (penY + y) * atlasW + (penX + x);
                int src = y * w + x;
                atlas[dst] = bitmap[src];
            }
        }

        // Metrics
        Glyph g = Glyph();
        g.u0 = (float)penX / (float)atlasW;
        g.v0 = (float)penY / (float)atlasH;
        g.u1 = (float)(penX + w) / (float)atlasW;
        g.v1 = (float)(penY + h) / (float)atlasH;

        g.width = w;
        g.height = h;
        g.bearingX = xoff;
        g.bearingY = -yoff;
        g.advance = (float)advance * scale;

        glyphs[c] = g;

        penX += w + 1;
        rowHeight = std::max(rowHeight, h);

        stbtt_FreeBitmap(bitmap, nullptr);
    }

    return GenFontAtlasError_Success;
}

int main() {
    std::cout << std::filesystem::current_path() << std::endl;
    std::vector<unsigned char> atlas;
    std::unordered_map<char, Glyph> glyphs;
    auto result = GenerateFontAtlas("Klub04TT-Normal.ttf", 48.0f, 512, 512, atlas, glyphs);
    if (result == GenFontAtlasError_Success) {
        stbi_write_png("Klub04TT-Normal.png", 512, 512, 1, atlas.data(), 512);

        std::ofstream out("Klub04TT-Normal.txt");

        out << "CapstoneAtlasV1\n";
        out << "atlas " << 512 << " " << 512 << "\n";
        out << "glyphs " << glyphs.size() << "\n";
        out << "char u0 v0 u1 v1 width height bearingX bearingY advance\n";

        for (const auto& [c, g] : glyphs)
        {
            out << static_cast<int>(c) << " "
                << g.u0 << " " << g.v0 << " "
                << g.u1 << " " << g.v1 << " "
                << g.width << " " << g.height << " "
                << g.bearingX << " " << g.bearingY << " "
                << g.advance << "\n";
        }
    }
    else {
        std::cout << "Failed to generate font atlas with code " << result << std::endl;
    }
}