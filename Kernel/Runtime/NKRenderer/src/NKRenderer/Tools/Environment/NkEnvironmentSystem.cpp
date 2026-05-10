// =============================================================================
// NkEnvironmentSystem.cpp  — NKRenderer v5.0
//
// D.2d : IBL prefiltering CPU au startup.
//   - BRDF LUT 256x256 RG8     : split-sum integration (Karis 2013) via Hammersley
//   - Irradiance cubemap 32x32 : convolution cos-weighted hemisphere (Lambert)
//   - Prefilter cubemap 128x128 (5 mips) : importance sampling GGX par roughness
//
// "Source" environment : un sky procedural (skyTop / horizon / ground gradient).
// On l'echantillonne directement en CPU plutot que de creer une vraie cubemap
// d'input — ca evite la dependance HDR/EXR loader pour la phase D.2d minimale.
//
// Cout : ~3-8s au startup selon CPU (single-threaded). Acceptable comme one-shot,
// optimisable plus tard via std::thread + tiled work distribution.
// =============================================================================
#include "NkEnvironmentSystem.h"
#include "NKThreading/NkThreadPool.h"
#include <cstdint>
#include <cstring>
#include <cmath>
#include <vector>

namespace nkentseu {
    namespace renderer {

        // ── Helpers cubemap directions (convention OpenGL / Vulkan-equivalente) ──
        // Reconstruit la direction 3D normalisee depuis (face, u, v ∈ [-1,1]).
        static inline void CubemapFaceUVToDir(uint32 face, float u, float v, float& dx, float& dy, float& dz) {
            switch (face) {
                case 0: dx =  1.f; dy = -v;  dz = -u; break;  // +X
                case 1: dx = -1.f; dy = -v;  dz =  u; break;  // -X
                case 2: dx =  u;   dy =  1.f; dz =  v; break;  // +Y (sky)
                case 3: dx =  u;   dy = -1.f; dz = -v; break;  // -Y (ground)
                case 4: dx =  u;   dy = -v;  dz =  1.f; break;  // +Z
                case 5: dx = -u;   dy = -v;  dz = -1.f; break;  // -Z
                default: dx = 0.f; dy = 1.f; dz = 0.f; break;
            }
            float l = std::sqrt(dx*dx + dy*dy + dz*dz);
            if (l > 1e-6f) { dx /= l; dy /= l; dz /= l; }
        }

        // ── Sky gradient procedural (notre "environment input") ─────────────────
        // Gradient ciel haut -> horizon -> sol selon dir.y.
        static inline NkVec3f SampleSkyGradient(float dx, float dy, float /*dz*/,
                                                 const NkVec3f& skyTop,
                                                 const NkVec3f& horizon,
                                                 const NkVec3f& ground) {
            (void)dx;
            float t = dy;  // [-1, 1]
            NkVec3f c;
            if (t >= 0.f) {
                c.x = horizon.x*(1.f-t) + skyTop.x*t;
                c.y = horizon.y*(1.f-t) + skyTop.y*t;
                c.z = horizon.z*(1.f-t) + skyTop.z*t;
            } else {
                float k = -t;
                c.x = horizon.x*(1.f-k) + ground.x*k;
                c.y = horizon.y*(1.f-k) + ground.y*k;
                c.z = horizon.z*(1.f-k) + ground.z*k;
            }
            return c;
        }

        // ── Hammersley sequence (low-discrepancy 2D) ────────────────────────────
        // Van der Corput radical inverse base 2.
        static inline float RadicalInverseVdC(uint32 bits) {
            bits = (bits << 16u) | (bits >> 16u);
            bits = ((bits & 0x55555555u) << 1u)  | ((bits & 0xAAAAAAAAu) >> 1u);
            bits = ((bits & 0x33333333u) << 2u)  | ((bits & 0xCCCCCCCCu) >> 2u);
            bits = ((bits & 0x0F0F0F0Fu) << 4u)  | ((bits & 0xF0F0F0F0u) >> 4u);
            bits = ((bits & 0x00FF00FFu) << 8u)  | ((bits & 0xFF00FF00u) >> 8u);
            return float(bits) * 2.3283064365386963e-10f;
        }
        static inline void Hammersley(uint32 i, uint32 N, float& x, float& y) {
            x = float(i) / float(N);
            y = RadicalInverseVdC(i);
        }

        // ── Construction d'un repere TBN orthonorme depuis N ─────────────────────
        static inline void BuildTBN(float Nx, float Ny, float Nz,
                                     float& Tx, float& Ty, float& Tz,
                                     float& Bx, float& By, float& Bz) {
            float ax = std::fabs(Ny) < 0.999f ? 0.f : 1.f;
            float ay = std::fabs(Ny) < 0.999f ? 1.f : 0.f;
            float az = 0.f;
            // T = normalize(cross(up, N))
            Tx = ay*Nz - az*Ny;
            Ty = az*Nx - ax*Nz;
            Tz = ax*Ny - ay*Nx;
            float l = std::sqrt(Tx*Tx + Ty*Ty + Tz*Tz);
            if (l > 1e-6f) { Tx/=l; Ty/=l; Tz/=l; }
            // B = cross(N, T)
            Bx = Ny*Tz - Nz*Ty;
            By = Nz*Tx - Nx*Tz;
            Bz = Nx*Ty - Ny*Tx;
        }

        // ── Importance sample GGX dans le repere de N ───────────────────────────
        // xi ∈ [0,1)^2, retourne la half-vector H en world space.
        static inline void ImportanceSampleGGX(float xiX, float xiY, float roughness,
                                                float Nx, float Ny, float Nz,
                                                float& Hx, float& Hy, float& Hz) {
            float a = roughness * roughness;
            float phi      = 6.28318530718f * xiX;
            float cosTheta = std::sqrt((1.f - xiY) / (1.f + (a*a - 1.f) * xiY));
            float sinTheta = std::sqrt(std::fmax(0.f, 1.f - cosTheta*cosTheta));

            float lx = sinTheta * std::cos(phi);
            float ly = sinTheta * std::sin(phi);
            float lz = cosTheta;

            // World-space via TBN
            float Tx, Ty, Tz, Bx, By, Bz;
            BuildTBN(Nx, Ny, Nz, Tx, Ty, Tz, Bx, By, Bz);
            Hx = Tx*lx + Bx*ly + Nx*lz;
            Hy = Ty*lx + By*ly + Ny*lz;
            Hz = Tz*lx + Bz*ly + Nz*lz;
            float l = std::sqrt(Hx*Hx + Hy*Hy + Hz*Hz);
            if (l > 1e-6f) { Hx/=l; Hy/=l; Hz/=l; }
        }

        // ── G_Smith pour BRDF LUT (Karis variation k=a/2) ───────────────────────
        static inline float G_Schlick(float cosT, float k) {
            return cosT / (cosT * (1.f - k) + k);
        }
        static inline float G_Smith_IBL(float NoV, float NoL, float roughness) {
            float a = roughness;
            float k = (a*a) / 2.f;
            return G_Schlick(NoV, k) * G_Schlick(NoL, k);
        }

        // ── Integrate split-sum BRDF (Karis 2013) ───────────────────────────────
        // Retourne (scale, bias) avec F0_scale = 1-(1-VoH)^5*(1-Fc) etc.
        static inline void IntegrateBRDF(float NoV, float roughness, uint32 numSamples,
                                          float& outScale, float& outBias) {
            // V dans le plan XZ avec V.z = NoV (frame ou N=Z).
            float Vx = std::sqrt(std::fmax(0.f, 1.f - NoV*NoV));
            float Vy = 0.f;
            float Vz = NoV;

            float A = 0.f, B = 0.f;
            for (uint32 i=0; i<numSamples; i++) {
                float xiX, xiY;
                Hammersley(i, numSamples, xiX, xiY);
                float Hx, Hy, Hz;
                ImportanceSampleGGX(xiX, xiY, roughness, 0.f, 0.f, 1.f, Hx, Hy, Hz);

                // L = reflect(-V, H) = 2*(V·H)*H - V
                float VoH = Vx*Hx + Vy*Hy + Vz*Hz;
                float Lx = 2.f*VoH*Hx - Vx;
                float Ly = 2.f*VoH*Hy - Vy;
                float Lz = 2.f*VoH*Hz - Vz;

                float NoL = std::fmax(Lz, 0.f);
                float NoH = std::fmax(Hz, 0.f);
                VoH       = std::fmax(VoH, 0.f);

                if (NoL > 0.f) {
                    float G     = G_Smith_IBL(NoV, NoL, roughness);
                    float G_Vis = (G * VoH) / std::fmax(NoH * NoV, 1e-6f);
                    float Fc    = std::pow(1.f - VoH, 5.f);
                    A += (1.f - Fc) * G_Vis;
                    B += Fc * G_Vis;
                }
            }
            outScale = A / float(numSamples);
            outBias  = B / float(numSamples);
        }

        NkEnvironmentSystem::~NkEnvironmentSystem() { Shutdown(); }

        bool NkEnvironmentSystem::Init(NkIDevice* device, const NkEnvironmentConfig& cfg) {
            mDevice = device;
            mCfg    = cfg;

            // ── Cree les textures GPU avec leurs tailles finales ────────────────
            const uint32 irrSize  = mCfg.irradianceSize > 0 ? mCfg.irradianceSize : 32;
            const uint32 prefSize = mCfg.prefilterSize  > 0 ? mCfg.prefilterSize  : 128;
            const uint32 prefMips = mCfg.prefilterMips  > 0 ? mCfg.prefilterMips  : 5;
            const uint32 lutSize  = mCfg.brdfLUTSize    > 0 ? mCfg.brdfLUTSize    : 256;

            {
                auto td = NkTextureDesc::Cubemap(irrSize, NkGPUFormat::NK_RGBA8_UNORM, 1);
                td.debugName = "EnvIrradiance";
                mIrradiance = mDevice->CreateTexture(td);
            }
            {
                auto td = NkTextureDesc::Cubemap(prefSize, NkGPUFormat::NK_RGBA8_UNORM, prefMips);
                td.debugName = "EnvPrefilter";
                mPrefilter = mDevice->CreateTexture(td);
            }
            {
                auto td = NkTextureDesc::Tex2D(lutSize, lutSize, NkGPUFormat::NK_RG8_UNORM, 1);
                td.debugName = "BRDFLUT";
                mBrdfLUT = mDevice->CreateTexture(td);
            }

            mEnvSampler = mDevice->CreateSampler(NkSamplerDesc::Clamp());
            mLutSampler = mDevice->CreateSampler(NkSamplerDesc::Clamp());

            // ── BRDF LUT : indep du sky, integration une fois ici. ─────────────
            // 64 samples Hammersley = bon compromis qualite/vitesse pour 256x256.
            // Parallelise par ligne (grainSize=8) via NkThreadPool.
            if (mBrdfLUT.IsValid()) {
                std::vector<uint8_t> lut(lutSize * lutSize * 2);
                const uint32 N = 64;

                auto& pool = ::nkentseu::threading::NkThreadPool::GetGlobal();
                pool.ParallelFor(lutSize, [&](nk_size yi) {
                    uint32 y = (uint32)yi;
                    float roughness = (float(y) + 0.5f) / float(lutSize);
                    for (uint32 x = 0; x < lutSize; x++) {
                        float NoV = (float(x) + 0.5f) / float(lutSize);
                        float A = 0.f, B = 0.f;
                        IntegrateBRDF(NoV, roughness, N, A, B);
                        uint32 idx = (y * lutSize + x) * 2;
                        lut[idx + 0] = uint8_t(NkClamp(A, 0.f, 1.f) * 255.f);
                        lut[idx + 1] = uint8_t(NkClamp(B, 0.f, 1.f) * 255.f);
                    }
                }, /*grainSize=*/8);
                pool.Join();

                mDevice->WriteTexture(mBrdfLUT, lut.data());
            }

            // ── Sky/horizon/ground initial (couleurs typiques jour ext). Le
            //    user peut surcharger via LoadProcedural plus tard. ─────────────
            LoadProcedural({0.45f, 0.65f, 0.95f},
                           {0.85f, 0.85f, 0.85f},
                           {0.18f, 0.14f, 0.10f});

            return mIrradiance.IsValid() && mPrefilter.IsValid() && mBrdfLUT.IsValid();
        }

        void NkEnvironmentSystem::LoadProcedural(const NkVec3f& skyTop,
                                                  const NkVec3f& horizon,
                                                  const NkVec3f& ground) {
            if (!mDevice) return;

            const uint32 irrSize  = mCfg.irradianceSize > 0 ? mCfg.irradianceSize : 32;
            const uint32 prefSize = mCfg.prefilterSize  > 0 ? mCfg.prefilterSize  : 128;
            const uint32 prefMips = mCfg.prefilterMips  > 0 ? mCfg.prefilterMips  : 5;

            // ── Irradiance convolution (cos-weighted hemisphere autour de N) ───
            // 8 strates × 32 azimuts = 256 samples par pixel — bon compromis
            // qualite/vitesse pour 32x32 (32*32*6*256 = 1.6M evals au total).
            // Parallelisation : 6 threads (1 par face), chaque thread ecrit son
            // propre buf+upload sequentiel a la fin (WriteTextureRegion non
            // thread-safe sur OpenGL backend).
            if (mIrradiance.IsValid()) {
                std::vector<std::vector<uint8_t>> faceBuf(6);
                auto irrFaceWork = [&](uint32 face) {
                    auto& buf = faceBuf[face];
                    buf.assign(irrSize * irrSize * 4, 0);
                    const float kPI = 3.14159265358979f;
                    const uint32 nTheta = 8;
                    const uint32 nPhi   = 32;
                    const float dTheta = 0.5f * kPI / float(nTheta);
                    const float dPhi   = 2.0f * kPI / float(nPhi);

                    for (uint32 y = 0; y < irrSize; y++) {
                        for (uint32 x = 0; x < irrSize; x++) {
                            float u = ((float)x + 0.5f) / (float)irrSize * 2.f - 1.f;
                            float v = ((float)y + 0.5f) / (float)irrSize * 2.f - 1.f;
                            float Nx, Ny, Nz;
                            CubemapFaceUVToDir(face, u, v, Nx, Ny, Nz);

                            float Tx, Ty, Tz, Bx, By, Bz;
                            BuildTBN(Nx, Ny, Nz, Tx, Ty, Tz, Bx, By, Bz);

                            float Cx = 0.f, Cy = 0.f, Cz = 0.f;
                            uint32 nSamp = 0;
                            for (uint32 ti = 0; ti < nTheta; ti++) {
                                float theta = (float(ti) + 0.5f) * dTheta;
                                float sT = std::sin(theta), cT = std::cos(theta);
                                for (uint32 pi = 0; pi < nPhi; pi++) {
                                    float phi = (float(pi) + 0.5f) * dPhi;
                                    float sP = std::sin(phi), cP = std::cos(phi);
                                    float lx = sT * cP, ly = sT * sP, lz = cT;
                                    float Wx = Tx*lx + Bx*ly + Nx*lz;
                                    float Wy = Ty*lx + By*ly + Ny*lz;
                                    float Wz = Tz*lx + Bz*ly + Nz*lz;
                                    NkVec3f s = SampleSkyGradient(Wx, Wy, Wz, skyTop, horizon, ground);
                                    float w = cT * sT;
                                    Cx += s.x * w; Cy += s.y * w; Cz += s.z * w;
                                    nSamp++;
                                }
                            }
                            float scale = kPI / float(nSamp);
                            Cx *= scale; Cy *= scale; Cz *= scale;

                            uint32 idx = (y * irrSize + x) * 4;
                            buf[idx + 0] = (uint8_t)(NkClamp(Cx, 0.f, 1.f) * 255.f);
                            buf[idx + 1] = (uint8_t)(NkClamp(Cy, 0.f, 1.f) * 255.f);
                            buf[idx + 2] = (uint8_t)(NkClamp(Cz, 0.f, 1.f) * 255.f);
                            buf[idx + 3] = 255;
                        }
                    }
                };

                auto& pool = ::nkentseu::threading::NkThreadPool::GetGlobal();
                pool.ParallelFor(6, [&](nk_size f) {
                    irrFaceWork((uint32)f);
                }, /*grainSize=*/1);
                pool.Join();
                // Upload sequentiel apres synchronisation (WriteTextureRegion non
                // thread-safe sur OpenGL backend, le ctx GL est lie au thread main).
                for (uint32 f = 0; f < 6; f++) {
                    mDevice->WriteTextureRegion(mIrradiance, faceBuf[f].data(),
                                                 0, 0, 0, irrSize, irrSize, 1,
                                                 0, f);
                }
            }

            // ── Prefilter GGX par mip (roughness = mip / (mipCount-1)) ─────────
            // Mip 0 (roughness=0) = mirror reflection (input direct).
            // Mip N-1 (roughness=1) = blur maximum.
            // 32 samples Hammersley + GGX importance par texel. Parallelisation
            // par face (6 threads par mip) — le mip 0 (128x128) domine le cout.
            if (mPrefilter.IsValid()) {
                const uint32 numSamples = 32;
                for (uint32 mip = 0; mip < prefMips; mip++) {
                    uint32 mipSize = prefSize >> mip;
                    if (mipSize < 1) mipSize = 1;
                    float roughness = (prefMips > 1) ? float(mip) / float(prefMips - 1) : 0.f;

                    std::vector<std::vector<uint8_t>> faceBuf(6);
                    auto prefFaceWork = [&](uint32 face) {
                        auto& buf = faceBuf[face];
                        buf.assign(mipSize * mipSize * 4, 0);
                        for (uint32 y = 0; y < mipSize; y++) {
                            for (uint32 x = 0; x < mipSize; x++) {
                                float u = ((float)x + 0.5f) / (float)mipSize * 2.f - 1.f;
                                float v = ((float)y + 0.5f) / (float)mipSize * 2.f - 1.f;
                                float Nx, Ny, Nz;
                                CubemapFaceUVToDir(face, u, v, Nx, Ny, Nz);
                                float Vx = Nx, Vy = Ny, Vz = Nz;

                                if (roughness < 1e-3f) {
                                    NkVec3f s = SampleSkyGradient(Nx, Ny, Nz, skyTop, horizon, ground);
                                    uint32 idx = (y * mipSize + x) * 4;
                                    buf[idx + 0] = (uint8_t)(NkClamp(s.x, 0.f, 1.f) * 255.f);
                                    buf[idx + 1] = (uint8_t)(NkClamp(s.y, 0.f, 1.f) * 255.f);
                                    buf[idx + 2] = (uint8_t)(NkClamp(s.z, 0.f, 1.f) * 255.f);
                                    buf[idx + 3] = 255;
                                    continue;
                                }

                                float Cx = 0.f, Cy = 0.f, Cz = 0.f;
                                float sumW = 0.f;
                                for (uint32 i = 0; i < numSamples; i++) {
                                    float xiX, xiY;
                                    Hammersley(i, numSamples, xiX, xiY);
                                    float Hx, Hy, Hz;
                                    ImportanceSampleGGX(xiX, xiY, roughness, Nx, Ny, Nz, Hx, Hy, Hz);
                                    float VoH = Vx*Hx + Vy*Hy + Vz*Hz;
                                    float Lx = 2.f*VoH*Hx - Vx;
                                    float Ly = 2.f*VoH*Hy - Vy;
                                    float Lz = 2.f*VoH*Hz - Vz;
                                    float NoL = std::fmax(Nx*Lx + Ny*Ly + Nz*Lz, 0.f);
                                    if (NoL > 0.f) {
                                        NkVec3f s = SampleSkyGradient(Lx, Ly, Lz, skyTop, horizon, ground);
                                        Cx += s.x * NoL;
                                        Cy += s.y * NoL;
                                        Cz += s.z * NoL;
                                        sumW += NoL;
                                    }
                                }
                                float inv = (sumW > 1e-6f) ? 1.f / sumW : 0.f;
                                Cx *= inv; Cy *= inv; Cz *= inv;

                                uint32 idx = (y * mipSize + x) * 4;
                                buf[idx + 0] = (uint8_t)(NkClamp(Cx, 0.f, 1.f) * 255.f);
                                buf[idx + 1] = (uint8_t)(NkClamp(Cy, 0.f, 1.f) * 255.f);
                                buf[idx + 2] = (uint8_t)(NkClamp(Cz, 0.f, 1.f) * 255.f);
                                buf[idx + 3] = 255;
                            }
                        }
                    };

                    auto& pool = ::nkentseu::threading::NkThreadPool::GetGlobal();
                    pool.ParallelFor(6, [&](nk_size f) {
                        prefFaceWork((uint32)f);
                    }, /*grainSize=*/1);
                    pool.Join();
                    for (uint32 f = 0; f < 6; f++) {
                        mDevice->WriteTextureRegion(mPrefilter, faceBuf[f].data(),
                                                     0, 0, 0, mipSize, mipSize, 1,
                                                     mip, f);
                    }
                }
            }
        }

        void NkEnvironmentSystem::Shutdown() {
            if (mLutSampler.IsValid()) { mDevice->DestroySampler(mLutSampler); mLutSampler={}; }
            if (mEnvSampler.IsValid()) { mDevice->DestroySampler(mEnvSampler); mEnvSampler={}; }
            if (mBrdfLUT.IsValid())    { mDevice->DestroyTexture(mBrdfLUT);    mBrdfLUT={}; }
            if (mPrefilter.IsValid())  { mDevice->DestroyTexture(mPrefilter);  mPrefilter={}; }
            if (mIrradiance.IsValid()) { mDevice->DestroyTexture(mIrradiance); mIrradiance={}; }
        }

    } // namespace renderer
} // namespace nkentseu
