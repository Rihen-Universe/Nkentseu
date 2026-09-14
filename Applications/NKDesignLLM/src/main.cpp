// -----------------------------------------------------------------------------
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// NKDesignLLM — L'AUTRE COTE DU CONTRAT. Une invite dans un FICHIER, une reponse
//               dans un FICHIER, et rien d'autre ne traverse.
//
// =============================================================================
//  POURQUOI CE PROGRAMME EXISTE, ET POURQUOI IL EST SI PAUVRE
// =============================================================================
//  Rodolf a pose la meme exigence pour les DEUX generateurs de la maison :
//
//    « rassure-toi qu'il va a la longue permettre d'entrainer un modele plus
//      puissant qui fonctionne sur notre systeme de generation de modele
//      editable, image-to-3D, text-to-3D. — et ca doit etre pareil pour le
//      design UI. »
//
//  Cote 3D, la propriete est deja tenue : le modeleur ne connait qu'un contrat
//  de processus externe (`python <generateur> --image {image} --out {out}`) et
//  ignore tout de TripoSR. Le jour ou un modele entraine chez Rihen le
//  remplace, pas une ligne du modeleur ne bouge.
//
//  Ce programme est l'equivalent exact cote design. NkUIDesign ne le connait
//  pas : elle connait un GABARIT de ligne de commande a deux trous.
//
//      NKDesignLLM --invite <fichier> --sortie <fichier>
//      -> code 0 et le fichier de sortie existe        : c'est la reponse
//      -> code != 0 et « REFUS : ... » sur stderr      : et rien n'est ecrit
//
//  ⚠️ C'EST TOUT LE CONTRAT. Pas de jetons, pas de streaming, pas de fenetre de
//     contexte, pas de temperature negociee, pas d'outils. Chacune de ces
//     choses serait une capacite propre a CE moteur, et deviendrait la porte
//     par laquelle le remplacement serait impossible. Tout ce qu'un dorsal
//     futur doit savoir faire, c'est LIRE UN FICHIER ET EN ECRIRE UN AUTRE.
//
// =============================================================================
//  POURQUOI UN PROCESSUS SEPARE ET PAS UNE BIBLIOTHEQUE DANS L'EDITEUR
// =============================================================================
//  Mesure du 14/09 : le 7B quantifie occupe 4 444 Mo de VRAM sur une carte de
//  8 Go (4 168 de poids + 224 de KV-cache + 52 d'activations). L'en-tete de
//  `Applications/NKQwen2Chat` porte la mesure de Rodolf du 9 aout : deux
//  instances sur cette carte et « le pilote Vulkan ACCEPTE quand meme
//  l'allocation en debordant sur la memoire systeme. Aucun appel n'echoue --
//  CreateBuffer et Upload rendent tous deux reussi -- mais le calcul lit
//  n'importe quoi, et la generation sort !!!!!!! ».
//
//  NkUIDesign est elle-meme une application GPU. Charger le modele DANS son
//  processus, ce serait signer cet echec-qui-se-presente-en-vert. Ici le modele
//  vit dans SON processus, repond, et meurt : l'editeur ne porte jamais 4,4 Go.
//
//  ⚠️ ET LE VERROU D'INSTANCE UNIQUE EST LA POUR LA MEME RAISON. Aucune API GPU
//     ne dit « un autre PROCESSUS occupe deja la carte ». C'est une exclusion
//     entre processus, pas une question de VRAM disponible.
//
// =============================================================================
//  CE QUI N'EST PAS LA, ET QUI EST NOMME
// =============================================================================
//  - Aucun entrainement. L'adaptateur `--lora=<f.nkla>` est CHARGE s'il est
//    donne, mais rien ici n'en fabrique. L'entrainement du modele de design
//    n'est pas commence, et ce fichier ne pretend pas le contraire.
//  - Aucune troncature silencieuse. Une invite qui deborde la fenetre de
//    contexte fait un REFUS NOMME : tronquer donnerait une reponse plausible a
//    une question amputee, et personne ne saurait pourquoi elle est a cote.
//  - Aucun reglage de temperature : la generation est DETERMINISTE (greedy).
//    Deux fois la meme invite doivent donner la meme reponse -- c'est ce qui
//    rend un rejet reproductible, et c'est ce que la sonde de NkUIDesign
//    attend d'un dorsal.
// -----------------------------------------------------------------------------

#include "NKInfer/NkOllamaLocate.h"
#include "NKInfer/NkQwen2Gpu.h"
#include "NKInfer/NkQwen2Tokenizer.h"
#include "NKFileSystem/NkFile.h"
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

	/// Le refus, TOUJOURS par la meme porte : un seul endroit qui ecrit
	/// « REFUS : », donc un seul format a reconnaitre de l'autre cote.
	int Refus(const char *quoi, const char *detail = nullptr) {
		std::fprintf(stderr, "REFUS : %s%s%s\n", quoi, detail ? " -- " : "",
					 detail ? detail : "");
		std::fflush(stderr);
		return 1;
	}

	const char *ArgValue(int argc, char **argv, const char *key) {
		const usize klen = std::strlen(key);
		for (int i = 1; i < argc; ++i) {
			if (std::strncmp(argv[i], key, klen) == 0 && argv[i][klen] == '=')
				return argv[i] + klen + 1;
			// La forme separee « --invite <fichier> », parce que c'est celle que
			// le gabarit par defaut de NkUIDesign ecrit.
			if (std::strcmp(argv[i], key) == 0 && i + 1 < argc)
				return argv[i + 1];
		}
		return nullptr;
	}

	int32 ArgInt(int argc, char **argv, const char *key, int32 def) {
		const char *v = ArgValue(argc, argv, key);
		return v ? (int32)std::atoi(v) : def;
	}

} // namespace

int main(int argc, char **argv) {
#if defined(_WIN32)
	// SANS CECI LES ACCENTS SORTENT EN CHARABIA sur la console. Le fichier de
	// sortie, lui, est ecrit en UTF-8 quoi qu'il arrive : c'est lui qui compte.
	SetConsoleOutputCP(CP_UTF8);
	SetConsoleCP(CP_UTF8);
#endif

	const char *invite = ArgValue(argc, argv, "--invite");
	const char *sortie = ArgValue(argc, argv, "--sortie");
	if (!invite || !sortie) {
		std::printf(
			"NKDesignLLM -- l'autre cote du contrat de NkUIDesign.\n\n"
			"Usage : NKDesignLLM --invite <fichier> --sortie <fichier> [options]\n"
			"  --modele=<nom:tag|chemin.gguf>  le modele (defaut : le seul installe)\n"
			"  --tokens=<n>                    longueur maximale de la reponse (defaut 256)\n"
			"  --contexte=<n>                  fenetre de contexte (defaut 4096)\n"
			"  --lora=<f.nkla>                 adaptateur entraine chez Rihen (facultatif)\n"
			"  --systeme=<texte>               l'amorce systeme (facultatif)\n\n"
			"Rend 0 et ecrit <sortie>, ou rend non nul avec « REFUS : ... » sur stderr\n"
			"et n'ecrit RIEN. Aucun autre contrat -- c'est voulu : voir l'en-tete.\n");
		return 2;
	}

	if (!NkFile::Exists(invite))
		return Refus("fichier d'invite introuvable", invite);
	const NkString prompt = NkFile::ReadAllText(invite);
	if (prompt.Empty())
		return Refus("fichier d'invite vide", invite);

	// ⚠️ EFFACER LA SORTIE AVANT TOUT. « Le fichier existe apres » doit vouloir
	//    dire « CE lancement l'a ecrit ». Sans cette ligne, un fichier laisse
	//    par un lancement precedent ferait passer un echec pour une reussite --
	//    et rendrait la MEME reponse indefiniment. (L'appelant l'efface aussi de
	//    son cote ; deux gardes valent mieux, elles ne se contredisent pas.)
	if (NkFile::Exists(sortie))
		NkFile::Delete(sortie);

#if defined(_WIN32)
	// UNE SEULE INSTANCE A LA FOIS -- cf. l'en-tete. Le verrou n'est pas
	// relache : Windows le libere a la fin du processus, y compris tue. Un
	// verrou qu'on oublie de rendre apres un plantage bloquerait tous les
	// lancements suivants.
	{
		HANDLE once = CreateMutexA(nullptr, TRUE, "Global\\NKDesignLLM_instance_unique");
		if (once == nullptr || GetLastError() == ERROR_ALREADY_EXISTS)
			return Refus("une autre instance tourne deja",
						 "le modele occupe 4,4 Go ; deux instances n'entrent pas dans 8 Go, "
						 "et la seconde produirait du charabia SANS la moindre erreur");
	}
#endif

	// Backend verrouille sur Vulkan : output.weight = 447 Mo depasse les 128 Mo
	// garantis par ressource en D3D11 (lecon du jalon 5, NKQwen2Chat).
	if (const char *cur = std::getenv("NK_TENSOR_API")) {
		(void)cur;
	} else {
#if defined(_WIN32)
		_putenv_s("NK_TENSOR_API", "vulkan");
#else
		setenv("NK_TENSOR_API", "vulkan", 1);
#endif
	}

	// -- Le modele : nom, chemin, ou le seul installe -------------------------
	NkString gguf;
	if (const char *m = ArgValue(argc, argv, "--modele")) {
		if (NkFile::Exists(m)) {
			gguf = NkString(m);
		} else {
			NkString rerr;
			gguf = NkOllamaResolve(m, &rerr);
			if (gguf.Empty())
				return Refus("modele introuvable", rerr.CStr());
		}
	} else {
		NkVector<NkString> models;
		if (NkOllamaListModels(models) && models.Size() == 1) {
			NkString rerr;
			gguf = NkOllamaResolve(models[0].CStr(), &rerr);
		}
		if (gguf.Empty())
			return Refus("aucun modele designe et plusieurs (ou zero) installes",
						 "passez --modele=<nom:tag> ou --modele=<chemin.gguf>");
	}

	NkTensorGpu &gpu = NkTensorGpu::Get();
	if (!gpu.IsAvailable())
		return Refus("aucun peripherique de calcul GPU disponible");

	const int32 maxSeq = ArgInt(argc, argv, "--contexte", 4096);
	const int32 maxNew = ArgInt(argc, argv, "--tokens", 256);

	NkQwen2GpuOptions opt;
	opt.maxSeqLen = maxSeq;
	opt.maxBatchTokens = 64;
	opt.verbose = false;

	NkChrono clkLoad;
	NkQwen2Gpu model;
	NkString err;
	if (!model.Load(gguf.CStr(), opt, &err))
		return Refus("chargement du modele", err.CStr());
	const float64 loadSec = clkLoad.Elapsed().ToSeconds();

	// L'adaptateur entraine chez Rihen, FACULTATIF. C'est ce qui distinguera un
	// jour NOTRE modele du modele de base ; aujourd'hui il n'y en a aucun, et on
	// le DIT plutot que de laisser croire.
	if (const char *lora = ArgValue(argc, argv, "--lora")) {
		NkString lerr;
		if (model.LoadLora(lora))
			std::fprintf(stderr, "[NKDesignLLM] adaptateur : %s (rang %d)\n", lora,
						 model.LoraRank());
		else
			// On le DIT et on continue sur le socle : repondre avec le modele de
			// base en laissant croire qu'il est affine serait le pire des cas.
			std::fprintf(stderr, "[NKDesignLLM] !! adaptateur NON charge -- modele DE BASE\n");
	}

	NkQwen2Tokenizer tok;
	if (!tok.LoadFromGGUF(gguf.CStr(), &err))
		return Refus("chargement du vocabulaire", err.CStr());

	// -- Le tour ChatML -------------------------------------------------------
	const char *sys = ArgValue(argc, argv, "--systeme");
	NkString tour("<|im_start|>system\n");
	tour += sys && *sys
				? sys
				: "Tu produis exactement ce qui t'est demande, en francais, sans "
				  "explication, sans balise de code, sans commentaire.";
	tour += "<|im_end|>\n<|im_start|>user\n";
	tour += prompt;
	tour += "<|im_end|>\n<|im_start|>assistant\n";

	NkVector<int32> ids;
	if (!tok.EncodeWithSpecials(tour, ids) || ids.Size() == 0)
		return Refus("encodage de l'invite");

	// ⚠️ PAS DE TRONCATURE SILENCIEUSE. Une invite amputee donne une reponse
	//    plausible a une question qu'on n'a pas posee, et le defaut est alors
	//    cherche partout sauf ici.
	if ((int32)ids.Size() + maxNew > maxSeq) {
		char b[192];
		std::snprintf(b, sizeof(b),
					  "invite de %d tokens + %d de reponse > contexte de %d "
					  "(augmentez --contexte ou raccourcissez l'invite)",
					  (int)ids.Size(), (int)maxNew, (int)maxSeq);
		return Refus("invite trop longue", b);
	}

	// -- La generation, DETERMINISTE -----------------------------------------
	// ⚠️ ON N'ECRIT PAS SA PROPRE BOUCLE `Forward`, ET C'EST UNE MESURE, PAS UN
	//    gout. La premiere version de ce fichier pilotait Forward token par
	//    token, comme NKQwen2Chat. Elle a refuse la PREMIERE invite reelle :
	//
	//        REFUS : prefill -- Forward : count doit etre dans [1, maxBatchTokens]
	//
	//    L'invite de NkUIDesign fait 4 430 octets (le format + le catalogue
	//    engendre depuis le registre), soit de l'ordre de 1 200 tokens, et
	//    `maxBatchTokens` vaut 64. NKQwen2Chat ne rencontre jamais ce mur parce
	//    qu'on lui tape des questions d'une ligne ; un prompt de generation, lui,
	//    porte tout le catalogue.
	//
	//    `Generate` DECOUPE deja le prefill en tranches de `maxBatchTokens` (c'est
	//    ecrit dans NkQwen2Gpu.h). Ecrire une seconde fois ce decoupage ici
	//    aurait fait deux verites a tenir d'accord -- exactement ce que ce depot
	//    passe son temps a retirer. Et on ne perd rien : ce programme ECRIT UN
	//    FICHIER, il n'a aucun ecran a remplir au fur et a mesure.
	//
	//    temperature <= 0 -> glouton deterministe : deux fois la meme invite
	//    doivent donner la meme reponse, sinon un rejet n'est pas reproductible.
	NkChrono clkGen;
	NkVector<int32> produced;
	const int32 stopId = tok.ImEndId();
	uint32 rng = 42u;
	float64 preSec = 0.0, msPerTok = 0.0;
	if (!model.Generate(ids, maxNew, 0.0f, 0, rng, stopId, produced, &preSec, &msPerTok, &err))
		return Refus("generation", err.CStr());
	const float64 genSec = clkGen.Elapsed().ToSeconds();
	(void)genSec;

	if (produced.Size() == 0)
		return Refus("le modele n'a produit aucun token");

	// Le token d'arret peut se retrouver DANS la suite produite : il ne doit pas
	// atterrir dans le texte. On coupe a la premiere occurrence -- et on ne le
	// fait qu'ici, pas dans le decodage, pour que « combien de tokens produits »
	// reste le chiffre du modele et pas celui du nettoyage.
	{
		NkVector<int32> propre;
		for (uint32 i = 0; i < (uint32)produced.Size(); ++i) {
			if (produced[i] == stopId || produced[i] == tok.EndOfTextId())
				break;
			propre.PushBack(produced[i]);
		}
		if (propre.Size() == 0)
			return Refus("le modele n'a produit que son token d'arret");
		produced = propre;
	}
	const int32 n = (int32)produced.Size();

	// LE DECODAGE SE FAIT SUR LA SUITE ENTIERE, jamais token par token : le BPE
	// de Qwen est BYTE-LEVEL, un « e » accentue vaut deux octets qui peuvent
	// tomber dans DEUX tokens. Decoder chacun isolement rendrait des octets
	// orphelins -- du charabia la ou le modele a raison.
	const NkString reponse = tok.Decode(produced);
	if (reponse.Empty())
		return Refus("la reponse decodee est vide");

	if (!NkFile::WriteAllText(sortie, reponse.CStr()))
		return Refus("ecriture de la sortie", sortie);

	// Les chiffres vont sur stderr : stdout reste libre, et le SEUL resultat
	// est le fichier. Un appelant qui lirait stdout lirait des mesures.
	std::fprintf(stderr,
				 "[NKDesignLLM] %d tokens | chargement %.1f s | prefill %.1f s | %.0f ms/token\n",
				 (int)n, loadSec, preSec, msPerTok);
	return 0;
}
