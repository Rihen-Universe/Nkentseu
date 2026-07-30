// =============================================================================
// NkVoiceLoopDemo — boucle voix bout-en-bout (Phase 8, brique 6 "Boucle voix").
// -----------------------------------------------------------------------------
// Cable les briques DEJA EXISTANTES en une boucle reelle :
//   entree audio -> NkDenoiser (debruitage REEL) -> NkASRModel (ASR) -> logique
//   de commande -> NkVoiceSynth (TTS) -> sortie WAV (NKAudio/NKFileSystem).
//
// ⚠️ LIMITE HONNETE (documentee des le depart, pas dissimulee) : ce depot n'a
// PAS de reconnaissance vocale a vocabulaire ouvert (Whisper-like) — NkASRModel
// (NKSpeech/NkAsrModel.h) est un modele BiGRU+CTC PEDAGOGIQUE entraine en
// quelques secondes sur un TRES PETIT vocabulaire de "mots" synthetiques
// (suites de tons, cf. NKASRTest). Il ne peut donc PAS transcrire une vraie
// phrase humaine arbitraire. Deux choses sont demontrees ICI, l'une a cote de
// l'autre, pour ne RIEN fabriquer :
//   1) La boucle COMPLETE (capture -> debruitage -> ASR -> logique -> TTS ->
//      sortie) est cablee et executee sur un FICHIER AUDIO REEL deja present
//      dans le depot (`ma_voix.wav`, une vraie capture micro d'une session
//      NkMicRecord anterieure — substitut honnete a un micro temps reel : cet
//      environnement d'execution n'a PAS d'acces materiel a un microphone,
//      donc AUCUNE capture live n'est tentee ni pretendue ici). Le debruitage
//      (NkDenoiser) et l'extraction MFCC (NkAudioFeatures) operent sur de
//      VRAIES donnees micro. Le decodage ASR qui suit est explicitement
//      etiquete "NON SIGNIFICATIF" dans la sortie (le modele n'a jamais appris
//      cette voix) : on prouve le CABLAGE, pas une reconnaissance.
//   2) Le chemin de SUCCES (reconnaissance correcte -> reponse parlee correcte)
//      est demontre sur de l'audio SYNTHETIQUE dont le contenu EST dans le
//      vocabulaire appris (meme technique que NKASRTest : mots = suites de
//      tons), pour prouver que la logique de commande + le TTS fonctionnent
//      quand l'ASR reconnait correctement.
//
// Sortie NKAudio : la reponse parlee est ecrite en WAV (NkFile) — la lecture
// audio temps reel via un device NKAudio est deja prouvee separement par
// NkAudioPlayer/NkAudioDemo (composants existants) ; elle n'est PAS invoquee
// ici pour garder ce test non-interactif et bloquant (bonne pratique projet).
// =============================================================================
#include "NKAudio/NkAudio.h"
#include "NKAudio/NkDenoiser.h"
#include "NKFileSystem/NkFile.h"
#include "NKSpeech/NkAudioFeatures.h"
#include "NKSpeech/NkAsrModel.h"
#include "NKSpeech/NkVoiceSynth.h"
#include "NKTrain/NkTrain.h"
#include "NKOptim/NkOptim.h"
#include "NKAutograd/NkVar.h"
#include "NKTensor/NkTensor.h"

#include <cstdio>
#include <cmath>

using namespace nkentseu;
using namespace nkentseu::ai;

// --- Utilitaires locaux (WAV out, identiques en esprit a NKSpeechTest/NkMicRecord) ---

static void WriteWavPcm16(const char *path, const float *samples, int n, int sampleRate) {
	FILE *fp = fopen(path, "wb");
	if (!fp)
		return;
	auto u32 = [&](unsigned v) { fputc(v & 255, fp); fputc((v >> 8) & 255, fp); fputc((v >> 16) & 255, fp); fputc((v >> 24) & 255, fp); };
	auto u16 = [&](unsigned v) { fputc(v & 255, fp); fputc((v >> 8) & 255, fp); };
	const int dataBytes = n * 2;
	fwrite("RIFF", 1, 4, fp); u32(36 + dataBytes); fwrite("WAVE", 1, 4, fp);
	fwrite("fmt ", 1, 4, fp); u32(16); u16(1); u16(1);
	u32(sampleRate); u32(sampleRate * 2); u16(2); u16(16);
	fwrite("data", 1, 4, fp); u32(dataBytes);
	for (int i = 0; i < n; ++i) {
		float v = samples[i];
		if (v > 1.0f) v = 1.0f;
		if (v < -1.0f) v = -1.0f;
		int s = (int)(v * 32767.0f);
		u16((unsigned)(s & 0xFFFF));
	}
	fclose(fp);
}

// Synthétise l'audio d'un "mot-commande" (suite de symboles = suite de tons),
// même technique que NKASRTest::Synthesize (dupliquée ici pour ne PAS créer de
// dépendance croisée entre applications — chaque app NKSpeech reste autonome).
static void SynthesizeCommand(const NkVector<int32> &word, int sr, int segMs, uint32 seed, const double *freqs,
							  int nFreqs, double noiseAmp, NkVector<float32> &out) {
	const int seg = sr * segMs / 1000;
	out.Clear();
	uint32 s = seed ? seed : 1u;
	for (uint32 i = 0; i < word.Size(); ++i) {
		const double f = freqs[word[i] % nFreqs];
		for (int n = 0; n < seg; ++n) {
			s = s * 1664525u + 1013904223u;
			const double noise = ((double)((s >> 9) & 0xFFFFu) / 65535.0 - 0.5) * noiseAmp;
			double env = 1.0;
			const int fade = seg / 8;
			if (n < fade)
				env = 0.5 * (1.0 - std::cos(3.14159265 * n / fade));
			else if (n > seg - fade)
				env = 0.5 * (1.0 - std::cos(3.14159265 * (seg - n) / fade));
			const double v = 0.6 * env * std::sin(2.0 * 3.14159265358979 * f * n / sr) + noise;
			out.PushBack((float32)v);
		}
	}
}

// MFCC standardisées (comme NKASRTest::FeaturesOf) -> NkVar [1,dims] par trame.
static void FeaturesOf(const NkVector<float32> &audio, int sr, NkVector<NkVar> &frames, int &dims) {
	NkAudioFeatureConfig cfg;
	cfg.sampleRate = sr;
	cfg.melBands = 26;
	cfg.mfccCount = 13;
	cfg.useDeltas = true;
	NkFeatureMatrix m = NkAudioFeatures::MFCC(audio.Data(), (int32)audio.Size(), cfg);
	dims = m.dims;
	const int F = m.frames, D = m.dims;
	NkVector<double> mean, var;
	mean.Resize((nk_size)D);
	var.Resize((nk_size)D);
	for (int d = 0; d < D; ++d) {
		double mu = 0;
		for (int f = 0; f < F; ++f)
			mu += m.data[(nk_size)(f * D + d)];
		mu /= (F > 0 ? F : 1);
		double vv = 0;
		for (int f = 0; f < F; ++f) {
			double e = m.data[(nk_size)(f * D + d)] - mu;
			vv += e * e;
		}
		mean[(nk_size)d] = mu;
		var[(nk_size)d] = vv / (F > 0 ? F : 1) + 1e-6;
	}
	frames.Clear();
	for (int f = 0; f < F; ++f) {
		NkTensor t = NkTensor::Zeros(NkShape{(int64)1, (int64)D});
		float *tp = t.DataAs<float>();
		for (int d = 0; d < D; ++d)
			tp[d] = (float)((m.data[(nk_size)(f * D + d)] - mean[(nk_size)d]) / std::sqrt(var[(nk_size)d]));
		frames.PushBack(NkVar::Leaf(t, false));
	}
}

static bool SameSeq(const NkVector<int32> &a, const NkVector<int32> &b) {
	if (a.Size() != b.Size())
		return false;
	for (uint32 i = 0; i < a.Size(); ++i)
		if (a[i] != b[i])
			return false;
	return true;
}

// "Logique" de commande : symboles reconnus -> reponse parlee (phrase phonétique
// au format NkVoiceSynth::Speak, cf. exemples déjà validés dans NKSpeechTest/
// NkVoiceSynth.h : "s a l u", "b o~ n j u r").
static const char *ResponseFor(const NkVector<int32> &decoded, const NkVector<int32> &cmdA, const NkVector<int32> &cmdB) {
	if (SameSeq(decoded, cmdA))
		return "s a l u"; // "salut" -> commande A reconnue
	if (SameSeq(decoded, cmdB))
		return "b o~ n j u r"; // "bonjour" -> commande B reconnue
	return "_"; // commande non reconnue -> silence (pas de reponse fabriquee)
}

int main(int argc, char **argv) {
	printf("=== NkVoiceLoopDemo : micro -> debruitage -> ASR -> logique -> TTS -> sortie ===\n\n");
	int nbOk = 0, nbTotal = 0;

	// ---------------------------------------------------------------------
	// ETAPE 0 — Entraîne le mini-modèle ASR (2 commandes, tons bien séparés,
	// même méthode que NKASRTest « cas facile » pour une convergence fiable).
	// ---------------------------------------------------------------------
	const int SR = 16000, SEGMS = 180, BLANK = 3, V = 4;
	const double kFreqs[3] = {300.0, 1000.0, 3000.0};
	NkVector<int32> cmdA, cmdB; // A = {0,1,2}, B = {2,1,0}
	{
		int32 a[3] = {0, 1, 2}, b[3] = {2, 1, 0};
		for (int i = 0; i < 3; ++i) {
			cmdA.PushBack(a[i]);
			cmdB.PushBack(b[i]);
		}
	}
	printf("-- Entrainement du mini-ASR de commandes (2 mots, BiGRU+CTC) --\n");
	NkVector<NkVar> featsA, featsB;
	int dims = 0;
	{
		NkVector<float32> audioA, audioB;
		SynthesizeCommand(cmdA, SR, SEGMS, 100u, kFreqs, 3, 0.05, audioA);
		SynthesizeCommand(cmdB, SR, SEGMS, 107u, kFreqs, 3, 0.05, audioB);
		FeaturesOf(audioA, SR, featsA, dims);
		FeaturesOf(audioB, SR, featsB, dims);
	}
	NkASRModel model(dims, 64, V, 777u);
	NkVector<NkVar> params;
	model.Parameters(params);
	optim::NkAdam adam(params, 0.002f);
	train::NkLRSchedule sched;
	sched.peakLr = 0.003f;
	sched.warmupSteps = 40;
	const int STEPS = 200;
	sched.totalSteps = STEPS * 2;
	sched.minLrRatio = 0.1;
	int64 gstep = 0;
	double lastLoss = 0.0;
	for (int step = 1; step <= STEPS; ++step) {
		NkVector<NkVar> *feats[2] = {&featsA, &featsB};
		NkVector<int32> *tgts[2] = {&cmdA, &cmdB};
		double epochLoss = 0;
		for (int k = 0; k < 2; ++k) {
			++gstep;
			adam.SetLearningRate(sched.LrAt(gstep));
			adam.ZeroGrad();
			NkVar logits = model.Forward(*feats[k]);
			NkVector<NkVector<int32>> tgt;
			tgt.PushBack(*tgts[k]);
			NkVar loss = autograd::CTCLoss(logits, tgt, BLANK);
			loss.Backward();
			adam.Step();
			epochLoss += loss.Value().GetItem(NkShape{(int64)0});
		}
		lastLoss = epochLoss * 0.5;
	}
	printf("   perte CTC finale = %.5f (200 pas, 2 commandes)\n", lastLoss);

	// Vérifie que le mini-ASR reconnaît bien ses 2 commandes (condition nécessaire
	// pour que le "chemin de succès" ci-dessous ait un sens).
	NkVector<int32> decA, decB;
	NkCTCBeamSearchDecode(model.Forward(featsA).Value(), BLANK, 5, decA);
	NkCTCBeamSearchDecode(model.Forward(featsB).Value(), BLANK, 5, decB);
	const bool asrTrained = SameSeq(decA, cmdA) && SameSeq(decB, cmdB);
	++nbTotal;
	nbOk += asrTrained ? 1 : 0;
	printf("  [ %s ] mini-ASR entraine reconnait ses 2 commandes (A={0,1,2}->OK=%d, B={2,1,0}->OK=%d)\n",
		   asrTrained ? "OK" : "KO", SameSeq(decA, cmdA), SameSeq(decB, cmdB));

	// ---------------------------------------------------------------------
	// CHEMIN 1 — boucle complète sur un FICHIER AUDIO REEL (vraie capture
	// micro, session NkMicRecord antérieure). Prouve capture-fichier + debruitage
	// + cablage complet sur de VRAIES données. Etiquette honnêtement l'ASR.
	// ---------------------------------------------------------------------
	const char *realWavPath = (argc > 1) ? argv[1] : "ma_voix.wav";
	printf("\n-- Chemin 1 (fichier audio REEL, substitut honnete au micro live) : %s --\n", realWavPath);
	{
		audio::AudioSample smp = audio::AudioLoader::Load(realWavPath);
		if (!smp.IsValid()) {
			printf("  [ATTENTION] fichier introuvable/illisible (%s) -- chemin 1 saute (fichier optionnel,\n"
				   "              relancer depuis la racine du depot ou passer un chemin en argv[1]).\n",
				   realWavPath);
		} else {
			printf("  charge : %llu frames @ %d Hz, %d canal(aux) (%.2fs) -- VRAIE capture micro\n",
				   (unsigned long long)smp.frameCount, smp.sampleRate, smp.channels, smp.GetDuration());

			// Débruitage RÉEL (NkDenoiser, même composant que NkMicRecord).
			NkVector<float32> clean;
			audio::NkDenoiseOptions denOpt;
			const bool denOk = audio::NkDenoiser::Process(smp.data, (int32)smp.frameCount, smp.channels,
														   smp.sampleRate, clean, denOpt);
			++nbTotal;
			nbOk += denOk ? 1 : 0;
			printf("  [ %s ] debruitage NkDenoiser (soustraction spectrale + gate + normalisation) sur audio REEL\n",
				   denOk ? "OK" : "KO");

			if (denOk && clean.Size() > 0) {
				// Vers MONO pour la suite (moyenne des canaux) si multi-canal.
				NkVector<float32> mono;
				if (smp.channels > 1) {
					const uint64 frames = clean.Size() / (uint64)smp.channels;
					mono.Resize(frames);
					for (uint64 f = 0; f < frames; ++f) {
						double acc = 0;
						for (int32 c = 0; c < smp.channels; ++c)
							acc += clean[f * (uint64)smp.channels + (uint64)c];
						mono[f] = (float32)(acc / smp.channels);
					}
				} else {
					mono = clean;
				}

				NkVector<NkVar> realFeats;
				int realDims = 0;
				FeaturesOf(mono, smp.sampleRate, realFeats, realDims);
				NkVector<int32> decReal;
				if (realDims == dims && realFeats.Size() > 0) {
					NkCTCBeamSearchDecode(model.Forward(realFeats).Value(), BLANK, 5, decReal);
				}
				printf("  ASR sur audio REEL -> symboles decodes {");
				for (uint32 i = 0; i < decReal.Size(); ++i)
					printf("%s%d", i ? "," : "", decReal[i]);
				printf("}\n");
				printf("  ⚠️  NON SIGNIFICATIF : le mini-ASR n'a appris QUE 2 tons synthetiques, PAS la voix\n"
					   "      humaine de ce fichier -- ce decodage sert a PROUVER LE CABLAGE, pas une vraie\n"
					   "      reconnaissance (limite honnete, documentee en tete de ce fichier).\n");

				const char *resp = ResponseFor(decReal, cmdA, cmdB);
				printf("  logique de commande -> reponse phonetique \"%s\" (silence '_' si commande non reconnue,\n"
					   "      attendu ici puisque le decodage ci-dessus n'est pas significatif)\n",
					   resp);
				NkVoiceSynthConfig vcfg = NkVoiceSynthConfig::Homme();
				NkVector<float32> wav = NkVoiceSynth::Speak(resp, vcfg);
				if (wav.Size() > 0) {
					WriteWavPcm16("nkvoiceloop_reponse_fichier_reel.wav", wav.Data(), (int)wav.Size(), vcfg.sampleRate);
					printf("  TTS -> nkvoiceloop_reponse_fichier_reel.wav ecrit (%d ech.)\n", (int)wav.Size());
				}
			}
			audio::AudioLoader::Free(smp);
		}
	}

	// ---------------------------------------------------------------------
	// CHEMIN 2 — chemin de SUCCES : audio SYNTHETIQUE dans le vocabulaire
	// appris -> ASR reconnait correctement -> logique -> TTS correct.
	// Prouve que logique+TTS fonctionnent quand l'ASR reconnait correctement.
	// ---------------------------------------------------------------------
	printf("\n-- Chemin 2 (audio SYNTHETIQUE, dans le vocabulaire appris) : commande B = {2,1,0} --\n");
	{
		NkVector<int32> decB2;
		NkCTCBeamSearchDecode(model.Forward(featsB).Value(), BLANK, 5, decB2);
		const bool recognized = SameSeq(decB2, cmdB);
		++nbTotal;
		nbOk += recognized ? 1 : 0;
		printf("  ASR -> symboles decodes {");
		for (uint32 i = 0; i < decB2.Size(); ++i)
			printf("%s%d", i ? "," : "", decB2[i]);
		printf("}  [ %s ] (cible {2,1,0})\n", recognized ? "OK" : "KO");

		const char *resp = ResponseFor(decB2, cmdA, cmdB);
		const bool respOk = NkString(resp) == NkString("b o~ n j u r");
		++nbTotal;
		nbOk += respOk ? 1 : 0;
		printf("  [ %s ] logique de commande -> reponse phonetique \"%s\" (attendu : \"bonjour\")\n",
			   respOk ? "OK" : "KO", resp);

		NkVoiceSynthConfig vcfg = NkVoiceSynthConfig::Femme();
		NkVector<float32> wav = NkVoiceSynth::Speak(resp, vcfg);
		const bool ttsOk = wav.Size() > 0;
		++nbTotal;
		nbOk += ttsOk ? 1 : 0;
		if (ttsOk)
			WriteWavPcm16("nkvoiceloop_reponse_synthetique.wav", wav.Data(), (int)wav.Size(), vcfg.sampleRate);
		printf("  [ %s ] TTS -> nkvoiceloop_reponse_synthetique.wav ecrit (%d ech.) -- boucle COMPLETE reussie :\n"
			   "      commande reconnue -> logique -> reponse parlee correcte.\n",
			   ttsOk ? "OK" : "KO", (int)wav.Size());
	}

	printf("\n=== Resultat : %d/%d OK ===\n", nbOk, nbTotal);
	printf("Sortie NKAudio : reponses ecrites en WAV (lecture temps reel deja prouvee separement par\n"
		   "NkAudioPlayer/NkAudioDemo, non invoquee ici pour rester non-interactif et bloquant).\n");
	return (nbOk == nbTotal) ? 0 : 1;
}
