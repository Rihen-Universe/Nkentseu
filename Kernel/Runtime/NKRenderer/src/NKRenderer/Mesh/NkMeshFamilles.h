#pragma once
// -----------------------------------------------------------------------------
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @File    Kernel/Runtime/NKRenderer/src/NKRenderer/Mesh/NkMeshFamilles.h
// @License Proprietary - All Rights Reserved (see LICENSE)
// -----------------------------------------------------------------------------
// LES FAMILLES D'OBJETS, EN BIBLIOTHEQUE PARTAGEE (22/09, Q10.1)
//
// POURQUOI ELLES ONT DEMENAGE. Ecrites le 21/09, elles vivaient dans
// `Applications/NK3DModeler/.../NkCreaFamilles.cpp` : Noge, Nogee et NKScena ne
// pouvaient pas construire une table sans passer par le modeleur. Or une table
// n'est pas une affaire de modeleur, c'est de la GEOMETRIE. Elles sont donc ici,
// a cote de `NkEditMesh` dont elles se servent, et NK3DModeler n'est plus qu'un
// appelant parmi d'autres.
//
// ⚠️ CE MODULE NE CONNAIT PAS LA SCENE, ET C'EST LA COUPURE QUI COMPTE. L'ancien
//    code appelait `Demo3DHostCreateMeshNode` : il fabriquait des NOEUDS, donc il
//    ne pouvait servir qu'a l'application qui possede ces noeuds. Ici on rend des
//    MAILLAGES (sommets + indices + nom + matiere), en coordonnees objet. C'est
//    a l'hote de decider ce qu'il en fait : un noeud de scene, une entite ECS,
//    un export .obj, ou rien du tout. Un banc sans fenetre le prouve
//    (`NKEditMeshHarness`, batterie `famille/*`).
//
// ⚠️ LE DETAIL FAIT PARTIE DU CONTRAT (Rodolf, 21/09 : « je ne vois pas de
//    details, des creux »). En niveau « detaille », chaque constructeur pose ce
//    qui fait reconnaitre l'objet de pres, avec les operations qui EXISTENT DEJA
//    dans NkEditMesh : InsetSelectedFaces a profondeur negative (panneau en
//    creux), BevelSelected (chanfreins), SubdivideSelectedFaces, le modificateur
//    Array (rangees), SpinSelected (pieces tournees), BuildSweep (balayage le
//    long d'une courbe). Rien n'est recode.
// -----------------------------------------------------------------------------
#include "NkMeshSystem.h" // NkVertex3D + NkVector + NkVec3f (transitif), comme NkEditMesh.h

namespace nkentseu {
	namespace renderer {

		/// Les parametres qu'un appelant (ou un modele de langue) remplit. Tout est
		/// optionnel : zero = « prends ton defaut », et chaque constructeur borne ce
		/// qu'il recoit. Un objet sort TOUJOURS, meme d'un formulaire vide.
		struct NkFamilleParams {
				char famille[24] = {0};
				char style[24] = {0};
				float32 largeur = 0.f, hauteur = 0.f, profondeur = 0.f;
				int32 nombre = 0;   ///< battants, etages, marches, pieds... selon la famille
				int32 fenetres = 0; ///< maison : fenetres par facade et par etage
				bool detaille = true;
		};

		/// UNE PIECE : un maillage triangule, nomme, avec sa matiere. Les coordonnees
		/// sont celles de l'OBJET : le sol a y = 0, centre en x = z = 0.
		/// ── LES LIAISONS (25/09, Q18.1) ────────────────────────────────────────
		/// Rodolf : « les parties s'articulent : la poignee ouvre la porte ».
		/// ⚠️ CE N'EST PAS UNE SECONDE HIERARCHIE. Le modeleur en a deja une
		///    (`Demo3DHostSetNodeParent`) et chaque noeud a deja un PIVOT
		///    (`Demo3DHostSetNodeOrigin` : « point autour duquel il tourne, et la
		///    deplacer NE DEPLACE PAS la matiere »). Ce que la famille ajoute, c'est
		///    la seule chose qu'elle est seule a savoir : QUI porte QUI, OU passe
		///    l'axe, et jusqu'ou ca tourne. Le reste est deja en place, et une
		///    seconde mecanique aurait diverge de la premiere.
		/// ⚠️ ET UNE PARTIE RESTE UNE PARTIE : parente ne veut pas dire fondue. Elle
		///    garde son nom, sa matiere, sa selection ; on peut la casser du parent.
		enum class NkFamilleLiaison : uint8 {
			Fixe = 0,     ///< solidaire du parent
			Charniere,    ///< tourne autour de `axe` passant par `pivot` (butees en DEGRES)
			Glissiere,    ///< coulisse le long de `axe` (butees en METRES)
		};

		struct NkFamillePiece {
				char nom[24] = {0};
				char matiere[24] = {0};
				NkVector<NkVertex3D> verts;
				NkVector<uint32> indices;
				uint32 faces = 0; ///< faces AVANT triangulation (n-gons) : la mesure « simple contre detaille »
				// ── ARTICULATION ───────────────────────────────────────────────
				int32 parent = -1;  ///< indice d'une piece DEJA emise, ou -1
				NkFamilleLiaison liaison = NkFamilleLiaison::Fixe;
				/// Le pivot est en coordonnees OBJET, comme les sommets : c'est le
				/// point que la rotation ne doit pas bouger (l'arete des gonds, l'axe
				/// d'une roue). Il vaut (0,0,0) tant qu'aucune famille ne le place --
				/// et (0,0,0) est alors le centre de l'objet, pas « pas de pivot ».
				float32 pivot[3] = {0.f, 0.f, 0.f};
				float32 axe[3] = {0.f, 1.f, 0.f};
				float32 butee[2] = {0.f, 0.f}; ///< min, max ; egales = pas de butee ecrite
		};

		/// Construit la famille. Rend le nombre de pieces, ou -1 avec `pourquoi`.
		/// `out` est vide au depart ; il est rempli dans l'ordre de construction.
		int32 NkFamilleConstruire(const NkFamilleParams &p, NkVector<NkFamillePiece> &out, char *pourquoi,
								  uint32 capPourquoi);

		/// La famille est-elle construite ici ?
		bool NkFamilleConnue(const char *famille);

		/// Le nom de la i-eme famille, ou nullptr au-dela. Sert a ecrire la liste sans
		/// la recopier a la main (une liste recopiee se perime : c'est arrive au
		/// lexique de reconnaissance, qui connaissait des familles inexistantes).
		const char *NkFamilleNom(int32 i);

		/// ── LE FORMULAIRE, AVEC SES VALEURS AUTORISEES (Q10.1) ─────────────────
		/// ⚠️ DEFAUT PAYE LE 21/09 : le modele a ecrit `silhouette vase` -- un mot pris
		///    dans la demande, absent de la table -- et le DOCUMENT ENTIER a ete refuse.
		///    Un formulaire qui demande une valeur sans dire lesquelles existe pour
		///    etre mal rempli. Chaque champ enumere donc ses valeurs ici, a la source,
		///    et le meme texte sert a l'invite ET au controle.
		/// Ecrit dans `out` les lignes du formulaire de `famille`. Rend faux si la
		/// famille est inconnue.
		bool NkFamilleFormulaire(const char *famille, char *out, uint32 cap);

		/// `valeur` est-elle autorisee pour `champ` de `famille` ? Rend vrai aussi
		/// quand le champ est libre (un nombre). Quand elle est refusee, `remplacement`
		/// recoit la valeur par defaut -- on CORRIGE au lieu de rejeter le document.
		bool NkFamilleValeurAutorisee(const char *famille, const char *champ, const char *valeur, char *remplacement,
									  uint32 cap);

	} // namespace renderer
} // namespace nkentseu
