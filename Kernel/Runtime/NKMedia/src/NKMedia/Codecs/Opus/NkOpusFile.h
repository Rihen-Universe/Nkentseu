// =============================================================================
// NKMedia/Codecs/Opus/NkOpusFile.h
// -----------------------------------------------------------------------------
// Lecture COMPLÈTE d'un fichier .opus (encapsulation Ogg, RFC 7845) → PCM
// int16 48 kHz : demux Ogg (NkMediaDemux) + en-tête OpusHead (canaux,
// pre-skip, gain de sortie) + décodage paquet-par-paquet (NkOpusDecoder) +
// retrait du pre-skip + trim à la granule finale + application du gain.
//
// LIMITES V1 (héritées de NkOpusDecoder) : mono uniquement (un fichier stéréo
// est refusé proprement), mode hybride SILK+CELT non décodé. Zero-STL.
//
// AUTEUR : Rihen — LICENCE : usage régi par le fichier LICENSE (racine)
// =============================================================================
#pragma once

#include "NKCore/NkTypes.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/String/NkString.h"

namespace nkentseu {
	namespace media {

		struct NkOpusFile {
				// Détection rapide : buffer Ogg dont le premier flux est Opus.
				static bool Probe(const uint8 *data, usize size);

				// Décode un fichier .opus complet (buffer mémoire) → PCM int16
				// 48 kHz. outChannels = canaux décodés (1 ou 2, entrelacé), outSampleRate =
				// 48000. Renvoie false (et remplit outError si fourni) sur
				// format invalide / stéréo / flux non-Opus.
				static bool Decode(const uint8 *data, usize size, NkVector<int16> &outPcm, int32 &outChannels,
								   int32 &outSampleRate, NkString *outError = nullptr);

				// Variante fichier (lecture via NKFileSystem).
				static bool DecodeFile(const char *path, NkVector<int16> &outPcm, int32 &outChannels,
									   int32 &outSampleRate, NkString *outError = nullptr);

				// Auto-test headless : forge un flux Ogg-Opus minimal en mémoire
				// (OpusHead + OpusTags + 2 trames CELT silence) et vérifie le
				// décodage (960 échantillons après pre-skip + trim granule).
				static bool SelfTest();
		};

	} // namespace media
} // namespace nkentseu
