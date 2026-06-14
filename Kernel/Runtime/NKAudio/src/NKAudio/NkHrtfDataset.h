/**
 * @File    NkHrtfDataset.h
 * @Brief   HRTF dataset : conteneur des HRIR (Head-Related Impulse Responses)
 *          pour spatialisation 3D AAA via convolution.
 *
 * @Author  TEUGUIA TADJUIDJE Rodolf Séderis
 * @License Proprietary - Free to use and modify
 *
 * @Description
 *  HRTF (Head-Related Transfer Function) : la signature acoustique de la tete
 *  + oreilles + buste qui filtre le son selon sa direction d'arrivee. Permet
 *  une perception 3D precise au casque (impossible avec le simple panning).
 *
 *  HRIR (Head-Related Impulse Response) = reponse impulsionnelle de la tete
 *  pour une direction donnee. Un fichier court (~128-256 samples) par
 *  couple azimut/elevation.
 *
 *  Datasets standard :
 *   - MIT KEMAR (public domain, ~710 positions, le plus utilise)
 *   - CIPIC (45 sujets humains)
 *   - SADIE (University of York)
 *
 *  Format Nkentseu compact : un seul fichier .nkhrtf (binaire) contenant
 *  tous les HRIR. Voir specs format en bas du fichier.
 *
 * @MultiPlatform
 *  100% C++17 standard. Aucune dependance OS.
 */

#pragma once

#ifndef NK_NKAUDIO_SRC_NKAUDIO_NKHRTFDATASET_H_INCLUDED
#define NK_NKAUDIO_SRC_NKAUDIO_NKHRTFDATASET_H_INCLUDED

#include "NKAudio/NKAudio.h"
#include "NKMemory/NkAllocator.h"

namespace nkentseu {
    namespace audio {

        /**
         * @brief Une paire de HRIR (gauche + droite) pour une direction donnee.
         *
         * Pointeurs vers les samples dans le buffer global du dataset.
         */
        struct NKENTSEU_AUDIO_API NkHrirPair {
            const float32* leftIR  = nullptr;
            const float32* rightIR = nullptr;
            int32          length  = 0;     ///< Nombre de samples par IR
        };

        /**
         * @brief Dataset HRTF (collection de HRIR pre-mesures).
         *
         * Charge depuis un fichier .nkhrtf (format compact Nkentseu).
         * Permet le lookup d'une paire HRIR pour une direction donnee
         * (azimut + elevation), avec interpolation entre voisins.
         */
        class NKENTSEU_AUDIO_API NkHrtfDataset {
        public:
            static constexpr int32 MAX_AZIMUTHS    = 72;  ///< 5° resolution typique
            static constexpr int32 MAX_ELEVATIONS  = 14;  ///< -40° a 90° par 10°
            static constexpr int32 MAX_IR_LENGTH   = 512; ///< Limite raisonnable

            NkHrtfDataset() noexcept = default;
            ~NkHrtfDataset() noexcept;

            NkHrtfDataset(const NkHrtfDataset&)            = delete;
            NkHrtfDataset& operator=(const NkHrtfDataset&) = delete;

            /**
             * @brief Charge un dataset HRTF depuis un fichier .nkhrtf binaire.
             *
             * Format :
             *   - 4 bytes magic   : "NKHR"
             *   - 4 bytes version : 1
             *   - 4 bytes int32   : sampleRate (typique 44100)
             *   - 4 bytes int32   : nAzimuths
             *   - 4 bytes int32   : nElevations
             *   - 4 bytes int32   : irLength
             *   - Pour chaque (elev, az) : 2*irLength float32 (L, R)
             *
             * @param path     Chemin du fichier .nkhrtf
             * @param allocator Allocateur pour les buffers internes
             * @return true si succes
             */
            bool LoadFromFile(const char* path,
                               memory::NkAllocator* allocator = nullptr) noexcept;

            /**
             * @brief Libere les buffers internes.
             */
            void Unload() noexcept;

            /**
             * @brief Genere un dataset HRTF synthetique base sur un modele
             *        spherique (head sphere). Pas de fichier requis.
             *
             * Modele :
             *  - Tete = sphere de rayon 8.75 cm
             *  - ITD (Interaural Time Difference) : delai inter-aural
             *    base sur l'azimut et le rayon de la tete.
             *  - Head shadow : attenuation lowpass cote oppose a la source.
             *  - HRIR = impulse delayee + filtree par cote.
             *
             * Moins precis qu'un dataset mesure (MIT KEMAR / CIPIC) mais
             * donne une perception 3D fonctionnelle pour MVP. Aucune
             * dependance externe.
             *
             * @param sampleRate    Sample rate cible (typique 44100 ou 48000)
             * @param irLength      Longueur de chaque HRIR (typique 128 ou 256)
             * @param nAzimuths     Resolution azimut (typique 36 = pas 10°)
             * @param nElevations   Resolution elevation (typique 14)
             * @param allocator     Allocateur (nullptr = defaut)
             * @return true si succes.
             */
            bool CreateSynthetic(int32 sampleRate,
                                  int32 irLength,
                                  int32 nAzimuths,
                                  int32 nElevations,
                                  memory::NkAllocator* allocator = nullptr) noexcept;

            /**
             * @brief Cherche la paire HRIR la plus proche pour (azimut, elevation).
             *
             * Plus proche voisin (nearest-neighbor) pour MVP. Interpolation
             * bilineaire en option future.
             *
             * @param azimuthDeg   Azimut en degres [0..360] (0=devant, 90=droite)
             * @param elevationDeg Elevation en degres [-40..90]
             * @return Pair HRIR (left + right). length=0 si dataset non charge.
             */
            NkHrirPair GetHRIR(float32 azimuthDeg, float32 elevationDeg) const noexcept;

            // ── Acces lecture ────────────────────────────────────────────

            bool   IsLoaded() const noexcept       { return mData != nullptr; }
            int32  GetSampleRate() const noexcept  { return mSampleRate; }
            int32  GetIrLength() const noexcept    { return mIrLength; }
            int32  GetAzimuthCount() const noexcept   { return mNumAzimuths; }
            int32  GetElevationCount() const noexcept { return mNumElevations; }

        private:
            // Layout interne : mData[elev_idx][az_idx][channel(L=0/R=1)][sample]
            // Stocke comme un seul buffer continu de taille
            //   nAzimuths * nElevations * 2 * irLength * sizeof(float32)
            float32*             mData            = nullptr;
            usize                mDataSize        = 0;
            int32                mSampleRate      = 0;
            int32                mNumAzimuths     = 0;
            int32                mNumElevations   = 0;
            int32                mIrLength        = 0;
            float32              mElevationMinDeg = -40.0f;
            float32              mElevationMaxDeg = 90.0f;
            memory::NkAllocator* mAllocator       = nullptr;

            /// Calcule l'index lineaire (az, elev) -> offset dans mData.
            usize IndexOf(int32 azIdx, int32 elevIdx) const noexcept;

            /// Helper clamp
            static float32 Clampf(float32 v, float32 mn, float32 mx) noexcept;
        };

    } // namespace audio
} // namespace nkentseu

#endif // NK_NKAUDIO_SRC_NKAUDIO_NKHRTFDATASET_H_INCLUDED
