// =============================================================================
// NKIlyana — le modèle de Rihen, entraîné depuis zéro sur son propre corpus.
// -----------------------------------------------------------------------------
// PREMIER JALON, ET SEULEMENT LUI : prouver la chaîne complète — tokenizer,
// données, architecture, entraînement, génération — sur un modèle d'environ
// 20 millions de paramètres, en quelques heures de GPU. Le but n'est PAS un
// modèle utile : avec 5,2 millions de tokens pour 20 millions de paramètres, on
// est à un cinquantième de ce qu'il faudrait, et il apprendra son corpus par
// cœur. C'est assumé. Wikipédia et la vraie taille viennent après.
//
// TROIS MODES :
//   --data   prépare le corpus (tri en trois bacs + identité) et le tokenizer
//   --train  entraîne
//   --parler cause avec le modèle entraîné
//
// SUR L'ARCHITECTURE — À LIRE AVANT DE S'ÉTONNER. Ilyana n'est pas bâtie sur le
// bloc de `NKInfer/NkQwen2Gpu` (RoPE, RMSNorm, SwiGLU, attention par groupes)
// mais sur `nn::NkGPT` (pré-LN, positions apprises, MLP GELU, attention
// multi-têtes). Raison : le bloc Qwen2 du dépôt n'a de rétropropagation que
// pour des adaptateurs LoRA — `NkQwen2Backward` le dit explicitement, « les
// poids du socle sont GELÉS », il ne produit AUCUN gradient pour wq/wk/wv/wo ni
// pour le MLP, et il tourne sur CPU avec B=1. Entraîner depuis zéro sur ce
// chemin demanderait d'écrire tous les gradients manquants ET de les porter sur
// GPU. `nn::NkGPT`, lui, passe par NKAutograd, tourne entièrement sur GPU et a
// déjà entraîné les paliers 1 à 3. On prouve la chaîne sur ce qui marche ;
// RoPE/RMSNorm/SwiGLU sont une amélioration identifiée, pas un préalable.
// AUTEUR : Rihen — LICENCE : Propriétaire - usage régi par le fichier LICENSE à la racine du dépôt
// =============================================================================
#include "NkIlyanaIdentite.h"
#include "NkIlyanaDialogues.h"
#include "NkIlyanaValeurs.h"
#include "NkIlyanaTri.h"

#include "NKGpt/NkGptTrainer.h"
#include "NKData/NkBpeTrainer.h"
#include "NKLogger/NkLog.h"
#include "NKTime/NkChrono.h"
#include "NKContainers/String/NkString.h"
#include "NKContainers/Sequential/NkVector.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

using namespace nkentseu;
using namespace nkentseu::ai;

// -----------------------------------------------------------------------------
// Petits utilitaires fichiers (FILE*, comme le reste de NKAI)
// -----------------------------------------------------------------------------
static NkString LireFichier(const char *path) {
	NkString s;
	FILE *f = fopen(path, "rb");
	if (!f)
		return s;
	char buf[1 << 16];
	for (;;) {
		const nk_size got = fread(buf, 1, sizeof(buf), f);
		if (got == 0)
			break;
		s.Append(buf, got);
	}
	fclose(f);
	return s;
}

// Supprime les retours chariot. DEUX raisons, pas une :
//  - les blocs Question/Reponse sont séparés par une ligne vide, donc par
//    « \r\n\r\n » dans un fichier Windows ; tout le code de découpage du dépôt
//    cherche « \n\n » et ne trouve alors RIEN — le corpus entier passe pour un
//    seul bloc, sans la moindre erreur visible (constaté ici : 1 paire au lieu
//    de 100 017) ;
//  - un « \r » est un octet de plus à apprendre à chaque ligne, qui ne porte
//    aucun sens.
static NkString NormaliserFinsDeLigne(const NkString &s) {
	NkString o;
	o.Reserve(s.Size());
	const char *p = s.Data();
	const nk_size n = s.Size();
	for (nk_size i = 0; i < n; ++i)
		if (p[i] != '\r')
			o.Append(p[i]);
	return o;
}

// Retire le « --- » que le generateur du corpus a colle a la fin de certaines
// reponses. MESURE : 36 334 reponses sur 100 017 en portent un, les autres non.
// Ce n'est donc meme pas une convention de fin de reponse que le modele pourrait
// apprendre proprement : c'est un artefact applique AU HASARD une fois sur trois,
// et il ressort tel quel dans le texte genere.
static NkString RetirerTiretsFinaux(const NkString &bloc) {
	nk_size n = bloc.Size();
	// Fin de bloc : blancs, puis une suite de '-', puis blancs.
	while (n > 0) {
		const char c = bloc.Data()[n - 1];
		if (c == ' ' || c == '\t' || c == '\n' || c == '\r')
			--n;
		else
			break;
	}
	nk_size fin = n;
	while (n > 0 && bloc.Data()[n - 1] == '-')
		--n;
	if (n == fin)
		return bloc; // pas de tirets a retirer
	if (fin - n < 2)
		return bloc; // un tiret isole peut appartenir a un mot compose
	while (n > 0) {
		const char c = bloc.Data()[n - 1];
		if (c == ' ' || c == '\t')
			--n;
		else
			break;
	}
	return bloc.SubStr(0, n);
}

static bool EcrireFichier(const char *path, const NkString &s) {
	FILE *f = fopen(path, "wb");
	if (!f)
		return false;
	const bool ok = s.Size() == 0 || fwrite(s.Data(), 1, s.Size(), f) == s.Size();
	fclose(f);
	return ok;
}

static NkString Joindre(const char *dir, const char *nom) {
	NkString p(dir);
	if (p.Size() > 0) {
		const char last = p.Data()[p.Size() - 1];
		if (last != '/' && last != '\\')
			p.Append('/');
	}
	p.Append(nom);
	return p;
}

// Lecture d'un argument nommé : --cle valeur
static const char *Arg(int argc, char **argv, const char *cle, const char *defaut) {
	for (int i = 1; i + 1 < argc; ++i)
		if (strcmp(argv[i], cle) == 0)
			return argv[i + 1];
	return defaut;
}

static bool Drapeau(int argc, char **argv, const char *cle) {
	for (int i = 1; i < argc; ++i)
		if (strcmp(argv[i], cle) == 0)
			return true;
	return false;
}

static int64 ArgEntier(int argc, char **argv, const char *cle, int64 defaut) {
	const char *v = Arg(argc, argv, cle, nullptr);
	return v ? (int64)strtoll(v, nullptr, 10) : defaut;
}

static double ArgReel(int argc, char **argv, const char *cle, double defaut) {
	const char *v = Arg(argc, argv, cle, nullptr);
	return v ? strtod(v, nullptr) : defaut;
}

// =============================================================================
// MODE --data : trier le corpus, écrire l'identité, entraîner le tokenizer
// =============================================================================
static int ModeData(int argc, char **argv) {
	const char *source = Arg(argc, argv, "--source", "D:/Projets/Camrail/AI/BulkGen/dlg_ollama_fr.txt");
	const char *sortie = Arg(argc, argv, "--sortie", "D:/Projets/Camrail/AI/Ilyana");
	const int repetitions = (int)ArgEntier(argc, argv, "--repetitions", 12);
	const int vocab = (int)ArgEntier(argc, argv, "--vocab", 16384);
	const bool avecQuarantaine = Drapeau(argc, argv, "--avec-quarantaine");
	// Regenerer le corpus SANS toucher au tokenizer. Indispensable pour corriger
	// le corpus d'identite en cours de route : un entrainement ne peut REPRENDRE
	// depuis un checkpoint que si le decoupage en tokens est le meme — les
	// embeddings sont indexes par identifiant de token, un vocabulaire different
	// les rendrait tous faux, sans la moindre erreur visible.
	const bool garderTokenizer = Drapeau(argc, argv, "--garder-tokenizer");

	logger.Info("=== Ilyana / preparation des donnees ===");
	logger.Info("source      : {0}", source);
	logger.Info("sortie      : {0}", sortie);

	NkString lu = LireFichier(source);
	if (lu.Size() < 1000) {
		logger.Info("ERREUR : corpus introuvable ou trop court ({0} octets).", (unsigned long long)lu.Size());
		return 1;
	}
	NkString brut = NormaliserFinsDeLigne(lu);
	logger.Info("corpus lu   : {0} octets ({1} retours chariot retires)", (unsigned long long)brut.Size(),
				(unsigned long long)(lu.Size() - brut.Size()));

	// ---- Tri en trois bacs, bloc par bloc (un bloc = une paire Question/Reponse)
	NkString bacs[3];
	int64 compte[3] = {0, 0, 0};
	int64 nettoyes = 0;
	int64 blocsGroupes = 0, dialoguesSynthetiques = 0;
	{
		// Regroupement en dialogues. Les paires produites par le generateur se
		// suivent PAR THEME (elles ont ete engendrees par lots) : enchainer trois
		// paires consecutives donne donc un echange a peu pres coherent, sans
		// avoir a mesurer la moindre similarite. On n'en groupe qu'UN TIERS : si
		// tout le corpus devenait multi-tours, elle n'apprendrait plus a repondre
		// a une question posee seule.
		const int kTaille = 3;
		NkString tampon[3][kTaille];
		int nTampon[3] = {0, 0, 0};
		int64 groupe[3] = {0, 0, 0};

		auto viderTampon = [&](int b) {
			if (nTampon[b] == 0)
				return;
			const bool enDialogue = (nTampon[b] == kTaille) && (groupe[b] % 3 == 0);
			if (enDialogue) {
				for (int i = 0; i < nTampon[b]; ++i) {
					bacs[b].Append(tampon[b][i]);
					bacs[b].Append("\n"); // simple retour : on reste DANS le meme bloc
				}
				bacs[b].Append("\n"); // ligne vide = fin du dialogue
				++dialoguesSynthetiques;
				blocsGroupes += nTampon[b];
			} else {
				for (int i = 0; i < nTampon[b]; ++i) {
					bacs[b].Append(tampon[b][i]);
					bacs[b].Append("\n\n");
				}
			}
			++groupe[b];
			nTampon[b] = 0;
		};

		nk_size pos = 0;
		const nk_size n = brut.Size();
		while (pos < n) {
			const nk_size fin = brut.Find("\n\n", pos);
			const nk_size len = (fin == NkString::npos) ? (n - pos) : (fin - pos);
			NkString bloc = brut.SubStr(pos, len);
			pos = (fin == NkString::npos) ? n : fin + 2;
			if (bloc.Size() < 8)
				continue;
			const nk_size avant = bloc.Size();
			bloc = RetirerTiretsFinaux(bloc);
			if (bloc.Size() != avant)
				++nettoyes;
			const int b = (int)ilyana::Classer(bloc);
			tampon[b][nTampon[b]] = bloc;
			++nTampon[b];
			++compte[b];
			if (nTampon[b] == kTaille)
				viderTampon(b);
		}
		for (int b = 0; b < 3; ++b)
			viderTampon(b);
	}
	logger.Info("dialogues   : {0} paires regroupees en {1} echanges a plusieurs tours (un tiers des paires)",
				(long long)blocsGroupes, (long long)dialoguesSynthetiques);
	logger.Info("nettoyage   : {0} blocs debarrasses d'un « --- » final du generateur", (long long)nettoyes);
	const int64 total = compte[0] + compte[1] + compte[2];
	logger.Info("tri         : {0} paires -> verifiable {1} ({2}%), quarantaine {3} ({4}%), neutre {5} ({6}%)",
				(long long)total, (long long)compte[0], total ? (100.0 * (double)compte[0] / (double)total) : 0.0,
				(long long)compte[1], total ? (100.0 * (double)compte[1] / (double)total) : 0.0, (long long)compte[2],
				total ? (100.0 * (double)compte[2] / (double)total) : 0.0);

	EcrireFichier(Joindre(sortie, "bac_verifiable.txt").CStr(), bacs[0]);
	EcrireFichier(Joindre(sortie, "bac_quarantaine.txt").CStr(), bacs[1]);
	EcrireFichier(Joindre(sortie, "bac_neutre.txt").CStr(), bacs[2]);

	// ---- Identité --------------------------------------------------------
	NkString identite;
	const int64 pairesIdent = ilyana::EcrireIdentite(identite, repetitions);
	// Les echanges a plusieurs tours SUR ELLE-MEME : c'est la seule matiere sur
	// laquelle elle sait quelque chose, donc la seule ou tenir un fil a du sens.
	const int64 toursDlg = ilyana::EcrireDialogues(identite, repetitions);
	// La charte : dire « je ne sais pas », tenir ferme SANS s'enteter, refuser de
	// juger la sincerite de quiconque, respecter, connaitre ses limites.
	ilyana::EquilibreCharte eq;
	const int64 toursVal = ilyana::EcrireValeurs(identite, repetitions, &eq);
	if (!EcrireFichier(Joindre(sortie, "identite.txt").CStr(), identite)) {
		// Une ecriture qui echoue en silence (dossier de sortie inexistant) laisse
		// croire que le corpus est pret alors qu'il n'existe pas.
		logger.Info("ERREUR : ecriture impossible dans {0} — le dossier existe-t-il ?", sortie);
		return 1;
	}
	logger.Info("identite    : {0} paires + {1} tours de dialogue + {2} tours de charte ({3} octets)",
				(long long)pairesIdent, (long long)toursDlg, (long long)toursVal,
				(unsigned long long)identite.Size());
	// L'equilibre de la charte est ce qui distingue la fermete de l'entetement :
	// on le MESURE au lieu de le supposer. Un desequilibre marque signifierait
	// qu'on remplace un defaut par son symetrique.
	logger.Info("charte      : {0} tours de FERMETE (contredite a tort) contre {1} d'HUMILITE "
				"(contredite a raison) — rapport {2}",
				(long long)eq.fermete, (long long)eq.humilite,
				eq.humilite ? (double)eq.fermete / (double)eq.humilite : 0.0);

	// ---- Assemblage du corpus d'entraînement ------------------------------
	// L'identité vient EN TÊTE : les premières séquences vues comptent, et cela
	// garantit qu'elle est présente même si le corpus est tronqué par maxChars.
	NkString corpus;
	corpus.Append(identite);
	corpus.Append(bacs[0]);
	corpus.Append(bacs[2]);
	if (avecQuarantaine)
		corpus.Append(bacs[1]);
	const NkString cheminCorpus = Joindre(sortie, "fr_ilyana.txt");
	if (!EcrireFichier(cheminCorpus.CStr(), corpus)) {
		logger.Info("ERREUR : ecriture du corpus impossible ({0}).", cheminCorpus.CStr());
		return 1;
	}
	const double partIdent = corpus.Size() ? (100.0 * (double)identite.Size() / (double)corpus.Size()) : 0.0;
	logger.Info("corpus     : {0} octets ecrits dans {1}", (unsigned long long)corpus.Size(), cheminCorpus.CStr());
	logger.Info("             quarantaine {0}, part de l'identite {1}% du corpus",
				avecQuarantaine ? "INCLUSE" : "EXCLUE", partIdent);
	if (partIdent < 0.5)
		logger.Info("             ATTENTION : sous ~0,5%%, l'identite risque de ne pas etre retenue.");

	// ---- Tokenizer sur CE corpus (pas sur un autre) -----------------------
	// Le tokenizer doit être appris sur le texte que le modèle verra : un
	// tokenizer entraîné ailleurs découperait mal les tournures propres à ce
	// corpus, à commencer par le nom du père.
	const NkString cheminBpe = Joindre(sortie, "ilyana.nkbpe");
	data::NkBpe bpe;
	if (garderTokenizer) {
		if (!data::LoadBpe(cheminBpe.CStr(), bpe)) {
			logger.Info("ERREUR : tokenizer existant introuvable ({0}) — impossible de le garder.",
						cheminBpe.CStr());
			return 1;
		}
		logger.Info("tokenizer  : CONSERVE tel quel ({0} fusions) — un entrainement en cours peut donc "
					"reprendre sur ce nouveau corpus.",
					(unsigned long long)bpe.merges.Size());
	} else {
		data::NkBpeTrainConfig bcfg;
		bcfg.targetVocab = vocab;
		bcfg.pretok = data::NK_PRETOK_WORD_PUNCT;
		bcfg.verbose = true;
		NkVector<NkString> textes;
		textes.PushBack(corpus);
		data::NkBpeTrainStats bst;
		NkChrono chrono;
		if (!data::TrainBpeFast(textes, bcfg, bpe, &bst)) {
			logger.Info("ERREUR : entrainement du tokenizer.");
			return 1;
		}
		if (!data::SaveBpe(cheminBpe.CStr(), bpe)) {
			logger.Info("ERREUR : ecriture du tokenizer ({0}).", cheminBpe.CStr());
			return 1;
		}
		logger.Info("tokenizer  : {0} fusions en {1} s -> {2}", (long long)bst.merges, chrono.Elapsed().seconds,
					cheminBpe.CStr());
		logger.Info("             corpus = {0} tokens ({1} octets/token)", (long long)bst.finalSymbolsWeighted,
					bst.finalSymbolsWeighted ? (double)corpus.Size() / (double)bst.finalSymbolsWeighted : 0.0);
	}

	// ---- Contrôle : le nom du père survit-il a l'aller-retour ? -----------
	// Un tokenizer qui massacrerait justement les mots qu'on veut faire retenir
	// serait un echec silencieux. On le verifie explicitement.
	{
		NkString phrase("Question: Qui est ton pere ?\nReponse: Mon pere est TEUGUIA TADJUIDJE Rodolf Sederis.\n");
		NkVector<int32> ids;
		bpe.Encode(phrase, ids);
		const NkString retour = data::DecodeAll(bpe, ids);
		logger.Info("controle   : la phrase d'identite fait {0} tokens, aller-retour {1}", (long long)ids.Size(),
					(retour == phrase) ? "EXACT" : "*** ROMPU ***");
		if (!(retour == phrase))
			return 1;

		// Coût en tokens de chaque nom. Le tokenizer conservé n'a jamais vu celui
		// de la mère : il le découpe donc plus finement. On le MESURE au lieu de
		// l'affirmer — c'est ce nombre qui dit combien de fois elle devra le lire
		// pour le retenir.
		auto compter = [&](const char *txt) {
			NkVector<int32> t;
			bpe.Encode(NkString(txt), t);
			NkString rt = data::DecodeAll(bpe, t);
			const bool exact = (rt == NkString(txt));
			return exact ? (int64)t.Size() : (int64)-1;
		};
		const int64 nPere = compter("TEUGUIA TADJUIDJE Rodolf Sederis");
		const int64 nMere = compter("KEBEYENG BODOFIA Alfonsine Armelle Sarah");
		logger.Info("             nom du pere : {0} tokens · nom de la mere : {1} tokens (-1 = aller-retour rompu)",
					(long long)nPere, (long long)nMere);
		if (nPere < 0 || nMere < 0) {
			logger.Info("ERREUR : un nom ne survit pas a l'aller-retour du tokenizer.");
			return 1;
		}
	}

	logger.Info("=== donnees pretes ===");
	logger.Info("Entrainement :  NKIlyana.exe --train --corpus {0} --bpe {1}", cheminCorpus.CStr(), cheminBpe.CStr());
	return 0;
}

// =============================================================================
// MODE --train
// =============================================================================
static int64 CompterParametres(int64 V, int64 d, int64 L, int64 T) {
	// Décompte exact de nn::NkGPT : embeddings token + position, L blocs
	// (2 LayerNorm affines, 4 projections d->d avec biais, MLP d->4d->d), un
	// LayerNorm final, une tête d->V avec biais.
	const int64 emb = V * d + T * d;
	const int64 bloc = 2 * (2 * d) + 4 * (d * d + d) + (d * 4 * d + 4 * d) + (4 * d * d + d);
	const int64 tete = d * V + V;
	return emb + L * bloc + 2 * d + tete;
}

static int ModeTrain(int argc, char **argv) {
	gpt::NkGptConfig cfg;
	cfg.corpusFile = NkString(Arg(argc, argv, "--corpus", "D:/Projets/Camrail/AI/Ilyana/fr_ilyana.txt"));
	cfg.bpePath = NkString(Arg(argc, argv, "--bpe", "D:/Projets/Camrail/AI/Ilyana/ilyana.nkbpe"));
	cfg.savePath = NkString(Arg(argc, argv, "--save", "D:/Projets/Camrail/AI/Ilyana/ilyana.nkgp"));
	cfg.qaMarker = NkString("Reponse: "); // le corpus n'accentue pas ce marqueur
	cfg.maxChars = (nk_size)ArgEntier(argc, argv, "--maxchars", 60000000);

	cfg.T = ArgEntier(argc, argv, "--T", 256);
	cfg.d = ArgEntier(argc, argv, "--d", 384);
	cfg.H = ArgEntier(argc, argv, "--heads", 6);
	cfg.L = ArgEntier(argc, argv, "--layers", 4);
	cfg.B = ArgEntier(argc, argv, "--B", 6);

	cfg.steps = (int)ArgEntier(argc, argv, "--steps", 4000);
	cfg.accum = (int)ArgEntier(argc, argv, "--accum", 4);
	cfg.lr = (float)ArgReel(argc, argv, "--lr", 3e-4);
	cfg.warmup = (int)ArgEntier(argc, argv, "--warmup", -1);
	cfg.saveEvery = (int)ArgEntier(argc, argv, "--saveevery", 200);
	cfg.valFrac = (float)ArgReel(argc, argv, "--valfrac", 0.02);
	cfg.valEvery = (int)ArgEntier(argc, argv, "--valevery", 250);

	const char *load = Arg(argc, argv, "--load", nullptr);
	if (load) {
		cfg.loadPath = NkString(load);
		cfg.resume = true;
	}
	cfg.seed = NkString(Arg(argc, argv, "--amorce", "Question: Qui est ton pere ?\nReponse:"));
	cfg.genLen = (int)ArgEntier(argc, argv, "--genlen", 120);
	// Chaque sequence d'entrainement commence par un token-etiquette de langue.
	// Generer SANS cette etiquette placerait le modele dans une situation qu'il
	// n'a jamais vue a la premiere position — on la lui donne donc aussi ici.
	cfg.genLang = NkString("fr");
	cfg.verbose = true;

	logger.Info("=== Ilyana / entrainement ===");
	logger.Info("corpus {0}", cfg.corpusFile.CStr());
	logger.Info("tokenizer {0}", cfg.bpePath.CStr());
	logger.Info("T={0} d={1} tetes={2} couches={3} B={4} accum={5} pas={6} lr={7}", (long long)cfg.T,
				(long long)cfg.d, (long long)cfg.H, (long long)cfg.L, (long long)cfg.B, cfg.accum, cfg.steps,
				(double)cfg.lr);

	// Un ordre de grandeur AVANT de lancer : c'est le moment de s'apercevoir
	// qu'on a demandé un modèle deux fois trop gros pour 8 Go.
	{
		data::NkBpe sonde;
		if (data::LoadBpe(cfg.bpePath.CStr(), sonde)) {
			const int64 V = 256 + (int64)sonde.merges.Size() + 1; // +1 tag de langue
			const int64 P = CompterParametres(V, cfg.d, cfg.L, cfg.T);
			const double moPar = (double)P * 4.0 / (1024.0 * 1024.0);
			logger.Info("vocabulaire {0} -> {1} parametres ({2} Mo de poids, ~{3} Mo avec gradients et Adam)",
						(long long)V, (long long)P, moPar, moPar * 4.0);
			const int64 emb = V * cfg.d + cfg.d * V + V;
			logger.Info("             dont {0}% dans les embeddings et la tete de sortie",
						P ? (100.0 * (double)emb / (double)P) : 0.0);
		}
	}

	gpt::NkGptTrainer t(cfg);
	if (!t.Prepare()) {
		logger.Info("ERREUR : preparation impossible.");
		return 1;
	}
	logger.Info("GPU : {0}", t.UseGpu() ? "OUI (entrainement resident)" : "NON (repli CPU, ce sera tres lent)");

	NkChrono chrono;
	t.Fit();
	logger.Info("Entrainement termine en {0} s.", chrono.Elapsed().seconds);

	// NE PAS rappeler t.Save() ici. `Fit()` a DEJA ecrit le checkpoint final AVEC
	// l'etat de l'optimiseur (moments d'Adam + pas global), ce qui permet une
	// reprise exacte. `NkGptTrainer::Save()` ecrit les POIDS SEULS : l'appeler
	// apres Fit ecraserait le bon fichier par une version degradee, et la reprise
	// repartirait sans etat d'Adam — avec un pic de perte. Constate a la mesure :
	// 8,8 Mo au lieu de 26,1 Mo, soit exactement le tiers (poids sans les deux
	// moments).

	// La question qui donne son sens au jalon.
	logger.Info("--- Ce qu'elle repond ---");
	NkString r = t.Generate(NkString("Question: Qui est ton pere ?\nReponse:"), 60, 0.8, t.GenLangIndex());
	logger.Info("{0}", r.CStr());
	NkString r2 = t.Generate(NkString("Question: Comment tu t'appelles ?\nReponse:"), 60, 0.8, t.GenLangIndex());
	logger.Info("{0}", r2.CStr());
	return 0;
}

// =============================================================================
// MODE --parler : recharger un modèle et l'interroger
// =============================================================================
static int ModeParler(int argc, char **argv) {
	gpt::NkGptConfig cfg;
	cfg.loadPath = NkString(Arg(argc, argv, "--load", "D:/Projets/Camrail/AI/Ilyana/ilyana.nkgp"));
	cfg.bpePath = NkString(Arg(argc, argv, "--bpe", "D:/Projets/Camrail/AI/Ilyana/ilyana.nkbpe"));
	cfg.resume = false;
	cfg.verbose = true;
	cfg.genLen = (int)ArgEntier(argc, argv, "--genlen", 80);
	cfg.genLang = NkString("fr"); // meme etiquette de langue qu'a l'entrainement

	gpt::NkGptTrainer t(cfg);
	if (!t.Prepare()) {
		logger.Info("ERREUR : modele illisible.");
		return 1;
	}
	const char *q = Arg(argc, argv, "--question", nullptr);
	gpt::NkSampleParams sp;
	sp.temperature = ArgReel(argc, argv, "--temp", 0.8);
	sp.topK = (int)ArgEntier(argc, argv, "--topk", 40);
	sp.topP = ArgReel(argc, argv, "--topp", 0.95);

	if (q) {
		NkString amorce("Question: ");
		amorce.Append(q);
		amorce.Append("\nReponse:");
		logger.Info("{0}", t.Generate(amorce, cfg.genLen, sp, t.GenLangIndex()).CStr());
		return 0;
	}
	// Sans question : une petite batterie fixe, pour voir d'un coup d'oeil ce
	// qu'elle a retenu.
	static const char *kQuestions[] = {"Qui est ton pere ?",		"Comment tu t'appelles ?",
									   "Qui es-tu ?",			"Dans quel langage es-tu ecrite ?",
									   "Quelle langue parles-tu ?", "Sais-tu tout ?"};
	for (int i = 0; i < (int)(sizeof(kQuestions) / sizeof(kQuestions[0])); ++i) {
		NkString amorce("Question: ");
		amorce.Append(kQuestions[i]);
		amorce.Append("\nReponse:");
		logger.Info("--- {0}", kQuestions[i]);
		logger.Info("{0}", t.Generate(amorce, cfg.genLen, sp, t.GenLangIndex()).CStr());
	}
	return 0;
}

// =============================================================================
// MODE --causer : poser des questions au clavier, sans recharger le modele
// -----------------------------------------------------------------------------
// `--parler` relance tout le processus par question : Vulkan, les noyaux, les
// 20 M de poids. Pour ESSAYER le modele, il faut pouvoir enchainer.
//
// ⚠️ CE N'EST PAS UNE CONVERSATION, et il faut le savoir avant d'essayer.
// Ilyana a ete entrainee sur des blocs « Question:/Reponse: » INDEPENDANTS,
// separes par une ligne vide, la question masquee dans la perte. Elle n'a
// JAMAIS vu d'echange a plusieurs tours. Chaque question est donc traitee seule,
// sans memoire de la precedente — lui envoyer l'historique la placerait dans une
// situation qu'elle n'a jamais rencontree a l'entrainement. Un vrai dialogue
// suppose un corpus multi-tours : c'est une etape a part entiere, pas un reglage.
// =============================================================================
static int ModeCauser(int argc, char **argv) {
	setvbuf(stdout, nullptr, _IONBF, 0);
	gpt::NkGptConfig cfg;
	cfg.loadPath = NkString(Arg(argc, argv, "--load", "D:/Projets/Camrail/AI/Ilyana/ilyana.nkgp"));
	cfg.bpePath = NkString(Arg(argc, argv, "--bpe", "D:/Projets/Camrail/AI/Ilyana/ilyana.nkbpe"));
	cfg.resume = false;
	cfg.verbose = false;
	cfg.genLang = NkString("fr");

	gpt::NkGptTrainer t(cfg);
	if (!t.Prepare()) {
		printf("ERREUR : modele illisible (%s).\n", cfg.loadPath.CStr());
		return 1;
	}
	gpt::NkSampleParams sp;
	sp.temperature = ArgReel(argc, argv, "--temp", 0.7);
	sp.topK = (int)ArgEntier(argc, argv, "--topk", 40);
	sp.topP = ArgReel(argc, argv, "--topp", 0.9);
	int genLen = (int)ArgEntier(argc, argv, "--genlen", 60);

	printf("\n=== Ilyana ===\n");
	printf("Modele : %s\n", cfg.loadPath.CStr());
	printf("Chaque question est traitee SEULE : elle n'a pas ete entrainee au dialogue\n");
	printf("a plusieurs tours, elle ne se souvient donc pas de la question precedente.\n");
	printf("Commandes : /quitter, /temp <x>, /len <n>\n\n");

	char ligne[2048];
	for (;;) {
		printf("> ");
		if (!fgets(ligne, sizeof(ligne), stdin))
			break;
		nk_size n = 0;
		while (ligne[n] != '\0' && ligne[n] != '\n' && ligne[n] != '\r')
			++n;
		ligne[n] = '\0';
		if (n == 0)
			continue;
		if (strcmp(ligne, "/quitter") == 0 || strcmp(ligne, "/quit") == 0)
			break;
		if (strncmp(ligne, "/temp ", 6) == 0) {
			sp.temperature = strtod(ligne + 6, nullptr);
			printf("(temperature = %.2f)\n", sp.temperature);
			continue;
		}
		if (strncmp(ligne, "/len ", 5) == 0) {
			genLen = (int)strtol(ligne + 5, nullptr, 10);
			printf("(longueur = %d tokens)\n", genLen);
			continue;
		}
		NkString amorce("Question: ");
		amorce.Append(ligne);
		amorce.Append("\nReponse:");
		NkString rep = t.Generate(amorce, genLen, sp, t.GenLangIndex());
		// Ne garder que la reponse, et s'arreter au bloc suivant : le modele
		// enchaine volontiers sur une nouvelle paire Question:/Reponse:, ce qui
		// n'interesse personne.
		const nk_size deb = rep.Find("Reponse:");
		NkString sortie = (deb == NkString::npos) ? rep : rep.SubStr(deb + 8);
		const nk_size fin = sortie.Find("Question:");
		if (fin != NkString::npos)
			sortie = sortie.SubStr(0, fin);
		printf("%s\n\n", sortie.CStr());
	}
	printf("\nA bientot.\n");
	return 0;
}

int main(int argc, char **argv) {
	if (Drapeau(argc, argv, "--data"))
		return ModeData(argc, argv);
	if (Drapeau(argc, argv, "--causer"))
		return ModeCauser(argc, argv);
	if (Drapeau(argc, argv, "--train"))
		return ModeTrain(argc, argv);
	if (Drapeau(argc, argv, "--parler"))
		return ModeParler(argc, argv);

	logger.Info("NKIlyana — le modele de Rihen.");
	logger.Info("  --data    [--source f] [--sortie d] [--repetitions n] [--vocab n] [--avec-quarantaine]");
	logger.Info("  --train   [--corpus f] [--bpe f] [--save f] [--load f] [--steps n] [--d n] [--layers n]");
	logger.Info("            [--heads n] [--T n] [--B n] [--accum n] [--lr x] [--saveevery n]");
	logger.Info("  --parler  [--load f] [--bpe f] [--question \"...\"] [--temp x] [--topk n] [--topp x]");
	return 0;
}
