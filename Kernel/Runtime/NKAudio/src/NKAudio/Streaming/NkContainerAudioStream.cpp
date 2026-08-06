/**
 * @File    NkContainerAudioStream.cpp
 * @Brief   Implementation ContainerAudioStream (demux + decode AAC/PCM par paquets).
 * @Author  TEUGUIA TADJUIDJE Rodolf Séderis
 * @License Proprietary - All Rights Reserved (see LICENSE)
 */

#include "NKAudio/Streaming/NkContainerAudioStream.h"
#include "NKMedia/NkMediaDemux.h"
#include "NKLogger/NkLog.h"
#include <cstring>

namespace nkentseu {
	namespace audio {

		// Octets par échantillon (1 canal) pour une profondeur PCM donnée. 0 = non géré
		// (24/32-bit flottant, ou toute profondeur non standard).
		static int32 PcmBytesPerSample(int32 bits) {
			if (bits == 8 || bits == 16 || bits == 24)
				return bits / 8;
			return 0;
		}

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

			mChannels = (tr->channels == 2) ? 2 : 1;
			mSampleRate = tr->sampleRate;

			if (tr->codec == NkString("aac")) {
				mCodec = Codec::AAC;
				if (!mDecoder.Init(mSampleRate, mChannels)) {
					logger.Info("[ContainerAudioStream] Init decodeur AAC echoue (sr={0}, ch={1}) : {2}", mSampleRate,
								mChannels, path);
					return false;
				}
			} else if (tr->codec == NkString("pcm")) {
				mCodec = Codec::PCM;
				mBitsPerSample = tr->bitsPerSample;
				mPcmBigEndian = tr->pcmBigEndian;
				if (PcmBytesPerSample(mBitsPerSample) == 0) {
					logger.Info("[ContainerAudioStream] PCM {0}-bit non gere (8/16/24 seulement) : {1}",
								mBitsPerSample, path);
					return false;
				}
			} else if (tr->codec == NkString("opus")) {
				// Opus-dans-WebM/MKV : paquets Opus BRUTS dans les SimpleBlocks (pas
				// d'encapsulation Ogg). Le decodeur NKMedia decode paquet par paquet et sort
				// TOUJOURS du 48 kHz (sortie native Opus, quel que soit le taux d'origine).
				// Mono et STEREO (SILK MS->LR + CELT mid/side/intensity). >2 canaux
				// (mapping multicanal) -> echec propre, le caller peut retomber ailleurs.
				if (tr->channels > 2) {
					logger.Info("[ContainerAudioStream] Opus multicanal (>2) non gere : {0}", path);
					return false;
				}
				mCodec = Codec::OPUS;
				mChannels = (tr->channels == 2) ? 2 : 1;
				mSampleRate = 48000;
				mOpus.Init(mChannels);
				// Pre-skip : OpusHead (RFC 7845 par5.1) dans le CodecPrivate de la piste —
				// uint16 LE a l'offset 10. Repli 312 (valeur libopus standard) si absent.
				mOpusPreSkipLeft = 312;
				if (tr->codecPrivate.Size() >= 19 && tr->codecPrivate[0] == 'O' &&
					tr->codecPrivate[1] == 'p' && tr->codecPrivate[2] == 'u' &&
					tr->codecPrivate[3] == 's') {
					mOpusPreSkipLeft =
						(int32)tr->codecPrivate[10] | ((int32)tr->codecPrivate[11] << 8);
				}
			} else {
				logger.Info("[ContainerAudioStream] Codec '{0}' non gere (AAC/PCM/Opus) : {1}",
							tr->codec.CStr(), path);
				return false;
			}

			mPackets.Reserve(packets.Size());
			usize totalBytes = 0;
			nk_int64 totalDiscard = 0;
			for (usize i = 0; i < packets.Size(); ++i) {
				PacketRef ref{packets[i].offset, packets[i].size};
				// DiscardPadding WebM (dernier paquet Opus) : ns -> frames 48 kHz a jeter
				// en FIN de paquet decode (arrondi au plus proche, convention ffmpeg).
				if (packets[i].discardPaddingNs > 0)
					ref.discardFrames =
						(nk_int32)((packets[i].discardPaddingNs * 48 + 500000) / 1000000);
				mPackets.PushBack(ref);
				totalBytes += packets[i].size;
				totalDiscard += ref.discardFrames;
			}
			mNumPackets = mPackets.Size();
			mLeftover.Resize(2048); // >= 1024 frames stereo (AAC) ; redimensionne si besoin (PCM)

			if (mCodec == Codec::AAC) {
				// Priming : le 1er paquet AAC (1024 echantillons de delai encodeur standard) est
				// decode puis JETE, pour s'aligner sur l'horodatage video (meme convention validee
				// vs ffmpeg : corr 1.000000 sur les flux de test). Le PCM n'a PAS de priming (pas
				// d'encodeur a delai de bloc -> aucun paquet a sauter).
				mPacketIndex = 0;
				DecodeNextPacket();
				mLeftoverAvail = 0;
				mLeftoverPos = 0;
				mApproxFrameCount = (nk_int64)((mNumPackets > 0 ? mNumPackets - 1 : 0)) * 1024;
			} else if (mCodec == Codec::OPUS) {
				mPacketIndex = 0;
				// Un paquet Opus peut porter jusqu'a 120 ms = 5760 frames a 48 kHz (× canaux).
				mLeftover.Resize((usize)5760 * (usize)mChannels);
				// ~20 ms (960 ech.) par paquet, moins le pre-skip et le DiscardPadding de fin
				// de flux : APPROXIMATIF (suffisant, meme usage que l'estimation AAC).
				mApproxFrameCount = (nk_int64)mNumPackets * 960 - mOpusPreSkipLeft - totalDiscard;
				if (mApproxFrameCount < 0)
					mApproxFrameCount = 0;
			} else {
				mPacketIndex = 0;
				const int32 bytesPerFrame = PcmBytesPerSample(mBitsPerSample) * mChannels;
				mApproxFrameCount = (bytesPerFrame > 0) ? (nk_int64)(totalBytes / (usize)bytesPerFrame) : 0;
			}

			logger.Info("[ContainerAudioStream] Ouvert : {0} ({1}, {2} Hz, {3} ch, {4} paquets, ~{5} frames)", path,
						(mCodec == Codec::AAC) ? "AAC" : ((mCodec == Codec::OPUS) ? "Opus" : "PCM"),
						mSampleRate, mChannels, (long long)mNumPackets,
						(long long)mApproxFrameCount);
			return true;
		}

		int32 ContainerAudioStream::DecodeNextOpusPacket() noexcept {
			// Le pre-skip (delai de l'encodeur, OpusHead) se consomme sur les PREMIERS
			// echantillons decodes — potentiellement sur plusieurs paquets si > 1 paquet.
			for (;;) {
				if (mPacketIndex >= mNumPackets)
					return 0;
				const PacketRef &p = mPackets[mPacketIndex];
				++mPacketIndex;
				if (p.offset + p.size > mBytes.Size())
					continue; // paquet corrompu/tronque -> saute
				// DecodePacket renvoie le nombre TOTAL de valeurs int16 (frames × canaux).
				const int32 vals =
					mOpus.DecodePacket(mBytes.Data() + p.offset, (int32)p.size, mLeftover.Data());
				int32 n = vals / mChannels; // frames
				// DiscardPadding (WebM) : jette les dernieres frames du paquet (padding
				// d'encodeur en fin de flux — ffmpeg fait pareil).
				if (p.discardFrames > 0) {
					n -= p.discardFrames;
					if (n < 0)
						n = 0;
				}
				if (n <= 0)
					continue; // paquet indecodable -> saute (silence)
				if (mOpusPreSkipLeft >= n) {
					mOpusPreSkipLeft -= n; // paquet entierement mange par le pre-skip
					continue;
				}
				mLeftoverPos = mOpusPreSkipLeft; // partie restante du pre-skip sur CE paquet
				mOpusPreSkipLeft = 0;
				mLeftoverAvail = n;
				return n - mLeftoverPos;
			}
		}

		int32 ContainerAudioStream::DecodeNextAacPacket() noexcept {
			if (mPacketIndex >= mNumPackets)
				return 0;
			const PacketRef &p = mPackets[mPacketIndex];
			++mPacketIndex;
			if (p.offset + p.size > mBytes.Size())
				return 0; // paquet corrompu/tronque -> saute (silence pour ce paquet)
			const int32 n = mDecoder.DecodeFrame(mBytes.Data() + p.offset, (int32)p.size, mLeftover.Data());
			mLeftoverPos = 0;
			mLeftoverAvail = n;
			return n;
		}

		// PCM brut -> int16 entrelace (aucun etat entre paquets, simple conversion de format).
		// 8-bit = non-signe centre sur 128 (convention WAV/QuickTime) ; 16-bit = copie directe
		// (avec byte-swap si big-endian "twos") ; 24-bit = on ne garde que l'octet de poids fort
		// (equivalent a une troncature 24->16 bit, la meme perte que la conversion int16 finale
		// operee de toute facon par ReadFrames sur TOUTE profondeur superieure a 16 bit).
		int32 ContainerAudioStream::DecodeNextPcmPacket() noexcept {
			if (mPacketIndex >= mNumPackets)
				return 0;
			const PacketRef &p = mPackets[mPacketIndex];
			++mPacketIndex;
			if (p.offset + p.size > mBytes.Size())
				return 0;
			const int32 bytesPerSample = PcmBytesPerSample(mBitsPerSample);
			const int32 bytesPerFrame = bytesPerSample * mChannels;
			if (bytesPerFrame <= 0)
				return 0;
			const int32 frames = (int32)(p.size / (usize)bytesPerFrame);
			if (frames <= 0)
				return 0;
			if ((usize)(frames * mChannels) > mLeftover.Size())
				mLeftover.Resize((usize)(frames * mChannels));
			const nk_uint8 *src = mBytes.Data() + p.offset;
			nk_int16 *dst = mLeftover.Data();
			const int32 total = frames * mChannels;
			if (mBitsPerSample == 16) {
				for (int32 i = 0; i < total; ++i) {
					const nk_uint8 *s = src + (usize)i * 2;
					dst[i] = mPcmBigEndian ? (nk_int16)(((nk_uint16)s[0] << 8) | (nk_uint16)s[1])
										   : (nk_int16)(((nk_uint16)s[1] << 8) | (nk_uint16)s[0]);
				}
			} else if (mBitsPerSample == 8) {
				for (int32 i = 0; i < total; ++i)
					dst[i] = (nk_int16)(((int32)src[i] - 128) * 256); // non-signe centre -> signe 16-bit
			} else { // 24-bit : garde l'octet de poids fort (troncature vers int16)
				for (int32 i = 0; i < total; ++i) {
					const nk_uint8 *s = src + (usize)i * 3;
					dst[i] = mPcmBigEndian ? (nk_int16)(((nk_uint16)s[0] << 8) | (nk_uint16)s[1])
										   : (nk_int16)(((nk_uint16)s[2] << 8) | (nk_uint16)s[1]);
				}
			}
			mLeftoverPos = 0;
			mLeftoverAvail = frames;
			return frames;
		}

		int32 ContainerAudioStream::DecodeNextPacket() noexcept {
			if (mCodec == Codec::AAC)
				return DecodeNextAacPacket();
			if (mCodec == Codec::OPUS)
				return DecodeNextOpusPacket();
			return DecodeNextPcmPacket();
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
				const nk_int16 *src = mLeftover.Data() + (usize)mLeftoverPos * (usize)mChannels;
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
			mLeftoverAvail = 0;
			mLeftoverPos = 0;
			if (mCodec == Codec::AAC) {
				// +1 : le paquet 0 est le priming (deja consomme/jete a Open) — la frame 0 logique
				// correspond au paquet 1. Granularite paquet (1024 frames), arrondi au plus proche.
				usize target = (usize)(frameIdx / 1024) + 1;
				if (target > mNumPackets)
					target = mNumPackets;
				mPacketIndex = target;
				return true;
			}
			if (mCodec == Codec::OPUS) {
				// Granularite paquet (~20 ms = 960 ech. a 48 kHz), APPROXIMATIF comme l'AAC.
				// L'etat inter-trame (LPC/LTP SILK, overlap CELT) perd sa continuite apres un
				// saut -> on reinitialise le decodeur pour repartir d'un etat propre (bref
				// glitch de convergence, imperceptible — meme categorie que la note PNS AAC).
				usize target = (usize)(frameIdx / 960);
				if (target > mNumPackets)
					target = mNumPackets;
				mPacketIndex = target;
				mOpus.Init(mChannels);
				mOpusPreSkipLeft = 0; // le pre-skip ne concerne que le tout debut du flux
				return true;
			}
			// PCM : seek EXACT (pas de granularite de frame imposee par un codec) — parcourt les
			// paquets en sommant leur nombre de frames jusqu'a atteindre la cible. Le reliquat
			// (frameIdx tombant AU MILIEU d'un paquet) est absorbe en decodant ce paquet puis en
			// avancant mLeftoverPos dessus.
			const int32 bytesPerFrame = PcmBytesPerSample(mBitsPerSample) * mChannels;
			if (bytesPerFrame <= 0) {
				mPacketIndex = mNumPackets;
				return true;
			}
			nk_int64 acc = 0;
			usize pi = 0;
			for (; pi < mNumPackets; ++pi) {
				const nk_int64 pf = (nk_int64)(mPackets[pi].size / (usize)bytesPerFrame);
				if (acc + pf > frameIdx)
					break;
				acc += pf;
			}
			mPacketIndex = pi;
			if (pi < mNumPackets && frameIdx > acc) {
				// La cible tombe au milieu du paquet `pi` : le decoder puis sauter le reliquat.
				if (DecodeNextPcmPacket() > 0) {
					const int32 skip = (int32)(frameIdx - acc);
					mLeftoverPos = (skip < mLeftoverAvail) ? skip : mLeftoverAvail;
				}
			}
			return true;
		}

	} // namespace audio
} // namespace nkentseu
