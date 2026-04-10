#pragma once
// =============================================================================
// NkRendererCommand.h
// Accumulateur de draw calls pour un pass de rendu.
//
// Le NkRendererCommand découple la soumission des objets à rendre
// de l'exécution GPU :
//   1. L'application appelle Draw2D / Draw3D / DrawText / DrawLine…
//   2. Les commandes sont stockées dans des buffers de tri.
//   3. À la fin du pass, le renderer les trie par (layer → material → depth)
//      et les soumet au NkICommandBuffer RHI.
//
// Cette approche est inspirée du render queue d'Unreal Engine :
//   Draw calls 2D triés par layer puis profondeur (painter's algorithm)
//   Draw calls 3D triés par material, puis front-to-back (opaque)
//              ou back-to-front (transparent)
//   Texte rendu en dernier sur un layer overlay dédié
// =============================================================================
#include "NkRendererTypes.h"

namespace nkentseu {
namespace renderer {

    // =========================================================================
    // Commande de lumière (pour l'éclairage 3D dans le renderer)
    // =========================================================================
    enum class NkLightType : uint8 {
        Directional = 0,
        Point       = 1,
        Spot        = 2,
        Area        = 3,
    };

    struct NkLightDesc {
        NkLightType type      = NkLightType::Directional;
        NkVec3f     position  = {0.f, 5.f, 0.f};
        NkVec3f     direction = {0.f,-1.f, 0.f};
        NkVec3f     color     = {1.f, 1.f, 1.f};
        float32     intensity = 1.f;
        float32     radius    = 10.f;    // point / spot
        float32     innerAngle= 15.f;   // spot inner cone (degrees)
        float32     outerAngle= 30.f;   // spot outer cone (degrees)
        bool        castShadow= false;
    };

    // =========================================================================
    // Commandes de debug/gizmos (toujours en dernier, wireframe)
    // =========================================================================
    struct NkDebugLine {
        NkVec3f start, end;
        NkVec4f color    = {1.f,0.f,0.f,1.f};
        float32 thickness= 1.f;
        bool    depthTest= false;
    };

    struct NkDebugAABB {
        NkVec3f min, max;
        NkVec4f color = {0.f,1.f,0.f,1.f};
        bool    depthTest = false;
    };

    struct NkDebugSphere {
        NkVec3f center;
        float32 radius;
        NkVec4f color = {0.f,0.f,1.f,1.f};
        bool    depthTest = false;
    };

    // =========================================================================
    // NkRendererCommand
    // =========================================================================
    class NkRendererCommand {
    public:
        NkRendererCommand() = default;
        ~NkRendererCommand() { Reset(); }

        NkRendererCommand(const NkRendererCommand&) = delete;
        NkRendererCommand& operator=(const NkRendererCommand&) = delete;

        // ── Réinitialisation (appelée par le renderer en début de pass) ────────
        void Reset();

        // ── 2D ───────────────────────────────────────────────────────────────

        // Quad texturé
        void Draw2D(const NkDrawCall2D& call);

        // Sprite (shortcut : mesh quad implicite)
        void DrawSprite(NkTextureHandle tex,
                        NkVec2f position, NkVec2f size,
                        NkVec4f color = {1.f,1.f,1.f,1.f},
                        float32 rotation = 0.f,
                        float32 depth = 0.f, uint32 layer = 0);

        // Rectangle plein
        void DrawRect2D(NkVec2f position, NkVec2f size,
                        NkVec4f color,
                        float32 depth = 0.f, uint32 layer = 0);

        // Rectangle en wireframe
        void DrawRectOutline2D(NkVec2f position, NkVec2f size,
                               NkVec4f color, float32 thickness = 1.f,
                               float32 depth = 0.f, uint32 layer = 0);

        // Cercle plein
        void DrawCircle2D(NkVec2f center, float32 radius,
                          NkVec4f color, uint32 segments = 32,
                          float32 depth = 0.f, uint32 layer = 0);

        // Ligne 2D
        void DrawLine2D(NkVec2f start, NkVec2f end,
                        NkVec4f color, float32 thickness = 1.f,
                        float32 depth = 0.f, uint32 layer = 0);

        // ── 3D ───────────────────────────────────────────────────────────────
        void Draw3D(const NkDrawCall3D& call);

        // Shortcut : mesh + material instance + transform world
        void Draw3D(NkMeshHandle mesh, NkMaterialInstHandle mat,
                    const NkMat4f& transform,
                    bool castShadow=true, bool receiveShadow=true,
                    uint32 layer=0);

        // ── Lumières ─────────────────────────────────────────────────────────
        // Les lumières sont collectées ici et passées au renderer
        // (implémentation forward ou deferred selon le backend).
        void AddLight(const NkLightDesc& light);

        // Shortcut lumière directionnelle
        void AddDirectionalLight(NkVec3f direction, NkVec3f color, float32 intensity=1.f,
                                  bool castShadow=false);

        // Shortcut lumière ponctuelle
        void AddPointLight(NkVec3f position, NkVec3f color,
                           float32 intensity=1.f, float32 radius=10.f,
                           bool castShadow=false);

        // ── Texte ─────────────────────────────────────────────────────────────
        void DrawText(const NkTextDesc& desc);
        void DrawText(NkFontHandle font, const char* text,
                      NkVec2f position, float32 size=0.f,
                      NkVec4f color={1.f,1.f,1.f,1.f},
                      float32 maxWidth=0.f, uint32 layer=0);

        // ── Debug / Gizmos ────────────────────────────────────────────────────
        void DrawDebugLine  (const NkDebugLine& line);
        void DrawDebugLine  (NkVec3f start, NkVec3f end,
                             NkVec4f color={1.f,0.f,0.f,1.f},
                             float32 thickness=1.f, bool depthTest=false);
        void DrawDebugAABB  (const NkDebugAABB& aabb);
        void DrawDebugAABB  (NkVec3f min, NkVec3f max,
                             NkVec4f color={0.f,1.f,0.f,1.f},
                             bool depthTest=false);
        void DrawDebugSphere(const NkDebugSphere& sphere);
        void DrawDebugSphere(NkVec3f center, float32 radius,
                             NkVec4f color={0.f,0.f,1.f,1.f},
                             bool depthTest=false);
        void DrawDebugCross (NkVec3f position, float32 size=0.1f,
                             NkVec4f color={1.f,1.f,0.f,1.f},
                             bool depthTest=false);
        void DrawDebugGrid  (NkVec3f origin, NkVec3f normalAxis,
                             uint32 cells=10, float32 cellSize=1.f,
                             NkVec4f color={0.4f,0.4f,0.4f,1.f});

        // ── Accès interne (réservé au renderer) ───────────────────────────────
        // Appelé par l'implémentation du renderer pour trier et soumettre.
        void Sort();

        const NkVector<NkDrawCall2D>&  GetDrawCalls2D()  const { return mCalls2D; }
        const NkVector<NkDrawCall3D>&  GetDrawCalls3D()  const { return mCalls3D; }
        const NkVector<NkLightDesc>&   GetLights()       const { return mLights; }
        const NkVector<NkTextDesc>&    GetTextCalls()    const { return mTextCalls; }
        const NkVector<NkDebugLine>&   GetDebugLines()   const { return mDebugLines; }
        const NkVector<NkDebugAABB>&   GetDebugAABBs()   const { return mDebugAABBs; }
        const NkVector<NkDebugSphere>& GetDebugSpheres() const { return mDebugSpheres; }

        uint32 TotalDrawCalls() const {
            return (uint32)(mCalls2D.Size() + mCalls3D.Size());
        }

        // Données de sprite accumulées (quad 2D implicite)
        struct SpriteCall {
            NkTextureHandle tex;
            NkVec2f         position;
            NkVec2f         size;
            NkVec4f         color;
            float32         rotation;
            float32         depth;
            uint32          layer;
        };
        const NkVector<SpriteCall>& GetSpriteCalls() const { return mSpriteCalls; }

        // Données de primitives 2D (rects, cercles, lignes)
        struct Prim2DCall {
            enum class Kind : uint8 { RectFill, RectOutline, Circle, Line } kind;
            NkVec2f p0, p1;   // position/size ou start/end
            NkVec4f color;
            float32 extra;    // thickness ou radius selon le kind
            uint32  segments; // pour cercle
            float32 depth;
            uint32  layer;
        };
        const NkVector<Prim2DCall>& GetPrim2DCalls() const { return mPrim2D; }

    private:
        NkVector<NkDrawCall2D>   mCalls2D;
        NkVector<NkDrawCall3D>   mCalls3D;
        NkVector<SpriteCall>     mSpriteCalls;
        NkVector<Prim2DCall>     mPrim2D;
        NkVector<NkLightDesc>    mLights;
        NkVector<NkTextDesc>     mTextCalls;
        NkVector<NkDebugLine>    mDebugLines;
        NkVector<NkDebugAABB>    mDebugAABBs;
        NkVector<NkDebugSphere>  mDebugSpheres;

        bool mSorted = false;

        // Clé de tri 64-bit : [layer 8b][material 24b][depth 32b]
        static uint64 SortKey2D(const NkDrawCall2D& c);
        static uint64 SortKey3D(const NkDrawCall3D& c, bool transparent);
    };

} // namespace renderer
} // namespace nkentseu