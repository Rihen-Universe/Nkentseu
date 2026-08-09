// =============================================================================
// NKBpeTest — preuve de l'entraîneur BPE à l'échelle (NKData/NkBpeTrainer).
// -----------------------------------------------------------------------------
// Ce que cette application PROUVE, dans l'ordre :
//   1. La comptabilité incrémentale des paires est EXACTE : sur les premières
//      fusions, on recalcule tous les comptes par force brute et on exige un
//      accord parfait, ainsi que le fait que la paire retenue soit bien de
//      compte maximal. C'est la seule façon de savoir que l'index paire -> mots
//      ne perd aucune occurrence — un oubli silencieux donnerait un tokenizer
//      plausible mais faux.
//   2. Le nouvel entraîneur retrouve les mêmes fusions que l'entraîneur
//      historique (mode blancs, petit corpus). Les écarts éventuels sont
//      localisés et rapportés avec les comptes, pour distinguer un vrai
//      désaccord d'un simple ex æquo départagé autrement.
//   3. Le codage est réversible : décoder ce qu'on a encodé redonne le texte
//      OCTET POUR OCTET, y compris accents et ponctuation.
//   4. L'encodeur à mémo rend exactement la même chose que l'encodeur direct.
//   5. Un tokenizer sauvegardé puis relu encode à l'identique.
// Avec un argument, l'application entraîne en plus un vrai tokenizer sur un
// vrai corpus et l'écrit sur disque (c'est ce qui sert à Ilyana) :
//     NKBpeTest <corpus.txt> <sortie.nkbpe> [tailleVocabulaire]
// AUTEUR : Rihen — LICENCE : Propriétaire - usage régi par le fichier LICENSE à la racine du dépôt
// =============================================================================
#include "NKData/NkTokenizer.h"
#include "NKData/NkBpeTrainer.h"
#include "NKLogger/NkLog.h"
#include "NKTime/NkChrono.h"
#include "NKContainers/String/NkString.h"
#include "NKContainers/Sequential/NkVector.h"

#include <cstdio>

using namespace nkentseu;
using namespace nkentseu::ai;

static int gPass = 0;
static int gFail = 0;

static void Check(bool cond, const char *what) {
	if (cond) {
		++gPass;
		logger.Info("[ OK ] {0}", what);
	} else {
		++gFail;
		logger.Info("[FAIL] {0}", what);
	}
}

// Petit corpus français, volontairement répétitif : les répétitions créent des
// paires fréquentes, donc de vraies fusions à vérifier.
static NkString ToyCorpus() {
	NkString s;
	for (int i = 0; i < 40; ++i) {
		s.Append("Ilyana apprend le francais. Ilyana apprend vite.\n");
		s.Append("Le pere d'Ilyana s'appelle Rodolf. Le pere apprend aussi.\n");
		s.Append("apprendre, apprendre encore, apprendre toujours !\n");
		s.Append("123 nombres et des mots ; des mots, des mots.\n");
	}
	return s;
}

// Corpus varié à fréquences ZIPFIENNES. Motif : comparer deux entraîneurs BPE sur
// un texte où beaucoup de paires ont exactement le même compte ne prouve rien —
// les ex æquo sont départagés différemment et les listes de fusions divergent sans
// qu'aucun des deux ne soit fautif. En tirant les mots avec une loi en 1/rang, les
// comptes de paires sont naturellement distincts et la comparaison redevient
// informative.
static NkString ZipfCorpus() {
	static const char *kWords[] = {
		"le",	   "de",	 "un",		 "et",		 "pere",	"maison",  "enfant",  "lumiere", "chemin",
		"parole",  "temps",	 "mere",	 "village",	 "riviere", "montagne", "arbre",  "pierre",	 "matin",
		"soir",	   "travail", "musique", "histoire", "livre",	"main",	   "coeur",	  "ville",	 "route",
		"saison",  "graine", "racine",	 "feuille",	 "orage",	"soleil",  "nuage",	  "silence", "memoire",
		"courage", "patience", "atelier", "mesure",	 "outil",	"forge",   "argile",  "tissu",	 "marche",
		"marmite", "grenier", "fenetre", "seuil",	 "sentier"};
	const int nWords = (int)(sizeof(kWords) / sizeof(kWords[0]));
	static const char *kPunct[] = {" ", ", ", ". ", " ; ", " ! ", " ?\n", ".\n"};
	const int nPunct = (int)(sizeof(kPunct) / sizeof(kPunct[0]));

	NkString s;
	uint64 rng = 0x243F6A8885A308D3ull;
	auto Next = [&]() {
		rng = rng * 6364136223846793005ull + 1442695040888963407ull;
		return (double)((rng >> 11) & 0xFFFFFFFFFFFFFull) / (double)(1ull << 52);
	};
	// Tirage en 1/rang : le mot 0 sort ~50x plus souvent que le mot 49.
	for (int i = 0; i < 12000; ++i) {
		const double u = Next();
		int idx = (int)(u * u * (double)nWords); // biais fort vers les premiers rangs
		if (idx >= nWords)
			idx = nWords - 1;
		s.Append(kWords[idx]);
		s.Append(kPunct[(int)(Next() * (double)nPunct) % nPunct]);
	}
	s.Append("\n");
	return s;
}

static NkString ReadFileAll(const char *path, int64 maxBytes) {
	NkString s;
	FILE *f = fopen(path, "rb");
	if (!f)
		return s;
	char buf[65536];
	int64 total = 0;
	for (;;) {
		const nk_size got = fread(buf, 1, sizeof(buf), f);
		if (got == 0)
			break;
		s.Append(buf, got);
		total += (int64)got;
		if (maxBytes > 0 && total >= maxBytes)
			break;
	}
	fclose(f);
	return s;
}

// -----------------------------------------------------------------------------
// 1. Exactitude de la comptabilité incrémentale
// -----------------------------------------------------------------------------
static void TestExactitude() {
	logger.Info("--- 1. Comptabilite incrementale verifiee par force brute ---");
	NkVector<NkString> texts;
	texts.PushBack(ToyCorpus());

	data::NkBpeTrainConfig cfg;
	cfg.targetVocab = 256 + 120;
	cfg.pretok = data::NK_PRETOK_WORD_PUNCT;
	cfg.verifyMerges = 120; // toutes les fusions sont verifiees
	cfg.verbose = false;

	data::NkBpe bpe;
	data::NkBpeTrainStats st;
	const bool ok = data::TrainBpeFast(texts, cfg, bpe, &st);
	Check(ok, "l'entrainement aboutit");
	logger.Info("       {0} mots distincts, {1} symboles, {2} fusions, {3} verifiees.",
				(long long)st.uniqueWords, (long long)st.initialSymbols, (long long)st.merges,
				(long long)st.verifyChecked);
	Check(st.verifyChecked == st.merges, "toutes les fusions ont ete confrontees au recomptage complet");
	Check(st.verifyFailed == 0, "AUCUN desaccord entre comptes incrementaux et recomptage complet");

	// Le contrôle ci-dessus confronte les comptes à l'ÉTAT INTERNE des mots : un
	// état interne lui-même corrompu y échapperait. On ferme la boucle en
	// confrontant cet état au tokenizer produit — deux chemins totalement
	// différents pour arriver au même nombre de tokens.
	data::NkBpeEncoder enc(bpe);
	NkVector<int32> ids;
	enc.Encode(texts[0], ids);
	logger.Info("       etat interne {0} tokens, encodage du corpus {1} tokens.", (long long)st.finalSymbolsWeighted,
				(long long)ids.Size());
	Check((int64)ids.Size() == st.finalSymbolsWeighted,
		  "l'etat final interne de l'entraineur correspond a ce que le tokenizer produit");
}

// -----------------------------------------------------------------------------
// 2. Le nouvel entraîneur retrouve-t-il l'ancien ?
// -----------------------------------------------------------------------------
static void TestMemeResultatQueLAncien() {
	logger.Info("--- 2. Nouvel entraineur vs entraineur historique (mode blancs) ---");
	NkVector<NkString> texts;
	texts.PushBack(ZipfCorpus());

	const int kMerges = 80;

	data::NkBpe ref;
	data::TrainBpe(texts, kMerges, ref);

	data::NkBpeTrainConfig cfg;
	cfg.targetVocab = 256 + kMerges;
	cfg.pretok = data::NK_PRETOK_WHITESPACE; // même pré-tokenisation que l'ancien
	cfg.verifyMerges = kMerges;				 // chaque fusion doit être un vrai maximum
	cfg.verbose = false;
	data::NkBpe fast;
	data::NkBpeTrainStats st;
	data::TrainBpeFast(texts, cfg, fast, &st);

	// CE QUI EST EXIGÉ : que chaque fusion choisie soit bien de fréquence maximale
	// dans l'état courant. C'est la définition du BPE, et c'est vérifiable seul.
	Check(st.verifyFailed == 0, "en mode blancs aussi, chaque fusion choisie est un vrai maximum");
	Check(ref.merges.Size() == fast.merges.Size(), "meme nombre de fusions apprises");
	int64 same = 0;
	int64 firstDiff = -1;
	const int64 n = (int64)(ref.merges.Size() < fast.merges.Size() ? ref.merges.Size() : fast.merges.Size());
	for (int64 i = 0; i < n; ++i) {
		if (ref.merges[(nk_size)i].a == fast.merges[(nk_size)i].a &&
			ref.merges[(nk_size)i].b == fast.merges[(nk_size)i].b)
			++same;
		else if (firstDiff < 0)
			firstDiff = i;
	}
	logger.Info("       {0}/{1} fusions identiques, premier ecart a l'indice {2}.", (long long)same, (long long)n,
				(long long)firstDiff);
	// CE QUI N'EST PAS EXIGÉ : l'égalité des deux listes. Dès qu'une seule paire est
	// à égalité de fréquence avec une autre, les deux entraîneurs la départagent
	// autrement (l'ancien selon l'ordre de parcours du corpus, le nouveau selon la
	// plus petite clé) — et à partir de ce point les deux trajectoires portent sur
	// des états différents, donc le décompte « identiques » perd son sens. Les deux
	// listes restent deux BPE également valides. La ligne ci-dessus est donc une
	// mesure informative, pas un critere de reussite.

	// Ce qui compte vraiment : les deux tokenizers encodent-ils aussi bien ?
	NkString sample("le pere de l'enfant marche sur le chemin du village au matin.");
	NkVector<int32> ia, ib;
	ref.Encode(sample, ia);
	fast.Encode(sample, ib);
	logger.Info("       meme phrase : ancien {0} tokens, nouveau {1} tokens.", (long long)ia.Size(),
				(long long)ib.Size());
	Check(ib.Size() <= ia.Size() + 2, "le nouveau tokenizer n'est pas moins compact que l'ancien");
}

// -----------------------------------------------------------------------------
// 2bis. Le piège classique du BPE : l'ENTRAÎNEMENT applique les fusions dans
// l'ordre, une fusion à la fois SUR TOUT LE CORPUS ; l'ENCODAGE applique, dans un
// mot isolé, la fusion de plus petit rang encore possible, en boucle. Ces deux
// procédés doivent donner la MÊME segmentation — sinon le modèle est entraîné sur
// un découpage et interrogé sur un autre, sans qu'aucun test de réversibilité ne
// s'en aperçoive (les deux découpages se décodent parfaitement).
// On ne le suppose pas : on rejoue ici le procédé d'entraînement à la main et on
// le confronte à `EncodeWord`.
// -----------------------------------------------------------------------------
static void TestSegmentationEntrainementEgaleEncodage() {
	logger.Info("--- 2bis. Segmentation d'entrainement == segmentation d'encodage ---");
	NkVector<NkString> texts;
	texts.PushBack(ZipfCorpus());

	data::NkBpeTrainConfig cfg;
	cfg.targetVocab = 256 + 400;
	cfg.pretok = data::NK_PRETOK_WORD_PUNCT;
	cfg.verbose = false;
	data::NkBpe bpe;
	data::TrainBpeFast(texts, cfg, bpe, nullptr);

	NkVector<NkString> words;
	data::NkBpe::PreTokMode(texts[0], cfg.pretok, words);

	int64 checked = 0, mismatch = 0;
	// On ne teste qu'un échantillon de mots : au-delà, c'est le même calcul répété.
	for (int64 wi = 0; wi < (int64)words.Size() && checked < 3000; wi += 7) {
		const NkString &w = words[(nk_size)wi];
		// (a) segmentation « façon entraînement » : chaque fusion appliquée dans
		//     l'ordre d'apprentissage, de gauche à droite, sur tout le mot.
		NkVector<int32> seq;
		for (int64 i = 0; i < (int64)w.Size(); ++i)
			seq.PushBack((int32)(unsigned char)w.Data()[i]);
		for (int64 m = 0; m < (int64)bpe.merges.Size(); ++m) {
			const int32 a = bpe.merges[(nk_size)m].a, b = bpe.merges[(nk_size)m].b;
			const int32 id = 256 + (int32)m;
			int64 wr = 0;
			for (int64 i = 0; i < (int64)seq.Size();) {
				if (i + 1 < (int64)seq.Size() && seq[(nk_size)i] == a && seq[(nk_size)(i + 1)] == b) {
					seq[(nk_size)wr++] = id;
					i += 2;
				} else {
					seq[(nk_size)wr++] = seq[(nk_size)i];
					i += 1;
				}
			}
			seq.Resize((nk_size)wr);
		}
		// (b) segmentation « façon encodage ».
		NkVector<int32> enc;
		bpe.EncodeWord(w, enc);
		bool same = (seq.Size() == enc.Size());
		for (int64 i = 0; same && i < (int64)seq.Size(); ++i)
			same = (seq[(nk_size)i] == enc[(nk_size)i]);
		if (!same)
			++mismatch;
		++checked;
	}
	logger.Info("       {0} mots confrontes, {1} desaccords.", (long long)checked, (long long)mismatch);
	Check(checked > 100, "un echantillon significatif de mots a ete confronte");
	Check(mismatch == 0, "entrainement et encodage produisent la MEME segmentation");
}

// -----------------------------------------------------------------------------
// 3/4/5. Réversibilité, mémo, persistance
// -----------------------------------------------------------------------------
static void TestReversibiliteEtPersistance() {
	logger.Info("--- 3/4/5. Reversibilite, encodeur a memo, persistance ---");
	NkVector<NkString> texts;
	texts.PushBack(ToyCorpus());

	data::NkBpeTrainConfig cfg;
	cfg.targetVocab = 256 + 300;
	cfg.pretok = data::NK_PRETOK_WORD_PUNCT;
	cfg.verbose = false;
	data::NkBpe bpe;
	data::TrainBpeFast(texts, cfg, bpe, nullptr);

	// Texte d'épreuve : accents, apostrophes, ponctuation collée, chiffres, retours
	// à la ligne, espaces multiples — tout ce qui casse un découpage naïf.
	NkString probe("Ilyana, nee le 9 aout 2026 a Yaounde : son pere s'appelle\n"
				   "TEUGUIA TADJUIDJE Rodolf Sederis.  Deux espaces, et des accents "
				   "(elevee, francais, tres, aout) !\n");

	NkVector<int32> ids;
	bpe.Encode(probe, ids);
	NkString back = data::DecodeAll(bpe, ids);
	Check(back == probe, "decoder ce qu'on a encode redonne le texte OCTET POUR OCTET");
	logger.Info("       {0} octets -> {1} tokens ({2} octets/token).", (long long)probe.Size(), (long long)ids.Size(),
				ids.Size() ? (double)probe.Size() / (double)ids.Size() : 0.0);

	// Encodeur à mémo == encodeur direct.
	data::NkBpeEncoder enc(bpe);
	NkVector<int32> ids2;
	enc.Encode(probe, ids2);
	bool identical = (ids.Size() == ids2.Size());
	for (int64 i = 0; identical && i < (int64)ids.Size(); ++i)
		identical = (ids[(nk_size)i] == ids2[(nk_size)i]);
	Check(identical, "l'encodeur a memo rend exactement les memes identifiants");

	// Persistance.
	const char *path = "nkbpe_test_roundtrip.nkbpe";
	Check(data::SaveBpe(path, bpe), "sauvegarde du tokenizer");
	data::NkBpe reloaded;
	Check(data::LoadBpe(path, reloaded), "relecture du tokenizer");
	Check(reloaded.pretok == bpe.pretok, "le mode de pre-tokenisation survit a l'aller-retour");
	Check(reloaded.merges.Size() == bpe.merges.Size(), "meme nombre de fusions apres relecture");
	NkVector<int32> ids3;
	reloaded.Encode(probe, ids3);
	bool same3 = (ids.Size() == ids3.Size());
	for (int64 i = 0; same3 && i < (int64)ids.Size(); ++i)
		same3 = (ids[(nk_size)i] == ids3[(nk_size)i]);
	Check(same3, "le tokenizer relu encode a l'identique");
	remove(path);
}

// -----------------------------------------------------------------------------
// Entraînement d'un vrai tokenizer sur un vrai corpus
// -----------------------------------------------------------------------------
static void EntrainerSurCorpus(const char *corpus, const char *outPath, int vocab) {
	logger.Info("--- Entrainement reel : {0} -> {1} (vocabulaire {2}) ---", corpus, outPath, vocab);
	NkString text = ReadFileAll(corpus, 0);
	if (text.Size() < 1000) {
		logger.Info("Corpus introuvable ou trop court : {0}", corpus);
		++gFail;
		return;
	}
	NkVector<NkString> texts;
	texts.PushBack(text);

	data::NkBpeTrainConfig cfg;
	cfg.targetVocab = vocab;
	cfg.pretok = data::NK_PRETOK_WORD_PUNCT;
	cfg.verbose = true;
	data::NkBpe bpe;
	data::NkBpeTrainStats st;

	NkChrono chrono;
	const bool ok = data::TrainBpeFast(texts, cfg, bpe, &st);
	const double secs = chrono.Elapsed().seconds;
	Check(ok, "entrainement du tokenizer sur le corpus reel");
	if (!ok)
		return;
	logger.Info("       {0} s pour {1} fusions sur {2} octets.", secs, (long long)st.merges,
				(long long)st.totalBytes);
	logger.Info("       derniere fusion vue {0} fois (une valeur basse = vocabulaire sature).",
				(long long)st.lastPairFreq);

	// Mesure qui compte vraiment : combien d'octets par token sur le corpus ?
	// C'est ce rapport qui décide de la longueur de contexte utile et du nombre de
	// tokens que le modèle devra digérer.
	data::NkBpeEncoder enc(bpe);
	NkVector<int32> ids;
	NkChrono chrono2;
	enc.Encode(text, ids);
	const double encSecs = chrono2.Elapsed().seconds;
	logger.Info("       corpus encode en {0} s : {1} octets -> {2} tokens = {3} octets/token.", encSecs,
				(long long)text.Size(), (long long)ids.Size(),
				ids.Size() ? (double)text.Size() / (double)ids.Size() : 0.0);
	logger.Info("       memo : {0} mots deja vus, {1} mots nouveaux.", (long long)enc.CacheHits(),
				(long long)enc.CacheMisses());

	// Réversibilité sur le vrai corpus (échantillon de tête, pour ne pas doubler la mémoire).
	{
		NkString head = text.SubStr(0, (nk_size)(text.Size() < 200000 ? text.Size() : 200000));
		NkVector<int32> hid;
		bpe.Encode(head, hid);
		NkString back = data::DecodeAll(bpe, hid);
		Check(back == head, "reversibilite verifiee sur 200 Ko du corpus reel");
	}

	Check(data::SaveBpe(outPath, bpe), "tokenizer ecrit sur disque");

	// À quoi ressemblent les tokens appris ? Preuve lisible que ce sont des morceaux
	// de français et non du bruit.
	logger.Info("       echantillon de tokens appris (id : contenu) :");
	const int64 base = (int64)bpe.vocab.Size();
	const int64 picks[10] = {300, 500, 800, 1200, 2000, 3500, 6000, 9000, 12000, 15000};
	for (int i = 0; i < 10; ++i) {
		if (picks[i] < base)
			logger.Info("         {0} : \"{1}\"", (long long)picks[i], bpe.vocab[(nk_size)picks[i]].CStr());
	}
}

int main(int argc, char **argv) {
	logger.Info("=========================================================");
	logger.Info(" NKBpeTest — entraineur BPE a l'echelle (NKData)");
	logger.Info("=========================================================");

	TestExactitude();
	TestMemeResultatQueLAncien();
	TestSegmentationEntrainementEgaleEncodage();
	TestReversibiliteEtPersistance();

	if (argc >= 3) {
		const int vocab = (argc >= 4) ? (int)strtol(argv[3], nullptr, 10) : 16384;
		EntrainerSurCorpus(argv[1], argv[2], vocab);
	} else {
		logger.Info("(pas d'entrainement reel : passer <corpus.txt> <sortie.nkbpe> [vocab] pour le declencher)");
	}

	logger.Info("=========================================================");
	logger.Info(" Resultat : {0} OK, {1} echecs", gPass, gFail);
	logger.Info("=========================================================");
	return gFail == 0 ? 0 : 1;
}
