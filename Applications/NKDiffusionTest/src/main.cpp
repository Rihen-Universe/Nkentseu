// =============================================================================
// NKDiffusionTest — DDPM JOUET 2D CPU : "cercle bruite -> cercle" par
// debruitage iteratif (NKGen::NkDiffusion + NKNN + NKOptim).
//
// Algorithme suivi : Ho, Jain, Abbeel, "Denoising Diffusion Probabilistic
// Models", NeurIPS 2020, arXiv:2006.11239 (cf Kernel/AI/NKGen/src/NKGen/
// NkDiffusion.h pour le detail des formules et des choix d'implementation).
//
// On n'a pas de dataset reel -> on GENERE nous-memes un jeu de donnees
// synthetique 2D (points sur un cercle, rayon+jitter). On entraine le petit
// MLP epsilon_theta(x_t, t) a predire le bruit gaussien ajoute (loss L2), puis
// on ECHANTILLONNE en partant de bruit pur et en iterant T pas de debruitage.
// La qualite est mesuree par un critere CHIFFRE (pas visuel) : distance
// moyenne au plus proche voisin d'entrainement + statistiques de rayon
// (mean/std), AVANT vs APRES entrainement.
//
// AVERTISSEMENT (honnetete) : ceci reste un PROTOTYPE JOUET, 100% CPU. Pas
// d'image, pas de branchement NKImage/NKRHI ici.
//
// Ce fichier contient TROIS sections independantes (Jalon 1 puis Jalon 2) :
//   1) Diffusion 2D INCONDITIONNELLE (cercle) -- section d'origine, inchangee.
//   2) Diffusion 2D CONDITIONNEE PAR CLASSE (cercle / carre / spirale) --
//      conditionnement par concatenation d'un one-hot de classe a l'entree du
//      MLP (gen::NkDiffusion, cf NkDiffusion.h -- pattern Mirza & Osindero,
//      "Conditional Generative Adversarial Nets", 2014, arXiv:1411.1784 ;
//      PAS le classifier-free guidance de Ho & Salimans 2022, cf commentaire
//      de NkDiffusion.h). Mesure CHIFFREE par classe : matrice de distance
//      moyenne au plus proche voisin genere(classe c) vs train(classe c'),
//      le minimum de chaque ligne doit etre sur la diagonale (c'=c) --
//      preuve objective que le label demande influence bien la forme produite.
//   3) Extension 3D INCONDITIONNELLE (nuage de points sur une sphere) --
//      prouve que le pipeline generalise au-dela de 2D. AVERTISSEMENT :
//      c'est un NUAGE DE POINTS BRUT, PAS un maillage (aucune topologie/face,
//      pas d'extraction de surface -- marching cubes non traite ici).
// =============================================================================
#include "NKGen/NkGen.h"
#include "NKOptim/NkOptim.h"
#include "NKAutograd/NkVar.h"
#include "NKTensor/NkTensor.h"
#include "NKLogger/NkLog.h"

#include <cmath>

using namespace nkentseu;
using namespace nkentseu::ai;

static const float NK_PI = 3.14159265358979323846f;

// ---- RNG utilitaires (LCG + Box-Muller, meme schema que les autres bancs
// NKGen, ex. NKObjectGenTest::FillRandn) : pas de dependance supplementaire,
// suffisant pour du bruit d'entrainement/dataset jouet. --------------------
static uint32 NextLcg(uint32 &s) {
	s = s * 1664525u + 1013904223u;
	return s;
}

static float RandUnit(uint32 &s) { // uniforme dans [0,1)
	return (float)((NextLcg(s) >> 8) & 0xFFFFFFu) / 16777216.0f;
}

static void FillGaussianLocal(NkTensor &t, uint32 &s) {
	float *p = t.DataAs<float>();
	int64 n = NkShapeNumel(t.Shape());
	for (int64 i = 0; i < n; ++i) {
		double u1 = (double)RandUnit(s);
		if (u1 < 1e-7)
			u1 = 1e-7;
		double u2 = (double)RandUnit(s);
		p[i] = (float)(std::sqrt(-2.0 * std::log(u1)) * std::cos(6.2831853 * u2));
	}
}

// ---- Jeu de donnees synthetique : cercle 2D (rayon `radius`, epaisseur
// `jitter`) -- genere ici en C++, aucun fichier externe. -------------------
static NkTensor MakeCircleDataset(uint32 n, float radius, float jitter, uint32 &s) {
	NkTensor x = NkTensor::Zeros(NkShape{(int64)n, 2});
	float *p = x.DataAs<float>();
	for (uint32 i = 0; i < n; ++i) {
		float angle = RandUnit(s) * 2.0f * NK_PI;
		float r = radius + (RandUnit(s) - 0.5f) * 2.0f * jitter;
		p[i * 2 + 0] = r * std::cos(angle);
		p[i * 2 + 1] = r * std::sin(angle);
	}
	return x;
}

static void RadiusStats(const NkTensor &pts, double &meanR, double &stdR) {
	const float *p = pts.DataAs<float>();
	int64 n = pts.Shape()[0];
	double sum = 0.0;
	for (int64 i = 0; i < n; ++i) {
		double dx = (double)p[i * 2 + 0];
		double dy = (double)p[i * 2 + 1];
		sum += std::sqrt(dx * dx + dy * dy);
	}
	meanR = sum / (double)n;
	double var = 0.0;
	for (int64 i = 0; i < n; ++i) {
		double dx = (double)p[i * 2 + 0];
		double dy = (double)p[i * 2 + 1];
		double r = std::sqrt(dx * dx + dy * dy);
		var += (r - meanR) * (r - meanR);
	}
	stdR = std::sqrt(var / (double)n);
}

// Distance moyenne, sur tous les points de `samples`, au plus proche point de
// `train` (recherche brute -- N petit, jouet). C'est le critere CHIFFRE
// demande : mesure objective de "la distribution generee ressemble-t-elle a
// la distribution d'entrainement", pas une inspection visuelle.
static double MeanNearestNeighborDist(const NkTensor &samples, const NkTensor &train) {
	const float *sp = samples.DataAs<float>();
	const float *tp = train.DataAs<float>();
	int64 ns = samples.Shape()[0];
	int64 nt = train.Shape()[0];
	double sumMin = 0.0;
	for (int64 i = 0; i < ns; ++i) {
		double best = 1e18;
		for (int64 j = 0; j < nt; ++j) {
			double dx = (double)sp[i * 2 + 0] - (double)tp[j * 2 + 0];
			double dy = (double)sp[i * 2 + 1] - (double)tp[j * 2 + 1];
			double d2 = dx * dx + dy * dy;
			if (d2 < best)
				best = d2;
		}
		sumMin += std::sqrt(best);
	}
	return sumMin / (double)ns;
}

// ---- Jeu de donnees synthetique : carre 2D (demi-cote `half`, jitter) --
// points tires uniformement sur le PERIMETRE (4 aretes) -- classe 1 du test
// de conditionnement. ------------------------------------------------------
static NkTensor MakeSquareDataset(uint32 n, float half, float jitter, uint32 &s) {
	NkTensor x = NkTensor::Zeros(NkShape{(int64)n, 2});
	float *p = x.DataAs<float>();
	for (uint32 i = 0; i < n; ++i) {
		uint32 edge = (uint32)(RandUnit(s) * 4.0f);
		if (edge >= 4)
			edge = 3;
		float t = (RandUnit(s) * 2.0f - 1.0f) * half;
		float px = 0.0f, py = 0.0f;
		switch (edge) {
			case 0:
				px = t;
				py = -half;
				break; // bas
			case 1:
				px = t;
				py = half;
				break; // haut
			case 2:
				px = -half;
				py = t;
				break; // gauche
			default:
				px = half;
				py = t;
				break; // droite
		}
		px += (RandUnit(s) - 0.5f) * 2.0f * jitter;
		py += (RandUnit(s) - 0.5f) * 2.0f * jitter;
		p[i * 2 + 0] = px;
		p[i * 2 + 1] = py;
	}
	return x;
}

// ---- Jeu de donnees synthetique : spirale d'Archimede 2D (r = a + b*theta,
// theta in [0, thetaMax], jitter) -- classe 2 du test de conditionnement. --
static NkTensor MakeSpiralDataset(uint32 n, float a, float b, float thetaMax, float jitter, uint32 &s) {
	NkTensor x = NkTensor::Zeros(NkShape{(int64)n, 2});
	float *p = x.DataAs<float>();
	for (uint32 i = 0; i < n; ++i) {
		float theta = RandUnit(s) * thetaMax;
		float r = a + b * theta;
		float px = r * std::cos(theta) + (RandUnit(s) - 0.5f) * 2.0f * jitter;
		float py = r * std::sin(theta) + (RandUnit(s) - 0.5f) * 2.0f * jitter;
		p[i * 2 + 0] = px;
		p[i * 2 + 1] = py;
	}
	return x;
}

// ---- Jeu de donnees synthetique : sphere 3D (rayon `radius`, jitter RADIAL)
// -- generalisation 3D de MakeCircleDataset. Direction uniforme sur la sphere
// via 3 gaussiennes normalisees (Marsaglia 1972 : un vecteur gaussien N(0,I3)
// normalise est uniforme sur la sphere unite). -----------------------------
static NkTensor MakeSphereDataset(uint32 n, float radius, float jitter, uint32 &s) {
	NkTensor x = NkTensor::Zeros(NkShape{(int64)n, 3});
	float *p = x.DataAs<float>();
	for (uint32 i = 0; i < n; ++i) {
		double u1 = (double)RandUnit(s);
		if (u1 < 1e-7)
			u1 = 1e-7;
		double u2 = (double)RandUnit(s);
		double g0 = std::sqrt(-2.0 * std::log(u1)) * std::cos(6.2831853 * u2);
		double g1 = std::sqrt(-2.0 * std::log(u1)) * std::sin(6.2831853 * u2);
		double u3 = (double)RandUnit(s);
		if (u3 < 1e-7)
			u3 = 1e-7;
		double u4 = (double)RandUnit(s);
		double g2 = std::sqrt(-2.0 * std::log(u3)) * std::cos(6.2831853 * u4);
		double norm = std::sqrt(g0 * g0 + g1 * g1 + g2 * g2);
		if (norm < 1e-9)
			norm = 1e-9;
		float r = radius + (RandUnit(s) - 0.5f) * 2.0f * jitter;
		p[i * 3 + 0] = (float)(g0 / norm) * r;
		p[i * 3 + 1] = (float)(g1 / norm) * r;
		p[i * 3 + 2] = (float)(g2 / norm) * r;
	}
	return x;
}

// Version generique (dimension quelconque) de RadiusStats -- distance a
// l'origine (rayon) mean/std, reutilisee par la sphere 3D.
static void RadiusStatsND(const NkTensor &pts, int64 dim, double &meanR, double &stdR) {
	const float *p = pts.DataAs<float>();
	int64 n = pts.Shape()[0];
	NkVector<double> radii;
	radii.Reserve((NkVector<double>::SizeType)n);
	double sum = 0.0;
	for (int64 i = 0; i < n; ++i) {
		double d2 = 0.0;
		for (int64 d = 0; d < dim; ++d) {
			double v = (double)p[i * dim + d];
			d2 += v * v;
		}
		double r = std::sqrt(d2);
		radii.PushBack(r);
		sum += r;
	}
	meanR = sum / (double)n;
	double var = 0.0;
	for (int64 i = 0; i < n; ++i) {
		double diff = radii[i] - meanR;
		var += diff * diff;
	}
	stdR = std::sqrt(var / (double)n);
}

// Version generique (dimension quelconque) de MeanNearestNeighborDist --
// reutilisee par le test de conditionnement (dim=2) et par la sphere 3D
// (dim=3).
static double MeanNearestNeighborDistND(const NkTensor &samples, const NkTensor &train, int64 dim) {
	const float *sp = samples.DataAs<float>();
	const float *tp = train.DataAs<float>();
	int64 ns = samples.Shape()[0];
	int64 nt = train.Shape()[0];
	double sumMin = 0.0;
	for (int64 i = 0; i < ns; ++i) {
		double best = 1e18;
		for (int64 j = 0; j < nt; ++j) {
			double d2 = 0.0;
			for (int64 d = 0; d < dim; ++d) {
				double diff = (double)sp[i * dim + d] - (double)tp[j * dim + d];
				d2 += diff * diff;
			}
			if (d2 < best)
				best = d2;
		}
		sumMin += std::sqrt(best);
	}
	return sumMin / (double)ns;
}

int main() {
	logger.Info("=== NKDiffusionTest : DDPM jouet 2D (cercle), debruitage iteratif (Ho et al. 2020) ===");

	uint32 rng = 1234u;
	const uint32 N = 300;
	const float RADIUS = 2.0f;
	const float JITTER = 0.08f;
	NkTensor X0 = MakeCircleDataset(N, RADIUS, JITTER, rng);

	double trainMeanR = 0.0, trainStdR = 0.0;
	RadiusStats(X0, trainMeanR, trainStdR);
	logger.Info("Jeu d'entrainement : {0} points sur un cercle (rayon cible {1}, jitter {2}) -> rayon mesure : "
				"mean={3} std={4}",
				N, RADIUS, JITTER, trainMeanR, trainStdR);

	// ---- Schedule de bruit (lineaire, Ho et al. 2020 Sec.4) --------------
	const uint32 T = 100;
	gen::NkDiffusionSchedule sched = gen::NkDiffusionSchedule::Linear(T);
	logger.Info("Schedule lineaire T={0} : alphaBar[0]={1}  alphaBar[T-1]={2} (doit etre proche de 0 => bruit "
				"quasi-pur en fin de forward)",
				T, sched.alphaBars[0], sched.alphaBars[T - 1]);

	// ---- Modele : petit MLP epsilon_theta(x_t, t) -------------------------
	const uint32 TIME_EMBED = 8;
	const uint32 HIDDEN = 64;
	gen::NkDiffusion model(2u, TIME_EMBED, HIDDEN, 4242u);
	NkVector<NkVar> params;
	model.Parameters(params);

	// ---- Mesure AVANT entrainement (poids initiaux aleatoires) ------------
	const uint32 NSAMPLE = 200;
	NkTensor sampleBefore = model.Sample(NSAMPLE, sched, 999u);
	double beforeMeanR = 0.0, beforeStdR = 0.0;
	RadiusStats(sampleBefore, beforeMeanR, beforeStdR);
	double beforeNN = MeanNearestNeighborDist(sampleBefore, X0);
	logger.Info("AVANT entrainement : dist. moyenne au plus proche voisin = {0} ; rayon genere mean={1} std={2}",
				beforeNN, beforeMeanR, beforeStdR);

	// ---- Entrainement : perte L2(bruit predit, bruit reel) -----------------
	optim::NkAdam adam(params, 0.001f);
	const uint32 EPOCHS = 4000;
	const uint32 BATCH = 64;
	const float *x0Raw = X0.DataAs<float>();
	double lastLoss = 0.0;

	for (uint32 e = 0; e < EPOCHS; ++e) {
		NkTensor x0Batch = NkTensor::Zeros(NkShape{(int64)BATCH, 2});
		float *xp = x0Batch.DataAs<float>();
		NkVector<uint32> tIdx((NkVector<uint32>::SizeType)BATCH, 0u);
		for (uint32 b = 0; b < BATCH; ++b) {
			uint32 idx = (uint32)(RandUnit(rng) * (float)N);
			if (idx >= N)
				idx = N - 1;
			xp[b * 2 + 0] = x0Raw[idx * 2 + 0];
			xp[b * 2 + 1] = x0Raw[idx * 2 + 1];
			uint32 t = (uint32)(RandUnit(rng) * (float)T);
			if (t >= T)
				t = T - 1;
			tIdx[b] = t;
		}
		NkTensor eps = NkTensor::Zeros(NkShape{(int64)BATCH, 2});
		FillGaussianLocal(eps, rng);
		NkTensor xt = gen::NkDiffusionForward(x0Batch, sched, tIdx.Data(), eps);
		NkTensor modelIn = model.BuildModelInput(xt, tIdx.Data());

		adam.ZeroGrad(); // Backward() n'efface plus les feuilles : un pas = un gradient
		NkVar xin = NkVar::Leaf(modelIn, false);
		NkVar epsTarget = NkVar::Leaf(eps, false);
		NkVar epsPred = model.PredictNoise(xin);
		NkVar loss = autograd::MSE(epsPred, epsTarget);
		loss.Backward();
		adam.Step();

		lastLoss = loss.Value().GetItem(NkShape{(int64)0});
		if (e % 500 == 0)
			logger.Info("  epoque {0} : perte L2(bruit predit, bruit reel) = {1}", e, lastLoss);
	}
	logger.Info("Entrainement termine ({0} epoques) : perte finale = {1}", EPOCHS, lastLoss);

	// ---- Mesure APRES entrainement -----------------------------------------
	NkTensor sampleAfter = model.Sample(NSAMPLE, sched, 999u);
	double afterMeanR = 0.0, afterStdR = 0.0;
	RadiusStats(sampleAfter, afterMeanR, afterStdR);
	double afterNN = MeanNearestNeighborDist(sampleAfter, X0);
	logger.Info("APRES entrainement : dist. moyenne au plus proche voisin = {0} ; rayon genere mean={1} std={2}",
				afterNN, afterMeanR, afterStdR);

	int pass = 0, fail = 0;
	auto check = [&](bool ok, const char *name) {
		(ok ? pass : fail)++;
		logger.Info("[{0}] {1}", ok ? "OK" : "KO", name);
	};
	check(afterNN < beforeNN, "distance au plus proche voisin diminue apres entrainement");
	check(afterNN < 0.5, "distance au plus proche voisin < 0.5 apres entrainement (proche du cercle reel)");
	check(std::fabs(afterMeanR - trainMeanR) < 0.5, "rayon genere moyen proche du rayon d'entrainement (+/-0.5)");
	check(afterStdR < trainStdR * 4.0 + 0.3, "dispersion du rayon genere raisonnable (pas d'explosion)");

	logger.Info("=== Resultat : {0} OK, {1} echec(s) ===", pass, fail);

	// =========================================================================
	// SECTION 2 -- Jalon 2 : diffusion 2D CONDITIONNEE PAR CLASSE.
	// Trois classes/formes (cercle / carre / spirale), meme MLP epsilon_theta,
	// conditionnement par concatenation d'un one-hot de classe a l'entree
	// (gen::NkDiffusion(dataDim, timeEmbedDim, hidden, seed, numClasses) ;
	// cf NkDiffusion.h pour le detail et les sources citees).
	// =========================================================================
	logger.Info("=== SECTION 2 : diffusion CONDITIONNEE PAR CLASSE (cercle / carre / spirale) ===");

	uint32 rngC = 5678u;
	const uint32 NCLS = 300;
	const uint32 NUM_CLASSES = 3;
	NkTensor X0circle = MakeCircleDataset(NCLS, 2.0f, 0.08f, rngC);
	NkTensor X0square = MakeSquareDataset(NCLS, 2.0f, 0.08f, rngC);
	NkTensor X0spiral = MakeSpiralDataset(NCLS, 0.2f, 0.16f, 4.0f * NK_PI, 0.08f, rngC);
	const NkTensor *trainSets[NUM_CLASSES] = {&X0circle, &X0square, &X0spiral};
	const char *classNames[NUM_CLASSES] = {"cercle", "carre", "spirale"};

	gen::NkDiffusionSchedule schedC = gen::NkDiffusionSchedule::Linear(T);

	const uint32 TIME_EMBED_C = 8;
	const uint32 HIDDEN_C = 96;
	gen::NkDiffusion modelC(2u, TIME_EMBED_C, HIDDEN_C, 777u, NUM_CLASSES);
	NkVector<NkVar> paramsC;
	modelC.Parameters(paramsC);

	const uint32 NSAMPLE_C = 150;

	logger.Info("--- AVANT entrainement (poids aleatoires) : matrice NN genere(classe) vs train(classe) ---");
	for (uint32 c = 0; c < NUM_CLASSES; ++c) {
		NkVector<uint32> condIdx((NkVector<uint32>::SizeType)NSAMPLE_C, c);
		NkTensor s = modelC.Sample(NSAMPLE_C, schedC, 999u + c, condIdx.Data());
		for (uint32 t = 0; t < NUM_CLASSES; ++t) {
			double d = MeanNearestNeighborDistND(s, *trainSets[t], 2);
			logger.Info("  genere(label={0}) vs train({1}) : NN moyen = {2}", classNames[c], classNames[t], d);
		}
	}

	optim::NkAdam adamC(paramsC, 0.001f);
	const uint32 EPOCHS_C = 6000;
	const uint32 BATCH_C = 64;
	double lastLossC = 0.0;

	for (uint32 e = 0; e < EPOCHS_C; ++e) {
		NkTensor x0Batch = NkTensor::Zeros(NkShape{(int64)BATCH_C, 2});
		float *xp = x0Batch.DataAs<float>();
		NkVector<uint32> tIdx((NkVector<uint32>::SizeType)BATCH_C, 0u);
		NkVector<uint32> clsIdx((NkVector<uint32>::SizeType)BATCH_C, 0u);
		for (uint32 b = 0; b < BATCH_C; ++b) {
			uint32 c = (uint32)(RandUnit(rngC) * (float)NUM_CLASSES);
			if (c >= NUM_CLASSES)
				c = NUM_CLASSES - 1;
			const float *raw = trainSets[c]->DataAs<float>();
			uint32 idx = (uint32)(RandUnit(rngC) * (float)NCLS);
			if (idx >= NCLS)
				idx = NCLS - 1;
			xp[b * 2 + 0] = raw[idx * 2 + 0];
			xp[b * 2 + 1] = raw[idx * 2 + 1];
			uint32 t = (uint32)(RandUnit(rngC) * (float)T);
			if (t >= T)
				t = T - 1;
			tIdx[b] = t;
			clsIdx[b] = c;
		}
		NkTensor eps = NkTensor::Zeros(NkShape{(int64)BATCH_C, 2});
		FillGaussianLocal(eps, rngC);
		NkTensor xt = gen::NkDiffusionForward(x0Batch, schedC, tIdx.Data(), eps);
		NkTensor modelIn = modelC.BuildModelInput(xt, tIdx.Data(), clsIdx.Data());

		adamC.ZeroGrad(); // Backward() n'efface plus les feuilles : un pas = un gradient
		NkVar xin = NkVar::Leaf(modelIn, false);
		NkVar epsTarget = NkVar::Leaf(eps, false);
		NkVar epsPred = modelC.PredictNoise(xin);
		NkVar loss = autograd::MSE(epsPred, epsTarget);
		loss.Backward();
		adamC.Step();

		lastLossC = loss.Value().GetItem(NkShape{(int64)0});
		if (e % 1000 == 0)
			logger.Info("  epoque {0} : perte L2 (conditionne) = {1}", e, lastLossC);
	}
	logger.Info("Entrainement conditionne termine ({0} epoques) : perte finale = {1}", EPOCHS_C, lastLossC);

	logger.Info("--- APRES entrainement : matrice NN genere(classe) vs train(classe) ---");
	double nnMatrix[NUM_CLASSES][NUM_CLASSES];
	for (uint32 c = 0; c < NUM_CLASSES; ++c) {
		NkVector<uint32> condIdx((NkVector<uint32>::SizeType)NSAMPLE_C, c);
		NkTensor s = modelC.Sample(NSAMPLE_C, schedC, 999u + c, condIdx.Data());
		for (uint32 t = 0; t < NUM_CLASSES; ++t) {
			nnMatrix[c][t] = MeanNearestNeighborDistND(s, *trainSets[t], 2);
			logger.Info("  genere(label={0}) vs train({1}) : NN moyen = {2}", classNames[c], classNames[t],
						nnMatrix[c][t]);
		}
	}

	int passC = 0, failC = 0;
	auto checkC = [&](bool ok, const char *name, const char *cls) {
		(ok ? passC : failC)++;
		logger.Info("[{0}] {1} (label={2})", ok ? "OK" : "KO", name, cls);
	};
	for (uint32 c = 0; c < NUM_CLASSES; ++c) {
		bool isRowMin = true;
		for (uint32 t = 0; t < NUM_CLASSES; ++t)
			if (t != c && nnMatrix[c][t] < nnMatrix[c][c])
				isRowMin = false;
		checkC(isRowMin, "distance NN minimale = classe demandee (le label pilote bien la forme)", classNames[c]);
		checkC(nnMatrix[c][c] < 0.5, "distance NN intra-classe < 0.5 (forme proche du train de sa classe)",
			   classNames[c]);
	}
	logger.Info("=== Resultat conditionnement : {0} OK, {1} echec(s) ===", passC, failC);

	// =========================================================================
	// SECTION 3 -- Extension 3D (nuage de points sur une sphere), diffusion
	// INCONDITIONNELLE (meme gen::NkDiffusion, dataDim=3 au lieu de 2 -- la
	// classe est deja generique sur dataDim, cf NkDiffusion.h). Prouve que le
	// pipeline generalise au-dela de 2D.
	// AVERTISSEMENT (honnetete) : ceci est un NUAGE DE POINTS BRUT (positions
	// 3D independantes), PAS un maillage -- aucune topologie/face, aucune
	// extraction de surface (marching cubes NON traite dans cette section).
	// =========================================================================
	logger.Info("=== SECTION 3 : extension 3D -- nuage de points sur une SPHERE (inconditionnel) ===");
	logger.Info("AVERTISSEMENT : nuage de points BRUT, PAS un maillage (pas de topologie/faces, pas d'extraction de "
				"surface ici).");

	uint32 rng3 = 9001u;
	const uint32 N3 = 300;
	const float RADIUS3 = 2.0f;
	const float JITTER3 = 0.08f;
	NkTensor X0sphere = MakeSphereDataset(N3, RADIUS3, JITTER3, rng3);

	double trainMeanR3 = 0.0, trainStdR3 = 0.0;
	RadiusStatsND(X0sphere, 3, trainMeanR3, trainStdR3);
	logger.Info("Jeu d'entrainement 3D : {0} points sur une sphere (rayon cible {1}, jitter {2}) -> rayon mesure : "
				"mean={3} std={4}",
				N3, RADIUS3, JITTER3, trainMeanR3, trainStdR3);

	gen::NkDiffusionSchedule sched3 = gen::NkDiffusionSchedule::Linear(T);

	const uint32 TIME_EMBED_3 = 8;
	const uint32 HIDDEN_3 = 64;
	gen::NkDiffusion model3(3u, TIME_EMBED_3, HIDDEN_3, 3131u); // numClasses=0 (defaut) : inconditionnel
	NkVector<NkVar> params3;
	model3.Parameters(params3);

	const uint32 NSAMPLE3 = 200;
	NkTensor sampleBefore3 = model3.Sample(NSAMPLE3, sched3, 999u);
	double beforeMeanR3 = 0.0, beforeStdR3 = 0.0;
	RadiusStatsND(sampleBefore3, 3, beforeMeanR3, beforeStdR3);
	double beforeNN3 = MeanNearestNeighborDistND(sampleBefore3, X0sphere, 3);
	logger.Info(
		"AVANT entrainement (3D) : dist. moyenne au plus proche voisin = {0} ; rayon genere mean={1} std={2}",
		beforeNN3, beforeMeanR3, beforeStdR3);

	optim::NkAdam adam3(params3, 0.001f);
	const uint32 EPOCHS3 = 4000;
	const uint32 BATCH3 = 64;
	const float *x0Raw3 = X0sphere.DataAs<float>();
	double lastLoss3 = 0.0;

	for (uint32 e = 0; e < EPOCHS3; ++e) {
		NkTensor x0Batch = NkTensor::Zeros(NkShape{(int64)BATCH3, 3});
		float *xp = x0Batch.DataAs<float>();
		NkVector<uint32> tIdx((NkVector<uint32>::SizeType)BATCH3, 0u);
		for (uint32 b = 0; b < BATCH3; ++b) {
			uint32 idx = (uint32)(RandUnit(rng3) * (float)N3);
			if (idx >= N3)
				idx = N3 - 1;
			xp[b * 3 + 0] = x0Raw3[idx * 3 + 0];
			xp[b * 3 + 1] = x0Raw3[idx * 3 + 1];
			xp[b * 3 + 2] = x0Raw3[idx * 3 + 2];
			uint32 t = (uint32)(RandUnit(rng3) * (float)T);
			if (t >= T)
				t = T - 1;
			tIdx[b] = t;
		}
		NkTensor eps = NkTensor::Zeros(NkShape{(int64)BATCH3, 3});
		FillGaussianLocal(eps, rng3);
		NkTensor xt = gen::NkDiffusionForward(x0Batch, sched3, tIdx.Data(), eps);
		NkTensor modelIn = model3.BuildModelInput(xt, tIdx.Data());

		adam3.ZeroGrad(); // Backward() n'efface plus les feuilles : un pas = un gradient
		NkVar xin = NkVar::Leaf(modelIn, false);
		NkVar epsTarget = NkVar::Leaf(eps, false);
		NkVar epsPred = model3.PredictNoise(xin);
		NkVar loss = autograd::MSE(epsPred, epsTarget);
		loss.Backward();
		adam3.Step();

		lastLoss3 = loss.Value().GetItem(NkShape{(int64)0});
		if (e % 500 == 0)
			logger.Info("  epoque {0} : perte L2 (3D) = {1}", e, lastLoss3);
	}
	logger.Info("Entrainement 3D termine ({0} epoques) : perte finale = {1}", EPOCHS3, lastLoss3);

	NkTensor sampleAfter3 = model3.Sample(NSAMPLE3, sched3, 999u);
	double afterMeanR3 = 0.0, afterStdR3 = 0.0;
	RadiusStatsND(sampleAfter3, 3, afterMeanR3, afterStdR3);
	double afterNN3 = MeanNearestNeighborDistND(sampleAfter3, X0sphere, 3);
	logger.Info(
		"APRES entrainement (3D) : dist. moyenne au plus proche voisin = {0} ; rayon genere mean={1} std={2}",
		afterNN3, afterMeanR3, afterStdR3);

	int pass3 = 0, fail3 = 0;
	auto check3 = [&](bool ok, const char *name) {
		(ok ? pass3 : fail3)++;
		logger.Info("[{0}] {1}", ok ? "OK" : "KO", name);
	};
	check3(afterNN3 < beforeNN3, "3D : distance au plus proche voisin diminue apres entrainement");
	check3(afterNN3 < 0.5, "3D : distance au plus proche voisin < 0.5 apres entrainement (proche de la sphere reelle)");
	check3(std::fabs(afterMeanR3 - trainMeanR3) < 0.5, "3D : rayon genere moyen proche du rayon d'entrainement (+/-0.5)");
	check3(afterStdR3 < trainStdR3 * 4.0 + 0.3, "3D : dispersion du rayon genere raisonnable (pas d'explosion)");
	logger.Info("=== Resultat 3D : {0} OK, {1} echec(s) ===", pass3, fail3);

	int totalPass = pass + passC + pass3;
	int totalFail = fail + failC + fail3;
	logger.Info("=== BILAN GLOBAL : {0} OK, {1} echec(s) au total (2D inconditionnel {2}KO, conditionnement {3}KO, "
				"3D {4}KO) ===",
				totalPass, totalFail, fail, failC, fail3);
	return totalFail == 0 ? 0 : 1;
}
