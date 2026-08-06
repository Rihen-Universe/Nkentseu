// @License Proprietary - All Rights Reserved (see LICENSE)
// =============================================================================
// NKQwen2Chat — PARLER au Qwen2.5 7B quantifié, ses propres questions.
//
// POURQUOI CETTE APPLICATION EXISTE
//   NKQwen2GpuTest PROUVE que le forward GPU est juste, mais son prompt est
//   ÉCRIT EN DUR (« Quelle est la capitale du Cameroun ? »). Un test qui prouve
//   ne remplace pas un outil qui sert : pour juger ce que le modèle sait
//   vraiment, il faut pouvoir lui poser SES questions. C'est tout l'objet de ce
//   programme, et sa seule différence avec le test.
//
// DEUX CHOIX QUI NE SONT PAS DE CONFORT
//
//   1. On n'appelle PAS NkQwen2Gpu::Generate(), qui ne rend la main qu'une fois
//      TOUS les tokens produits. À ~590 ms/token, une réponse de cent tokens
//      laisserait l'écran muet une minute — on croirait le programme planté. On
//      pilote donc Forward() token par token et on ÉCRIT AU FUR ET À MESURE.
//
//   2. On ne réinitialise PAS le cache entre deux questions. Le KV-cache tient
//      déjà tout l'échange : reconstruire le prompt complet à chaque tour
//      referait le préfill de toute la conversation (2,2 s pour 38 tokens, et
//      ça croît sans fin). Conserver le cache donne le multi-tour GRATUITEMENT
//      — le modèle se souvient de ce qui précède sans qu'on le lui répète.
//
// LE DÉCODAGE NE PEUT PAS ÊTRE FAIT TOKEN PAR TOKEN
//   Le BPE de Qwen est BYTE-LEVEL : un « é » vaut deux octets, qui peuvent
//   tomber dans DEUX tokens différents. Décoder chaque token isolément
//   afficherait des octets orphelins — du charabia là où le modèle a raison. On
//   redécode donc la suite ENTIÈRE à chaque pas et on n'affiche que ce qui s'y
//   est ajouté. Coût négligeable devant les 590 ms du pas lui-même.
//
// Usage : NKQwen2Chat.exe [chemin_gguf_ou_blob_ollama]
// Backend verrouillé sur Vulkan : output.weight = 447 Mo dépasse les 128 Mo
// garantis par ressource en D3D11 (leçon du jalon 5).
// =============================================================================

#include "NKInfer/NkQwen2Gpu.h"
#include "NKInfer/NkQwen2Tokenizer.h"
#include "NKInfer/NkSampling.h"
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

// Fenêtre de contexte. Le KV-cache coûte ~0,11 Mo par token (28 couches,
// 4 têtes KV, 128 par tête, f32) : 2048 tokens ≈ 230 Mo, à comparer aux 4,12 Go
// du modèle sur une carte de 8 Go. C'est le bon compromis pour un dialogue.
static const int32 kMaxSeq = 2048;

// Plafond par réponse. À 590 ms/token, 256 tokens font déjà 2,5 minutes
// d'attente : au-delà, ce n'est plus une conversation.
static const int32 kDefaultNewTokens = 256;

static void PrintBytes(const char *label, uint64 bytes) {
	printf("    %-34s %8.1f Mo\n", label, (float64)bytes / 1048576.0);
}

static void PrintHelp() {
	printf("\n  Commandes :\n");
	printf("    /aide            cette aide\n");
	printf("    /oublie          vide la mémoire de la conversation (repart à zéro)\n");
	printf("    /temp <valeur>   0 = déterministe (défaut) · 0.7 = créatif\n");
	printf("    /tokens <n>      longueur maximale d'une réponse (défaut %d)\n", kDefaultNewTokens);
	printf("    /etat            contexte utilisé, VRAM, vitesse\n");
	printf("    /quitte          sortir\n\n");
}

int main(int argc, char **argv) {
#if defined(_WIN32)
	// SANS CECI, LES ACCENTS SORTENT EN CHARABIA. La console Windows n'est pas
	// en UTF-8 par défaut : « Yaoundé » s'y affiche « Yaound├® » — c'est ce
	// qu'on voit dans les sorties du test. Le modèle avait raison, la console
	// mentait.
	SetConsoleOutputCP(CP_UTF8);
	SetConsoleCP(CP_UTF8);
#endif

	printf("=== NKQwen2Chat — dialogue avec le Qwen2.5 7B quantifié, sur GPU ===\n\n");

	// ---- Verrou de backend, AVANT toute utilisation du GPU -------------------
	{
		const char *cur = getenv("NK_TENSOR_API");
		if (!cur || cur[0] == 0) {
#if defined(_WIN32)
			_putenv_s("NK_TENSOR_API", "vulkan");
#else
			setenv("NK_TENSOR_API", "vulkan", 1);
#endif
			printf("  backend verrouillé sur \"vulkan\" (output.weight = 447 Mo > 128 Mo garantis par D3D11)\n");
		} else {
			printf("  NK_TENSOR_API = \"%s\" (fourni par l'environnement, respecté tel quel)\n", cur);
		}
	}

	const char *path = nullptr;
	if (argc > 1)
		path = argv[1];
	else if (const char *e = getenv("NK_GGUF_PATH"))
		path = e;
	else
		path = "C:/Users/Rihen/.ollama/models/blobs/"
			   "sha256-2bada8a7450677000f678be90653b85d364de7db25eb5ea54136ada5f3933730";
	printf("  modèle : %s\n\n", path);

	NkTensorGpu &gpu = NkTensorGpu::Get();
	if (!gpu.IsAvailable()) {
		printf("  ÉCHEC : aucun device compute GPU disponible.\n");
		return 1;
	}
	printf("  backend compute réellement obtenu : %s\n", gpu.BackendName());
	if (!std::strstr(gpu.BackendName(), "ulkan"))
		printf("  !! ATTENTION : backend != Vulkan — les tenseurs de 306/447 Mo peuvent être refusés.\n");

	// ---- Chargement ----------------------------------------------------------
	printf("\n  Chargement du 7B (une dizaine de secondes, dont la lecture disque)...\n");
	NkQwen2GpuOptions opt;
	opt.maxSeqLen = kMaxSeq;
	opt.maxBatchTokens = 64;
	opt.verbose = false; // en dialogue, le détail par couche n'aide personne

	NkQwen2Gpu model;
	NkString err;
	NkChrono loadClk;
	if (!model.Load(path, opt, &err)) {
		printf("  ÉCHEC du chargement : %s\n", err.CStr());
		return 1;
	}
	const float64 loadSec = loadClk.Elapsed().ToSeconds();

	const NkQwen2GpuStats &st = model.Stats();
	printf("\n  Modèle résident GPU — %d couches, chargé en %.1f s\n", model.LayerCount(), loadSec);
	PrintBytes("poids quantifiés + normes", st.weightBytes);
	PrintBytes("KV-cache (contexte de 2048)", st.kvBytes);
	PrintBytes("activations", st.scratchBytes);
	PrintBytes("TOTAL VRAM", st.TotalBytes());

	NkQwen2Tokenizer tok;
	NkString terr;
	if (!tok.LoadFromGGUF(path, &terr)) {
		printf("  ÉCHEC du tokenizer : %s\n", terr.CStr());
		return 1;
	}

	printf("\n  Prêt. Pose ta question et valide. « /aide » pour les commandes.\n");
	printf("  Compte environ une demi-seconde par mot produit : c'est un 7B sur une carte de 8 Go.\n");

	// ---- État de la session --------------------------------------------------
	float32 temperature = 0.0f; // déterministe : deux fois la même question, la même réponse
	int32 maxNew = kDefaultNewTokens;
	uint32 rng = 42u;
	bool firstTurn = true;
	float64 lastMsPerTok = 0.0;

	char line[4096];
	for (;;) {
		printf("\n\033[1mtoi >\033[0m ");
		fflush(stdout);
		if (!fgets(line, sizeof(line), stdin))
			break;

		// Retirer le retour chariot
		usize len = std::strlen(line);
		while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r'))
			line[--len] = 0;

		// UN BOM UTF-8 EN TETE FERAIT PASSER UNE COMMANDE POUR UNE QUESTION.
		// Quand l'entree vient d'un tube plutot que du clavier, Windows prefixe
		// volontiers trois octets invisibles (EF BB BF) : « /aide » n'est alors
		// plus reconnu et part au modele, qui repond poliment a cote. Constate
		// au premier essai de ce programme.
		const char *in = line;
		if (len >= 3 && (unsigned char)in[0] == 0xEF && (unsigned char)in[1] == 0xBB &&
			(unsigned char)in[2] == 0xBF) {
			in += 3;
			len -= 3;
		}
		if (len == 0)
			continue;

		// ---- Commandes -------------------------------------------------------
		if (in[0] == '/') {
			if (!std::strcmp(in, "/quitte") || !std::strcmp(in, "/quit"))
				break;
			if (!std::strcmp(in, "/aide")) {
				PrintHelp();
				continue;
			}
			if (!std::strcmp(in, "/oublie")) {
				model.ResetCache();
				firstTurn = true;
				printf("  (mémoire de la conversation vidée)\n");
				continue;
			}
			if (!std::strncmp(in, "/temp", 5)) {
				temperature = (float32)atof(in + 5);
				printf("  (température = %.2f%s)\n", temperature,
					   temperature <= 0.0f ? " — déterministe" : "");
				continue;
			}
			if (!std::strncmp(in, "/tokens", 7)) {
				const int32 v = atoi(in + 7);
				if (v > 0)
					maxNew = v;
				printf("  (réponse limitée à %d tokens)\n", maxNew);
				continue;
			}
			if (!std::strcmp(in, "/etat")) {
				printf("  contexte utilisé : %d / %d tokens · VRAM %.2f Go · %.0f ms/token\n",
					   model.CacheLength(), kMaxSeq, (float64)st.TotalBytes() / 1073741824.0, lastMsPerTok);
				continue;
			}
			printf("  commande inconnue — « /aide » pour la liste\n");
			continue;
		}

		// ---- Construction du tour ChatML -------------------------------------
		// L'amorce système n'est posée QU'UNE FOIS : le cache la conserve, la
		// répéter la ferait compter deux fois dans le contexte.
		NkString turn;
		if (firstTurn)
			turn += "<|im_start|>system\nTu es un assistant francophone. Réponds TOUJOURS en français, jamais en anglais ni en chinois, de façon claire et concise.<|im_end|>\n";
		turn += "<|im_start|>user\n";
		turn += in;
		turn += "<|im_end|>\n<|im_start|>assistant\n";
		firstTurn = false;

		NkVector<int32> ids;
		if (!tok.EncodeWithSpecials(turn, ids) || ids.Size() == 0) {
			printf("  (échec de l'encodage de la question)\n");
			continue;
		}

		// LE CONTEXTE EST FINI, ET LE DIRE VAUT MIEUX QUE LE SUBIR. Dépasser
		// maxSeqLen corromprait le cache en silence : on préfère annoncer la
		// perte de mémoire, seule solution honnête tant que la fenêtre
		// glissante n'est pas écrite.
		if (model.CacheLength() + (int32)ids.Size() + maxNew > kMaxSeq) {
			printf("  (contexte plein : la conversation repart à zéro — le modèle ne se souviendra plus de ce qui précède)\n");
			model.ResetCache();
			NkString fresh("<|im_start|>system\nTu es un assistant francophone. Réponds TOUJOURS en français, jamais en anglais ni en chinois, de façon claire et concise.<|im_end|>\n"
						   "<|im_start|>user\n");
			fresh += in;
			fresh += "<|im_end|>\n<|im_start|>assistant\n";
			ids.Clear();
			if (!tok.EncodeWithSpecials(fresh, ids)) {
				printf("  (échec de l'encodage)\n");
				continue;
			}
		}

		// ---- Préfill ---------------------------------------------------------
		NkChrono clk;
		NkTensor logits;
		if (!model.Forward(&ids[0], (int32)ids.Size(), logits, nullptr, &err)) {
			printf("  (échec du préfill : %s)\n", err.CStr());
			continue;
		}
		const float64 prefillSec = clk.Elapsed().ToSeconds();

		// ---- Génération, AFFICHÉE AU FUR ET À MESURE -------------------------
		printf("\n\033[1mIA  >\033[0m ");
		fflush(stdout);

		NkVector<int32> produced;
		NkString shown; // ce qui est DÉJÀ à l'écran : sert à n'afficher que le delta
		const int32 stopId = tok.ImEndId();
		NkChrono genClk;
		int32 n = 0;
		for (; n < maxNew; ++n) {
			const int32 next = (temperature <= 0.0f) ? NkSampleGreedy(logits)
													 : NkSampleTopK(logits, temperature, 40, rng);
			if (next < 0 || next == stopId || next == tok.EndOfTextId())
				break;

			produced.PushBack(next);

			// Redécodage complet puis affichage du seul ajout — cf. l'en-tête :
			// un caractère accentué peut chevaucher deux tokens.
			const NkString full = tok.Decode(produced);
			if (full.Size() > shown.Size()) {
				printf("%s", full.CStr() + shown.Size());
				fflush(stdout);
				shown = full;
			}

			if (!model.Forward(&next, 1, logits, nullptr, &err)) {
				printf("\n  (échec du pas de génération : %s)\n", err.CStr());
				break;
			}
		}
		const float64 genSec = genClk.Elapsed().ToSeconds();
		lastMsPerTok = (n > 0) ? (genSec * 1000.0 / (float64)n) : 0.0;

		// Le token de fin de tour doit entrer dans le cache : sans lui, le tour
		// suivant s'enchaînerait sur une réponse que le modèle croit inachevée.
		if (stopId >= 0)
			model.Forward(&stopId, 1, logits, nullptr, &err);

		printf("\n\n  \033[2m%d tokens · préfill %.1f s · %.0f ms/token · contexte %d/%d\033[0m\n",
			   n, prefillSec, lastMsPerTok, model.CacheLength(), kMaxSeq);
	}

	printf("\n  À bientôt.\n");
	return 0;
}
