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
 *  fichier, charge UNE FOIS en RAM — les paquets eux-memes, compresses, pas le PCM).
 *  ReadFrames() decode alors les paquets un par un (AAC-LC, 1024 echantillons/canal
 *  chacun) au fil de l'eau, sur demande du AudioStreamPlayer (thread worker dedie).
 *  Seul le CODEC AAC est gere pour l'instant (couvre la quasi-totalite des MP4 reels
 *  — telephone/YouTube/export ffmpeg) ; Open() echoue proprement sinon.
 *
 *  Le PRIMING de l'encodeur AAC (1024 echantillons de delai standard) est saute au
 *  premier paquet, pour aligner l'audio sur l'horodatage video (meme convention que
 *  la comparaison ffmpeg qui a valide le decodeur : corr 1.000000).
 *
 * @note Seek() est APPROXIMATIF (granularite 1024 echantillons = ~23ms a 44100 Hz,
 *       du meme ordre que la granularite d'une image video) : saute au paquet le
 *       plus proche. L'etat PNS (bruit de confort) perd sa continuite inter-trame
 *       exacte apres un seek — imperceptible (c'est un GENERATEUR de bruit, pas un
 *       signal a preserver bit-exact).
 */

#include "NKAudio/Streaming/NkAudioStream.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKMedia/Codecs/Aac/NkAacDecoder.h"

namespace nkentseu {
	namespace audio {

		class NKENTSEU_AUDIO_API ContainerAudioStream : public IAudioStream {
			public:
				ContainerAudioStream() = default;
				~ContainerAudioStream() override = default;

				/// Ouvre `path` (MP4/MOV/WebM...), demuxe la piste audio et prepare le
				/// decodeur. Renvoie false si conteneur non supporte, pas de piste audio,
				/// ou codec != AAC (echec propre : le caller peut retomber sur autre chose).
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
					return mPacketIndex >= mNumPackets && mLeftoverAvail == 0;
				}

			private:
				// Decode le paquet `mPacketIndex` dans mLeftover (int16 entrelace), avance
				// mPacketIndex. Renvoie le nombre de frames decodees (0 si echec/EOF).
				int32 DecodeNextPacket() noexcept;

				media::NkAacDecoder mDecoder;
				NkVector<nk_uint8> mBytes; // fichier complet (les paquets pointent dedans)

				// Paquets demuxes : (offset, taille) dans mBytes.
				struct PacketRef {
						usize offset;
						usize size;
				};
				NkVector<PacketRef> mPackets;
				usize mNumPackets = 0;

				usize mPacketIndex = 0; // prochain paquet a decoder
				int32 mSampleRate = 0;
				int32 mChannels = 0;
				nk_int64 mApproxFrameCount = 0;

				// Reste du dernier paquet decode, pas encore consomme par ReadFrames.
				nk_int16 mLeftover[2048] = {0}; // 1024 frames x 2 canaux max
				int32 mLeftoverAvail = 0;		 // frames restantes dans mLeftover
				int32 mLeftoverPos = 0;			 // position de lecture dans mLeftover
		};

	} // namespace audio
} // namespace nkentseu
