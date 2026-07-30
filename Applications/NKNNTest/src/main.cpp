// =============================================================================
// NKNNTest — entraîne XOR via l'API propre NKNN (Dense) + NKOptim (SGD).
//
// Même réseau que NKAutogradTest, mais assemblé avec des COUCHES réutilisables et
// un OPTIMISEUR (au lieu du SGD manuel). Prouve la pile Phase 2 :
//   NKTensor -> NKAutograd -> NKNN + NKOptim.
// =============================================================================
#include "NKNN/NkNN.h"
#include "NKOptim/NkOptim.h"
#include "NKAutograd/NkVar.h"
#include "NKTensor/NkTensor.h"
#include "NKLogger/NkLog.h"

#include <cstdio>
#include <cmath>

using namespace nkentseu;
using namespace nkentseu::ai;

int main() {
	printf("=== NKNNTest : XOR via NKNN (Dense) + NKOptim (SGD) ===\n\n");

	// Jeu XOR : 4 exemples.
	const float Xd[8] = {0, 0, 0, 1, 1, 0, 1, 1};
	const float Yd[4] = {0, 1, 1, 0};
	NkTensor X = NkTensor::FromData(NkShape{4, 2}, Xd, NkDType::NK_F32);
	NkTensor Y = NkTensor::FromData(NkShape{4, 1}, Yd, NkDType::NK_F32);

	// Modèle : Dense(2->8) -> tanh -> Dense(8->1) -> sigmoid.
	nn::NkDense l1(2, 8, 12345u);
	nn::NkDense l2(8, 1, 6789u);

	// Optimiseur : SGD sur tous les paramètres (W1,b1,W2,b2), momentum 0.9.
	NkVector<NkVar> params;
	l1.Parameters(params);
	l2.Parameters(params);
	optim::NkSGD opt(params, /*lr*/ 0.5f, /*momentum*/ 0.9f);

	NkVar xin = NkVar::Leaf(X, false);
	NkVar yt = NkVar::Leaf(Y, false);

	auto forward = [&](const NkVar &x) {
		NkVar h = nn::Tanh(l1.Forward(x));
		return nn::Sigmoid(l2.Forward(h));
	};

	const int epochs = 5000;
	double lastLoss = 0.0;
	for (int e = 0; e <= epochs; ++e) {
		NkVar o = forward(xin);
		NkVar loss = nn::MSELoss(o, yt);
		loss.Backward();
		opt.Step();
		lastLoss = loss.Value().GetItem(NkShape{(int64)0});
		if (e % 500 == 0)
			printf("  epoch %5d : perte = %.6f\n", e, lastLoss);
	}

	// Prédictions finales.
	NkVar o = forward(xin);
	const NkTensor &pred = o.Value();
	int correct = 0;
	printf("\n  XOR : entrée -> prédiction (cible)\n");
	for (int i = 0; i < 4; ++i) {
		double p = pred.GetItem(NkShape{(int64)i, (int64)0});
		bool ok = (p > 0.5) == (Yd[i] > 0.5);
		correct += ok ? 1 : 0;
		printf("    [%d,%d] -> %.3f  (%.0f)  %s\n", (int)Xd[i * 2], (int)Xd[i * 2 + 1], p, (double)Yd[i],
			   ok ? "OK" : "KO");
	}

	const bool xorOk = (correct == 4) && (lastLoss < 1e-2);
	printf("\n  [ %s ] XOR appris via NKNN+NKOptim (%d/4, perte finale %.6f)\n", xorOk ? "OK" : "KO", correct,
		   lastLoss);

	// -----------------------------------------------------------------------
	// Classification 3 classes (Adam + entropie croisée) : 3 clusters 2D.
	// Modèle : Dense(2->16) -> relu -> Dense(16->3) -> logits ; CrossEntropyLoss.
	// -----------------------------------------------------------------------
	printf("\n-- Classification 3 classes (Adam + CrossEntropy) --\n");

	const uint32 NC = 3, PER = 12, NP = NC * PER; // 36 points
	const float centers[3][2] = {{-2.f, -2.f}, {2.f, -2.f}, {0.f, 2.f}};
	NkTensor Xc = NkTensor::Zeros(NkShape{(int64)NP, 2});
	NkVector<int32> labels;
	{
		float *xp = Xc.DataAs<float>();
		uint32 s = 777u;
		auto jit = [&s]() {
			s = s * 1664525u + 1013904223u;
			return ((float)((s >> 9) & 0x7FFFu) / 32767.0f - 0.5f) * 0.9f;
		};
		for (uint32 c = 0; c < NC; ++c)
			for (uint32 k = 0; k < PER; ++k) {
				uint32 i = c * PER + k;
				xp[i * 2 + 0] = centers[c][0] + jit();
				xp[i * 2 + 1] = centers[c][1] + jit();
				labels.PushBack((int32)c);
			}
	}
	NkTensor Oh = nn::OneHot(labels.Data(), NP, NC);

	nn::NkDense c1(2, 16, 4242u);
	nn::NkDense c2(16, NC, 909u);
	NkVector<NkVar> cparams;
	c1.Parameters(cparams);
	c2.Parameters(cparams);
	optim::NkAdam adam(cparams, /*lr*/ 0.02f);

	NkVar xin2 = NkVar::Leaf(Xc, false);
	NkVar yoh = NkVar::Leaf(Oh, false);
	auto fwd2 = [&](const NkVar &x) { return c2.Forward(nn::Relu(c1.Forward(x))); };

	double clsLoss = 0.0;
	for (int e = 0; e <= 2000; ++e) {
		NkVar logits = fwd2(xin2);
		NkVar loss = nn::CrossEntropyLoss(logits, yoh);
		loss.Backward();
		adam.Step();
		clsLoss = loss.Value().GetItem(NkShape{(int64)0});
		if (e % 400 == 0)
			printf("  epoch %5d : perte CE = %.6f\n", e, clsLoss);
	}

	// Exactitude finale (argmax des logits).
	NkTensor logits = fwd2(xin2).Value().Contiguous();
	const float *lp = logits.DataAs<float>();
	int good = 0;
	for (uint32 i = 0; i < NP; ++i) {
		int best = 0;
		float bv = lp[i * NC];
		for (uint32 c = 1; c < NC; ++c)
			if (lp[i * NC + c] > bv) {
				bv = lp[i * NC + c];
				best = (int)c;
			}
		if (best == labels[i])
			++good;
	}
	const double acc = (double)good / (double)NP;
	const bool clsOk = acc >= 0.95;
	printf("  [ %s ] classification (%d/%d = %.1f%% exactitude, perte CE %.6f)\n", clsOk ? "OK" : "KO", good, (int)NP,
		   acc * 100.0, clsLoss);

	// =========================================================================
	// NKOptim : clipping de gradient (Pascanu, Mikolov & Bengio, ICML 2013).
	// =========================================================================
	logger.Info("-- NKOptim : clipping de gradient --");

	bool clipByValueOk = false;
	{
		// 1) Clip PAR VALEUR : gradient volontairement énorme -> ramené sous le seuil,
		//    élément par élément (assertion numérique réelle sur chaque composante).
		NkVar p = NkVar::Leaf(NkTensor::Zeros(NkShape{4}), true);
		float gd[4] = {1000.0f, -500.0f, 50.0f, -2000.0f};
		p.SetGrad(NkTensor::FromData(NkShape{4}, gd, NkDType::NK_F32));

		NkVector<NkVar> params1;
		params1.PushBack(p);
		const float clipVal = 5.0f;
		optim::ClipGradients(params1, clipVal, optim::NkGradClipMode::NK_CLIP_BY_VALUE);

		NkTensor g2 = p.Grad().Contiguous();
		const float *gp = g2.DataAs<float>();
		bool inRange = true;
		for (int i = 0; i < 4; ++i)
			if (gp[i] > clipVal + 1e-4f || gp[i] < -clipVal - 1e-4f)
				inRange = false;
		// La composante non saturante (50 -> clippée à +5, mais signe/borne exacts) doit
		// être EXACTEMENT sur le seuil (signe positif préservé).
		const bool sat = std::fabs(gp[2] - clipVal) < 1e-4f;
		clipByValueOk = inRange && sat;
		logger.Info("  [ {0} ] clip PAR VALEUR (seuil={1}) : brut=[{2},{3},{4},{5}] -> clippe=[{6},{7},{8},{9}]",
					clipByValueOk ? "OK" : "KO", clipVal, gd[0], gd[1], gd[2], gd[3], gp[0], gp[1], gp[2], gp[3]);
	}

	bool clipByNormOk = false;
	{
		// 2) Clip PAR NORME GLOBALE : deux paramètres, norme globale énorme -> ramenée
		//    sous le seuil, la DIRECTION relative entre les gradients est préservée.
		float g1d[2] = {30.0f, 40.0f};		 // norme = 50
		float g2d[3] = {0.0f, 0.0f, 60.0f}; // norme = 60 -> norme globale = sqrt(50^2+60^2)
		NkVar p1 = NkVar::Leaf(NkTensor::Zeros(NkShape{2}), true);
		NkVar p2 = NkVar::Leaf(NkTensor::Zeros(NkShape{3}), true);
		p1.SetGrad(NkTensor::FromData(NkShape{2}, g1d, NkDType::NK_F32));
		p2.SetGrad(NkTensor::FromData(NkShape{3}, g2d, NkDType::NK_F32));

		NkVector<NkVar> params2;
		params2.PushBack(p1);
		params2.PushBack(p2);
		const float clipVal2 = 10.0f;
		const double preNorm =
			optim::ClipGradients(params2, clipVal2, optim::NkGradClipMode::NK_CLIP_BY_GLOBAL_NORM);

		NkTensor g1a = p1.Grad().Contiguous();
		NkTensor g2a = p2.Grad().Contiguous();
		double sumSq = 0.0;
		{
			const float *gp = g1a.DataAs<float>();
			for (int i = 0; i < 2; ++i)
				sumSq += (double)gp[i] * (double)gp[i];
		}
		{
			const float *gp = g2a.DataAs<float>();
			for (int i = 0; i < 3; ++i)
				sumSq += (double)gp[i] * (double)gp[i];
		}
		const double postNorm = std::sqrt(sumSq);
		const double expectedPreNorm = std::sqrt(50.0 * 50.0 + 60.0 * 60.0);
		const bool preNormOk = std::fabs(preNorm - expectedPreNorm) < 1e-2;
		const bool postNormOk = (postNorm <= clipVal2 + 1e-2) && (postNorm > clipVal2 * 0.99);

		const float *gp1 = g1a.DataAs<float>();
		const bool dirOk = std::fabs(((double)gp1[1] / (double)gp1[0]) - (40.0 / 30.0)) < 1e-3;

		clipByNormOk = preNormOk && postNormOk && dirOk;
		logger.Info("  [ {0} ] clip PAR NORME GLOBALE : norme avant={1:.4} (attendu {2:.4}), norme apres={3:.4} "
					"(seuil={4}), direction preservee={5}",
					clipByNormOk ? "OK" : "KO", preNorm, expectedPreNorm, postNorm, clipVal2, dirOk);
	}

	bool clipRegressionOk = false;
	{
		// 3) Non-régression : Step() SANS jamais toucher au clipping (comportement
		//    historique) doit produire EXACTEMENT la même trajectoire que Step() avec
		//    le clipping explicitement désactivé (NK_CLIP_NONE, la valeur par défaut) —
		//    prouve que l'ajout du clipping n'a AUCUN effet de bord quand il n'est pas activé.
		auto trainXorOnce = [](bool touchClipApiButDisabled) -> double {
			const float Xd2[8] = {0, 0, 0, 1, 1, 0, 1, 1};
			const float Yd2[4] = {0, 1, 1, 0};
			NkTensor X2 = NkTensor::FromData(NkShape{4, 2}, Xd2, NkDType::NK_F32);
			NkTensor Y2 = NkTensor::FromData(NkShape{4, 1}, Yd2, NkDType::NK_F32);
			nn::NkDense d1(2, 8, 42u);
			nn::NkDense d2(8, 1, 43u);
			NkVector<NkVar> ps;
			d1.Parameters(ps);
			d2.Parameters(ps);
			optim::NkSGD sgd(ps, 0.5f, 0.9f);
			if (touchClipApiButDisabled)
				sgd.SetGradClip(optim::NkGradClipMode::NK_CLIP_NONE, 1.0f); // désactivé explicitement
			NkVar xin3 = NkVar::Leaf(X2, false), yt3 = NkVar::Leaf(Y2, false);
			double lastLoss = 0.0;
			for (int e = 0; e <= 500; ++e) {
				NkVar h3 = nn::Tanh(d1.Forward(xin3));
				NkVar o3 = nn::Sigmoid(d2.Forward(h3));
				NkVar loss3 = nn::MSELoss(o3, yt3);
				loss3.Backward();
				sgd.Step();
				lastLoss = loss3.Value().GetItem(NkShape{(int64)0});
			}
			return lastLoss;
		};
		const double lossA = trainXorOnce(false);
		const double lossB = trainXorOnce(true);
		clipRegressionOk = std::fabs(lossA - lossB) < 1e-12;
		logger.Info("  [ {0} ] non-regression : perte sans toucher l'API clip = {1:.8}, perte avec "
					"SetGradClip(NONE) explicite = {2:.8} (doivent etre IDENTIQUES)",
					clipRegressionOk ? "OK" : "KO", lossA, lossB);
	}

	// =========================================================================
	// NKNN : Dropout (masque Bernoulli inversé) — taux de zéros + no-op en éval.
	// =========================================================================
	logger.Info("-- NKNN : Dropout --");
	bool dropoutOk = false;
	{
		const int64 N = 200000;
		NkVar x = NkVar::Leaf(NkTensor::Full(NkShape{N}, 1.0), false);
		const float p = 0.3f;
		nn::NkDropout drop(p, 999u);

		// Mode ENTRAÎNEMENT (par défaut) : vérifie le taux de zéros statistiquement.
		NkVar y = drop.Forward(x);
		NkTensor yc = y.Value().Contiguous();
		const float *yp = yc.DataAs<float>();
		int64 zeroCount = 0;
		double sum = 0.0;
		for (int64 i = 0; i < N; ++i) {
			if (yp[i] == 0.0f)
				++zeroCount;
			sum += yp[i];
		}
		const double zeroRate = (double)zeroCount / (double)N;
		const double mean = sum / (double)N; // doit rester ~1.0 (inversion : pas de biais d'échelle)
		const bool trainStatsOk = (std::fabs(zeroRate - (double)p) < 0.01) && (std::fabs(mean - 1.0) < 0.02);

		// Mode ÉVALUATION : NO-OP strict, zéro drop.
		drop.SetTraining(false);
		NkVar yEval = drop.Forward(x);
		NkTensor ye = yEval.Value().Contiguous();
		const float *yep = ye.DataAs<float>();
		int64 zeroEval = 0;
		for (int64 i = 0; i < N; ++i)
			if (yep[i] == 0.0f)
				++zeroEval;
		const bool evalOk = (zeroEval == 0);

		dropoutOk = trainStatsOk && evalOk;
		logger.Info("  [ {0} ] Dropout(p={1}) : taux de zeros mesure={2:.4} (attendu ~{3}), moyenne={4:.4} "
					"(attendu ~1.0) ; mode eval zeros={5}/{6}",
					dropoutOk ? "OK" : "KO", p, zeroRate, p, mean, zeroEval, N);
	}

	// =========================================================================
	// NKNN : NkMLP prêt à l'emploi (Sequential) — entraîné sur la classification 3
	// classes déjà construite plus haut (Xc/labels/Oh, xin2/yoh) : résultat RÉEL.
	// =========================================================================
	logger.Info("-- NKNN : NkMLP pret a l'emploi (NkSequential) --");
	bool mlpOk = false;
	{
		nn::NkMLP mlp(NkVector<uint32>{2, 16, 16, (uint32)NC}, /*dropoutP*/ 0.0f, 3131u);
		NkVector<NkVar> mparams;
		mlp.Parameters(mparams);
		optim::NkAdam adamMlp(mparams, 0.02f);

		double mlpLoss = 0.0;
		for (int e = 0; e <= 2000; ++e) {
			NkVar logitsMlp = mlp.Forward(xin2);
			NkVar lossMlp = nn::CrossEntropyLoss(logitsMlp, yoh);
			lossMlp.Backward();
			adamMlp.Step();
			mlpLoss = lossMlp.Value().GetItem(NkShape{(int64)0});
		}

		NkTensor logitsF = mlp.Forward(xin2).Value().Contiguous();
		const float *flp = logitsF.DataAs<float>();
		int mlpGood = 0;
		for (uint32 i = 0; i < NP; ++i) {
			int best = 0;
			float bv = flp[i * NC];
			for (uint32 c = 1; c < NC; ++c)
				if (flp[i * NC + c] > bv) {
					bv = flp[i * NC + c];
					best = (int)c;
				}
			if (best == labels[i])
				++mlpGood;
		}
		const double mlpAcc = (double)mlpGood / (double)NP;
		mlpOk = (mlpAcc >= 0.95) && (mparams.Size() == 6); // 3 couches Dense (W+b chacune) = 6 tenseurs
		logger.Info("  [ {0} ] NkMLP({1} parametres) : {2}/{3} = {4:.4}% exactitude (perte finale {5:.6})",
					mlpOk ? "OK" : "KO", mparams.Size(), mlpGood, (int)NP, mlpAcc * 100.0, mlpLoss);
	}

	const int totalTests = 7;
	const int pass = (xorOk ? 1 : 0) + (clsOk ? 1 : 0) + (clipByValueOk ? 1 : 0) + (clipByNormOk ? 1 : 0) +
					  (clipRegressionOk ? 1 : 0) + (dropoutOk ? 1 : 0) + (mlpOk ? 1 : 0);
	printf("\n=== Résultat : %d OK, %d échec(s) ===\n", pass, totalTests - pass);
	return (pass == totalTests) ? 0 : 1;
}
