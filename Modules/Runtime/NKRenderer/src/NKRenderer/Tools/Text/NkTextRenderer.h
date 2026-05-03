#pragma once
// =============================================================================
// NkTextRenderer.h  — NKRenderer v4.0  (Tools/Text/)
//
// Pont entre NKRenderer et les bibliothèques de texte.
//
// Backends de texte disponibles (extensibles via callback) :
//   1. NKFont (défaut) — TTF/OTF/WOFF, atlas bitmap + SDF, extrusion 3D
//   2. Custom (NkTextFontLoaderDesc) — tout autre système (FreeType, etc.)
//
// Backend UI :
//   3. NKUI (NkUIFontBridge) — pour le rendu UI immédiat mode
//
// Fonctionnalités :
//   • Rendu 2D batché via NkRender2D
//   • Rendu SDF (net à toute taille)
//   • Extrusion 3D (titre 3D, logo) via NkFont::GenerateTextMesh3D
//   • Mesure de texte (CalcTextSize)
//   • Multi-polices, multi-tailles
// =============================================================================
#include "NKRenderer/Core/NkRendererTypes.h"
#include "NKRenderer/Core/NkTextureLibrary.h"
#include "NKRHI/Core/NkIDevice.h"
#include "NKFont/NkFont.h"           // NKFont backend

namespace nkentseu {
namespace renderer {

    class NkRender2D;
    class NkRender3D;

    // =========================================================================
    // Callbacks pour backend de texte custom (FreeType, DirectWrite, etc.)
    // =========================================================================
    using NkTextLoadFn    = bool(*)(const char* path, float32 sizePx, void* user, void** outHandle);
    using NkTextGlyphFn   = bool(*)(void* handle, uint32 cp, uint8* atlasPx, int32 atlasW,
                                      int32 ax, int32 ay, int32 gw, int32 gh, void* user,
                                      float32* outAx, float32* outU0, float32* outV0,
                                      float32* outU1, float32* outV1,
                                      float32* outX0, float32* outY0,
                                      float32* outX1, float32* outY1);
    using NkTextMetricsFn = void(*)(void* handle, void* user,
                                      float32* outAscent, float32* outDescent, float32* outLineH);
    using NkTextDestroyFn = void(*)(void* handle, void* user);

    struct NkTextFontLoaderDesc {
        NkTextLoadFn    Load     = nullptr;
        NkTextGlyphFn   GetGlyph = nullptr;
        NkTextMetricsFn Metrics  = nullptr;
        NkTextDestroyFn Destroy  = nullptr;
        void*           userData = nullptr;
        bool IsValid() const { return Load && GetGlyph && Metrics && Destroy; }
    };

    // =========================================================================
    // NkFontEntry — police chargée (NKFont ou custom)
    // =========================================================================
    struct NkFontEntry {
        NkFontHandle    handle;
        NkTexHandle     atlasTexture;
        bool            isSDF    = false;
        bool            isCustom = false;
        float32         sizePixels = 16.f;
        float32         ascent    = 0.f;
        float32         descent   = 0.f;
        float32         lineH     = 0.f;

        // NKFont backend
        NkFont*         nkfFont  = nullptr;
        NkFontAtlas*    nkfAtlas = nullptr;

        // Custom backend
        void*                mHandle = nullptr;
        NkTextFontLoaderDesc mCustom;
    };

    // =========================================================================
    // NkTextRenderer
    // =========================================================================
    class NkTextRenderer {
    public:
        NkTextRenderer() = default;
        ~NkTextRenderer();

        bool Init(NkIDevice* device, NkTextureLibrary* texLib, NkRender2D* r2d);
        void Shutdown();

        // ── Chargement de polices ─────────────────────────────────────────────
        // Backend NKFont (défaut)
        NkFontHandle LoadFont(const char* path, float32 sizePx, bool sdf = false);
        NkFontHandle LoadFontFromMemory(const uint8* data, uint32 size,
                                          float32 sizePx, bool sdf = false);
        // Police embarquée (NKFont default font)
        NkFontHandle GetDefaultFont();

        // Backend custom
        NkFontHandle LoadFontCustom(const char* path, float32 sizePx,
                                      const NkTextFontLoaderDesc& desc);

        void         UnloadFont(NkFontHandle& handle);

        // ── Rendu 2D (délègue à NkRender2D) ──────────────────────────────────
        void DrawText(NkVec2f pos, const char* text, NkFontHandle font,
                       float32 size, uint32 colorRGBA = 0xFFFFFFFF);
        void DrawTextCentered(NkRectF bounds, const char* text, NkFontHandle font,
                               float32 size, uint32 colorRGBA = 0xFFFFFFFF);
        void DrawTextWorld(NkVec3f worldPos, const NkMat4f& viewProj,
                            uint32 vpW, uint32 vpH,
                            const char* text, NkFontHandle font,
                            float32 size, uint32 colorRGBA = 0xFFFFFFFF);

        // ── Mesure ────────────────────────────────────────────────────────────
        NkVec2f CalcTextSize(const char* text, NkFontHandle font, float32 size) const;
        float32 CalcTextWidth(const char* text, NkFontHandle font, float32 size) const;

        // ── Extrusion 3D (via NKFont) ─────────────────────────────────────────
        // Retourne un handle de mesh utilisable dans NkRender3D
        NkMeshHandle ExtrudeText3D(const char* text, NkFontHandle font,
                                    float32 scale, float32 depth,
                                    NkMeshSystem* meshSys,
                                    const NkMat4f& transform = NkMat4f::Identity());

        // ── Flush (appelé en fin de frame par NkRendererImpl) ─────────────────
        void FlushPending(NkICommandBuffer* cmd);

    private:
        NkIDevice*        mDevice  = nullptr;
        NkTextureLibrary* mTexLib  = nullptr;
        NkRender2D*       mRender2D= nullptr;
        uint64            mNextId  = 1;

        NkVector<NkFontEntry*> mFonts;
        NkFontHandle           mDefaultFont;

        NkFontEntry* FindEntry(NkFontHandle h) const;
        NkFontHandle AllocHandle();
        bool         BuildAtlasTexture(NkFontEntry* e);
    };

} // namespace renderer
} // namespace nkentseu
