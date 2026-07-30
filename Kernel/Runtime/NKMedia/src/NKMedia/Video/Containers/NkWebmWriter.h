// =============================================================================
// NKMedia/Video/Containers/NkWebmWriter.h
// -----------------------------------------------------------------------------
// Muxer conteneur WebM (EBML/Matroska) — écrit un fichier .webm from-scratch
// (sans ffmpeg). Accepte des paquets vidéo DÉJÀ encodés (VP8 ou VP9) et,
// optionnellement, des paquets audio Opus (le muxer N'ENCODE PAS : il empaquete
// des octets bruts fournis par l'appelant, exactement comme NkAviWriter /
// NkMovWriter / NkWavWriter). Structure produite :
//
//   [EBML Header]  (DocType "webm")
//   [Segment]
//     [Info]     TimestampScale (1 ms) + Duration + MuxingApp/WritingApp
//     [Tracks]   TrackEntry vidéo (V_VP8/V_VP9) [+ TrackEntry audio A_OPUS]
//     [Cluster]  Timestamp + SimpleBlock(s)   (un nouveau Cluster à chaque
//                image-clé vidéo ou quand l'horodatage relatif int16 déborde)
//     ...
//
// Les tailles EBML « inconnues » (Segment) sont réservées puis rapiécées à la
// fermeture ; chaque Cluster est bufferisé en mémoire (borné à un cluster) et
// écrit avec sa taille exacte. Complément du démux EBML déjà présent dans
// NkVideoReader (ParseWebm / WalkWebmTracks / WalkWebmVideoClusters).
//
// Réf : spec Matroska/WebM (EBML). Zero-STL, nkentseu::media.
//
// AUTEUR : Rihen — LICENCE : usage régi par le fichier LICENSE à la racine du dépôt
// =============================================================================
#pragma once

#include "NKCore/NkTypes.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKFileSystem/NkFile.h"

namespace nkentseu {
	namespace media {

		enum class NkWebmVideoCodec { NONE, VP8, VP9 };
		enum class NkWebmAudioCodec { NONE, OPUS };

		// Configuration passée à Open. La piste vidéo est obligatoire ; la piste
		// audio Opus est optionnelle (audioCodec == OPUS + un OpusHead valide).
		struct NkWebmConfig {
				// --- Vidéo (obligatoire) ---
				NkWebmVideoCodec videoCodec = NkWebmVideoCodec::VP9;
				int32 width = 0;
				int32 height = 0;

				// --- Audio Opus (optionnel) ---
				NkWebmAudioCodec audioCodec = NkWebmAudioCodec::NONE;
				int32 audioSampleRate = 48000;
				int32 audioChannels = 2;
				// CodecPrivate = en-tête « OpusHead » (magic "OpusHead" + version +
				// canaux + pre-skip + samplerate + gain + mapping). Requis si Opus.
				const uint8 *audioCodecPrivate = nullptr;
				usize audioCodecPrivateSize = 0;
		};

		struct NkWebmWriter {
			public:
				// Ouvre `path` et écrit l'en-tête EBML + Segment + Info + Tracks.
				bool Open(const char *path, const NkWebmConfig &config);

				// Ajoute un paquet vidéo déjà encodé. `timestampMs` = horodatage de
				// présentation en millisecondes. `isKeyframe` = image-clé (keyframe VP8/
				// VP9). Les paquets doivent être fournis en ordre d'horodatage croissant.
				bool AddVideoFrame(const uint8 *data, usize size, int64 timestampMs, bool isKeyframe);

				// Ajoute un paquet audio Opus déjà encodé (une trame Opus = 2.5..60 ms).
				// Toujours marqué image-clé (l'audio Opus n'a pas de prédiction inter-trame
				// côté conteneur). `timestampMs` en millisecondes.
				bool AddAudioFrame(const uint8 *data, usize size, int64 timestampMs);

				// Flush du dernier Cluster + rapiéçage Duration/Segment. Ferme le fichier.
				bool Finalize();

				bool IsOpen() const {
					return mFile.IsOpen();
				}
				int32 VideoFrameCount() const {
					return mVideoFrames;
				}
				int32 AudioFrameCount() const {
					return mAudioFrames;
				}

				// Round-trip autonome : écrit un WebM à un flux VP9 minimal synthétique
				// (deux « paquets » factices), relit les octets bruts et re-parse l'EBML
				// pour vérifier la structure (EBML Header, Segment, Info, Tracks vidéo,
				// Cluster/SimpleBlock avec le bon numéro de piste et le flag image-clé).
				static bool SelfTest();

			private:
				NkFile mFile;
				NkWebmConfig mConfig;

				int32 mVideoFrames = 0;
				int32 mAudioFrames = 0;
				int64 mMaxEndTsMs = 0; // pour Duration (dernier horodatage rencontré)

				// Numéros de piste fixes : vidéo = 1, audio = 2.
				static constexpr uint64 kVideoTrack = 1;
				static constexpr uint64 kAudioTrack = 2;

				// Cluster courant bufferisé (contenu : Timestamp + SimpleBlocks).
				NkVector<uint8> mCluster;
				int64 mClusterBaseTs = 0;
				bool mClusterOpen = false;

				// Positions à rapiécer dans le fichier.
				nk_int64 mSegmentSizePos = 0;   // les 8 octets de taille du Segment
				nk_int64 mSegmentDataStart = 0; // 1er octet du contenu du Segment
				nk_int64 mDurationPos = 0;		// les 8 octets du double Duration

				void StartCluster(int64 tsMs);
				void FlushCluster();
				bool AddBlock(uint64 trackNum, const uint8 *data, usize size, int64 timestampMs,
							  bool keyframe);
		};

	} // namespace media
} // namespace nkentseu
