// =============================================================================
// NKTTSTrain — chargeur LJSpeech + (à venir) entraînement TTS acoustique.
//   Étape 1 (ce fichier) : PROUVER le pipeline de données.
//     metadata.csv (texte↔WAV) → lecteur WAV PCM16 → mel-spectrogramme (NkAudioFeatures).
//   Usage : NKTTSTrain.exe [<dossier LJSpeech-1.1>]   (défaut = VoiceDatasets/…)
// =============================================================================
#include "NKSpeech/NkAudioFeatures.h"
#include "NKSpeech/NkGriffinLim.h"

// --- Etape 3b : modele acoustique appris (Transformer texte -> spectrogramme) ---
#include "NKTensor/NkTensor.h"
#include "NKTensor/NkTensorGpu.h"
#include "NKAutograd/NkVar.h"
#include "NKNN/NkDense.h"
#include "NKNN/NkTransformer.h"
#include "NKOptim/NkOptim.h"

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cmath>

using namespace nkentseu;
using namespace nkentseu::ai;

// Lecteur WAV MINIMAL : RIFF/WAVE PCM16 → float mono [-1,1] + sampleRate.
static bool ReadWavMono(const char *path, NkVector<float32> &out, int32 &sampleRate) {
	FILE *f = fopen(path, "rb");
	if (!f)
		return false;
	auto u32 = [&]() { unsigned char b[4]; if (fread(b, 1, 4, f) != 4) return 0u; return (unsigned)b[0] | (b[1] << 8) | (b[2] << 16) | ((unsigned)b[3] << 24); };
	auto u16 = [&]() { unsigned char b[2]; if (fread(b, 1, 2, f) != 2) return 0u; return (unsigned)b[0] | (b[1] << 8); };
	char riff[4];
	if (fread(riff, 1, 4, f) != 4 || memcmp(riff, "RIFF", 4) != 0) { fclose(f); return false; }
	u32(); // taille
	char wave[4];
	if (fread(wave, 1, 4, f) != 4 || memcmp(wave, "WAVE", 4) != 0) { fclose(f); return false; }

	int32 channels = 1, bits = 16;
	sampleRate = 22050;
	bool haveFmt = false;
	// Parcourt les chunks jusqu'à "data".
	for (;;) {
		char id[4];
		if (fread(id, 1, 4, f) != 4) { fclose(f); return false; }
		unsigned sz = u32();
		if (memcmp(id, "fmt ", 4) == 0) {
			u16();							 // audioFormat (1 = PCM)
			channels = (int32)u16();
			sampleRate = (int32)u32();
			u32();							 // byteRate
			u16();							 // blockAlign
			bits = (int32)u16();
			haveFmt = true;
			if (sz > 16)
				fseek(f, (long)(sz - 16), SEEK_CUR);
		} else if (memcmp(id, "data", 4) == 0) {
			if (!haveFmt || bits != 16) { fclose(f); return false; } // on ne gère que PCM16
			const unsigned nSamplesTotal = sz / 2;			// int16
			const unsigned frames = nSamplesTotal / (unsigned)(channels > 0 ? channels : 1);
			out.Resize((uint64)frames);
			for (unsigned i = 0; i < frames; ++i) {
				int32 acc = 0;
				for (int32 c = 0; c < channels; ++c) {
					unsigned char b[2];
					if (fread(b, 1, 2, f) != 2) { out.Resize((uint64)i); fclose(f); return i > 0; }
					acc += (int16)(b[0] | (b[1] << 8));
				}
				out[(uint64)i] = (float32)acc / (float32)(channels * 32768);
			}
			fclose(f);
			return true;
		} else {
			fseek(f, (long)sz, SEEK_CUR); // chunk inconnu → saute
		}
	}
}

// Écrivain WAV MINIMAL : float mono [-1,1] → RIFF/WAVE PCM16.
static bool WriteWavMono(const char *path, const float32 *mono, int32 samples, int32 sampleRate) {
	FILE *f = fopen(path, "wb");
	if (!f)
		return false;
	auto w32 = [&](unsigned v) { unsigned char b[4] = { (unsigned char)v, (unsigned char)(v >> 8), (unsigned char)(v >> 16), (unsigned char)(v >> 24) }; fwrite(b, 1, 4, f); };
	auto w16 = [&](unsigned v) { unsigned char b[2] = { (unsigned char)v, (unsigned char)(v >> 8) }; fwrite(b, 1, 2, f); };
	const unsigned dataBytes = (unsigned)samples * 2u;
	fwrite("RIFF", 1, 4, f);
	w32(36u + dataBytes);
	fwrite("WAVE", 1, 4, f);
	fwrite("fmt ", 1, 4, f);
	w32(16u);						 // taille sous-chunk fmt
	w16(1u);						 // PCM
	w16(1u);						 // mono
	w32((unsigned)sampleRate);		 // sampleRate
	w32((unsigned)sampleRate * 2u);	 // byteRate (mono 16 bit)
	w16(2u);						 // blockAlign
	w16(16u);						 // bitsPerSample
	fwrite("data", 1, 4, f);
	w32(dataBytes);
	for (int32 i = 0; i < samples; ++i) {
		float32 s = mono[i];
		if (s > 1.0f) s = 1.0f;
		if (s < -1.0f) s = -1.0f;
		int32 v = (int32)(s * 32767.0f);
		w16((unsigned)(int16)v);
	}
	fclose(f);
	return true;
}

// Reconstruit une onde depuis une magnitude (FGLA), la rend ECOUTABLE (RMS + soft-clip tanh)
// et l'ecrit en WAV. Facteur commun etape 2 / inference etape 3b.
static void ReconNormalizeWrite(const NkMagSpectrogram &mag, const NkGriffinLimConfig &gl, int32 sr,
								const char *path) {
	NkVector<float32> recon = NkGriffinLim::Reconstruct(mag, gl);
	double rms = 0.0;
	for (uint64 i = 0; i < recon.Size(); ++i)
		rms += (double)recon[i] * recon[i];
	rms = (recon.Size() > 0) ? sqrt(rms / (double)recon.Size()) : 0.0;
	double gain = (rms > 1e-9) ? (0.20 / rms) : 1.0;
	for (uint64 i = 0; i < recon.Size(); ++i)
		recon[i] = (float32)tanh((double)recon[i] * gain);
	bool w = WriteWavMono(path, recon.Data(), (int32)recon.Size(), sr);
	printf("  ecrit : %s (%s, %.2fs)\n", path, w ? "OK" : "KO", (double)recon.Size() / (double)sr);
}

// =============================================================================
// ETAPE 3b — MODELE ACOUSTIQUE APPRIS (Transformer texte -> spectrogramme magnitude).
//   Embedding caracteres + positions -> blocs Transformer (encodeur texte) -> upsampling
//   a debit fixe (matmul par matrice de repetition) -> tete magnitude. Cible = log(1+|STFT|)
//   du vrai clip (re-echantillonnee a T_texte*R). Perte MSE, optimiseur Adam. Inference :
//   magnitude predite -> FGLA (etape 2) -> WAV = la voix de la locutrice.
//   ⚠️ Debit FIXE (pas d'alignement appris) : le RYTHME sera approximatif ; c'est le 1er
//   modele appris (petite echelle), on prouve la chaine texte->onde et on entend la voix.
// =============================================================================
namespace {

// Vocabulaire caractere->id construit a la volee (ASCII minuscule).
struct NkCharVocab {
		int32 map[128];
		char inv[128];
		int32 size = 0;
		NkCharVocab() {
			for (int32 i = 0; i < 128; ++i) {
				map[i] = -1;
				inv[i] = 0;
			}
		}
		int32 Get(char c) {
			unsigned uc = (unsigned char)c;
			if (uc >= 128)
				return -1;
			if (uc >= 'A' && uc <= 'Z')
				uc = uc - 'A' + 'a';
			if (map[uc] < 0) {
				map[uc] = size;
				inv[size] = (char)uc;
				++size;
			}
			return map[uc];
		}
};

struct NkClip {
		NkVector<int32> ids;	// sequence de char-ids
		NkVector<float32> spec; // log(1+|STFT|), frames*bins row-major
		int32 frames = 0, bins = 0;
		char id[32] = {0};
		char text[256] = {0};
};

static int32 EnvI(const char *k, int32 def) {
	const char *e = getenv(k);
	return e ? atoi(e) : def;
}
static float32 EnvF(const char *k, float32 def) {
	const char *e = getenv(k);
	return e ? (float32)atof(e) : def;
}

} // namespace

static int RunTraining(const char *dir) {
	using namespace nkentseu::ai;
	NkTensorGpu &gpu = NkTensorGpu::Get();
	printf("=== NKTTSTrain — ETAPE 3b : modele acoustique appris (texte -> voix) ===\n");
	printf("  GPU compute : %s (%s)\n", gpu.IsAvailable() ? "OUI" : "NON", gpu.BackendName());

	// ---- Hyperparametres (env) ----
	const int32 N = EnvI("NK_TTS_N", 1);		 // nb de clips (1 = surajuste 1 phrase)
	const int32 steps = EnvI("NK_TTS_STEPS", 600);
	const int32 d = EnvI("NK_TTS_D", 128);
	const int32 H = EnvI("NK_TTS_H", 4);
	const int32 L = EnvI("NK_TTS_L", 3);
	const int32 R = EnvI("NK_TTS_R", 8);		 // trames de spectro par caractere (debit fixe)
	const float32 lr = EnvF("NK_TTS_LR", 3e-4f);

	NkGriffinLimConfig gl;
	gl.fftSize = 1024;
	gl.hopSize = 256;
	gl.iterations = 150;
	const int32 BINS = gl.fftSize / 2 + 1; // 513

	// ---- Chargement des clips (texte -> ids, audio -> log-magnitude) ----
	char metaPath[1024];
	snprintf(metaPath, sizeof(metaPath), "%s/metadata.csv", dir);
	FILE *meta = fopen(metaPath, "r");
	if (!meta) {
		printf("[ERREUR] metadata.csv introuvable : %s\n", metaPath);
		return 1;
	}
	NkCharVocab vocab;
	NkVector<NkClip> clips;
	int32 maxT = 8;
	char line[8192];
	while (fgets(line, sizeof(line), meta) && (int32)clips.Size() < N) {
		char *p1 = strchr(line, '|');
		if (!p1)
			continue;
		*p1 = '\0';
		const char *cid = line;
		char *p2 = strchr(p1 + 1, '|');
		const char *text = (p2 ? p2 + 1 : p1 + 1);
		char textBuf[256];
		snprintf(textBuf, sizeof(textBuf), "%s", text);
		for (char *c = textBuf; *c; ++c)
			if (*c == '\n' || *c == '\r') {
				*c = '\0';
				break;
			}
		char wavPath[1200];
		snprintf(wavPath, sizeof(wavPath), "%s/wavs/%s.wav", dir, cid);
		NkVector<float32> audio;
		int32 sr = 0;
		if (!ReadWavMono(wavPath, audio, sr))
			continue;

		NkClip clip;
		snprintf(clip.id, sizeof(clip.id), "%s", cid);
		snprintf(clip.text, sizeof(clip.text), "%s", textBuf);
		for (const char *c = textBuf; *c; ++c) {
			int32 id = vocab.Get(*c);
			if (id >= 0)
				clip.ids.PushBack(id);
		}
		if ((int32)clip.ids.Size() < 2 || (int32)clip.ids.Size() > 220)
			continue;
		if ((int32)clip.ids.Size() > maxT)
			maxT = (int32)clip.ids.Size();

		NkMagSpectrogram m = NkGriffinLim::Magnitude(audio.Data(), (int32)audio.Size(), gl);
		clip.frames = m.frames;
		clip.bins = m.bins;
		clip.spec.Resize((uint64)m.frames * (uint64)m.bins);
		for (uint64 i = 0; i < clip.spec.Size(); ++i)
			clip.spec[i] = (float32)log(1.0 + (double)m.data[i]);
		clips.PushBack(clip);
		printf("  clip[%d] %s : %d car -> %d trames | \"%.50s\"\n", (int)clips.Size() - 1, clip.id,
			   (int)clip.ids.Size(), clip.frames, clip.text);
	}
	fclose(meta);
	if (clips.Size() == 0) {
		printf("[ERREUR] aucun clip charge.\n");
		return 1;
	}
	printf("  vocab = %d caracteres, %d clips, maxT=%d, d=%d L=%d H=%d R=%d\n", vocab.size,
		   (int)clips.Size(), maxT, d, L, H, R);

	// ---- Construction du modele ----
	const uint32 V = (uint32)vocab.size;
	NkVar tokEmb = NkVar::Leaf(nn::RandnTensor(NkShape{(int64)V, (int64)d}, 0.02, 11u), true);
	NkVar posEmb = NkVar::Leaf(nn::RandnTensor(NkShape{(int64)(maxT + 1), (int64)d}, 0.02, 12u), true);
	NkVector<nn::NkTransformerBlock> blocks;
	for (int32 l = 0; l < L; ++l)
		blocks.PushBack(nn::NkTransformerBlock((uint32)d, (uint32)H, 100u + (uint32)l * 17u));
	nn::NkLayerNorm lnf((uint32)d);
	nn::NkDense fc1((uint32)d, (uint32)d, 201u);
	nn::NkDense fc2((uint32)d, (uint32)BINS, 202u); // tete magnitude (log)

	NkVector<NkVar> params;
	params.PushBack(tokEmb);
	params.PushBack(posEmb);
	for (uint32 l = 0; l < blocks.Size(); ++l)
		blocks[l].Parameters(params);
	lnf.Parameters(params);
	fc1.Parameters(params);
	fc2.Parameters(params);
	printf("  %d tenseurs de parametres.\n", (int)params.Size());

	optim::NkAdam adam(params, lr);

	// ---- Passe AVANT (texte -> spectrogramme predit) depuis des char-ids quelconques ----
	// Sert a la fois a l'entrainement (avec cible) et a l'inference « dis ce texte ».
	auto forwardIds = [&](const NkVector<int32> &ids) -> NkVar {
		const int32 T = (int32)ids.Size();
		const int32 Tout = T * R;
		NkTensor idsT = NkTensor::Zeros(NkShape{(int64)1, (int64)T});
		NkTensor posT = NkTensor::Zeros(NkShape{(int64)1, (int64)T});
		{
			float *ip = idsT.DataAs<float>();
			float *pp = posT.DataAs<float>();
			for (int32 t = 0; t < T; ++t) {
				ip[t] = (float)ids[(uint64)t];
				pp[t] = (float)t;
			}
		}
		NkVar te = autograd::Embedding(tokEmb, idsT);
		NkVar pe = autograd::Embedding(posEmb, posT);
		NkVar x = autograd::Add(te, pe); // [1,T,d]
		for (uint32 l = 0; l < blocks.Size(); ++l)
			x = blocks[l].Forward(x);
		x = lnf.Forward(x);
		NkVar xf = autograd::Reshape(x, NkShape{(int64)T, (int64)d});
		// Matrice d'upsampling a debit fixe U [Tout,T] : U[i, i/R] = 1.
		NkTensor U = NkTensor::Zeros(NkShape{(int64)Tout, (int64)T});
		{
			float *upp = U.DataAs<float>();
			for (int32 i = 0; i < Tout; ++i)
				upp[i * T + (i / R)] = 1.0f;
		}
		NkVar Uv = NkVar::Leaf(U, false);
		NkVar up = autograd::Matmul(Uv, xf);		 // [Tout,d]
		NkVar hid = autograd::Gelu(fc1.Forward(up)); // [Tout,d]
		return fc2.Forward(hid);					 // [Tout,BINS] (log-magnitude predite)
	};

	// Cible d'un clip re-echantillonnee a [Tout,BINS] (debit fixe).
	auto targetOf = [&](const NkClip &clip) -> NkVar {
		const int32 Tout = (int32)clip.ids.Size() * R;
		NkTensor tgt = NkTensor::Zeros(NkShape{(int64)Tout, (int64)BINS});
		float *tp = tgt.DataAs<float>();
		for (int32 i = 0; i < Tout; ++i) {
			int32 sf = (int32)((int64)i * clip.frames / (Tout > 0 ? Tout : 1));
			if (sf >= clip.frames)
				sf = clip.frames - 1;
			for (int32 k = 0; k < BINS; ++k)
				tp[i * BINS + k] = clip.spec[(uint64)sf * clip.bins + k];
		}
		return NkVar::Leaf(tgt, false);
	};

	// Synthetise depuis des char-ids : forward -> magnitude -> FGLA -> WAV.
	auto synthToWav = [&](const NkVector<int32> &ids, const char *path) {
		NkVar pred = forwardIds(ids);
		NkTensor pcpu = pred.Value().ToCPU();
		const float *pp = pcpu.DataAs<float>();
		const int32 Tout = (int32)ids.Size() * R;
		NkMagSpectrogram pm;
		pm.frames = Tout;
		pm.bins = BINS;
		pm.data.Resize((uint64)Tout * (uint64)BINS);
		for (uint64 i = 0; i < pm.data.Size(); ++i) {
			double mg = exp((double)pp[i]) - 1.0;
			pm.data[i] = (float32)(mg > 0.0 ? mg : 0.0);
		}
		ReconNormalizeWrite(pm, gl, 22050, path);
	};

	// ---- Boucle d'entrainement ----
	double ema = 0.0;
	for (int32 step = 0; step < steps; ++step) {
		const NkClip &clip = clips[(uint64)(step % (int32)clips.Size())];
		NkVar pred = forwardIds(clip.ids);
		NkVar loss = autograd::MSE(pred, targetOf(clip));
		adam.ZeroGrad();
		loss.Backward();
		adam.Step();
		double lv = loss.Value().ToCPU().GetItem(NkShape{(int64)0});
		ema = (step == 0) ? lv : (0.98 * ema + 0.02 * lv);
		if (step % 50 == 0 || step == steps - 1)
			printf("  step %4d/%d  loss=%.5f  ema=%.5f\n", step, steps, lv, ema);
	}

	// ---- Inference : reproduit les clips d'entrainement (texte -> voix apprise) ----
	printf("\n  --- Inference : les phrases APPRISES (max 4 fichiers) ---\n");
	int32 nOut = (int32)clips.Size();
	if (nOut > 4)
		nOut = 4;
	for (int32 c = 0; c < nOut; ++c) {
		const NkClip &clip = clips[(uint64)c];
		char outPath[128];
		snprintf(outPath, sizeof(outPath), "nktts_learned_%02d.wav", c);
		printf("  [%s] \"%.50s\"\n", clip.id, clip.text);
		synthToWav(clip.ids, outPath);
	}

	// ---- Mode « dis ce texte » : NK_TTS_SAY="ta phrase" -> nktts_say.wav ----
	const char *sayText = getenv("NK_TTS_SAY");
	if (sayText && sayText[0]) {
		NkVector<int32> ids;
		int32 unknown = 0;
		for (const char *c = sayText; *c; ++c) {
			unsigned uc = (unsigned char)*c;
			if (uc >= 128)
				continue;
			if (uc >= 'A' && uc <= 'Z')
				uc = uc - 'A' + 'a';
			int32 id = (uc < 128) ? vocab.map[uc] : -1; // vocab FIGE (pas de nouveaux ids en inference)
			if (id >= 0)
				ids.PushBack(id);
			else
				++unknown;
		}
		printf("\n  --- « dis ce texte » : \"%.60s\" (%d car connus, %d inconnus ignores) ---\n",
			   sayText, (int)ids.Size(), unknown);
		if (ids.Size() >= 2)
			synthToWav(ids, "nktts_say.wav");
		else
			printf("  [KO] trop peu de caracteres connus (le modele n'a vu que %d caracteres a l'entrainement)\n",
				   vocab.size);
	}

	gpu.Shutdown();
	printf("\n=== Modele acoustique appris : entraine (ema=%.5f), voix ecrite ===\n", ema);
	return 0;
}

int main(int argc, char **argv) {
	// Mode entrainement (etape 3b) : NKTTSTrain.exe --train [<dir>]
	bool train = false;
	const char *dirArg = "D:/Projets/2026/Nkentseu/VoiceDatasets/LJSpeech/LJSpeech-1.1";
	for (int i = 1; i < argc; ++i) {
		if (strcmp(argv[i], "--train") == 0)
			train = true;
		else
			dirArg = argv[i];
	}
	if (train)
		return RunTraining(dirArg);

	printf("=== NKTTSTrain — chargeur LJSpeech (etape 1 : pipeline de donnees) ===\n\n");

	const char *dir = dirArg;
	char metaPath[1024];
	snprintf(metaPath, sizeof(metaPath), "%s/metadata.csv", dir);
	FILE *meta = fopen(metaPath, "r");
	if (!meta) {
		printf("[ERREUR] metadata.csv introuvable : %s\n", metaPath);
		printf("         (le dataset est-il extrait ? passe le dossier LJSpeech-1.1 en argument)\n");
		return 1;
	}

	NkAudioFeatureConfig cfg;
	cfg.sampleRate = 22050; // LJSpeech
	cfg.melBands = 80;		// standard TTS
	cfg.useDeltas = false;

	int32 shown = 0, ok = 0;
	int64 totalFrames = 0;
	NkVector<float32> firstAudio; // premier clip retenu pour le round-trip vocodeur
	int32 firstSr = 22050;
	char firstId[64] = { 0 };
	char line[8192];
	while (fgets(line, sizeof(line), meta) && shown < 5) {
		// Format : "LJ001-0001|Transcription brute|Transcription normalisee"
		char *p1 = strchr(line, '|');
		if (!p1)
			continue;
		*p1 = '\0';
		const char *id = line;
		char *p2 = strchr(p1 + 1, '|');
		const char *text = (p2 ? p2 + 1 : p1 + 1);
		// retire le \n final du texte
		char textBuf[4096];
		snprintf(textBuf, sizeof(textBuf), "%s", text);
		for (char *c = textBuf; *c; ++c)
			if (*c == '\n' || *c == '\r') { *c = '\0'; break; }

		char wavPath[1200];
		snprintf(wavPath, sizeof(wavPath), "%s/wavs/%s.wav", dir, id);
		NkVector<float32> audio;
		int32 sr = 0;
		++shown;
		if (!ReadWavMono(wavPath, audio, sr)) {
			printf("  [%s] WAV illisible (%s)\n", id, wavPath);
			continue;
		}
		NkFeatureMatrix mel = NkAudioFeatures::LogMelSpectrogram(audio.Data(), (int32)audio.Size(), cfg);
		++ok;
		totalFrames += mel.frames;
		if (firstAudio.Size() == 0) {
			firstAudio = audio;
			firstSr = sr;
			snprintf(firstId, sizeof(firstId), "%s", id);
		}
		printf("  [%s] %.2fs @ %d Hz -> mel %d trames x %d bandes | texte(%d) : \"%.60s%s\"\n",
			   id, (double)audio.Size() / (double)sr, sr, mel.frames, mel.dims, (int)strlen(textBuf), textBuf,
			   strlen(textBuf) > 60 ? "..." : "");
	}
	fclose(meta);

	printf("\n  [ %s ] %d/%d echantillons charges (WAV -> mel-spectrogramme)\n", (ok == shown && ok > 0) ? "OK " : "KO",
		   ok, shown);

	// -------------------------------------------------------------------------
	// ETAPE 2 — VOCODEUR sur VRAIE VOIX : clip reel -> STFT magnitude -> Griffin-Lim
	//   -> onde re-synthetisee. Valide la moitie « vocodeur » du TTS sur voix humaine
	//   (aucun modele appris ici : c'est la reconstruction de phase pure). Ecrit 2 WAV
	//   (original + reconstruit) pour une ecoute A/B.
	// -------------------------------------------------------------------------
	if (firstAudio.Size() > 0) {
		printf("\n=== Etape 2 — vocodeur Griffin-Lim sur vraie voix [%s] ===\n", firstId);
		NkGriffinLimConfig gl;
		gl.fftSize = 1024;
		gl.hopSize = 256;
		gl.iterations = 150;

		NkMagSpectrogram mag = NkGriffinLim::Magnitude(firstAudio.Data(), (int32)firstAudio.Size(), gl);
		printf("  STFT magnitude : %d trames x %d bins (fft=%d, hop=%d)\n", mag.frames, mag.bins, gl.fftSize, gl.hopSize);

		NkVector<float32> recon = NkGriffinLim::Reconstruct(mag, gl);

		// JUSTESSE DU CONTENU = erreur sur la MAGNITUDE reconstruite (le buzz de phase
		// Griffin-Lim gonfle l'energie mais NE change PAS le contenu spectral = la voix).
		// On recalcule |STFT| de la reconstruction et on compare a la cible (trames internes).
		{
			NkMagSpectrogram magR = NkGriffinLim::Magnitude(recon.Data(), (int32)recon.Size(), gl);
			int32 mf = (magR.frames < mag.frames) ? magR.frames : mag.frames;
			double num = 0.0, den2 = 0.0;
			for (int32 f = 2; f < mf - 2; ++f) // saute les bords peu recouverts
				for (int32 k = 0; k < mag.bins; ++k) {
					double a = mag.data[(nk_size)f * mag.bins + k];
					double b = magR.data[(nk_size)f * magR.bins + k];
					num += (a - b) * (a - b);
					den2 += a * a;
				}
			double magErr = (den2 > 0.0) ? sqrt(num / den2) : 1.0;
			printf("  erreur magnitude reconstruite = %.2f%% (contenu vocal ; <~15%% = voix correcte)\n", magErr * 100.0);
		}

		// Energie relative (original vs reconstruit) sur la longueur commune.
		int32 n = (int32)((recon.Size() < firstAudio.Size()) ? recon.Size() : firstAudio.Size());
		double eOrig = 0.0, eRec = 0.0;
		for (int32 i = 0; i < n; ++i) {
			eOrig += (double)firstAudio[(uint64)i] * firstAudio[(uint64)i];
			eRec += (double)recon[(uint64)i] * recon[(uint64)i];
		}
		double ratio = (eOrig > 0.0) ? (eRec / eOrig) : 0.0;
		printf("  reconstruit : %d echantillons (%.2fs), energie brute ratio rec/orig = %.4f\n",
			   (int)recon.Size(), (double)recon.Size() / (double)firstSr, ratio);

		// Rendu ECOUTABLE : normalise en RMS a une cible confortable, puis SOFT-CLIP tanh
		// (ecrase les pics transitoires du buzz Griffin-Lim SANS ecraser le corps de la voix).
		double rms = 0.0;
		for (uint64 i = 0; i < recon.Size(); ++i)
			rms += (double)recon[i] * recon[i];
		rms = (recon.Size() > 0) ? sqrt(rms / (double)recon.Size()) : 0.0;
		const double targetRms = 0.20;
		double gain = (rms > 1e-9) ? (targetRms / rms) : 1.0;
		for (uint64 i = 0; i < recon.Size(); ++i) {
			double v = (double)recon[i] * gain;
			recon[i] = (float32)tanh(v); // corps ~lineaire (tanh(0.2)~0.197), pics -> ~±1
		}
		printf("  rendu ecoutable : RMS %.4f -> cible %.2f (gain %.2f) + soft-clip tanh\n", rms, targetRms, gain);

		bool w1 = WriteWavMono("nktts_ljspeech_orig.wav", firstAudio.Data(), (int32)firstAudio.Size(), firstSr);
		bool w2 = WriteWavMono("nktts_ljspeech_recon.wav", recon.Data(), (int32)recon.Size(), firstSr);
		printf("  ecrit : nktts_ljspeech_orig.wav (%s) + nktts_ljspeech_recon.wav (%s)\n",
			   w1 ? "OK" : "KO", w2 ? "OK" : "KO");
		printf("  -> ECOUTE A/B : la reconstruction doit sonner comme la vraie locutrice (voix, prosodie).\n");
	}

	printf("\n=== Pipeline de donnees LJSpeech %s ===\n", (ok > 0) ? "OPERATIONNEL" : "EN ECHEC");
	return (ok > 0) ? 0 : 1;
}
