#pragma once
/**
 * @File    NkContainerAudioStream.h
 * @Brief   IAudioStream sur un CONTENEUR VIDEO (MP4/MOV/WebM...) : demuxe la piste
 *          audio ET decode par PAQUETS a la demande (jamais tout en RAM), pour les
 *          longs films/videos (un film de 2h en AAC stereo 44100 decode entier =
 *          ~2,5 Go de float32 -> inacceptable pour une lecture RAM d'un coup).
 * @Author  TEUGUIA TADJUIDJE Rodolf Séderis
 * @License Proprietary - Free to use and modify
 *
 * @Architecture
 *  A l'ouverture : NkMediaDemux extrait la LISTE des paquets (offset+taille dans le
 *  fichier, charge UNE FOIS en RAM — les paquets eux-memes, compresses/bruts, pas
 *  le PCM decode). ReadFrames() decode alors les paquets un par un au fil de l'eau,
 *  sur demande du AudioStreamPlayer (thread worker dedie). Codecs geres :
 *   - **AAC-LC** : 1024 echantillons/canal par paquet (raw_data_block).
 *   - **PCM non compresse** (twos/sowt/lpcm, 8/16/24/32-bit) : taille de paquet
 *     VARIABLE (aucune notion de "frame" cote conteneur) — juste une conversion de
 *     format (endianness + profondeur -> int16), aucun etat entre paquets.
 *   - **Opus-dans-WebM** (mono + STEREO) : paquets Opus BRUTS dans les SimpleBlocks
 *     (pas d'encapsulation Ogg), decodes par NkOpusDecoder (SILK/CELT/hybride,
 *     stereo MS->LR + mid/side/intensity), sortie native 48 kHz. Le PRE-SKIP (delai
 *     encodeur, RFC 7845) est lu dans l'OpusHead du CodecPrivate de la piste
 *     (uint16 LE offset 10, repli 312) et consomme sur les premieres trames —
 *     valide corr 1.000000 lag 0 vs ffmpeg (mono).
 *  Autre codec (ex. MP3 embarque, Opus multicanal >2) -> Open() echoue proprement (le
 *  caller retombe sur un autre chemin, ex. RAM complete via NkMP3Codec — voir OpenAudioStream()).
 *
 *  Le PRIMING de l'encodeur AAC (1024 echantillons de delai standard) est saute au
 *  premier paquet, pour aligner l'audio sur l'horodatage video (meme convention que
 *  la comparaison ffmpeg qui a valide le decodeur : corr 1.000000). Le PCM n'a PAS
 *  de priming (pas d'encodeur a delai de bloc).
 *
 * @note Seek() est APPROXIMATIF pour l'AAC (granularite 1024 echantillons = ~23ms a
 *       44100 Hz, du meme ordre que la granularite d'une image video) : saute au
 *       paquet le plus proche. L'etat PNS (bruit de confort) perd sa continuite
 *       inter-trame exacte apres un seek — imperceptible (c'est un GENERATEUR de
 *       bruit, pas un signal a preserver bit-exact). Pour le PCM, Seek() est EXACT
 *       (pas de granularite de frame imposee par un codec).
 */

#include "NKAudio/Streaming/NkAudioStream.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKMedia/Codecs/Aac/NkAacDecoder.h"
#include "NKMedia/Codecs/Opus/NkOpusDecoder.h"

namespace nkentseu {
	namespace audio {

		class NKENTSEU_AUDIO_API ContainerAudioStream : public IAudioStream {
			public:
				ContainerAudioStream() = default;
				~ContainerAudioStream() override = default;

				/// Ouvre `path` (MP4/MOV/WebM...), demuxe la piste audio et prepare le
				/// decodeur. Renvoie false si conteneur non supporte, pas de piste audio,
				/// ou codec non gere (AAC/PCM/Opus seulement — echec propre : le caller
				/// peut retomber sur autre chose, ex. RAM complete pour du MP3 embarque).
				bool Open(const char *path) noexcept;

				int32 ReadFrames(float32 *outBuf, int32 maxFrames) noexcept override;
				bool Seek(nk_int64 frameIdx) noexcept override;

				nk_int64 GetFrameCount() const noexcept override {
					return mApproxFrameCount;
				}

				int32 GetSampleRate() const noexcept override {
					return mSampleRate;
				}

				int32 GetChannels() const noexcept override {
					return mChannels;
				}

				bool IsEOF() const noexcept override {
					return mPacketIndex >= mNumPackets && mLeftoverPos >= mLeftoverAvail;
				}

			private:
				enum class Codec { AAC, PCM, OPUS };

				// Decode/convertit le paquet `mPacketIndex` dans mLeftover (int16 entrelace),
				// avance mPacketIndex. Renvoie le nombre de frames obtenues (0 si echec/EOF).
				int32 DecodeNextPacket() noexcept;
				int32 DecodeNextAacPacket() noexcept;
				int32 DecodeNextPcmPacket() noexcept;
				int32 DecodeNextOpusPacket() noexcept;

				media::NkAacDecoder mDecoder;
				media::NkOpusDecoder mOpus;	   // Opus-dans-WebM (mono, sortie 48 kHz native)
				int32 mOpusPreSkipLeft = 0;	   // echantillons de pre-skip (OpusHead) a jeter
				NkVector<nk_uint8> mBytes; // fichier complet (les paquets pointent dedans)

				// Paquets demuxes : (offset, taille) dans mBytes. discardFrames = frames 48 kHz
				// a JETER en fin de paquet decode (DiscardPadding WebM du dernier paquet Opus).
				struct PacketRef {
						usize offset;
						usize size;
						nk_int32 discardFrames = 0;
				};
				NkVector<PacketRef> mPackets;
				usize mNumPackets = 0;

				usize mPacketIndex = 0; // prochain paquet a decoder
				int32 mSampleRate = 0;
				int32 mChannels = 0;
				nk_int64 mApproxFrameCount = 0;

				Codec mCodec = Codec::AAC;
				int32 mBitsPerSample = 16; // PCM seulement
				bool mPcmBigEndian = false; // PCM seulement

				// Reste du dernier paquet decode/converti, pas encore consomme par ReadFrames.
				// Dynamique : un paquet AAC fait toujours <=1024 frames, mais un paquet PCM peut
				// etre bien plus gros (pas de taille de "frame" imposee par le conteneur).
				NkVector<nk_int16> mLeftover;
				int32 mLeftoverAvail = 0; // frames valides dans mLeftover
				int32 mLeftoverPos = 0;	  // position de lecture dans mLeftover
		};

	} // namespace audio
} // namespace nkentseu
