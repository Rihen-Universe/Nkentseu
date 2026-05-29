#pragma once
/**
 * @File    NkFLACCodec.h
 * @Brief   Decodeur FLAC (Free Lossless Audio Codec) from scratch.
 * @Author  TEUGUIA TADJUIDJE Rodolf Séderis
 * @License Proprietary - Free to use and modify
 *
 * @Reference
 *  Spec officielle Xiph.org : https://xiph.org/flac/format.html
 *  Implementation totalement autonome - aucune dependance externe.
 *
 * @Architecture
 *  Le decodeur traite un buffer FLAC complet en memoire en 3 etapes :
 *   1. Validation magic "fLaC" et parsing des blocs de metadata
 *      (STREAMINFO obligatoire : sample rate, channels, bps, total samples).
 *   2. Iteration sur les frames. Chaque frame contient :
 *      - Un header (sync code 14 bits + parametres + UTF-8 frame number + CRC-8)
 *      - N subframes (un par canal), chacun :
 *          * CONSTANT, VERBATIM, FIXED (order 0-4), ou LPC (order 1-32)
 *          * Section residuelle codee en Rice partitionne
 *      - Un padding optionnel + CRC-16
 *   3. Decorrelation des canaux pour stereo joint (L+S, S+R, M+S),
 *      puis conversion en float32 normalise [-1.0, 1.0].
 *
 * @Output
 *  AudioSample interleaved float32 normalise (compatible avec le reste du
 *  pipeline NKAudio : mixer, effects, backends).
 */

#include "NKAudio/NKAudio.h"

namespace nkentseu {
    namespace audio {

        class NKENTSEU_AUDIO_API NkFLACCodec {
            public:
                /// Decode un buffer FLAC complet en AudioSample (float32 interleaved).
                /// @param data       Pointeur vers le debut du fichier FLAC (magic "fLaC").
                /// @param size       Taille du buffer en octets.
                /// @param allocator  Allocateur a utiliser pour le buffer sortie
                ///                   (nullptr = allocateur global par defaut).
                /// @return  AudioSample valide si decode OK, AudioSample vide sinon.
                ///          Le caller libere via AudioLoader::Free() ou sample.Free().
                static AudioSample Decode(const uint8* data, usize size,
                                          memory::NkAllocator* allocator = nullptr) noexcept;
        };

    } // namespace audio
} // namespace nkentseu
