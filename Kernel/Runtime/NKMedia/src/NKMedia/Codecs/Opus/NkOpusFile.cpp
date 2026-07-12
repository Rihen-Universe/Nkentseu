// =============================================================================
// NKMedia/Codecs/Opus/NkOpusFile.cpp
// -----------------------------------------------------------------------------
// Implémentation de la lecture .opus (Ogg-Opus, RFC 7845). Voir NkOpusFile.h.
//
// AUTEUR : Rihen — LICENCE : usage régi par le fichier LICENSE (racine)
// =============================================================================

#include "NkOpusFile.h"

#include "NKMedia/NkMediaProbe.h"
#include "NKMedia/NkMediaDemux.h"
#include "NKMedia/Codecs/Opus/NkOpusDecoder.h"
#include "NKMedia/Codecs/Opus/NkOpusRange.h"
#include "NKMath/NkFunctions.h"
#include "NKFileSystem/NkFile.h"

namespace nkentseu {
	namespace media {

		namespace {

			inline uint16 U16LE(const uint8 *p) {
				return (uint16)((uint16)p[0] | ((uint16)p[1] << 8));
			}

			inline bool IsOpusHead(const uint8 *pl, usize size) {
				return size >= 19 && pl[0] == 'O' && pl[1] == 'p' && pl[2] == 'u' && pl[3] == 's' && pl[4] == 'H' &&
					   pl[5] == 'e' && pl[6] == 'a' && pl[7] == 'd';
			}

			inline void SetError(NkString *outError, const char *msg) {
				if (outError != nullptr) {
					*outError = msg;
				}
			}

		} // namespace

		bool NkOpusFile::Probe(const uint8 *data, usize size) {
			if (NkMediaProbe::DetectContainer(data, size) != NkMediaContainer::NK_OGG)
				return false;
			// Premier flux BOS = OpusHead ? (payload de la première page)
			if (size < 28)
				return false;
			const uint8 nsegs = data[26];
			if (27u + nsegs + 8u > size)
				return false;
			return IsOpusHead(data + 27 + nsegs, size - 27 - nsegs);
		}

		bool NkOpusFile::Decode(const uint8 *data, usize size, NkVector<int16> &outPcm, int32 &outChannels,
								int32 &outSampleRate, NkString *outError) {
			outPcm.Clear();
			outChannels = 0;
			outSampleRate = 0;

			NkMediaInfo info;
			if (!NkMediaProbe::Probe(data, size, info) || info.container != NkMediaContainer::NK_OGG) {
				SetError(outError, "pas un fichier Ogg");
				return false;
			}

			NkVector<NkMediaPacket> packets;
			if (!NkMediaDemux::ExtractAudioPackets(data, size, info, packets) || packets.Size() < 2) {
				SetError(outError, "demux Ogg : aucun paquet");
				return false;
			}

			// Paquet 0 : OpusHead (version, canaux, pre-skip, gain, mapping).
			const uint8 *head = data + packets[0].offset;
			const usize headSize = packets[0].size;
			if (!IsOpusHead(head, headSize)) {
				SetError(outError, "premier flux Ogg non-Opus (OpusHead absent)");
				return false;
			}
			const uint8 version = head[8];
			if ((version >> 4) != 0) {
				SetError(outError, "version OpusHead incompatible");
				return false;
			}
			const int32 channels = (int32)head[9];
			const int32 preSkip = (int32)U16LE(head + 10);
			const int16 gainQ8 = (int16)U16LE(head + 16);
			if (channels != 1) {
				// Le décodeur NkOpusDecoder est mono pour l'instant (V1).
				SetError(outError, "Opus stereo pas encore supporte (decodeur mono)");
				return false;
			}

			// Paquet 1 : OpusTags (ignoré). Paquets 2.. : audio.
			NkOpusDecoder dec;
			dec.Init(1);

			int16 frame[960 * 4]; // marge : paquets multi-trames (codes 1-3)
			int64 lastGranule = -1;
			for (usize p = 2; p < packets.Size(); ++p) {
				// Mode hybride (TOC config 12-15) : pas encore décodé — refuser
				// proprement plutôt que de sortir un signal faux en silence.
				if (packets[p].size > 0) {
					const uint8 config = (uint8)(data[packets[p].offset] >> 3);
					if (config >= 12 && config <= 15) {
						outPcm.Clear();
						SetError(outError, "mode Opus hybride (SILK+CELT) pas encore supporte");
						return false;
					}
				}
				const int32 n = dec.DecodePacket(data + packets[p].offset, (int32)packets[p].size, frame);
				for (int32 i = 0; i < n; ++i)
					outPcm.PushBack(frame[i]);
				if (packets[p].granule >= 0)
					lastGranule = packets[p].granule;
			}
			if (outPcm.IsEmpty()) {
				SetError(outError, "aucun echantillon decode (mode hybride non supporte ?)");
				return false;
			}

			// Pre-skip : les preSkip premiers échantillons sont du warm-up.
			if (preSkip > 0 && (usize)preSkip < outPcm.Size()) {
				outPcm.Erase(outPcm.Begin(), outPcm.Begin() + preSkip);
			} else if (preSkip > 0) {
				outPcm.Clear();
				SetError(outError, "pre-skip >= flux decode");
				return false;
			}

			// Trim : la granulepos finale = nombre total d'échantillons (pre-skip
			// inclus) — coupe le padding du dernier paquet.
			if (lastGranule > 0) {
				const int64 valid = lastGranule - (int64)preSkip;
				if (valid > 0 && (usize)valid < outPcm.Size())
					outPcm.Resize((usize)valid);
			}

			// Gain de sortie Q7.8 dB (rarement non-nul).
			if (gainQ8 != 0) {
				const float32 g = math::NkPow(10.f, (float32)gainQ8 / (20.f * 256.f));
				for (usize i = 0; i < outPcm.Size(); ++i) {
					const int32 v = (int32)((float32)outPcm[i] * g);
					outPcm[i] = (int16)(v < -32768 ? -32768 : (v > 32767 ? 32767 : v));
				}
			}

			outChannels = 1;
			outSampleRate = 48000;
			return true;
		}

		bool NkOpusFile::DecodeFile(const char *path, NkVector<int16> &outPcm, int32 &outChannels,
									int32 &outSampleRate, NkString *outError) {
			NkVector<nk_uint8> bytes = NkFile::ReadAllBytes(path);
			if (bytes.Size() == 0) {
				SetError(outError, "lecture fichier impossible");
				return false;
			}
			return Decode(bytes.Data(), (usize)bytes.Size(), outPcm, outChannels, outSampleRate, outError);
		}

		// =====================================================================
		// SelfTest : forge un Ogg-Opus minimal en mémoire et le décode.
		// =====================================================================

		namespace {

			void OggAppendPage(NkVector<uint8> &out, uint8 headerType, int64 granule, uint32 serial,
							   uint32 seq, const uint8 *lacing, uint8 nsegs, const uint8 *payload,
							   usize payloadSize) {
				const uint8 magic[4] = {'O', 'g', 'g', 'S'};
				for (int i = 0; i < 4; ++i)
					out.PushBack(magic[i]);
				out.PushBack(0); // version
				out.PushBack(headerType);
				for (int i = 0; i < 8; ++i)
					out.PushBack((uint8)((uint64)granule >> (8 * i)));
				for (int i = 0; i < 4; ++i)
					out.PushBack((uint8)(serial >> (8 * i)));
				for (int i = 0; i < 4; ++i)
					out.PushBack((uint8)(seq >> (8 * i)));
				for (int i = 0; i < 4; ++i)
					out.PushBack(0); // CRC : non vérifié par le demux
				out.PushBack(nsegs);
				for (uint8 i = 0; i < nsegs; ++i)
					out.PushBack(lacing[i]);
				for (usize i = 0; i < payloadSize; ++i)
					out.PushBack(payload[i]);
			}

		} // namespace

		bool NkOpusFile::SelfTest() {
			// Paquet audio : trame CELT-only FB 20 ms silence (même forge que
			// NkOpusDecoder::SelfTest) → 960 échantillons.
			uint8 pkt[64];
			pkt[0] = (uint8)((31 << 3) | 0); // config 31, mono, code 0
			NkOpusRangeEncoder enc;
			enc.Init(pkt + 1, sizeof(pkt) - 1);
			enc.EncodeBitLogp(1, 15); // silence
			enc.Done();
			const uint8 pktLen = (uint8)(1 + (int32)enc.RangeBytes());

			// OpusHead : version 1, mono, pre-skip 100, rate 48000, gain 0, mapping 0.
			const int32 kPreSkip = 100;
			uint8 opusHead[19] = {'O', 'p', 'u', 's', 'H', 'e', 'a', 'd', 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0};
			opusHead[10] = (uint8)(kPreSkip & 0xFF);
			opusHead[11] = (uint8)(kPreSkip >> 8);
			opusHead[12] = (uint8)(48000 & 0xFF);
			opusHead[13] = (uint8)((48000 >> 8) & 0xFF);

			// OpusTags : vendor vide, 0 commentaire.
			uint8 opusTags[16] = {'O', 'p', 'u', 's', 'T', 'a', 'g', 's', 0, 0, 0, 0, 0, 0, 0, 0};

			// Flux : page BOS (OpusHead) + page (OpusTags) + page EOS (2 trames,
			// granule = 2*960).
			const uint32 serial = 0x1234;
			NkVector<uint8> ogg;
			{
				uint8 lace[1] = {19};
				OggAppendPage(ogg, 0x02, 0, serial, 0, lace, 1, opusHead, 19);
			}
			{
				uint8 lace[1] = {16};
				OggAppendPage(ogg, 0x00, 0, serial, 1, lace, 1, opusTags, 16);
			}
			{
				uint8 lace[2] = {pktLen, pktLen};
				NkVector<uint8> payload;
				for (uint8 i = 0; i < pktLen; ++i)
					payload.PushBack(pkt[i]);
				for (uint8 i = 0; i < pktLen; ++i)
					payload.PushBack(pkt[i]);
				OggAppendPage(ogg, 0x04, 2 * 960, serial, 2, lace, 2, payload.Data(), payload.Size());
			}

			if (!Probe(ogg.Data(), ogg.Size()))
				return false;

			NkVector<int16> pcm;
			int32 channels = 0;
			int32 rate = 0;
			if (!Decode(ogg.Data(), ogg.Size(), pcm, channels, rate))
				return false;
			if (channels != 1 || rate != 48000)
				return false;
			// 2 trames de 960 − pre-skip 100 = 1820 échantillons attendus.
			if (pcm.Size() != (usize)(2 * 960 - kPreSkip))
				return false;
			// Silence : tous les échantillons doivent être nuls.
			for (usize i = 0; i < pcm.Size(); ++i) {
				if (pcm[i] != 0)
					return false;
			}
			return true;
		}

	} // namespace media
} // namespace nkentseu
