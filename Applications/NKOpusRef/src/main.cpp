// =============================================================================
// NKOpusRef — harnais de validation du décodeur Opus/CELT de NKMedia.
// -----------------------------------------------------------------------------
// Décode un clip (WebM/Opus CELT) via NKMedia (demux → TOC → NkCeltDecoder) et
// compare le PCM au fichier de RÉFÉRENCE (décodé par ffmpeg, 48 kHz mono float).
// Métriques : RMS erreur, corrélation, énergie relative. Sert de filet pour porter
// quant_all_bands (l'erreur doit tomber quand le décodage devient correct).
//
//   NKOpusRef.exe <clip_opus> <ref48k.wav>
//
// AUTEUR : Rihen — LICENCE : usage régi par le fichier LICENSE à la racine du dépôt
// =============================================================================
#include "NKMedia/NkMediaProbe.h"
#include "NKMedia/NkMediaDemux.h"
#include "NKMedia/Codecs/Opus/NkOpusPacket.h"
#include "NKMedia/Codecs/Opus/Celt/NkCeltDecoder.h"
#include "NKAudio/NkAudio.h"
#include "NKContainers/Sequential/NkVector.h"

#include <cstdio>
#include <cmath>

using namespace nkentseu;

static int32 LmFromMs(float32 ms) {
	// N = ms/1000*48000 ; LM = log2(N/120). 2.5→0, 5→1, 10→2, 20→3.
	if (ms <= 2.5f)
		return 0;
	if (ms <= 5.0f)
		return 1;
	if (ms <= 10.0f)
		return 2;
	return 3;
}

int main(int argc, char **argv) {
	if (argc < 3) {
		printf("Usage : NKOpusRef <clip_opus> <ref48k.wav>\n");
		return 2;
	}

	// 1) Demux → paquets Opus.
	NkVector<nk_uint8> bytes;
	media::NkMediaInfo info;
	NkVector<media::NkMediaPacket> packets;
	if (!media::NkMediaDemux::ExtractAudioPacketsFile(argv[1], bytes, info, packets)) {
		printf("[ERREUR] demux/extraction paquets : %s\n", argv[1]);
		return 1;
	}
	const media::NkMediaTrack *au = info.FirstAudio();
	printf("Clip : %s | %s | codec=%s | %d paquets\n", argv[1], info.ContainerName(),
		   au ? au->codec.CStr() : "?", (int)packets.Size());
	if (!au || au->codec != NkString("opus")) {
		printf("[ERREUR] ce harnais ne gère que l'Opus (CELT). Codec = %s\n", au ? au->codec.CStr() : "?");
		return 1;
	}

	// 2) Décodage NKMedia : chaque paquet → TOC → trames → NkCeltDecoder.
	media::NkCeltDecoder decoder;
	decoder.Init(1);
	NkVector<float32> myPcm;
	int32 nFramesTot = 0;
	for (uint64 p = 0; p < packets.Size(); ++p) {
		const uint8 *pkt = bytes.Data() + packets[p].offset;
		const usize plen = packets[p].size;
		media::NkOpusPacketInfo op;
		if (!media::NkOpusPacket::Parse(pkt, plen, op))
			continue;
		const int32 LM = LmFromMs(op.frameSizeMs);
		const int32 N = (1 << LM) * 120;
		for (int32 f = 0; f < op.frameCount; ++f) {
			const uint8 *fd = pkt + op.frames[f].offset;
			const int32 fl = (int32)op.frames[f].size;
			NkVector<float32> frame;
			frame.Resize((uint64)N);
			media::NkCeltDecoder::NkFrameFlags flags;
			decoder.DecodeFrame(fd, fl, LM, frame.Data(), &flags);
			for (int32 i = 0; i < N; ++i)
				myPcm.PushBack(frame[(uint64)i]);
			++nFramesTot;
		}
	}
	printf("Décodé NKMedia : %d trames, %d échantillons (48 kHz mono)\n", nFramesTot, (int)myPcm.Size());

	// 3) Référence ffmpeg (WAV float 48 kHz).
	audio::AudioSample ref = audio::AudioLoader::Load(argv[2]);
	if (!ref.IsValid()) {
		printf("[ERREUR] chargement référence : %s\n", argv[2]);
		return 1;
	}
	printf("Référence : %d Hz | %d canal | %d frames\n", ref.sampleRate, ref.channels, (int)ref.frameCount);

	// 4) Comparaison sur la longueur commune (canal 0).
	const int32 refCh = ref.channels > 0 ? ref.channels : 1;
	const int32 nRef = (int32)ref.frameCount;
	const int32 nMy = (int32)myPcm.Size();
	const int32 n = nRef < nMy ? nRef : nMy;
	double se = 0.0, er = 0.0, em = 0.0, dot = 0.0, maxErr = 0.0;
	for (int32 i = 0; i < n; ++i) {
		const float32 r = ref.data[(uint64)i * (uint64)refCh];
		const float32 m = myPcm[(uint64)i];
		const double d = (double)m - (double)r;
		se += d * d;
		er += (double)r * (double)r;
		em += (double)m * (double)m;
		dot += (double)r * (double)m;
		if (d < 0)
			;
		const double ad = d < 0 ? -d : d;
		if (ad > maxErr)
			maxErr = ad;
	}
	const double rmsErr = n > 0 ? ::sqrt(se / n) : 0.0;
	const double rmsRef = n > 0 ? ::sqrt(er / n) : 0.0;
	const double rmsMy = n > 0 ? ::sqrt(em / n) : 0.0;
	const double corr = (er > 0 && em > 0) ? dot / (::sqrt(er) * ::sqrt(em)) : 0.0;
	audio::AudioLoader::Free(ref);

	printf("\n=== COMPARAISON (n=%d) ===\n", n);
	printf("  RMS reference : %.5f\n", rmsRef);
	printf("  RMS NKMedia   : %.5f\n", rmsMy);
	printf("  RMS erreur    : %.5f  (rel = %.1f%%)\n", rmsErr, rmsRef > 0 ? 100.0 * rmsErr / rmsRef : 0.0);
	printf("  correlation   : %.4f  (1 = identique)\n", corr);
	printf("  erreur max    : %.5f\n", maxErr);
	printf("\n(Le decodeur non-silence est en cours : erreur elevee = attendu. Ce harnais sert a la faire tomber.)\n");
	return 0;
}
