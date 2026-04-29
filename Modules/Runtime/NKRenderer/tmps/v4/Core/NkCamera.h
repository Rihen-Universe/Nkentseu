#pragma once
// =============================================================================
// NkCamera.h  — Caméras 2D et 3D (pures CPU, pas de ressource GPU)
// =============================================================================
#include "NKRenderer/Core/NkRendererTypes.h"

namespace nkentseu {
    namespace renderer {

        // =============================================================================
        // NkCamera3D — caméra perspective ou orthographique 3D
        // =============================================================================
        enum class NkProjType : uint32 { NK_PERSPECTIVE, NK_ORTHOGRAPHIC };

        class NkCamera3D {
           public:
                // ── Configuration ─────────────────────────────────────────────────────────
                NkProjType type      = NkProjType::NK_PERSPECTIVE;
                NkVec3f    position  = {0, 0, 5};
                NkVec3f    target    = {0, 0, 0};
                NkVec3f    up        = {0, 1, 0};

                // Perspective
                float32    fovDeg    = 60.f;
                float32    aspect    = 16.f / 9.f;
                float32    nearZ     = 0.1f;
                float32    farZ      = 1000.f;

                // Orthographique
                float32    orthoLeft    = -5.f;
                float32    orthoRight   =  5.f;
                float32    orthoBottom  = -5.f;
                float32    orthoTop     =  5.f;
                float32    orthoNear    = -100.f;
                float32    orthoFar     =  100.f;

                // NDC convention : false=OpenGL[-1,1], true=Vulkan/DX/Metal[0,1]
                bool       depthZeroToOne = false;

                // ── Matrices ──────────────────────────────────────────────────────────────
                NkMat4f View()           const { return NkMat4f::LookAt(position, target, up); }
                NkMat4f Projection()     const;
                NkMat4f ViewProjection() const { return Projection() * View(); }

                // ── Helpers ───────────────────────────────────────────────────────────────
                NkVec3f Forward()  const;
                NkVec3f Right()    const;
                NkVec3f Up()       const { return up; }

                void SetAspect(uint32 w, uint32 h) {
                    aspect = (h > 0) ? (float32)w / (float32)h : 1.f;
                }

                // Orbite autour d'un point central
                void Orbit(const NkVec3f& center, float32 yawDeg,
                            float32 pitchDeg, float32 dist);

                // Pan dans le plan de la caméra
                void Pan(float32 dx, float32 dy);

                // Zoom (avance vers la cible)
                void Zoom(float32 delta);

                // Ray depuis NDC [-1,1]
                struct Ray { NkVec3f origin; NkVec3f direction; };
                Ray ScreenRay(float32 ndcX, float32 ndcY) const;

                // ── Factory ───────────────────────────────────────────────────────────────
                static NkCamera3D Perspective(float32 fov, float32 aspect,
                                               float32 nr, float32 fr,
                                               bool z01 = false) {
                    NkCamera3D c;
                    c.type = NkProjType::NK_PERSPECTIVE;
                    c.fovDeg = fov; c.aspect = aspect;
                    c.nearZ = nr; c.farZ = fr;
                    c.depthZeroToOne = z01;
                    return c;
                }
                static NkCamera3D Orthographic(float32 l, float32 r,
                                                float32 b, float32 t,
                                                float32 nr = -100.f,
                                                float32 fr =  100.f,
                                                bool z01 = false) {
                    NkCamera3D c;
                    c.type = NkProjType::NK_ORTHOGRAPHIC;
                    c.orthoLeft = l; c.orthoRight = r;
                    c.orthoBottom = b; c.orthoTop = t;
                    c.orthoNear = nr; c.orthoFar = fr;
                    c.depthZeroToOne = z01;
                    return c;
                }

                // Caméra lumière pour shadow map orthographique
                static NkCamera3D ShadowLight(const NkVec3f& dir,
                                               const NkVec3f& center,
                                               float32 radius = 10.f,
                                               bool z01 = false);
        };

        // =============================================================================
        // NkCamera2D — caméra orthographique 2D avec zoom/pan
        // =============================================================================
        class NkCamera2D {
           public:
                NkVec2f position = {0, 0};
                float32 rotation = 0.f;          // degrés
                float32 zoom     = 1.f;
                float32 width    = 1280.f;
                float32 height   = 720.f;
                bool    yFlip    = true;          // true = Y vers le bas (pixel)

                NkMat4f View()           const;
                NkMat4f Projection()     const;
                NkMat4f ViewProjection() const { return Projection() * View(); }

                // Conversions pixel ↔ monde
                NkVec2f ScreenToWorld(float32 sx, float32 sy) const;
                NkVec2f WorldToScreen(float32 wx, float32 wy) const;

                void Pan(float32 dx, float32 dy) {
                    position.x += dx;
                    position.y += dy;
                }
                void SetCenter(float32 x, float32 y) { position = {x, y}; }
                void SetSize  (uint32 w, uint32 h) {
                    width = (float32)w;
                    height = (float32)h;
                }
                void SetZoom  (float32 z) { zoom = NkMax(0.01f, z); }
                void ZoomToFit(const NkRectF& r) {
                    position = {r.x + r.w * .5f, r.y + r.h * .5f};
                    zoom = NkMin(width / r.w, height / r.h);
                }

                // Pixel-space : 1 world unit = 1 pixel, origine = haut-gauche
                static NkCamera2D PixelSpace(float32 w, float32 h) {
                    NkCamera2D c;
                    c.width = w; c.height = h; c.zoom = 1.f;
                    c.position = {w * .5f, h * .5f};
                    return c;
                }
        };

        // =============================================================================
        // UBOs standards (std140, alignés 16 bytes)
        // =============================================================================

        // Per-frame (Set 0, Binding 0)
        struct alignas(16) NkSceneUBO {
            float model[16];
            float view[16];
            float proj[16];
            float lightVP[16];
            float lightDir[4];
            float eyePos[4];
            float time;
            float dt;
            float ndcZScale;
            float ndcZOffset;
        };

        // Per-object push constant (max 80 bytes < 128 limite)
        struct alignas(16) NkObjectPC {
            float model[16];    // 64 bytes
            float tint[4];      // 16 bytes — total 80
        };

        // Bones pour skinning (Set 3, Binding 0) — max 64 bones
        struct alignas(16) NkBonesUBO {
            static constexpr uint32 kMaxBones = 64;
            float bones[kMaxBones][16];
        };

        // Material PBR (Set 2, Binding 0)
        struct alignas(16) NkPBRMaterialUBO {
            float baseColor[4];
            float emissiveColor[4];
            float metallic;
            float roughness;
            float occlusion;
            float normalScale;
            float alphaClip;
            float emissiveScale;
            float clearcoat;
            float clearcoatRoughness;
            float transmission;
            float ior;
            float _pad[2];
        };

    } // namespace renderer
} // namespace nkentseu
