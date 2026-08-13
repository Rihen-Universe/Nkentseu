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
#include "NKMedia/Document/NkPageWeb.h"
#include "NKMedia/Document/NkAspirateur.h"
#include "NKMedia/Document/NkArchive.h"
#include "NKNetwork/HTTP/NkHTTPClient.h"
#include "NKImage/Core/NkImage.h" // inflate DEFLATE : degzip des reponses HTTP
#include "NkIlyanaPdf.h"
#include "NkIlyanaSondePdf.h"
#include "NkIlyanaCitation.h"
#include "NKFileSystem/NkDirectory.h"

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
	// --horizon : pas global VISÉ (absolu). Préférable à --steps pour tout run
	// susceptible d'être interrompu : le trainer soustrait lui-même ce qui est
	// déjà fait, en lisant le checkpoint — la seule source qui survive à une
	// coupure de courant. Relancer dix fois avec le même horizon donne alors
	// exactement le même entraînement.
	cfg.horizon = ArgEntier(argc, argv, "--horizon", 0);
	// --echantillons N : afficher du texte engendre tous les N pas (0 = jamais).
	// A METTRE A 0 SUR UN RUN LONG. Ces echantillons ne servent qu'a regarder
	// l'evolution, mais ils allouent sur le GPU par-dessus l'entrainement : le
	// 2026-08-12, ce pic a tue un run au pas 500 sur 6000
	// (VK_ERROR_OUT_OF_DEVICE_MEMORY) parce que le bureau occupait deja la
	// carte. Un affichage de confort ne doit pas couter quinze heures.
	cfg.sampleEvery = (int)ArgEntier(argc, argv, "--echantillons", 100);
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
		// --nouvelle-phase : repartir des poids avec un pas d'apprentissage NEUF.
		// Sans lui, une phase destinee a enseigner un comportement herite du pas
		// deja descendu au plancher par la phase precedente — mesure : 900 pas a
		// 1e-05 n'ont rien appris et ont fait BAISSER la batterie de 8/19 a 6/19.
		cfg.freshSchedule = Drapeau(argc, argv, "--nouvelle-phase");
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

	// ── Répondre EN LISANT, quand une bibliothèque est fournie ──
	//
	// La recherche retrouve le passage, on le place devant la question, et elle
	// répond à partir de lui. C'est la forme « Contexte → Question → Réponse »
	// qu'on lui a enseignée par recopie (voir NkIlyanaCitation.h) : sans cet
	// apprentissage, elle ignorerait purement et simplement le passage.
	//
	// ⚠️ La SOURCE est affichée à côté de la réponse, toujours. Une réponse
	// fondée sur un passage qu'on ne montre pas ne vaut pas mieux qu'une réponse
	// inventée : elle est seulement plus difficile à démentir.
	const char *dossierBiblio = Arg(argc, argv, "--bibliotheque", nullptr);
	if (q && dossierBiblio) {
		ilyana::NkIndex index;
		if (!index.Charger(Joindre(dossierBiblio, "index.nkidx").CStr())) {
			logger.Info("ERREUR : index introuvable — lancer --indexer d'abord.");
			return 1;
		}
		NkVector<ilyana::Ouvrage> cat;
		ilyana::LireCatalogue(Joindre(dossierBiblio, "catalogue.txt").CStr(), cat);

		NkVector<ilyana::Resultat> res;
		index.Chercher(NkString(q), 1, res);
		if (res.Size() == 0) {
			printf("\nQuestion : %s\n", q);
			printf("Reponse  : %s\n", ilyana::kReponseAbsente());
			printf("(aucun passage de la bibliotheque ne contient ces mots)\n");
			return 0;
		}

		// Le contexte doit tenir dans la fenêtre AVEC la question et la réponse.
		// Un contexte trop long serait tronqué par la tokenisation, et elle
		// répondrait à partir d'un texte amputé sans que rien ne le signale.
		NkString ctx = res[0].texte;
		if (ctx.Size() > 600)
			ctx = ctx.SubStr(0, 600);

		NkString amorce("Contexte: ");
		amorce.Append(ctx);
		amorce.Append("\nQuestion: ");
		amorce.Append(q);
		amorce.Append("\nReponse:");

		const ilyana::Ouvrage *o = ilyana::OuvrageDeLOffset(cat, index.OffsetPassage(res[0].passage));
		printf("\nQuestion : %s\n", q);
		printf("Source   : %s\n", o ? o->titre.CStr() : "(bibliotheque)");
		printf("Reponse  : %s\n", CouperReponse(t.Generate(amorce, cfg.genLen, sp, t.GenLangIndex())).CStr());
		return 0;
	}

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

	// ⚠️ LE FICHIER EXISTE-T-IL ? A verifier AVANT tout le reste.
	//
	// Sans ce controle, un chemin devenu invalide (fichier deplace, renomme,
	// supprime) traverse toute la chaine et ressort en « 0 octet de contenu,
	// 0 operation » — le symptome exact d'un lecteur casse. Le 2026-08-11, cette
	// confusion a coute trois reconstructions completes et deux hypotheses
	// fausses (objets perimes, puis conflit avec mbed-TLS) pour un fichier qui
	// avait simplement ete deplace. Un outil doit dire « introuvable » quand
	// c'est introuvable, jamais laisser croire a une panne interne.
	{
		FILE *test = fopen(fLivre, "rb");
		if (!test) {
			logger.Infof("ERREUR : fichier INTROUVABLE ou illisible : %s\n", fLivre);
			logger.Info("Verifier le chemin — un fichier deplace ou renomme donne exactement la meme "
						"apparence qu'un lecteur en panne.");
			return 1;
		}
		fclose(test);
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
		logger.Infof("      tables /ToUnicode : %lld declarees, %lld effectivement lues\n",
					 (long long)diag.tuDeclaree, (long long)diag.tuLue);
		if (diag.sondePrise) {
			logger.Infof("      sonde sur la police '%s' (%s), relevee au premier ToUnicode vide :\n",
						 diag.policeSondee.Size() ? diag.policeSondee.CStr() : "?",
						 diag.tableDeuxOctets ? "composite, codes 2 octets" : "simple, codes 1 octet");
			logger.Infof("      codes DEMANDES a CETTE police : %u %u %u %u %u %u\n", diag.codesDemandes[0],
						 diag.codesDemandes[1], diag.codesDemandes[2], diag.codesDemandes[3],
						 diag.codesDemandes[4], diag.codesDemandes[5]);
			logger.Infof("      codes CONTENUS dans SA table (%lld entrees) : %u %u %u %u %u %u\n",
						 (long long)diag.entreesTable, diag.codesTable[0], diag.codesTable[1],
						 diag.codesTable[2], diag.codesTable[3], diag.codesTable[4], diag.codesTable[5]);
		}
		// ⚠️ REFUS D'UN TEXTE ILLISIBLE — le garde-fou le plus important de ce mode.
		// Une police sans table /ToUnicode ne declare pas ce que son glyphe
		// represente. Le lecteur laisse alors le caractere vide, et ce qui SURNAGE
		// — accents, ponctuation, symboles isoles — a toutes les apparences d'un
		// texte extrait sans en etre un. Mesure sur un cours produit par LaTeX :
		// 99 667 caracteres rencontres dont 76 993 sans equivalent, et les 23 %
		// restants donnaient « É ? è ?,é ». Sans ce refus, la bibliotheque se
		// remplirait d'ordures en affichant que tout va bien : une panne franche
		// vaut infiniment mieux qu'une corruption silencieuse.
		if (diag.glyphes > 0 && (double)diag.sansUnicode / (double)diag.glyphes > 0.25) {
			logger.Infof("REFUSE : %lld caracteres sur %lld (%.0f%%) sans AUCUN equivalent lisible.\n",
						 (long long)diag.sansUnicode, (long long)diag.glyphes,
						 100.0 * (double)diag.sansUnicode / (double)diag.glyphes);
			logger.Info("Les polices de ce document ne declarent pas ce que representent leurs glyphes. Ce qui "
						"surnagerait ne serait pas du texte mais du charabia — mieux vaut ne rien deposer que "
						"polluer la bibliotheque.");
			logger.Info("Par ordre de preference : la SOURCE .tex si tu l'as (les formules y sont du texte), "
						"une edition EPUB, ou une autre edition du PDF.");
			return 1;
		}

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
	} else if ((nomBas.Size() > 5 && nomBas.SubStr(nomBas.Size() - 5) == ".html") ||
			   (nomBas.Size() > 4 && nomBas.SubStr(nomBas.Size() - 4) == ".htm")) {
		// Une page web est faite en majorite de ce qui n'est PAS l'article. On
		// garde les blocs qui ont l'allure d'un paragraphe et on jette le reste,
		// en disant combien : un tri qui ne rend pas ses comptes ne se verifie pas.
		int64 gardes = 0;
		int64 jetes = 0;
		brut = media::PageWebVersTexte(LireFichier(fLivre), gardes, jetes);
		logger.Infof("Page web : %lld blocs gardes, %lld ecartes (menus, pieds de page, liens).\n",
					 (long long)gardes, (long long)jetes);
		if (gardes == 0) {
			logger.Info("ERREUR : aucun paragraphe reconnu. La page est-elle surtout faite de listes et de "
						"liens, ou son contenu est-il charge par du script (auquel cas le fichier enregistre "
						"ne contient pas le texte) ?");
			return 1;
		}
		if (jetes > gardes * 10)
			logger.Info("ATTENTION : plus de dix fois plus de blocs ecartes que gardes — verifier que "
						"l'article a bien ete pris.");
	} else if (nomBas.Size() > 4 && nomBas.SubStr(nomBas.Size() - 4) == ".zip") {
		// Une archive de sources : on prend le code et le texte, on laisse le reste.
		int64 fichiers = 0;
		int64 ignores = 0;
		media::DiagArchive da;
		brut = media::LireArchive(fLivre, fichiers, ignores, 2u << 20, &da);
		logger.Infof("Archive : %lld fichier(s) de source retenus, %lld ecartes.\n", (long long)fichiers,
					 (long long)ignores);
		if (fichiers == 0) {
			// Dire A QUELLE ETAPE ca a echoue : « rien retenu » a des causes
			// opposees, et les confondre envoie chercher au mauvais endroit.
			if (da.catalogueIllisible > 0)
				logger.Info("ERREUR : le catalogue de l'archive n'a pas pu etre lu — format ZIP64 (au-dela "
							"de 65535 entrees) ou archive endommagee.");
			else if (da.decompressionRatee > 0)
				logger.Infof("ERREUR : %lld entree(s) non decompressees — methode de compression non geree "
							 "(seuls « stocke » et « deflate » le sont).\n",
							 (long long)da.decompressionRatee);
			else if (da.mauvaiseExtension > 0)
				logger.Infof("ERREUR : aucun fichier de source reconnu (%lld ecartes sur leur extension, "
							 "%lld dossiers).\n",
							 (long long)da.mauvaiseExtension, (long long)da.dossiers);
			else
				logger.Info("ERREUR : archive vide ou illisible.");
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
	// L'ORIGINE compte plus pour une page web que pour un livre : la page changera
	// ou disparaitra, et une citation qui renvoie a une adresse morte doit au
	// moins dire d'ou elle venait. `--url` la consigne a la place du chemin local,
	// qui ne veut rien dire pour quelqu'un d'autre.
	const char *url = Arg(argc, argv, "--url", nullptr);
	o.source = url ? NkString(url) : NkString(fLivre);
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

// =============================================================================
// MODE --citations : fabriquer les exemples qui lui apprennent à CITER.
// -----------------------------------------------------------------------------
// Voir NkIlyanaCitation.h pour le raisonnement complet. En deux phrases : on ne
// lui enseigne pas des faits mais un GESTE — rendre la phrase d'un texte qui
// contient les mots demandés —, et cet exemple-là se fabrique par pure recopie,
// sans qu'aucune machine ait à affirmer quoi que ce soit. Une part des exemples
// porte sur des mots ABSENTS, dont la bonne réponse est de constater l'absence :
// sans eux, un modèle apprend qu'il faut toujours répondre, et invente.
// =============================================================================
static int ModeCitations(int argc, char **argv) {
	const char *dossier = Arg(argc, argv, "--bibliotheque", nullptr);
	const char *fCorpus = Arg(argc, argv, "--corpus", nullptr);
	const char *fSortie = Arg(argc, argv, "--sortie", "citations.txt");
	const int64 combien = ArgEntier(argc, argv, "--combien", 20000);
	const double partAbsente = ArgReel(argc, argv, "--part-absente", 0.25);

	NkString cheminBiblio;
	if (dossier) {
		cheminBiblio = Joindre(dossier, "tout.txt");
		fCorpus = cheminBiblio.CStr();
	}
	if (!fCorpus) {
		logger.Info("usage : --citations (--bibliotheque <dossier> | --corpus <fichier>)");
		logger.Info("        [--sortie f] [--combien 20000] [--part-absente 0.25]");
		return 1;
	}

	const NkString texte = NormaliserFinsDeLigne(LireFichier(fCorpus));
	if (texte.Size() < 1000) {
		logger.Infof("ERREUR : corpus illisible ou trop court : %s\n", fCorpus);
		return 1;
	}
	NkVector<NkString> blocs;
	DecouperEnBlocs(texte, blocs);
	if (blocs.Size() < 10) {
		logger.Info("ERREUR : trop peu de passages — le corpus utilise-t-il la ligne vide comme separateur ?");
		return 1;
	}

	NkString out;
	out.Reserve((nk_size)combien * 700);
	int64 faits = 0;
	int64 absents = 0;
	int64 refuses = 0;
	NkString exemple;
	NkVector<NkString> motsAilleurs;

	// On parcourt les passages en les espaçant : deux exemples tirés de passages
	// voisins se ressemblent, et un corpus d'exemples redondants enseigne moins
	// qu'un corpus varié de même taille.
	const nk_size pas = (blocs.Size() > (nk_size)combien) ? (blocs.Size() / (nk_size)combien) : 1;
	nk_size i = 0;
	nk_size tours = 0;
	while (faits < combien && tours < blocs.Size() * 3) {
		const nk_size idx = (i % blocs.Size());
		i += pas ? pas : 1;
		++tours;
		const NkString &bloc = blocs[idx];

		// Un exemple sur quatre porte sur des mots ABSENTS. Les mots viennent
		// d'un passage éloigné, et FabriquerExemple VÉRIFIE qu'ils sont bien
		// absents avant d'écrire la réponse : on n'enseigne pas une fausseté.
		const bool negatif = (partAbsente > 0.0) && ((faits % (int64)(1.0 / partAbsente)) == (int64)0) && faits > 0;
		bool ok = false;
		if (negatif) {
			const NkString &autre = blocs[(idx + blocs.Size() / 2) % blocs.Size()];
			ilyana::MotsDeContenu(autre, motsAilleurs);
			ok = ilyana::FabriquerExemple(bloc, (nk_size)faits, &motsAilleurs, exemple);
			if (ok)
				++absents;
		} else {
			ok = ilyana::FabriquerExemple(bloc, (nk_size)faits, nullptr, exemple);
		}
		if (!ok) {
			++refuses;
			continue;
		}
		out.Append(exemple);
		out.Append('\n');
		++faits;
	}

	if (!EcrireFichier(fSortie, out)) {
		logger.Infof("ERREUR : ecriture impossible : %s\n", fSortie);
		return 1;
	}

	logger.Info("=== Ilyana / exemples de citation ===");
	logger.Infof("%lld exemples ecrits dans %s (%.1f Mo)\n", (long long)faits, fSortie,
				 (double)out.Size() / (1024.0 * 1024.0));
	logger.Infof("  dont %lld sur des mots ABSENTS (%.1f%%) — la reponse juste y est « %s »\n",
				 (long long)absents, faits ? (100.0 * (double)absents / (double)faits) : 0.0,
				 ilyana::kReponseAbsente());
	logger.Infof("  %lld passages ecartes : trop courts, ou sans mot designant une seule phrase.\n",
				 (long long)refuses);
	logger.Info("");
	logger.Info("Ces exemples ne contiennent AUCUNE affirmation produite par une machine :");
	logger.Info("le contexte et la reponse sont copies du corpus, la question est faite de");
	logger.Info("mots qui en sont extraits. Rien n'y peut donc etre faux qui ne le soit deja");
	logger.Info("dans le corpus lui-meme.");
	return 0;
}

// =============================================================================
// MODE --mesurer : le tokenizer est-il adapte a ce texte ?
// -----------------------------------------------------------------------------
// POURQUOI CETTE MESURE DECIDE DE TOUT. Le tokenizer se fige a son entrainement.
// S'il decoupe mal un domaine, Ilyana l'apprend mal ET ce domaine lui coute trois
// fois plus de place dans sa fenetre de 256 tokens — deux peines pour un seul
// defaut. Et on ne peut pas le refaire apres coup sans invalider le modele, dont
// les acquis sont indexes par NUMERO de token.
//
// L'unite qui parle est le nombre d'OCTETS PAR TOKEN. Sur du francais courant, un
// bon tokenizer tourne autour de 4. En dessous de 2, le texte est reduit en
// miettes : c'est le signe qu'il faut refaire le tokenizer AVANT d'entrainer,
// pendant que c'est encore gratuit.
// =============================================================================
static int ModeMesurer(int argc, char **argv) {
	const char *fBpe = Arg(argc, argv, "--bpe", nullptr);
	const char *fTexte = Arg(argc, argv, "--texte", nullptr);
	const int64 maxOctets = ArgEntier(argc, argv, "--max-octets", 2 * 1024 * 1024);

	if (!fBpe || !fTexte) {
		logger.Info("usage : --mesurer --bpe <tokenizer.nkbpe> --texte <fichier> [--max-octets N]");
		return 1;
	}

	data::NkBpe bpe;
	if (!data::LoadBpe(fBpe, bpe)) {
		logger.Infof("ERREUR : tokenizer illisible : %s\n", fBpe);
		return 1;
	}

	NkString texte = LireFichier(fTexte);
	if (texte.Size() == 0) {
		logger.Infof("ERREUR : texte illisible ou vide : %s\n", fTexte);
		return 1;
	}
	if ((int64)texte.Size() > maxOctets)
		texte = texte.SubStr(0, (nk_size)maxOctets);

	data::NkBpeEncoder enc(bpe);
	NkVector<int32> ids;
	enc.Encode(texte, ids);
	if (ids.Size() == 0) {
		logger.Info("ERREUR : encodage vide.");
		return 1;
	}
	const double octetsParToken = (double)texte.Size() / (double)ids.Size();

	logger.Info("=== Ilyana / mesure du tokenizer ===");
	logger.Infof("%s sur %s\n", fBpe, fTexte);
	logger.Infof("  %.2f Mo -> %llu tokens, soit %.2f octets par token\n",
				 (double)texte.Size() / (1024.0 * 1024.0), (unsigned long long)ids.Size(), octetsParToken);
	if (octetsParToken >= 3.5)
		logger.Info("  VERDICT : bien adapte — ce texte est decoupe aussi finement que du francais courant.");
	else if (octetsParToken >= 2.5)
		logger.Info("  VERDICT : passable — ce texte coute plus cher qu'il ne devrait, sans etre illisible.");
	else
		logger.Info("  VERDICT : MAL ADAPTE — ce texte est reduit en miettes. Le tokenizer devrait etre refait "
					"sur un echantillon qui en contient, AVANT d'entrainer dessus.");
	return 0;
}

// =============================================================================
// MODE --aspirer : parcourir un site public et en rapporter le texte.
// -----------------------------------------------------------------------------
// RECOLTER N'EST PAS DEPOSER. Ce mode ecrit un fichier ; il ne remplit PAS la
// bibliotheque. On peut donc relire ce qui a ete pris avant de l'y verser — et il
// FAUT le faire : un site melange des articles, des mentions legales et des
// commentaires, et rien ne les distingue automatiquement.
//
// Les quatre garde-fous — meme domaine, robots.txt, delai entre requetes,
// plafond de pages — sont dans NkAspirateur.h, chacun avec sa raison.
// =============================================================================
// Le corps d'une réponse HTTP peut arriver COMPRESSÉ (Content-Encoding gzip)
// même quand on ne l'a pas demandé : certains serveurs servent des fichiers
// pré-compressés quoi qu'on annonce (constaté sur gamemath.com — l'en-tête
// gzip portait encore le nom du fichier temporaire d'origine). NkHTTPClient ne
// décode pas encore les encodages de transfert ; on reconnaît donc le cadre
// RFC 1952 à sa signature (1F 8B 08) et on le défait ici, avec l'inflate déjà
// écrit du dépôt (NkImage::DecompressRaw). Renvoie false si le cadre est
// annoncé gzip mais indécodable — l'appelant doit alors JETER la page plutôt
// que de verser du binaire dans la récolte.
static bool DegzipSiBesoin(NkString &corps) {
	const uint8 *p = reinterpret_cast<const uint8 *>(corps.Data());
	const usize n = corps.Size();
	if (n < 18 || p[0] != 0x1Fu || p[1] != 0x8Bu || p[2] != 8u)
		return true; // pas du gzip : rien a faire
	const uint8 flg = p[3];
	usize i = 10; // signature(2) + methode(1) + drapeaux(1) + mtime(4) + xfl(1) + os(1)
	if (flg & 0x04u) { // FEXTRA
		if (i + 2 > n)
			return false;
		i += 2 + ((usize)p[i] | ((usize)p[i + 1] << 8));
	}
	if (flg & 0x08u) { // FNAME, terminee par 0
		while (i < n && p[i])
			++i;
		if (i < n)
			++i;
	}
	if (flg & 0x10u) { // FCOMMENT
		while (i < n && p[i])
			++i;
		if (i < n)
			++i;
	}
	if (flg & 0x02u) // FHCRC
		i += 2;
	if (i + 8 > n) {
		logger.Infof("  gzip : corps TRONQUE (%llu octets, donnees attendues a %llu)\n",
					 (unsigned long long)n, (unsigned long long)i);
		return false;
	}
	// ISIZE : taille decompressee mod 2^32, dans les 4 derniers octets.
	const usize isize = (usize)p[n - 4] | ((usize)p[n - 3] << 8) | ((usize)p[n - 2] << 16) |
						((usize)p[n - 1] << 24);
	if (isize == 0) {
		corps = NkString();
		return true;
	}
	if (isize > (usize)256 * 1024 * 1024) {
		logger.Infof("  gzip : taille annoncee aberrante (%llu octets)\n", (unsigned long long)isize);
		return false; // garde-fou : une page web ne fait pas 256 Mo
	}
	NkVector<uint8> clair;
	clair.Resize(isize);
	usize ecrit = 0;
	if (!NkDeflate::DecompressRaw(p + i, n - 8 - i, clair.Data(), isize, ecrit)) {
		// Le champ d'erreur se LIT : corps complet ou pas, tailles en jeu.
		logger.Infof("  gzip : inflate ECHOUE (corps %llu o, donnees %llu o a l'offset %llu, "
					 "annonce %llu o, ecrit %llu o)\n",
					 (unsigned long long)n, (unsigned long long)(n - 8 - i), (unsigned long long)i,
					 (unsigned long long)isize, (unsigned long long)ecrit);
		return false;
	}
	NkString s;
	s.Append(reinterpret_cast<const char *>(clair.Data()), ecrit);
	corps = s;
	return true;
}

static int ModeAspirer(int argc, char **argv) {
	const char *urlDepart = Arg(argc, argv, "--url", nullptr);
	const char *fSortie = Arg(argc, argv, "--sortie", "aspire.txt");
	const int64 maxPages = ArgEntier(argc, argv, "--max-pages", 100);
	const int64 delaiMs = ArgEntier(argc, argv, "--delai", 1000);
	const int64 profMax = ArgEntier(argc, argv, "--profondeur", 3);

	if (!urlDepart) {
		logger.Info("usage : --aspirer --url https://un.site [--sortie f] [--max-pages 100]");
		logger.Info("        [--delai 1000] [--profondeur 3]");
		return 1;
	}

	const NkString depart(urlDepart);
	const NkString hote = media::HoteDe(depart);
	if (hote.Size() == 0) {
		logger.Info("ERREUR : adresse incomprehensible.");
		return 1;
	}

	net::NkHTTPClient http;

	// robots.txt D'ABORD. Le lire apres avoir commence a parcourir n'aurait aucun
	// sens : on aurait deja pris ce qu'on n'avait pas le droit de prendre.
	NkVector<NkString> interdits;
	{
		NkString rurl = media::RacineDe(depart);
		rurl.Append("/robots.txt");
		const net::NkHTTPResponse r = http.Get(rurl.CStr());
		if (r.statusCode == 200) {
			NkString corpsRobots = r.body;
			if (!DegzipSiBesoin(corpsRobots))
				corpsRobots = NkString(); // indecodable : comme absent
			media::LireRobots(corpsRobots, interdits);
			logger.Infof("robots.txt : %llu chemin(s) interdits, respectes.\n",
						 (unsigned long long)interdits.Size());
		} else {
			// La reponse PORTE la raison de l'echec. Ne pas la lire, c'est
			// diagnostiquer a l'aveugle : un code 0 ne dit pas si l'adresse est
			// mal formee, si la connexion a echoue ou si le TLS a ete refuse.
			logger.Infof("robots.txt : code %u%s%s\n", (unsigned)r.statusCode,
						 r.error.Empty() ? "" : " — ", r.error.Empty() ? "" : r.error.CStr());
		}
	}

	NkVector<NkString> file;	 // adresses a visiter
	NkVector<int32> profondeurs; // profondeur de chacune
	NkVector<NkString> vues;	 // deja visitees
	file.PushBack(depart);
	profondeurs.PushBack(0);

	NkString out;
	int64 pages = 0;
	int64 refusees = 0;
	int64 vides = 0;
	int64 totalGardes = 0;
	int64 totalJetes = 0;

	// Curseur plutot que retrait en tete. Retirer le premier element d'un vecteur
	// a chaque tour est couteux (tout se decale), et surtout `Erase(0)` s'est
	// revele fatal ici — un plantage a la destruction de la chaine, le « 0 » etant
	// pris pour un pointeur nul. Avancer un indice ne peut pas se tromper.
	nk_size curseur = 0;
	while (curseur < file.Size() && pages < maxPages) {
		const NkString url = file[curseur];
		const int32 prof = profondeurs[curseur];
		++curseur;

		bool dejaVue = false;
		for (nk_size i = 0; i < vues.Size(); ++i)
			if (vues[i] == url) {
				dejaVue = true;
				break;
			}
		if (dejaVue)
			continue;
		vues.PushBack(url);

		if (!(media::HoteDe(url) == hote) || media::CheminInterdit(url, interdits)) {
			++refusees;
			continue;
		}

		const net::NkHTTPResponse r = http.Get(url.CStr());
		// Pause APRES la requete : la suivante ne partira pas avant.
		NkChrono::Sleep((int64)delaiMs);
		if (r.statusCode != 200 || r.body.Size() == 0) {
			++vides;
			// La PREMIERE page qui echoue dit pourquoi. Sans cela, un site
			// entierement inaccessible se solde par « 0 page lue » sans raison,
			// et l'on cherche le defaut du mauvais cote.
			if (vides == 1)
				logger.Infof("  echec sur %s : code %u%s%s\n", url.CStr(), (unsigned)r.statusCode,
							 r.error.Empty() ? "" : " — ", r.error.Empty() ? "" : r.error.CStr());
			continue;
		}

		NkString corps = r.body;
		if (!DegzipSiBesoin(corps)) {
			++vides;
			if (vides == 1)
				logger.Infof("  echec sur %s : corps annonce gzip mais indecodable\n", url.CStr());
			continue;
		}

		int64 gardes = 0;
		int64 jetes = 0;
		const NkString texte = media::PageWebVersTexte(corps, gardes, jetes);
		totalGardes += gardes;
		totalJetes += jetes;
		++pages;
		if (texte.Size() > 0) {
			// L'adresse est ECRITE dans le fichier : une fois les pages mises bout
			// a bout, plus rien ne dirait d'ou vient un paragraphe, et une
			// citation deviendrait invérifiable.
			out.Append("[source] ");
			out.Append(url);
			out.Append("\n\n", 2);
			out.Append(texte);
		} else {
			++vides;
		}

		if (prof < (int32)profMax) {
			NkVector<NkString> liens;
			media::ReleverLiens(corps, url, liens);
			for (nk_size i = 0; i < liens.Size(); ++i)
				if (media::HoteDe(liens[i]) == hote) {
					file.PushBack(liens[i]);
					profondeurs.PushBack(prof + 1);
				}
		}
		if ((pages % 10) == 0)
			logger.Infof("  %lld pages lues, %llu en attente...\n", (long long)pages,
						 (unsigned long long)file.Size());
	}

	if (!EcrireFichier(fSortie, out)) {
		logger.Infof("ERREUR : ecriture impossible : %s\n", fSortie);
		return 1;
	}

	logger.Info("=== Ilyana / aspiration ===");
	logger.Infof("site %s — %lld page(s) lues, %.2f Mo de texte\n", hote.CStr(), (long long)pages,
				 (double)out.Size() / (1024.0 * 1024.0));
	logger.Infof("  %lld blocs gardes, %lld ecartes (menus, pieds de page)\n", (long long)totalGardes,
				 (long long)totalJetes);
	logger.Infof("  %lld adresses ecartees (hors domaine ou interdites), %lld pages sans texte\n",
				 (long long)refusees, (long long)vides);
	if (pages >= maxPages)
		logger.Info("  ARRETE PAR LE PLAFOND : il restait des pages a visiter (--max-pages pour aller plus loin).");
	logger.Info("");
	logger.Info("RELIRE ce fichier AVANT de le deposer : un site melange articles, mentions");
	logger.Info("legales et commentaires, et rien ne les distingue automatiquement. Ensuite :");
	logger.Infof("  --ajouter --livre %s --domaine <x> --url %s --bibliotheque <dossier>\n", fSortie,
				 urlDepart);
	return 0;
}

// =============================================================================
// MODE --sonder : ce qu'un corpus de PDF DÉCLARE, avant d'écrire du code
// =============================================================================
// Compte, sur tout un dossier, les clés que le lecteur ignore aujourd'hui
// (/Info, /Outlines, /Annots, /StructTreeRoot…). Le périmètre du lecteur a été
// fixé par mesure sur 95 documents réels ; toute extension suit la même
// discipline. Ce mode ne dépose RIEN : il compte.
static int ModeSonder(int argc, char **argv) {
	const char *dossier = Arg(argc, argv, "--dossier", nullptr);
	const char *fCsv = Arg(argc, argv, "--csv", nullptr);
	if (!dossier) {
		logger.Info("usage : --sonder --dossier <dossier de PDF> [--csv sortie.csv]");
		return 1;
	}

	NkVector<NkString> fichiers = NkDirectory::GetFiles(dossier, "*.pdf");
	if (fichiers.Size() == 0) {
		logger.Infof("ERREUR : aucun .pdf dans %s\n", dossier);
		return 1;
	}

	// Totaux « nombre de DOCUMENTS qui possèdent la clé », jamais le cumul des
	// occurrences : la question posée est « combien de documents en ont ? ».
	int32 nOuverts = 0, nChiffres = 0, nInfo = 0, nTitre = 0, nMeta = 0;
	int32 nSignets = 0, nAnnots = 0, nLiens = 0, nNotes = 0, nSurl = 0;
	int32 nForm = 0, nStruct = 0, nDests = 0, nEmb = 0, nLang = 0;
	int32 nLzw = 0, nCcitt = 0, nJbig2 = 0, nJpx = 0, nDct = 0;
	int64 totalSignets = 0, totalLiens = 0;
	// Phase 1 : ce que la lecture rend vraiment, pas ce que le document déclare.
	int32 nTitreLu = 0, nTitreAccents = 0, nDateLue = 0, nLangueLue = 0;
	NkVector<NkString> exemplesTitres;
	// Polices : ce qui décide si un document est lisible ou muet.
	int32 nDocFf3 = 0, nDocType1C = 0, nDocType1CMuet = 0;
	int64 totPolices = 0, totType1C = 0, totType1CMuettes = 0;
	NkVector<NkString> docsType1CMuets;
	int32 nSignetsSansStruct = 0;
	// Phase 3 : (page, MCID) suffit-il a identifier un bloc marque ?
	int32 nMcrStm = 0, nFormBalise = 0;
	int64 totMcrStm = 0;
	// L'arbre est-il seulement DECLARE, ou effectivement LU ? Distinction qui a
	// coute cher sur /ToUnicode : une cle presente qu'on ne sait pas lire se
	// presente exactement comme une cle absente.
	int32 nStructLu = 0, nStructMuet = 0;
	int64 totEntreesStruct = 0;

	// Le TITRE est écrit dans le CSV, pas seulement compté. Le journal passe par
	// la console, qui n'est pas en UTF-8 sous Windows : elle mange les accents
	// et ferait croire à un défaut de conversion là où il n'y en a pas. Un
	// fichier, lui, garde les octets tels quels.
	NkString csv("fichier;ouvert;info;champs;titre;metadata;signets;annots;liens;"
				 "notes;surlignages;champsForm;structTree;dests;embarques;lang;lzw;titreLu\n");

	for (nk_size i = 0; i < fichiers.Size(); ++i) {
		const ilyana::SondePdf s = ilyana::SonderPdf(fichiers[i].CStr());
		if (s.chiffre)
			++nChiffres;
		if (!s.ouvert) {
			if (fCsv) {
				csv.Append(fichiers[i]);
				csv.Append(";0;0;0;0;0;0;0;0;0;0;0;0;0;0;0;0\n");
			}
			continue;
		}
		++nOuverts;
		if (s.info) ++nInfo;
		if (s.titre) ++nTitre;
		if (s.metadata) ++nMeta;
		if (s.signets > 0) { ++nSignets; totalSignets += s.signets; }
		if (s.annots > 0) ++nAnnots;
		if (s.liens > 0) { ++nLiens; totalLiens += s.liens; }
		if (s.notes > 0) ++nNotes;
		if (s.surlignages > 0) ++nSurl;
		if (s.champsForm > 0) ++nForm;
		if (s.structTree) ++nStruct;
		// Valeur MARGINALE des signets une fois la structure livrée : un document
		// qui porte les deux n'a rien à gagner de plus de /Outlines, puisque
		// /StructTreeRoot contient déjà la hiérarchie H1..H6. Seule cette
		// population-ci justifierait encore le chantier.
		if (s.signets > 0 && !s.structTree) ++nSignetsSansStruct;
		if (s.mcrAvecStm > 0) { ++nMcrStm; totMcrStm += s.mcrAvecStm; }
		if (s.structTree) {
			if (s.entreesStruct > 0) { ++nStructLu; totEntreesStruct += s.entreesStruct; }
			else ++nStructMuet;
		}
		if (s.formDansDocBalise) ++nFormBalise;
		if (s.dests) ++nDests;
		if (s.embarques) ++nEmb;
		if (s.lang) ++nLang;
		if (s.lzw) ++nLzw;
		if (s.ccitt) ++nCcitt;
		if (s.jbig2) ++nJbig2;
		if (s.jpx) ++nJpx;
		if (s.dct) ++nDct;
		if (!s.titreLu.Empty()) {
			++nTitreLu;
			// On garde les titres ACCENTUÉS : ce sont eux qui prouvent que la
			// conversion d'encodage fonctionne. Un titre ASCII ne prouve rien.
			if (s.titreNonAscii && exemplesTitres.Size() < 6u) {
				++nTitreAccents;
				exemplesTitres.PushBack(s.titreLu);
			} else if (s.titreNonAscii) {
				++nTitreAccents;
			}
		}
		if (s.dateLue) ++nDateLue;
		if (!s.langueLue.Empty()) ++nLangueLue;
		totPolices += s.polices;
		totType1C += s.policesType1C;
		totType1CMuettes += s.policesType1CMuettes;
		if (s.policesFontFile3 > 0) ++nDocFf3;
		if (s.policesType1C > 0) ++nDocType1C;
		if (s.policesType1CMuettes > 0) {
			++nDocType1CMuet;
			// Les NOMS sont conservés : un compteur ne dit pas si ces documents
			// sont réellement en échec de lecture, or c'est cela seul qui décide
			// si le chantier vaut d'être ouvert.
			docsType1CMuets.PushBack(fichiers[i]);
		}

		if (fCsv) {
			char ligne[512];
			snprintf(ligne, sizeof(ligne), ";1;%d;%d;%d;%d;%d;%d;%d;%d;%d;%d;%d;%d;%d;%d;%d;",
					 s.info ? 1 : 0, s.infoChamps, s.titre ? 1 : 0, s.metadata ? 1 : 0, s.signets,
					 s.annots, s.liens, s.notes, s.surlignages, s.champsForm, s.structTree ? 1 : 0,
					 s.dests ? 1 : 0, s.embarques ? 1 : 0, s.lang ? 1 : 0, s.lzw ? 1 : 0);
			csv.Append(fichiers[i]);
			csv.Append(ligne);
			// Titre en dernière colonne : les points-virgules qu'il pourrait
			// contenir sont remplacés, sinon ils décaleraient les colonnes de
			// tout le reste du fichier.
			NkString t = s.titreLu;
			for (nk_size k = 0; k < t.Size(); ++k)
				if (t.Data()[k] == ';')
					((char *)t.Data())[k] = ',';
			csv.Append(t);
			csv.Append("\n", 1);
		}
		if (((int32)i % 25) == 24)
			logger.Infof("  %llu / %llu documents sondes...\n", (unsigned long long)(i + 1),
						 (unsigned long long)fichiers.Size());
	}

	const double base = (nOuverts > 0) ? (double)nOuverts : 1.0;
	auto pct = [&](int32 n) { return 100.0 * (double)n / base; };

	logger.Info("=== Ilyana / sondage PDF ===");
	logger.Infof("%llu fichier(s), %d ouvert(s), %d chiffre(s) (refuses)\n",
				 (unsigned long long)fichiers.Size(), nOuverts, nChiffres);
	logger.Info("Presence par cle, en pourcentage des documents OUVERTS :");
	logger.Infof("  /Info non vide        %4d  (%.0f%%)   dont /Title : %d (%.0f%%)\n", nInfo,
				 pct(nInfo), nTitre, pct(nTitre));
	logger.Infof("  /Metadata (XMP)       %4d  (%.0f%%)\n", nMeta, pct(nMeta));
	logger.Infof("  /Outlines (signets)   %4d  (%.0f%%)   total : %lld entrees\n", nSignets,
				 pct(nSignets), (long long)totalSignets);
	logger.Infof("  /Annots               %4d  (%.0f%%)\n", nAnnots, pct(nAnnots));
	logger.Infof("    dont /Link          %4d  (%.0f%%)   total : %lld liens\n", nLiens, pct(nLiens),
				 (long long)totalLiens);
	logger.Infof("    dont /Text          %4d  (%.0f%%)\n", nNotes, pct(nNotes));
	logger.Infof("    dont /Highlight     %4d  (%.0f%%)\n", nSurl, pct(nSurl));
	logger.Infof("  /AcroForm /Fields     %4d  (%.0f%%)\n", nForm, pct(nForm));
	logger.Infof("  /StructTreeRoot       %4d  (%.0f%%)\n", nStruct, pct(nStruct));
	logger.Infof("  /Outlines SANS /StructTreeRoot %4d  (%.0f%%)  <- valeur MARGINALE des signets\n",
				 nSignetsSansStruct, pct(nSignetsSansStruct));
	logger.Infof("    ... arbre effectivement LU : %4d doc  (%lld entrees)   MUET : %d doc\n",
				 nStructLu, (long long)totEntreesStruct, nStructMuet);
	logger.Info("  -- (page, MCID) suffit-il a identifier un bloc marque ? --");
	logger.Infof("    /MCR portant une cle /Stm    : %4d doc  (%lld occurrences)\n", nMcrStm,
				 (long long)totMcrStm);
	logger.Infof("    doc balises AVEC Form XObject : %4d doc  <- borne SUPERIEURE\n", nFormBalise);
	logger.Infof("  /Dests                %4d  (%.0f%%)\n", nDests, pct(nDests));
	logger.Infof("  /Names /EmbeddedFiles %4d  (%.0f%%)\n", nEmb, pct(nEmb));
	logger.Infof("  /Lang                 %4d  (%.0f%%)\n", nLang, pct(nLang));
	logger.Info("Filtres presents dans les octets bruts :");
	logger.Infof("  LZWDecode %d  ·  CCITTFax %d  ·  JBIG2 %d  ·  JPX %d  ·  DCT(JPEG) %d\n", nLzw,
				 nCcitt, nJbig2, nJpx, nDct);

	logger.Info("--- Polices : de quoi depend la LISIBILITE ---");
	logger.Infof("  polices distinctes rencontrees      : %lld\n", (long long)totPolices);
	logger.Infof("  documents avec un /FontFile3 (CFF)  : %4d  (%.0f%%)\n", nDocFf3, pct(nDocFf3));
	logger.Infof("  documents avec du /Subtype /Type1C  : %4d  (%.0f%%)   (%lld polices)\n",
				 nDocType1C, pct(nDocType1C), (long long)totType1C);
	logger.Infof("  ... dont MUETTES (ni /ToUnicode ni /Differences) : %4d doc  (%.0f%%)   (%lld "
				 "polices)\n",
				 nDocType1CMuet, pct(nDocType1CMuet), (long long)totType1CMuettes);
	for (nk_size i = 0; i < docsType1CMuets.Size(); ++i)
		logger.Infof("      %s\n", docsType1CMuets[i].CStr());

	logger.Info("--- Phase 1 : ce que la LECTURE rend (et non ce qui est declare) ---");
	logger.Infof("  titres lus            %4d  (%.0f%%)   dont accentues : %d\n", nTitreLu,
				 pct(nTitreLu), nTitreAccents);
	logger.Infof("  dates analysees       %4d  (%.0f%%)\n", nDateLue, pct(nDateLue));
	logger.Infof("  langues lues          %4d  (%.0f%%)\n", nLangueLue, pct(nLangueLue));
	if (exemplesTitres.Size() > 0) {
		logger.Info("  Titres accentues, a lire A L'OEIL — un compteur ne distingue pas");
		logger.Info("  un texte correct d'un charabia bien forme :");
		for (nk_size i = 0; i < exemplesTitres.Size(); ++i)
			logger.Infof("    | %s\n", exemplesTitres[i].CStr());
	}

	if (fCsv && !EcrireFichier(fCsv, csv))
		logger.Infof("ATTENTION : ecriture du CSV impossible : %s\n", fCsv);
	else if (fCsv)
		logger.Infof("Detail par document : %s\n", fCsv);
	return 0;
}

// =============================================================================
// MODE --empreintes : le filet de non-régression de l'extraction de texte
// =============================================================================
// Écrit, pour chaque PDF, une EMPREINTE du texte assemblé — pas un compte.
//
// POURQUOI UNE EMPREINTE ET NON UN COMPTE. Un compte de passages identique ne
// prouve rien : l'ORDRE peut changer sans que le nombre bouge, et c'est
// précisément le risque quand on touche au rendu. Une empreinte du texte
// concaténé change au moindre octet déplacé.
//
// POURQUOI DEUX POPULATIONS. Les documents avec /StructTreeRoot DOIVENT changer
// — c'est l'objet du travail sur l'ordre de lecture logique. Ceux qui n'en ont
// pas ne doivent pas bouger d'un octet. Les mélanger rendrait la mesure
// aveugle : une régression sur les seconds serait noyée dans les changements
// attendus des premiers. La colonne `struct` sépare donc les deux.
static int ModeEmpreintes(int argc, char **argv) {
	const char *dossier = Arg(argc, argv, "--dossier", nullptr);
	const char *fCsv = Arg(argc, argv, "--csv", "empreintes.csv");
	if (!dossier) {
		logger.Info("usage : --empreintes --dossier <dossier de PDF> [--csv f]");
		return 1;
	}
	NkVector<NkString> fichiers = NkDirectory::GetFiles(dossier, "*.pdf");
	if (fichiers.Size() == 0) {
		logger.Infof("ERREUR : aucun .pdf dans %s\n", dossier);
		return 1;
	}

	// Le COMMIT est écrit dans le fichier. Une référence dont on ignore l'état
	// du dépôt n'est pas reproductible : dans six semaines, un écart ne dirait
	// plus si le code a changé ou si la référence était déjà obsolète.
	const char *commit = Arg(argc, argv, "--commit", "inconnu");
	NkString csv("# empreintes d'extraction PDF — reference de non-regression\n# commit : ");
	csv.Append(commit);
	csv.Append("\n# dossier : ");
	csv.Append(dossier);
	csv.Append("\nfichier;struct;pages;passages;caracteres;empreinte\n");
	int32 nAvec = 0, nSans = 0;

	for (nk_size i = 0; i < fichiers.Size(); ++i) {
		// La présence de /StructTreeRoot décide de la population : elle est lue
		// AVANT l'extraction, avec le même lecteur.
		bool avecStruct = false;
		{
			media::pdf::NkPdfDoc d;
			if (d.Open(fichiers[i].CStr()) == media::pdf::NK_PDF_OK)
				avecStruct = d.DictGet(d.Catalog(), "StructTreeRoot").IsDictLike();
			d.Close();
		}

		int64 pages = 0, muettes = 0;
		const NkString texte = ilyana::LirePdf(fichiers[i].CStr(), pages, muettes);

		// FNV-1a 64 bits : court, sans dépendance, et suffisant ici — on ne se
		// défend pas contre un adversaire, on détecte un changement.
		uint64 h = 1469598103934665603ull;
		for (nk_size k = 0; k < texte.Size(); ++k) {
			h ^= static_cast<uint64>(static_cast<uint8>(texte.Data()[k]));
			h *= 1099511628211ull;
		}

		// Le nombre de PASSAGES en plus du hash : une empreinte qui diffère dit
		// « ça a changé » et rien d'autre. Les compteurs disent de combien et
		// dans quel sens — c'est ce qui évite une heure de bissection quand un
		// document sort du lot.
		int64 passages = 0;
		{
			nk_size k = 0;
			while (k + 1 < texte.Size()) {
				if (texte.Data()[k] == '\n' && texte.Data()[k + 1] == '\n') {
					++passages;
					k += 2;
					continue;
				}
				++k;
			}
			if (texte.Size() > 0)
				++passages; // le dernier bloc n'est pas suivi d'un separateur
		}

		char ligne[256];
		snprintf(ligne, sizeof(ligne), ";%d;%lld;%lld;%llu;%016llx\n", avecStruct ? 1 : 0,
				 (long long)pages, (long long)passages, (unsigned long long)texte.Size(),
				 (unsigned long long)h);
		csv.Append(fichiers[i]);
		csv.Append(ligne);
		if (avecStruct)
			++nAvec;
		else
			++nSans;
		if (((int32)i % 25) == 24)
			logger.Infof("  %llu / %llu...\n", (unsigned long long)(i + 1),
						 (unsigned long long)fichiers.Size());
	}

	if (!EcrireFichier(fCsv, csv)) {
		logger.Infof("ERREUR : ecriture impossible : %s\n", fCsv);
		return 1;
	}
	logger.Info("=== Ilyana / empreintes d'extraction ===");
	logger.Infof("%llu document(s) : %d AVEC /StructTreeRoot, %d SANS\n",
				 (unsigned long long)fichiers.Size(), nAvec, nSans);
	logger.Infof("Empreintes ecrites : %s\n", fCsv);
	logger.Info("Les %d documents SANS structure doivent rester IDENTIQUES apres toute");
	logger.Info("modification du rendu. Un seul ecart = on s'arrete et on cherche.");
	return 0;
}

int main(int argc, char **argv) {
	if (Drapeau(argc, argv, "--empreintes"))
		return ModeEmpreintes(argc, argv);
	if (Drapeau(argc, argv, "--sonder"))
		return ModeSonder(argc, argv);
	if (Drapeau(argc, argv, "--controle"))
		return ModeControle(argc, argv);
	if (Drapeau(argc, argv, "--aspirer"))
		return ModeAspirer(argc, argv);
	if (Drapeau(argc, argv, "--mesurer"))
		return ModeMesurer(argc, argv);
	if (Drapeau(argc, argv, "--citations"))
		return ModeCitations(argc, argv);
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
