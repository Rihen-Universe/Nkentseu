// =============================================================================
// NKQwen2Ask — interroger un modèle AVEC les adaptateurs qu'on vient d'entraîner.
//
// POURQUOI UN OUTIL SÉPARÉ DE NKQwen2Chat
// ---------------------------------------
// NKQwen2Chat dialogue avec le modèle de BASE : il garde un KV-cache et écrit la
// réponse mot à mot, ce qui donne une conversation fluide. Mais ce chemin ne
// connaît pas les adaptateurs LoRA.
//
// Ici, on emprunte le MÊME chemin de calcul que l'entraînement — adaptateurs
// compris. C'est ce qui rend la comparaison « avant / après » honnête : si la
// réponse change, c'est l'apprentissage qui l'a changée, pas une autre
// implémentation. Le prix est connu et assumé : la génération se fait SANS
// KV-cache (la séquence entière est réévaluée à chaque token), donc une réponse
// prend des dizaines de secondes au lieu de quelques-unes.
//
// Sans --adapters, le modèle répond tel qu'il est sorti du GGUF : c'est
// exactement la mesure « avant » qu'il faut prendre avant d'entraîner.
//
// Usage :
//   NKQwen2Ask --gguf=<modele.gguf> [--adapters=<fichier.nkla>] [options]
//
//   --adapters=<f>   adaptateurs .nkla à appliquer (défaut : aucun)
//   --tokens=<n>     longueur maximale d'une réponse (défaut 48)
//   --rank=<n>       rang LoRA — doit être celui de l'entraînement (défaut 8)
//   --max-seq=<n>    contexte maximal en tokens (défaut 256)
//   --question=<q>   poser UNE question et sortir (sinon : dialogue)
//
// Zéro STL. Portable Windows / Linux / macOS.
// =============================================================================
#include "NKInfer/NkQwen2LoraGpu.h"
#include "NKInfer/NkLoraGpu.h"
#include "NKInfer/NkOllamaLocate.h" // --model=nom:tag plutôt qu'une empreinte
#include "NKInfer/NkQwen2Tokenizer.h"
#include "NKTensor/NkTensorGpu.h"
#include "NKTime/NkChrono.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#if defined(_WIN32)
	#include <windows.h>
#endif

using namespace nkentseu;
using namespace nkentseu::ai;
using namespace nkentseu::ai::infer;

namespace {

	const char *ArgValue(int argc, char **argv, const char *key) {
		const usize klen = strlen(key);
		for (int i = 1; i < argc; ++i)
			if (strncmp(argv[i], key, klen) == 0 && argv[i][klen] == '=')
				return argv[i] + klen + 1;
		return nullptr;
	}

	int32 ArgInt(int argc, char **argv, const char *key, int32 def) {
		const char *v = ArgValue(argc, argv, key);
		return v ? (int32)atoi(v) : def;
	}

	// Une console peut préfixer la première ligne saisie de trois octets
	// invisibles (la marque d'ordre d'octets UTF-8). Les laisser passer ferait
	// d'une commande « /quitte » une question posée au modèle.
	const char *SkipBom(const char *s) {
		const unsigned char *u = (const unsigned char *)s;
		if (u[0] == 0xEF && u[1] == 0xBB && u[2] == 0xBF)
			return s + 3;
		return s;
	}

	void Trim(char *s) {
		usize n = strlen(s);
		while (n > 0 && (s[n - 1] == '\n' || s[n - 1] == '\r' || s[n - 1] == ' '))
			s[--n] = 0;
	}

	// Le gabarit ChatML est celui sur lequel le modèle a été entraîné : s'en
	// écarter fait répondre à côté, ou dans une autre langue.
	NkString BuildPrompt(const NkString &question) {
		return NkString("<|im_start|>system\nTu es un assistant francophone. "
						"Réponds en français, brièvement.<|im_end|>\n"
						"<|im_start|>user\n")
			   + question + NkString("<|im_end|>\n<|im_start|>assistant\n");
	}

	bool Ask(NkQwen2LoraGpu &model, const NkQwen2Tokenizer &tok, const NkString &question, int32 maxNew) {
		NkVector<int32> ids;
		if (!tok.EncodeWithSpecials(BuildPrompt(question), ids)) {
			printf("  échec d'encodage de la question\n");
			return false;
		}
		NkVector<int32> out;
		float64 sec = 0.0;
		NkString err;
		printf("  (calcul en cours — sans KV-cache, compter plusieurs dizaines de secondes)\n");
		if (!model.Generate(ids, maxNew, tok.ImEndId(), out, &sec, &err)) {
			printf("  échec : %s\n", err.CStr());
			return false;
		}
		printf("\n  %s\n\n  [%llu tokens en %.1f s]\n", tok.Decode(out).CStr(),
			   (unsigned long long)out.Size(), sec);

		// LE DIRE QUAND LA RÉPONSE EST COUPÉE. Une phrase qui s'arrête au milieu
		// ressemble à un modèle défaillant, alors que c'est seulement la limite
		// de longueur qui a été atteinte. Sans ce message, on cherche l'erreur là
		// où elle n'est pas. Le signe : le dernier token n'est pas la marque de
		// fin de tour.
		const bool finished = out.Size() > 0 && out[out.Size() - 1] == tok.ImEndId();
		if (!finished)
			printf("  /!\\ réponse TRONQUÉE à %d tokens — relance avec « --tokens=%d »\n"
				   "      (ou « /tokens %d » en dialogue) pour la voir en entier\n",
				   maxNew, maxNew * 2, maxNew * 2);
		printf("\n");
		return true;
	}

} // namespace

int main(int argc, char **argv) {
#if defined(_WIN32)
	// Sans cela, les accents des réponses sortent en caractères parasites.
	SetConsoleOutputCP(CP_UTF8);
	SetConsoleCP(CP_UTF8);
#endif
	printf("=== NKQwen2Ask — interroger un modèle affiné ===\n\n");

	// Trois façons de désigner le modèle, de la plus commode à la plus explicite :
	//   1. rien du tout      -> le seul modèle installé, s'il n'y en a qu'un ;
	//   2. --model=nom:tag   -> résolu via les manifestes d'Ollama ;
	//   3. --gguf=<chemin>   -> le chemin exact, qui garde le dernier mot.
	// Demander une empreinte SHA-256 de 64 caractères comme SEULE entrée était
	// une barrière inutile : personne ne la tape sans se tromper.
	NkString gguf;
	if (const char *p = ArgValue(argc, argv, "--gguf")) {
		gguf = NkString(p);
	} else if (const char *m = ArgValue(argc, argv, "--model")) {
		NkString rerr;
		gguf = NkOllamaResolve(m, &rerr);
		if (gguf.Empty()) {
			printf("  %s\n\n", rerr.CStr());
			NkOllamaPrintModels();
			return 2;
		}
		printf("modèle « %s » résolu automatiquement\n", m);
	} else {
		NkVector<NkString> models;
		if (NkOllamaListModels(models) && models.Size() == 1) {
			NkString rerr;
			gguf = NkOllamaResolve(models[0].CStr(), &rerr);
			if (!gguf.Empty())
				printf("un seul modèle installé, utilisé par défaut : %s\n", models[0].CStr());
		}
	}
	if (gguf.Empty()) {
		printf("Usage : NKQwen2Ask [--model=<nom:tag> | --gguf=<chemin.gguf>] [options]\n"
			   "  --model=<nom>    modele Ollama, ex. « qwen2.5:7b-instruct »\n"
			   "  --gguf=<chemin>  chemin explicite d'un .gguf\n"
			   "  --adapters=<f>   adaptateurs .nkla a appliquer (defaut : aucun)\n"
			   "  --tokens=<n>     longueur maximale d'une reponse (defaut 192)\n"
			   "  --rank=<n>       rang LoRA, celui de l'entrainement (defaut 8)\n"
			   "  --max-seq=<n>    contexte maximal en tokens (defaut 512)\n"
			   "  --question=<q>   poser UNE question et sortir\n\n");
		NkOllamaPrintModels();
		return 2;
	}
	const char *ggufPath = gguf.CStr();
	const char *adaptPath = ArgValue(argc, argv, "--adapters");
	const char *oneShot = ArgValue(argc, argv, "--question");
	// 192 et non 48 : à 48, une explication un peu fournie se coupait au milieu
	// d'une phrase, ce qui donne l'impression d'un modèle défaillant. Le coût est
	// du temps, pas de la mémoire — et il n'est payé que si le modèle parle
	// vraiment jusque-là.
	int32 maxNew = ArgInt(argc, argv, "--tokens", 192);
	const int32 rank = ArgInt(argc, argv, "--rank", 8);
	const int32 maxSeq = ArgInt(argc, argv, "--max-seq", 512);

	NkTensorGpu &gpu = NkTensorGpu::Get();
	if (!gpu.IsAvailable()) {
		printf("aucun device compute GPU disponible\n");
		return 1;
	}
	printf("backend compute : %s\n", gpu.BackendName());

	NkQwen2LoraGpuOptions opt;
	opt.maxSeqLen = maxSeq;
	opt.loraRank = rank;
	opt.loraAlpha = (float32)(2 * rank);
	opt.verbose = false;

	NkQwen2LoraGpu model;
	NkString err;
	NkChrono clk;
	if (!model.Load(ggufPath, opt, &err)) {
		printf("chargement impossible : %s\n", err.CStr());
		return 1;
	}
	printf("modèle chargé en %.1f s\n", clk.Elapsed().ToSeconds());

	// ---- adaptateurs -------------------------------------------------------
	// Sans eux, on mesure le modèle « avant ». C'est un mode LÉGITIME, pas un
	// repli : c'est la référence à laquelle comparer l'affinage.
	if (adaptPath) {
		NkLoraGpuNklaInfo info;
		if (!NkLoraGpuLoadNKLA(adaptPath, model.Lora(), &info, &err)) {
			printf("adaptateurs refusés : %s\n", err.CStr());
			return 1;
		}
		printf("adaptateurs appliqués : %s\n", adaptPath);
		printf("  %llu paramètres · rang %u · alpha %g · %u couches × %u projections\n",
			   (unsigned long long)info.paramCount, info.rank, (double)info.alpha, info.layerCount,
			   info.projCount);
		if (info.flags & NK_NKLA_TRAIN_STATE)
			printf("  (fichier de reprise : son état d'entraînement est ignoré ici)\n");
	} else {
		printf("aucun adaptateur — réponses du modèle D'ORIGINE (référence « avant »)\n");
	}

	NkQwen2Tokenizer tok;
	if (!tok.LoadFromGGUF(ggufPath, &err)) {
		printf("tokenizer : %s\n", err.CStr());
		return 1;
	}

	if (oneShot) {
		printf("\n  > %s\n", oneShot);
		return Ask(model, tok, NkString(oneShot), maxNew) ? 0 : 1;
	}

	printf("\nPose ta question et valide.\n");
	printf("  /tokens <n>   longueur maximale d'une réponse (actuel : %d)\n", maxNew);
	printf("  /quitte       sortir\n\n");
	char line[2048];
	for (;;) {
		printf("> ");
		fflush(stdout);
		if (!fgets(line, (int)sizeof(line), stdin))
			break;
		char *s = (char *)SkipBom(line);
		Trim(s);
		if (s[0] == 0)
			continue;
		if (!strcmp(s, "/quitte") || !strcmp(s, "/quit"))
			break;
		// Régler la longueur SANS relancer : le chargement du modèle coûte une
		// dizaine de secondes, le refaire pour changer un nombre serait absurde.
		if (!strncmp(s, "/tokens", 7)) {
			const int32 v = atoi(s + 7);
			if (v > 0 && v <= maxSeq) {
				maxNew = v;
				printf("  longueur maximale : %d tokens\n", maxNew);
			} else
				printf("  valeur attendue entre 1 et %d (le contexte maximal)\n", maxSeq);
			continue;
		}
		Ask(model, tok, NkString(s), maxNew);
	}
	return 0;
}
