#pragma once
// =============================================================================
// NkTextRenderer.h
// Rendu de texte 2D (overlay) et 3D (billboard + extrudé) du NKRenderer.
//
// Fonctionnalités :
//   • Texte 2D overlay (HUD, UI, debug)
//   • Texte 3D Billboard (labels qui font face à la caméra)
//   • Texte 3D Extrudé (lettres volumiques, comme Blender)
//   • Gestion des polices FreeType (TTF, OTF)
//   • Atlas de glyphes GPU (texture Gray8, packing automatique)
//   • Rendu multi-styles : couleur, gradient, contour, ombre portée
//   • Wrapping, alignement, espacement
//   • Emojis (si police supportée)
// =============================================================================
#include "NKRenderer/Core/NkRenderTypes.h"
#include "NKRenderer/Resources/NkTexture.h"
#include "NkRenderer2D.h"
#include "NKRHI/Core/NkIDevice.h"
#include "NKRHI/Commands/NkICommandBuffer.h"
#include "NKFont/NkFontFace.h"

namespace nkentseu {
    namespace renderer {

        // =============================================================================
        // Style de texte
        // =============================================================================
        struct NkTextStyle {
            NkColor4f  color        = NkColor4f::White();
            NkColor4f  outlineColor = NkColor4f::Black();
            float      outlineWidth = 0.f;      // 0 = pas de contour
            NkColor4f  shadowColor  = {0,0,0,0.5f};
            NkVec2f    shadowOffset = {2.f, 2.f};
            bool       shadow       = false;
            // Gradient vertical
            bool       gradient     = false;
            NkColor4f  gradientTop  = NkColor4f::White();
            NkColor4f  gradientBot  = {0.7f, 0.7f, 0.7f, 1.f};
            // Bold / Italic (simulés si non supportés par la police)
            bool       bold         = false;
            bool       italic       = false;
            float      letterSpacing= 0.f;  // en pixels
            float      lineSpacing  = 1.2f; // multiplicateur de la hauteur de ligne
        };

        // =============================================================================
        // Alignement du texte
        // =============================================================================
        enum class NkTextAlign  : uint32 { NK_LEFT, NK_CENTER, NK_RIGHT, NK_JUSTIFY };
        enum class NkTextVAlign : uint32 { NK_TOP, NK_MIDDLE, NK_BOTTOM, NK_BASELINE };

        // =============================================================================
        // NkFont — une police chargée avec atlas de glyphes GPU
        // =============================================================================
        class NkFont {
            public:
                NkFont()  = default;
                ~NkFont() { Destroy(); }
                NkFont(const NkFont&) = delete;
                NkFont& operator=(const NkFont&) = delete;

                // Charger une police TTF/OTF
                bool Load(NkIDevice* device, const char* path, uint32 fontSize = 24,
                        uint32 oversample = 2);

                // Charger depuis un buffer mémoire
                bool LoadFromMemory(NkIDevice* device, const uint8* data, usize size,
                                    uint32 fontSize = 24, uint32 oversample = 2);

                // Créer une variante de taille différente depuis la même police
                NkFont* CreateSized(NkIDevice* device, uint32 newSize) const;

                void Destroy();
                bool IsValid() const { return mAtlasTex && mAtlasTex->IsValid(); }

                // ── Métriques ─────────────────────────────────────────────────────────────
                int32   GetAscender()   const { return mFace ? mFace->GetAscender()   : 0; }
                int32   GetDescender()  const { return mFace ? mFace->GetDescender()  : 0; }
                int32   GetLineHeight() const { return mFace ? mFace->GetLineHeight() : 0; }
                uint32  GetFontSize()   const { return mFontSize; }

                // ── Mesure de texte ───────────────────────────────────────────────────────
                NkVec2f MeasureText(const char* text, const NkTextStyle& style = {}) const;
                NkVec2f MeasureLine(const char* text, uint32 len) const;

                // ── Accès glyphes ─────────────────────────────────────────────────────────
                const NkFTGlyph* GetGlyph(uint32 codepoint) const;
                bool GetGlyphUV(uint32 codepoint, NkVec2f& uvMin, NkVec2f& uvMax,
                                NkVec2f& bearing, int32& xAdvance, uint32& w, uint32& h) const;
                NkTexture2D*     GetAtlas() const { return mAtlasTex; }
                NkSamplerHandle  GetSampler() const;

                // ── Atlas rebuild ─────────────────────────────────────────────────────────
                // Ajouter des caractères (ex: cyrillique, japonais hiragana, etc.)
                bool AddGlyphRange(NkIDevice* device, uint32 start, uint32 end);
                bool RebuildAtlas(NkIDevice* device);

                const NkString& GetPath() const { return mPath; }
                NkFontID GetID() const { return mID; }

            private:
                NkFTFontLibrary* mLibrary = nullptr;
                NkFTFontFace*    mFace    = nullptr;
                NkTexture2D*     mAtlasTex = nullptr;
                uint32           mFontSize = 24;
                NkString         mPath;
                NkFontID         mID;
                static uint64    sIDCounter;
                NkVector<uint8>  mFontBytes;
                mutable NkFTGlyph mScratchGlyph{};

                // Glyph cache (UV dans l'atlas pour chaque codepoint)
                struct GlyphUV {
                    NkVec2f uvMin, uvMax;
                    NkVec2f bearing;
                    int32   xAdvance = 0;
                    uint32  w, h;
                };
                NkUnorderedMap<uint32, GlyphUV> mGlyphUV;

                void BuildAtlasFromFace(NkIDevice* device);
        };

        // =============================================================================
        // NkTextMesh — géométrie CPU pour un bloc de texte (mis en cache)
        // =============================================================================
        struct NkTextGlyph {
            NkVec2f pos;          // position du quad
            NkVec2f size;         // taille du quad
            NkVec2f uvMin, uvMax; // UV dans l'atlas
        };

        struct NkTextMesh {
            NkVector<NkVertex2D>  vertices;
            NkVector<uint32>      indices;
            NkVec2f               bounds; // width, height total
            uint32                lineCount = 0;
        };

        NkTextMesh BuildTextMesh(const NkFont* font, const char* text,
                                const NkTextStyle& style = {},
                                NkTextAlign align = NkTextAlign::NK_LEFT,
                                float maxWidth = 0.f);   // 0 = pas de wrap

        // =============================================================================
        // Texte 3D extrudé — lettres volumiques (comme Blender)
        // =============================================================================
        struct NkText3DParams {
            float  extrude    = 0.1f;   // profondeur d'extrusion
            float  bevelDepth = 0.02f;  // chanfrein des bords
            uint32 bevelRes   = 2;      // résolution du chanfrein
            float  charSpacing= 0.f;    // espacement entre lettres
            bool   back       = true;   // face arrière
            bool   front      = true;   // face avant
            bool   sides      = true;   // faces latérales
        };

        // Génère un NkMeshData pour du texte 3D extrudé
        NkMeshData BuildExtruded3DText(const NkFont* font, const char* text,
                                        const NkText3DParams& params = {});

        // =============================================================================
        // NkTextRenderer — rendu de texte 2D et 3D
        // =============================================================================
        class NkTextRenderer {
            public:
                NkTextRenderer()  = default;
                ~NkTextRenderer() { Shutdown(); }
                NkTextRenderer(const NkTextRenderer&) = delete;
                NkTextRenderer& operator=(const NkTextRenderer&) = delete;

                // ── Cycle de vie ──────────────────────────────────────────────────────────
                bool Init(NkIDevice* device, NkRenderPassHandle renderPass,
                        uint32 width, uint32 height);
                void Shutdown();
                void Resize(uint32 width, uint32 height);

                // ── Chargement des polices ─────────────────────────────────────────────────
                NkFont* LoadFont(const char* path, uint32 size = 24);
                NkFont* GetDefaultFont() const { return mDefaultFont; }
                void    SetDefaultFont(NkFont* f) { mDefaultFont = f; }

                // ── Frame ─────────────────────────────────────────────────────────────────
                void Begin(NkICommandBuffer* cmd);
                void End();

                // ── Texte 2D overlay ──────────────────────────────────────────────────────
                void DrawText2D(NkFont* font, const char* text,
                                float x, float y,
                                const NkTextStyle& style = {},
                                NkTextAlign align   = NkTextAlign::NK_LEFT,
                                NkTextVAlign valign = NkTextVAlign::NK_TOP,
                                float depth         = 0.f);

                // Avec wrap automatique
                void DrawText2DWrapped(NkFont* font, const char* text,
                                    float x, float y, float maxWidth,
                                    const NkTextStyle& style = {},
                                    NkTextAlign align = NkTextAlign::NK_LEFT,
                                    float depth = 0.f);

                // Mesurer sans rendre
                NkVec2f MeasureText2D(NkFont* font, const char* text,
                                    const NkTextStyle& style = {}) const;

                // ── Texte 3D Billboard (face caméra) ─────────────────────────────────────
                void DrawText3DBillboard(NkFont* font, const char* text,
                                        const NkVec3f& worldPos,
                                        const NkMat4f& view, const NkMat4f& proj,
                                        float worldSize = 0.5f,
                                        const NkTextStyle& style = {},
                                        NkTextAlign align = NkTextAlign::NK_CENTER);

                // ── Texte 3D ancré (reste à une position world, pas de rotation) ──────────
                void DrawText3DAnchored(NkFont* font, const char* text,
                                        const NkVec3f& worldPos,
                                        const NkMat4f& viewProj,
                                        float worldSize = 0.5f,
                                        const NkTextStyle& style = {},
                                        NkTextAlign align = NkTextAlign::NK_CENTER,
                                        bool clipBehindCamera = true);

                // ── Texte 3D extrudé (mesh volumique) ─────────────────────────────────────
                // Génère un NkStaticMesh avec le texte en relief
                NkStaticMesh* CreateExtruded3DText(NkIDevice* device,
                                                    NkFont* font, const char* text,
                                                    NkMaterial* material,
                                                    const NkText3DParams& params = {});

                // ── FPS / Debug ────────────────────────────────────────────────────────────
                void DrawFPS(float x = 10.f, float y = 10.f,
                            const NkTextStyle& style = {});
                void DrawDebugInfo(const NkString& info, float x = 10.f, float y = 30.f);

                // ── Accesseurs ────────────────────────────────────────────────────────────
                bool IsValid() const { return mIsValid; }

            private:
                struct TextQuadBatch {
                    NkFont*      font    = nullptr;
                    NkVector<NkBatchVert> verts;
                    NkVector<uint32>      indices;
                    float        minDepth= 0.f;
                    bool         is3D    = false;
                    float        mvp[16] = {};
                };

                NkIDevice*         mDevice       = nullptr;
                NkICommandBuffer*  mCmd          = nullptr;
                uint32             mWidth        = 0;
                uint32             mHeight       = 0;
                bool               mIsValid      = false;
                bool               mInFrame      = false;
                NkRenderPassHandle mRenderPass;

                // Polices
                NkFont*            mDefaultFont  = nullptr;
                NkVector<NkFont*>  mFonts;
                NkFTFontLibrary    mFTLibrary;

                // GPU
                NkBufferHandle     mVBO;
                NkBufferHandle     mIBO;
                NkBufferHandle     mUBO;           // pour le MVP
                NkShaderHandle     mShader;
                NkPipelineHandle   mPipeline;
                NkDescSetHandle    mLayout;
                NkUnorderedMap<uint64, NkDescSetHandle> mFontDescSets;

                // Batches par font
                NkVector<TextQuadBatch> mBatches;
                TextQuadBatch*          mCurrentBatch = nullptr;

                bool InitShaders(NkRenderPassHandle rp);
                bool InitBuffers();
                bool InitDescriptors();

                void FlushBatches();
                void StartBatch(NkFont* font, bool is3D, const float* mvp16 = nullptr);
                void PushGlyph(const NkFont* font, uint32 codepoint,
                            float x, float y, float scale,
                            const NkColor4f& color);

                void BuildOrthoMVP(float out[16]) const;
                void BuildBillboardMVP(const NkVec3f& worldPos, float worldW, float worldH,
                                        const NkMat4f& view, const NkMat4f& proj,
                                        float out[16]) const;
        };

        // =============================================================================
        // NkFontLibrary — gestionnaire de polices
        // =============================================================================
        class NkFontLibrary {
            public:
                static NkFontLibrary& Get();

                void Init(NkIDevice* device);
                void Shutdown();

                NkFont* Load(const char* path, uint32 size = 24);
                NkFont* FindOrLoad(const char* name, uint32 size = 24);
                NkFont* GetDefault(uint32 size = 24) const;
                void    SetSearchPath(const char* path) { mSearchPath = path; }

            private:
                NkFontLibrary() = default;
                NkIDevice*   mDevice = nullptr;
                NkString     mSearchPath;
                NkUnorderedMap<NkString, NkFont*> mFonts;
                NkFont* mDefault = nullptr;

                NkString FindFontFile(const char* name) const;
        };

    } // namespace renderer
} // namespace nkentseu
