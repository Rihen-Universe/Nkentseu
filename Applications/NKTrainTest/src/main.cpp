// =============================================================================
// NKTrainTest — entraînement de bout en bout via NKTrain.
//   Modèle Dense(2->32)->relu->Dense(32->4) entraîné sur 4 amas 2D (NKData) avec
//   Adam (NKOptim) + entropie croisée (NKNN), boucle TrainEpoch (NKTrain).
//   Suivi perte + exactitude par époque ; exactitude sur un jeu de TEST séparé.
//   Section MNIST optionnelle (NK_MNIST_DIR).
// =============================================================================
#include "NKTrain/NkTrain.h"
#include "NKNN/NkNN.h"
#include "NKOptim/NkOptim.h"
#include "NKData/NkData.h"
#include "NKAutograd/NkVar.h"
#include "NKTensor/NkTensor.h"
#include "NKLogger/NkLog.h"

#include <cstdio>
#include <cstdlib>

using namespace nkentseu;
using namespace nkentseu::ai;

int main() {
	printf("=== NKTrainTest : entraînement de bout en bout (NKTrain) ===\n\n");

	// Jeux train (4x40=160) et test (4x15=60), amas 2D, graines différentes.
	const uint32 NC = 4;
	data::NkDataset train = data::MakeBlobs(NC, 40, 111u);
	data::NkDataset test = data::MakeBlobs(NC, 15, 999u);
	data::NkDataLoader trainLoader(train, 32, /*shuffle*/ true, 7u);
	data::NkDataLoader testLoader(test, 32, /*shuffle*/ false, 1u);

	// Modèle : Dense(2->32) -> relu -> Dense(32->4) (logits).
	nn::NkDense l1(2, 32, 1234u);
	nn::NkDense l2(32, NC, 5678u);
	auto forward = [&](const NkVar &x) { return l2.Forward(nn::Relu(l1.Forward(x))); };

	NkVector<NkVar> params;
	l1.Parameters(params);
	l2.Parameters(params);
	optim::NkAdam adam(params, /*lr*/ 0.01f);

	printf("-- Entraînement (4 classes, 160 exemples, Adam+CE) --\n");
	train::EpochStats st;
	for (int e = 1; e <= 60; ++e) {
		st = train::TrainEpoch(forward, adam, trainLoader);
		if (e % 10 == 0 || e == 1)
			printf("  époque %3d : perte = %.5f  exactitude train = %.1f%%\n", e, st.loss, st.acc * 100.0);
	}

	const double testAcc = train::Accuracy(forward, testLoader);
	printf("\n  exactitude finale : train = %.1f%%  |  TEST (jamais vu) = %.1f%%\n", st.acc * 100.0, testAcc * 100.0);

	int pass = 0, fail = 0;
	bool trainOk = st.acc >= 0.98;
	bool testOk = testAcc >= 0.90;
	(trainOk ? pass : fail)++;
	(testOk ? pass : fail)++;
	printf("  [ %s ] convergence entraînement (>=98%%)\n", trainOk ? "OK" : "KO");
	printf("  [ %s ] généralisation test (>=90%%)\n", testOk ? "OK" : "KO");

	// ------------------------------------------------------------------
	// Utilitaires réutilisables (Option A.1) : planificateur LR + époque
	// avec accumulation de gradient + validation forward-only.
	// ------------------------------------------------------------------
	printf("\n-- Utilitaires réutilisables (scheduler LR, accumulation, validation) --\n");

	// (a) Auto-test analytique du planificateur LR (warmup, pic, plancher, monotonie).
	const bool schedOk = train::SelfTest();
	(schedOk ? pass : fail)++;
	printf("  [ %s ] planificateur LR (warmup linéaire + cosine + plancher)\n", schedOk ? "OK" : "KO");

	// (b) Entraînement d'un modèle NEUF via TrainEpochAccum (accum=2) piloté par un
	//     scheduler LR global (warmup court + décroissance cosine). Doit converger.
	nn::NkDense a1(2, 32, 4321u);
	nn::NkDense a2(32, NC, 8765u);
	auto afwd = [&](const NkVar &x) { return a2.Forward(nn::Relu(a1.Forward(x))); };
	NkVector<NkVar> aparams;
	a1.Parameters(aparams);
	a2.Parameters(aparams);
	optim::NkAdam aopt(aparams, /*lr*/ 0.01f);

	const int ACC_EPOCHS = 60;
	train::NkLRSchedule sched;
	sched.peakLr = 0.01f;
	sched.warmupSteps = trainLoader.NumBatches() * 3; // ~3 époques de warmup
	sched.totalSteps = trainLoader.NumBatches() * ACC_EPOCHS;
	sched.minLrRatio = 0.1;
	int64 gstep = 0;

	train::EpochStats ast;
	double valLoss = 0.0;
	for (int e = 1; e <= ACC_EPOCHS; ++e) {
		ast = train::TrainEpochAccum(afwd, aopt, trainLoader, /*accum*/ 2, &sched, &gstep);
		if (e % 15 == 0 || e == 1) {
			valLoss = train::EvalLoss(afwd, testLoader);
			printf("  époque %3d : perte train = %.5f  exactitude = %.1f%%  perte val = %.5f  lr = %.5f\n",
				   e, ast.loss, ast.acc * 100.0, valLoss, (double)sched.LrAt(gstep));
		}
	}
	const double accTestAcc = train::Accuracy(afwd, testLoader);
	const bool accOk = ast.acc >= 0.95 && accTestAcc >= 0.85;
	(accOk ? pass : fail)++;
	printf("  [ %s ] convergence avec accumulation+scheduler (train %.1f%% / test %.1f%%)\n",
		   accOk ? "OK" : "KO", ast.acc * 100.0, accTestAcc * 100.0);

	// ------------------------------------------------------------------
	// MNIST (optionnel) : entraînement réel si NK_MNIST_DIR est défini.
	// ------------------------------------------------------------------
	printf("\n-- MNIST (optionnel) --\n");
	const char *dir = getenv("NK_MNIST_DIR");
	if (!dir || !*dir) {
		printf("  (NK_MNIST_DIR non défini : section MNIST ignorée)\n");
	} else {
		char img[1024], lbl[1024];
		snprintf(img, sizeof(img), "%s/train-images-idx3-ubyte", dir);
		snprintf(lbl, sizeof(lbl), "%s/train-labels-idx1-ubyte", dir);
		data::NkDataset mnist = data::LoadMnist(img, lbl);
		if (!mnist.IsValid()) {
			printf("  MNIST introuvable dans %s (section ignorée)\n", dir);
		} else {
			printf("  MNIST : %u exemples ; entraînement d'un MLP 784->64->10 (Adam+CE)...\n", mnist.Size());
			data::NkDataLoader ml(mnist, 64, true, 3u);
			nn::NkDense m1(784, 64, 11u);
			nn::NkDense m2(64, 10, 22u);
			auto mfwd = [&](const NkVar &x) { return m2.Forward(nn::Relu(m1.Forward(x))); };
			NkVector<NkVar> mp;
			m1.Parameters(mp);
			m2.Parameters(mp);
			optim::NkAdam mopt(mp, 0.001f);
			for (int e = 1; e <= 3; ++e) {
				train::EpochStats ms = train::TrainEpoch(mfwd, mopt, ml);
				printf("    époque %d : perte = %.4f  exactitude = %.1f%%\n", e, ms.loss, ms.acc * 100.0);
			}
		}
	}

	// ------------------------------------------------------------------
	// Jalon 2/3 (2026-07-26) : callbacks génériques + checkpoint modèle+optimiseur
	// généralisé + reprise après interruption. Nouveau : NKTrain/NkCallback.h +
	// NKTrain/NkCheckpoint.h + train::Fit (boucle pilotée par callbacks).
	// ------------------------------------------------------------------
	logger.Info("");
	logger.Info("-- Callbacks (Jalon 3) : early stopping (logique pure, déterministe) --");
	{
		// Suite de pertes de validation SYNTHÉTIQUE et connue : améliore nettement deux
		// fois (delta > minDelta) puis stagne (delta < minDelta) -> doit stopper quand le
		// nombre d'époques SANS amélioration atteint `patience`.
		train::NkEarlyStopping es(/*patience*/ 3, /*minDelta*/ 0.01);
		train::EpochStats dummy;
		const double vals[7] = {1.0, 0.5, 0.2, 0.199, 0.198, 0.197, 0.196};
		int64 stopEpoch = -1;
		for (int64 e = 1; e <= 7; ++e) {
			es.OnEpochEnd(e, dummy, vals[e - 1]);
			if (es.StopRequested() && stopEpoch < 0)
				stopEpoch = e;
		}
		// e1->e2 (1.0->0.5) et e2->e3 (0.5->0.2) améliorent (> minDelta) : bad reset à 0.
		// e3->e4/e5/e6 : delta ~0.001 < minDelta -> bad=1,2,3 -> stop dès bad>=patience=3 (époque 6).
		const bool ok = (stopEpoch == 6) && (es.BadEpochs() >= 3);
		(ok ? pass : fail)++;
		logger.Info("  [ {0} ] NkEarlyStopping s'arrête à l'époque {1} (attendu 6) après {2} époques sans "
					"amélioration (patience=3)",
					ok ? "OK" : "KO", stopEpoch, es.BadEpochs());
	}

	logger.Info("-- Callbacks (Jalon 3) : early stopping arrête RÉELLEMENT la boucle Fit() --");
	{
		nn::NkDense e1(2, 16, 111u);
		nn::NkDense e2(16, NC, 222u);
		auto efwd = [&](const NkVar &x) { return e2.Forward(nn::Relu(e1.Forward(x))); };
		NkVector<NkVar> eparams;
		e1.Parameters(eparams);
		e2.Parameters(eparams);
		optim::NkAdam eopt(eparams, 0.01f);
		// minDelta astronomique -> AUCUNE amélioration ne peut jamais être reconnue après la
		// 1ère époque (qui initialise `best`) : arrêt garanti et déterministe à l'époque
		// (1 + patience), quelle que soit la valeur réelle de la perte.
		train::NkEarlyStopping esCb(/*patience*/ 2, /*minDelta*/ 1.0e9);
		NkVector<train::NkTrainCallback *> cbs;
		cbs.PushBack(&esCb);
		data::NkDataLoader trainLoader2(train, 32, true, 21u);
		data::NkDataLoader valLoader2(test, 32, false, 1u);
		train::EpochStats fitStats = train::Fit(efwd, eopt, trainLoader2, &valLoader2, /*fromEpoch*/ 1,
												/*toEpoch*/ 50, cbs);
		(void)fitStats;
		const bool stoppedEarly = esCb.StopRequested() && esCb.StoppedEpoch() == 3;
		(stoppedEarly ? pass : fail)++;
		logger.Info("  [ {0} ] Fit() honore StopRequested() : arrêté à l'époque {1}/50 (attendu 3)",
					stoppedEarly ? "OK" : "KO", esCb.StoppedEpoch());
	}

	logger.Info("-- Callbacks (Jalon 3) : planificateur LR cosine RÉEL (pilote Adam via callback) --");
	{
		nn::NkDense l1c(2, 32, 31u);
		nn::NkDense l2c(32, NC, 32u);
		auto lfwd = [&](const NkVar &x) { return l2c.Forward(nn::Relu(l1c.Forward(x))); };
		NkVector<NkVar> lparams;
		l1c.Parameters(lparams);
		l2c.Parameters(lparams);
		optim::NkAdam lopt(lparams, 0.0f); // LR initial sans importance : écrasé par la callback
		data::NkDataLoader lLoader(train, 32, true, 41u);
		train::NkLRSchedule sched;
		sched.peakLr = 0.02f;
		sched.warmupSteps = lLoader.NumBatches() * 2;
		sched.totalSteps = lLoader.NumBatches() * 30;
		sched.minLrRatio = 0.1;
		train::NkLRSchedulerCallback<optim::NkAdam> lrCb(lopt, sched);
		train::NkLoggingCallback logCb(10); // démontre la callback de journalisation (NKLogger)
		NkVector<train::NkTrainCallback *> lcbs;
		lcbs.PushBack(&lrCb);
		lcbs.PushBack(&logCb);
		int64 lgstep = 0;
		train::Fit(lfwd, lopt, lLoader, nullptr, 1, 30, lcbs, &lgstep);
		const float expectedLr = sched.LrAt(lgstep);
		const float actualLr = lopt.LearningRate();
		const bool lrOk = (expectedLr - actualLr < 1e-6f) && (actualLr - expectedLr < 1e-6f);
		(lrOk ? pass : fail)++;
		logger.Info("  [ {0} ] NkLRSchedulerCallback pilote réellement Adam.SetLearningRate : lr final = {1} "
					"(attendu {2})",
					lrOk ? "OK" : "KO", (double)actualLr, (double)expectedLr);
	}

	logger.Info("-- Callbacks (Jalon 3) : décroissance PAR PALIERS (alternative au cosine) --");
	{
		nn::NkDense d1s(2, 32, 51u);
		nn::NkDense d2s(32, NC, 52u);
		auto dfwd = [&](const NkVar &x) { return d2s.Forward(nn::Relu(d1s.Forward(x))); };
		NkVector<NkVar> dparams;
		d1s.Parameters(dparams);
		d2s.Parameters(dparams);
		optim::NkAdam dopt(dparams, 0.0f);
		data::NkDataLoader dLoader(train, 32, true, 61u);
		train::NkStepDecaySchedule stepSched;
		stepSched.peakLr = 0.01f;
		stepSched.stepSize = 5;
		stepSched.decayRate = 0.5;
		train::NkStepDecayCallback<optim::NkAdam> stepCb(dopt, stepSched);
		NkVector<train::NkTrainCallback *> dcbs;
		dcbs.PushBack(&stepCb);
		const int64 STEP_EPOCHS = 12;
		train::Fit(dfwd, dopt, dLoader, nullptr, 1, STEP_EPOCHS, dcbs);
		const float expectedLr = stepSched.LrAt(STEP_EPOCHS);
		const float actualLr = dopt.LearningRate();
		const bool stepOk = (expectedLr - actualLr < 1e-9f) && (actualLr - expectedLr < 1e-9f);
		(stepOk ? pass : fail)++;
		logger.Info("  [ {0} ] NkStepDecayCallback : lr(époque {1}) = {2} (attendu {3} = {4}*{5}^{6})",
					stepOk ? "OK" : "KO", STEP_EPOCHS, (double)actualLr, (double)expectedLr,
					(double)stepSched.peakLr, stepSched.decayRate, (STEP_EPOCHS - 1) / stepSched.stepSize);
	}

	// ------------------------------------------------------------------
	// Checkpoint GÉNÉRIQUE (modèle + optimiseur Adam + état de boucle) : généralise
	// dans NKTrain ce qui n'existait QUE dans NkGptTrainer (format « NKGP » v4).
	// ------------------------------------------------------------------
	logger.Info("-- Checkpoint générique (NKTrain/NkCheckpoint.h) : poids + Adam + état, reprise EXACTE --");
	{
		nn::NkDense c1(2, 32, 55u);
		nn::NkDense c2(32, NC, 66u);
		auto cfwd = [&](const NkVar &x) { return c2.Forward(nn::Relu(c1.Forward(x))); };
		NkVector<NkVar> paramsA;
		c1.Parameters(paramsA);
		c2.Parameters(paramsA);
		optim::NkAdam optA(paramsA, 0.01f);
		data::NkDataLoader ckptLoader(train, 32, true, 909u);
		for (int e = 1; e <= 8; ++e) // état Adam non trivial (m/v/step != 0) avant sauvegarde
			train::TrainEpoch(cfwd, optA, ckptLoader);

		train::NkTrainState stA;
		stA.epoch = 8;
		stA.globalStep = 8 * (int64)ckptLoader.NumBatches();
		stA.bestMetric = 0.1234;
		stA.badEpochs = 2;
		const char *ckpath = "Build/nktrain_ckpt_test.nktc";
		const bool savedOk = train::SaveCheckpoint(ckpath, paramsA, &optA, &stA);
		(savedOk ? pass : fail)++;
		logger.Info("  [ {0} ] SaveCheckpoint (poids+Adam+état, {1} paramètres) -> {2}", savedOk ? "OK" : "KO",
					paramsA.Size(), ckpath);

		// B : même architecture, poids INITIAUX DIFFÉRENTS (seeds différentes) -> écrasés au
		// chargement. Prouve que LoadCheckpointWeights ne dépend pas de l'init de départ.
		nn::NkDense b1(2, 32, 111u);
		nn::NkDense b2(32, NC, 222u);
		NkVector<NkVar> paramsB;
		b1.Parameters(paramsB);
		b2.Parameters(paramsB);
		auto bfwd = [&](const NkVar &x) { return b2.Forward(nn::Relu(b1.Forward(x))); };

		const bool wOk = train::LoadCheckpointWeights(ckpath, paramsB);
		bool weightsMatch = wOk;
		for (uint32 i = 0; weightsMatch && i < paramsA.Size(); ++i) {
			NkTensor ta = paramsA[i].Value().ToCPU().Contiguous();
			NkTensor tb = paramsB[i].Value().ToCPU().Contiguous();
			const NkShape &sh = ta.Shape();
			int64 n = 1;
			for (uint32 d = 0; d < sh.Size(); ++d)
				n *= sh[d];
			const float *pa = ta.DataAs<float>();
			const float *pb = tb.DataAs<float>();
			for (int64 k = 0; k < n; ++k)
				if (pa[k] != pb[k]) {
					weightsMatch = false;
					break;
				}
		}
		(weightsMatch ? pass : fail)++;
		logger.Info("  [ {0} ] LoadCheckpointWeights : poids B == poids A, octet pour octet", weightsMatch ? "OK" : "KO");

		optim::NkAdam optB(paramsB, 0.01f);
		const bool optOk = train::LoadCheckpointOptState(ckpath, paramsB, optB);
		const bool stepMatch = optOk && optB.StepCount() == optA.StepCount();
		(stepMatch ? pass : fail)++;
		logger.Info("  [ {0} ] LoadCheckpointOptState : pas Adam repris = {1} (attendu {2})",
					stepMatch ? "OK" : "KO", optB.StepCount(), optA.StepCount());

		train::NkTrainState stB;
		const bool stOk = train::LoadCheckpointTrainState(ckpath, stB);
		const double dMet = stB.bestMetric - stA.bestMetric;
		const bool stMatch = stOk && stB.epoch == stA.epoch && stB.globalStep == stA.globalStep &&
							 stB.badEpochs == stA.badEpochs && dMet > -1e-9 && dMet < 1e-9;
		(stMatch ? pass : fail)++;
		logger.Info("  [ {0} ] LoadCheckpointTrainState : époque={1} pas={2} meilleure={3} patience={4}",
					stMatch ? "OK" : "KO", stB.epoch, stB.globalStep, stB.bestMetric, stB.badEpochs);

		// Reprise EXACTE : UN pas Adam identique (même lot fixe) sur A (continué en mémoire) et
		// B (repris depuis le checkpoint) doit produire des poids IDENTIQUES -> preuve que les
		// moments m/v + le compteur de pas comptent réellement dans la mise à jour.
		data::NkBatch fixedBatch = ckptLoader.GetBatch(0);
		NkVar xa = NkVar::Leaf(fixedBatch.inputs, false);
		NkVar ya = NkVar::Leaf(fixedBatch.targets, false);
		NkVar lossA = nn::CrossEntropyLoss(cfwd(xa), ya);
		lossA.Backward();
		optA.Step();

		NkVar xb = NkVar::Leaf(fixedBatch.inputs, false);
		NkVar yb = NkVar::Leaf(fixedBatch.targets, false);
		NkVar lossB = nn::CrossEntropyLoss(bfwd(xb), yb);
		lossB.Backward();
		optB.Step();

		double maxDiff = 0.0;
		for (uint32 i = 0; i < paramsA.Size(); ++i) {
			NkTensor ta2 = paramsA[i].Value().ToCPU().Contiguous();
			NkTensor tb2 = paramsB[i].Value().ToCPU().Contiguous();
			const NkShape &sh = ta2.Shape();
			int64 n = 1;
			for (uint32 d = 0; d < sh.Size(); ++d)
				n *= sh[d];
			const float *pa = ta2.DataAs<float>();
			const float *pb = tb2.DataAs<float>();
			for (int64 k = 0; k < n; ++k) {
				double d2 = pa[k] - pb[k];
				if (d2 < 0)
					d2 = -d2;
				if (d2 > maxDiff)
					maxDiff = d2;
			}
		}
		const bool resumeExact = maxDiff < 1e-5;
		(resumeExact ? pass : fail)++;
		logger.Info("  [ {0} ] reprise EXACTE : 1 pas Adam identique A(continué) vs B(repris) -> écart max = {1}",
					resumeExact ? "OK" : "KO", maxDiff);
	}

	// ------------------------------------------------------------------
	// Reprise après INTERRUPTION au niveau de la boucle Fit() elle-même :
	// entraîne 1..5, checkpoint (poids+Adam+état), reconstruit tout depuis zéro
	// ("interruption" simulée), recharge, restaure l'état d'early-stopping, puis
	// reprend 6..12 SANS re-warmup ni perte de compteur de patience.
	// ------------------------------------------------------------------
	logger.Info("-- Reprise après interruption (Fit() : fromEpoch/toEpoch + état restauré) --");
	{
		nn::NkDense r1(2, 32, 777u);
		nn::NkDense r2(32, NC, 888u);
		auto rfwd = [&](const NkVar &x) { return r2.Forward(nn::Relu(r1.Forward(x))); };
		NkVector<NkVar> rparams;
		r1.Parameters(rparams);
		r2.Parameters(rparams);
		optim::NkAdam ropt(rparams, 0.01f);
		data::NkDataLoader rLoader(train, 32, true, 555u);
		data::NkDataLoader rVal(test, 32, false, 1u);
		train::NkEarlyStopping resEs(10, 1e-5); // patience large : ne doit PAS interrompre ici
		NkVector<train::NkTrainCallback *> rcbs;
		rcbs.PushBack(&resEs);
		int64 rgstep = 0;
		train::Fit(rfwd, ropt, rLoader, &rVal, 1, 5, rcbs, &rgstep); // "avant interruption"

		train::NkTrainState rst;
		rst.epoch = 5;
		rst.globalStep = rgstep;
		rst.bestMetric = resEs.Best();
		rst.badEpochs = resEs.BadEpochs();
		const char *rpath = "Build/nktrain_resume_test.nktc";
		train::SaveCheckpoint(rpath, rparams, &ropt, &rst);

		// "Interruption" simulée : tout reconstruit depuis zéro (autres seeds -> écrasées).
		nn::NkDense s1(2, 32, 1u);
		nn::NkDense s2(32, NC, 2u);
		auto sfwd = [&](const NkVar &x) { return s2.Forward(nn::Relu(s1.Forward(x))); };
		NkVector<NkVar> sparams;
		s1.Parameters(sparams);
		s2.Parameters(sparams);
		train::LoadCheckpointWeights(rpath, sparams);
		optim::NkAdam sopt(sparams, 0.01f);
		train::LoadCheckpointOptState(rpath, sparams, sopt);
		train::NkTrainState rst2;
		train::LoadCheckpointTrainState(rpath, rst2);
		train::NkEarlyStopping resEs2(10, 1e-5);
		resEs2.RestoreState(rst2);
		NkVector<train::NkTrainCallback *> scbs;
		scbs.PushBack(&resEs2);
		int64 sgstep = rst2.globalStep;
		train::EpochStats resumed = train::Fit(sfwd, sopt, rLoader, &rVal, rst2.epoch + 1, 12, scbs, &sgstep);

		const bool stateOk = rst2.epoch == 5 && rst2.globalStep == rgstep;
		const bool resumeRan = resumed.acc > 0.5; // la reprise doit continuer à bien apprendre
		(stateOk && resumeRan ? pass : fail)++;
		logger.Info("  [ {0} ] reprise Fit() après interruption : redémarre à l'époque {1}, exactitude finale = "
					"{2}% (état repris : pas={3}, meilleure={4})",
					(stateOk && resumeRan) ? "OK" : "KO", rst2.epoch + 1, resumed.acc * 100.0, rst2.globalStep,
					rst2.bestMetric);
	}

	logger.Info("");
	logger.Info("=== Résultat : {0} OK, {1} échec(s) ===", pass, fail);
	printf("\n=== Résultat : %d OK, %d échec(s) ===\n", pass, fail);
	return fail == 0 ? 0 : 1;
}
