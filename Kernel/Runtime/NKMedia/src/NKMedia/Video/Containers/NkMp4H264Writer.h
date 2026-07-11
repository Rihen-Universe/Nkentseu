// =============================================================================
// NKMedia/Video/Containers/NkMp4H264Writer.h
// -----------------------------------------------------------------------------
// Muxer MP4 (ISOBMFF) from-scratch pour une piste vidéo H.264 (AVC). Écrit ftyp
// (isom/avc1) + mdat (échantillons = NAL longueur-préfixées 4 octets) + moov avec
// l'entrée d'échantillon 'avc1' + la box 'avcC' (AVCDecoderConfigurationRecord :
// SPS/PPS) + tables stbl (stsd/stts/stsc/stsz/stco/stss). Produit un `.mp4` cliquable
// lisible par VLC, QuickTime, navigateurs, éditeurs. Boxes big-endian. Zero-STL.
//
// AUTEUR : Rihen — LICENCE : usage régi par le fichier LICENSE à la racine du dépôt
// =============================================================================
#pragma once

#include "NKCore/NkTypes.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKFileSystem/NkFile.h"

namespace nkentseu {
	namespace media {

		struct NkMp4H264Writer {
			public:
				// Ouvre `path` (.mp4/.mov). Piste vidéo H.264 (avc1). fps = num/den.
				bool Open(const char *path, int32 width, int32 height, int32 fpsNum, int32 fpsDen);

				// Fixe SPS/PPS (octets du NAL, en-tête compris, SANS start code) pour la box avcC.
				void SetSps(const uint8 *data, uint32 size);
				void SetPps(const uint8 *data, uint32 size);

				// Écrit un échantillon (données = NAL VCL longueur-préfixées 4 octets big-endian).
				// `sync` = image clé (IDR) → listée dans stss.
				bool WriteSample(const uint8 *data, uint32 size, bool sync);

				// Ajoute une PISTE AUDIO PCM (LPCM 16 bits LE, 'sowt'). `lang3` = code ISO-639-2
				// (ex "fre","eng","spa") ; nullptr → "und". Renvoie l'index de piste (ou -1 si plein).
				// Plusieurs pistes audio = choix de langue (marquées alternate_group commun).
				int32 AddAudioTrack(int32 sampleRate, int32 channels, const char *lang3);
				// Ajoute des trames PCM entrelacées à la piste `trackIdx`. Bufferisé, écrit à Close().
				void AppendAudioPcm(int32 trackIdx, const int16 *interleaved, uint32 frames);

				// Ajoute une PISTE SOUS-TITRES texte 3GPP ('tx3g'). Renvoie l'index (ou -1 si plein).
				int32 AddSubtitleTrack(const char *lang3);
				// Ajoute un sous-titre (UTF-8) affiché de `startMs` pendant `durMs` sur la piste `trackIdx`.
				void AddSubtitle(int32 trackIdx, const char *utf8, uint32 startMs, uint32 durMs);

				bool Close();

				bool IsOpen() const {
					return mFile.IsOpen();
				}
				int32 FrameCount() const {
					return (int32)mSizes.Size();
				}
				bool HasParameterSets() const {
					return mSps.Size() > 0 && mPps.Size() > 0;
				}

			private:
				static const int32 kMaxAudio = 8;
				static const int32 kMaxSubs = 8;
				// Piste audio PCM : format + langue + buffer d'échantillons (rempli en cours d'écriture).
				struct AudioTrk {
						int32 rate = 0, channels = 0;
						uint16 lang = 0x55C4; // 'und'
						NkVector<uint8> pcm;
						uint32 offset = 0, frames = 0; // remplis à Close()
				};
				// Piste sous-titres 'tx3g' : échantillons texte (avec trous vides) construits à la volée.
				struct SubTrk {
						uint16 lang = 0x55C4;
						NkVector<uint8> data;	   // concat des échantillons [u16 len][texte]
						NkVector<uint32> sizes;	   // taille de chaque échantillon
						NkVector<uint32> durs;	   // durée (ms) de chaque échantillon
						uint32 offset = 0;		   // offset du bloc dans mdat (à Close)
						uint32 lastEndMs = 0;	   // fin du dernier sous-titre placé
				};

				NkFile mFile;
				NkVector<uint32> mSizes;   // taille de chaque échantillon
				NkVector<uint32> mOffsets; // offset absolu de chaque échantillon
				NkVector<uint8> mSync;	   // 1 = échantillon synchro (IDR)
				NkVector<uint8> mSps, mPps;
				int32 mWidth = 0, mHeight = 0, mFpsNum = 30, mFpsDen = 1;
				nk_int64 mMdatSizePos = 0;
				nk_int64 mMdatStart = 0;
				AudioTrk mAudio[kMaxAudio];
				int32 mNumAudio = 0;
				SubTrk mSubs[kMaxSubs];
				int32 mNumSubs = 0;
		};

	} // namespace media
} // namespace nkentseu
