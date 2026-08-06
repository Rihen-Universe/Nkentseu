#pragma once
/**
 * @File    NkOpusCodec.h
 * @Brief   Decodeur Opus (.opus / Ogg-Opus, RFC 6716 + 7845) pour NKAudio.
 * @Author  TEUGUIA TADJUIDJE Rodolf Séderis
 * @License Proprietary - All Rights Reserved (see LICENSE)
 *
 * @Architecture
 *  Fine couche d'adaptation au-dessus du decodeur Opus from-scratch de
 *  NKMedia (media::NkOpusFile) : demux Ogg + OpusHead (pre-skip, gain) +
 *  decodage SILK/CELT paquet-par-paquet -> PCM int16 48 kHz, converti ici
 *  en AudioSample float32 normalise (pipeline NKAudio standard).
 *
 * @Limites (V1, heritees du decodeur NKMedia)
 *  - Mono uniquement (un fichier stereo est refuse proprement).
 *  - Mode hybride SILK+CELT (configs TOC 12-15) refuse proprement.
 *
 * @Output
 *  AudioSample interleaved float32 normalise, 48 kHz.
 */

#include "NKAudio/NkAudio.h"

namespace nkentseu {
	namespace audio {

		class NKENTSEU_AUDIO_API NkOpusCodec {
			public:
				/// Decode un buffer .opus (Ogg-Opus) complet en AudioSample.
				/// @param data       Debut du fichier ("OggS" + flux OpusHead).
				/// @param size       Taille du buffer en octets.
				/// @param allocator  Allocateur pour le buffer de sortie
				///                   (nullptr = allocateur global par defaut).
				/// @return  AudioSample valide si decode OK, vide sinon.
				static AudioSample Decode(const uint8 *data, usize size,
										  memory::NkAllocator *allocator = nullptr) noexcept;
		};

	} // namespace audio
} // namespace nkentseu
