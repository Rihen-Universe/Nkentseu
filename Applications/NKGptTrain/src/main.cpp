// =============================================================================
// NKGptTrain — pilote (thin driver) du GPT from-scratch NKAI.
// Toute la logique réutilisable vit dans le module NKGpt (nkentseu::ai::gpt) :
//   - NkGptCore  : tokenizer BPE, corpus, checkpoint « NKGP »
//   - NkGptTrainer : config + construction modèle + boucle Fit + génération + reprise
// Ici : lecture de la configuration -> NkGptConfig -> NkGptTrainer.
// N'IMPORTE QUELLE application peut faire pareil (voir wiki/AI/Entrainement-NKGptTrain.md).
//
// CONFIGURATION — deux sources combinées (toutes FACULTATIVES, défauts partout) :
//   1. Fichier : NK_GPT_CONFIG=chemin.cfg  (lignes « CLE=valeur », '#' = commentaire).
//   2. Variables d'environnement (via le système maison nkentseu::env) — PRIORITAIRES
//      sur le fichier (ajustement rapide sans éditer le .cfg).
//   Résolution : variable d'env réelle > fichier config > défaut codé.
//
//   Corpus  : NK_GPT_DIR (dossier) ou NK_GPT_FILE (fichier) ; NK_GPT_CHARS (budget).
//   Modèle  : NK_GPT_T/D/H/L/B, NK_GPT_MERGES.
//   Train   : NK_GPT_STEPS, NK_GPT_ACCUM, NK_GPT_LR, NK_GPT_WARMUP, NK_GPT_SAVEEVERY.
//   Valid.  : NK_GPT_VALFRAC (0..0.9 queue held-out par langue), NK_GPT_VALEVERY (éval /N pas).
//   Ckpt    : NK_GPT_SAVE, NK_GPT_LOAD, NK_GPT_RESUME=1 (reprendre l'entraînement).
//   Gen     : NK_GPT_PROMPT, NK_GPT_GENLEN, NK_GPT_LANG=fr|en|bbj.
// =============================================================================
#include "NKGpt/NkGptTrainer.h"
#include "NKTensor/NkTensorGpu.h"
#include "NKPlatform/NkEnv.h" // système env maison : env::GetEnvVar (header-only, liable, cross-plateforme)
#include "NKLogger/NkLog.h"	  // logger (macro)

#include <cstdio>  // FILE* pour lire le fichier de config (comme NkGptCore)
#include <cstdlib> // atol, atof (parsing numérique)

using namespace nkentseu;
using namespace nkentseu::ai;	   // NkTensorGpu
using namespace nkentseu::ai::gpt; // NkGptConfig, NkGptTrainer

// Une entrée du fichier de config (CLE=valeur).
struct NkCfgEntry {
	NkString key;
	NkString val;
};

// Table du fichier de config (vide si NK_GPT_CONFIG non fourni).
static NkVector<NkCfgEntry> gCfgFile;

// Lit une variable d'environnement via le système maison (env::GetEnvVar, cross-plateforme).
static NkString RawEnv(const char *name) {
	const char *v = env::GetEnvVar(name);
	return v ? NkString(v) : NkString();
}

// Retire les espaces/tabs/retours en tête et en queue.
static NkString Trim(const NkString &s) {
	nk_size a = 0;
	nk_size b = s.Size();
	while (a < b) {
		const char c = s.Data()[a];
		if (c != ' ' && c != '\t' && c != '\r' && c != '\n')
			break;
		++a;
	}
	while (b > a) {
		const char c = s.Data()[b - 1];
		if (c != ' ' && c != '\t' && c != '\r' && c != '\n')
			break;
		--b;
	}
	return s.SubStr(a, b - a);
}

// Charge un fichier « CLE=valeur » dans gCfgFile (ignore vides et lignes '#').
static void LoadConfigFile(const char *path) {
	FILE *f = fopen(path, "rb");
	if (!f)
		return;
	NkString all;
	char buf[8192];
	size_t n;
	while ((n = fread(buf, 1, sizeof(buf), f)) > 0)
		all.Append(buf, (NkString::SizeType)n);
	fclose(f);
	nk_size pos = 0;
	const nk_size sz = all.Size();
	while (pos < sz) {
		nk_size nl = all.Find("\n", pos);
		const nk_size len = (nl == NkString::npos) ? (sz - pos) : (nl - pos);
		NkString line = Trim(all.SubStr(pos, len));
		pos = (nl == NkString::npos) ? sz : nl + 1;
		if (line.Size() == 0 || line.Data()[0] == '#')
			continue;
		const nk_size eq = line.Find("=");
		if (eq == NkString::npos)
			continue;
		NkCfgEntry e;
		e.key = Trim(line.SubStr(0, eq));
		e.val = Trim(line.SubStr(eq + 1, line.Size() - eq - 1));
		if (e.key.Size() > 0)
			gCfgFile.PushBack(e);
	}
}

// Valeur brute d'une clé : variable d'env réelle > fichier config > vide.
static NkString CfgRaw(const char *key) {
	NkString e = RawEnv(key);
	if (!e.Empty())
		return e;
	for (nk_size i = 0; i < gCfgFile.Size(); ++i)
		if (gCfgFile[i].key == NkString(key))
			return gCfgFile[i].val;
	return NkString();
}

// Helpers typés (défaut si la clé est absente des deux sources).
static NkString CfgStr(const char *key) {
	return CfgRaw(key);
}

static bool CfgHas(const char *key) {
	return !CfgRaw(key).Empty();
}

static int64 CfgInt(const char *key, int64 def) {
	NkString v = CfgRaw(key);
	return v.Empty() ? def : (int64)atol(v.CStr());
}

static float CfgFloat(const char *key, float def) {
	NkString v = CfgRaw(key);
	return v.Empty() ? def : (float)atof(v.CStr());
}

int main() {
	logger.Info("=== NKGptTrain : petit GPT BPE (from-scratch, GPU-résident, zéro-STL) ===");
	NkTensorGpu &gpu = NkTensorGpu::Get();
	logger.Info("GPU compute : {0} ({1})", gpu.IsAvailable() ? "OUI" : "NON", gpu.BackendName());

	// ---- Fichier de config optionnel (NK_GPT_CONFIG) ----
	NkString cfgPath = RawEnv("NK_GPT_CONFIG");
	if (!cfgPath.Empty()) {
		LoadConfigFile(cfgPath.CStr());
		logger.Info("Config : {0} ({1} clés ; les variables d'env restent prioritaires).", cfgPath.CStr(),
					(unsigned long long)gCfgFile.Size());
	}

	// ---- Configuration (fichier + env -> NkGptConfig) ----
	NkGptConfig cfg;
	cfg.corpusFile = CfgStr("NK_GPT_FILE");
	cfg.corpusDir = CfgStr("NK_GPT_DIR");
	if (cfg.corpusFile.Empty() && cfg.corpusDir.Empty())
		cfg.corpusDir = NkString("D:/Projets/2026/Nkentseu/Nkentseu/Resources/Datasets");
	cfg.maxChars = CfgHas("NK_GPT_CHARS") ? (nk_size)CfgInt("NK_GPT_CHARS", 0)
										  : (!cfg.corpusFile.Empty() ? (nk_size)150000 : (nk_size)1200000);
	cfg.merges = (int)CfgInt("NK_GPT_MERGES", 600);
	cfg.T = CfgInt("NK_GPT_T", 128);
	cfg.d = CfgInt("NK_GPT_D", 256);
	cfg.H = CfgInt("NK_GPT_H", 8);
	cfg.L = CfgInt("NK_GPT_L", 4);
	cfg.B = CfgInt("NK_GPT_B", 16);
	cfg.steps = (int)CfgInt("NK_GPT_STEPS", 300);
	cfg.accum = (int)CfgInt("NK_GPT_ACCUM", 1);
	cfg.warmup = CfgHas("NK_GPT_WARMUP") ? (int)CfgInt("NK_GPT_WARMUP", 0) : -1; // -1 => steps/20
	cfg.lr = CfgFloat("NK_GPT_LR", 3e-4f);
	cfg.saveEvery = (int)CfgInt("NK_GPT_SAVEEVERY", 0);
	cfg.valFrac = CfgFloat("NK_GPT_VALFRAC", 0.f);
	cfg.valEvery = (int)CfgInt("NK_GPT_VALEVERY", 0);
	cfg.savePath = CfgStr("NK_GPT_SAVE");
	cfg.loadPath = CfgStr("NK_GPT_LOAD");
	cfg.resume = !cfg.loadPath.Empty() && (CfgInt("NK_GPT_RESUME", 0) != 0);
	{
		NkString prompt = CfgStr("NK_GPT_PROMPT");
		cfg.seed = prompt.Empty() ? NkString("Le ") : prompt;
	}
	cfg.genLen = (int)CfgInt("NK_GPT_GENLEN", 400);
	cfg.genLang = CfgStr("NK_GPT_LANG");

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
