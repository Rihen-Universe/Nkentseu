// =============================================================================
// NKASRTest — ASR acoustique from-scratch, bout-en-bout, sur audio SYNTHÉTIQUE.
//
// Prouve la brique 2 de la Phase 8 (ASR) en assemblant les briques livrées :
//   audio jouet (3 symboles = 3 tons distincts) → MFCC (NkAudioFeatures) → BiGRU +
//   tête linéaire (NkASRModel) → perte CTC (autograd::CTCLoss) → décodage glouton.
// Chaque « mot » est une suite de symboles rendus en tons ; le modèle doit
// apprendre à transcrire la suite SANS alignement fourni. La perte doit chuter et
// le décodage retrouver la suite cible pour tous les mots d'entraînement.
// =============================================================================
#include "NKSpeech/NkAudioFeatures.h"
#include "NKSpeech/NkAsrModel.h"
#include "NKTrain/NkTrain.h" // NkLRSchedule (Option A.1)
#include "NKOptim/NkOptim.h"
#include "NKAutograd/NkVar.h"
#include "NKTensor/NkTensor.h"

#include <cstdio>
#include <cmath>

using namespace nkentseu;
using namespace nkentseu::ai;

static int g_pass = 0, g_fail = 0;

// Fréquence d'un symbole : 3 tons bien séparés (MFCC distinctes).
static double SymFreq(int sym) {
	static const double f[3] = {300.0, 1000.0, 3000.0}; // tons bien séparés (MFCC distinctes)
	return f[sym % 3];
}

// Synthétise l'audio d'un « mot » (suite de symboles) : chaque symbole = un ton de
// `segMs` ms, léger fondu + bruit déterministe (LCG) pour éviter le sur-ajustement trivial.
static void Synthesize(const NkVector<int32> &word, int sr, int segMs, uint32 seed, NkVector<float32> &out) {
	const int seg = sr * segMs / 1000;
	out.Clear();
	uint32 s = seed ? seed : 1u;
	for (uint32 i = 0; i < word.Size(); ++i) {
		const double f = SymFreq(word[i]);
		for (int n = 0; n < seg; ++n) {
			s = s * 1664525u + 1013904223u;
			const double noise = ((double)((s >> 9) & 0xFFFFu) / 65535.0 - 0.5) * 0.05;
			// Fondu entrée/sortie (fenêtre de Hann courte) pour des transitions douces.
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

// MFCC -> trames standardisées (moyenne 0, variance 1 par dimension sur le mot) en NkVar [1,dims].
static void FeaturesOf(const NkVector<float32> &audio, int sr, NkVector<NkVar> &frames, int &dims) {
	NkAudioFeatureConfig cfg;
	cfg.sampleRate = sr;
	cfg.melBands = 26;
	cfg.mfccCount = 13;
	cfg.useDeltas = true; // 13*3 = 39 dims
	NkFeatureMatrix m = NkAudioFeatures::MFCC(audio.Data(), (int32)audio.Size(), cfg);
	dims = m.dims;
	const int F = m.frames, D = m.dims;
	// Standardisation par dimension.
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

int main() {
	printf("=== NKASRTest : ASR acoustique BiGRU + CTC (audio synthétique) ===\n\n");
	const int SR = 16000, SEGMS = 180, BLANK = 3, V = 4; // symboles 0,1,2 + blanc=3

	// Jeu de « mots » (suites de symboles) à apprendre.
	NkVector<NkVector<int32>> words;
	{
		int32 w0[3] = {0, 1, 2}, w1[3] = {2, 1, 0}, w2[3] = {0, 2, 1}, w3[4] = {1, 0, 2, 1};
		NkVector<int32> a, b, c, d;
		for (int i = 0; i < 3; ++i) {
			a.PushBack(w0[i]);
			b.PushBack(w1[i]);
			c.PushBack(w2[i]);
		}
		for (int i = 0; i < 4; ++i)
			d.PushBack(w3[i]);
		words.PushBack(a);
		words.PushBack(b);
		words.PushBack(c);
		words.PushBack(d);
	}

	// Extrait les features de chaque mot (audio -> MFCC standardisées).
	printf("-- Extraction MFCC (NkAudioFeatures) --\n");
	NkVector<NkVector<NkVar>> feats;
	feats.Resize(words.Size());
	int dims = 0;
	for (uint32 w = 0; w < words.Size(); ++w) {
		NkVector<float32> audio;
		Synthesize(words[w], SR, SEGMS, 100u + w * 7u, audio);
		FeaturesOf(audio, SR, feats[(nk_size)w], dims);
		printf("  mot %u : %d échantillons -> %u trames × %d dims\n", w, (int)audio.Size(),
			   feats[(nk_size)w].Size(), dims);
	}
	(dims == 39 ? g_pass : g_fail)++;
	printf("  [ %s ] MFCC 39 dims (13 + Δ + ΔΔ)\n", dims == 39 ? "OK" : "KO");

	// Modèle acoustique BiGRU + CTC.
	const int HID = 64;
	printf("\n-- Entraînement (BiGRU %d->%d, tête ->%d, Adam + CTC) --\n", dims, HID, V);
	NkASRModel model(dims, HID, V, 2024u);
	NkVector<NkVar> params;
	model.Parameters(params);
	optim::NkAdam adam(params, 0.002f);

	// Planificateur LR (Option A.1) : warmup + décroissance cosine — stabilise
	// l'entraînement (un lr fixe trop élevé fait DIVERGER / collapser la perte CTC).
	const int STEPS = 250;
	const double invN = 1.0 / (double)words.Size();
	train::NkLRSchedule sched;
	sched.peakLr = 0.003f;
	sched.warmupSteps = 60;					   // en pas GLOBAUX (un par énoncé)
	sched.totalSteps = STEPS * words.Size();   // gstep total
	sched.minLrRatio = 0.1;

	double firstLoss = 0, lastLoss = 0;
	int64 gstep = 0;
	for (int step = 1; step <= STEPS; ++step) {
		double epochLoss = 0;
		// Mise à jour EN LIGNE par énoncé (un pas Adam par mot) : évite que les gradients
		// de séquences différentes s'additionnent en une direction moyenne qui diverge.
		for (uint32 w = 0; w < words.Size(); ++w) {
			++gstep;
			adam.SetLearningRate(sched.LrAt(gstep));
			adam.ZeroGrad();
			NkVar logits = model.Forward(feats[(nk_size)w]); // [T,1,V]
			NkVector<NkVector<int32>> tgt;
			tgt.PushBack(words[(nk_size)w]);
			NkVar loss = autograd::CTCLoss(logits, tgt, BLANK);
			loss.Backward();
			adam.Step();
			epochLoss += loss.Value().GetItem(NkShape{(int64)0});
		}
		epochLoss *= invN;
		if (step == 1)
			firstLoss = epochLoss;
		lastLoss = epochLoss;
		if (step % 50 == 0 || step == 1)
			printf("  step %3d : perte CTC moyenne = %.5f  (lr %.5f)\n", step, epochLoss, (double)sched.LrAt(gstep));
	}

	// Décodage glouton de chaque mot.
	printf("\n-- Décodage glouton --\n");
	int correct = 0;
	for (uint32 w = 0; w < words.Size(); ++w) {
		NkVar logits = model.Forward(feats[(nk_size)w]);
		NkVector<int32> dec;
		NkCTCGreedyDecode(logits.Value(), BLANK, dec);
		const bool ok = SameSeq(dec, words[(nk_size)w]);
		correct += ok ? 1 : 0;
		printf("  mot %u : cible {", w);
		for (uint32 i = 0; i < words[(nk_size)w].Size(); ++i)
			printf("%s%d", i ? "," : "", words[(nk_size)w][i]);
		printf("}  décodé {");
		for (uint32 i = 0; i < dec.Size(); ++i)
			printf("%s%d", i ? "," : "", dec[i]);
		printf("}  [%s]\n", ok ? "OK" : "KO");
	}

	const bool lossOk = lastLoss < firstLoss * 0.2 && lastLoss < 0.5;
	const bool decodeOk = correct == (int)words.Size();
	(lossOk ? g_pass : g_fail)++;
	(decodeOk ? g_pass : g_fail)++;
	printf("\n  [ %s ] la perte CTC a chuté (%.4f -> %.4f)\n", lossOk ? "OK" : "KO", firstLoss, lastLoss);
	printf("  [ %s ] %d/%u mots transcrits correctement\n", decodeOk ? "OK" : "KO", correct, words.Size());

	printf("\n=== Résultat : %d OK, %d échec(s) ===\n", g_pass, g_fail);
	return g_fail == 0 ? 0 : 1;
}
