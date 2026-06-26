#pragma once
// -----------------------------------------------------------------------------
// @File    NkGuiDrawList.h
// @Brief   Liste de commandes de dessin NKGui — sortie indépendante du backend.
// @License Proprietary - Free to use and modify
//
// Géométrie (sommets + indices) + commandes (clip + texture). Un backend
// (NKCanvas 2D / NKRHI 3D) consomme ce flux. Mémoire via NKMemory (NkVector).
// Réécriture PROPRE (s'inspire du modèle prouvé NkUIDrawList, noms neufs).
// -----------------------------------------------------------------------------

#include "NKGui/NkGuiExport.h"
#include "NKGui/Core/NkGuiTypes.h"
#include "NKContainers/Sequential/NkVector.h"

namespace nkentseu {

    struct NkFont;   // NKFont (forward) — AddText émet des glyphes texturés

    namespace nkgui {

        // Sommet minimal pour le GPU. col = RGBA empaqueté (a<<24|b<<16|g<<8|r).
        struct NkGuiVertex {
            NkVec2 pos;
            NkVec2 uv;    ///< (0,0) = couleur unie (pas de texture)
            uint32 col;
        };

        enum class NkGuiDrawCmdType : uint8 {
            Triangles,          ///< triangles unis
            TexturedTriangles   ///< triangles texturés (texte/atlas/image)
        };

        struct NkGuiDrawCmd {
            NkGuiDrawCmdType type     = NkGuiDrawCmdType::Triangles;
            uint32           idxOffset= 0;
            uint32           idxCount = 0;
            uint32           texId    = 0;
            NkRect           clipRect = { 0.f, 0.f, 1.0e9f, 1.0e9f };
        };

        // Empaquetage couleur (cohérent avec le dépaquetage côté backend).
        NKENTSEU_NKGUI_API_INLINE uint32 NkGuiPackColor(const NkColor& c) noexcept {
            return (static_cast<uint32>(c.a) << 24) | (static_cast<uint32>(c.b) << 16)
                 | (static_cast<uint32>(c.g) <<  8) |  static_cast<uint32>(c.r);
        }

        struct NKENTSEU_NKGUI_CLASS_EXPORT NkGuiDrawList {
                NkVector<NkGuiVertex>  vtx;
                NkVector<uint32>       idx;
                NkVector<NkGuiDrawCmd> cmds;
                NkRect                 clipStack[32] = {};
                int32                  clipDepth     = 0;

                // ── Cycle de frame ────────────────────────────────────────────────
                void   Reset() noexcept;
                // Concatène `other` à la fin (décale les indices) — fusion des draw-lists
                // de fenêtres triées par z-order à EndFrame.
                void   Append(const NkGuiDrawList& other) noexcept;
                NkRect CurrentClip() const noexcept;
                void   PushClipRect(const NkRect& r, bool intersect = true) noexcept;
                void   PopClipRect() noexcept;

                // ── Primitives ────────────────────────────────────────────────────
                void AddRectFilled(const NkRect& r, const NkColor& col, float32 rounding = 0.f) noexcept;
                void AddRect(const NkRect& r, const NkColor& col, float32 thickness = 1.f) noexcept;
                // Rectangle à DÉGRADÉ (4 couleurs de coin) — carré SV / barres du color picker.
                void AddRectFilledMultiColor(const NkRect& r, const NkColor& tl, const NkColor& tr,
                                             const NkColor& br, const NkColor& bl) noexcept;
                // Quad TEXTURÉ (image/icône) : `texId` = texture backend, `uv0/uv1` = coins
                // UV, `tint` multiplie l'échantillon (blanc = telle quelle).
                void AddImage(uint32 texId, const NkRect& r, const NkVec2& uv0, const NkVec2& uv1,
                              const NkColor& tint) noexcept;
                void AddLine(const NkVec2& a, const NkVec2& b, const NkColor& col, float32 thickness = 1.f) noexcept;
                void AddTriangleFilled(const NkVec2& a, const NkVec2& b, const NkVec2& c, const NkColor& col) noexcept;
                // Triangle à DÉGRADÉ (3 couleurs de sommet) — roue de teinte + triangle SV.
                void AddTriangleMultiColor(const NkVec2& a, const NkVec2& b, const NkVec2& c,
                                           const NkColor& ca, const NkColor& cb, const NkColor& cc) noexcept;
                void AddCircleFilled(const NkVec2& center, float32 r, const NkColor& col, int32 segs = 0) noexcept;

                // Texte : émet des quads texturés (atlas `texId`) à partir de la
                // face NKFont. `baseline` = ligne de base du 1er glyphe. `maxWidth`
                // >= 0 tronque (coupe au glyphe qui déborde).
                void AddText(const NkFont* face, uint32 texId, const NkVec2& baseline,
                             const char* text, const NkColor& col, float32 maxWidth = -1.f) noexcept;
                // Dessine la sous-chaîne [begin, end) (sans troncature) — brique du
                // retour à la ligne (TextWrapped) qui passe des plages de ligne.
                void AddTextRange(const NkFont* face, uint32 texId, const NkVec2& baseline,
                                  const char* begin, const char* end, const NkColor& col) noexcept;

            private:
                NkGuiDrawCmd& CurCmd(uint32 texId) noexcept;
                uint32        Vtx(const NkVec2& p, const NkVec2& uv, uint32 col) noexcept;
                void          Tri(uint32 a, uint32 b, uint32 c, uint32 texId) noexcept;
        };

    } // namespace nkgui
} // namespace nkentseu
