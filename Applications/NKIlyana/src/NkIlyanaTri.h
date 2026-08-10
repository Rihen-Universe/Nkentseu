// =============================================================================
// NkIlyanaTri.h — répartition du corpus en TROIS BACS.
// -----------------------------------------------------------------------------
// POURQUOI. Les 100 017 paires de `dlg_ollama_fr.txt` ont été produites par un
// modèle local, sans source. Pour la grammaire, l'arithmétique ou le code, une
// affirmation fausse se voit et se corrige : elle est VÉRIFIABLE. Pour une date,
// un nom propre ou une coutume, non — et un modèle entraîné dessus répétera
// l'erreur avec le même aplomb que le reste. D'où la règle actée : séparer, dès
// la préparation des données, ce qui est vérifiable de ce qui ne l'est pas, au
// lieu de découvrir le problème une fois le modèle entraîné.
//
// CE QUE FAIT LE TRI, ET CE QU'IL NE FAIT PAS. Ce sont des heuristiques
// lexicales, pas une compréhension du texte : elles se trompent dans les deux
// sens. Le choix assumé est donc de mettre en quarantaine AU MOINDRE DOUTE (un
// millésime, un nom de peuple, un mot d'histoire suffit), quitte à y envoyer des
// paires innocentes. Une paire vérifiable rangée en quarantaine ne coûte qu'un
// peu de corpus ; une affirmation inventée gardée à l'entraînement coûte une
// erreur apprise.
// AUTEUR : Rihen — LICENCE : Propriétaire - usage régi par le fichier LICENSE à la racine du dépôt
// =============================================================================
#pragma once

#include "NKContainers/String/NkString.h"

namespace ilyana {

	enum Bac {
		BAC_VERIFIABLE = 0, // maths, code, grammaire : une erreur se constate
		BAC_QUARANTAINE = 1,// histoire, culture, noms propres, dates : non sourcé
		BAC_NEUTRE = 2		// définitions générales, logique, langue courante
	};

	// Replie les accents UTF-8 sur leur lettre de base et met en minuscules, pour
	// que la recherche de mots-clés n'ait pas à exister en deux orthographes.
	// (« vérifiable » et « verifiable » doivent déclencher la même règle.)
	inline nkentseu::NkString Replier(const nkentseu::NkString &s) {
		nkentseu::NkString o;
		const char *p = s.Data();
		const nkentseu::nk_size n = s.Size();
		for (nkentseu::nk_size i = 0; i < n; ++i) {
			const unsigned char c = (unsigned char)p[i];
			if (c == 0xC3 && i + 1 < n) {
				const unsigned char d = (unsigned char)p[i + 1];
				char r = 0;
				if (d >= 0xA0 && d <= 0xA5)
					r = 'a';
				else if (d == 0xA7)
					r = 'c';
				else if (d >= 0xA8 && d <= 0xAB)
					r = 'e';
				else if (d >= 0xAC && d <= 0xAF)
					r = 'i';
				else if (d >= 0xB2 && d <= 0xB6)
					r = 'o';
				else if (d >= 0xB9 && d <= 0xBC)
					r = 'u';
				else if (d >= 0x80 && d <= 0x85)
					r = 'a';
				else if (d == 0x87)
					r = 'c';
				else if (d >= 0x88 && d <= 0x8B)
					r = 'e';
				else if (d >= 0x8C && d <= 0x8F)
					r = 'i';
				else if (d >= 0x92 && d <= 0x96)
					r = 'o';
				else if (d >= 0x99 && d <= 0x9C)
					r = 'u';
				if (r) {
					o.Append(r);
					++i;
					continue;
				}
			}
			if (c >= 'A' && c <= 'Z')
				o.Append((char)(c - 'A' + 'a'));
			else if (c < 0x80)
				o.Append((char)c);
			// Les autres octets non-ASCII sont ignorés : ils ne portent aucun mot-clé.
		}
		return o;
	}

	inline bool Contient(const nkentseu::NkString &hay, const char *needle) {
		return hay.Find(nkentseu::NkStringView(needle)) != nkentseu::NkString::npos;
	}

	// Un millésime plausible (1000-2099) : le marqueur le plus fiable d'une
	// affirmation historique non sourcée.
	inline bool ContientMillesime(const nkentseu::NkString &s) {
		const char *p = s.Data();
		const nkentseu::nk_size n = s.Size();
		for (nkentseu::nk_size i = 0; i + 3 < n; ++i) {
			if (p[i] < '1' || p[i] > '2')
				continue;
			if (p[i + 1] < '0' || p[i + 1] > '9')
				continue;
			if (p[i + 2] < '0' || p[i + 2] > '9')
				continue;
			if (p[i + 3] < '0' || p[i + 3] > '9')
				continue;
			// Pas au milieu d'un nombre plus long (« 12345 » n'est pas une date).
			if (i > 0 && p[i - 1] >= '0' && p[i - 1] <= '9')
				continue;
			if (i + 4 < n && p[i + 4] >= '0' && p[i + 4] <= '9')
				continue;
			return true;
		}
		return false;
	}

	inline Bac Classer(const nkentseu::NkString &bloc) {
		const nkentseu::NkString t = Replier(bloc);

		// ---- Quarantaine d'abord : au moindre doute, on isole ----------------
		static const char *kHisto[] = {
			"siecle",	  "guerre",		 "empereur",   "empire",	 "dynastie",   "royaume",	"roi ",
			"reine",	  "president",	 "revolution", "colonis",	 "independan", "bataille",	"traite de",
			"antiquite",  "moyen age",	 "prehistoire","archeolog",  "ethnie",	   "peuple ",	"tribu",
			"coutume",	  "tradition",	 "ancetre",	   "mythe",		 "legende",	   "divinite",	"rituel",
			"civilisation","fonde en",	 "ne en ",	   "mort en ",	 "regne",	   "conquete",	"esclav",
			"dieu ",	  "religion",	 "prophete",   "saint ",	 "eglise",	   "mosquee",	"royaute"};
		for (int i = 0; i < (int)(sizeof(kHisto) / sizeof(kHisto[0])); ++i)
			if (Contient(t, kHisto[i]))
				return BAC_QUARANTAINE;
		if (ContientMillesime(t))
			return BAC_QUARANTAINE;

		// ---- Vérifiable : maths, code, grammaire ------------------------------
		// Mots-clés délibérément SPÉCIFIQUES. Les formes courtes et tentantes ont
		// été écartées après constat sur le corpus réel : « aire » attrape
		// « savoir-faire », « somme » attrape « sommeil », « fonction » attrape
		// « fonctionnement », « moyenne » attrape « en moyenne », « condition » et
		// « instruction » attrapent la langue courante. Un bac « vérifiable » rempli
		// de généralités ne vaut rien : mieux vaut en laisser passer dans le bac
		// neutre que d'y faire entrer ce qui n'est pas vérifiable.
		static const char *kVerif[] = {
			// mathématiques
			"calcul", "addition", "soustraction", "multiplication", "equation", "somme de", "la somme",
			"produit de", "fraction", "pourcentage", "nombre premier", "racine carree", "geometrie",
			"triangle", "rectangle", "perimetre", "moyenne de", "moyenne arithmetique", "probabilite",
			"derivee", "integrale", "puissance de", "diviseur", "multiple de", "pair ou impair",
			"aire d", "l'aire", "division euclidienne", "theoreme", "chiffres apres la virgule",
			// code
			"algorithme", "programmation", "langage de programmation", "python", "javascript", "c++",
			"compilateur", "recursi", "pointeur", "code source", "debogage", "structure de donnees",
			"boucle for", "boucle while", "une fonction qui", "variable locale", "variable globale",
			"tableau d", "syntaxe de", "octet", "binaire", "hexadecimal",
			// grammaire
			"verbe", "conjug", "pluriel", "singulier", "accord du", "adjectif", "adverbe", "sujet du",
			"complement d", "participe", "orthograph", "grammaire", "nom commun", "pronom",
			"article defini", "terminaison", "auxiliaire", "imparfait", "passe compose", "subjonctif",
			"infinitif"};
		for (int i = 0; i < (int)(sizeof(kVerif) / sizeof(kVerif[0])); ++i)
			if (Contient(t, kVerif[i]))
				return BAC_VERIFIABLE;

		return BAC_NEUTRE;
	}

} // namespace ilyana
