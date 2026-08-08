// =============================================================================
// NKQwen2Train — entraînement LoRA REPRENABLE du 7B, sur GPU.
//
// POURQUOI CETTE APPLICATION EXISTE
// ---------------------------------
// Le jalon 6 a prouvé que l'entraînement fonctionne (gradient vérifié par
// différences finies, perte de validation qui baisse). Mais il l'a prouvé sur
// 200 pas, dans un TEST : tout y est en dur, et rien ne survit à une coupure.
// Or une époque sur les 100 017 paires demande ~10,6 jours à 9,15 s/pas. Sur
// une telle durée, une coupure de courant, un redémarrage Windows ou un
// plantage n'est pas une éventualité : c'est une CERTITUDE.
//
// D'où les deux propriétés qui gouvernent tout ce fichier :
//
//   1. REPRENDRE EXACTEMENT — pas seulement recharger les poids. Un checkpoint
//      porte les deux moments d'Adam, le compteur de pas, la position dans le
//      corpus et l'état du générateur aléatoire (format NKLA v2). Reprendre
//      sans les moments ferait diverger l'entraînement au premier pas : Adam
//      corrige le biais par b1ᵗ et b2ᵗ, et repartir de t = 0 applique la
//      correction du tout premier pas à des moments déjà grands.
//
//   2. ÉCRIRE SANS JAMAIS DÉTRUIRE — un checkpoint fait ~230 Mo et son écriture
//      dure plusieurs secondes. Une coupure PENDANT l'écriture ne doit pas
//      emporter le checkpoint précédent. On écrit donc dans un fichier
//      temporaire, on le renomme une fois complet, et on ALTERNE entre deux
//      fichiers : même un renommage interrompu laisse une génération valide.
//
// TOUT SE PILOTE PAR ARGUMENTS, jamais par variables d'environnement (règle du
// dépôt : le harnais NK_* est une dette). Changer de corpus, de taux
// d'apprentissage ou de durée ne demande AUCUNE recompilation.
//
// Usage :
//   NKQwen2Train --gguf=<blob.gguf> --corpus=<corpus.txt> [options]
//
//   --out=<prefixe>     préfixe des checkpoints (défaut « qwen2_lora »)
//   --lr=<f>            taux d'apprentissage (défaut 1e-4)
//   --steps=<n>         pas à effectuer sur CE lancement, 0 = jusqu'à la fin
//                       du corpus (défaut 0)
//   --save-every=<n>    sauvegarde tous les n pas (défaut 200 ≈ 30 min)
//   --rank=<n>          rang LoRA (défaut 8) — doit correspondre à la reprise
//   --max-seq=<n>       longueur max d'un exemple (défaut 128)
//   --valid-every=<n>   validation tous les n pas, 0 = jamais (défaut 500)
//   --fresh             ignorer un checkpoint existant et repartir de zéro
//
// Zéro STL. Portable Windows / Linux / macOS : aucun appel spécifique à une
// plateforme, tout passe par NKFileSystem et NKTensorGpu.
// =============================================================================
#include "NKInfer/NkQwen2LoraGpu.h"
#include "NKInfer/NkLoraGpu.h"
#include "NKInfer/NkQwen2Tokenizer.h"
#include "NKInfer/NkQwen2Sft.h"
#include "NKTensor/NkTensorGpu.h"
#include "NKFileSystem/NkFile.h"
#include "NKTime/NkChrono.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <csignal>

using namespace nkentseu;
using namespace nkentseu::ai;		 // NkTensorGpu
using namespace nkentseu::ai::infer; // le modèle, les adaptateurs, le tokenizer

namespace {

	// ── Arrêt propre ────────────────────────────────────────────────────────────
	// Ctrl+C ne doit pas tuer l'entraînement au milieu d'un pas : on lève un
	// drapeau, la boucle le voit, sauvegarde, et sort. `volatile sig_atomic_t`
	// est le SEUL type dont la norme garantisse la lecture depuis un gestionnaire
	// de signal.
	volatile sig_atomic_t gStopRequested = 0;

	void OnInterrupt(int) {
		gStopRequested = 1;
	}

	// ── Arguments ───────────────────────────────────────────────────────────────
	// « --clef=valeur ». Renvoie nullptr si absent, pour que l'appelant distingue
	// « non fourni » de « fourni vide ».
	const char *ArgValue(int argc, char **argv, const char *key) {
		const usize klen = strlen(key);
		for (int i = 1; i < argc; ++i) {
			if (strncmp(argv[i], key, klen) == 0 && argv[i][klen] == '=')
				return argv[i] + klen + 1;
		}
		return nullptr;
	}

	bool ArgFlag(int argc, char **argv, const char *key) {
		for (int i = 1; i < argc; ++i)
			if (strcmp(argv[i], key) == 0)
				return true;
		return false;
	}

	int32 ArgInt(int argc, char **argv, const char *key, int32 def) {
		const char *v = ArgValue(argc, argv, key);
		return v ? (int32)atoi(v) : def;
	}

	float32 ArgFloat(int argc, char **argv, const char *key, float32 def) {
		const char *v = ArgValue(argc, argv, key);
		return v ? (float32)atof(v) : def;
	}

	// ── Corpus ──────────────────────────────────────────────────────────────────
	struct QaPair {
			NkString q, r;
	};

	// Lit le corpus ENTIER (« Question: … » / « Reponse: … »). Le test n'en lisait
	// que les 4 premiers Mo parce qu'il lui fallait quelques centaines d'exemples ;
	// ici on entraîne sur tout, et 25 Mo de texte tiennent sans peine en mémoire —
	// c'est le modèle de 4 Go en VRAM qui contraint, pas le corpus.
	bool LoadCorpus(const char *path, NkVector<QaPair> &out) {
		NkFile f(path, NkFileMode::NK_READ_BINARY);
		if (!f.IsOpen())
			return false;
		const uint64 size = (uint64)f.Size();
		NkVector<char> buf;
		buf.Resize((NkVector<char>::SizeType)(size + 1));
		const usize got = f.Read(buf.Data(), (usize)size);
		buf[(NkVector<char>::SizeType)got] = 0;

		const char *p = buf.Data();
		const char *end = p + got;
		NkString pendingQ;
		bool haveQ = false;
		NkVector<char> line;
		while (p < end) {
			const char *nl = p;
			while (nl < end && *nl != '\n')
				++nl;
			int64 len = nl - p;
			while (len > 0 && p[len - 1] == '\r')
				--len;
			line.Resize((NkVector<char>::SizeType)(len + 1));
			if (len > 0)
				memcpy(line.Data(), p, (usize)len);
			line[(NkVector<char>::SizeType)len] = 0;
			const char *s = line.Data();
			if (len > 10 && strncmp(s, "Question: ", 10) == 0) {
				pendingQ = NkString(s + 10);
				haveQ = true;
			} else if (len > 9 && strncmp(s, "Reponse: ", 9) == 0 && haveQ) {
				QaPair qa;
				qa.q = pendingQ;
				qa.r = NkString(s + 9);
				out.PushBack(qa);
				haveQ = false;
			}
			p = (nl < end) ? nl + 1 : end;
		}
		return out.Size() > 0;
	}

	// ── Checkpoints ─────────────────────────────────────────────────────────────
	// DEUX fichiers alternés, plus un temporaire. Écrire directement sur le
	// fichier définitif exposerait à perdre les deux : la coupure arrive pendant
	// l'écriture, et il ne reste ni l'ancien ni le nouveau.
	NkString CkptPath(const NkString &prefix, int32 slot) {
		char buf[32];
		snprintf(buf, sizeof(buf), "_ckpt%d.nkla", slot);
		return prefix + NkString(buf);
	}

	// Écrit à côté puis renomme : le renommage est l'opération que les systèmes de
	// fichiers rendent atomique, l'écriture ne l'est jamais.
	bool SaveCheckpointAtomic(const NkString &prefix, int32 slot, NkQwen2LoraGpu &model,
							  const NkLoraTrainState &ts, int32 rank, NkLoraGpuNklaInfo *info,
							  NkString *err) {
		const NkString finalPath = CkptPath(prefix, slot);
		const NkString tmpPath = finalPath + NkString(".tmp");
		// qDim et kvDim ne sont pas des champs de la config : ils se DÉDUISENT du
		// nombre de têtes. Les recalculer ici plutôt que de les stocker évite deux
		// sources pour une même vérité (Qwen2.5 7B : 28 têtes Q, 4 têtes KV).
		const NkQwen2Config &c = model.Config();
		if (!NkLoraGpuSaveNKLA(tmpPath.CStr(), model.Lora(), rank, model.Lora()[0].q.alpha, c.dModel, c.ffnDim,
							   c.nHeads * c.headDim, c.nKVHeads * c.headDim, info, err, &ts))
			return false;
		// Le fichier cible doit disparaître avant le renommage : sous Windows,
		// renommer sur une cible existante échoue.
		if (NkFile::Exists(finalPath.CStr()))
			NkFile::Delete(finalPath.CStr());
		if (!NkFile::Move(tmpPath.CStr(), finalPath.CStr())) {
			if (err)
				*err = NkString("renommage du checkpoint impossible : ") + finalPath;
			return false;
		}
		return true;
	}

	// Reprend le checkpoint le PLUS AVANCÉ des deux qui soit intact. Un fichier
	// corrompu (empreinte FNV fausse) est ignoré au profit de l'autre : c'est
	// exactement ce pour quoi on en garde deux.
	bool TryResume(const NkString &prefix, NkQwen2LoraGpu &model, NkLoraTrainState &ts, int32 &usedSlot) {
		int32 best = -1;
		int64 bestStep = -1;
		NkLoraTrainState bestTs;
		for (int32 s = 0; s < 2; ++s) {
			const NkString p = CkptPath(prefix, s);
			if (!NkFile::Exists(p.CStr()))
				continue;
			NkLoraGpuNklaInfo info;
			NkLoraTrainState cand;
			NkString err;
			if (!NkLoraGpuLoadNKLA(p.CStr(), model.Lora(), &info, &err, &cand)) {
				printf("  checkpoint %s ignoré : %s\n", p.CStr(), err.CStr());
				continue;
			}
			if (!(info.flags & NK_NKLA_TRAIN_STATE)) {
				printf("  %s ne porte pas d'état de reprise (adaptateurs seuls) — ignoré\n", p.CStr());
				continue;
			}
			if (cand.step > bestStep) {
				bestStep = cand.step;
				best = s;
				bestTs = cand;
			}
		}
		if (best < 0)
			return false;
		// Le meilleur a peut-être été écrasé en VRAM par la tentative suivante :
		// on le RECHARGE pour être certain que la mémoire correspond à l'état
		// qu'on annonce reprendre.
		NkLoraGpuNklaInfo info;
		NkString err;
		if (!NkLoraGpuLoadNKLA(CkptPath(prefix, best).CStr(), model.Lora(), &info, &err, &ts))
			return false;
		usedSlot = best;
		(void)bestTs;
		return true;
	}

} // namespace

int main(int argc, char **argv) {
	printf("=== NKQwen2Train — entraînement LoRA reprenable ===\n\n");

	const char *ggufPath = ArgValue(argc, argv, "--gguf");
	const char *corpusPath = ArgValue(argc, argv, "--corpus");
	if (!ggufPath || !corpusPath) {
		printf("Usage : NKQwen2Train --gguf=<modele.gguf> --corpus=<corpus.txt> [options]\n"
			   "  --out=<prefixe>     prefixe des checkpoints (defaut « qwen2_lora »)\n"
			   "  --lr=<f>            taux d'apprentissage (defaut 1e-4)\n"
			   "  --steps=<n>         pas sur ce lancement, 0 = jusqu'a la fin du corpus\n"
			   "  --save-every=<n>    sauvegarde tous les n pas (defaut 200)\n"
			   "  --rank=<n>          rang LoRA (defaut 8)\n"
			   "  --max-seq=<n>       longueur max d'un exemple (defaut 128)\n"
			   "  --valid-every=<n>   validation tous les n pas, 0 = jamais (defaut 500)\n"
			   "  --fresh             ignorer tout checkpoint et repartir de zero\n");
		return 2;
	}

	const NkString outPrefix(ArgValue(argc, argv, "--out") ? ArgValue(argc, argv, "--out") : "qwen2_lora");
	const float32 lrArg = ArgFloat(argc, argv, "--lr", 1e-4f);
	const int32 stepsArg = ArgInt(argc, argv, "--steps", 0);
	const int32 saveEvery = ArgInt(argc, argv, "--save-every", 200);
	const int32 rank = ArgInt(argc, argv, "--rank", 8);
	const int32 maxT = ArgInt(argc, argv, "--max-seq", 128);
	const int32 validEvery = ArgInt(argc, argv, "--valid-every", 500);
	const bool fresh = ArgFlag(argc, argv, "--fresh");

	signal(SIGINT, OnInterrupt);
	signal(SIGTERM, OnInterrupt);

	// ---- GPU ---------------------------------------------------------------
	// Le backend est AFFICHÉ, jamais supposé : une mesure attribuée au mauvais
	// backend est un résultat faux, et c'est arrivé (cf. jalon 4).
	NkTensorGpu &gpu = NkTensorGpu::Get();
	if (!gpu.IsAvailable()) {
		printf("aucun device compute GPU disponible\n");
		return 1;
	}
	printf("backend compute : %s\n", gpu.BackendName());

	// ---- modèle ------------------------------------------------------------
	NkQwen2LoraGpuOptions opt;
	opt.maxSeqLen = maxT;
	opt.loraRank = rank;
	opt.loraAlpha = (float32)(2 * rank);
	opt.verbose = true;
	NkQwen2LoraGpu model;
	NkString err;
	NkChrono loadClk;
	if (!model.Load(ggufPath, opt, &err)) {
		printf("chargement impossible : %s\n", err.CStr());
		return 1;
	}
	printf("modèle chargé en %.1f s\n", loadClk.Elapsed().ToSeconds());

	NkQwen2Tokenizer tok;
	if (!tok.LoadFromGGUF(ggufPath, &err)) {
		printf("tokenizer : %s\n", err.CStr());
		return 1;
	}

	// ---- corpus ------------------------------------------------------------
	NkVector<QaPair> pairs;
	if (!LoadCorpus(corpusPath, pairs)) {
		printf("corpus illisible ou vide : %s\n", corpusPath);
		return 1;
	}
	printf("corpus : %u paires\n", (unsigned)pairs.Size());

	// Les exemples sont formatés À LA DEMANDE, pas tous d'avance : 100 017
	// exemples tokenisés tiendraient en mémoire, mais les former tous coûterait
	// plusieurs minutes avant le premier pas — un temps mort à chaque reprise.

	// ---- reprise -----------------------------------------------------------
	NkLoraTrainState ts;
	ts.lr = lrArg;
	ts.beta1 = 0.9f;
	ts.beta2 = 0.999f;
	ts.eps = 1e-8f;
	int32 slot = 0;
	bool resumed = false;
	if (!fresh) {
		resumed = TryResume(outPrefix, model, ts, slot);
		if (resumed) {
			printf("REPRISE : pas %lld, exemple %lld/%u, époque %lld\n", (long long)ts.step,
				   (long long)ts.corpusPos, (unsigned)pairs.Size(), (long long)ts.epoch);
			// On repart sur l'AUTRE emplacement : le checkpoint dont on vient de
			// repartir reste intact tant que le suivant n'est pas complet.
			slot = 1 - slot;
		} else {
			printf("aucun checkpoint exploitable — départ de zéro\n");
		}
	} else {
		printf("--fresh : les checkpoints existants sont ignorés\n");
	}

	NkQwen2LoraGpuTrainer trainer;
	if (!trainer.Init(&model, ts.lr, &err)) {
		printf("init du trainer : %s\n", err.CStr());
		return 1;
	}
	// SANS CECI, la reprise diverge : Adam corrigerait le biais comme au premier
	// pas alors que ses moments sont déjà grands.
	trainer.SetStepCount(ts.step);

	// ---- boucle ------------------------------------------------------------
	const int64 total = (stepsArg > 0) ? (int64)stepsArg : (int64)pairs.Size() - ts.corpusPos;
	printf("\n%lld pas prévus sur ce lancement, sauvegarde tous les %d pas\n", (long long)total, saveEvery);
	printf("Ctrl+C : arrêt PROPRE (le pas en cours se termine et l'état est écrit)\n\n");

	NkChrono runClk;
	int64 done = 0;
	double lossAcc = 0.0;
	int64 lossCount = 0;

	while (done < total && !gStopRequested) {
		if ((uint32)ts.corpusPos >= pairs.Size()) {
			// Fin d'époque : on repart au début. La position remise à zéro fait
			// partie de l'état sauvegardé, donc une reprise retombe au bon
			// endroit même si la coupure a lieu ici.
			ts.corpusPos = 0;
			++ts.epoch;
			printf("--- époque %lld terminée, on recommence le corpus ---\n", (long long)ts.epoch);
		}
		const QaPair &qa = pairs[(uint32)ts.corpusPos];
		NkQwen2SftExample ex;
		if (!NkQwen2SftFormatChatML(tok, qa.q, qa.r, ex, &err) || ex.tokens.Size() < 4
			|| (int32)ex.tokens.Size() > maxT) {
			// Exemple inutilisable (trop long, ou formatage refusé) : on l'ignore
			// SANS compter de pas — il ne doit pas gonfler le compteur d'Adam.
			++ts.corpusPos;
			continue;
		}

		const double loss = trainer.TrainExample(ex, &err);
		if (loss < 0.0) {
			printf("\nERREUR au pas %lld : %s\n", (long long)ts.step, err.CStr());
			// On sauvegarde avant de sortir : ce qui a été appris jusqu'ici a
			// coûté des heures et n'a pas à disparaître avec l'erreur.
			NkLoraGpuNklaInfo info;
			NkString serr;
			if (SaveCheckpointAtomic(outPrefix, slot, model, ts, rank, &info, &serr))
				printf("état sauvegardé dans %s avant l'arrêt\n", CkptPath(outPrefix, slot).CStr());
			return 1;
		}

		++ts.step;
		++ts.corpusPos;
		++done;
		lossAcc += loss;
		++lossCount;

		if (done % 10 == 0 || done == total) {
			const double el = runClk.Elapsed().ToSeconds();
			printf("pas %lld | perte %.4f (moy. %.4f) | %.2f s/pas | exemple %lld/%u\n", (long long)ts.step,
				   loss, lossAcc / (double)lossCount, el / (double)done, (long long)ts.corpusPos,
				   (unsigned)pairs.Size());
			fflush(stdout);
		}

		if (validEvery > 0 && (done % validEvery) == 0) {
			// Validation sur un exemple pris LOIN de la position courante : mesurer
			// sur ce qu'on vient de voir ne dirait rien d'autre que la mémorisation.
			const uint32 vi = (uint32)((ts.corpusPos + (int64)pairs.Size() / 2) % (int64)pairs.Size());
			NkQwen2SftExample vex;
			if (NkQwen2SftFormatChatML(tok, pairs[vi].q, pairs[vi].r, vex, &err)
				&& (int32)vex.tokens.Size() <= maxT) {
				const double vl = trainer.EvaluateExample(vex, &err);
				if (vl >= 0.0)
					printf("        validation (exemple %u, jamais vu récemment) : %.4f\n", vi, vl);
			}
		}

		if ((done % saveEvery) == 0) {
			NkLoraGpuNklaInfo info;
			NkChrono saveClk;
			if (!SaveCheckpointAtomic(outPrefix, slot, model, ts, rank, &info, &err)) {
				printf("\nSAUVEGARDE ÉCHOUÉE : %s\n", err.CStr());
				printf("l'entraînement CONTINUE, mais la reprise n'est plus garantie — "
					   "vérifie l'espace disque.\n");
			} else {
				printf("        checkpoint %s écrit (%.1f Mo, %.1f s)\n", CkptPath(outPrefix, slot).CStr(),
					   (double)info.fileBytes / 1048576.0, saveClk.Elapsed().ToSeconds());
				slot = 1 - slot; // l'autre emplacement pour la prochaine fois
			}
			fflush(stdout);
		}
	}

	// ---- sortie ------------------------------------------------------------
	if (gStopRequested)
		printf("\ninterruption demandée — écriture de l'état avant de quitter\n");

	NkLoraGpuNklaInfo info;
	if (!SaveCheckpointAtomic(outPrefix, slot, model, ts, rank, &info, &err)) {
		printf("SAUVEGARDE FINALE ÉCHOUÉE : %s\n", err.CStr());
		return 1;
	}
	printf("checkpoint final : %s (%.1f Mo)\n", CkptPath(outPrefix, slot).CStr(),
		   (double)info.fileBytes / 1048576.0);

	// Un fichier d'adaptateurs SEULS, sans état d'optimisation : c'est le
	// livrable pour l'inférence, trois fois plus léger que le checkpoint.
	const NkString finalNkla = outPrefix + NkString(".nkla");
	const NkQwen2Config &c = model.Config();
	if (NkLoraGpuSaveNKLA(finalNkla.CStr(), model.Lora(), rank, model.Lora()[0].q.alpha, c.dModel, c.ffnDim,
						  c.nHeads * c.headDim, c.nKVHeads * c.headDim, &info, &err))
		printf("adaptateurs livrables : %s (%.1f Mo, %llu paramètres)\n", finalNkla.CStr(),
			   (double)info.fileBytes / 1048576.0, (unsigned long long)info.paramCount);

	const double el = runClk.Elapsed().ToSeconds();
	printf("\n%lld pas en %.0f s (%.2f s/pas) | perte moyenne %.4f | total cumulé : %lld pas\n", (long long)done,
		   el, done ? el / (double)done : 0.0, lossCount ? lossAcc / (double)lossCount : 0.0,
		   (long long)ts.step);
	return 0;
}
