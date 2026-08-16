// =============================================================================
// RegleFacile.cpp — LE MEME JEU QUE RegleContratNu.cpp, EN TROIS FONCTIONS.
//
//     RegleContratNu.cpp   contrat nu                  ~560 lignes
//     RegleFacile.cpp     ConquerorRegleFacile.h      ~110 lignes, ce fichier
//
// Les deux jouent EXACTEMENT le meme jeu : carre 5x5, DUPLIQUER sur une case
// voisine vide, les ennemis adjacents a la case posee changent de camp, la
// partie s'arrete des qu'un joueur ne peut plus jouer.
//
// CE QUE VOUS N'ECRIVEZ PLUS
// --------------------------
// Creer et detruire une instance, creer et detruire un etat, le cloner, le
// serialiser, le deserialiser, le hacher, dire si la partie est finie, dire qui
// a gagne, dire si un joueur est bloque, verifier qu'un coup est legal, exposer
// le plateau en JSON, exposer les parametres. Dix-huit fonctions, ecrites une
// fois pour toutes dans ConquerorRegleFacile.h.
//
// Il reste les TROIS qui contiennent votre jeu :
//
//     Construire       a quoi la partie ressemble au depart
//     CoupsPossibles   ce qu'on a le droit de faire
//     Appliquer        ce qui se passe quand on le fait
//
// COMMENCEZ PAR CE FICHIER. Lisez RegleContratNu.cpp ensuite, pour voir ce que
// l'echafaudage vous epargne — et pour savoir quoi ecrire le jour ou votre
// moteur aura besoin d'un etat que `Partie` ne sait pas porter.
//
// POUR L'UTILISER : copier dans travail/rules/ et sauvegarder.
// =============================================================================

#include "Conqueror/ConquerorRegleFacile.h"

using namespace nkentseu;
using namespace nkentseu::conqueror;
using namespace nkentseu::conqueror::facile;

// -----------------------------------------------------------------------------
// VOTRE JEU. Trois methodes, et rien d'autre.
// -----------------------------------------------------------------------------
struct RegleFacile {

	static constexpr int32 kCote = 5;

	// ---- 1. A QUOI LA PARTIE RESSEMBLE AU DEPART ----------------------------
	void Construire(Grille &g) {
		g.topologie = NkcTopology::Square4;	  // 4 voisins : le plus simple a suivre
		g.nbJoueurs = 2;
		g.AjouterRectangle(kCote, kCote);
		g.PoserAuxCoins(2);	  // un totem par joueur, aux coins opposes (symetrie §4.2)
	}

	// ---- 2. CE QU'ON A LE DROIT DE FAIRE ------------------------------------
	//
	// DUPLIQUER : depuis un de mes totems, vers une case voisine VIDE.
	//
	// L'ORDRE EST NORMATIF (REGLES §17.4) : cases par index croissant, puis
	// voisins dans l'ordre de la topologie. Il fixe l'ordre de generation, donc
	// la reproductibilite. Ne le changez jamais sans raison — et si vous le
	// changez, sachez que toutes vos mesures anterieures deviennent incomparables.
	void CoupsPossibles(const Partie &p, ListeCoups &out) {
		NkcCoord voisins[8];
		for (int32 i = 0; i < p.nbCases; ++i) {
			if (p.cases[i].owner != static_cast<int8>(p.joueur)) continue;
			const NkcCoord de = p.ou[i];
			const int32	   n  = p.Voisins(de, voisins, 8);
			for (int32 k = 0; k < n; ++k)
				if (p.Vide(voisins[k]))
					out.Dupliquer(p.joueur, de, voisins[k]);
		}
		// PASSER est ajoute automatiquement si cette liste reste vide : le
		// contrat exige qu'il ne soit legal QUE dans ce cas (REGLES §13).
	}

	// ---- 3. CE QUI SE PASSE QUAND ON LE FAIT --------------------------------
	void Appliquer(Partie &p, const NkcMove &m, Evenements &ev) {
		// Le cadre a deja verifie que le coup est legal : ici, on l'execute.
		p.Poser(m.to, m.player);
		ev.Duplique(m.player, m.from, m.to);
		p.conquete[m.player] += 10;	  // en DIXIEMES entiers : 10 == 1,0 point

		// Les ennemis voisins de la case POSEE changent de camp. Une seule
		// vague : pas de propagation en chaine, c'est le palier 0.
		NkcCoord	voisins[8];
		const int32 n		= p.Voisins(m.to, voisins, 8);
		int32		retournes = 0;
		for (int32 k = 0; k < n; ++k) {
			if (!p.Ennemie(voisins[k], m.player)) continue;
			const int8 ancien = p.Proprietaire(voisins[k]);
			p.Poser(voisins[k], m.player);
			ev.Retourne(m.player, voisins[k], ancien);
			p.energie[m.player]	 += 2;
			p.conquete[m.player] += 10;
			++retournes;
		}
		// CASCADE : le sommet emotionnel du jeu (PXG 1). L'atelier l'affiche en
		// gros au centre du plateau — encore faut-il l'emettre.
		if (retournes >= 2) ev.Cascade(m.player, m.to, retournes);

		p.PasserLaMain();
		// Le cadre recompte les totems et teste la fin de partie tout seul.
	}
};

// Une ligne, et le module est complet : les vingt et une fonctions de la vtable
// et les deux symboles exportes.
NKC_REGLES(RegleFacile, "RegleFacile", "1.0.0", "Cours ConquerorLab")
