// =============================================================================
// NkEnvironmentSystem.cpp  — NKRenderer v5.0
//
// D.2d : IBL prefiltering CPU au startup avec cache disque.
//   - BRDF LUT 256x256 RG8     : split-sum integration (Karis 2013) via Hammersley
//   - Irradiance cubemap 32x32 : convolution cos-weighted hemisphere (Lambert)
//   - Prefilter cubemap 128x128 (5 mips) : importance sampling GGX par roughness
//
// Cache disque (nk_ibl_cache.bin par defaut) : premiere execution ~0.5-2s, suivantes <50ms.
// Invalidation automatique si les parametres sky ou les tailles changent (hash FNV-32).
// =============================================================================
#include "NkEnvironmentSystem.h"
#include "NKThreading/NkThreadPool.h"
#include "NKLogger/NkLog.h"
#include "NKFileSystem/NkPath.h"
#include "NKFileSystem/NkDirectory.h"
#include <cstdint>
#include <cstring>
#include <cmath>
#include <cstdio>
#include <vector>

namespace nkentseu {
    namespace renderer {

        // ── Cache disque IBL ─────────────────────────────────────────────────────
        // Format : magic(4) + version(4) + hash(4) + irrSize(4) + prefSize(4)
        //        + prefMips(4) + lutSize(4)
        //        + LUT data (lutSize*lutSize*2)
        //        + Irr data (6 * irrSize*irrSize*4)
        //        + Pref data (sum_mip 6*(prefSize>>mip)^2*4)
        static constexpr uint32 kIBLMagic   = 0x4E4B4942u; // 'NKIB'
        static constexpr uint32 kIBLVersion = 2u;

        static uint32 IBLHash(const NkVec3f& sky, const NkVec3f& hor, const NkVec3f& gnd,
                               uint32 irrSz, uint32 prefSz, uint32 prefM, uint32 lutSz) {
            uint32 h = 0x811c9dc5u;
            auto mix = [&](uint32 v) { h = (h ^ v) * 0x01000193u; };
            uint32 b;
            auto mf = [&](float v) { memcpy(&b, &v, 4); mix(b); };
            mf(sky.x); mf(sky.y); mf(sky.z);
            mf(hor.x); mf(hor.y); mf(hor.z);
            mf(gnd.x); mf(gnd.y); mf(gnd.z);
            mix(irrSz); mix(prefSz); mix(prefM); mix(lutSz);
            mix(kIBLVersion);
            return h;
        }

        static NkPath IBLCachePath(const char* dir, uint32 hash) {
            char buf[64];
            snprintf(buf, sizeof(buf), "nk_ibl_%08x.bin", hash);
            NkPath cacheDir;
            if (dir && dir[0]) {
                cacheDir = NkPath(dir) / "ibl";
            } else {
                cacheDir = NkPath::GetExecutableDirectory() / "cache" / "ibl";
            }
            NkDirectory::CreateRecursive(cacheDir);
            return cacheDir / buf;
        }

        // Charge le cache et uploade directement sur le device.
        // Retourne true si le cache est valide et a ete uploade.
        static bool TryLoadIBLCache(const NkPath& path, uint32 hash,
                                     NkIDevice* device,
                                     NkTextureHandle brdfLUT,
                                     NkTextureHandle irr,
                                     NkTextureHandle pref,
                                     uint32 irrSz, uint32 prefSz, uint32 prefM, uint32 lutSz) {
            FILE* f = fopen(path.CStr(), "rb");
            if (!f) {
                logger.Info("[IBL] Cache miss (fichier absent) : {0}\n", path.CStr());
                return false;
            }

            uint32 hdr[7];
            if (fread(hdr, 4, 7, f) != 7) { fclose(f); return false; }
            if (hdr[0] != kIBLMagic || hdr[1] != kIBLVersion || hdr[2] != hash
             || hdr[3] != irrSz || hdr[4] != prefSz || hdr[5] != prefM || hdr[6] != lutSz) {
                logger.Info("[IBL] Cache invalide (hash ou tailles differentes) : {0}\n", path.CStr());
                logger.Info("[IBL]   magic={0:#x} ver={1} hash={2:#x} irr={3} pref={4} mips={5} lut={6}\n",
                            hdr[0], hdr[1], hdr[2], hdr[3], hdr[4], hdr[5], hdr[6]);
                fclose(f); return false;
            }

            // LUT
            {
                std::vector<uint8_t> buf(lutSz * lutSz * 2);
                if (fread(buf.data(), 1, buf.size(), f) != buf.size()) { fclose(f); return false; }
                device->WriteTexture(brdfLUT, buf.data());
            }
            // Irradiance (6 faces)
            for (uint32 face = 0; face < 6; face++) {
                std::vector<uint8_t> buf(irrSz * irrSz * 4);
                if (fread(buf.data(), 1, buf.size(), f) != buf.size()) { fclose(f); return false; }
                device->WriteTextureRegion(irr, buf.data(), 0, 0, 0, irrSz, irrSz, 1, 0, face);
            }
            // Prefilter (mips x 6 faces)
            for (uint32 mip = 0; mip < prefM; mip++) {
                uint32 mipSz = prefSz >> mip; if (mipSz < 1) mipSz = 1;
                for (uint32 face = 0; face < 6; face++) {
                    std::vector<uint8_t> buf(mipSz * mipSz * 4);
                    if (fread(buf.data(), 1, buf.size(), f) != buf.size()) { fclose(f); return false; }
                    device->WriteTextureRegion(pref, buf.data(), 0, 0, 0, mipSz, mipSz, 1, mip, face);
                }
            }
            fclose(f);
            logger.Info("[IBL] Cache charge (hit) : {0}\n", path.CStr());
            return true;
        }

        static void SaveIBLCache(const NkPath& path, uint32 hash,
                                  uint32 irrSz, uint32 prefSz, uint32 prefM, uint32 lutSz,
                                  const uint8_t* lutData,
                                  const std::vector<std::vector<uint8_t>>& irrData,
                                  const std::vector<std::vector<std::vector<uint8_t>>>& prefData) {
            FILE* f = fopen(path.CStr(), "wb");
            if (!f) {
                logger.Errorf("[IBL] Impossible d'ecrire le cache : {0} (verifier les droits d'ecriture)\n",
                              path.CStr());
                return;
            }
            uint32 hdr[7] = { kIBLMagic, kIBLVersion, hash, irrSz, prefSz, prefM, lutSz };
            fwrite(hdr, 4, 7, f);
            fwrite(lutData, 1, lutSz * lutSz * 2, f);
            for (uint32 face = 0; face < 6; face++) fwrite(irrData[face].data(), 1, irrData[face].size(), f);
            for (uint32 mip = 0; mip < prefM; mip++)
                for (uint32 face = 0; face < 6; face++)
                    fwrite(prefData[mip][face].data(), 1, prefData[mip][face].size(), f);
            fclose(f);
            logger.Info("[IBL] Cache sauvegarde : {0}\n", path.CStr());
        }

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

            // LoadProcedural gere tout : LUT + sky + cache disque.
            LoadProcedural({0.40f, 0.55f, 0.80f},
                           {0.45f, 0.48f, 0.52f},
                           {0.10f, 0.08f, 0.06f});

            return mIrradiance.IsValid() && mPrefilter.IsValid() && mBrdfLUT.IsValid();
        }

        void NkEnvironmentSystem::LoadProcedural(const NkVec3f& skyTop,
                                                  const NkVec3f& horizon,
                                                  const NkVec3f& ground) {
            if (!mDevice) return;

            const uint32 irrSize  = mCfg.irradianceSize > 0 ? mCfg.irradianceSize : 32;
            const uint32 prefSize = mCfg.prefilterSize  > 0 ? mCfg.prefilterSize  : 128;
            const uint32 prefMips = mCfg.prefilterMips  > 0 ? mCfg.prefilterMips  : 5;
            const uint32 lutSize  = mCfg.brdfLUTSize    > 0 ? mCfg.brdfLUTSize    : 256;

            // ── Cache disque ────────────────────────────────────────────────────
            uint32 hash = IBLHash(skyTop, horizon, ground, irrSize, prefSize, prefMips, lutSize);
            if (mCfg.enableCache) {
                auto path = IBLCachePath(mCfg.cacheDir, hash);
                if (TryLoadIBLCache(path, hash, mDevice, mBrdfLUT, mIrradiance, mPrefilter,
                                     irrSize, prefSize, prefMips, lutSize)) {
                    return;  // charge depuis cache : aucun calcul CPU
                }
            }

            auto& pool = ::nkentseu::threading::NkThreadPool::GetGlobal();

            // ── BRDF LUT ────────────────────────────────────────────────────────
            // 32 samples suffisent pour un gradient sky sans hautes frequences.
            std::vector<uint8_t> lutData(lutSize * lutSize * 2);
            if (mBrdfLUT.IsValid()) {
                const uint32 N = 32;
                pool.ParallelFor(lutSize, [&](nk_size yi) {
                    uint32 y = (uint32)yi;
                    float roughness = (float(y) + 0.5f) / float(lutSize);
                    for (uint32 x = 0; x < lutSize; x++) {
                        float NoV = (float(x) + 0.5f) / float(lutSize);
                        float A = 0.f, B = 0.f;
                        IntegrateBRDF(NoV, roughness, N, A, B);
                        uint32 idx = (y * lutSize + x) * 2;
                        lutData[idx + 0] = uint8_t(NkClamp(A, 0.f, 1.f) * 255.f);
                        lutData[idx + 1] = uint8_t(NkClamp(B, 0.f, 1.f) * 255.f);
                    }
                }, /*grainSize=*/8);
                pool.Join();
                mDevice->WriteTexture(mBrdfLUT, lutData.data());
            }

            // ── Irradiance convolution ──────────────────────────────────────────
            // 4 strates × 16 azimuts = 64 samples : suffisant pour ciel gradient.
            std::vector<std::vector<uint8_t>> irrData(6);
            if (mIrradiance.IsValid()) {
                auto irrFaceWork = [&](uint32 face) {
                    auto& buf = irrData[face];
                    buf.assign(irrSize * irrSize * 4, 0);
                    const float kPI = 3.14159265358979f;
                    const uint32 nTheta = 4;
                    const uint32 nPhi   = 16;
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
                            buf[idx+0] = (uint8_t)(NkClamp(Cx, 0.f, 1.f) * 255.f);
                            buf[idx+1] = (uint8_t)(NkClamp(Cy, 0.f, 1.f) * 255.f);
                            buf[idx+2] = (uint8_t)(NkClamp(Cz, 0.f, 1.f) * 255.f);
                            buf[idx+3] = 255;
                        }
                    }
                };
                pool.ParallelFor(6, [&](nk_size f) { irrFaceWork((uint32)f); }, 1);
                pool.Join();
                for (uint32 f = 0; f < 6; f++)
                    mDevice->WriteTextureRegion(mIrradiance, irrData[f].data(),
                                                 0, 0, 0, irrSize, irrSize, 1, 0, f);
            }

            // ── Prefilter GGX par mip ───────────────────────────────────────────
            // 16 samples : qualite correcte pour ciel sans hautes frequences.
            std::vector<std::vector<std::vector<uint8_t>>> prefData(prefMips,
                std::vector<std::vector<uint8_t>>(6));
            if (mPrefilter.IsValid()) {
                const uint32 numSamples = 16;
                for (uint32 mip = 0; mip < prefMips; mip++) {
                    uint32 mipSize = prefSize >> mip; if (mipSize < 1) mipSize = 1;
                    float roughness = (prefMips > 1) ? float(mip) / float(prefMips - 1) : 0.f;
                    auto& mipBufs = prefData[mip];

                    auto prefFaceWork = [&](uint32 face) {
                        auto& buf = mipBufs[face];
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
                                    buf[idx+0]=(uint8_t)(NkClamp(s.x,0.f,1.f)*255.f);
                                    buf[idx+1]=(uint8_t)(NkClamp(s.y,0.f,1.f)*255.f);
                                    buf[idx+2]=(uint8_t)(NkClamp(s.z,0.f,1.f)*255.f);
                                    buf[idx+3]=255; continue;
                                }
                                float Cx = 0.f, Cy = 0.f, Cz = 0.f, sumW = 0.f;
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
                                        NkVec3f s = SampleSkyGradient(Lx,Ly,Lz,skyTop,horizon,ground);
                                        Cx+=s.x*NoL; Cy+=s.y*NoL; Cz+=s.z*NoL; sumW+=NoL;
                                    }
                                }
                                float inv = (sumW > 1e-6f) ? 1.f / sumW : 0.f;
                                Cx*=inv; Cy*=inv; Cz*=inv;
                                uint32 idx = (y * mipSize + x) * 4;
                                buf[idx+0]=(uint8_t)(NkClamp(Cx,0.f,1.f)*255.f);
                                buf[idx+1]=(uint8_t)(NkClamp(Cy,0.f,1.f)*255.f);
                                buf[idx+2]=(uint8_t)(NkClamp(Cz,0.f,1.f)*255.f);
                                buf[idx+3]=255;
                            }
                        }
                    };
                    pool.ParallelFor(6, [&](nk_size f) { prefFaceWork((uint32)f); }, 1);
                    pool.Join();
                    for (uint32 f = 0; f < 6; f++)
                        mDevice->WriteTextureRegion(mPrefilter, mipBufs[f].data(),
                                                     0, 0, 0, mipSize, mipSize, 1, mip, f);
                }
            }

            // ── Sauvegarde du cache pour les prochains lancements ───────────────
            if (mCfg.enableCache) {
                auto path = IBLCachePath(mCfg.cacheDir, hash);
                SaveIBLCache(path, hash, irrSize, prefSize, prefMips, lutSize,
                              lutData.data(), irrData, prefData);
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
