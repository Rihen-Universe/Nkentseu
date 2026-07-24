// =============================================================================
// NKMedia/Audio/Containers/NkWavWriter.h
// -----------------------------------------------------------------------------
// Muxer conteneur WAV (RIFF/WAVE) — écrit un fichier .wav from-scratch (sans
// ffmpeg). Accepte des échantillons PCM DÉJÀ entrelacés (entiers 8/16/24/32-bit
// ou flottants 32/64-bit) tels quels — aucune conversion, pas de dépendance à
// NKAudio (même philosophie que NkAviWriter : le muxer ne connaît que les
// octets qu'on lui donne + les métadonnées de format). En-tête canonique
// 44 octets (RIFF/fmt /data), tailles rapiécées à la fermeture.
//
// Réf : spec RIFF/WAVE (Microsoft/IBM). Zero-STL, nkentseu::media.
//
// AUTEUR : Rihen — LICENCE : usage régi par le fichier LICENSE à la racine du dépôt
// =============================================================================
#pragma once

#include "NKCore/NkTypes.h"
#include "NKFileSystem/NkFile.h"

namespace nkentseu {
	namespace media {

		struct NkWavWriter {
			public:
				// Ouvre `path` et écrit l'en-tête RIFF/WAVE (fmt + début de data). `bitsPerSample`
				// = 8/16/24/32 en PCM entier, ou 32/64 si `isFloat` (WAVE_FORMAT_IEEE_FLOAT).
				bool Open(const char *path, int32 sampleRate, int32 channels, int32 bitsPerSample,
						  bool isFloat = false);

				// Écrit des échantillons entrelacés BRUTS (`data`/`size` en octets, déjà dans le
				// format annoncé à Open — aucune conversion). Peut être appelé plusieurs fois
				// (streaming) : le chunk `data` grandit à chaque appel.
				bool WriteSamples(const void *data, usize size);

				// Rapièce les tailles RIFF/data. Ferme le fichier.
				bool Close();

				bool IsOpen() const {
					return mFile.IsOpen();
				}
				usize BytesWritten() const {
					return mDataSize;
				}

				// Écrit un petit WAV PCM 16-bit dans un fichier temporaire, le relit en RIFF
				// brut (sans passer par un lecteur WAV externe) et vérifie l'en-tête et les
				// échantillons — round-trip bit-exact, indépendant de NKAudio.
				static bool SelfTest();

			private:
				NkFile mFile;
				usize mDataSize = 0;

				// Positions à rapiécer.
				nk_int64 mRiffSizePos = 0;
				nk_int64 mDataSizePos = 0;

				void PutU32(uint32 v);
				void PutU16(uint16 v);
				void PutBytes(const void *p, usize n);
				void PatchU32(nk_int64 pos, uint32 v);
		};

	} // namespace media
} // namespace nkentseu
