#pragma once
// -----------------------------------------------------------------------------
// @File    Kernel/Runtime/NKRenderer/src/NKRenderer/Tools/MeshSculpt/NkMeshSculpt.h
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @Brief   Sculpture VOLUMIQUE : appliquer un trait de brosse aux sommets reels
//          d'un NkEditMesh.
//
// LES DEUX MODES, ET CE QUI LES SEPARE VRAIMENT (le DESIGN.md de PixolSculpt ne
// le disait pas -- il ne decrit que le premier) :
//
//   Sculpture 2.5D  -- mute un G-buffer en espace ECRAN (le « pixol »). Cout
//                      borne par la resolution. ⚠️ Le resultat N'EST PAS de la
//                      geometrie : il depend du point de vue, et tourner la
//                      camera ne revele rien, parce qu'il n'y a rien derriere.
//                      C'est un mode de maquette. Il vit dans Tools/PixolSculpt.
//   Sculpture (ici) -- deplace les SOMMETS d'un NkEditMesh. Survit a la
//                      rotation, s'exporte, s'annule comme un geste de la main.
//
// ⚠️ POURQUOI CE MODULE VIT DANS NKRenderer ET NON DANS Noge
//    `Engine/Noge/src/Noge/Sculpt/NkSculpting.h` decrit deja cette sculpture --
//    mais contre `Noge::NkEditableMesh`, et NK3DModeler ne depend pas de Noge
//    (mesure du 19/09 : zero `#include "Noge/` dans toute l'application).
//    L'autorite de topologie du modeleur est `renderer::NkEditMesh`. Un module
//    de sculpture qui ne touche pas la structure que l'outil edite ne sert a
//    personne, si complet soit-il.
//
// ⚠️ LA SOUDURE LOGIQUE, ET POURQUOI ELLE N'EST PAS UN DETAIL
//    Dans ce depot, « notre Vert EST un coin » : un cube porte 24 sommets pour
//    8 coins, parce que chaque copie porte la normale de SA face. Deplacer
//    chaque sommet le long de SA normale ECARTE les trois copies d'un coin --
//    le cube se DECHIRE, et le maillage gagne des bords la ou il n'y en avait
//    pas. Un maillage dense masque completement ce defaut (ses sommets sont
//    presque tous uniques) : c'est le cas defavorable, et c'est la raison pour
//    laquelle un cube de 8 coins vaut une sphere de 287 sommets comme banc.
//    On regroupe donc les sommets COINCIDENTS, on calcule UN deplacement par
//    groupe, et on l'applique a tous ses membres.
// -----------------------------------------------------------------------------

#include "NKRenderer/Mesh/NkEditMesh.h"
#include "NKRenderer/Tools/MeshSculpt/NkBrushDesc.h"

namespace nkentseu {
	namespace renderer {

		// Un point du trait, DANS LE REPERE DE L'OBJET.
		// ⚠️ Jamais en pixels ecran : un trait en pixels ne survit pas a une
		//    rotation de camera. Jamais par indices d'elements non plus : ils sont
		//    reconstruits a chaque changement de topologie, donc ils deviennent
		//    SILENCIEUSEMENT faux -- et la sculpture EST du remaillage. (Contrat
		//    ecrit dans echanges/sculpt.questions.md, SCULPT-2.)
		struct NkSculptPoint {
				NkVec3f pos = {0.f, 0.f, 0.f};
				NkVec3f normal = {0.f, 1.f, 0.f}; ///< de quel cote de la surface on etait
				float32 radius = 0.25f;			  ///< unites monde ; 0 => celui de la brosse
				float32 pressure = 1.f;			  ///< [0..1]
		};

		// Ce qu'un trait a fait. ⚠️ `vertsMoved` seul ne prouve RIEN : « le
		// maillage a change » est satisfait par n'importe quel defaut. C'est
		// `volumeBefore/After` qui dit la DIRECTION.
		//
		// ⚠️ CE COMPTE RENDU NE PORTE PAS LE NEGATIF « aucun sommet hors rayon
		//    n'a bouge », ET C'EST VOLONTAIRE. Ce module n'ecrit QUE dans les
		//    groupes qu'il a retenus : un tel champ vaudrait zero PAR
		//    CONSTRUCTION, y compris si le rayon etait mal calcule. Un critere
		//    que le defaut cherche ne peut pas faire rougir ne mesure rien -- le
		//    depot l'a paye trois fois (« 6 parties, somme de faces exacte »
		//    satisfait par n'importe quel decoupage ; un alignement a 0,908 quand
		//    le hasard vaut 0,900 ; une fidelite mesuree sur des sommets qui sont
		//    sur la surface par construction).
		//    Le negatif se mesure DEHORS, en comparant une copie independante des
		//    positions d'avant -- c'est ce que fait le banc.
		struct NkSculptApply {
				uint32 groupsInRadius = 0;	 ///< groupes de sommets sous le rayon
				uint32 vertsMoved = 0;		 ///< sommets reellement deplaces
				float32 volumeBefore = 0.f;
				float32 volumeAfter = 0.f;
				float32 maxDisplacement = 0.f;
				bool applied = false; ///< false => AUCUN octet du maillage n'a ete ecrit
		};

		// Volume signe du maillage (produit mixte, eventail par face).
		// ⚠️ C'EST LE CRITERE DE DIRECTION. « Le maillage a change » ne distingue
		//    pas un RAISE d'un LOWER ni d'un bug : une version faite au hasard le
		//    satisferait une fois sur deux. Le volume signe, lui, a un SENS.
		float32 NkSculptSignedVolume(const NkEditMesh &mesh) noexcept;

		// Applique un trait. Rend le compte rendu ci-dessus.
		//
		// ⚠️ LE ZERO N'ECRIT RIEN DU TOUT. Force nulle, trait vide, ou trait hors
		//    du maillage : la fonction sort avant d'avoir touche un seul octet --
		//    y compris les NORMALES. Recalculer les normales « au cas ou » suffirait
		//    a faire echouer une comparaison au bit, et le critere du zero serait
		//    perdu pour une raison qui n'a rien a voir avec la sculpture.
		// ── LA SYMETRIE : ELLE APPARTIENT AU MODE, PAS A LA BROSSE ─────────────
		// Blender : la symetrie est un reglage du MODE Sculpture (X, Y, Z
		// independants), pas une propriete de la brosse. Le meme pinceau sculpte
		// en miroir ou non selon un interrupteur qui n'est pas dans son fichier --
		// et c'est juste : « creuser » ne change pas de nature parce qu'on
		// travaille un visage plutot qu'une coquille.
		//
		// CE QUE CETTE FONCTION FAIT : elle DEPLIE le trait. Chaque tampon donne
		// naissance a ses images miroir (jusqu'a huit avec les trois axes), et
		// c'est le trait deplie qui part a `NkSculptApplyStroke`. Rien d'autre ne
		// change : une seule primitive, un seul chemin de deformation.
		//
		// ⚠️ LA COUTURE, ET C'EST LA QUE LE PRECEDENT S'EST FAIT PRENDRE. Un
		//    tampon POSE SUR LE PLAN est son propre miroir : le dupliquer y
		//    appliquerait la brosse DEUX FOIS, et la couture se creuserait deux
		//    fois plus que ses voisins. Le meme defaut, dans sa version geometrique,
		//    a ete mesure sur l'outil Transform le 25/09 (somme 34 au lieu de 0).
		//    Une image miroir qui retombe sur un tampon deja present est donc
		//    ECARTEE -- et c'est un critere du banc, pas une precaution silencieuse.
		//
		// Le miroir est pris autour de l'ORIGINE DE L'OBJET (le trait est deja en
		// repere objet), comme dans Blender.
		// Rend le nombre de tampons deplies (>= count si au moins un axe est actif).
		uint32 NkSculptExpandSymmetry(const NkSculptPoint *in, uint32 count, uint8 symX, uint8 symY,
									  uint8 symZ, NkVector<NkSculptPoint> &out) noexcept;

		// ── LA MUTATION DU MASQUE, ET ELLE N'A QU'UNE SOURCE ────────────────
		// `NK_MASQUE_IGNORE=1` rend l'etat d'AVANT le masque : les brosses ET
		// l'outil Transform de sculpture ignorent les poids. Les criteres de
		// protection des bancs doivent alors rougir.
		// ⚠️ UNE SEULE LECTURE POUR LES DEUX SITES. Deux `getenv` dans deux unites
		//    de compilation seraient deux compteurs sans code commun : ils
		//    pourraient diverger (une faute de frappe dans l'un), et la mutation
		//    n'en couvrirait plus qu'un -- en le laissant croire.
		bool NkSculptMasqueIgnore() noexcept;

		NkSculptApply NkSculptApplyStroke(NkEditMesh &mesh, const NkBrushDesc &brush,
										  const NkSculptPoint *points, uint32 count) noexcept;

	} // namespace renderer
} // namespace nkentseu
