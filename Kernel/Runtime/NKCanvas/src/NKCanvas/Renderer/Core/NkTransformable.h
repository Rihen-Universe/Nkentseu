#pragma once
// =============================================================================
// NkTransformable.h — Base class fournissant position/rotation/scale/origin
//
// Equivalent fonctionnel de sf::Transformable (SFML) avec nomenclature
// Nkentseu. Toute classe drawable qui veut etre positionnable/anime herite de
// NkTransformable et appelle GetTransform() pour obtenir une NkTransform a
// composer avec son geometry.
//
// MODELE
//   - position : translation finale dans le monde
//   - rotation : angle (radians) autour de `origin` (pivot)
//   - scale    : facteurs d'echelle autour de `origin`
//   - origin   : pivot local (en coords du drawable, ex. son centre pour une
//                rotation autour du centre)
//
// COMPOSITION : T = Translate(position) * Rotate(rotation) * Scale(scale) * Translate(-origin)
//   Applique a un point local p :
//     world = position + Rotate(scale * (p - origin))
//
// CACHE
//   La NkTransform est recalculee uniquement quand un setter/mutator marque
//   l'etat sale (mDirty/mInverseDirty). Acces GetTransform() ulterieurs en
//   O(1) sans recalcul. Les fields sont `mutable` pour permettre const-correctness.
//
// USAGE
//   class NkSprite : public NkTransformable, public NkDrawable {
//       void Draw(NkRenderTarget& target, NkRenderStates states) const override {
//           states.transform *= GetTransform();
//           target.Draw(mVertices, states);
//       }
//   };
// =============================================================================

#include "NKCanvas/Renderer/Core/NkTransform.h"
#include "NKCanvas/Renderer/Core/NkRenderer2DTypes.h" // NkVec2f
#include "NKMath/NKMath.h"

namespace nkentseu {
    namespace renderer {

        class NkTransformable {
            public:
                NkTransformable() noexcept = default;
                virtual ~NkTransformable() noexcept = default;

                NkTransformable(const NkTransformable&)            = default;
                NkTransformable(NkTransformable&&) noexcept        = default;
                NkTransformable& operator=(const NkTransformable&) = default;
                NkTransformable& operator=(NkTransformable&&) noexcept = default;

                // ── Setters ─────────────────────────────────────────────────────

                /// Place le drawable a la position monde (x, y).
                void SetPosition(const NkVec2f& position) noexcept {
                    mPosition = position;
                    mDirty = true; mInverseDirty = true;
                }
                void SetPosition(float32 x, float32 y) noexcept { SetPosition({x, y}); }

                /// Fixe l'angle de rotation (radians) autour de `origin`.
                void SetRotation(float32 angleRadians) noexcept {
                    mRotation = angleRadians;
                    mDirty = true; mInverseDirty = true;
                }

                /// Fixe les facteurs d'echelle autour de `origin`.
                void SetScale(const NkVec2f& scale) noexcept {
                    mScale = scale;
                    mDirty = true; mInverseDirty = true;
                }
                void SetScale(float32 sx, float32 sy) noexcept { SetScale({sx, sy}); }

                /// Fixe le pivot local (rotation et scale se font autour).
                /// Exemple : pour un sprite 64x64 que tu veux faire tourner
                /// autour de son centre, SetOrigin({32, 32}).
                void SetOrigin(const NkVec2f& origin) noexcept {
                    mOrigin = origin;
                    mDirty = true; mInverseDirty = true;
                }
                void SetOrigin(float32 x, float32 y) noexcept { SetOrigin({x, y}); }

                // ── Getters ─────────────────────────────────────────────────────

                NkVec2f GetPosition() const noexcept { return mPosition; }
                float32 GetRotation() const noexcept { return mRotation; }
                NkVec2f GetScale()    const noexcept { return mScale;    }
                NkVec2f GetOrigin()   const noexcept { return mOrigin;   }

                // ── Mutators incrementaux ───────────────────────────────────────

                /// Translation : position += offset.
                void Move(const NkVec2f& offset) noexcept {
                    mPosition.x += offset.x; mPosition.y += offset.y;
                    mDirty = true; mInverseDirty = true;
                }
                void Move(float32 dx, float32 dy) noexcept { Move({dx, dy}); }

                /// Rotation cumulative : rotation += delta.
                void Rotate(float32 deltaRadians) noexcept {
                    mRotation += deltaRadians;
                    mDirty = true; mInverseDirty = true;
                }

                /// Scale cumulatif (multiplicatif) : scale *= factors.
                void Scale(const NkVec2f& factors) noexcept {
                    mScale.x *= factors.x; mScale.y *= factors.y;
                    mDirty = true; mInverseDirty = true;
                }
                void Scale(float32 sx, float32 sy) noexcept { Scale({sx, sy}); }

                // ── Acces a la transformation calculee (avec cache) ─────────────

                /// Retourne la NkTransform combinee, recalculee si necessaire.
                ///
                /// Matrice cible (SFML-style) :
                ///   T = Translate(position) * Rotate(rotation) * Scale(scale) * Translate(-origin)
                ///
                /// Forme 3x3 affine developpee :
                ///   | sx*cos   -sy*sin   px - ox*sx*cos + oy*sy*sin |
                ///   | sx*sin    sy*cos   py - ox*sx*sin - oy*sy*cos |
                ///   |   0         0                  1                |
                ///
                /// Le ctor NkTransform prend (a00, a01, a02, a10, a11, a12) :
                ///   a00 = sx*cos = sxc
                ///   a01 = -sy*sin = -sys     ← fix 2026-05-30 : etait -syc (FAUX, dégénerait)
                ///   a02 = tx
                ///   a10 = sx*sin = sxs
                ///   a11 = sy*cos = syc       ← fix 2026-05-30 : etait sys (FAUX, dégénerait)
                ///   a12 = ty
                ///
                /// Bug d'origine : avec rotation=0, scale=(1,1) on obtenait sys=0
                /// et syc=1, et le code mettait m01=-syc=-1, m11=sys=0 → matrice
                /// dégénérée transformant tout en ligne Y=ty. Conséquence :
                /// paddles NkRectangleShape invisibles (vertices alignés en ligne),
                /// même symptôme sur ball/dashes/circles. Les scores marchent car
                /// DrawFilledRect ne passe PAS par NkTransformable.
                const NkTransform& GetTransform() const noexcept {
                    if (mDirty) {
                        const float32 c   = math::NkCos(mRotation);
                        const float32 s   = math::NkSin(mRotation);
                        const float32 sxc = mScale.x * c;
                        const float32 sxs = mScale.x * s;
                        const float32 syc = mScale.y * c;
                        const float32 sys = mScale.y * s;
                        const float32 tx = -mOrigin.x * sxc + mOrigin.y * sys + mPosition.x;
                        const float32 ty = -mOrigin.x * sxs - mOrigin.y * syc + mPosition.y;
                        mCachedTransform = NkTransform(
                            sxc, -sys, tx,
                            sxs,  syc, ty
                        );
                        mDirty = false;
                    }
                    return mCachedTransform;
                }

                /// Retourne l'inverse de la transformation (utile pour mapper
                /// des coords ecran vers local). Cache distinct.
                const NkTransform& GetInverseTransform() const noexcept {
                    if (mInverseDirty) {
                        mCachedInverse = GetTransform().GetInverse();
                        mInverseDirty = false;
                    }
                    return mCachedInverse;
                }

            private:
                NkVec2f mPosition{0.f, 0.f};
                float32 mRotation{0.f};        ///< radians
                NkVec2f mScale{1.f, 1.f};
                NkVec2f mOrigin{0.f, 0.f};

                mutable NkTransform mCachedTransform;
                mutable NkTransform mCachedInverse;
                mutable bool        mDirty{true};
                mutable bool        mInverseDirty{true};
        };

    } // namespace renderer
} // namespace nkentseu
