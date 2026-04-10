#pragma once
// =============================================================================
// NkCamera.h — Caméras 2D et 3D du NKRenderer.
//
//  NkFrustum       6 plans de clipping — frustum culling AABB/sphère
//  NkCamera        Interface abstraite commune à toutes les caméras
//  NkCamera2D      Caméra orthographique 2D (UI, sprites)
//  NkCamera3D      Caméra 3D perspective/ortho — frustum, yaw/pitch, projection space
//  NkOrbitalCam    Caméra arcball autour d'un pivot (éditeur, outils)
// =============================================================================
#include "NkRenderTypes.h"

namespace nkentseu {
    namespace renderer {

        // =====================================================================
        // NkFrustum
        // 6 plans de clipping extraits de la matrice View-Projection.
        // Algorithme de Gribb-Hartmann (extraction directe depuis VP).
        // Utilisé pour le frustum culling des meshes, lumières, particules.
        // =====================================================================
        struct NkFrustum {
            // Ordre : 0=gauche, 1=droite, 2=bas, 3=haut, 4=near, 5=far
            // Chaque plan est (nx, ny, nz, d) tel que n·p + d >= 0 = dedans.
            NkVec4f planes[6] = {};

            // Extrait les 6 plans depuis une matrice VP
            void ExtractFromVP(const NkMat4f& vp) noexcept {
                const float* m = &vp[0][0];
                planes[0] = { m[3] + m[0],  m[7] + m[4],  m[11] + m[8],  m[15] + m[12] }; // gauche
                planes[1] = { m[3] - m[0],  m[7] - m[4],  m[11] - m[8],  m[15] - m[12] }; // droite
                planes[2] = { m[3] + m[1],  m[7] + m[5],  m[11] + m[9],  m[15] + m[13] }; // bas
                planes[3] = { m[3] - m[1],  m[7] - m[5],  m[11] - m[9],  m[15] - m[13] }; // haut
                planes[4] = { m[3] + m[2],  m[7] + m[6],  m[11] + m[10], m[15] + m[14] }; // near
                planes[5] = { m[3] - m[2],  m[7] - m[6],  m[11] - m[10], m[15] - m[14] }; // far
                for (int i = 0; i < 6; ++i) {
                    float len = math::NkSqrt(planes[i].x * planes[i].x
                                        + planes[i].y * planes[i].y
                                        + planes[i].z * planes[i].z);
                    if (len > 1e-6f) {
                        planes[i].x /= len;
                        planes[i].y /= len;
                        planes[i].z /= len;
                        planes[i].w /= len;
                    }
                }
            }

            // Renvoie true si la sphère est (partiellement) visible
            bool ContainsSphere(const NkVec3f& center, float radius) const noexcept {
                for (int i = 0; i < 6; ++i) {
                    if (planes[i].x * center.x + planes[i].y * center.y
                    + planes[i].z * center.z + planes[i].w < -radius)
                        return false;
                }
                return true;
            }

            // Renvoie true si l'AABB est (partiellement) visible
            bool ContainsAABB(const NkVec3f& minPt, const NkVec3f& maxPt) const noexcept {
                for (int i = 0; i < 6; ++i) {
                    float px = (planes[i].x >= 0.f) ? maxPt.x : minPt.x;
                    float py = (planes[i].y >= 0.f) ? maxPt.y : minPt.y;
                    float pz = (planes[i].z >= 0.f) ? maxPt.z : minPt.z;
                    if (planes[i].x * px + planes[i].y * py + planes[i].z * pz + planes[i].w < 0.f)
                        return false;
                }
                return true;
            }
        };

        // =====================================================================
        // NkCamera — Interface abstraite commune à toutes les caméras
        // =====================================================================
        class NkCamera {
            public:
                virtual ~NkCamera() = default;

                // Matrices de transformation
                virtual NkMat4f GetViewMatrix() const = 0;
                virtual NkMat4f GetProjectionMatrix(float aspectRatio) const = 0;

                // Raccourci View-Projection
                NkMat4f GetViewProjectionMatrix(float aspectRatio) const {
                    return GetProjectionMatrix(aspectRatio) * GetViewMatrix();
                }

                // Frustum pour le culling (nécessite l'aspect ratio)
                NkFrustum GetFrustum(float aspectRatio) const {
                    NkFrustum f;
                    f.ExtractFromVP(GetViewProjectionMatrix(aspectRatio));
                    return f;
                }

                // Propriétés communes (peuvent être redéfinies)
                virtual NkVec3f GetPosition() const = 0;
                virtual NkVec3f GetForward() const = 0;
                virtual NkVec3f GetUp() const = 0;
                virtual NkVec3f GetRight() const = 0;
        };

        // =====================================================================
        // NkCamera2D — caméra orthographique 2D (UI, sprites, tilemaps)
        // Espace écran : [0, width] × [0, height], Y vers le bas.
        // Supporte zoom, rotation et pan avec lazy rebuild (dirty flag).
        // =====================================================================
        class NkCamera2D : public NkCamera {
            public:
                NkCamera2D() = default;
                NkCamera2D(float32 width, float32 height,
                        float32 nearZ = -1.f, float32 farZ = 1.f) noexcept;

                void SetOrtho(float32 width, float32 height,
                            float32 nearZ = -1.f, float32 farZ = 1.f) noexcept;

                void SetPosition(NkVec2 pos)      noexcept { mPosition = pos;      mDirty = true; }
                void SetZoom    (float32 zoom)    noexcept { mZoom     = zoom;     mDirty = true; }
                void SetRotation(float32 angleDeg)noexcept { mRotation = angleDeg; mDirty = true; }

                float32 GetZoom()     const noexcept { return mZoom;     }
                float32 GetRotation() const noexcept { return mRotation; }
                float32 GetWidth()    const noexcept { return mWidth;    }
                float32 GetHeight()   const noexcept { return mHeight;   }

                // Implémentation de NkCamera
                NkMat4f GetViewMatrix() const override;
                NkMat4f GetProjectionMatrix(float aspectRatio) const override;
                NkVec3f GetPosition() const override { return NkVec3f(mPosition.x, mPosition.y, 0.f); }
                NkVec3f GetForward() const override;
                NkVec3f GetUp() const override;
                NkVec3f GetRight() const override;

                // Conversion espace écran ↔ espace monde
                NkVec2 ScreenToWorld(NkVec2 screenPos) const noexcept;
                NkVec2 WorldToScreen(NkVec2 worldPos)  const noexcept;

            private:
                void RebuildIfDirty() const noexcept;

                float32        mWidth    = 800.f;
                float32        mHeight   = 600.f;
                float32        mNearZ    = -1.f;
                float32        mFarZ     = 1.f;
                NkVec2         mPosition = {};
                float32        mZoom     = 1.f;
                float32        mRotation = 0.f;
                mutable NkMat4f mProjection;
                mutable NkMat4f mView;
                mutable bool   mDirty   = true;
        };

        // =====================================================================
        // NkCamera3D — caméra 3D perspective ou orthographique
        //
        // Fonctionnalités :
        //   - Perspective ou orthographique (SetPerspective / SetOrthographic)
        //   - LookAt ou contrôle FPS (yaw + pitch)
        //   - Espace NDC configurable (Vulkan/DX = zero-one, OpenGL = neg-one)
        //   - Matrices cachées (dirty flag) → pas de recompute sans changement
        // =====================================================================
        class NkCamera3D : public NkCamera {
            public:
                NkCamera3D() = default;
                NkCamera3D(float32 fovDeg, float32 aspect,
                        float32 nearZ = 0.01f, float32 farZ = 1000.f) noexcept;

                // ── Projection ────────────────────────────────────────────────────
                void SetPerspective (float32 fovDeg, float32 aspect,
                                    float32 nearZ = 0.01f, float32 farZ = 1000.f) noexcept;
                void SetOrthographic(float32 left,  float32 right,
                                    float32 bottom, float32 top,
                                    float32 nearZ = -10.f, float32 farZ = 10.f) noexcept;

                // ── Transform ─────────────────────────────────────────────────────
                void SetPosition(NkVec3 pos)    noexcept { mPosition = pos;    mDirty = true; }
                void SetTarget  (NkVec3 target) noexcept { mTarget   = target; mDirty = true; }
                void SetUp      (NkVec3 up)     noexcept { mUp       = up;     mDirty = true; }

                void LookAt(NkVec3 eye, NkVec3 target, NkVec3 up) noexcept;

                // ── Contrôle FPS (yaw/pitch) ──────────────────────────────────────
                void SetYaw  (float32 deg) noexcept { mYaw   = deg; mDirty = true; }
                void SetPitch(float32 deg) noexcept { mPitch = deg; mDirty = true; }

                // ── Accesseurs ────────────────────────────────────────────────────
                NkVec3  GetPosition() const override { return mPosition; }
                NkVec3  GetTarget()   const noexcept { return mTarget;   }
                float32 GetFOV()      const noexcept { return mFovDeg;   }
                float32 GetAspect()   const noexcept { return mAspect;   }
                float32 GetNearZ()    const noexcept { return mNearZ;    }
                float32 GetFarZ()     const noexcept { return mFarZ;     }
                NkVec3  GetForward()  const override;
                NkVec3  GetRight()    const override;
                NkVec3  GetUp()       const override { return mUp; }

                // ── Implémentation de NkCamera ────────────────────────────────────
                NkMat4f GetViewMatrix() const override;
                NkMat4f GetProjectionMatrix(float aspectRatio) const override;

                // ── Convention NDC ────────────────────────────────────────────────
                void SetProjectionSpace(NkProjectionSpace ps) noexcept {
                    mProjSpace = ps; mDirty = true;
                }
                NkProjectionSpace GetProjectionSpace() const noexcept { return mProjSpace; }

            private:
                void RebuildIfDirty()     const noexcept;
                void RebuildPerspective() const noexcept;
                void RebuildView()        const noexcept;

                bool           mIsPerspective = true;
                float32        mFovDeg        = 60.f;
                float32        mAspect        = 16.f / 9.f;
                float32        mNearZ         = 0.01f;
                float32        mFarZ          = 1000.f;
                // Ortho
                float32        mOrthoLeft     = -1.f, mOrthoRight  = 1.f;
                float32        mOrthoBottom   = -1.f, mOrthoTop    = 1.f;

                NkVec3         mPosition      = {0, 0, 5};
                NkVec3         mTarget        = {0, 0, 0};
                NkVec3         mUp            = {0, 1, 0};
                float32        mYaw           = -90.f;
                float32        mPitch         = 0.f;

                NkProjectionSpace mProjSpace  = NkProjectionSpace::NK_NDC_ZERO_ONE;

                mutable NkMat4f mProjection;
                mutable NkMat4f mView;
                mutable bool   mDirty         = true;
        };

        // =====================================================================
        // NkOrbitalCam — caméra arcball autour d'un pivot
        // Idéale pour les éditeurs, visionneuses de modèles, outils.
        // Stocke la position en coordonnées sphériques et se convertit en NkCamera3D.
        // =====================================================================
        struct NkOrbitalCam {
            NkVec3f pivot       = { 0.f, 0.f, 0.f };
            float   distance    = 5.f;
            float   yawDeg      = 0.f;     // rotation horizontale autour du pivot
            float   pitchDeg    = 20.f;    // élévation verticale [-89, 89]
            float   minDist     = 0.5f;
            float   maxDist     = 500.f;
            float   minPitchDeg = -89.f;
            float   maxPitchDeg =  89.f;

            // ── Contrôles ─────────────────────────────────────────────────────
            void Orbit(float dYaw, float dPitch) noexcept {
                yawDeg   += dYaw;
                pitchDeg  = math::NkClamp(pitchDeg + dPitch, minPitchDeg, maxPitchDeg);
            }

            void Zoom(float delta) noexcept {
                distance = math::NkClamp(distance - delta, minDist, maxDist);
            }

            void PanPivot(const NkVec3f& delta) noexcept {
                pivot += delta;
            }

            // ── Conversion vers NkCamera3D ────────────────────────────────────
            NkCamera3D ToCamera3D(float32 fovDeg = 60.f, NkProjectionSpace ps = NkProjectionSpace::NK_NDC_ZERO_ONE) const noexcept {
                const float toRad = 3.14159265f / 180.f;
                float yawR   = yawDeg   * toRad;
                float pitchR = pitchDeg * toRad;
                float cp     = math::NkCos(pitchR);

                NkVec3f pos = {
                    pivot.x + distance * cp * math::NkSin(yawR),
                    pivot.y + distance * math::NkSin(pitchR),
                    pivot.z + distance * cp * math::NkCos(yawR)
                };

                NkCamera3D cam;
                cam.LookAt(pos, pivot, {0.f, 1.f, 0.f});
                cam.SetProjectionSpace(ps);
                // Définit une perspective par défaut (l'utilisateur pourra la modifier ultérieurement)
                cam.SetPerspective(fovDeg, 1.0f, 0.1f, 1000.f);
                return cam;
            }
        };
    } // namespace renderer
} // namespace nkentseu