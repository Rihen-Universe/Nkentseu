#pragma once
// =============================================================================
// NkDrawable.h — Interface abstraite des objets dessinables (style SFML)
//
// Equivalent de sf::Drawable. Toute classe qui peut etre rendue par un
// NkRenderTarget (NkRenderWindow ou NkRenderTexture) en herite et implemente
// la methode Draw(target, states).
//
// SIGNATURE
//   void Draw(NkRenderTarget& target, const NkRenderStates& states) const
//
//   - target : la cible de rendu (fenetre, texture offscreen). Ses methodes
//              concretes (target.Draw(vertices, count, primitive, states))
//              effectuent le veritable submit batch.
//   - states : etat compose passe par l'appelant. Le drawable peut le copier
//              et le modifier localement (typiquement : states.transform *=
//              GetTransform() s'il herite aussi de NkTransformable, pour
//              empiler son propre transform sur celui du parent).
//
// COMPOSITION TRANSFORMS — pattern recommande
//   void NkSprite::Draw(NkRenderTarget& target, const NkRenderStates& states) const {
//       NkRenderStates local = states;
//       local.transform *= GetTransform();   // applique notre transform
//       local.texture    = mTexture;          // attache notre texture
//       target.Draw(mVertices, 4, NkPrimitiveType::NK_TRIANGLE_STRIP, local);
//   }
//
// COEXISTENCE AVEC NkIDrawable2D (heritage)
//   L'ancienne interface `NkIDrawable2D` (signature Draw(NkIRenderer2D&)) est
//   conservee pour la compat des sous-systemes existants (NkSprite, NkText,
//   les 5 backends). La migration vers NkDrawable se fait en etape A.8.
//   NkRenderTarget exposera des overloads acceptant les deux interfaces le
//   temps de la transition.
// =============================================================================

namespace nkentseu {
    namespace renderer {

        class NkRenderTarget;   // NkRenderTarget.h — pas encore livre (etape A.5)
        struct NkRenderStates;  // NkRenderStates.h  — etat compose (etape A.2, fait)

        class NkDrawable {
            public:
                virtual ~NkDrawable() noexcept = default;

                /// Methode de rendu : le drawable se dessine sur `target`, en
                /// composant son propre etat avec `states`.
                /// `const` : un drawable ne mute pas pendant son rendu ;
                /// les caches mutables (NkTransformable) restent valides.
                virtual void Draw(NkRenderTarget& target,
                                  const NkRenderStates& states) const = 0;
        };

    } // namespace renderer
} // namespace nkentseu
