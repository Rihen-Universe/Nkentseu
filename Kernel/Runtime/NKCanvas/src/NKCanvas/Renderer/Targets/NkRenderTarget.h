#pragma once
// =============================================================================
// NkRenderTarget.h — Cible de rendu 2D abstraite (style SFML)
//
// Equivalent de sf::RenderTarget. Surface generique sur laquelle on peut
// rendre des NkDrawable. Deux implementations concretes :
//   - NkRenderWindow  : rend dans la surface d'une NkWindow (back buffer)
//   - NkRenderTexture : rend dans une texture offscreen (post-process, mini-map, …)
//
// HIERARCHIE
//   NkRenderTarget (abstract)
//     ├── NkRenderWindow   (concrete : wrappe NkWindow + NkIGraphicsContext + NkIRenderer2D)
//     └── NkRenderTexture  (concrete : framebuffer offscreen ; livre en stub pour l'instant)
//
// USAGE
//   NkRenderWindow target(window);
//   target.Clear({30, 30, 30, 255});
//     NkSprite sprite(myTexture);
//     sprite.SetPosition({100, 100});
//     target.Draw(sprite);
//
//     NkVertexArray va(NkPrimitiveType::NK_TRIANGLES, 3);
//     va[0] = NkVertex{ 0,  0, 0, 0, 255,0,0,255};
//     va[1] = NkVertex{50,  0, 0, 0, 0,255,0,255};
//     va[2] = NkVertex{25, 50, 0, 0, 0,0,255,255};
//     target.Draw(va);
//   target.Display();
//
// DUAL-INTERFACE DRAWABLE
//   En attendant la migration NkSprite/NkText (etape A.8), NkRenderTarget
//   expose des overloads pour les deux interfaces :
//     - Draw(const NkDrawable&, NkRenderStates) — nouveau pattern
//     - Draw(const NkIDrawable2D&)              — ancien pattern (delegue au renderer interne)
// =============================================================================

#include "NKCanvas/Renderer/Core/NkRenderer2DTypes.h"
#include "NKCanvas/Renderer/Core/NkRenderStates.h"
#include "NKCanvas/Renderer/Core/NkDrawable.h"

namespace nkentseu {
    namespace renderer {

        class NkIRenderer2D;
        class NkIDrawable2D;
        class NkVertexArray;
        class NkRenderer2D;

        class NkRenderTarget {
            public:
                virtual ~NkRenderTarget() noexcept = default;

                // ── Frame lifecycle ────────────────────────────────────────────

                /// Vide le framebuffer avec la couleur indiquee.
                virtual void Clear(const NkColor2D& color = NkColor2D::Black) = 0;

                /// Presente le rendu (swap buffers pour RenderWindow, finalise
                /// pour RenderTexture). A appeler en fin de frame.
                virtual void Display() = 0;

                // ── Camera (view) ──────────────────────────────────────────────

                virtual void     SetView(const NkView2D& view) = 0;
                virtual NkView2D GetView()       const = 0;
                virtual NkView2D GetDefaultView() const = 0;

                // ── Viewport (sous-region pixel ou est rendu le view) ──────────

                virtual void     SetViewport(NkRect2i viewport) = 0;
                virtual NkRect2i GetViewport() const = 0;

                // ── Dimensions du target en pixels ─────────────────────────────

                virtual math::NkVec2u GetSize() const = 0;

                // ── API Draw — nouvelle interface NkDrawable ───────────────────

                /// Dessine un NkDrawable avec un etat compose donne. Le drawable
                /// est responsable de l'appel a target.Draw(vertices, …).
                void Draw(const NkDrawable& drawable,
                          const NkRenderStates& states = NkRenderStates::Default()) {
                    drawable.Draw(*this, states);
                }

                /// Dessine un NkVertexArray (raccourci : delegue au submit raw vertices).
                void Draw(const NkVertexArray& va,
                          const NkRenderStates& states = NkRenderStates::Default());

                /// Submit raw : envoie un tableau de vertices au backend, en
                /// appliquant le primitive + l'etat indique. C'est le point
                /// d'entree bas niveau qu'utilisent NkDrawable::Draw().
                virtual void Draw(const NkVertex* vertices,
                                  uint32 count,
                                  NkPrimitiveType primitive,
                                  const NkRenderStates& states = NkRenderStates::Default()) = 0;

                // ── API Draw — compat ancienne interface NkIDrawable2D ─────────

                /// Compat : un drawable ancien style (NkIDrawable2D::Draw(NkIRenderer2D&)).
                /// Delegue au renderer interne. Sera retire quand NkSprite/NkText
                /// auront migre vers NkDrawable (etape A.8).
                void Draw(const NkIDrawable2D& drawable);

                // ── Mapping pixel <-> coords monde ─────────────────────────────

                virtual NkVec2f MapPixelToCoords(NkVec2i pixel) const = 0;
                virtual NkVec2i MapCoordsToPixel(NkVec2f point) const = 0;

                // ── Acces au renderer bas niveau (avance / interop backends) ───

                virtual NkIRenderer2D*       GetRenderer()       noexcept = 0;
                virtual const NkIRenderer2D* GetRenderer() const noexcept = 0;

                /// Acces a la facade user-facing NkRenderer2D (api SFML-friendly,
                /// inclut Draw(NkDrawable&) avec dispatch correct vers *this).
                virtual NkRenderer2D&        GetRenderer2D()       noexcept = 0;
                virtual const NkRenderer2D&  GetRenderer2D() const noexcept = 0;

                // ── Capture d'ecran (readback GPU -> fichier image) ─────────────
                /// Lit le contenu rendu de la cible et l'enregistre dans `path`. Le
                /// format est deduit de l'extension (.png/.jpg/.bmp/.tga/... via
                /// NkImage). A appeler APRES Display() (la frame doit etre presentee).
                /// Defaut : non supporte (false) ; surcharge par cible/backend.
                /// Sert au debug par capture et a l'export d'image cote utilisateur.
                virtual bool Capture(const char* path) const { return false; }
        };

    } // namespace renderer
} // namespace nkentseu
