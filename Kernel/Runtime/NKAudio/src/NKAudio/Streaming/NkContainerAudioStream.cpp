/**
 * @File    NkContainerAudioStream.cpp
 * @Brief   Implementation ContainerAudioStream (demux + decode AAC par paquets).
 * @Author  TEUGUIA TADJUIDJE Rodolf Séderis
 * @License Proprietary - Free to use and modify
 */

#include "NKAudio/Streaming/NkContainerAudioStream.h"
#include "NKMedia/NkMediaDemux.h"
#include "NKLogger/NkLog.h"
#include <cstring>

namespace nkentseu {
	namespace audio {

		bool ContainerAudioStream::Open(const char *path) noexcept {
			if (!path)
				return false;

			media::NkMediaInfo info;
			NkVector<media::NkMediaPacket> packets;
			if (!media::NkMediaDemux::ExtractAudioPacketsFile(path, mBytes, info, packets)) {
				logger.Info("[ContainerAudioStream] Demux echoue : {0}", path);
				return false;
			}
			const media::NkMediaTrack *tr = info.FirstAudio();
			if (!tr || packets.Empty()) {
				logger.Info("[ContainerAudioStream] Pas de piste audio : {0}", path);
				return false;
			}
			if (!(tr->codec == NkString("aac"))) {
				logger.Info("[ContainerAudioStream] Codec '{0}' non gere (AAC seulement) : {1}", tr->codec.CStr(),
							path);
				return false;
			}
			mChannels = (tr->channels == 2) ? 2 : 1;
			mSampleRate = tr->sampleRate;
			if (!mDecoder.Init(mSampleRate, mChannels)) {
				logger.Info("[ContainerAudioStream] Init decodeur AAC echoue (sr={0}, ch={1}) : {2}", mSampleRate,
							mChannels, path);
				return false;
			}

			mPackets.Reserve(packets.Size());
			for (usize i = 0; i < packets.Size(); ++i)
				mPackets.PushBack(PacketRef{packets[i].offset, packets[i].size});
			mNumPackets = mPackets.Size();

			// Priming : le 1er paquet AAC (1024 echantillons de delai encodeur standard) est
			// decode puis JETE, pour s'aligner sur l'horodatage video (meme convention validee
			// vs ffmpeg : corr 1.000000 sur les flux de test).
			mPacketIndex = 0;
			DecodeNextPacket();
			mLeftoverAvail = 0;
			mLeftoverPos = 0;

			// Duree approx (paquets restants x 1024, priming deja retire).
			mApproxFrameCount = (nk_int64)((mNumPackets > 0 ? mNumPackets - 1 : 0)) * 1024;

			logger.Info("[ContainerAudioStream] Ouvert : {0} ({1} Hz, {2} ch, {3} paquets, ~{4} frames)", path,
						mSampleRate, mChannels, (long long)mNumPackets, (long long)mApproxFrameCount);
			return true;
		}

		int32 ContainerAudioStream::DecodeNextPacket() noexcept {
			if (mPacketIndex >= mNumPackets)
				return 0;
			const PacketRef &p = mPackets[mPacketIndex];
			++mPacketIndex;
			if (p.offset + p.size > mBytes.Size())
				return 0; // paquet corrompu/tronque -> saute (silence pour ce paquet)
			const int32 n = mDecoder.DecodeFrame(mBytes.Data() + p.offset, (int32)p.size, mLeftover);
			mLeftoverPos = 0;
			mLeftoverAvail = n;
			return n;
		}

		int32 ContainerAudioStream::ReadFrames(float32 *outBuf, int32 maxFrames) noexcept {
			int32 written = 0;
			while (written < maxFrames) {
				if (mLeftoverPos >= mLeftoverAvail) {
					if (DecodeNextPacket() <= 0) {
						// Un SEUL paquet illisible (corrompu/décodage échoué) ne doit pas arrêter
						// tout le flux : on continue avec le suivant (miroir de l'ancien chemin
						// RAM, `for (...) { if (hors bornes) continue; }`). Seule la VRAIE fin de
						// flux (plus de paquets du tout) est terminale.
						if (mPacketIndex >= mNumPackets)
							break;
						continue;
					}
					continue;
				}
				const int32 avail = mLeftoverAvail - mLeftoverPos;
				const int32 want = maxFrames - written;
				const int32 n = (want < avail) ? want : avail;
				const nk_int16 *src = mLeftover + (usize)mLeftoverPos * (usize)mChannels;
				float32 *dst = outBuf + (usize)written * (usize)mChannels;
				for (int32 i = 0; i < n * mChannels; ++i)
					dst[i] = (float32)src[i] / 32768.0f;
				mLeftoverPos += n;
				written += n;
			}
			return written;
		}

		bool ContainerAudioStream::Seek(nk_int64 frameIdx) noexcept {
			if (frameIdx < 0)
				frameIdx = 0;
			// +1 : le paquet 0 est le priming (deja consomme/jete a Open) — la frame 0 logique
			// correspond au paquet 1. Granularite paquet (1024 frames), arrondi au plus proche.
			usize target = (usize)(frameIdx / 1024) + 1;
			if (target > mNumPackets)
				target = mNumPackets;
			mPacketIndex = target;
			mLeftoverAvail = 0;
			mLeftoverPos = 0;
			return true;
		}

	} // namespace audio
} // namespace nkentseu
