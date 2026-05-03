// =============================================================================
// NkTextRenderer.cpp  — NKRenderer v4.0
// =============================================================================
#include "NkTextRenderer.h"
#include "NKRenderer/Tools/Render2D/NkRender2D.h"
#include "NKRenderer/Mesh/NkMeshSystem.h"
#include <cstring>
#include <cstdio>

namespace nkentseu {
namespace renderer {

    NkTextRenderer::~NkTextRenderer() { Shutdown(); }

    bool NkTextRenderer::Init(NkIDevice* device, NkTextureLibrary* texLib, NkRender2D* r2d) {
        mDevice  = device;
        mTexLib  = texLib;
        mRender2D= r2d;

        // Charger la police par défaut (NKFont embedded)
        mDefaultFont = GetDefaultFont();
        return true;
    }

    void NkTextRenderer::Shutdown() {
        for (auto* e : mFonts) {
            if (e) {
                if (e->nkfAtlas) { e->nkfAtlas->Clear(); delete e->nkfAtlas; }
                if (e->isCustom && e->mCustom.Destroy)
                    e->mCustom.Destroy(e->mHandle, e->mCustom.userData);
                delete e;
            }
        }
        mFonts.Clear();
    }

    NkFontHandle NkTextRenderer::AllocHandle() {
        return {mNextId++};
    }

    NkFontEntry* NkTextRenderer::FindEntry(NkFontHandle h) const {
        for (auto* e : mFonts)
            if (e && e->handle == h) return e;
        return nullptr;
    }

    // ── Chargement NKFont ─────────────────────────────────────────────────────
    NkFontHandle NkTextRenderer::LoadFont(const char* path, float32 sizePx, bool sdf) {
        auto* entry  = new NkFontEntry();
        entry->handle    = AllocHandle();
        entry->sizePixels= sizePx;
        entry->isSDF     = sdf;
        entry->isCustom  = false;
        entry->nkfAtlas  = new NkFontAtlas();
        entry->nkfAtlas->sdfMode = sdf;

        NkFontConfig cfg;
        cfg.sizePixels = sizePx;
        entry->nkfFont = entry->nkfAtlas->AddFontFromFile(path, sizePx, &cfg);
        if (!entry->nkfFont) {
            delete entry->nkfAtlas; delete entry; return NkFontHandle::Null();
        }
        if (!entry->nkfAtlas->Build()) {
            delete entry->nkfAtlas; delete entry; return NkFontHandle::Null();
        }

        if (!BuildAtlasTexture(entry)) {
            delete entry->nkfAtlas; delete entry; return NkFontHandle::Null();
        }

        entry->ascent  = entry->nkfFont->ascent;
        entry->descent = entry->nkfFont->descent;
        entry->lineH   = entry->nkfFont->lineAdvance;
        mFonts.PushBack(entry);
        return entry->handle;
    }

    NkFontHandle NkTextRenderer::LoadFontFromMemory(const uint8* data, uint32 size,
                                                      float32 sizePx, bool sdf) {
        auto* entry      = new NkFontEntry();
        entry->handle    = AllocHandle();
        entry->sizePixels= sizePx;
        entry->isSDF     = sdf;
        entry->isCustom  = false;
        entry->nkfAtlas  = new NkFontAtlas();
        entry->nkfAtlas->sdfMode = sdf;

        NkFontConfig cfg; cfg.sizePixels = sizePx;
        entry->nkfFont = entry->nkfAtlas->AddFontFromMemory(data, size, sizePx, &cfg);
        if (!entry->nkfFont || !entry->nkfAtlas->Build()) {
            delete entry->nkfAtlas; delete entry; return NkFontHandle::Null();
        }
        if (!BuildAtlasTexture(entry)) {
            delete entry->nkfAtlas; delete entry; return NkFontHandle::Null();
        }
        entry->ascent = entry->nkfFont->ascent;
        entry->descent= entry->nkfFont->descent;
        entry->lineH  = entry->nkfFont->lineAdvance;
        mFonts.PushBack(entry);
        return entry->handle;
    }

    NkFontHandle NkTextRenderer::GetDefaultFont() {
        if (mDefaultFont.IsValid()) return mDefaultFont;
        // NKFont has embedded font data accessible via NkFontAtlas
        auto* entry      = new NkFontEntry();
        entry->handle    = AllocHandle();
        entry->sizePixels= 13.f;
        entry->isCustom  = false;
        entry->nkfAtlas  = new NkFontAtlas();
        // Add default font (NKFont provides embedded Proggy font)
        NkFontConfig cfg; cfg.sizePixels = 13.f;
        // Use default ranges
        cfg.glyphRanges = NkFontAtlas::GetGlyphRangesDefault();
        entry->nkfFont = entry->nkfAtlas->AddFontFromMemory(
            NkFontEmbedded::GetData(),
            (nkft_size)NkFontEmbedded::GetSize(),
            13.f, &cfg);
        if (!entry->nkfFont || !entry->nkfAtlas->Build()) {
            delete entry->nkfAtlas; delete entry; return NkFontHandle::Null();
        }
        if (!BuildAtlasTexture(entry)) {
            delete entry->nkfAtlas; delete entry; return NkFontHandle::Null();
        }
        entry->ascent = entry->nkfFont->ascent;
        entry->descent= entry->nkfFont->descent;
        entry->lineH  = entry->nkfFont->lineAdvance;
        mFonts.PushBack(entry);
        mDefaultFont = entry->handle;
        return mDefaultFont;
    }

    bool NkTextRenderer::BuildAtlasTexture(NkFontEntry* e) {
        uint8* pixels = nullptr; int32 w = 0, h = 0;
        e->nkfAtlas->GetTexDataAsRGBA32(&pixels, &w, &h);
        if (!pixels || w <= 0 || h <= 0) return false;

        NkTextureDesc2 tdesc;
        tdesc.pixels   = pixels;
        tdesc.width    = (uint32)w;
        tdesc.height   = (uint32)h;
        tdesc.srgb     = false;
        tdesc.genMips  = false;
        tdesc.debugName= "FontAtlas";
        e->atlasTexture= mTexLib->Create(tdesc);
        e->nkfAtlas->ClearTexData();  // libère les pixels CPU
        return e->atlasTexture.IsValid();
    }

    // ── Custom backend ────────────────────────────────────────────────────────
    NkFontHandle NkTextRenderer::LoadFontCustom(const char* path, float32 sizePx,
                                                  const NkTextFontLoaderDesc& desc) {
        if (!desc.IsValid()) return NkFontHandle::Null();
        auto* entry      = new NkFontEntry();
        entry->handle    = AllocHandle();
        entry->sizePixels= sizePx;
        entry->isCustom  = true;
        entry->mCustom   = desc;

        if (!desc.Load(path, sizePx, desc.userData, &entry->mHandle)) {
            delete entry; return NkFontHandle::Null();
        }
        desc.Metrics(entry->mHandle, desc.userData, &entry->ascent, &entry->descent, &entry->lineH);

        // Build atlas from custom backend (simple 512x512)
        const int32 atlasW = 512, atlasH = 512;
        NkVector<uint8> atlasPx(atlasW * atlasH * 4, 0);
        // Rasterize ASCII range into atlas
        int32 cx = 1, cy = 1, rowH = 0;
        for (uint32 cp = 32; cp < 127; cp++) {
            int32 gw = 0, gh = 0;
            if (!desc.GetGlyph(entry->mHandle, cp, atlasPx.Data(), atlasW,
                                cx, cy, 0, 0, desc.userData,
                                nullptr, nullptr, nullptr, nullptr, nullptr,
                                nullptr, nullptr, nullptr, nullptr)) continue;
            // Simplified atlas packing — real impl would use rect packer
            cx += gw + 1;
            if (cx + gw + 1 >= atlasW) { cx = 1; cy += rowH + 1; rowH = 0; }
            if (gh > rowH) rowH = gh;
        }

        NkTextureDesc2 tdesc;
        tdesc.pixels   = atlasPx.Data();
        tdesc.width    = (uint32)atlasW;
        tdesc.height   = (uint32)atlasH;
        tdesc.srgb     = false;
        tdesc.genMips  = false;
        entry->atlasTexture = mTexLib->Create(tdesc);
        if (!entry->atlasTexture.IsValid()) {
            desc.Destroy(entry->mHandle, desc.userData);
            delete entry; return NkFontHandle::Null();
        }
        mFonts.PushBack(entry);
        return entry->handle;
    }

    void NkTextRenderer::UnloadFont(NkFontHandle& handle) {
        for (uint32 i = 0; i < (uint32)mFonts.Size(); i++) {
            if (mFonts[i] && mFonts[i]->handle == handle) {
                auto* e = mFonts[i];
                if (e->nkfAtlas) { e->nkfAtlas->Clear(); delete e->nkfAtlas; }
                if (e->isCustom && e->mCustom.Destroy)
                    e->mCustom.Destroy(e->mHandle, e->mCustom.userData);
                if (e->atlasTexture.IsValid()) mTexLib->Release(e->atlasTexture);
                delete e;
                mFonts.RemoveAt(i);
                break;
            }
        }
        handle = NkFontHandle::Null();
    }

    // ── Rendu 2D ─────────────────────────────────────────────────────────────
    void NkTextRenderer::DrawText(NkVec2f pos, const char* text,
                                    NkFontHandle font, float32 size, uint32 colorRGBA) {
        NkFontEntry* e = FindEntry(font);
        if (!e || !text) return;

        // Scale factor : requested size / original bake size
        float32 scale = (e->nkfFont) ? (size / e->sizePixels) : 1.f;

        float32 x = pos.x, y = pos.y;
        const char* p = text;
        while (*p) {
            NkFontCodepoint cp = NkFontDecodeUTF8(&p, p + 4);
            const NkFontGlyph* g = e->nkfFont ? e->nkfFont->FindGlyph(cp) : nullptr;
            if (!g) continue;
            if (g->visible) {
                NkRectF dst  = {x + g->x0*scale, y + g->y0*scale,
                                 (g->x1-g->x0)*scale, (g->y1-g->y0)*scale};
                NkRectF uvr  = {g->u0, g->v0, g->u1-g->u0, g->v1-g->v0};
                // Unpack color
                float32 r = ((colorRGBA>>24)&0xFF)/255.f;
                float32 gv= ((colorRGBA>>16)&0xFF)/255.f;
                float32 b = ((colorRGBA>> 8)&0xFF)/255.f;
                float32 a = ( colorRGBA     &0xFF)/255.f;
                mRender2D->DrawSprite(dst, e->atlasTexture, {r,gv,b,a}, uvr);
            }
            x += g->advanceX * scale;
        }
    }

    void NkTextRenderer::DrawTextCentered(NkRectF bounds, const char* text,
                                            NkFontHandle font, float32 size, uint32 colorRGBA) {
        NkVec2f sz = CalcTextSize(text, font, size);
        NkVec2f pos = {
            bounds.x + (bounds.w - sz.x) * 0.5f,
            bounds.y + (bounds.h - sz.y) * 0.5f
        };
        DrawText(pos, text, font, size, colorRGBA);
    }

    void NkTextRenderer::DrawTextWorld(NkVec3f worldPos, const NkMat4f& viewProj,
                                        uint32 vpW, uint32 vpH,
                                        const char* text, NkFontHandle font,
                                        float32 size, uint32 colorRGBA) {
        // Project world → NDC → screen
        NkVec4f clip = viewProj * NkVec4f{worldPos.x, worldPos.y, worldPos.z, 1.f};
        if (clip.w <= 0.f) return;
        float32 ndcX =  clip.x / clip.w;
        float32 ndcY = -clip.y / clip.w;
        float32 sx   = (ndcX * 0.5f + 0.5f) * (float32)vpW;
        float32 sy   = (ndcY * 0.5f + 0.5f) * (float32)vpH;
        DrawText({sx, sy}, text, font, size, colorRGBA);
    }

    // ── Mesure ────────────────────────────────────────────────────────────────
    NkVec2f NkTextRenderer::CalcTextSize(const char* text, NkFontHandle font, float32 size) const {
        NkFontEntry* e = FindEntry(font);
        if (!e || !e->nkfFont || !text) return {0,0};
        float32 scale = size / e->sizePixels;
        float32 w = e->nkfFont->CalcTextSizeX(text) * scale;
        float32 h = e->lineH * scale;
        return {w, h};
    }

    float32 NkTextRenderer::CalcTextWidth(const char* text, NkFontHandle font, float32 size) const {
        return CalcTextSize(text, font, size).x;
    }

    // ── Extrusion 3D ─────────────────────────────────────────────────────────
    NkMeshHandle NkTextRenderer::ExtrudeText3D(const char* text, NkFontHandle font,
                                                 float32 scale, float32 depth,
                                                 NkMeshSystem* meshSys,
                                                 const NkMat4f& transform) {
        NkFontEntry* e = FindEntry(font);
        if (!e || !e->nkfFont || !text || !meshSys) return NkMeshHandle::Null();

        // Use NKFont 3D extrusion
        NkFontGlyphMesh3D mesh3D = e->nkfFont->GenerateTextMesh3D(
            text, scale, depth, transform, {1,1,1,1});

        if (mesh3D.vertices.Empty()) return NkMeshHandle::Null();

        // Convert NkFontGlyph3DVertex → NkVertex3D
        NkVector<NkVertex3D> verts(mesh3D.vertices.Size());
        for (uint32 i = 0; i < (uint32)mesh3D.vertices.Size(); i++) {
            auto& src = mesh3D.vertices[i];
            auto& dst = verts[i];
            dst.pos    = src.position;
            dst.normal = src.normal;
            dst.uv     = src.uv;
            dst.tangent= {1,0,0};
            dst.color  = 0xFFFFFFFF;
        }

        NkVector<uint32> idxBuf;
        for (auto idx : mesh3D.indices) idxBuf.PushBack((uint32)idx);

        NkMeshDesc desc = NkMeshDesc::Simple(
            NkVertexLayout::Default3D(),
            verts.Data(), (uint32)verts.Size(),
            idxBuf.Data(), (uint32)idxBuf.Size());
        desc.debugName = text;

        return meshSys->Create(desc);
    }

    void NkTextRenderer::FlushPending(NkICommandBuffer* cmd) {
        // NkRender2D handles its own flush; nothing extra needed here
    }

} // namespace renderer
} // namespace nkentseu
