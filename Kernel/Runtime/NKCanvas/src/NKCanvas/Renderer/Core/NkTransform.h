#pragma once
// =============================================================================
// NkTransform.h — Transformation affine 2D wrappant math::NkMat4f
//
// Equivalent fonctionnel de sf::Transform (SFML) avec nomenclature Nkentseu.
//
// STOCKAGE
//   Wrappe `math::NkMat4f` (16 float32 column-major, union avec data[16] /
//   mat[4][4] / m{col}{row} / col[4]). On profite ainsi des conventions et de
//   l'interop avec le reste du moteur (NKRenderer 3D, NKCamera, …) ; un
//   `NkTransform` 2D peut etre passe a une routine 3D sans conversion via
//   GetMatrix4().
//
//   Layout effectif (les composantes 2D affines sur les axes col0/col1/col3) :
//
//                              ┌                          ┐
//                              │ data[0]  data[4]  0  data[12] │
//                              │ data[1]  data[5]  0  data[13] │
//                              │   0        0      1     0     │
//                              │   0        0      0     1     │
//                              └                          ┘
//
// PERFORMANCE
//   Operations 2D specialisees : Combine fait 12 mul / 8 add (vs 64 mul / 48
//   add pour une multiplication 4x4 generale via `mMatrix * other.mMatrix`).
//   On accede aux floats via mMatrix.data[i] pour rester optimal.
//
// USAGE
//   NkTransform t;
//   t.Translate({100, 50}).Rotate(0.5f).Scale({2, 2});
//   NkVec2f world = t.TransformPoint({10, 0});
//   const float32* gpu = t.GetMatrix();    // 16 floats column-major → uniform
//   const math::NkMat4f& m4 = t.GetMatrix4();  // pour interop avec NKRenderer
//
// COMPOSITION
//   T = A * B applique B PUIS A (convention math : v' = T*v = A*(B*v)).
//   Pour un parent qui contient un enfant : world = parent * local.
// =============================================================================

#include "NKCore/NkTypes.h"
#include "NKMath/NKMath.h"   // math::NkMat4f, math::NkCos / NkSin
#include "NkRenderer2DTypes.h" // NkVec2f, NkRect2f

namespace nkentseu {
    namespace renderer {

        class NkTransform {
            public:
                // ── Construction ────────────────────────────────────────────────
                /// Construit la matrice identite (ctor NkMat4f(1, true) = identite).
                NkTransform() noexcept : mMatrix(1.f, true) {}

                /// Construit a partir des 6 composantes 2D affines.
                /// Ordre : a00, a01, a02, a10, a11, a12 (lignes 1 et 2 du 3x3 affine).
                /// La 3eme ligne est implicitement (0, 0, 1) ; la 4eme dimension
                /// 4x4 reste identite (m22 = m33 = 1, le reste a 0).
                NkTransform(float32 a00, float32 a01, float32 a02,
                            float32 a10, float32 a11, float32 a12) noexcept
                    : mMatrix(1.f, true) {
                    mMatrix.data[ 0] = a00; mMatrix.data[ 4] = a01; mMatrix.data[12] = a02;
                    mMatrix.data[ 1] = a10; mMatrix.data[ 5] = a11; mMatrix.data[13] = a12;
                }

                /// Singleton identite (equivalent sf::Transform::Identity).
                static const NkTransform& Identity() noexcept {
                    static const NkTransform kId;
                    return kId;
                }

                // ── Accesseurs ──────────────────────────────────────────────────
                /// Pointeur vers les 16 floats column-major (pour upload GPU).
                const float32* GetMatrix() const noexcept { return mMatrix.data; }

                /// Acces direct a la math::NkMat4f sous-jacente (interop NKRenderer/NKCamera).
                const math::NkMat4f& GetMatrix4() const noexcept { return mMatrix; }
                math::NkMat4f&       GetMatrix4()       noexcept { return mMatrix; }

                // ── Operations 2D affines (mutent *this, retournent *this) ──────

                /// Combine cette transformation avec `other` (this = this * other).
                /// L'ordre d'application apres combine : `other` PUIS `this`.
                /// 12 mul / 8 add (specialise 2D, vs 64 mul / 48 add pour Mat4 generale).
                NkTransform& Combine(const NkTransform& other) noexcept {
                    const float32* a = mMatrix.data;
                    const float32* b = other.mMatrix.data;
                    // Indices : data[col*4 + row], donc col0=indices 0-3, col1=4-7, col3=12-15.
                    const float32 r00 = a[0]*b[0] + a[4]*b[1];
                    const float32 r01 = a[0]*b[4] + a[4]*b[5];
                    const float32 r02 = a[0]*b[12] + a[4]*b[13] + a[12];
                    const float32 r10 = a[1]*b[0] + a[5]*b[1];
                    const float32 r11 = a[1]*b[4] + a[5]*b[5];
                    const float32 r12 = a[1]*b[12] + a[5]*b[13] + a[13];
                    mMatrix.data[ 0] = r00; mMatrix.data[ 4] = r01; mMatrix.data[12] = r02;
                    mMatrix.data[ 1] = r10; mMatrix.data[ 5] = r11; mMatrix.data[13] = r12;
                    return *this;
                }

                /// Translation par `offset`.
                NkTransform& Translate(NkVec2f offset) noexcept {
                    mMatrix.data[12] += mMatrix.data[0] * offset.x + mMatrix.data[4] * offset.y;
                    mMatrix.data[13] += mMatrix.data[1] * offset.x + mMatrix.data[5] * offset.y;
                    return *this;
                }

                /// Rotation d'`angleRadians` autour de l'origine (0, 0).
                NkTransform& Rotate(float32 angleRadians) noexcept {
                    const float32 c = math::NkCos(angleRadians);
                    const float32 s = math::NkSin(angleRadians);
                    const float32 r00 = mMatrix.data[0] * c    + mMatrix.data[4] * s;
                    const float32 r01 = mMatrix.data[0] * (-s) + mMatrix.data[4] * c;
                    const float32 r10 = mMatrix.data[1] * c    + mMatrix.data[5] * s;
                    const float32 r11 = mMatrix.data[1] * (-s) + mMatrix.data[5] * c;
                    mMatrix.data[0] = r00; mMatrix.data[4] = r01;
                    mMatrix.data[1] = r10; mMatrix.data[5] = r11;
                    return *this;
                }

                /// Rotation d'`angleRadians` autour du point `center`.
                NkTransform& Rotate(float32 angleRadians, NkVec2f center) noexcept {
                    return Translate(center).Rotate(angleRadians).Translate({-center.x, -center.y});
                }

                /// Scale par `factors` autour de l'origine (0, 0).
                NkTransform& Scale(NkVec2f factors) noexcept {
                    mMatrix.data[0] *= factors.x; mMatrix.data[4] *= factors.y;
                    mMatrix.data[1] *= factors.x; mMatrix.data[5] *= factors.y;
                    return *this;
                }

                /// Scale par `factors` autour du point `center`.
                NkTransform& Scale(NkVec2f factors, NkVec2f center) noexcept {
                    return Translate(center).Scale(factors).Translate({-center.x, -center.y});
                }

                // ── Application de la transformation ────────────────────────────

                /// Transforme un point 2D : retourne T * (point, 1).
                NkVec2f TransformPoint(NkVec2f point) const noexcept {
                    return NkVec2f(
                        mMatrix.data[0] * point.x + mMatrix.data[4] * point.y + mMatrix.data[12],
                        mMatrix.data[1] * point.x + mMatrix.data[5] * point.y + mMatrix.data[13]
                    );
                }

                /// Transforme une AABB. Calcule la AABB englobante de la AABB
                /// d'entree transformee (peut etre plus grande si rotation/skew).
                NkRect2f TransformRect(const NkRect2f& rect) const noexcept {
                    const NkVec2f corners[4] = {
                        TransformPoint({rect.left,              rect.top                }),
                        TransformPoint({rect.left + rect.width, rect.top                }),
                        TransformPoint({rect.left,              rect.top + rect.height  }),
                        TransformPoint({rect.left + rect.width, rect.top + rect.height  })
                    };
                    float32 minX = corners[0].x, maxX = corners[0].x;
                    float32 minY = corners[0].y, maxY = corners[0].y;
                    for (int32 i = 1; i < 4; ++i) {
                        if (corners[i].x < minX) minX = corners[i].x; else if (corners[i].x > maxX) maxX = corners[i].x;
                        if (corners[i].y < minY) minY = corners[i].y; else if (corners[i].y > maxY) maxY = corners[i].y;
                    }
                    return NkRect2f(minX, minY, maxX - minX, maxY - minY);
                }

                // ── Inverse ─────────────────────────────────────────────────────

                /// Retourne la matrice inverse (transformation reciproque).
                /// Specialisee 2D affine. Retourne l'identite si determinant nul.
                NkTransform GetInverse() const noexcept {
                    const float32 a  = mMatrix.data[0],  b  = mMatrix.data[4];
                    const float32 c  = mMatrix.data[1],  d  = mMatrix.data[5];
                    const float32 tx = mMatrix.data[12], ty = mMatrix.data[13];
                    const float32 det = a * d - b * c;
                    if (det == 0.f) return Identity();
                    const float32 invDet = 1.f / det;
                    return NkTransform(
                          d * invDet, -b * invDet, -(d * tx - b * ty) * invDet,
                         -c * invDet,  a * invDet, -(-c * tx + a * ty) * invDet
                    );
                }

                // ── Operateurs ──────────────────────────────────────────────────

                /// Multiplication (composition) : retourne *this * other.
                NkTransform operator*(const NkTransform& other) const noexcept {
                    NkTransform r(*this);
                    r.Combine(other);
                    return r;
                }

                /// Composition en place : *this = *this * other.
                NkTransform& operator*=(const NkTransform& other) noexcept {
                    return Combine(other);
                }

                /// Transforme un point (raccourci).
                NkVec2f operator*(NkVec2f point) const noexcept {
                    return TransformPoint(point);
                }

                /// Egalite stricte composante par composante.
                bool operator==(const NkTransform& other) const noexcept {
                    for (int32 i = 0; i < 16; ++i) if (mMatrix.data[i] != other.mMatrix.data[i]) return false;
                    return true;
                }
                bool operator!=(const NkTransform& other) const noexcept { return !(*this == other); }

            private:
                math::NkMat4f mMatrix;  ///< Matrice 4x4 column-major (NkMath).
        };

    } // namespace renderer
} // namespace nkentseu
