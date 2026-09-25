#pragma once
// -----------------------------------------------------------------------------
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @File    Kernel/Runtime/NKRenderer/src/NKRenderer/Mesh/NkMeshFamilleFichier.h
// @License Proprietary - All Rights Reserved (see LICENSE)
// -----------------------------------------------------------------------------
// UNE FAMILLE EST UN FICHIER (Q16), ET LE C++ NE GARDE QUE L'INTERPRETE.
//
// Rodolf : « est-ce que les familles peuvent dynamiquement etre mises a jour ? »
// Aujourd'hui non : `NkMeshFamilles` est du C++ compile, et ajouter une famille
// demande une reconstruction. La famille PERSONNAGE est la plus grosse a ecrire :
// l'ecrire en C++ serait la refaire.
//
// ⚠️ DEUX LISTES SEPAREES, ET C'EST LA LECTURE DES PLANCHES QUI L'IMPOSE. Sur les
//    24 planches de block-out deposees par Rodolf, le decoupage suit
//    l'ANATOMIE et non le SQUELETTE : le deltoide est separe du biceps, le
//    fessier de la cuisse, le pectoral de l'abdomen -- les coutures ne tombent
//    PAS aux articulations. Une famille porte donc `piece` (ce qu'on voit, ce
//    qu'on selectionne, ce qui a une matiere) ET `liaison` (ce qui tourne),
//    et rien n'oblige les deux listes a coincider.
//
// ⚠️ UNE MATIERE PAR PIECE, pas par objet : sur ces memes planches, la COULEUR
//    EST la separation des parties. C'est un moyen de communication, pas un
//    agrement.
//
// ── LE FORMAT, en une page ────────────────────────────────────────────────────
//   famille table
//   synonymes table, bureau, desk
//   formulaire largeur <metres> profondeur <metres> hauteur <metres>
//   borne largeur 0.4 4.0 1.6          # min max defaut (bornes de CONSTRUCTION)
//   plausible hauteur 0.72 0.78 0.75   # bornes de VRAISEMBLANCE (autre chose)
//   valeurs style simple|chinois       # la premiere est le defaut
//   var e = 0.035
//   piece plateau matiere bois
//     pave  -largeur*0.5  hauteur-e  -profondeur*0.5  largeur*0.5  hauteur  profondeur*0.5
//     si detaille
//       chanfrein 0 1 0 0.01 2
//     fin
//   fin
//   pour x z nom dans (-lx, -lz, pied_ag) (lx, -lz, pied_ad)
//     tournee nom matiere bois en x 0 z profil 0.02 0 0.022 0.03
//   fin
//   liaison battant sur montant_droit charniere pivot xGond 0 0 axe 0 1 0 butees 0 110
//
// ⚠️ ET LE FICHIER PEUT DIRE « JE RESTE EN C++ » : `implementation cpp` suivi de
//    `pourquoi <texte>` et `manque <texte>`. La condition de retrait est alors
//    ECRITE dans le fichier, au lieu d'etre un silence. Une famille que
//    l'interprete ne couvre pas ne disparait pas : elle declare ce qui lui
//    manque, et son formulaire est quand meme lu ici.
// -----------------------------------------------------------------------------
#include "NKRenderer/Mesh/NkMeshFamilles.h"

namespace nkentseu {
	namespace renderer {

		struct NkFamilleParams;
		struct NkFamillePiece;

		/// Charge (ou recharge) toutes les familles d'un dossier. Rend le nombre de
		/// fichiers LUS SANS ERREUR ; `pourquoi` recoit le premier refus nomme.
		/// ⚠️ RECHARGEMENT A CHAUD : appeler a nouveau remplace la table. Un fichier
		///    invalide ne remplace RIEN -- on garde la version qui marchait, et on le
		///    dit. Perdre une famille qui fonctionnait parce qu'on vient de mal taper
		///    une ligne serait le pire des deux mondes.
		int32 NkFamilleFichierCharger(const char *dossier, char *pourquoi, uint32 capPourquoi);

		/// Le dossier charge en dernier (pour le rechargement a chaud sans argument).
		const char *NkFamilleFichierDossier();

		/// Une famille vient-elle d'un fichier ? (faux = elle est encore en C++)
		bool NkFamilleFichierConnue(const char *famille);

		/// Le formulaire LU DANS LE FICHIER. Faux si la famille n'y est pas.
		bool NkFamilleFichierFormulaire(const char *famille, char *out, uint32 cap);

		/// Construit depuis le fichier. Rend le nombre de pieces, -1 et `pourquoi`.
		int32 NkFamilleFichierConstruire(const NkFamilleParams &p, NkVector<NkFamillePiece> &out, char *pourquoi,
										 uint32 capPourquoi);

		/// Le i-eme nom de famille lue dans un fichier, ou nullptr au-dela.
		const char *NkFamilleFichierNom(int32 i);

	} // namespace renderer
} // namespace nkentseu
