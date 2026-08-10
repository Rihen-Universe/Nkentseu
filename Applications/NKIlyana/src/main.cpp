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
#include "NkIlyanaControle.h"
#include "NkIlyanaRecherche.h"
#include "NkIlyanaBibliotheque.h"
#include "NKMedia/Document/NkEpub.h"
#include "NKMedia/Document/NkLatex.h"
#include "NkIlyanaPdf.h"
#include "NkIlyanaCitation.h"

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
// MODE --controle : la batterie. Elle repond OUI ou NON, pas « ca semble bon ».
// =============================================================================
// Coupe la generation a la fin de la reponse : le modele continue volontiers sur
// une question suivante qu'il s'invente. Sans cette coupe, on evaluerait aussi
// ce bavardage.
static NkString CouperReponse(const NkString &brut) {
	// ⚠️ `Generate` rend l'AMORCE SUIVIE de la génération, pas la génération
	// seule. Chercher « Question: » sans le savoir le trouve en position 0 et ne
	// renvoie que du vide : la batterie a annoncé 1/19 sans qu'une seule réponse
	// ait été lue — et son unique « OK » passait parce qu'une chaîne vide ne
	// contient aucun interdit. Un score qui ne mesure rien est pire que pas de
	// score du tout.
	// On repart donc du DERNIER « Reponse: » (celui du tour courant), et on coupe
	// ce qui le suit.
	nk_size debut = 0;
	nk_size p = brut.Find("Reponse:");
	while (p != NkString::npos) {
		debut = p + 8; // longueur de « Reponse: »
		const nk_size suiv = brut.Find("Reponse:", debut);
		if (suiv == NkString::npos)
			break;
		p = suiv;
	}
	NkString suite = brut.SubStr(debut);

	nk_size fin = suite.Size();
	static const char *kBornes[] = {"\nQuestion:", "\n\n", "Question:"};
	for (int i = 0; i < 3; ++i) {
		const nk_size b = suite.Find(kBornes[i]);
		if (b != NkString::npos && b < fin)
			fin = b;
	}
	return suite.SubStr(0, fin);
}

static int ModeControle(int argc, char **argv) {
	gpt::NkGptConfig cfg;
	cfg.loadPath = NkString(Arg(argc, argv, "--load", "D:/Projets/Camrail/AI/Ilyana2/ilyana.nkgp"));
	cfg.bpePath = NkString(Arg(argc, argv, "--bpe", "D:/Projets/Camrail/AI/Ilyana2/ilyana.nkbpe"));
	cfg.genLang = NkString("fr");
	cfg.verbose = false;
	const int genLen = (int)ArgEntier(argc, argv, "--genlen", 48);

	gpt::NkGptTrainer t(cfg);
	if (!t.Prepare()) {
		logger.Info("ERREUR : modele illisible ({0}).", cfg.loadPath.CStr());
		return 2;
	}

	// GLOUTON : temperature nulle, un seul candidat. Deux executions du meme
	// modele doivent donner le meme score, sinon on compare du bruit.
	gpt::NkSampleParams sp;
	sp.temperature = 0.01;
	sp.topK = 1;
	sp.topP = 1.0;

	logger.Info("=== BATTERIE DE CONTROLE — {0} ===", cfg.loadPath.CStr());
	logger.Info("(generation gloutonne : resultat reproductible)");

	int reussis = 0;
	NkString catCourante;
	int catReussis = 0, catTotal = 0;

	for (int i = 0; i < ilyana::kNbControles; ++i) {
		const ilyana::CasControle &c = ilyana::kControles[i];
		if (!(catCourante == NkString(c.categorie))) {
			if (catTotal > 0)
				logger.Info("  -> {0} : {1}/{2}", catCourante.CStr(), catReussis, catTotal);
			catCourante = NkString(c.categorie);
			catReussis = 0;
			catTotal = 0;
		}

		// Dialogue : chaque tour est ajoute a l'historique, avec la VRAIE reponse
		// du modele — c'est ce qui teste la fermete sous contradiction.
		NkString histo;
		NkString derniere;
		for (int q = 0; q < 4 && c.tours[q] != nullptr; ++q) {
			NkString amorce(histo);
			amorce.Append("Question: ");
			amorce.Append(c.tours[q]);
			amorce.Append("\nReponse:");
			derniere = CouperReponse(t.Generate(amorce, genLen, sp, t.GenLangIndex()));
			histo = amorce;
			histo.Append(derniere);
			histo.Append("\n");
		}

		const NkString plat = ilyana::Aplatir(derniere);
		bool ok = true;
		if (c.doitContenir != nullptr) {
			const bool a = plat.Find(ilyana::Aplatir(NkString(c.doitContenir)).CStr()) != NkString::npos;
			const bool b = c.doitContenir2 != nullptr &&
						   plat.Find(ilyana::Aplatir(NkString(c.doitContenir2)).CStr()) != NkString::npos;
			if (!a && !b)
				ok = false;
		}
		if (ok && c.interdit != nullptr &&
			plat.Find(ilyana::Aplatir(NkString(c.interdit)).CStr()) != NkString::npos)
			ok = false;
		if (ok && c.aucuneAnnee && ilyana::ContientAnnee(derniere))
			ok = false;

		++catTotal;
		if (ok) {
			++catReussis;
			++reussis;
		}
		logger.Info("  [{0}] {1} | {2}", ok ? "OK" : "KO", c.tours[0], derniere.CStr());
	}
	if (catTotal > 0)
		logger.Info("  -> {0} : {1}/{2}", catCourante.CStr(), catReussis, catTotal);

	const double pct = 100.0 * (double)reussis / (double)ilyana::kNbControles;
	logger.Info("=== SCORE : {0}/{1} ({2}%) ===", reussis, ilyana::kNbControles, pct);
	logger.Info("Un reentrainement qui FAIT BAISSER ce score ne doit pas etre promu.");
	return (reussis == ilyana::kNbControles) ? 0 : 1;
}

// =============================================================================
// MODE --wiki : préparer le corpus RÉEL (Wikipédia FR) — en FLUX.
// 2,1 Go : rien n'est chargé en mémoire, on lit et écrit paragraphe par
// paragraphe. Deux nettoyages, tous deux MESURÉS avant d'être décidés :
//   - CRLF (un « \r » par ligne = un octet appris pour rien, et tout le
//     découpage du dépôt cherche « \n\n ») ;
//   - paragraphes MUTILÉS par l'extracteur, où le contenu d'un modèle a disparu
//     (« né le à Moulins », « des premières décennies du . ») : ~2 % des lignes.
//     Les garder apprendrait à Ilyana à omettre les dates.
// Tout le reste est conservé tel quel : du texte humain, sourcé, relu.
// =============================================================================
static bool ParagrapheMutile(const NkString &p) {
	static const char *kMarques[] = {"le à ", "du .", " en .", " le .", "{{", "}}", "[[", "]]", "<ref"};
	for (int i = 0; i < (int)(sizeof(kMarques) / sizeof(kMarques[0])); ++i)
		if (p.Find(kMarques[i]) != NkString::npos)
			return true;
	return false;
}

static int ModeWiki(int argc, char **argv) {
	const char *source =
		Arg(argc, argv, "--source", "D:/Projets/Camrail/AI/Resources/Datasets/fr_wikimedia-wikipedia.txt");
	const char *sortie = Arg(argc, argv, "--sortie", "D:/Projets/Camrail/AI/IlyanaWiki/fr_wiki.txt");
	const int64 maxOctets = ArgEntier(argc, argv, "--max-octets", 0); // 0 = tout
	const int64 minPar = ArgEntier(argc, argv, "--min-paragraphe", 120);

	logger.Info("=== Ilyana / corpus REEL (Wikipedia FR) ===");
	FILE *fi = fopen(source, "rb");
	if (!fi) {
		logger.Info("ERREUR : source introuvable ({0}).", source);
		return 1;
	}
	FILE *fo = fopen(sortie, "wb");
	if (!fo) {
		fclose(fi);
		logger.Info("ERREUR : ecriture impossible ({0}) — le dossier existe-t-il ?", sortie);
		return 1;
	}

	NkChrono chrono;
	static char ligne[1 << 16];
	NkString para;
	int64 luOctets = 0, ecritOctets = 0;
	int64 parsLus = 0, parsGardes = 0, parsMutiles = 0, parsCourts = 0;

	auto viderParagraphe = [&]() {
		if (para.Size() == 0)
			return;
		++parsLus;
		if (ParagrapheMutile(para))
			++parsMutiles;
		else if ((int64)para.Size() < minPar)
			++parsCourts; // titres de section, restes d'une ligne
		else {
			fwrite(para.Data(), 1, para.Size(), fo);
			fwrite("\n\n", 1, 2, fo);
			ecritOctets += (int64)para.Size() + 2;
			++parsGardes;
		}
		para = NkString();
	};

	while (fgets(ligne, (int)sizeof(ligne), fi) != nullptr) {
		nk_size n = 0;
		while (ligne[n] != '\0')
			++n;
		luOctets += (int64)n;
		while (n > 0 && (ligne[n - 1] == '\n' || ligne[n - 1] == '\r'))
			--n; // fin de ligne, CR compris (fichier CRLF : mesuré)
		if (n == 0) {
			viderParagraphe(); // ligne vide = fin de paragraphe
			continue;
		}
		if (para.Size() > 0)
			para.Append(' ');
		para.Append(ligne, n);
		if (maxOctets > 0 && ecritOctets >= maxOctets)
			break;
	}
	viderParagraphe();
	fclose(fi);
	fclose(fo);

	const double secs = chrono.Elapsed().seconds;
	logger.Info("lu          : {0} octets en {1} s", (long long)luOctets, secs);
	logger.Info("paragraphes : {0} lus -> {1} gardes ({2}%)", (long long)parsLus, (long long)parsGardes,
				parsLus ? (100.0 * (double)parsGardes / (double)parsLus) : 0.0);
	logger.Info("ecartes     : {0} mutiles ({1}%), {2} trop courts (titres de section)", (long long)parsMutiles,
				parsLus ? (100.0 * (double)parsMutiles / (double)parsLus) : 0.0, (long long)parsCourts);
	logger.Info("ecrit       : {0} octets dans {1}", (long long)ecritOctets, sortie);

	// ATTRIBUTION — exigence de la licence, pas une politesse.
	{
		NkString att;
		att.Append("Corpus derive de Wikipedia en francais (Wikimedia Foundation).\n");
		att.Append("Licence : Creative Commons Attribution - partage dans les memes conditions 4.0\n");
		att.Append("          (CC BY-SA 4.0) - https://creativecommons.org/licenses/by-sa/4.0/deed.fr\n\n");
		att.Append("Modifications apportees : conversion des fins de ligne CRLF en LF, et retrait des\n");
		att.Append("paragraphes ou l'extraction a vide le contenu d'un modele (dates absentes,\n");
		att.Append("balisage residuel). Aucun ajout, aucune reformulation.\n\n");
		att.Append("Tout modele entraine sur ce corpus herite de cette obligation d'attribution.\n");
		att.Append("Voir docs/SOURCES_TIERCES.md et THIRD_PARTY_LICENSES.md a la racine du depot.\n");
		NkString cheminAtt(sortie);
		cheminAtt.Append(".ATTRIBUTION.txt");
		if (!EcrireFichier(cheminAtt.CStr(), att))
			logger.Info("ATTENTION : attribution non ecrite ({0}).", cheminAtt.CStr());
		else
			logger.Info("attribution : {0}", cheminAtt.CStr());
	}
	return 0;
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

// =============================================================================
// MODE --melange : préparer le corpus de la SECONDE phase (identité + charte)
// -----------------------------------------------------------------------------
// POURQUOI CE MODE EXISTE. Mesuré sur le run réel : l'identité pèse 0,5 Mo dans
// un corpus de 1657 Mo, soit 0,03 %. Sur 18 millions de tokens vus, elle ne
// croise donc qui elle est que sur ~5 500 tokens éparpillés. La phase de langue
// ne PEUT PAS lui apprendre son identité — ce n'est pas un défaut d'entraînement,
// c'est une question de proportions. D'où une seconde phase, courte, dédiée.
//
// LE PIÈGE QUE CE MODE ÉVITE. Réentraîner sur la seule identité lui ferait
// désapprendre le français : c'est l'oubli catastrophique, et il est brutal — le
// modèle se met à réciter ses quelques milliers de phrases d'identité et ne sait
// plus rien construire d'autre. On mélange donc de la PROSE avec, en proportion
// choisie. La prose n'est pas là pour enseigner : elle est là pour RETENIR.
//
// DEUX CHOIX QUI NE SONT PAS DES DÉTAILS :
//  - la prose est prélevée en SONDANT le gros corpus à intervalles réguliers, pas
//    en lisant son début. Un dump Wikipédia n'est pas dans un ordre neutre ; ses
//    premiers méga-octets se ressemblent. Réviser dessus appauvrirait sa langue ;
//  - identité et prose sont ENTRELACÉES, pas concaténées. Concaténées, le modèle
//    traverse un long moment sans voir l'une des deux, et l'oubli reprend pendant
//    ce moment-là.
// =============================================================================
static void DecouperEnBlocs(const NkString &s, NkVector<NkString> &out) {
	nk_size i = 0;
	while (i < s.Size()) {
		nk_size fin = s.Find("\n\n", i);
		if (fin == NkString::npos)
			fin = s.Size();
		if (fin > i)
			out.PushBack(s.SubStr(i, fin - i));
		i = (fin >= s.Size()) ? s.Size() : fin + 2;
	}
}

static int ModeMelange(int argc, char **argv) {
	const char *fIdent = Arg(argc, argv, "--identite", nullptr);
	const char *dossier = Arg(argc, argv, "--bibliotheque", nullptr);
	const char *fSortie = Arg(argc, argv, "--sortie", "phase2.txt");
	const double part = ArgReel(argc, argv, "--part", 0.25);
	const int64 tailleMo = ArgEntier(argc, argv, "--taille", 8);

	// Sur une bibliothèque, la prose vient de tout.txt et le catalogue permet
	// d'équilibrer les domaines. Sur un fichier isolé, on prélève au fil du texte.
	NkString cheminBiblio;
	NkVector<ilyana::Ouvrage> cat;
	const char *fCorpus = Arg(argc, argv, "--corpus", nullptr);
	if (dossier) {
		cheminBiblio = Joindre(dossier, "tout.txt");
		fCorpus = cheminBiblio.CStr();
		ilyana::LireCatalogue(Joindre(dossier, "catalogue.txt").CStr(), cat);
	}

	if (!fIdent || !fCorpus) {
		logger.Info("usage : --melange --identite <identite.txt> --corpus <gros.txt> "
					"[--sortie f] [--part 0.25] [--taille 8]");
		return 1;
	}
	if (part <= 0.0 || part >= 1.0) {
		logger.Info("ERREUR : --part doit etre strictement entre 0 et 1 (0.25 = un quart d'identite).");
		return 1;
	}

	const NkString ident = NormaliserFinsDeLigne(LireFichier(fIdent));
	if (ident.Size() == 0) {
		logger.Infof("ERREUR : identite illisible ou vide : %s\n", fIdent);
		return 1;
	}
	NkVector<NkString> blocsIdent;
	DecouperEnBlocs(ident, blocsIdent);
	if (blocsIdent.Size() == 0) {
		logger.Info("ERREUR : aucun bloc dans l'identite (separateur attendu : ligne vide).");
		return 1;
	}

	FILE *fc = fopen(fCorpus, "rb");
	if (!fc) {
		logger.Infof("ERREUR : corpus illisible : %s\n", fCorpus);
		return 1;
	}
	fseek(fc, 0, SEEK_END);
	const int64 tailleCorpus = (int64)ftell(fc);

	const int64 cible = tailleMo * 1024 * 1024;
	const int64 cibleIdent = (int64)((double)cible * part);
	const int64 cibleProse = cible - cibleIdent;

	// Combien de sondages, et de quelle taille. On en veut beaucoup et de taille
	// modeste : cent prélèvements de 16 Ko couvrent bien plus de sujets qu'un
	// seul de 1,6 Mo, pour le même volume.
	// Marge de 60 % : chaque sondage perd ses deux extrémités au recadrage sur
	// les frontières de blocs. Sans marge, on prélève « juste ce qu'il faut » et
	// on finit par rejouer de la prose — ce qui la sur-représente au hasard du
	// découpage, exactement ce qu'on cherchait à éviter en sondant large.
	const int64 tailleSonde = 16 * 1024;
	int64 nbSondes = ((cibleProse * 8 / 5) + tailleSonde - 1) / tailleSonde;
	if (nbSondes < 1)
		nbSondes = 1;

	NkVector<NkString> blocsProse;
	char *tampon = (char *)malloc((nk_size)tailleSonde + 1);
	if (!tampon) {
		fclose(fc);
		logger.Info("ERREUR : allocation du tampon de sondage impossible.");
		return 1;
	}

	// Sonde une plage d'octets. Isolé en lambda parce qu'on l'appelle soit une
	// fois sur tout le fichier, soit une fois par domaine quand on équilibre.
	auto sonder = [&](int64 depart, int64 arret, int64 combien) {
		const int64 etendue = arret - depart;
		if (etendue <= 0 || combien <= 0)
			return;
		for (int64 s = 0; s < combien; ++s) {
			int64 pos = depart;
			if (etendue > tailleSonde)
				pos += (int64)(((double)s / (double)combien) * (double)(etendue - tailleSonde));
			if (fseek(fc, (long)pos, SEEK_SET) != 0)
				continue;
			const nk_size got = fread(tampon, 1, (nk_size)tailleSonde, fc);
			if (got == 0)
				continue;
			NkString brut(tampon, got);
			// On a atterri au hasard AU MILIEU d'une ligne, et peut-être au
			// milieu d'un caractère accentué (UTF-8 tient sur plusieurs octets).
			// On jette donc jusqu'au premier séparateur de blocs, et on coupe au
			// dernier : ce qui reste est fait de blocs entiers, donc valide.
			const nk_size deb = brut.Find("\n\n");
			if (deb == NkString::npos)
				continue;
			const nk_size fin = brut.RFind("\n\n");
			if (fin == NkString::npos || fin <= deb + 2)
				continue;
			DecouperEnBlocs(brut.SubStr(deb + 2, fin - deb - 2), blocsProse);
		}
	};

	if (cat.Size() > 0) {
		// ÉQUILIBRAGE ENTRE DOMAINES. Sans lui, le prélèvement est proportionnel
		// à la TAILLE : un traité de mathématiques de 400 pages pèserait plus que
		// cinq livres d'histoire réunis, et Ilyana apprendrait surtout à parler
		// mathématiques. On donne donc à chaque domaine la même part, quel que
		// soit le nombre de pages qu'il occupe sur le disque.
		NkVector<NkString> domaines;
		for (nk_size i = 0; i < cat.Size(); ++i) {
			bool vu = false;
			for (nk_size d = 0; d < domaines.Size(); ++d)
				if (domaines[d] == cat[i].domaine) {
					vu = true;
					break;
				}
			if (!vu)
				domaines.PushBack(cat[i].domaine);
		}
		const int64 parDomaine = nbSondes / (int64)(domaines.Size() ? domaines.Size() : 1);
		for (nk_size d = 0; d < domaines.Size(); ++d) {
			// Les ouvrages d'un même domaine se partagent la part du domaine.
			int64 nbOuvrages = 0;
			for (nk_size i = 0; i < cat.Size(); ++i)
				if (cat[i].domaine == domaines[d])
					++nbOuvrages;
			const int64 parOuvrage = parDomaine / (nbOuvrages > 0 ? nbOuvrages : 1);
			for (nk_size i = 0; i < cat.Size(); ++i)
				if (cat[i].domaine == domaines[d])
					sonder((int64)cat[i].debut, (int64)cat[i].fin, parOuvrage);
		}
		logger.Infof("equilibrage : %llu domaine(s), %lld sondages chacun\n",
					 (unsigned long long)domaines.Size(), (long long)parDomaine);
	} else {
		sonder(0, tailleCorpus, nbSondes);
	}
	free(tampon);
	fclose(fc);

	if (blocsProse.Size() == 0) {
		logger.Info("ERREUR : aucun bloc de prose prelevé — le corpus utilise-t-il bien la ligne vide "
					"comme separateur ?");
		return 1;
	}

	// Entrelacement. On avance dans les deux listes en parallèle, en insérant un
	// bloc d'identité dès que sa part réelle passe sous la part visée. Les deux
	// listes bouclent : l'identité se répète (c'est voulu, elle doit s'imprimer),
	// la prose aussi si le corpus prélevé ne suffit pas.
	NkString out;
	out.Reserve((nk_size)cible + (1 << 16));
	int64 volIdent = 0;
	int64 volProse = 0;
	nk_size iI = 0;
	nk_size iP = 0;
	while ((volIdent + volProse) < cible) {
		const int64 total = volIdent + volProse;
		const bool prendreIdent = (total == 0) ? true : ((double)volIdent / (double)total) < part;
		if (prendreIdent && volIdent < cibleIdent) {
			const NkString &b = blocsIdent[iI % blocsIdent.Size()];
			++iI;
			out.Append(b);
			out.Append("\n\n", 2);
			volIdent += (int64)b.Size() + 2;
		} else {
			const NkString &b = blocsProse[iP % blocsProse.Size()];
			++iP;
			out.Append(b);
			out.Append("\n\n", 2);
			volProse += (int64)b.Size() + 2;
		}
	}

	if (!EcrireFichier(fSortie, out)) {
		logger.Infof("ERREUR : ecriture impossible : %s\n", fSortie);
		return 1;
	}

	const double partReelle = (double)volIdent / (double)(volIdent + volProse);
	logger.Info("=== Ilyana / melange phase 2 ===");
	logger.Infof("identite : %llu blocs distincts, repetes %llu fois\n",
				 (unsigned long long)blocsIdent.Size(),
				 (unsigned long long)(iI / (blocsIdent.Size() ? blocsIdent.Size() : 1)));
	const double tailleGo = (double)tailleCorpus / (1024.0 * 1024.0 * 1024.0);
	if (tailleGo >= 1.0)
		logger.Infof("prose    : %llu blocs preleves par %lld sondages repartis sur %.1f Go\n",
					 (unsigned long long)blocsProse.Size(), (long long)nbSondes, tailleGo);
	else
		logger.Infof("prose    : %llu blocs preleves par %lld sondages repartis sur %.1f Mo\n",
					 (unsigned long long)blocsProse.Size(), (long long)nbSondes,
					 (double)tailleCorpus / (1024.0 * 1024.0));
	logger.Infof("sortie   : %s — %.1f Mo, dont %.1f%% d'identite (vise %.1f%%)\n", fSortie,
				 (double)out.Size() / (1024.0 * 1024.0), partReelle * 100.0, part * 100.0);
	if (iP < blocsProse.Size())
		logger.Infof("note : %.0f%% de la prose prelevee a suffi — aucune repetition de prose.\n",
					 100.0 * (double)iP / (double)blocsProse.Size());
	else
		logger.Info("note : la prose prelevee a ete rejouee au moins une fois — elle est donc "
					"sur-representee ; c'est le signe que la marge de sondage est trop courte.");
	logger.Info("");
	logger.Info("RAPPEL : cette phase se lance depuis le checkpoint de la phase de langue,");
	logger.Info("avec un pas d'apprentissage FAIBLE et peu de pas. Et on ne promeut le");
	logger.Info("resultat que si la batterie (--controle) ne BAISSE pas.");
	return 0;
}

// =============================================================================
// MODE --chercher : retrouver un passage, pour qu'elle LISE au lieu de deviner.
// -----------------------------------------------------------------------------
// Volontairement séparé de la génération. Une recherche se juge toute seule —
// « le passage rendu contient-il la réponse ? » se vérifie à l'œil, sans modèle.
// Les brancher d'emblée l'un dans l'autre rendrait indiscernables deux échecs
// très différents : n'avoir pas trouvé le passage, ou l'avoir mal utilisé.
// =============================================================================
// Affiche un résultat AVEC SA SOURCE quand elle est connue. C'est tout l'objet
// de la manœuvre : une réponse sans source ne se vérifie pas, donc ne vaut pas
// mieux qu'une invention — elle est seulement plus difficile à démentir.
static void AfficherResultat(const ilyana::NkIndex &index, const NkVector<ilyana::Ouvrage> &cat,
							 const ilyana::Resultat &r, nk_size rang) {
	const ilyana::Ouvrage *o = ilyana::OuvrageDeLOffset(cat, index.OffsetPassage(r.passage));
	if (o)
		printf("\n--- %llu — %s [%s] (score %.2f) ---\n", (unsigned long long)rang, o->titre.CStr(),
			   o->domaine.CStr(), r.score);
	else
		printf("\n--- %llu (score %.2f, passage #%u) ---\n", (unsigned long long)rang, r.score, r.passage);
	NkString t = r.texte;
	if (t.Size() > 600)
		t = t.SubStr(0, 600);
	printf("%s\n", t.CStr());
}

static int ModeChercher(int argc, char **argv) {
	const char *fCorpus = Arg(argc, argv, "--corpus", nullptr);
	const char *dossier = Arg(argc, argv, "--bibliotheque", nullptr);
	const char *question = Arg(argc, argv, "--question", nullptr);
	const int64 k = ArgEntier(argc, argv, "--k", 3);
	const int64 maxOctets = ArgEntier(argc, argv, "--max-octets", 64ll * 1024 * 1024);

	if (!fCorpus && !dossier) {
		logger.Info("usage : --chercher --bibliotheque <dossier> [--question \"...\"] [--k 3]");
		logger.Info("    ou : --chercher --corpus <fichier> [--question \"...\"] [--max-octets N]");
		return 1;
	}

	ilyana::NkIndex index;
	NkVector<ilyana::Ouvrage> cat;
	NkChrono chrono;
	double secs = 0.0;

	// Deux usages : sur une bibliothèque (index déjà construit, donc rechargé —
	// et les résultats CITENT leur ouvrage), ou sur un fichier isolé (indexé à la
	// volée). Recharger plutôt que reconstruire est ce qui rend la consultation
	// d'une bibliothèque instantanée.
	if (dossier) {
		const NkString fIdx = Joindre(dossier, "index.nkidx");
		if (!index.Charger(fIdx.CStr())) {
			logger.Infof("ERREUR : index introuvable ou illisible : %s — lancer --indexer d'abord.\n",
						 fIdx.CStr());
			return 1;
		}
		secs = chrono.Elapsed().seconds;
		ilyana::LireCatalogue(Joindre(dossier, "catalogue.txt").CStr(), cat);
	} else if (fCorpus) {
		if (!index.Construire(fCorpus, maxOctets)) {
			logger.Infof("ERREUR : indexation impossible : %s\n", fCorpus);
			return 1;
		}
		secs = chrono.Elapsed().seconds;
	}

	logger.Info("=== Ilyana / recherche documentaire ===");
	logger.Infof("index : %llu passages, %llu mots distincts, %lld mots au total — en %.2f s\n",
				 (unsigned long long)index.NbPassages(), (unsigned long long)index.NbMotsDistincts(),
				 (long long)index.TotalMots(), secs);

	NkVector<ilyana::Resultat> res;
	if (question) {
		index.Chercher(NkString(question), (int32)k, res);
		printf("\nQuestion : %s\n", question);
		if (res.Size() == 0)
			printf("\nAucun passage ne contient ces mots.\n");
		for (nk_size i = 0; i < res.Size(); ++i)
			AfficherResultat(index, cat, res[i], i + 1);
		return 0;
	}

	// Sans question : boucle interactive.
	printf("\nPose une question (ligne vide pour sortir).\n");
	char ligne[1024];
	for (;;) {
		printf("\n> ");
		fflush(stdout);
		if (!fgets(ligne, sizeof(ligne), stdin))
			break;
		nk_size n = strlen(ligne);
		while (n > 0 && (ligne[n - 1] == '\n' || ligne[n - 1] == '\r'))
			ligne[--n] = 0;
		if (n == 0)
			break;
		index.Chercher(NkString(ligne), (int32)k, res);
		if (res.Size() == 0) {
			printf("Aucun passage ne contient ces mots.\n");
			continue;
		}
		for (nk_size i = 0; i < res.Size(); ++i)
			AfficherResultat(index, cat, res[i], i + 1);
	}
	return 0;
}

// =============================================================================
// MODE --ajouter : déposer un livre dans la bibliothèque.
// =============================================================================
static NkString NomDeFichier(const char *chemin) {
	const char *deb = chemin;
	for (const char *p = chemin; *p; ++p)
		if (*p == '/' || *p == '\\')
			deb = p + 1;
	NkString s(deb);
	const nk_size point = s.RFind(".");
	return (point == NkString::npos || point == 0) ? s : s.SubStr(0, point);
}

static int ModeAjouter(int argc, char **argv) {
	const char *fLivre = Arg(argc, argv, "--livre", nullptr);
	const char *domaine = Arg(argc, argv, "--domaine", nullptr);
	const char *titre = Arg(argc, argv, "--titre", nullptr);
	const char *dossier = Arg(argc, argv, "--bibliotheque", "bibliotheque");

	if (!fLivre || !domaine) {
		logger.Info("usage : --ajouter --livre <fichier.txt> --domaine <maths|physique|histoire|...>");
		logger.Info("        [--titre \"...\"] [--bibliotheque <dossier>]");
		return 1;
	}

	// L'extension décide du lecteur. L'EPUB est une archive : il faut l'ouvrir
	// avant de pouvoir lire quoi que ce soit.
	NkString nomBas(fLivre);
	for (nk_size i = 0; i < nomBas.Size(); ++i) {
		char *d = (char *)nomBas.Data();
		if (d[i] >= 'A' && d[i] <= 'Z')
			d[i] = (char)(d[i] - 'A' + 'a');
	}
	const bool estEpub = nomBas.Size() > 5 && nomBas.SubStr(nomBas.Size() - 5) == ".epub";

	const bool estPdf = nomBas.Size() > 4 && nomBas.SubStr(nomBas.Size() - 4) == ".pdf";

	NkString brut;
	if (estPdf) {
		int64 pages = 0;
		int64 muettes = 0;
		ilyana::DiagPdf diag;
		brut = ilyana::LirePdf(fLivre, pages, muettes, 72.0, &diag);
		logger.Infof("PDF : %lld page(s) parcourues, %lld sans aucun texte.\n", (long long)pages,
					 (long long)muettes);
		logger.Infof("      %lld caractere(s) rencontres, dont %lld sans equivalent lisible.\n",
					 (long long)diag.glyphes, (long long)diag.sansUnicode);
		logger.Infof("      contenu %lld o, %lld operations, %lld ordres de texte, glyphes %lld/%lld\n",
					 (long long)diag.octetsContenu, (long long)diag.operations, (long long)diag.opsTexte,
					 (long long)diag.glyphesObtenus, (long long)diag.glyphesDemandes);
		// Le diagnostic distingue TROIS echecs que rien ne separe a l'oeil, et
		// qu'il serait faux de confondre : accuser les polices quand le flux n'a
		// pas ete lu enverrait chercher a cote pendant des heures.
		if (pages > 0 && muettes * 2 > pages) {
			if (diag.operations == 0)
				logger.Info("ATTENTION : le contenu des pages n'a pas ete execute. Le document est peut-etre "
							"chiffre, ou d'une variante non geree.");
			else if (diag.opsTexte == 0)
				logger.Info("ATTENTION : aucun ordre de texte dans ce document — ses pages sont sans doute des "
							"IMAGES (livre scanne). Il faudrait une reconnaissance de caracteres, qui n'existe "
							"pas ici.");
			else if (diag.glyphesDemandes == 0)
				logger.Info("ATTENTION : le texte est bien present mais AUCUNE POLICE ne se resout — le lecteur "
							"ne sait pas encore lire les polices de ce document (les PDF produits par LaTeX "
							"utilisent Type1/CFF, la ou le lecteur attend du TrueType). Si tu as la SOURCE .tex, "
							"elle vaut bien mieux : les formules y sont du texte.");
			else
				logger.Info("ATTENTION : les polices ne declarent pas ce que representent leurs glyphes (table "
							"/ToUnicode absente) — ce qu'ils dessinent est indevinable, et on prefere ne rien "
							"ecrire qu'inventer.");
		}
		if (brut.Size() == 0) {
			logger.Infof("ERREUR : aucun texte extrait de %s\n", fLivre);
			return 1;
		}
	} else if (nomBas.Size() > 4 && nomBas.SubStr(nomBas.Size() - 4) == ".tex") {
		// La source LaTeX vaut BIEN MIEUX que le PDF qu'elle produit : les
		// formules y sont du texte structure, la ou le PDF n'en garde que des
		// glyphes epars. Les \input sont suivis, sans quoi un livre decoupe en un
		// fichier par chapitre ne rendrait que sa page de titre.
		brut = media::LireLatex(fLivre);
		if (brut.Size() == 0) {
			logger.Infof("ERREUR : aucun texte extrait de %s\n", fLivre);
			return 1;
		}
		logger.Infof("LaTeX : %.2f Mo de texte (formules conservees telles quelles).\n",
					 (double)brut.Size() / (1024.0 * 1024.0));
	} else if (estEpub) {
		int64 pages = 0;
		brut = media::LireEpub(fLivre, pages);
		if (brut.Size() == 0) {
			logger.Infof("ERREUR : aucun texte extrait de %s\n", fLivre);
			logger.Info("Un EPUB qui ne rend rien est presque toujours PROTEGE (DRM) : le texte y est "
						"chiffre, et aucun lecteur ne peut l'ouvrir sans la cle.");
			return 1;
		}
		logger.Infof("EPUB : %lld chapitre(s) lus.\n", (long long)pages);
	} else {
		brut = LireFichier(fLivre);
	}
	if (brut.Size() == 0) {
		logger.Infof("ERREUR : livre illisible ou vide : %s\n", fLivre);
		return 1;
	}
	const NkString propre = ilyana::NettoyerOuvrage(brut);
	if (propre.Size() == 0) {
		logger.Info("ERREUR : le livre est vide apres nettoyage.");
		return 1;
	}

	const NkString fTout = Joindre(dossier, "tout.txt");
	const NkString fCat = Joindre(dossier, "catalogue.txt");

	// Le dossier doit exister. On ne le crée pas en douce : si le chemin est
	// faux, mieux vaut le dire que semer des fichiers ailleurs.
	FILE *f = fopen(fTout.CStr(), "ab");
	if (!f) {
		logger.Infof("ERREUR : impossible d'ecrire dans %s — le dossier '%s' existe-t-il ?\n", fTout.CStr(), dossier);
		return 1;
	}
	fseek(f, 0, SEEK_END);
	const uint64 debut = (uint64)ftell(f);
	const bool ok = fwrite(propre.Data(), 1, propre.Size(), f) == propre.Size() && fflush(f) == 0;
	const uint64 fin = (uint64)ftell(f);
	fclose(f);
	if (!ok) {
		logger.Info("ERREUR : ecriture incomplete dans tout.txt.");
		return 1;
	}

	ilyana::Ouvrage o;
	o.domaine = NkString(domaine);
	o.titre = titre ? NkString(titre) : NomDeFichier(fLivre);
	o.source = NkString(fLivre);
	o.debut = debut;
	o.fin = fin;
	if (!ilyana::AjouterAuCatalogue(fCat.CStr(), o)) {
		logger.Info("ERREUR : catalogue non mis a jour — l'ouvrage est dans tout.txt mais INTROUVABLE. "
					"Retirer les octets ajoutes ou corriger le catalogue a la main.");
		return 1;
	}

	// Comptage des passages, pour dire tout de suite si le decoupage a pris.
	int64 blocs = 0;
	for (nk_size i = 0; i + 1 < propre.Size(); ++i)
		if (propre.Data()[i] == '\n' && propre.Data()[i + 1] == '\n')
			++blocs;

	logger.Info("=== Ilyana / bibliotheque ===");
	logger.Infof("ajoute : %s  [%s]\n", o.titre.CStr(), o.domaine.CStr());
	logger.Infof("  %.2f Mo a l'origine -> %.2f Mo apres nettoyage, %lld passages\n",
				 (double)brut.Size() / (1024.0 * 1024.0), (double)propre.Size() / (1024.0 * 1024.0),
				 (long long)blocs);
	logger.Infof("  octets %llu a %llu dans %s\n", (unsigned long long)debut, (unsigned long long)fin,
				 fTout.CStr());
	if (blocs < 5)
		logger.Info("  ATTENTION : tres peu de passages — le texte est-il bien du texte, "
					"ou une conversion ratee (PDF de formules, colonnes melangees) ?");
	logger.Info("");
	logger.Info("Penser a REINDEXER apres avoir ajoute vos livres :  --indexer");
	return 0;
}

// =============================================================================
// MODE --indexer : (re)construire l'index de la bibliotheque, et l'enregistrer.
// =============================================================================
static int ModeIndexer(int argc, char **argv) {
	const char *dossier = Arg(argc, argv, "--bibliotheque", "bibliotheque");
	const int64 maxOctets = ArgEntier(argc, argv, "--max-octets", 0);

	const NkString fTout = Joindre(dossier, "tout.txt");
	const NkString fCat = Joindre(dossier, "catalogue.txt");
	const NkString fIdx = Joindre(dossier, "index.nkidx");

	ilyana::NkIndex index;
	NkChrono chrono;
	if (!index.Construire(fTout.CStr(), maxOctets)) {
		logger.Infof("ERREUR : rien a indexer dans %s\n", fTout.CStr());
		return 1;
	}
	const double secs = chrono.Elapsed().seconds;
	if (!index.Sauver(fIdx.CStr())) {
		logger.Infof("ERREUR : index non enregistre : %s\n", fIdx.CStr());
		return 1;
	}

	NkVector<ilyana::Ouvrage> cat;
	ilyana::LireCatalogue(fCat.CStr(), cat);

	logger.Info("=== Ilyana / index de la bibliotheque ===");
	logger.Infof("%llu passages, %llu mots distincts, %lld mots au total — en %.2f s\n",
				 (unsigned long long)index.NbPassages(), (unsigned long long)index.NbMotsDistincts(),
				 (long long)index.TotalMots(), secs);
	logger.Infof("enregistre : %s\n", fIdx.CStr());
	logger.Infof("%llu ouvrage(s) au catalogue :\n", (unsigned long long)cat.Size());
	for (nk_size i = 0; i < cat.Size(); ++i)
		logger.Infof("  [%s] %s — %.2f Mo\n", cat[i].domaine.CStr(), cat[i].titre.CStr(),
					 (double)(cat[i].fin - cat[i].debut) / (1024.0 * 1024.0));
	return 0;
}

int main(int argc, char **argv) {
	if (Drapeau(argc, argv, "--controle"))
		return ModeControle(argc, argv);
	if (Drapeau(argc, argv, "--melange"))
		return ModeMelange(argc, argv);
	if (Drapeau(argc, argv, "--ajouter"))
		return ModeAjouter(argc, argv);
	if (Drapeau(argc, argv, "--indexer"))
		return ModeIndexer(argc, argv);
	if (Drapeau(argc, argv, "--chercher"))
		return ModeChercher(argc, argv);
	if (Drapeau(argc, argv, "--wiki"))
		return ModeWiki(argc, argv);
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
	logger.Info("  --melange --identite f --corpus f [--sortie f] [--part 0.25] [--taille 8]");
	logger.Info("            corpus de la 2e phase : identite entrelacee avec de la prose prelevee");
	return 0;
}
