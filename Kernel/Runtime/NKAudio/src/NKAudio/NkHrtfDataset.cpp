/**
 * @File    NkHrtfDataset.cpp
 * @Brief   Implementation NkHrtfDataset (chargement + lookup HRIR).
 *
 * @Author  TEUGUIA TADJUIDJE Rodolf Séderis
 * @License Proprietary - Free to use and modify
 */

#include "NKAudio/NkHrtfDataset.h"
#include "NKFileSystem/NkFile.h"
#include "NKLogger/NkLog.h"
#include <cstring>
#include <cmath>

namespace nkentseu {
    namespace audio {

        static constexpr uint32 NKHR_MAGIC   = 0x52484B4Eu; // 'NKHR' little-endian
        static constexpr uint32 NKHR_VERSION = 1u;

        NkHrtfDataset::~NkHrtfDataset() noexcept {
            Unload();
        }

        void NkHrtfDataset::Unload() noexcept {
            if (mData) {
                memory::NkFree(mData, mAllocator);
                mData = nullptr;
            }
            mDataSize        = 0;
            mSampleRate      = 0;
            mNumAzimuths     = 0;
            mNumElevations   = 0;
            mIrLength        = 0;
            mAllocator       = nullptr;
        }

        usize NkHrtfDataset::IndexOf(int32 azIdx, int32 elevIdx) const noexcept {
            // [elev][az][channel(2)][sample(irLength)]
            // Flatten : elev * (nAz * 2 * irLen) + az * (2 * irLen)
            return (usize(elevIdx) * usize(mNumAzimuths) + usize(azIdx))
                 * usize(2) * usize(mIrLength);
        }

        bool NkHrtfDataset::LoadFromFile(const char* path,
                                          memory::NkAllocator* allocator) noexcept {
            Unload();
            if (!path) return false;

            NkFile f;
            if (!f.Open(path, NkFileMode::NK_READ_BINARY)) {
                logger.Error("[NkHrtfDataset] Impossible d'ouvrir : {0}", path);
                return false;
            }

            // Header (6 uint32)
            uint32 header[6];
            if (f.Read(header, sizeof(header)) != sizeof(header)) {
                logger.Error("[NkHrtfDataset] Lecture header echec.");
                f.Close();
                return false;
            }
            if (header[0] != NKHR_MAGIC) {
                logger.Error("[NkHrtfDataset] Magic 'NKHR' invalide.");
                f.Close();
                return false;
            }
            if (header[1] != NKHR_VERSION) {
                logger.Error("[NkHrtfDataset] Version {0} non supportee (attend {1}).",
                             header[1], NKHR_VERSION);
                f.Close();
                return false;
            }
            mSampleRate    = int32(header[2]);
            mNumAzimuths   = int32(header[3]);
            mNumElevations = int32(header[4]);
            mIrLength      = int32(header[5]);

            if (mNumAzimuths <= 0 || mNumAzimuths > MAX_AZIMUTHS ||
                mNumElevations <= 0 || mNumElevations > MAX_ELEVATIONS ||
                mIrLength <= 0 || mIrLength > MAX_IR_LENGTH) {
                logger.Error("[NkHrtfDataset] Dimensions invalides : {0}az x {1}el x {2}len.",
                             mNumAzimuths, mNumElevations, mIrLength);
                f.Close();
                return false;
            }

            // Allouer buffer total : nAz * nElev * 2 * irLen * float32
            usize total = usize(mNumAzimuths) * usize(mNumElevations)
                        * usize(2) * usize(mIrLength) * sizeof(float32);
            mAllocator = allocator;
            mData = static_cast<float32*>(
                memory::NkAlloc(total, allocator, sizeof(float32)));
            if (!mData) {
                logger.Error("[NkHrtfDataset] Allocation buffer ({0} bytes) echec.",
                             (int)total);
                f.Close();
                return false;
            }
            mDataSize = total;

            if (f.Read(mData, total) != total) {
                logger.Error("[NkHrtfDataset] Lecture data echec.");
                Unload();
                f.Close();
                return false;
            }
            f.Close();

            logger.Info("[NkHrtfDataset] Charge : {0}az x {1}el x {2} samples a {3} Hz "
                        "({4} KB).",
                        mNumAzimuths, mNumElevations, mIrLength, mSampleRate,
                        (int)(total / 1024));
            return true;
        }

        // ────────────────────────────────────────────────────────────────────
        //  CreateSynthetic : genere un dataset HRTF base sur un modele
        //  spherique. Permet d'avoir une spatialisation 3D fonctionnelle
        //  sans dataset externe.
        //
        //  Modele physique simplifie :
        //   - Tete = sphere rayon r=0.0875m
        //   - ITD (Interaural Time Difference) :
        //       ITD(az) = (r/c) * (sin(az) + az)  pour |az| <= pi/2
        //     ou c=343 m/s (vitesse du son dans l'air).
        //   - Head shadow : cote oppose attenue + lowpass.
        //   - HRIR L et R = impulse decalee + filtree.
        //
        //  Resultat : HRIR plausibles, perception 3D au casque OK.
        //  Moins precis qu'un dataset MIT KEMAR mais immediat.
        // ────────────────────────────────────────────────────────────────────

        bool NkHrtfDataset::CreateSynthetic(int32 sampleRate,
                                             int32 irLength,
                                             int32 nAzimuths,
                                             int32 nElevations,
                                             memory::NkAllocator* allocator) noexcept {
            Unload();
            if (sampleRate <= 0 || irLength <= 0 || nAzimuths <= 0 || nElevations <= 0)
                return false;
            if (irLength > MAX_IR_LENGTH || nAzimuths > MAX_AZIMUTHS
                || nElevations > MAX_ELEVATIONS) {
                logger.Error("[NkHrtfDataset] CreateSynthetic : dimensions hors limites.");
                return false;
            }

            mSampleRate    = sampleRate;
            mIrLength      = irLength;
            mNumAzimuths   = nAzimuths;
            mNumElevations = nElevations;
            mAllocator     = allocator;

            usize total = usize(nAzimuths) * usize(nElevations)
                        * usize(2) * usize(irLength) * sizeof(float32);
            mData = static_cast<float32*>(
                memory::NkAlloc(total, allocator, sizeof(float32)));
            if (!mData) {
                logger.Error("[NkHrtfDataset] CreateSynthetic : alloc echec ({0} bytes).",
                             (int)total);
                return false;
            }
            mDataSize = total;
            ::memset(mData, 0, total);

            constexpr float32 kHeadRadius    = 0.0875f;   // m
            constexpr float32 kSpeedOfSound  = 343.0f;    // m/s
            constexpr float32 kPi            = 3.14159265358979f;
            constexpr float32 kTwoPi         = 6.28318530717958f;

            float32 maxItdSec = kHeadRadius * (1.0f + kPi * 0.5f) / kSpeedOfSound;
            int32   maxItdSamples = int32(maxItdSec * float32(sampleRate));
            if (maxItdSamples >= irLength / 2) maxItdSamples = (irLength / 2) - 1;

            for (int32 ie = 0; ie < nElevations; ++ie) {
                // Elevation lineaire de mElevationMinDeg a mElevationMaxDeg
                float32 elNorm = float32(ie) / float32(nElevations - 1 > 0 ? nElevations - 1 : 1);
                float32 elDeg  = mElevationMinDeg + elNorm * (mElevationMaxDeg - mElevationMinDeg);
                float32 elRad  = elDeg * (kPi / 180.0f);
                float32 elCos  = ::cosf(elRad);
                // Attenuation elevation : sons venant du haut/bas un peu attenuees
                float32 elAttenu = 0.7f + 0.3f * elCos;

                for (int32 ia = 0; ia < nAzimuths; ++ia) {
                    // Azimut [0, 360)
                    float32 azDeg = float32(ia) * 360.0f / float32(nAzimuths);
                    // Convertir vers azimut signe [-180, 180] (0=devant, 90=droite)
                    float32 azSigned = (azDeg > 180.0f) ? (azDeg - 360.0f) : azDeg;
                    float32 azRad    = azSigned * (kPi / 180.0f);

                    // ITD (sec) selon Woodworth's formula
                    float32 absAz = (azRad < 0.0f) ? -azRad : azRad;
                    float32 itdSec = (kHeadRadius / kSpeedOfSound)
                                   * (::sinf(absAz) + absAz);
                    if (absAz > kPi * 0.5f) {
                        itdSec = (kHeadRadius / kSpeedOfSound)
                               * (::sinf(kPi - absAz) + (kPi - absAz));
                    }
                    int32 itdSamples = int32(itdSec * float32(sampleRate));
                    if (itdSamples > maxItdSamples) itdSamples = maxItdSamples;

                    // Quelle oreille est plus pres ? L si az<0, R si az>0
                    int32 delayL, delayR;
                    if (azSigned < 0.0f) {
                        delayL = 0;
                        delayR = itdSamples;
                    } else {
                        delayL = itdSamples;
                        delayR = 0;
                    }

                    // ILD (Interaural Level Difference) : cote eloigne attenue
                    // Approximation : -3 a -6 dB au maximum (az = ±90°)
                    float32 ildLinear = ::sinf(absAz) * 0.5f; // 0 (front) -> 0.5 (cote)
                    float32 gainNear  = 1.0f * elAttenu;
                    float32 gainFar   = (1.0f - ildLinear) * elAttenu;

                    float32 gainL, gainR;
                    if (azSigned < 0.0f) {
                        // Source a gauche : L=near, R=far
                        gainL = gainNear; gainR = gainFar;
                    } else {
                        gainL = gainFar; gainR = gainNear;
                    }

                    // Construit le HRIR : impulse decalee + filtree par lowpass
                    // (head shadow simple pour cote oppose : 1-pole lowpass)
                    float32* irL = mData + IndexOf(ia, ie);
                    float32* irR = irL + irLength;

                    // Coefficient lowpass pour head shadow
                    // Cote oppose : cutoff plus bas (max 8 kHz a |az|=90°)
                    float32 lpfCoefL = (azSigned > 0.0f)
                        ? Clampf(8000.0f * (1.0f - ildLinear) / float32(sampleRate), 0.05f, 0.5f)
                        : 0.5f;
                    float32 lpfCoefR = (azSigned < 0.0f)
                        ? Clampf(8000.0f * (1.0f - ildLinear) / float32(sampleRate), 0.05f, 0.5f)
                        : 0.5f;

                    // Apply impulse + decroissance exponentielle douce (simule
                    // les reflexions/diffractions de l'oreille externe)
                    float32 stateL = 0.0f, stateR = 0.0f;
                    for (int32 i = 0; i < irLength; ++i) {
                        // Impulse a delayL/R, puis decroissance
                        float32 srcL = (i == delayL) ? gainL : 0.0f;
                        float32 srcR = (i == delayR) ? gainR : 0.0f;
                        // Lowpass one-pole : y = y_prev + coef*(x - y_prev)
                        stateL += lpfCoefL * (srcL - stateL);
                        stateR += lpfCoefR * (srcR - stateR);
                        irL[i] = stateL;
                        irR[i] = stateR;
                    }
                    // Normalisation (eviter clipping post-convolution) :
                    // somme(|ir|) ≈ gainNear → ramener IR a energie ~1
                    // Skip pour rester simple (le limiter master gere le clip)
                }
            }

            logger.Info("[NkHrtfDataset] Synthetic genere : {0}az x {1}el x {2} samples a {3} Hz "
                        "({4} KB).",
                        nAzimuths, nElevations, irLength, sampleRate, (int)(total / 1024));
            return true;
        }

        // ────────────────────────────────────────────────────────────────────
        //  Helper Clampf utilise par CreateSynthetic
        // ────────────────────────────────────────────────────────────────────

        float32 NkHrtfDataset::Clampf(float32 v, float32 mn, float32 mx) noexcept {
            return v < mn ? mn : (v > mx ? mx : v);
        }

        NkHrirPair NkHrtfDataset::GetHRIR(float32 azimuthDeg,
                                           float32 elevationDeg) const noexcept {
            NkHrirPair result{};
            if (!mData || mNumAzimuths <= 0 || mNumElevations <= 0) return result;

            // Normalize azimut [0, 360)
            while (azimuthDeg < 0.0f)   azimuthDeg += 360.0f;
            while (azimuthDeg >= 360.0f) azimuthDeg -= 360.0f;

            // Clamp elevation
            if (elevationDeg < mElevationMinDeg) elevationDeg = mElevationMinDeg;
            if (elevationDeg > mElevationMaxDeg) elevationDeg = mElevationMaxDeg;

            // Map azimut [0, 360) -> [0, nAz)
            float32 azNorm = azimuthDeg / 360.0f;
            int32 azIdx = int32(azNorm * float32(mNumAzimuths));
            if (azIdx >= mNumAzimuths) azIdx = mNumAzimuths - 1;
            if (azIdx < 0) azIdx = 0;

            // Map elevation [min, max] -> [0, nElev)
            float32 elNorm = (elevationDeg - mElevationMinDeg)
                          / (mElevationMaxDeg - mElevationMinDeg);
            int32 elIdx = int32(elNorm * float32(mNumElevations));
            if (elIdx >= mNumElevations) elIdx = mNumElevations - 1;
            if (elIdx < 0) elIdx = 0;

            usize base = IndexOf(azIdx, elIdx);
            result.leftIR  = mData + base;
            result.rightIR = mData + base + mIrLength;
            result.length  = mIrLength;
            return result;
        }

    } // namespace audio
} // namespace nkentseu
