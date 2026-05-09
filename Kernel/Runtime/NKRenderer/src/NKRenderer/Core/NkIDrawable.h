#pragma once
// =============================================================================
// NkIDrawable.h  — NKRenderer v5.0  (Core/)
//
// Interface UE-style pour tout objet qui produit des draw calls. Le scene
// graph itere sur ses NkIDrawable visibles et appelle Submit() — l'objet
// decide quoi pousser (3D mesh, sprite, particles, debug lines, etc.).
//
// Decouple le scene graph (qui gere transforms/visibilite/parent) du
// renderer (qui gere les sous-systemes : Render2D, Render3D, VFX, ...).
// L'editeur futur (Nogee/) iterera sur les NkIDrawable des assets en scene
// pour les afficher en preview.
// =============================================================================
#include "NkRendererTypes.h"
#include "NkSceneContext.h"

namespace nkentseu {
    namespace renderer {

        class NkRenderer;

        // Categorie de visibilite — le scene graph filtre par categorie
        // (ex : geometry passes ne voient que NK_DRAWABLE_OPAQUE/TRANSPARENT,
        //  shadow pass ne voit que NK_DRAWABLE_OPAQUE avec castShadow=true,
        //  UI pass ne voit que NK_DRAWABLE_UI).
        enum class NkDrawableCategory : uint8 {
            NK_DRAWABLE_OPAQUE      = 0,   // mesh 3D opaque
            NK_DRAWABLE_TRANSPARENT = 1,   // mesh 3D blend (verre, glass, fog)
            NK_DRAWABLE_VFX         = 2,   // particles, trails, beams
            NK_DRAWABLE_2D          = 3,   // sprites, formes, UI
            NK_DRAWABLE_DEBUG       = 4,   // gizmos, axes, AABB
        };

        // =====================================================================
        // NkIDrawable
        // =====================================================================
        class NkIDrawable {
            public:
                virtual ~NkIDrawable() = default;

                // ── Identification ─────────────────────────────────────────────
                virtual NkDrawableCategory GetCategory() const noexcept = 0;

                // ── Visibilite / culling ───────────────────────────────────────
                // Si false, le scene graph skip Submit() pour cet objet.
                virtual bool IsVisible() const noexcept = 0;

                // AABB monde pour le frustum culling. Si non disponible (drawable
                // procedural ou fullscreen), retourner NkAABB::Infinite().
                virtual NkAABB GetWorldAABB() const noexcept = 0;

                // Doit-il projeter une ombre dans la passe shadow ?
                virtual bool CastsShadow() const noexcept { return false; }

                // ── Submit ─────────────────────────────────────────────────────
                // Le scene graph appelle Submit() pendant le frame, apres avoir
                // verifie IsVisible() + frustum culling. L'objet construit ses
                // NkDrawCall* et les soumet via le bon sous-systeme du renderer
                // (renderer->GetRender3D()->Submit(...) etc.).
                virtual void Submit(NkRenderer* renderer, const NkSceneContext& ctx) = 0;
        };

    } // namespace renderer
} // namespace nkentseu
