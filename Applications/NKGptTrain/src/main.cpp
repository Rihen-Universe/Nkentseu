// =============================================================================
// NKGptTrain — pilote (thin driver) du GPT from-scratch NKAI.
// Toute la logique réutilisable vit dans le module NKGpt (nkentseu::ai::gpt) :
//   - NkGptCore  : tokenizer BPE, corpus, checkpoint « NKGP »
//   - NkGptTrainer : config + construction modèle + boucle Fit + génération + reprise
// Ici : lecture des variables d'environnement -> NkGptConfig -> NkGptTrainer.
// N'IMPORTE QUELLE application peut faire pareil (voir wiki/AI/Entrainement-NKGptTrain.md).
//
//   Corpus  : NK_GPT_DIR (dossier) ou NK_GPT_FILE (fichier) ; NK_GPT_CHARS (budget).
//   Modèle  : NK_GPT_T/D/H/L/B, NK_GPT_MERGES.
//   Train   : NK_GPT_STEPS, NK_GPT_ACCUM, NK_GPT_LR, NK_GPT_WARMUP, NK_GPT_SAVEEVERY.
//   Ckpt    : NK_GPT_SAVE, NK_GPT_LOAD, NK_GPT_RESUME=1 (reprendre l'entraînement).
//   Gen     : NK_GPT_PROMPT, NK_GPT_GENLEN, NK_GPT_LANG=fr|en|bbj.
// =============================================================================
#include "NKGpt/NkGptTrainer.h"
#include "NKTensor/NkTensorGpu.h"
#include "NKLogger/NkLog.h" // logger (macro)

#include <cstdlib> // getenv, atol, atoi, atof

using namespace nkentseu;
using namespace nkentseu::ai;	   // NkTensorGpu
using namespace nkentseu::ai::gpt; // NkGptConfig, NkGptTrainer

int main() {
	logger.Info("=== NKGptTrain : petit GPT BPE (from-scratch, GPU-résident, zéro-STL) ===");
	NkTensorGpu &gpu = NkTensorGpu::Get();
	logger.Info("GPU compute : {0} ({1})", gpu.IsAvailable() ? "OUI" : "NON", gpu.BackendName());

	auto envI = [](const char *k, int64 def) -> int64 {
		const char *v = getenv(k);
		return v ? (int64)atol(v) : def;
	};
	auto envS = [](const char *k) -> NkString {
		const char *v = getenv(k);
		return v ? NkString(v) : NkString();
	};

	// ---- Variables d'environnement -> configuration ----
	const char *envf = getenv("NK_GPT_FILE");
	const char *envc = getenv("NK_GPT_CHARS");
	NkGptConfig cfg;
	cfg.corpusFile = envf ? NkString(envf) : NkString();
	cfg.corpusDir = envS("NK_GPT_DIR");
	if (cfg.corpusFile.Empty() && cfg.corpusDir.Empty())
		cfg.corpusDir = NkString("D:/Projets/2026/Nkentseu/Nkentseu/Resources/Datasets");
	cfg.maxChars = envc ? (nk_size)atol(envc) : (envf ? (nk_size)150000 : (nk_size)1200000);
	cfg.merges = (int)envI("NK_GPT_MERGES", 600);
	cfg.T = envI("NK_GPT_T", 128);
	cfg.d = envI("NK_GPT_D", 256);
	cfg.H = envI("NK_GPT_H", 8);
	cfg.L = envI("NK_GPT_L", 4);
	cfg.B = envI("NK_GPT_B", 16);
	cfg.steps = (int)envI("NK_GPT_STEPS", 300);
	cfg.accum = (int)envI("NK_GPT_ACCUM", 1);
	cfg.warmup = getenv("NK_GPT_WARMUP") ? (int)envI("NK_GPT_WARMUP", 0) : -1; // -1 => steps/20
	{
		const char *e = getenv("NK_GPT_LR");
		cfg.lr = e ? (float)atof(e) : 3e-4f;
	}
	cfg.saveEvery = (int)envI("NK_GPT_SAVEEVERY", 0);
	cfg.savePath = envS("NK_GPT_SAVE");
	cfg.loadPath = envS("NK_GPT_LOAD");
	cfg.resume = !cfg.loadPath.Empty() && (envI("NK_GPT_RESUME", 0) != 0);
	{
		const char *e = getenv("NK_GPT_PROMPT");
		cfg.seed = e ? NkString(e) : NkString("Le ");
	}
	cfg.genLen = (int)envI("NK_GPT_GENLEN", 400);
	cfg.genLang = envS("NK_GPT_LANG");

	// ---- Préparation (corpus/checkpoint + BPE + modèle) ----
	NkGptTrainer trainer(cfg);
	if (!trainer.Prepare()) {
		gpu.Shutdown();
		return 2;
	}

	// ---- Mode CHARGEMENT seul : on génère et on sort (sauf reprise d'entraînement) ----
	if (!cfg.loadPath.Empty() && !cfg.resume) {
		const int gl = trainer.GenLangIndex();
		logger.Info("=== TEXTE GÉNÉRÉ (langue {0}, amorce « {1} », {2} tokens) ===",
					gl >= 0 ? trainer.Langs()[(nk_size)gl].CStr() : "auto", cfg.seed.CStr(), cfg.genLen);
		logger.Info("{0}", trainer.Generate(cfg.seed, cfg.genLen, 0.8, gl).CStr());
		logger.Info("=========================================================");
		gpu.Shutdown();
		return 0;
	}

	// ---- Entraînement + génération finale ----
	trainer.Fit();
	trainer.GenerateFinal();

	const double ema = trainer.LastEma();
	const bool ok = ema < 5.0; // seuil indicatif (vocab BPE => base ln(V) plus élevée)
	logger.Info("[{0}] le GPT a appris (perte {1}) et génère du texte {2}.", ok ? " OK " : "FAIL", ema,
				trainer.UseGpu() ? "100% sur GPU" : "sur CPU");
	gpu.Shutdown();
	return ok ? 0 : 1;
}
