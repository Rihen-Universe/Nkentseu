#include "NkTextRenderer.h"

#include "NKLogger/NkLog.h"
#include "NKRenderer/Core/NkShaderAsset.h"
#include "NKRHI/Core/NkGraphicsApi.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace nkentseu {
    namespace renderer {

        uint64 NkFont::sIDCounter = 1;

        namespace {
            struct DecodedRune {
                uint32 cp = 0;
                uint32 bytes = 0;
            };

            static DecodedRune DecodeUtf8(const char* s, uint32 maxBytes) {
                DecodedRune r{};
                if (!s || maxBytes == 0) return r;
                const uint8 b0 = (uint8)s[0];
                if ((b0 & 0x80u) == 0u) {
                    r.cp = (uint32)b0;
                    r.bytes = 1;
                    return r;
                }
                if ((b0 & 0xE0u) == 0xC0u && maxBytes >= 2) {
                    const uint8 b1 = (uint8)s[1];
                    if ((b1 & 0xC0u) == 0x80u) {
                        r.cp = (uint32)(b0 & 0x1Fu) << 6 | (uint32)(b1 & 0x3Fu);
                        r.bytes = 2;
                        return r;
                    }
                }
                if ((b0 & 0xF0u) == 0xE0u && maxBytes >= 3) {
                    const uint8 b1 = (uint8)s[1];
                    const uint8 b2 = (uint8)s[2];
                    if ((b1 & 0xC0u) == 0x80u && (b2 & 0xC0u) == 0x80u) {
                        r.cp = ((uint32)(b0 & 0x0Fu) << 12)
                             | ((uint32)(b1 & 0x3Fu) << 6)
                             | (uint32)(b2 & 0x3Fu);
                        r.bytes = 3;
                        return r;
                    }
                }
                if ((b0 & 0xF8u) == 0xF0u && maxBytes >= 4) {
                    const uint8 b1 = (uint8)s[1];
                    const uint8 b2 = (uint8)s[2];
                    const uint8 b3 = (uint8)s[3];
                    if ((b1 & 0xC0u) == 0x80u && (b2 & 0xC0u) == 0x80u && (b3 & 0xC0u) == 0x80u) {
                        r.cp = ((uint32)(b0 & 0x07u) << 18)
                             | ((uint32)(b1 & 0x3Fu) << 12)
                             | ((uint32)(b2 & 0x3Fu) << 6)
                             | (uint32)(b3 & 0x3Fu);
                        r.bytes = 4;
                        return r;
                    }
                }
                // Invalid sequence: consume one byte.
                r.cp = 0xFFFDu;
                r.bytes = 1;
                return r;
            }

            static NkColor4f LerpColor(const NkColor4f& a, const NkColor4f& b, float t) {
                const float k = NkClamp(t, 0.0f, 1.0f);
                return {
                    a.r + (b.r - a.r) * k,
                    a.g + (b.g - a.g) * k,
                    a.b + (b.b - a.b) * k,
                    a.a + (b.a - a.a) * k
                };
            }

            static bool ReadFileBytes(const char* path, NkVector<uint8>& out) {
                out.Clear();
                if (!path || !path[0]) return false;
                std::ifstream f(path, std::ios::binary);
                if (!f) return false;
                f.seekg(0, std::ios::end);
                const std::streamoff sz = f.tellg();
                if (sz <= 0) return false;
                f.seekg(0, std::ios::beg);
                out.Resize((usize)sz);
                f.read((char*)out.Data(), sz);
                return f.good();
            }

            struct TextLineView {
                const char* ptr = nullptr;
                uint32 len = 0; // bytes
            };

            static void SplitLinesView(const char* text, NkVector<TextLineView>& out) {
                out.Clear();
                if (!text) return;
                const char* p = text;
                const char* lineStart = p;
                while (*p) {
                    if (*p == '\n') {
                        out.PushBack({lineStart, (uint32)(p - lineStart)});
                        ++p;
                        lineStart = p;
                        continue;
                    }
                    ++p;
                }
                out.PushBack({lineStart, (uint32)(p - lineStart)});
            }

            static std::vector<std::string> WrapTextSimple(const NkFont* font, const char* text,
                                                           float maxWidth, const NkTextStyle& style) {
                std::vector<std::string> lines;
                if (!font || !text || maxWidth <= 0.0f) {
                    if (text) {
                        std::string s(text);
                        size_t start = 0;
                        while (start <= s.size()) {
                            size_t pos = s.find('\n', start);
                            if (pos == std::string::npos) {
                                lines.emplace_back(s.substr(start));
                                break;
                            }
                            lines.emplace_back(s.substr(start, pos - start));
                            start = pos + 1;
                            if (start == s.size()) lines.emplace_back("");
                        }
                    }
                    return lines;
                }

                std::string src(text);
                size_t rowStart = 0;
                while (rowStart <= src.size()) {
                    size_t nl = src.find('\n', rowStart);
                    const std::string row = (nl == std::string::npos)
                        ? src.substr(rowStart)
                        : src.substr(rowStart, nl - rowStart);

                    std::string current;
                    size_t i = 0;
                    while (i < row.size()) {
                        while (i < row.size() && row[i] == ' ') ++i;
                        size_t w0 = i;
                        while (i < row.size() && row[i] != ' ') ++i;
                        const std::string word = row.substr(w0, i - w0);
                        if (word.empty()) break;

                        std::string candidate = current.empty() ? word : (current + " " + word);
                        NkVec2f m = font->MeasureLine(candidate.c_str(), (uint32)candidate.size());
                        if (!candidate.empty()) {
                            const uint32 glyphCount = (uint32)candidate.size();
                            if (glyphCount > 1) {
                                m.x += style.letterSpacing * (float)(glyphCount - 1);
                            }
                        }

                        if (!current.empty() && m.x > maxWidth) {
                            lines.emplace_back(current);
                            current = word;
                        } else {
                            current = candidate;
                        }
                    }
                    lines.emplace_back(current);

                    if (nl == std::string::npos) break;
                    rowStart = nl + 1;
                    if (rowStart == src.size()) lines.emplace_back("");
                }
                return lines;
            }

            static bool MvpEqual(const float a[16], const float b[16]) {
                return std::memcmp(a, b, sizeof(float) * 16) == 0;
            }

            static uint64 MakeTexKey(NkTextureHandle tex, NkSamplerHandle samp) {
                return (tex.id << 1) ^ (samp.id * 0x9E3779B97F4A7C15ull);
            }
        } // namespace

        // =========================================================================
        // NkFont
        // =========================================================================

        bool NkFont::Load(NkIDevice* device, const char* path, uint32 fontSize, uint32 oversample) {
            (void)oversample;
            if (!device || !path || !path[0] || fontSize == 0) return false;

            NkVector<uint8> bytes;
            if (!ReadFileBytes(path, bytes)) {
                logger_src.Infof("[NkTextRenderer] Font load failed: %s\n", path);
                return false;
            }

            if (!LoadFromMemory(device, bytes.Data(), bytes.Size(), fontSize, oversample)) {
                return false;
            }
            mPath = path;
            return true;
        }

        bool NkFont::LoadFromMemory(NkIDevice* device, const uint8* data, usize size,
                                    uint32 fontSize, uint32 oversample) {
            (void)oversample;
            if (!device || !data || size == 0 || fontSize == 0) return false;

            Destroy();

            mLibrary = new NkFTFontLibrary();
            if (!mLibrary || !mLibrary->Init()) {
                if (mLibrary) {
                    delete mLibrary;
                    mLibrary = nullptr;
                }
                return false;
            }

            mFontBytes.Resize(size);
            std::memcpy(mFontBytes.Data(), data, size);
            mFontSize = fontSize;
            mID = { sIDCounter++ };

            mFace = mLibrary->LoadFont(mFontBytes.Data(), mFontBytes.Size(), (uint16)fontSize);
            if (!mFace) {
                mLibrary->Destroy();
                delete mLibrary;
                mLibrary = nullptr;
                mFontBytes.Clear();
                mID = {};
                return false;
            }

            // Preload ASCII by default.
            mGlyphUV.Clear();
            for (uint32 cp = 32; cp <= 126; ++cp) {
                mGlyphUV[cp] = {};
            }
            BuildAtlasFromFace(device);
            return IsValid();
        }

        NkFont* NkFont::CreateSized(NkIDevice* device, uint32 newSize) const {
            if (!device || newSize == 0) return nullptr;
            NkFont* out = new NkFont();
            if (!out) return nullptr;

            bool ok = false;
            if (!mPath.Empty()) {
                ok = out->Load(device, mPath.CStr(), newSize);
            } else if (!mFontBytes.Empty()) {
                ok = out->LoadFromMemory(device, mFontBytes.Data(), mFontBytes.Size(), newSize);
            }

            if (!ok) {
                delete out;
                return nullptr;
            }
            return out;
        }

        void NkFont::Destroy() {
            if (mAtlasTex) {
                delete mAtlasTex;
                mAtlasTex = nullptr;
            }

            if (mLibrary && mFace) {
                mLibrary->FreeFont(mFace);
                mFace = nullptr;
            }
            if (mLibrary) {
                mLibrary->Destroy();
                delete mLibrary;
                mLibrary = nullptr;
            }

            mGlyphUV.Clear();
            mFontBytes.Clear();
            mPath.Clear();
            mFontSize = 24;
            mID = {};
            mScratchGlyph = {};
        }

        NkVec2f NkFont::MeasureLine(const char* text, uint32 len) const {
            if (!text || len == 0) {
                return {0.0f, (float)NkMax(1, GetLineHeight())};
            }

            float x = 0.0f;
            uint32 i = 0;
            while (i < len && text[i]) {
                if (text[i] == '\n') break;
                const DecodedRune r = DecodeUtf8(text + i, len - i);
                if (r.bytes == 0) break;
                i += r.bytes;

                NkVec2f uvMin{}, uvMax{}, bearing{};
                int32 xAdvance = 0;
                uint32 w = 0, h = 0;
                if (!GetGlyphUV(r.cp, uvMin, uvMax, bearing, xAdvance, w, h)) {
                    if (!GetGlyphUV((uint32)'?', uvMin, uvMax, bearing, xAdvance, w, h)) {
                        continue;
                    }
                }
                x += (float)xAdvance;
            }

            return {x, (float)NkMax(1, GetLineHeight())};
        }

        NkVec2f NkFont::MeasureText(const char* text, const NkTextStyle& style) const {
            if (!text || !text[0]) return {0.0f, 0.0f};

            NkVector<TextLineView> lines;
            SplitLinesView(text, lines);

            float maxW = 0.0f;
            for (uint32 li = 0; li < (uint32)lines.Size(); ++li) {
                const TextLineView& L = lines[li];
                NkVec2f m = MeasureLine(L.ptr, L.len);
                // Letter spacing per glyph (approx by bytes for simple cases).
                if (L.len > 1 && style.letterSpacing != 0.0f) {
                    m.x += style.letterSpacing * (float)(L.len - 1);
                }
                if (m.x > maxW) maxW = m.x;
            }

            const float lineH = (float)NkMax(1, GetLineHeight()) * NkMax(0.1f, style.lineSpacing);
            float totalH = lineH * (float)NkMax<uint32>(1u, (uint32)lines.Size());

            if (style.outlineWidth > 0.0f) {
                maxW += style.outlineWidth * 2.0f;
                totalH += style.outlineWidth * 2.0f;
            }
            if (style.shadow) {
                maxW += NkFabs(style.shadowOffset.x);
                totalH += NkFabs(style.shadowOffset.y);
            }
            return {maxW, totalH};
        }

        const NkFTGlyph* NkFont::GetGlyph(uint32 codepoint) const {
            if (!mFace) return nullptr;
            NkFTGlyph g{};
            if (!mFace->GetGlyph(codepoint, g) || !g.valid) return nullptr;
            mScratchGlyph = g;
            return &mScratchGlyph;
        }

        bool NkFont::GetGlyphUV(uint32 codepoint, NkVec2f& uvMin, NkVec2f& uvMax,
                                NkVec2f& bearing, int32& xAdvance, uint32& w, uint32& h) const {
            if (const GlyphUV* g = mGlyphUV.Find(codepoint)) {
                uvMin = g->uvMin;
                uvMax = g->uvMax;
                bearing = g->bearing;
                xAdvance = g->xAdvance;
                w = g->w;
                h = g->h;
                return true;
            }
            if (const GlyphUV* fb = mGlyphUV.Find((uint32)'?')) {
                uvMin = fb->uvMin;
                uvMax = fb->uvMax;
                bearing = fb->bearing;
                xAdvance = fb->xAdvance;
                w = fb->w;
                h = fb->h;
                return true;
            }
            return false;
        }

        NkSamplerHandle NkFont::GetSampler() const {
            return (mAtlasTex && mAtlasTex->IsValid()) ? mAtlasTex->GetSampler() : NkSamplerHandle{};
        }

        bool NkFont::AddGlyphRange(NkIDevice* device, uint32 start, uint32 end) {
            if (!mFace || !device) return false;
            if (start > end) {
                const uint32 t = start;
                start = end;
                end = t;
            }
            for (uint32 cp = start; cp <= end; ++cp) {
                if (!mGlyphUV.Contains(cp)) {
                    mGlyphUV[cp] = {};
                }
            }
            return RebuildAtlas(device);
        }

        bool NkFont::RebuildAtlas(NkIDevice* device) {
            if (!mFace || !device) return false;
            BuildAtlasFromFace(device);
            return IsValid();
        }

        void NkFont::BuildAtlasFromFace(NkIDevice* device) {
            if (!device || !mFace) return;

            NkVector<uint32> cps;
            cps.Reserve(NkMax<usize>(mGlyphUV.Size(), 96));
            if (mGlyphUV.Empty()) {
                for (uint32 cp = 32; cp <= 126; ++cp) cps.PushBack(cp);
            } else {
                mGlyphUV.ForEach([&](const uint32& cp, const GlyphUV&) {
                    cps.PushBack(cp);
                });
            }
            std::sort(cps.Begin(), cps.End());
            cps.Erase(std::unique(cps.Begin(), cps.End()), cps.End());

            struct CpuGlyph {
                uint32 cp = 0;
                int32 bearingX = 0;
                int32 bearingY = 0;
                int32 xAdvance = 0;
                uint32 w = 0;
                uint32 h = 0;
                NkVector<uint8> alpha;
                int32 px = 0;
                int32 py = 0;
            };

            NkVector<CpuGlyph> glyphs;
            glyphs.Reserve(cps.Size());
            uint64 totalArea = 0;

            for (uint32 i = 0; i < (uint32)cps.Size(); ++i) {
                NkFTGlyph g{};
                if (!mFace->GetGlyph(cps[i], g) || !g.valid) {
                    continue;
                }

                CpuGlyph cg{};
                cg.cp = cps[i];
                cg.bearingX = g.bearingX;
                cg.bearingY = g.bearingY;
                cg.xAdvance = g.xAdvance;
                cg.w = (uint32)NkMax(0, g.width);
                cg.h = (uint32)NkMax(0, g.height);
                if (cg.w > 0 && cg.h > 0 && g.bitmap) {
                    cg.alpha.Resize((usize)cg.w * (usize)cg.h);
                    const int32 srcPitch = g.pitch;
                    for (uint32 y = 0; y < cg.h; ++y) {
                        const uint8* srcRow = nullptr;
                        if (srcPitch >= 0) {
                            srcRow = g.bitmap + (usize)y * (usize)srcPitch;
                        } else {
                            srcRow = g.bitmap + (usize)(cg.h - 1u - y) * (usize)(-srcPitch);
                        }
                        uint8* dstRow = cg.alpha.Data() + (usize)y * (usize)cg.w;
                        std::memcpy(dstRow, srcRow, cg.w);
                    }
                    totalArea += (uint64)(cg.w + 2u) * (uint64)(cg.h + 2u);
                }
                glyphs.PushBack(cg);
            }

            if (glyphs.Empty()) return;

            uint32 atlasSize = 256u;
            while ((uint64)atlasSize * (uint64)atlasSize < NkMax<uint64>(totalArea, 1024u) && atlasSize < 4096u) {
                atlasSize *= 2u;
            }

            bool packed = false;
            for (uint32 attempt = 0; attempt < 6 && !packed; ++attempt) {
                int32 x = 1;
                int32 y = 1;
                int32 rowH = 0;
                packed = true;
                for (uint32 i = 0; i < (uint32)glyphs.Size(); ++i) {
                    CpuGlyph& g = glyphs[i];
                    if (g.w == 0 || g.h == 0) continue;
                    if (x + (int32)g.w + 1 > (int32)atlasSize) {
                        x = 1;
                        y += rowH + 1;
                        rowH = 0;
                    }
                    if (y + (int32)g.h + 1 > (int32)atlasSize) {
                        packed = false;
                        break;
                    }
                    g.px = x;
                    g.py = y;
                    x += (int32)g.w + 1;
                    rowH = NkMax(rowH, (int32)g.h);
                }
                if (!packed) {
                    atlasSize = NkMin<uint32>(atlasSize * 2u, 4096u);
                }
            }
            if (!packed) {
                logger_src.Infof("[NkTextRenderer] Atlas packing failed for font '%s'\n", mPath.CStr());
                return;
            }

            NkVector<uint8> pixels;
            pixels.Resize((usize)atlasSize * (usize)atlasSize * 4u);
            std::memset(pixels.Data(), 0, pixels.Size());

            NkUnorderedMap<uint32, GlyphUV> newUV;
            for (uint32 i = 0; i < (uint32)glyphs.Size(); ++i) {
                const CpuGlyph& g = glyphs[i];
                GlyphUV uv{};
                uv.bearing = {(float)g.bearingX, (float)g.bearingY};
                uv.xAdvance = g.xAdvance;
                uv.w = g.w;
                uv.h = g.h;

                if (g.w > 0 && g.h > 0) {
                    for (uint32 yy = 0; yy < g.h; ++yy) {
                        for (uint32 xx = 0; xx < g.w; ++xx) {
                            const uint8 a = g.alpha[(usize)yy * (usize)g.w + (usize)xx];
                            const usize di = (((usize)g.py + (usize)yy) * (usize)atlasSize + ((usize)g.px + (usize)xx)) * 4u;
                            pixels[di + 0] = 255;
                            pixels[di + 1] = 255;
                            pixels[di + 2] = 255;
                            pixels[di + 3] = a;
                        }
                    }
                    uv.uvMin = {(float)g.px / (float)atlasSize, (float)g.py / (float)atlasSize};
                    uv.uvMax = {(float)(g.px + (int32)g.w) / (float)atlasSize,
                                (float)(g.py + (int32)g.h) / (float)atlasSize};
                } else {
                    uv.uvMin = {0.0f, 0.0f};
                    uv.uvMax = {0.0f, 0.0f};
                }
                newUV[g.cp] = uv;
            }

            if (!mAtlasTex) mAtlasTex = new NkTexture2D();
            if (!mAtlasTex) return;

            NkTextureParams p = NkTextureParams::Linear();
            p.generateMips = false;
            p.srgb = false;
            p.wrapU = NkAddressMode::NK_CLAMP_TO_EDGE;
            p.wrapV = NkAddressMode::NK_CLAMP_TO_EDGE;
            p.anisotropy = 1.0f;

            if (!mAtlasTex->LoadFromMemory(device, pixels.Data(), atlasSize, atlasSize,
                                           NkGPUFormat::NK_RGBA8_UNORM, p)) {
                logger_src.Infof("[NkTextRenderer] Failed to upload glyph atlas\n");
                return;
            }

            mGlyphUV = newUV;
        }

        // =========================================================================
        // Text mesh generation
        // =========================================================================

        NkTextMesh BuildTextMesh(const NkFont* font, const char* text,
                                 const NkTextStyle& style, NkTextAlign align, float maxWidth) {
            NkTextMesh out{};
            if (!font || !font->IsValid() || !text || !text[0]) return out;

            const std::vector<std::string> lines = (maxWidth > 0.0f)
                ? WrapTextSimple(font, text, maxWidth, style)
                : WrapTextSimple(font, text, 0.0f, style);
            if (lines.empty()) return out;

            const float lineH = (float)NkMax(1, font->GetLineHeight()) * NkMax(0.1f, style.lineSpacing);
            const float asc = (float)font->GetAscender();

            out.lineCount = (uint32)lines.size();
            out.vertices.Clear();
            out.indices.Clear();
            out.vertices.Reserve(lines.size() * 64u);
            out.indices.Reserve(lines.size() * 96u);

            float maxW = 0.0f;
            for (size_t li = 0; li < lines.size(); ++li) {
                const std::string& L = lines[li];
                NkVec2f m = font->MeasureLine(L.c_str(), (uint32)L.size());
                const float lineWidth = m.x + ((L.size() > 1) ? style.letterSpacing * (float)(L.size() - 1) : 0.0f);
                maxW = NkMax(maxW, lineWidth);

                float x = 0.0f;
                if (align == NkTextAlign::NK_CENTER) x = -lineWidth * 0.5f;
                else if (align == NkTextAlign::NK_RIGHT) x = -lineWidth;

                const float baseline = asc + (float)li * lineH;
                uint32 p = 0;
                while (p < (uint32)L.size()) {
                    const DecodedRune r = DecodeUtf8(L.c_str() + p, (uint32)L.size() - p);
                    if (r.bytes == 0) break;
                    p += r.bytes;

                    NkVec2f uvMin{}, uvMax{}, bearing{};
                    int32 xAdv = 0;
                    uint32 w = 0, h = 0;
                    if (!font->GetGlyphUV(r.cp, uvMin, uvMax, bearing, xAdv, w, h)) {
                        if (!font->GetGlyphUV((uint32)'?', uvMin, uvMax, bearing, xAdv, w, h)) {
                            continue;
                        }
                    }

                    if (w > 0 && h > 0) {
                        const float x0 = x + bearing.x;
                        const float y0 = baseline - bearing.y;
                        const float x1 = x0 + (float)w;
                        const float y1 = y0 + (float)h;

                        const float t = (lineH > 1e-4f) ? (y0 / (lineH * (float)NkMax<size_t>(1, lines.size()))) : 0.0f;
                        const NkColor4f cTop = style.gradient ? LerpColor(style.gradientTop, style.gradientBot, t) : style.color;
                        const NkColor4f cBot = style.gradient ? LerpColor(style.gradientTop, style.gradientBot, t + (float)h / NkMax(1.0f, lineH)) : style.color;

                        const uint32 base = (uint32)out.vertices.Size();
                        out.vertices.PushBack({{x0, y0}, {uvMin.x, uvMin.y}, cTop});
                        out.vertices.PushBack({{x1, y0}, {uvMax.x, uvMin.y}, cTop});
                        out.vertices.PushBack({{x1, y1}, {uvMax.x, uvMax.y}, cBot});
                        out.vertices.PushBack({{x0, y1}, {uvMin.x, uvMax.y}, cBot});
                        out.indices.PushBack(base + 0);
                        out.indices.PushBack(base + 1);
                        out.indices.PushBack(base + 2);
                        out.indices.PushBack(base + 2);
                        out.indices.PushBack(base + 3);
                        out.indices.PushBack(base + 0);
                    }

                    x += (float)xAdv + style.letterSpacing + (style.bold ? 1.0f : 0.0f);
                }
            }

            out.bounds = {maxW, lineH * (float)lines.size()};
            return out;
        }

        NkMeshData BuildExtruded3DText(const NkFont* font, const char* text,
                                       const NkText3DParams& params) {
            NkMeshData md{};
            md.topology = NkMeshTopology::NK_TRIANGLES;
            if (!font || !font->IsValid() || !text || !text[0]) return md;

            const float depth = NkMax(0.001f, params.extrude);
            const float lineH = (float)NkMax(1, font->GetLineHeight());
            const float asc = (float)font->GetAscender();

            auto addV = [&](float x, float y, float z, const NkVec3f& n, NkVec2f uv) -> uint32 {
                NkVertex3D v{};
                v.position = {x, y, z};
                v.normal = n;
                v.tangent = {1, 0, 0};
                v.bitangent = {0, 1, 0};
                v.uv0 = uv;
                v.color = {1, 1, 1, 1};
                md.vertices.PushBack(v);
                return (uint32)md.vertices.Size() - 1u;
            };

            float penX = 0.0f;
            float baseline = 0.0f + asc;
            const uint32 nBytes = (uint32)std::strlen(text);
            uint32 p = 0;

            while (p < nBytes) {
                if (text[p] == '\n') {
                    penX = 0.0f;
                    baseline -= lineH;
                    ++p;
                    continue;
                }

                const DecodedRune r = DecodeUtf8(text + p, nBytes - p);
                if (r.bytes == 0) break;
                p += r.bytes;

                NkVec2f uvMin{}, uvMax{}, bearing{};
                int32 xAdv = 0;
                uint32 w = 0, h = 0;
                if (!font->GetGlyphUV(r.cp, uvMin, uvMax, bearing, xAdv, w, h)) {
                    if (!font->GetGlyphUV((uint32)'?', uvMin, uvMax, bearing, xAdv, w, h)) {
                        continue;
                    }
                }

                if (w > 0 && h > 0) {
                    const float x0 = penX + bearing.x;
                    const float x1 = x0 + (float)w;
                    const float yTop = baseline + bearing.y;
                    const float yBot = yTop - (float)h;
                    const float zF = 0.0f;
                    const float zB = -depth;

                    if (params.front) {
                        const uint32 i0 = addV(x0, yBot, zF, {0, 0, 1}, {uvMin.x, uvMax.y});
                        const uint32 i1 = addV(x1, yBot, zF, {0, 0, 1}, {uvMax.x, uvMax.y});
                        const uint32 i2 = addV(x1, yTop, zF, {0, 0, 1}, {uvMax.x, uvMin.y});
                        const uint32 i3 = addV(x0, yTop, zF, {0, 0, 1}, {uvMin.x, uvMin.y});
                        md.indices.PushBack(i0); md.indices.PushBack(i1); md.indices.PushBack(i2);
                        md.indices.PushBack(i2); md.indices.PushBack(i3); md.indices.PushBack(i0);
                    }

                    if (params.back) {
                        const uint32 i0 = addV(x0, yBot, zB, {0, 0, -1}, {uvMin.x, uvMax.y});
                        const uint32 i1 = addV(x1, yBot, zB, {0, 0, -1}, {uvMax.x, uvMax.y});
                        const uint32 i2 = addV(x1, yTop, zB, {0, 0, -1}, {uvMax.x, uvMin.y});
                        const uint32 i3 = addV(x0, yTop, zB, {0, 0, -1}, {uvMin.x, uvMin.y});
                        md.indices.PushBack(i2); md.indices.PushBack(i1); md.indices.PushBack(i0);
                        md.indices.PushBack(i0); md.indices.PushBack(i3); md.indices.PushBack(i2);
                    }

                    if (params.sides) {
                        auto addSide = [&](NkVec3f a, NkVec3f b, NkVec3f c, NkVec3f d, NkVec3f n) {
                            const uint32 i0 = addV(a.x, a.y, a.z, n, {0, 0});
                            const uint32 i1 = addV(b.x, b.y, b.z, n, {1, 0});
                            const uint32 i2 = addV(c.x, c.y, c.z, n, {1, 1});
                            const uint32 i3 = addV(d.x, d.y, d.z, n, {0, 1});
                            md.indices.PushBack(i0); md.indices.PushBack(i1); md.indices.PushBack(i2);
                            md.indices.PushBack(i2); md.indices.PushBack(i3); md.indices.PushBack(i0);
                        };

                        addSide({x0, yBot, zF}, {x0, yTop, zF}, {x0, yTop, zB}, {x0, yBot, zB}, {-1, 0, 0});
                        addSide({x1, yTop, zF}, {x1, yBot, zF}, {x1, yBot, zB}, {x1, yTop, zB}, {1, 0, 0});
                        addSide({x0, yTop, zF}, {x1, yTop, zF}, {x1, yTop, zB}, {x0, yTop, zB}, {0, 1, 0});
                        addSide({x1, yBot, zF}, {x0, yBot, zF}, {x0, yBot, zB}, {x1, yBot, zB}, {0, -1, 0});
                    }
                }

                penX += (float)xAdv + params.charSpacing;
            }

            if (!md.vertices.Empty()) {
                NkSubMesh sm{};
                sm.firstIndex = 0;
                sm.indexCount = (uint32)md.indices.Size();
                sm.firstVertex = 0;
                sm.vertexCount = (uint32)md.vertices.Size();
                sm.materialIdx = 0;
                sm.name = "Text3D";
                md.subMeshes.PushBack(sm);
                md.RecalculateAABB();
            }
            return md;
        }

        // =========================================================================
        // NkTextRenderer
        // =========================================================================

        bool NkTextRenderer::Init(NkIDevice* device, NkRenderPassHandle renderPass, uint32 width, uint32 height) {
            Shutdown();
            if (!device || !device->IsValid() || !renderPass.IsValid() || width == 0 || height == 0) {
                return false;
            }

            mDevice = device;
            mRenderPass = renderPass;
            mWidth = width;
            mHeight = height;

            if (!InitBuffers()) { Shutdown(); return false; }
            if (!InitDescriptors()) { Shutdown(); return false; }
            if (!InitShaders(renderPass)) { Shutdown(); return false; }

            mDefaultFont = NkFontLibrary::Get().GetDefault(24);
            mIsValid = true;
            return true;
        }

        void NkTextRenderer::Shutdown() {
            if (!mDevice) {
                mBatches.Clear();
                mCurrentBatch = nullptr;
                mFonts.Clear();
                mFontDescSets.Clear();
                mDefaultFont = nullptr;
                mCmd = nullptr;
                mIsValid = false;
                mInFrame = false;
                return;
            }

            mFontDescSets.ForEach([&](const uint64&, NkDescSetHandle set) {
                if (set.IsValid()) mDevice->FreeDescriptorSet(set);
            });
            mFontDescSets.Clear();

            if (mPipeline.IsValid()) mDevice->DestroyPipeline(mPipeline);
            if (mShader.IsValid()) mDevice->DestroyShader(mShader);
            if (mLayout.IsValid()) mDevice->DestroyDescriptorSetLayout(mLayout);
            if (mUBO.IsValid()) mDevice->DestroyBuffer(mUBO);
            if (mIBO.IsValid()) mDevice->DestroyBuffer(mIBO);
            if (mVBO.IsValid()) mDevice->DestroyBuffer(mVBO);

            mPipeline = {};
            mShader = {};
            mLayout = {};
            mUBO = {};
            mIBO = {};
            mVBO = {};

            mBatches.Clear();
            mCurrentBatch = nullptr;
            mFonts.Clear();
            mFontDescSets.Clear();
            mDefaultFont = nullptr;
            mCmd = nullptr;
            mDevice = nullptr;
            mRenderPass = {};
            mWidth = mHeight = 0;
            mIsValid = false;
            mInFrame = false;
        }

        void NkTextRenderer::Resize(uint32 width, uint32 height) {
            if (width == 0 || height == 0) return;
            mWidth = width;
            mHeight = height;
        }

        NkFont* NkTextRenderer::LoadFont(const char* path, uint32 size) {
            NkFont* f = NkFontLibrary::Get().Load(path, size);
            if (!f) return nullptr;
            bool already = false;
            for (const auto* it : mFonts) {
                if (it == f) { already = true; break; }
            }
            if (!already) mFonts.PushBack(f);
            if (!mDefaultFont) mDefaultFont = f;
            return f;
        }

        void NkTextRenderer::Begin(NkICommandBuffer* cmd) {
            if (!mIsValid || !cmd) return;
            if (mInFrame) End();
            mCmd = cmd;
            mInFrame = true;
            mBatches.Clear();
            mCurrentBatch = nullptr;
        }

        void NkTextRenderer::End() {
            if (!mInFrame) return;
            FlushBatches();
            mCmd = nullptr;
            mInFrame = false;
            mCurrentBatch = nullptr;
        }

        void NkTextRenderer::DrawText2D(NkFont* font, const char* text, float x, float y,
                                        const NkTextStyle& style, NkTextAlign align,
                                        NkTextVAlign valign, float depth) {
            (void)depth;
            if (!mInFrame || !text || !text[0]) return;
            if (!font) font = mDefaultFont ? mDefaultFont : NkFontLibrary::Get().GetDefault(24);
            if (!font || !font->IsValid()) return;

            NkVector<TextLineView> lines;
            SplitLinesView(text, lines);
            if (lines.Empty()) return;

            const float lineH = (float)NkMax(1, font->GetLineHeight()) * NkMax(0.1f, style.lineSpacing);
            const float asc = (float)font->GetAscender();

            float blockW = 0.0f;
            for (uint32 i = 0; i < (uint32)lines.Size(); ++i) {
                NkVec2f m = font->MeasureLine(lines[i].ptr, lines[i].len);
                if (lines[i].len > 1) m.x += style.letterSpacing * (float)(lines[i].len - 1);
                blockW = NkMax(blockW, m.x);
            }
            const float blockH = lineH * (float)NkMax<uint32>(1u, (uint32)lines.Size());

            float xBase = x;
            if (align == NkTextAlign::NK_CENTER) xBase -= blockW * 0.5f;
            else if (align == NkTextAlign::NK_RIGHT) xBase -= blockW;

            float yBase = y;
            if (valign == NkTextVAlign::NK_MIDDLE) yBase -= blockH * 0.5f;
            else if (valign == NkTextVAlign::NK_BOTTOM) yBase -= blockH;
            else if (valign == NkTextVAlign::NK_BASELINE) yBase -= asc;

            auto emitPass = [&](float ox, float oy, const NkColor4f& c, bool gradient) {
                StartBatch(font, false, nullptr);
                for (uint32 li = 0; li < (uint32)lines.Size(); ++li) {
                    const TextLineView& L = lines[li];
                    NkVec2f m = font->MeasureLine(L.ptr, L.len);
                    const float lineW = m.x + ((L.len > 1) ? style.letterSpacing * (float)(L.len - 1) : 0.0f);
                    float penX = xBase + ox;
                    if (align == NkTextAlign::NK_CENTER) penX += (blockW - lineW) * 0.5f;
                    else if (align == NkTextAlign::NK_RIGHT) penX += (blockW - lineW);
                    float baseline = yBase + oy + asc + (float)li * lineH;

                    uint32 p = 0;
                    while (p < L.len) {
                        const DecodedRune r = DecodeUtf8(L.ptr + p, L.len - p);
                        if (r.bytes == 0) break;
                        p += r.bytes;

                        NkVec2f uvMin{}, uvMax{}, bearing{};
                        int32 xAdv = 0;
                        uint32 gw = 0, gh = 0;
                        if (!font->GetGlyphUV(r.cp, uvMin, uvMax, bearing, xAdv, gw, gh)) {
                            if (!font->GetGlyphUV((uint32)'?', uvMin, uvMax, bearing, xAdv, gw, gh)) {
                                continue;
                            }
                        }

                        if (gw > 0 && gh > 0 && mCurrentBatch) {
                            const float x0 = penX + bearing.x;
                            const float y0 = baseline - bearing.y;
                            const float x1 = x0 + (float)gw;
                            const float y1 = y0 + (float)gh;
                            const uint32 base = (uint32)mCurrentBatch->verts.Size();

                            const NkColor4f cTop = gradient
                                ? LerpColor(style.gradientTop, style.gradientBot, (float)li / NkMax(1.0f, (float)lines.Size() - 1.0f))
                                : c;
                            const NkColor4f cBot = gradient
                                ? LerpColor(style.gradientTop, style.gradientBot, ((float)li + 0.5f) / NkMax(1.0f, (float)lines.Size()))
                                : c;

                            mCurrentBatch->verts.PushBack({x0, y0, uvMin.x, uvMin.y, cTop.r, cTop.g, cTop.b, cTop.a});
                            mCurrentBatch->verts.PushBack({x1, y0, uvMax.x, uvMin.y, cTop.r, cTop.g, cTop.b, cTop.a});
                            mCurrentBatch->verts.PushBack({x1, y1, uvMax.x, uvMax.y, cBot.r, cBot.g, cBot.b, cBot.a});
                            mCurrentBatch->verts.PushBack({x0, y1, uvMin.x, uvMax.y, cBot.r, cBot.g, cBot.b, cBot.a});
                            mCurrentBatch->indices.PushBack(base + 0);
                            mCurrentBatch->indices.PushBack(base + 1);
                            mCurrentBatch->indices.PushBack(base + 2);
                            mCurrentBatch->indices.PushBack(base + 2);
                            mCurrentBatch->indices.PushBack(base + 3);
                            mCurrentBatch->indices.PushBack(base + 0);
                        }

                        penX += (float)xAdv + style.letterSpacing + (style.bold ? 1.0f : 0.0f);
                    }
                }
            };

            if (style.shadow) {
                emitPass(style.shadowOffset.x, style.shadowOffset.y, style.shadowColor, false);
            }

            if (style.outlineWidth > 0.0f) {
                const float o = style.outlineWidth;
                const NkColor4f oc = style.outlineColor;
                emitPass(-o,  0, oc, false);
                emitPass( o,  0, oc, false);
                emitPass( 0, -o, oc, false);
                emitPass( 0,  o, oc, false);
                emitPass(-o, -o, oc, false);
                emitPass( o, -o, oc, false);
                emitPass(-o,  o, oc, false);
                emitPass( o,  o, oc, false);
            }

            emitPass(0.0f, 0.0f, style.color, style.gradient);
        }

        void NkTextRenderer::DrawText2DWrapped(NkFont* font, const char* text,
                                               float x, float y, float maxWidth,
                                               const NkTextStyle& style, NkTextAlign align,
                                               float depth) {
            if (!font) font = mDefaultFont ? mDefaultFont : NkFontLibrary::Get().GetDefault(24);
            if (!font || !text) return;

            const std::vector<std::string> lines = WrapTextSimple(font, text, maxWidth, style);
            if (lines.empty()) return;

            const float lineH = (float)NkMax(1, font->GetLineHeight()) * NkMax(0.1f, style.lineSpacing);
            for (size_t i = 0; i < lines.size(); ++i) {
                DrawText2D(font, lines[i].c_str(), x, y + (float)i * lineH, style, align, NkTextVAlign::NK_TOP, depth);
            }
        }

        NkVec2f NkTextRenderer::MeasureText2D(NkFont* font, const char* text, const NkTextStyle& style) const {
            if (!font) font = mDefaultFont;
            if (!font || !text) return {0, 0};
            return font->MeasureText(text, style);
        }

        void NkTextRenderer::DrawText3DBillboard(NkFont* font, const char* text,
                                                 const NkVec3f& worldPos,
                                                 const NkMat4f& view, const NkMat4f& proj,
                                                 float worldSize, const NkTextStyle& style,
                                                 NkTextAlign align) {
            if (!mInFrame || !text || !text[0]) return;
            if (!font) font = mDefaultFont ? mDefaultFont : NkFontLibrary::Get().GetDefault(24);
            if (!font || !font->IsValid()) return;

            const float scale = NkMax(1e-6f, worldSize / (float)NkMax(1u, font->GetFontSize()));
            const float lineH = (float)NkMax(1, font->GetLineHeight()) * NkMax(0.1f, style.lineSpacing) * scale;
            const float asc = (float)font->GetAscender() * scale;

            float mvp[16];
            BuildBillboardMVP(worldPos, 1.0f, 1.0f, view, proj, mvp);
            StartBatch(font, true, mvp);
            if (!mCurrentBatch) return;

            NkVector<TextLineView> lines;
            SplitLinesView(text, lines);
            for (uint32 li = 0; li < (uint32)lines.Size(); ++li) {
                const TextLineView& L = lines[li];
                NkVec2f lm = font->MeasureLine(L.ptr, L.len);
                float lineW = lm.x * scale;
                if (L.len > 1) lineW += style.letterSpacing * (float)(L.len - 1) * scale;

                float penX = 0.0f;
                if (align == NkTextAlign::NK_CENTER) penX -= lineW * 0.5f;
                else if (align == NkTextAlign::NK_RIGHT) penX -= lineW;
                float baseline = asc + (float)li * lineH;

                uint32 p = 0;
                while (p < L.len) {
                    const DecodedRune r = DecodeUtf8(L.ptr + p, L.len - p);
                    if (r.bytes == 0) break;
                    p += r.bytes;

                    PushGlyph(font, r.cp, penX, baseline, scale, style.color);

                    NkVec2f uvMin{}, uvMax{}, bearing{};
                    int32 xAdv = 0;
                    uint32 gw = 0, gh = 0;
                    if (font->GetGlyphUV(r.cp, uvMin, uvMax, bearing, xAdv, gw, gh)
                     || font->GetGlyphUV((uint32)'?', uvMin, uvMax, bearing, xAdv, gw, gh)) {
                        penX += ((float)xAdv + style.letterSpacing + (style.bold ? 1.0f : 0.0f)) * scale;
                    }
                }
            }
        }

        void NkTextRenderer::DrawText3DAnchored(NkFont* font, const char* text,
                                                const NkVec3f& worldPos, const NkMat4f& viewProj,
                                                float worldSize, const NkTextStyle& style,
                                                NkTextAlign align, bool clipBehindCamera) {
            if (!mInFrame || !text || !text[0]) return;
            if (!font) font = mDefaultFont ? mDefaultFont : NkFontLibrary::Get().GetDefault(24);
            if (!font || !font->IsValid()) return;

            const NkVec4f p = viewProj * NkVec4f(worldPos.x, worldPos.y, worldPos.z, 1.0f);
            if (clipBehindCamera && p.w <= 0.0f) return;
            const float invW = (NkFabs(p.w) > 1e-6f) ? (1.0f / p.w) : 1.0f;
            const float ndcX = p.x * invW;
            const float ndcY = p.y * invW;

            const float sx = (ndcX * 0.5f + 0.5f) * (float)mWidth;
            const float sy = (ndcY * 0.5f + 0.5f) * (float)mHeight;

            // Estimate screen size from a world-space offset along +X.
            const NkVec4f px = viewProj * NkVec4f(worldPos.x + worldSize, worldPos.y, worldPos.z, 1.0f);
            float scale = 1.0f;
            if (NkFabs(px.w) > 1e-6f) {
                const float ndcX2 = px.x / px.w;
                const float pixelSpan = NkFabs((ndcX2 - ndcX) * 0.5f * (float)mWidth);
                scale = pixelSpan / (float)NkMax(1u, font->GetFontSize());
            }
            scale = NkClamp(scale, 0.05f, 64.0f);

            NkVector<TextLineView> lines;
            SplitLinesView(text, lines);
            const float lineH = (float)NkMax(1, font->GetLineHeight()) * NkMax(0.1f, style.lineSpacing) * scale;
            const float asc = (float)font->GetAscender() * scale;

            StartBatch(font, false, nullptr);
            if (!mCurrentBatch) return;

            for (uint32 li = 0; li < (uint32)lines.Size(); ++li) {
                const TextLineView& L = lines[li];
                NkVec2f lm = font->MeasureLine(L.ptr, L.len);
                float lineW = lm.x * scale;
                if (L.len > 1) lineW += style.letterSpacing * (float)(L.len - 1) * scale;

                float penX = sx;
                if (align == NkTextAlign::NK_CENTER) penX -= lineW * 0.5f;
                else if (align == NkTextAlign::NK_RIGHT) penX -= lineW;
                float baseline = sy + asc + (float)li * lineH;

                uint32 pIdx = 0;
                while (pIdx < L.len) {
                    const DecodedRune r = DecodeUtf8(L.ptr + pIdx, L.len - pIdx);
                    if (r.bytes == 0) break;
                    pIdx += r.bytes;
                    PushGlyph(font, r.cp, penX, baseline, scale, style.color);

                    NkVec2f uvMin{}, uvMax{}, bearing{};
                    int32 xAdv = 0;
                    uint32 gw = 0, gh = 0;
                    if (font->GetGlyphUV(r.cp, uvMin, uvMax, bearing, xAdv, gw, gh)
                     || font->GetGlyphUV((uint32)'?', uvMin, uvMax, bearing, xAdv, gw, gh)) {
                        penX += ((float)xAdv + style.letterSpacing + (style.bold ? 1.0f : 0.0f)) * scale;
                    }
                }
            }
        }

        NkStaticMesh* NkTextRenderer::CreateExtruded3DText(NkIDevice* device, NkFont* font, const char* text,
                                                           NkMaterial* material, const NkText3DParams& params) {
            (void)material;
            if (!device || !font || !text || !text[0]) return nullptr;
            NkMeshData md = BuildExtruded3DText(font, text, params);
            if (md.vertices.Empty()) return nullptr;

            NkStaticMesh* sm = new NkStaticMesh();
            if (!sm || !sm->Create(device, md)) {
                if (sm) delete sm;
                return nullptr;
            }
            return sm;
        }

        void NkTextRenderer::DrawFPS(float x, float y, const NkTextStyle& style) {
            if (!mInFrame) return;
            static auto last = std::chrono::steady_clock::now();
            static float smoothed = 60.0f;
            const auto now = std::chrono::steady_clock::now();
            const float dt = std::chrono::duration<float>(now - last).count();
            last = now;
            if (dt > 1e-6f) {
                const float fps = 1.0f / dt;
                smoothed = smoothed * 0.9f + fps * 0.1f;
            }
            char buf[64];
            std::snprintf(buf, sizeof(buf), "FPS: %.1f", smoothed);
            DrawText2D(mDefaultFont, buf, x, y, style);
        }

        void NkTextRenderer::DrawDebugInfo(const NkString& info, float x, float y) {
            NkTextStyle s{};
            s.color = {0.85f, 0.9f, 1.0f, 1.0f};
            DrawText2D(mDefaultFont, info.CStr(), x, y, s);
        }

        bool NkTextRenderer::InitShaders(NkRenderPassHandle rp) {
            if (!mDevice || !rp.IsValid() || !mLayout.IsValid()) return false;

            const bool useVulkanGLSL = (mDevice->GetApi() == NkGraphicsApi::NK_GFX_API_VULKAN);
            const char* vs = useVulkanGLSL ? NkBuiltinShaders::SpriteVertVK() : NkBuiltinShaders::Text2DVert();
            const char* fs = useVulkanGLSL ? NkBuiltinShaders::SpriteFragVK() : NkBuiltinShaders::Text2DFrag();

            NkShaderDesc sd{};
            sd.debugName = "NkTextRenderer.Text";
            sd.AddGLSL(NkShaderStage::NK_VERTEX, vs, "main");
            sd.AddGLSL(NkShaderStage::NK_FRAGMENT, fs, "main");
            sd.AddHLSL(NkShaderStage::NK_VERTEX, NkBuiltinShaders::SpriteVertHLSL(), "VSMain");
            sd.AddHLSL(NkShaderStage::NK_FRAGMENT, NkBuiltinShaders::SpriteFragHLSL(), "PSMain");
            sd.AddMSL(NkShaderStage::NK_VERTEX, NkBuiltinShaders::SpriteVertMSL(), "VSMain");
            sd.AddMSL(NkShaderStage::NK_FRAGMENT, NkBuiltinShaders::SpriteFragMSL(), "PSMain");
            mShader = mDevice->CreateShader(sd);
            if (!mShader.IsValid()) return false;

            NkVertexLayout vtx{};
            vtx.AddAttribute(0, 0, NkGPUFormat::NK_RG32_FLOAT, 0,  "POSITION", 0);
            vtx.AddAttribute(1, 0, NkGPUFormat::NK_RG32_FLOAT, 8,  "TEXCOORD", 0);
            vtx.AddAttribute(2, 0, NkGPUFormat::NK_RGBA32_FLOAT, 16, "COLOR", 0);
            vtx.AddBinding(0, sizeof(NkBatchVert));

            NkGraphicsPipelineDesc pd{};
            pd.shader = mShader;
            pd.vertexLayout = vtx;
            pd.topology = NkPrimitiveTopology::NK_TRIANGLE_LIST;
            pd.rasterizer = NkRasterizerDesc::NoCull();
            pd.rasterizer.scissorTest = true;
            pd.depthStencil = NkDepthStencilDesc::NoDepth();
            pd.blend = NkBlendDesc::Alpha();
            pd.renderPass = rp;
            pd.descriptorSetLayouts.PushBack(mLayout);
            pd.debugName = "NkTextRenderer.Pipeline";
            mPipeline = mDevice->CreateGraphicsPipeline(pd);
            return mPipeline.IsValid();
        }

        bool NkTextRenderer::InitBuffers() {
            if (!mDevice) return false;
            static constexpr uint32 kMaxVerts = 131072;
            static constexpr uint32 kMaxIdx = 196608;

            mVBO = mDevice->CreateBuffer(NkBufferDesc::VertexDynamic((uint64)kMaxVerts * sizeof(NkBatchVert)));
            mIBO = mDevice->CreateBuffer(NkBufferDesc::IndexDynamic((uint64)kMaxIdx * sizeof(uint32)));
            mUBO = mDevice->CreateBuffer(NkBufferDesc::Uniform(sizeof(float) * 16u));
            return mVBO.IsValid() && mIBO.IsValid() && mUBO.IsValid();
        }

        bool NkTextRenderer::InitDescriptors() {
            if (!mDevice || !mUBO.IsValid()) return false;
            NkDescriptorSetLayoutDesc d{};
            d.debugName = "NkTextRenderer.Layout";
            d.Add(0, NkDescriptorType::NK_UNIFORM_BUFFER, NkShaderStage::NK_VERTEX);
            d.Add(1, NkDescriptorType::NK_COMBINED_IMAGE_SAMPLER, NkShaderStage::NK_FRAGMENT);
            mLayout = mDevice->CreateDescriptorSetLayout(d);
            mFontDescSets.Clear();
            return mLayout.IsValid();
        }

        void NkTextRenderer::FlushBatches() {
            if (!mCmd || !mDevice || !mPipeline.IsValid() || !mLayout.IsValid()) {
                mBatches.Clear();
                mCurrentBatch = nullptr;
                return;
            }

            NkViewport vp{};
            vp.x = 0.0f;
            vp.y = 0.0f;
            vp.width = (float)mWidth;
            vp.height = (float)mHeight;
            vp.minDepth = 0.0f;
            vp.maxDepth = 1.0f;
            vp.flipY = true;
            mCmd->SetViewport(vp);
            mCmd->SetScissor({0, 0, (int32)mWidth, (int32)mHeight});

            mCmd->BindGraphicsPipeline(mPipeline);

            for (uint32 bi = 0; bi < (uint32)mBatches.Size(); ++bi) {
                const TextQuadBatch& b = mBatches[bi];
                if (!b.font || !b.font->IsValid()) continue;
                if (b.verts.Empty() || b.indices.Empty()) continue;

                const NkTexture2D* atlas = b.font->GetAtlas();
                if (!atlas || !atlas->IsValid()) continue;
                const NkTextureHandle tex = atlas->GetHandle();
                const NkSamplerHandle smp = atlas->GetSampler();
                if (!tex.IsValid() || !smp.IsValid()) continue;

                const uint64 vb = (uint64)b.verts.Size() * sizeof(NkBatchVert);
                const uint64 ib = (uint64)b.indices.Size() * sizeof(uint32);

                NkBufferHandle vbo = mVBO;
                NkBufferHandle ibo = mIBO;
                bool usingTemp = false;

                if (!mDevice->WriteBuffer(mVBO, b.verts.Data(), vb, 0)
                 || !mDevice->WriteBuffer(mIBO, b.indices.Data(), ib, 0)) {
                    vbo = mDevice->CreateBuffer(NkBufferDesc::Vertex(vb, b.verts.Data()));
                    ibo = mDevice->CreateBuffer(NkBufferDesc::Index(ib, b.indices.Data()));
                    if (!vbo.IsValid() || !ibo.IsValid()) {
                        if (vbo.IsValid()) mDevice->DestroyBuffer(vbo);
                        if (ibo.IsValid()) mDevice->DestroyBuffer(ibo);
                        continue;
                    }
                    usingTemp = true;
                }

                if (!mDevice->WriteBuffer(mUBO, b.mvp, sizeof(float) * 16u, 0)) {
                    if (usingTemp) {
                        mDevice->DestroyBuffer(vbo);
                        mDevice->DestroyBuffer(ibo);
                    }
                    continue;
                }

                const uint64 key = MakeTexKey(tex, smp);
                NkDescSetHandle set{};
                if (NkDescSetHandle* found = mFontDescSets.Find(key)) {
                    set = *found;
                } else {
                    set = mDevice->AllocateDescriptorSet(mLayout);
                    if (!set.IsValid()) {
                        if (usingTemp) {
                            mDevice->DestroyBuffer(vbo);
                            mDevice->DestroyBuffer(ibo);
                        }
                        continue;
                    }
                    mFontDescSets[key] = set;
                }

                NkDescriptorWrite w[2]{};
                w[0].set = set;
                w[0].binding = 0;
                w[0].type = NkDescriptorType::NK_UNIFORM_BUFFER;
                w[0].buffer = mUBO;
                w[0].bufferRange = sizeof(float) * 16u;
                w[1].set = set;
                w[1].binding = 1;
                w[1].type = NkDescriptorType::NK_COMBINED_IMAGE_SAMPLER;
                w[1].texture = tex;
                w[1].sampler = smp;
                mDevice->UpdateDescriptorSets(w, 2);

                mCmd->BindDescriptorSet(set, 0);
                mCmd->BindVertexBuffer(0, vbo, 0);
                mCmd->BindIndexBuffer(ibo, NkIndexFormat::NK_UINT32, 0);
                mCmd->DrawIndexed((uint32)b.indices.Size(), 1, 0, 0, 0);

                if (usingTemp) {
                    mDevice->DestroyBuffer(vbo);
                    mDevice->DestroyBuffer(ibo);
                }
            }

            mBatches.Clear();
            mCurrentBatch = nullptr;
        }

        void NkTextRenderer::StartBatch(NkFont* font, bool is3D, const float* mvp16) {
            if (!font || !font->IsValid()) return;

            float mvpLocal[16];
            if (!mvp16) {
                BuildOrthoMVP(mvpLocal);
                mvp16 = mvpLocal;
            }

            if (mCurrentBatch && mCurrentBatch->font == font && mCurrentBatch->is3D == is3D
             && MvpEqual(mCurrentBatch->mvp, mvp16)) {
                return;
            }

            TextQuadBatch b{};
            b.font = font;
            b.is3D = is3D;
            std::memcpy(b.mvp, mvp16, sizeof(float) * 16u);
            mBatches.PushBack(b);
            mCurrentBatch = &mBatches.Back();
        }

        void NkTextRenderer::PushGlyph(const NkFont* font, uint32 codepoint,
                                       float x, float y, float scale,
                                       const NkColor4f& color) {
            if (!mCurrentBatch || !font) return;
            NkVec2f uvMin{}, uvMax{}, bearing{};
            int32 xAdvance = 0;
            uint32 w = 0, h = 0;
            if (!font->GetGlyphUV(codepoint, uvMin, uvMax, bearing, xAdvance, w, h)) return;

            if (w > 0 && h > 0) {
                const float x0 = x + bearing.x * scale;
                const float y0 = y - bearing.y * scale;
                const float x1 = x0 + (float)w * scale;
                const float y1 = y0 + (float)h * scale;

                const uint32 base = (uint32)mCurrentBatch->verts.Size();
                mCurrentBatch->verts.PushBack({x0, y0, uvMin.x, uvMin.y, color.r, color.g, color.b, color.a});
                mCurrentBatch->verts.PushBack({x1, y0, uvMax.x, uvMin.y, color.r, color.g, color.b, color.a});
                mCurrentBatch->verts.PushBack({x1, y1, uvMax.x, uvMax.y, color.r, color.g, color.b, color.a});
                mCurrentBatch->verts.PushBack({x0, y1, uvMin.x, uvMax.y, color.r, color.g, color.b, color.a});
                mCurrentBatch->indices.PushBack(base + 0);
                mCurrentBatch->indices.PushBack(base + 1);
                mCurrentBatch->indices.PushBack(base + 2);
                mCurrentBatch->indices.PushBack(base + 2);
                mCurrentBatch->indices.PushBack(base + 3);
                mCurrentBatch->indices.PushBack(base + 0);
            }
        }

        void NkTextRenderer::BuildOrthoMVP(float out[16]) const {
            std::memset(out, 0, sizeof(float) * 16u);
            if (mWidth == 0 || mHeight == 0) {
                out[0] = out[5] = out[10] = out[15] = 1.0f;
                return;
            }
            out[0] = 2.0f / (float)mWidth;
            out[5] = 2.0f / (float)mHeight;
            out[10] = 1.0f;
            out[12] = -1.0f;
            out[13] = -1.0f;
            out[15] = 1.0f;
        }

        void NkTextRenderer::BuildBillboardMVP(const NkVec3f& worldPos, float worldW, float worldH,
                                               const NkMat4f& view, const NkMat4f& proj,
                                               float out[16]) const {
            const NkMat4f invView = view.Inverse();
            const NkVec3f right = {invView.data[0], invView.data[1], invView.data[2]};
            const NkVec3f up = {-invView.data[4], -invView.data[5], -invView.data[6]}; // local +Y goes downward
            const NkVec3f fwd = {invView.data[8], invView.data[9], invView.data[10]};

            NkMat4f model = NkMat4f::Identity();
            model.data[0] = right.x * worldW; model.data[1] = right.y * worldW; model.data[2] = right.z * worldW;
            model.data[4] = up.x * worldH;    model.data[5] = up.y * worldH;    model.data[6] = up.z * worldH;
            model.data[8] = fwd.x;            model.data[9] = fwd.y;            model.data[10] = fwd.z;
            model.data[12] = worldPos.x;
            model.data[13] = worldPos.y;
            model.data[14] = worldPos.z;

            const NkMat4f mvp = proj * view * model;
            std::memcpy(out, mvp.data, sizeof(float) * 16u);
        }

        // =========================================================================
        // Renderer font library
        // =========================================================================

        NkFontLibrary& NkFontLibrary::Get() {
            static NkFontLibrary s;
            return s;
        }

        void NkFontLibrary::Init(NkIDevice* device) {
            mDevice = device;
            if (mSearchPath.Empty()) {
                mSearchPath = "Resources/Fonts";
            }
        }

        void NkFontLibrary::Shutdown() {
            mFonts.ForEach([&](const NkString&, NkFont* f) {
                if (f) delete f;
            });
            mFonts.Clear();
            mDefault = nullptr;
            mDevice = nullptr;
        }

        NkFont* NkFontLibrary::Load(const char* path, uint32 size) {
            if (!mDevice || !path || !path[0] || size == 0) return nullptr;
            char sizeBuf[32]{};
            std::snprintf(sizeBuf, sizeof(sizeBuf), "%u", (unsigned)size);
            NkString key = NkString(path) + "#" + NkString(sizeBuf);
            if (NkFont** f = mFonts.Find(key)) return *f;

            NkFont* font = new NkFont();
            if (!font) return nullptr;
            if (!font->Load(mDevice, path, size)) {
                delete font;
                return nullptr;
            }
            mFonts[key] = font;
            if (!mDefault) mDefault = font;
            return font;
        }

        NkFont* NkFontLibrary::FindOrLoad(const char* name, uint32 size) {
            if (!name || !name[0]) return GetDefault(size);
            std::filesystem::path p(name);
            if (std::filesystem::exists(p)) {
                return Load(p.string().c_str(), size);
            }

            const NkString file = FindFontFile(name);
            if (!file.Empty()) {
                return Load(file.CStr(), size);
            }
            return GetDefault(size);
        }

        NkFont* NkFontLibrary::GetDefault(uint32 size) const {
            if (mDefault && mDefault->IsValid() && mDefault->GetFontSize() == size) {
                return mDefault;
            }

            NkFontLibrary& self = const_cast<NkFontLibrary&>(*this);
            static const char* kCandidates[] = {
                "Roboto-Medium",
                "Roboto-Medium.ttf",
                "Karla-Regular",
                "Karla-Regular.ttf",
                "Cousine-Regular",
                "Cousine-Regular.ttf",
                "DroidSans",
                "DroidSans.ttf",
                nullptr
            };
            for (uint32 i = 0; kCandidates[i]; ++i) {
                NkFont* f = self.FindOrLoad(kCandidates[i], size);
                if (f && f->IsValid()) return f;
            }

            if (mDefault && mDefault->IsValid()) return mDefault;
            return nullptr;
        }

        NkString NkFontLibrary::FindFontFile(const char* name) const {
            if (!name || !name[0]) return {};
            namespace fs = std::filesystem;

            std::vector<fs::path> roots;
            if (!mSearchPath.Empty()) roots.push_back(fs::path(mSearchPath.CStr()));
            roots.push_back(fs::path("Resources/Fonts"));
            roots.push_back(fs::path("./Resources/Fonts"));

            const std::array<const char*, 6> exts = {"", ".ttf", ".otf", ".TTF", ".OTF", ".ttc"};
            for (const fs::path& r : roots) {
                for (const char* ext : exts) {
                    fs::path p = r / (std::string(name) + ext);
                    if (fs::exists(p) && fs::is_regular_file(p)) {
                        return NkString(p.string().c_str());
                    }
                }
            }
            return {};
        }

    } // namespace renderer
} // namespace nkentseu

