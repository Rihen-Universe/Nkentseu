// =============================================================================
// Applications/NKMeshMenuTest/src/main.cpp
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// POURQUOI CE PROGRAMME EXISTE
// -----------------------------------------------------------------------------
// Rodolf dit voir, en mode edition, « un menu Extruder » qui semble SEUL et sans
// contexte. Le menu contextuel du maillage declare QUATORZE commandes et filtre
// par sous-mode. Les deux affirmations ne peuvent pas etre vraies ensemble.
//
// Ce banc tranche par le CHIFFRE : il construit la liste exactement comme la vue
// la construit -- meme fonction, `NkMeshMenuBuild` -- et compte. Si aucun mode ne
// peut rendre une seule entree, alors ce que Rodolf voit n'est pas ce menu, et il
// faut chercher ailleurs. C'est le seul but de ce programme.
//
// L'ATTENDU EST DERIVE, PAS RECOPIE
// -----------------------------------------------------------------------------
// Un attendu en dur (11 / 12 / 11) se perime des que quelqu'un ajoute une
// commande, et se met a crier rouge sur une table correcte. Le banc REFAIT donc
// le compte depuis les masques de la table, et compare le resultat de la vraie
// fonction a ce recompte independant. Les deux chemins doivent tomber d'accord.
// Ma prediction ecrite avant la premiere execution -- 11 / 12 / 11 -- est dans
// scratchpad/agent-edition/attendu-e1b.md ; elle est affichee pour comparaison,
// mais ce n'est PAS elle qui fait rougir.
//
// SANS FENETRE ET SANS DEVICE
// -----------------------------------------------------------------------------
// `NkModelerMeshMenu.h` n'inclut que `NkDemo3DHost.h` (declarations pures, un
// seul include : NKCore/NkTypes.h) et `NkShortcutTable.h` (en-tete seul). La
// construction de la liste est du calcul pur. En revanche `NkMeshMenuRun` appelle
// la facade `demo::`, qui vit dans NkDemo3D.cpp et exige un device : CE BANC NE
// L'APPELLE JAMAIS, et c'est ce qui lui permet d'exister en console.
//
// CE QUI N'EST PAS COUVERT — a dire, plutot que de laisser croire
// -----------------------------------------------------------------------------
//   * ce qui est PEINT a l'ecran : `NkCtxMenuDraw` n'est pas appele ici ;
//   * le clic, le survol, le defilement du menu ;
//   * ce que fait une commande une fois choisie (`NkMeshMenuRun`) ;
//   * l'etat dans lequel Rodolf se trouve (viseur hors edition) : cet etat n'est
//     pas une propriete du menu, il se mesure dans la vue.
// =============================================================================
#include "NK3DModeler/Shell/NkModelerMeshMenu.h"
#include "NK3DModeler/Shell/NkModelerDeleteMenu.h" // le menu X (Blender)

#include <stdio.h>
#include <string.h>

using namespace nkentseu;
using namespace nkentseu::nk3d;

namespace {

	int32 gPass = 0, gFail = 0;

	void Cas(bool ok, const char *quoi) {
		if (ok) {
			gPass++;
			printf("  OK    %s\n", quoi);
		} else {
			gFail++;
			printf("  FAIL  %s\n", quoi);
		}
	}

	// Un appel de `NkMeshMenuBuild` et ses tableaux, au meme endroit : la vue les
	// declare sur sa pile, le banc aussi. Les recopier a chaque test invitait a en
	// dimensionner un de travers.
	struct Sortie {
			const char *labels[kMeshMenuCap];
			const char *shorts[kMeshMenuCap];
			bool enabled[kMeshMenuCap];
			NkMeshCmd ids[kMeshMenuCap];
			char keybuf[kMeshMenuCap][32];
			int32 n = 0;

			void Construire(int32 selMask, int32 selCount,
							const editorkit::NkShortcutTable &sc) {
				memset(keybuf, 0, sizeof(keybuf));
				n = NkMeshMenuBuild(selMask, selCount, sc, labels, shorts, enabled, ids, keybuf);
			}
			bool Contient(NkMeshCmd c) const {
				for (int32 i = 0; i < n; ++i)
					if (ids[i] == c)
						return true;
				return false;
			}
			int32 Actives() const {
				int32 a = 0;
				for (int32 i = 0; i < n; ++i)
					if (enabled[i])
						++a;
				return a;
			}
			int32 Grisees() const { return n - Actives(); }
	};

	// RECOMPTE INDEPENDANT : on relit la table et on applique le masque a la main.
	// C'est le second chemin ; s'il s'accorde avec `NkMeshMenuBuild`, le filtre
	// fait bien ce que la table declare.
	int32 RecompteDepuisLaTable(uint8 mode) {
		int32 nT = 0;
		const NkMeshMenuEntry *T = NkMeshMenuTable(nT);
		int32 n = 0;
		for (int32 i = 0; i < nT; ++i)
			if (T[i].modes & mode)
				++n;
		return n;
	}

	const char *NomMode(uint8 m) {
		return m == NK_MM_FACE ? "FACE" : (m == NK_MM_EDGE ? "ARETE" : "SOMMET");
	}

	// ── LE ZERO DU COMPTEUR, AVANT TOUT LE RESTE ────────────────────────────
	// Le compteur d'entrees ACTIVES doit avoir un zero connu, sinon les chiffres
	// qui suivent ne veulent rien dire. Une seule entree de la table porte
	// `needsSel = false` (Bisect) : sans selection, il doit en rester EXACTEMENT
	// une. Un compteur de pixels a deja rendu 40 sans aucune cible dans ce depot.
	void Test_ZeroDuCompteur(const editorkit::NkShortcutTable &sc) {
		printf("\n-- ZERO DU COMPTEUR (rien de selectionne : une seule commande survit) --\n");
		const uint8 modes[3] = {NK_MM_VERTEX, NK_MM_EDGE, NK_MM_FACE};
		for (int32 m = 0; m < 3; ++m) {
			Sortie s;
			s.Construire(modes[m], 0, sc);
			char q[160];
			snprintf(q, sizeof(q), "Z%d %-6s selection=0 -> %d active(s), %d grisee(s)", m + 1,
					 NomMode(modes[m]), (int)s.Actives(), (int)s.Grisees());
			Cas(s.Actives() == 1 && s.Contient(NkMeshCmd::Bisect) && s.enabled[0] == false, q);
		}
		for (int32 m = 0; m < 3; ++m) {
			Sortie s;
			s.Construire(modes[m], 3, sc);
			char q[160];
			snprintf(q, sizeof(q), "Z4 %-6s selection=3 -> %d grisee(s) (attendu 0)",
					 NomMode(modes[m]), (int)s.Grisees());
			Cas(s.Grisees() == 0, q);
		}
	}

	// ── P1..P4 : COMBIEN D'ENTREES PAR MODE ─────────────────────────────────
	void Test_CompteParMode(const editorkit::NkShortcutTable &sc) {
		printf("\n-- COMBIEN D'ENTREES S'AFFICHENT, PAR MODE --\n");
		int32 nT = 0;
		(void)NkMeshMenuTable(nT);
		Cas(nT == 14, "P4 la table declare 14 commandes");

		// Ma prediction ecrite avant la premiere execution. Affichee pour
		// comparaison ; ce qui fait foi est le recompte derive juste apres.
		static const int32 kPrediction[3] = {11, 12, 11};
		const uint8 modes[3] = {NK_MM_VERTEX, NK_MM_EDGE, NK_MM_FACE};
		for (int32 m = 0; m < 3; ++m) {
			Sortie s;
			s.Construire(modes[m], 3, sc);
			const int32 derive = RecompteDepuisLaTable(modes[m]);
			char q[200];
			snprintf(q, sizeof(q),
					 "P%d %-6s -> %d entrees (recompte independant %d ; prediction ecrite %d%s)",
					 m + 1, NomMode(modes[m]), (int)s.n, (int)derive, (int)kPrediction[m],
					 derive == kPrediction[m] ? "" : "  <-- LA PREDICTION ETAIT FAUSSE");
			Cas(s.n == derive, q);
		}
	}

	// ── N1, N2 : LES NEGATIFS QUI DOIVENT ECHOUER ───────────────────────────
	void Test_Negatifs(const editorkit::NkShortcutTable &sc) {
		printf("\n-- NEGATIFS (ils doivent echouer si le filtre casse) --\n");
		int32 nT = 0;
		(void)NkMeshMenuTable(nT);
		const uint8 modes[3] = {NK_MM_VERTEX, NK_MM_EDGE, NK_MM_FACE};

		// N1 : la plainte de Rodolf, testee de front.
		bool unSeul = false;
		for (int32 m = 0; m < 3; ++m) {
			Sortie s;
			s.Construire(modes[m], 3, sc);
			if (s.n <= 1)
				unSeul = true;
		}
		Cas(!unSeul,
			"N1 AUCUN mode ne rend une entree seule -- donc le menu N'EST PAS ce que Rodolf voit");

		// N2 : le filtre agit. Mutation prevue : retirer le `continue` -> les trois
		// modes rendent 14 et ce critere rougit. S'il reste vert, il ne garde rien.
		bool toutes = false;
		for (int32 m = 0; m < 3; ++m) {
			Sortie s;
			s.Construire(modes[m], 3, sc);
			if (s.n == nT)
				toutes = true;
		}
		Cas(!toutes, "N2 aucun mode ne rend les 14 -- le filtre par mode AGIT vraiment");

		// N3/N4 : le piege nomme par le header lui-meme (un masque recopie en le
		// DECALANT ne se voit que sur un seul des trois modes).
		{
			Sortie v, e, f;
			v.Construire(NK_MM_VERTEX, 3, sc);
			e.Construire(NK_MM_EDGE, 3, sc);
			f.Construire(NK_MM_FACE, 3, sc);
			Cas(v.Contient(NkMeshCmd::Fusionner) && !e.Contient(NkMeshCmd::Fusionner) &&
					!f.Contient(NkMeshCmd::Fusionner),
				"N3 Fusionner est en SOMMET seul (ni ARETE ni FACE)");
			Cas(f.Contient(NkMeshCmd::Inserer) && !v.Contient(NkMeshCmd::Inserer) &&
					!e.Contient(NkMeshCmd::Inserer),
				"N4 Inserer une face est en FACE seul (ni SOMMET ni ARETE)");
			Cas(v.Contient(NkMeshCmd::Extruder) && e.Contient(NkMeshCmd::Extruder) &&
					f.Contient(NkMeshCmd::Extruder),
				"P5 Extruder (NK_MM_ALL) sort dans LES TROIS modes");
		}
	}

	// ── P6 : LA PRIORITE DU MASQUE COMBINE ──────────────────────────────────
	// Maj+clic sur la pastille COMBINE les modes. La vue doit alors choisir un
	// mode d'affichage, et `NkMeshMenuBuild:159` prend FACE d'abord.
	void Test_MasqueCombine(const editorkit::NkShortcutTable &sc) {
		printf("\n-- MASQUE COMBINE (Maj+clic est offert : il faut savoir qui gagne) --\n");
		struct Cb {
				int32 mask;
				uint8 attendu;
				const char *quoi;
		};
		static const Cb kC[] = {
			{1 | 4, NK_MM_FACE, "P6 masque SOMMET|FACE -> le menu affiche FACE"},
			{1 | 2, NK_MM_EDGE, "P6 masque SOMMET|ARETE -> le menu affiche ARETE"},
			{2 | 4, NK_MM_FACE, "P6 masque ARETE|FACE -> le menu affiche FACE"},
			{7, NK_MM_FACE, "P6 masque des TROIS -> le menu affiche FACE"},
			{0, NK_MM_VERTEX, "P6 masque VIDE -> repli SOMMET (jamais le vide)"},
		};
		for (int32 i = 0; i < (int32)(sizeof(kC) / sizeof(kC[0])); ++i) {
			Sortie s;
			s.Construire(kC[i].mask, 3, sc);
			Cas(s.n == RecompteDepuisLaTable(kC[i].attendu), kC[i].quoi);
		}
	}

	// ── N5 : UN LIBELLE DE RACCOURCI EST UNE PROMESSE ECRITE A L'ECRAN ──────
	// Trois entrees declarent `command = ""` (SeparerAretes, Spin, Bisect) : elles
	// ne doivent RIEN annoncer. Une promesse fausse est pire que pas de libelle --
	// paye la veille dans Nogee, quatre raccourcis annonces et zero branche.
	void Test_AucuneFaussePromesse(const editorkit::NkShortcutTable &sc) {
		printf("\n-- AUCUNE FAUSSE PROMESSE DE RACCOURCI --\n");
		const uint8 modes[3] = {NK_MM_VERTEX, NK_MM_EDGE, NK_MM_FACE};
		int32 nT = 0;
		const NkMeshMenuEntry *T = NkMeshMenuTable(nT);
		bool toutesVides = true, aumoinsUneVide = false;
		for (int32 m = 0; m < 3; ++m) {
			Sortie s;
			s.Construire(modes[m], 3, sc);
			for (int32 i = 0; i < s.n; ++i) {
				// Retrouver la declaration de cette ligne pour savoir si elle a une cle.
				const char *cle = "";
				for (int32 t = 0; t < nT; ++t)
					if (T[t].cmd == s.ids[i])
						cle = T[t].command;
				const bool sansCle = (cle == nullptr || *cle == '\0');
				if (sansCle) {
					aumoinsUneVide = true;
					if (s.shorts[i] == nullptr || s.shorts[i][0] != '\0')
						toutesVides = false;
				}
			}
		}
		Cas(aumoinsUneVide, "N5a des entrees SANS cle de raccourci existent bien dans la table");
		Cas(toutesVides, "N5b aucune entree sans cle n'annonce de touche (chaine VIDE)");
	}


	// ══ LE MENU X, CELUI DE BLENDER ════════════════════════════════════════
	// X n'execute plus : il OUVRE un menu de ONZE entrees. La regle de la maison
	// -- « une entree qui ne peut rien produire se MONTRE et se REFUSE » -- est
	// exactement ce qu'il faut ici : un menu ampute apprendrait a l'utilisateur
	// que Blender n'a pas ces commandes.
	// ⚠ CE BANC N'APPELLE JAMAIS `NkDelMenuRun` : elle passe par les facades
	//   `demo::`, qui exigent un device. La CONSTRUCTION de la liste, elle, est
	//   du calcul pur -- c'est ce qui permet de la mesurer ici, sans fenetre.
	struct DelSnap {
			const char *labels[kDelMenuCap];
			const char *motifs[kDelMenuCap];
			bool enabled[kDelMenuCap];
			NkDelCmd ids[kDelMenuCap];
			int32 n = 0;
			void Build(int32 mask, int32 sel) {
				n = NkDelMenuBuild(mask, sel, labels, enabled, ids, motifs);
			}
			int32 Actives() const {
				int32 k = 0;
				for (int32 i = 0; i < n; ++i)
					if (enabled[i])
						++k;
				return k;
			}
			// LA SIGNATURE DES ACTIVES, pas leur NOMBRE. C'est une correction de
			// mon propre attendu (canal R22) : j'avais ecrit « le NOMBRE d'entrees
			// actives differe d'un sous-mode a l'autre ». Il ne differe PAS -- il
			// vaut quatre partout. Ce ne sont simplement pas LES MEMES quatre. Un
			// critere qui aurait compare deux nombres egaux par construction
			// n'aurait rien pu distinguer ; celui-ci compare des ENSEMBLES.
			uint32 Signature() const {
				uint32 b = 0;
				for (int32 i = 0; i < n; ++i)
					if (enabled[i])
						b |= (1u << (uint32)(int32)ids[i]);
				return b;
			}
			bool Actif(NkDelCmd c) const {
				for (int32 i = 0; i < n; ++i)
					if (ids[i] == c)
						return enabled[i];
				return false;
			}
	};

	void Test_MenuX() {
		printf("\n-- LE MENU X (Blender) : onze entrees, et celles qui refusent disent pourquoi --\n");
		DelSnap v, e, f, vide;
		v.Build(1, 3);
		e.Build(2, 3);
		f.Build(4, 3);
		vide.Build(4, 0); // rien de selectionne

		// (z) LE ZERO DU COMPTEUR : sans selection, AUCUNE entree n'est active.
		//     Sans ce cas, « quatre actives » plus bas ne prouverait pas que le
		//     compteur sait rendre autre chose.
		printf("   sans selection : %d entrees, %d actives\n", vide.n, vide.Actives());
		Cas(vide.n == 11 && vide.Actives() == 0,
			"X0 sans selection : les 11 entrees sont LA, et aucune n'est active");

		// (a) LES ONZE SONT PRESENTES DANS LES TROIS SOUS-MODES.
		printf("   sommet %d entrees / %d actives · arete %d / %d · face %d / %d\n", v.n, v.Actives(),
			   e.n, e.Actives(), f.n, f.Actives());
		Cas(v.n == 11 && e.n == 11 && f.n == 11,
			"X1 les 11 entrees de Blender sont presentes dans LES TROIS sous-modes");

		// (b) ET LES ENSEMBLES D'ACTIVES DIFFERENT.
		printf("   signatures : sommet=%u arete=%u face=%u\n", v.Signature(), e.Signature(),
			   f.Signature());
		Cas(v.Signature() != e.Signature() && e.Signature() != f.Signature() &&
				v.Signature() != f.Signature(),
			"X2 les trois sous-modes n'activent PAS les memes entrees");

		// (c) LES TROIS SUPPRESSIONS SONT ACTIVES PARTOUT. C'est le coeur du menu :
		//     « Faces » doit etre choisissable meme en sous-mode Sommet, sinon le
		//     menu ne fait que repeter ce que la touche faisait deja.
		Cas(v.Actif(NkDelCmd::Faces) && v.Actif(NkDelCmd::Edges) && v.Actif(NkDelCmd::Vertices) &&
				e.Actif(NkDelCmd::Faces) && f.Actif(NkDelCmd::Vertices),
			"X3 Sommets / Aretes / Faces sont choisissables dans les TROIS sous-modes");

		// (d) LE DISSOLVE EST CONTEXTUEL, ET C'EST LUI QUI FAIT LA DIFFERENCE.
		Cas(v.Actif(NkDelCmd::DissolveVerts) && !v.Actif(NkDelCmd::DissolveFaces) &&
				f.Actif(NkDelCmd::DissolveFaces) && !f.Actif(NkDelCmd::DissolveVerts) &&
				e.Actif(NkDelCmd::DissolveEdges) && !e.Actif(NkDelCmd::DissolveVerts),
			"X4 le dissolve n'est actif que sur l'element du sous-mode courant");

		// (e) CHAQUE REFUS PORTE UN MOTIF, ET CHAQUE ENTREE ACTIVE N'EN PORTE PAS.
		//     Les deux moities comptent : un motif sur une entree active voudrait
		//     dire que deux autorites repondent a la meme question.
		bool refusMuet = false, actifBavard = false;
		const DelSnap *tous[4] = {&v, &e, &f, &vide};
		for (int32 k = 0; k < 4; ++k)
			for (int32 i = 0; i < tous[k]->n; ++i) {
				const bool aMotif = (tous[k]->motifs[i][0] != 0);
				if (!tous[k]->enabled[i] && !aMotif)
					refusMuet = true;
				if (tous[k]->enabled[i] && aMotif)
					actifBavard = true;
			}
		Cas(!refusMuet, "X5 aucune entree refusee n'est MUETTE : chacune dit pourquoi");
		Cas(!actifBavard, "X5b aucune entree ACTIVE ne porte de motif de refus");

		// (f) LE DEFAUT DE CHAQUE SOUS-MODE EST CE QUE X FAISAIT AVANT.
		Cas(NkDelMenuDefaut(1) == NkDelCmd::Vertices && NkDelMenuDefaut(2) == NkDelCmd::Edges &&
				NkDelMenuDefaut(4) == NkDelCmd::Faces,
			"X6 le defaut de chaque sous-mode est celui que la touche executait");

		// (g) NEGATIF : une entree non implementee ne doit JAMAIS devenir active,
		//     quel que soit le sous-mode ou la selection. Sans ce cas, il suffirait
		//     d'un `return ""` egare pour que le menu promette ce qu'il ne sait pas
		//     faire -- et une fausse promesse est pire qu'une absence.
		bool promesse = false;
		const NkDelCmd kPasEcrites[] = {NkDelCmd::OnlyEdgesFaces, NkDelCmd::OnlyFaces,
										NkDelCmd::LimitedDissolve, NkDelCmd::EdgeCollapse,
										NkDelCmd::EdgeLoops};
		for (int32 k = 0; k < 4; ++k)
			for (int32 q = 0; q < 5; ++q)
				if (tous[k]->Actif(kPasEcrites[q]))
					promesse = true;
		Cas(!promesse, "X7 aucune des 5 commandes non ecrites ne se declare active");
	}

} // namespace

int main() {
	printf("=== NKMeshMenuTest : combien d'entrees le menu de maillage montre-t-il ? ===\n");
	printf("    (console, sans fenetre et sans device ; NkMeshMenuRun n'est jamais appelee)\n");

	// Table de raccourcis VIDE a ce stade : `FormatFor` rend faux partout, donc
	// aucune colonne de touche n'est remplie. C'est deliberement le cas le plus
	// severe pour N5 -- et c'est aussi ce qui montre que N5b passerait au vert
	// « pour la mauvaise raison » si on s'arretait la. D'ou N5a, qui exige qu'il
	// y ait bien des entrees sans cle a mesurer. Brancher la VRAIE table est le
	// pas suivant, et il porte sur main.cpp.
	editorkit::NkShortcutTable sc;

	Test_ZeroDuCompteur(sc);
	Test_CompteParMode(sc);
	Test_Negatifs(sc);
	Test_MasqueCombine(sc);
	Test_AucuneFaussePromesse(sc);
	Test_MenuX();

	printf("\n=== Resultat : %d OK / %d FAIL (sur %d cas nommes) ===\n", gPass, gFail,
		   gPass + gFail);
	return gFail == 0 ? 0 : 1;
}
