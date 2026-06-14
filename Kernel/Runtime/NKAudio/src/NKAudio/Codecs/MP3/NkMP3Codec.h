#pragma once
/**
 * @File    NkMP3Codec.h
 * @Brief   Decodeur MP3 (MPEG-1/2 Layer 3) from scratch, sans dependance.
 * @Author  TEUGUIA TADJUIDJE Rodolf Séderis
 * @License Proprietary - Free to use and modify
 *
 * @Reference
 *  ISO/IEC 11172-3 (MPEG-1 audio), ISO/IEC 13818-3 (MPEG-2 audio).
 *  Implementation totalement autonome - aucune dependance externe
 *  (pas de minimp3, pas de libmp3lame, etc.).
 *
 * @Support
 *  - MPEG-1 Layer 3 : sample rates 32/44.1/48 kHz, bitrates 32-320 kbps
 *  - MPEG-2 Layer 3 : sample rates 16/22.05/24 kHz, bitrates 8-160 kbps
 *  - Modes : Mono, Stereo, Joint Stereo (intensity + mid/side), Dual Channel
 *  - Variable Bit Rate (VBR) : supporté (chaque frame independante)
 *  - ID3v1 (debut) et ID3v2 (fin) : detectes et skippes
 *
 * @Output
 *  AudioSample float32 normalise dans [-1.0, 1.0] (interleaved par frame).
 *
 * @Status
 *  v1.2 : decodeur Layer 3 complet (port minimp3 CC0, scalaire portable).
 *  Tables Huffman, IMDCT 12/36 + window switching, polyphase synthesis
 *  filter bank et bit reservoir cross-frame sont livres.
 *  Limites connues : MPEG Layer 1 et Layer 2 silencieusement skippes,
 *  pas de SIMD, pas de streaming incremental (buffer complet en RAM),
 *  pas d'API de seek (SeekToFrame / SeekToTime).
 */

#include "NKAudio/NKAudio.h"

namespace nkentseu {
    namespace audio {

        class NKENTSEU_AUDIO_API NkMP3Codec {
            public:
                /// Decode un buffer MP3 complet en AudioSample (float32 interleaved).
                /// @param data       Pointeur vers le buffer MP3 (peut commencer par ID3v2).
                /// @param size       Taille du buffer en octets.
                /// @param allocator  Allocateur a utiliser pour la sortie.
                /// @return  AudioSample valide si OK, vide sinon.
                static AudioSample Decode(const uint8* data, usize size,
                                          memory::NkAllocator* allocator = nullptr) noexcept;
        };

    } // namespace audio
} // namespace nkentseu
