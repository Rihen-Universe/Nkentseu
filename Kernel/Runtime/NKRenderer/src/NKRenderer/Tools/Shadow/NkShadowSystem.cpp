// =============================================================================
// NkShadowSystem.cpp  — NKRenderer v5.0
//
// Etat actuel : D.3b minimal — 1 cascade directional shadow map.
//   - Atlas : NK_D32_FLOAT 1024x1024.
//   - RenderShadowPasses calcule une lightVP orthographique fittee a une
//     bounding sphere fixe de la scene (rayon 8 unites autour de l'origine,
//     adapte a Demo3D). 4 cascades + frustum-fitting reel arriveront plus tard.
//   - Le rendu shadow est delegue a NkRender3D::RenderShadowPass(cmd, lightVP)
//     qui itere ses opaques castShadow=true avec son shadow pipeline.
// =============================================================================
#include "NkShadowSystem.h"
#include "NKRenderer/Mesh/NkMeshSystem.h"
#include "NKRenderer/Materials/NkMaterialSystem.h"
#include "NKRenderer/Tools/Render3D/NkRender3D.h"
#include "NKLogger/NkLog.h"
#include <cmath>
#include <cstring>

namespace nkentseu {
    namespace renderer {

        // Layout std140 du ShadowUBO (binding=3 dans le shader PBR).
        // Doit matcher pbr.frag.gl.glsl exactement (alignement std140).
        struct ShadowUBOBlock {
            NkMat4f cascadeMats[4];
            // std140 : un float dans un array prend un vec4 -> on laisse 4 float
            // mais l'array est indexé .x pour matcher le shader (glsl autorise
            // float cascadeSplits[4] qui prend 4 vec4 en std140).
            float32 cascadeSplits[4 * 4];     // 4 entrees, chaque dans un vec4
            // Bornes UV du tile de chaque cascade dans l'atlas. xy=min, zw=max.
            // Permet au PBR FS de clamper / rejeter les samples qui sortent du
            // tile (fragments hors-frustum de la cascade selectionnee qui
            // sampleraient sinon les depth values d'un autre cascade).
            float32 cascadeTileBounds[4 * 4]; // 4 vec4 (xy=min, zw=max)
            int32   cascadeCount;
            float32 shadowBias;
            float32 normalBias;
            int32   softShadows;
            float32 softness;     // PCSS lightSizeUV
            float32 _pad0, _pad1, _pad2;  // align std140 vec4
        };

        bool NkShadowSystem::Init(NkIDevice* d, NkMeshSystem* m,
                                NkMaterialSystem* mat, const NkShadowSystemConfig& cfg) {
            mDevice = d; mMesh = m; mMat = mat; mCfg = cfg;

            // ── Atlas depth : grille de tuiles selon numCascades ─────────────────
            // 1 cascade -> 1x1 (legacy)
            // 2 cascades -> 2x1
            // 3-4 cascades -> 2x2 (la 4eme tuile reste libre si N=3)
            uint32 N = mCfg.numCascades;
            if (N < 1) N = 1; if (N > 4) N = 4;
            mCellsX = (N == 1) ? 1 : 2;
            mCellsY = (N <= 2) ? 1 : 2;
            mTileSize = mCfg.resolution > 0 ? mCfg.resolution : 1024;
            mAtlasW   = mTileSize * mCellsX;
            mAtlasH   = mTileSize * mCellsY;

            auto td = NkTextureDesc::DepthStencil(mAtlasW, mAtlasH, NkGPUFormat::NK_D32_FLOAT);
            td.debugName = "ShadowAtlas";
            mAtlasRhi = mDevice->CreateTexture(td);

            // ── FBO custom : depth-only (le shader shadow ecrit gl_FragDepth implicite) ──
            NkFramebufferDesc fbd;
            fbd.width  = mAtlasW;
            fbd.height = mAtlasH;
            fbd.depthAttachment = mAtlasRhi;
            fbd.debugName = "ShadowFB";
            mShadowFB = mDevice->CreateFramebuffer(fbd);
            // En VK, CreateFramebuffer auto-cree un RP depth-only matchant fbd.
            // On le recupere pour que le pipeline Shadow soit compatible (sinon
            // VUID-vkCmdDrawIndexed-renderPass-02684 : RP du draw incompatible
            // avec le RP du pipeline cree avec un swapchain RP color+depth).
            // En GL, retourne un handle null sans effet.
            if (mShadowFB.IsValid()) {
                mShadowRP = mDevice->GetFramebufferRenderPass(mShadowFB);
            }

            // ── Sampler avec compare-mode (sampler2DShadow dans le shader) ──
            mShadowSampler    = mDevice->CreateSampler(NkSamplerDesc::Shadow());
            // Sampler non-compare : meme texture mais lue comme profondeur
            // brute (utilisee par PCSS blocker search dans le shader PBR).
            // NkSamplerDesc::Clamp() = linear filter + clamp-to-edge, sans compare.
            mShadowRawSampler = mDevice->CreateSampler(NkSamplerDesc::Clamp());

            // ── ShadowUBO ──
            mUBOShadow = mDevice->CreateBuffer(NkBufferDesc::Uniform(sizeof(ShadowUBOBlock)));
            mActiveCascades = 0;  // stub : pas de cascades actives -> pas d'ombre
            for (int i = 0; i < 4; i++) {
                mCascadeMats[i]    = NkMat4f::Identity();
                mCascadeSplits[i]  = 0.f;
            }
            UploadShadowUBO();

            return mAtlasRhi.IsValid() && mUBOShadow.IsValid();
        }

        void NkShadowSystem::Shutdown() {
            if (mShadowFB.IsValid())      { mDevice->DestroyFramebuffer(mShadowFB); mShadowFB={}; }
            if (mUBOShadow.IsValid())     { mDevice->DestroyBuffer(mUBOShadow);     mUBOShadow={}; }
            if (mShadowSampler.IsValid())    { mDevice->DestroySampler(mShadowSampler);    mShadowSampler={}; }
            if (mShadowRawSampler.IsValid()) { mDevice->DestroySampler(mShadowRawSampler); mShadowRawSampler={}; }
            if (mAtlasRhi.IsValid())      { mDevice->DestroyTexture(mAtlasRhi);     mAtlasRhi={}; }
        }

        void NkShadowSystem::UploadShadowUBO() {
            ShadowUBOBlock b{};
            for (int c = 0; c < 4; c++) {
                b.cascadeMats[c] = mCascadeMats[c];
                b.cascadeSplits[c * 4] = mCascadeSplits[c];  // .x dans le vec4
                // Tile bounds : derive de (cellsX, cellsY, c). En mode 1-cascade
                // (mCellsX=1) tout l'atlas est cascade 0 -> bounds [0,0,1,1].
                uint32 tileX = (uint32)c % mCellsX;
                uint32 tileY = (uint32)c / mCellsX;
                float invCx = 1.f / float(mCellsX);
                float invCy = 1.f / float(mCellsY);
                b.cascadeTileBounds[c*4 + 0] = float(tileX)     * invCx; // minU
                b.cascadeTileBounds[c*4 + 1] = float(tileY)     * invCy; // minV
                b.cascadeTileBounds[c*4 + 2] = float(tileX + 1) * invCx; // maxU
                b.cascadeTileBounds[c*4 + 3] = float(tileY + 1) * invCy; // maxV
            }
            b.cascadeCount = (int32)mActiveCascades;
            // Bias en depth-NDC space (apres mapping [0,1]). Modifiable a runtime
            // via GetConfig().shadowBias. Default 0.005 — adapter selon le
            // ratio (atlas resolution) / (sceneRadius) pour eviter shadow acne /
            // peter-panning.
            b.shadowBias   = mCfg.shadowBias;
            b.normalBias   = mCfg.normalBias;
            // softShadows : encode le mode PCF pour le shader PBR.
            //   0 = NONE     -> PCF 3x3 dur (fallback)
            //   1 = PCF/Poisson uniforme (PCF3x3/PCF5x5/POISSON modes regroupes)
            //   2 = PCSS     -> contact-hardening (blocker search via tShadowMapRaw,
            //                   radius adaptatif selon penumbra calculee)
            int32 softMode = 0;
            if (mCfg.pcfMode == NkPCFMode::PCSS)         softMode = 2;
            else if (mCfg.pcfMode != NkPCFMode::NONE)    softMode = 1;
            b.softShadows  = softMode;
            b.softness     = mCfg.softness;
            mDevice->WriteBuffer(mUBOShadow, &b, sizeof(b));
        }

        void NkShadowSystem::BeginShadowPass(NkICommandBuffer* cmd,
                                            NkVec3f lightDir, const NkCamera3D& cam) {
            (void)cmd; (void)lightDir; (void)cam;
            // D.3b : compute cascade splits + matrices ici.
            mInPass = true;
        }

        void NkShadowSystem::EndShadowPass(NkICommandBuffer* cmd) {
            (void)cmd;
            mInPass = false;
        }

        // ── Helper : compute frustum corners en world space pour une sub-frustum
        // [zNear, zFar] de la camera. On interpole depuis les corners du frustum
        // complet pour eviter de recreer une projection sub-frustum. ──────────────
        static void ComputeSubFrustumCornersWS(const NkCamera3D& cam,
                                                float zNear, float zFar,
                                                NkVec3f outCorners[8]) {
            NkMat4f invVP = cam.GetViewProj().Inverse();
            // 4 rays : NDC corners (-1,-1) (1,-1) (1,1) (-1,1) projetes near + far.
            // OpenGL : NDC z near = -1, z far = +1.
            const float kz0 = -1.f, kz1 = 1.f;
            NkVec3f rayNear[4], rayFar[4];
            for (int i = 0; i < 4; i++) {
                float nx = (i == 1 || i == 2) ? 1.f : -1.f;
                float ny = (i >= 2) ? 1.f : -1.f;
                NkVec4f cn = invVP * NkVec4f{nx, ny, kz0, 1.f};
                NkVec4f cf = invVP * NkVec4f{nx, ny, kz1, 1.f};
                rayNear[i] = NkVec3f{cn.x/cn.w, cn.y/cn.w, cn.z/cn.w};
                rayFar[i]  = NkVec3f{cf.x/cf.w, cf.y/cf.w, cf.z/cf.w};
            }
            // Interpole linearly entre near et far selon le pourcentage de
            // [camNear..camFar] occupe par [zNear..zFar].
            float camNear = cam.GetNear(), camFar = cam.GetFar();
            float aNear = (zNear - camNear) / (camFar - camNear);
            float aFar  = (zFar  - camNear) / (camFar - camNear);
            for (int i = 0; i < 4; i++) {
                NkVec3f dir = rayFar[i] - rayNear[i];
                outCorners[i]     = rayNear[i] + dir * aNear;
                outCorners[i + 4] = rayNear[i] + dir * aFar;
            }
        }

        void NkShadowSystem::RenderShadowPasses(NkICommandBuffer* cmd) {
            if (!cmd || !mRender3D) return;

            // 1. Trouver la premiere lumiere directional avec castShadow.
            const NkSceneContext& ctx = mRender3D->GetSceneContext();
            const NkLightDesc* sun = nullptr;
            for (auto& l : ctx.lights) {
                if (l.type == NkLightType::NK_DIRECTIONAL && l.castShadow) {
                    sun = &l; break;
                }
            }
            if (!sun) {
                if (mActiveCascades != 0) {
                    mActiveCascades = 0;
                    UploadShadowUBO();
                }
                return;
            }

            // 2. Splits cascade : log + uniform (Practical CSM, lambda blend).
            //    splits[N-1] = farPlane (couverture totale).
            uint32 N = mCfg.numCascades;
            if (N < 1) N = 1; if (N > 4) N = 4;
            float camNear = ctx.camera.GetNear();
            float camFar  = ctx.camera.GetFar();
            // Bornes utilisateur : on utilise nearPlane/farPlane de la config si
            // elles sont raisonnables, sinon on tombe sur la cam.
            float zNear = (mCfg.nearPlane > 0.f) ? mCfg.nearPlane : camNear;
            float zFar  = (mCfg.farPlane  > 0.f) ? mCfg.farPlane  : camFar;
            if (zFar <= zNear) zFar = zNear + 1.f;

            float lambda = mCfg.lambda;
            float ratio  = zFar / zNear;
            float splits[4]={};
            for (uint32 c = 0; c < N; c++) {
                float si    = float(c + 1) / float(N);
                float logS  = zNear * std::pow(ratio, si);
                float uniS  = zNear + (zFar - zNear) * si;
                splits[c]   = lambda * logS + (1.f - lambda) * uniS;
            }

            // 3. Pour chaque cascade : sphere-fit du sub-frustum en world-space.
            //    Mode N=1 : fallback sur sceneRadius fixe centre origine (D.3b
            //    legacy) — evite un sphere fit sur TOUT le frustum camera qui
            //    donne un radius enorme et une precision depth catastrophique.
            NkVec3f lightDir = sun->direction.Normalized();
            NkVec3f up = (lightDir.y * lightDir.y > 0.9025f) ? NkVec3f{1,0,0} : NkVec3f{0,1,0};

            const bool singleCascadeFixed = (N == 1);
            const float32 sceneRadius = mCfg.sceneRadius > 0.f ? mCfg.sceneRadius : 10.f;

            for (uint32 c = 0; c < N; c++) {
                float subNear = (c == 0) ? zNear : splits[c - 1];
                float subFar  = splits[c];

                NkVec3f center;
                float   radius;
                if (singleCascadeFixed) {
                    // D.3b mode : sphere fixe centree origine, radius config
                    center = NkVec3f{0, 0, 0};
                    radius = sceneRadius;
                } else {
                    NkVec3f corners[8];
                    ComputeSubFrustumCornersWS(ctx.camera, subNear, subFar, corners);

                    // Sphere fit (centre = moyenne, radius = max distance corner-centre).
                    center = NkVec3f{0,0,0};
                    for (int i=0;i<8;i++) center = center + corners[i];
                    center = center * (1.f / 8.f);
                    radius = 0.f;
                    for (int i=0;i<8;i++) {
                        NkVec3f d = corners[i] - center;
                        float r = std::sqrt(d.x*d.x + d.y*d.y + d.z*d.z);
                        if (r > radius) radius = r;
                    }
                    // Marge XY (1.2x) : evite que des casters legerement hors sub-frustum
                    // (ex : sphere au bord d'une cascade) soient clippes du shadow render
                    // -> shadow qui clignote/disparait quand la camera bouge.
                    radius *= 1.2f;
                    if (radius < 1.f) radius = 1.f;
                }

                // Stable shadows (texel-snap) : on snap le centre du sphere fit
                // aux frontieres de texels du shadow map en light-space. Sans
                // ca, le centre bouge en sous-pixel a chaque frame quand la
                // camera tourne -> les texels de l'atlas changent infinitesimalement
                // -> flickering visible (shadow swimming + edge crawl).
                //
                // Etape 1 : construire le repere light-space (axes orthonorms
                //   alignes sur la direction de la lumiere).
                // Etape 2 : projeter le centre sur ces axes pour avoir les
                //   coords du centre en light-space.
                // Etape 3 : snap a la grille de texels (texelSize = 2*radius / tileSize).
                // Etape 4 : reconstruire le centre snappe en world-space.
                NkVec3f forward = lightDir;  // axe Z (pointing into light)
                NkVec3f right;
                NkVec3f upL;
                if (forward.y * forward.y > 0.9025f) {
                    right = NkVec3f{1, 0, 0};
                } else {
                    NkVec3f wUp = NkVec3f{0, 1, 0};
                    right = wUp.Cross(forward).Normalized();
                }
                upL = forward.Cross(right).Normalized();
                right = upL.Cross(forward).Normalized();

                float texelSize = (radius * 2.f) / float(mTileSize);
                float cxLS = right.Dot(center);
                float cyLS = upL.Dot(center);
                float czLS = forward.Dot(center);
                cxLS = std::floor(cxLS / texelSize) * texelSize;
                cyLS = std::floor(cyLS / texelSize) * texelSize;
                center = right * cxLS + upL * cyLS + forward * czLS;

                NkVec3f eye = center - lightDir * (radius * 4.f);
                NkMat4f lightView = NkMat4f::LookAt(eye, center, upL);
                NkMat4f lightProj = NkMat4f::Orthogonal(radius * 2.f, radius * 2.f,
                                                        0.1f, radius * 8.f);
                NkMat4f lightVP = lightProj * lightView;

                // Render matrix (vrai gl_Position pour shadow VS) : pas de tile bake.
                mCascadeRenderMats[c] = lightVP;

                // UBO matrix (lue par PBR FS pour echantillonner) : on bake une
                // post-projection T qui mappe NDC [-1,1] de la cascade vers la
                // sous-region NDC [tx*2-1+halfNdc..] correspondant a la tile
                // dans l'atlas. Apres le `*0.5+0.5` du PCF, on obtient l'UV de
                // la tile dans l'atlas.
                uint32 tileX = c % mCellsX;
                uint32 tileY = c / mCellsX;
                float halfX = 1.f / float(mCellsX);  // moitie de la largeur tile en UV
                float halfY = 1.f / float(mCellsY);
                float ndcCx = (float(tileX) + 0.5f) * 2.f / float(mCellsX) - 1.f;
                float ndcCy = (float(tileY) + 0.5f) * 2.f / float(mCellsY) - 1.f;
                NkMat4f T = NkMat4f::Identity();
                T.col[0] = NkVec4f{halfX, 0,    0, 0};
                T.col[1] = NkVec4f{0,    halfY, 0, 0};
                T.col[2] = NkVec4f{0,    0,    1, 0};
                T.col[3] = NkVec4f{ndcCx, ndcCy, 0, 1};
                mCascadeMats[c]   = T * lightVP;
                mCascadeSplits[c] = subFar;
            }
            mActiveCascades = N;
            UploadShadowUBO();

            // 4. Begin RP unique sur l'atlas full + clear depth. Pour chaque
            //    cascade on bouge le viewport sur sa tile et on render avec
            //    le render matrix correspondant.
            cmd->SetClearDepth(1.f, 0);
            NkRect2D area((int32)0, (int32)0, (int32)mAtlasW, (int32)mAtlasH);
            cmd->BeginRenderPass(NkRenderPassHandle{}, mShadowFB, area);

            for (uint32 c = 0; c < N; c++) {
                uint32 tileX = c % mCellsX;
                uint32 tileY = c / mCellsX;
                NkViewport vp{
                    float(tileX * mTileSize), float(tileY * mTileSize),
                    float(mTileSize),         float(mTileSize),
                    0.f, 1.f, /*flipY=*/false
                };
                NkRect2D scissor{
                    (int32)(tileX * mTileSize), (int32)(tileY * mTileSize),
                    (int32)mTileSize,           (int32)mTileSize
                };
                cmd->SetViewport(vp);
                cmd->SetScissor(scissor);
                mRender3D->RenderShadowPass(cmd, mCascadeRenderMats[c]);
            }

            cmd->EndRenderPass();

            // Transition explicite atlas DEPTH_WRITE -> SHADER_READ pour la pass
            // Geometry qui suit (binding 11 "tShadowMap" du PBR shader). Le
            // RenderGraph ne sait pas que cette texture (allouee hors-graph par
            // NkShadowSystem) doit etre transitee en lecture pour la pass
            // suivante. Sans ca : validation Vulkan signale layout
            // DEPTH_STENCIL_ATTACHMENT_OPTIMAL alors que le descriptor set
            // attend SHADER_READ_ONLY_OPTIMAL. Pattern equivalent dans Base03
            // (NkRHIDemoFullImage.cpp ~1556-1564).
            if (mAtlasRhi.IsValid()) {
                cmd->TextureBarrier(mAtlasRhi,
                                    NkResourceState::NK_DEPTH_WRITE,
                                    NkResourceState::NK_SHADER_READ,
                                    NkPipelineStage::NK_LATE_FRAGMENT,
                                    NkPipelineStage::NK_FRAGMENT_SHADER);
            }
        }

    } // namespace renderer
} // namespace nkentseu
