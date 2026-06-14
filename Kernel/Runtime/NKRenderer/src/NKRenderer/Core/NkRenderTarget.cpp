// =============================================================================
// NkRenderTarget.cpp — NKRenderer v4.0
// =============================================================================
#include "NkRenderTarget.h"
#include "NKRenderer/Tools/Render3D/NkRender3D.h"
#include <cmath>

namespace nkentseu {
    namespace renderer {

        bool NkRenderTarget::Init(NkIDevice* dev, NkTextureLibrary* texLib,
                                  const NkRenderTargetDesc& desc) {
            NkOffscreenDesc od;
            od.width    = desc.width;
            od.height   = desc.height;
            od.hdr      = desc.hdr;
            od.hasDepth = desc.depth;
            od.readable = true;
            od.readback = false;
            od.name     = desc.name;
            // Phase H.3 : si colorFmt explicite, override le default
            // RGBA8_SRGB / RGBA16F (decide par hdr bool).
            if (desc.colorFmt != NkGPUFormat::NK_UNDEFINED) {
                od.colorFmt = desc.colorFmt;
            }
            return mOffscreen.Init(dev, texLib, od);
        }

        void NkRenderTarget::Shutdown() {
            mOffscreen.Shutdown();
        }

        NkRenderPassHandle NkRenderTarget::BeginRender(NkICommandBuffer* cmd,
                                                        NkVec4f clearColor,
                                                        bool    clearDepth) {
            mOffscreen.BeginCapture(cmd, true, clearColor, clearDepth);
            return mOffscreen.GetRP();
        }

        void NkRenderTarget::EndRender(NkICommandBuffer* cmd) {
            mOffscreen.EndCapture(cmd);
        }

        void NkRenderTarget::FlushScene(NkICommandBuffer* cmd, NkRender3D* r3d) {
            if (!r3d || !mOffscreen.IsValid()) return;
            r3d->Flush(cmd, mOffscreen.GetRP());
        }

        bool NkRenderTarget::Resize(uint32 w, uint32 h) {
            return mOffscreen.Resize(w, h);
        }

        // ── Réflexion planaire ────────────────────────────────────────────────

        NkCamera3D NkRenderTarget::ReflectCamera(const NkCamera3D& cam, NkVec4f plane) {
            // plane = {nx, ny, nz, d} normalisé : nx·x + ny·y + nz·z + d = 0.
            // Réflexion d'un point P  : P'  = P - 2*(n·P + d)*n
            // Réflexion d'une direction V : V' = V - 2*(n·V)*n  (w=0, pas de terme d)
            float nx = plane.x, ny = plane.y, nz = plane.z, d = plane.w;

            auto reflPt = [&](NkVec3f p) -> NkVec3f {
                float t = 2.f * (nx*p.x + ny*p.y + nz*p.z + d);
                return { p.x - t*nx, p.y - t*ny, p.z - t*nz };
            };
            auto reflDir = [&](NkVec3f v) -> NkVec3f {
                float t = 2.f * (nx*v.x + ny*v.y + nz*v.z);
                return { v.x - t*nx, v.y - t*ny, v.z - t*nz };
            };

            NkCamera3DData data = cam.GetData();
            data.position = reflPt(data.position);
            data.target   = reflPt(data.target);
            data.up       = reflDir(data.up);

            NkCamera3D mirror;
            mirror.SetData(data);
            return mirror;
        }

        NkMat4f NkRenderTarget::ObliqueProjection(NkMat4f proj, const NkMat4f& view,
                                                    NkVec4f worldPlane) {
            // Algorithme : Eric Lengyel — "Oblique View Frustum Depth Projection and Clipping"
            // GPU Gems, 2005.
            //
            // Convention NkMat4f : COLUMN-MAJOR — mat[col][row].
            // mat[0..2][0..2] = rotation, mat[3][0..2] = translation.
            //
            // 1. Transformer le plan monde → espace œil (eye space).
            //    c_eye = (inv(view))^T * c_world
            //    Pour une view orthonormale [R|-R·pos] :
            //      c_eye.xyz = R (appliqué à n_world)  = {view[col][row] comme produit matrice-vecteur}
            //      c_eye.w   = camPos · n_world + d     (distance cam au plan monde)
            //    Récupération de camPos depuis view (sans paramètre supplémentaire) :
            //      camPos.k = - dot(view[k][0..2], view[3][0..2])
            //               = - (view[k][0]*view[3][0] + view[k][1]*view[3][1] + view[k][2]*view[3][2])

            float nx = worldPlane.x, ny = worldPlane.y, nz = worldPlane.z, nd = worldPlane.w;

            // Récupère camPos à partir des colonnes de view (pas besoin de passage en paramètre)
            float cpx = -(view[0][0]*view[3][0] + view[0][1]*view[3][1] + view[0][2]*view[3][2]);
            float cpy = -(view[1][0]*view[3][0] + view[1][1]*view[3][1] + view[1][2]*view[3][2]);
            float cpz = -(view[2][0]*view[3][0] + view[2][1]*view[3][1] + view[2][2]*view[3][2]);

            // Eye-space clip plane {cx, cy, cz, cw}
            // Normal : produit matrice-vecteur de la rotation de view avec n_world
            //   c.x = row0_of_view · n  = view[0][0]*nx + view[1][0]*ny + view[2][0]*nz
            float cx = view[0][0]*nx + view[1][0]*ny + view[2][0]*nz;
            float cy = view[0][1]*nx + view[1][1]*ny + view[2][1]*nz;
            float cz = view[0][2]*nx + view[1][2]*ny + view[2][2]*nz;
            float cw = cpx*nx + cpy*ny + cpz*nz + nd;  // distance cam au plan

            // 2. Point q = coin du frustum NDC dans la direction de c
            //    proj[0][0] = f/aspect, proj[1][1] = f (scale factors)
            //    proj[2][2] = -(far+near)/(far-near)
            //    proj[3][2] = -2*far*near/(far-near)  (col3, row2)
            float sign_cx = (cx >= 0.f) ? 1.f : -1.f;
            float sign_cy = (cy >= 0.f) ? 1.f : -1.f;
            float qx = sign_cx / proj[0][0];
            float qy = sign_cy / proj[1][1];
            float qz = -1.f;
            float qw = (1.f + proj[2][2]) / proj[3][2];  // ≈ 1/far (far plane corner)

            // 3. Calcule scale = 2 / (c · q)
            float c_dot_q = cx*qx + cy*qy + cz*qz + cw*qw;
            if (fabsf(c_dot_q) < 1e-7f) return proj;  // plan parallèle au near — pas d'oblique

            float scale = 2.f / c_dot_q;

            // 4. Remplace la 3ème ligne (row=2) de proj par : scale*c + row3_of_proj
            //    row3_of_proj (en col-major) = {proj[0][3], proj[1][3], proj[2][3], proj[3][3]}
            //    Standard GL perspective : {0, 0, -1, 0}
            proj[0][2] = scale * cx + proj[0][3];
            proj[1][2] = scale * cy + proj[1][3];
            proj[2][2] = scale * cz + proj[2][3];
            proj[3][2] = scale * cw + proj[3][3];

            return proj;
        }

    } // namespace renderer
} // namespace nkentseu
