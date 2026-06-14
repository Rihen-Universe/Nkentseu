#pragma once
/**
 * @File    NkOGGVorbisCodec.h
 * @Brief   Decodeur OGG Vorbis from-scratch (port stb_vorbis adapte Nkentseu).
 * @Author  TEUGUIA TADJUIDJE Rodolf Séderis
 * @License Proprietary - Free to use and modify
 *
 * @Reference
 *  Algorithme adapte de stb_vorbis.c v1.22 par Sean Barrett (Public Domain).
 *  Source originale : https://github.com/nothings/stb/blob/master/stb_vorbis.c
 *  Port en style Nkentseu : types int32/float32/uint8, allocator memory::NkAlloc,
 *  logger NkLog, lecture fichier via NkFile (heritage AAssetManager Android).
 *
 * @Format
 *  Ogg container (pages + packets) + Vorbis codec (codebooks dynamiques +
 *  floor curves + residue + MDCT). Spec : https://www.xiph.org/vorbis/doc/
 *
 * @Support
 *  - Mono, stereo, multi-canal (jusqu'a 8)
 *  - Sample rates 8 kHz a 192 kHz
 *  - Bitrates VBR (Variable Bit Rate) tous niveaux
 *  - Floor type 1 (le plus courant, type 0 deprecate)
 *  - Residue types 0, 1, 2
 *  - Headers Vorbis : identification, comment, setup
 *
 * @Output
 *  AudioSample float32 normalise dans [-1.0, 1.0] (interleaved par frame).
 */

#include "NKAudio/NKAudio.h"

namespace nkentseu {
    namespace audio {

        /**
         * @brief Configuration de l'encodeur OGG Vorbis.
         */
        struct NKENTSEU_AUDIO_API NkOGGVorbisEncoderConfig {
            float32 quality      = 0.4f;     ///< Qualite [-0.1, 1.0]. 0.4 ~ 128 kbps stereo.
            int32   bitrateNominal = -1;     ///< Si > 0 : mode CBR au bitrate specifie (bps).
            int32   bitrateMin     = -1;
            int32   bitrateMax     = -1;
        };

        class NKENTSEU_AUDIO_API NkOGGVorbisCodec {
            public:
                /// Decode un buffer OGG Vorbis complet en AudioSample.
                /// @param data       Pointeur vers le debut du fichier OGG ("OggS" magic).
                /// @param size       Taille du buffer en octets.
                /// @param allocator  Allocateur a utiliser pour le buffer sortie.
                /// @return  AudioSample valide si decode OK, vide sinon.
                static AudioSample Decode(const uint8* data, usize size,
                                          memory::NkAllocator* allocator = nullptr) noexcept;

                /**
                 * @brief Encode un AudioSample en buffer .ogg memoire.
                 *
                 * @param sample      Source audio (float32 interleaved).
                 * @param config      Parametres d'encodage (qualite ou bitrate).
                 * @param outSize     Taille du buffer encode en bytes (output).
                 * @param allocator   Allocateur pour le buffer retourne (nullptr = defaut).
                 * @return Buffer .ogg (a liberer via memory::NkFree), ou nullptr si echec.
                 */
                // static uint8* Encode(const AudioSample& sample,
                //                     const NkOGGVorbisEncoderConfig& config,
                //                     usize* outSize,
                //                     memory::NkAllocator* allocator = nullptr) noexcept;
        };

    } // namespace audio
} // namespace nkentseu
