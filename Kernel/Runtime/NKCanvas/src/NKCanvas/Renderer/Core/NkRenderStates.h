#pragma once
// =============================================================================
// NkRenderStates.h — Etat de rendu compose passe au draw (style SFML)
//
// Equivalent de sf::RenderStates : conteneur POD-ish des 4 etats qui pilotent
// un draw call dans NKCanvas :
//   - transform : matrice 2D affine appliquee aux vertices avant projection.
//                 Permet la composition parent->enfant (parent_state.transform
//                 multipliee par self.GetTransform()).
//   - blendMode : mode de blending (alpha, additive, multiply, none).
//   - texture   : texture courante (nullptr = couleur vertex uniquement).
//   - shader    : shader courant (nullptr = pipeline 2D par defaut du backend).
//
// USAGE
//   NkRenderStates states;
//   states.transform = parentTransform;
//   states.texture   = &mTexture;
//   target.Draw(va, states);
//
//   Par defaut, NkRenderStates() construit l'etat « pas de transform, alpha
//   blend, pas de texture, pas de shader ». Recuperable via
//   NkRenderStates::Default().
//
// FUTURS AJOUTS
//   - NkShader (systeme pas encore livre dans NKCanvas) : pour l'instant
//     forward-declare ; quand le module sera la, il suffira d'inclure son
//     header dans le .cpp consommateur.
// =============================================================================

#include "NkTransform.h"
#include "NkRenderer2DTypes.h" // NkBlendMode

namespace nkentseu {
    namespace renderer {

        class NkTexture; // NKCanvas/Renderer/Resources/NkTexture.h
        class NkShader;  // pas encore implemente dans NKCanvas — placeholder

        struct NkRenderStates {
            NkTransform        transform;
            NkBlendMode        blendMode = NkBlendMode::NK_ALPHA;
            const NkTexture*   texture   = nullptr;
            const NkShader*    shader    = nullptr;

            /// Constructeur par defaut : tout aux valeurs neutres.
            NkRenderStates() noexcept = default;

            /// Construction depuis une transform seule (sucre courant).
            explicit NkRenderStates(const NkTransform& t) noexcept : transform(t) {}

            /// Construction depuis une texture seule (cas drawables textures).
            explicit NkRenderStates(const NkTexture* tex) noexcept : texture(tex) {}

            /// Construction depuis un blend mode seul.
            explicit NkRenderStates(NkBlendMode bm) noexcept : blendMode(bm) {}

            /// Construction depuis un shader seul.
            explicit NkRenderStates(const NkShader* sh) noexcept : shader(sh) {}

            /// Etat par defaut (factory accessible en const&).
            static const NkRenderStates& Default() noexcept {
                static const NkRenderStates kDefault;
                return kDefault;
            }
        };

    } // namespace renderer
} // namespace nkentseu
