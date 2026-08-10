// =============================================================================
// NkIlyanaBibliotheque — déposer des livres, les indexer, s'en servir.
// -----------------------------------------------------------------------------
// L'IDÉE, EN UNE PHRASE. Un livre sert deux fois et de deux manières : on
// ENTRAÎNE Ilyana dessus pour qu'elle acquière la langue du domaine — son
// vocabulaire, ses tournures, sa façon de raisonner —, et on l'INDEXE pour
// qu'elle en tire des faits exacts et citables. Les poids retiennent une manière
// de parler ; l'index retient le texte.
//
// POURQUOI LES DEUX, ET PAS L'UN OU L'AUTRE :
//  - les POIDS sont permanents mais approximatifs. Ce qu'ils gardent d'un livre
//    de physique, c'est la façon dont la physique s'énonce. Une constante précise
//    en sera reconstruite de mémoire, donc parfois fausse — et on ne peut ni
//    vérifier ce qui a été retenu, ni corriger un fait isolé sans tout refaire ;
//  - l'INDEX est exact, vérifiable, et se met à jour en quelques secondes. Mais
//    il ne rend que ce qui est écrit : il n'apprend rien à Ilyana, il lui donne
//    de quoi lire.
//
// LA STRUCTURE SUR DISQUE, ET POURQUOI ELLE EST AINSI :
//   bibliotheque/
//     tout.txt        tous les ouvrages bout à bout, séparés par des lignes vides
//     catalogue.txt   un ouvrage par ligne : domaine, titre, plage d'octets, source
//     index.nkidx     l'index de recherche
//
// Un SEUL fichier concaténé plutôt qu'un fichier par livre : l'entraînement veut
// un flux continu, l'index travaille sur des positions, et le catalogue suffit à
// retrouver de quel ouvrage vient un octet donné. C'est ce qui permet de CITER
// une source sans compliquer ni l'index ni l'entraînement.
// AUTEUR : Rihen — LICENCE : Propriétaire - usage régi par le fichier LICENSE à la racine du dépôt
// =============================================================================
#ifndef NK_ILYANA_BIBLIOTHEQUE_H
#define NK_ILYANA_BIBLIOTHEQUE_H

#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/String/NkString.h"
#include "NKCore/NkTypes.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace ilyana {
	using namespace nkentseu;

	struct Ouvrage {
		NkString domaine;
		NkString titre;
		NkString source;
		uint64 debut = 0; // premier octet dans tout.txt
		uint64 fin = 0;	  // premier octet APRÈS l'ouvrage
	};

	// -------------------------------------------------------------------------
	// Nettoyage d'un ouvrage avant dépôt.
	//
	// Trois maux, trois remèdes — et chacun se paie cher si on l'ignore :
	//  - les retours chariot Windows : tout le découpage du dépôt cherche
	//    « \n\n » et ne trouve rien dans un fichier en « \r\n\r\n ». Déjà
	//    constaté : un corpus entier compté comme UN SEUL bloc, sans erreur ;
	//  - les lignes vides en rafale : elles fabriquent des passages vides ;
	//  - les paragraphes démesurés : un livre converti sans ligne vide devient un
	//    passage unique de plusieurs méga-octets. Inutilisable — on ne peut ni le
	//    citer, ni le classer (BM25 normalise par la longueur, donc un bloc
	//    énorme est systématiquement mal noté), ni le montrer.
	// -------------------------------------------------------------------------
	inline NkString NettoyerOuvrage(const NkString &brut, int64 tailleCible = 1200) {
		NkString sansCr;
		sansCr.Reserve(brut.Size());
		for (nk_size i = 0; i < brut.Size(); ++i)
			if (brut.Data()[i] != '\r')
				sansCr.Append(brut.Data()[i]);

		// Réduction des lignes vides multiples à une seule.
		NkString net;
		net.Reserve(sansCr.Size());
		int sauts = 0;
		for (nk_size i = 0; i < sansCr.Size(); ++i) {
			const char c = sansCr.Data()[i];
			if (c == '\n') {
				++sauts;
				if (sauts <= 2)
					net.Append(c);
			} else {
				sauts = 0;
				net.Append(c);
			}
		}

		// Recoupe des blocs trop longs, à la fin d'une PHRASE et non au milieu
		// d'un mot : un passage cité doit rester lisible.
		NkString out;
		out.Reserve(net.Size() + net.Size() / 8);
		nk_size i = 0;
		while (i < net.Size()) {
			nk_size fin = net.Find("\n\n", i);
			if (fin == NkString::npos)
				fin = net.Size();
			nk_size deb = i;
			while (deb < fin) {
				nk_size stop = deb + (nk_size)tailleCible;
				if (stop >= fin) {
					stop = fin;
				} else {
					// On cherche un point final dans la moitié suivante ; à
					// défaut une espace ; à défaut on coupe net (un mot coupé
					// vaut mieux qu'un bloc d'un méga-octet).
					nk_size p = stop;
					const nk_size limite = (fin < stop + (nk_size)tailleCible) ? fin : stop + (nk_size)tailleCible;
					bool trouve = false;
					while (p < limite) {
						const char c = net.Data()[p];
						if (c == '.' || c == '!' || c == '?') {
							stop = p + 1;
							trouve = true;
							break;
						}
						++p;
					}
					if (!trouve) {
						p = stop;
						while (p < limite && net.Data()[p] != ' ')
							++p;
						if (p < limite)
							stop = p;
					}
				}
				if (stop > deb) {
					out.Append(net.Data() + deb, stop - deb);
					out.Append("\n\n", 2);
				}
				deb = stop;
				while (deb < fin && (net.Data()[deb] == ' ' || net.Data()[deb] == '\n'))
					++deb;
			}
			i = (fin >= net.Size()) ? net.Size() : fin + 2;
		}
		return out;
	}

	// -------------------------------------------------------------------------
	// Catalogue : une ligne par ouvrage, séparée par des tabulations.
	// Volontairement en texte : il se lit à l'œil, se corrige à la main, et
	// survivra à ce programme. Un format binaire n'apporterait rien ici.
	// -------------------------------------------------------------------------
	inline bool LireCatalogue(const char *chemin, NkVector<Ouvrage> &out) {
		out.Clear();
		FILE *f = fopen(chemin, "rb");
		if (!f)
			return false;
		NkString tout;
		char buf[1 << 16];
		for (;;) {
			const nk_size got = fread(buf, 1, sizeof(buf), f);
			if (got == 0)
				break;
			tout.Append(buf, got);
		}
		fclose(f);

		nk_size i = 0;
		while (i < tout.Size()) {
			nk_size fin = tout.Find("\n", i);
			if (fin == NkString::npos)
				fin = tout.Size();
			NkString ligne = tout.SubStr(i, fin - i);
			i = fin + 1;
			if (ligne.Size() == 0 || ligne.Data()[0] == '#')
				continue;
			// domaine \t titre \t debut \t fin \t source
			NkString champs[5];
			int nc = 0;
			nk_size d = 0;
			for (nk_size p = 0; p <= ligne.Size() && nc < 5; ++p) {
				if (p == ligne.Size() || ligne.Data()[p] == '\t') {
					champs[nc++] = ligne.SubStr(d, p - d);
					d = p + 1;
				}
			}
			if (nc < 4)
				continue;
			Ouvrage o;
			o.domaine = champs[0];
			o.titre = champs[1];
			o.debut = (uint64)strtoull(champs[2].CStr(), nullptr, 10);
			o.fin = (uint64)strtoull(champs[3].CStr(), nullptr, 10);
			o.source = champs[4];
			out.PushBack(o);
		}
		return true;
	}

	inline bool AjouterAuCatalogue(const char *chemin, const Ouvrage &o) {
		const bool neuf = [&] {
			FILE *t = fopen(chemin, "rb");
			if (!t)
				return true;
			fclose(t);
			return false;
		}();
		FILE *f = fopen(chemin, "ab");
		if (!f)
			return false;
		if (neuf)
			fprintf(f, "# domaine\ttitre\tdebut\tfin\tsource\n");
		fprintf(f, "%s\t%s\t%llu\t%llu\t%s\n", o.domaine.CStr(), o.titre.CStr(), (unsigned long long)o.debut,
				(unsigned long long)o.fin, o.source.CStr());
		const bool ok = fflush(f) == 0;
		fclose(f);
		return ok;
	}

	// De quel ouvrage vient cet octet ? C'est ce qui permet de CITER.
	inline const Ouvrage *OuvrageDeLOffset(const NkVector<Ouvrage> &cat, uint64 offset) {
		for (nk_size i = 0; i < cat.Size(); ++i)
			if (offset >= cat[i].debut && offset < cat[i].fin)
				return &cat[i];
		return nullptr;
	}

} // namespace ilyana

#endif // NK_ILYANA_BIBLIOTHEQUE_H
