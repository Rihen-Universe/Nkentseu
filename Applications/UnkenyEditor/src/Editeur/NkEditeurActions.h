// =============================================================================
// NkEditeurActions.h — ce que l'editeur FAIT, sans interface
//
// A QUOI SERT CE FICHIER
//   Chaque geste de l'editeur (poser un acteur, choisir sous le curseur,
//   deplacer, supprimer, Jouer / Arreter, enregistrer) est une fonction du
//   MODELE, pas d'un panneau. Les panneaux ne font qu'appeler.
//
// POURQUOI — la meme raison que les bancs d'Unkeny
//   Une action ecrite dans un OnUI ne s'eprouve qu'avec une fenetre, une souris
//   et quelqu'un qui regarde. Ecrite ici, elle s'eprouve dans `--selftest`,
//   sans rien de tout cela (NkEditeurLancerBanc, plus bas).
//
// AUTEUR: Rihen
// LICENCE: Proprietary - All Rights Reserved (see LICENSE)
// =============================================================================
#pragma once

#include "Editeur/NkEditeurModele.h"

namespace nkentseu {
	namespace editeur {

		/// Vide la scene et pose l'exemple : sol, caisses, blob, eau, tissu. Un
		/// editeur qui s'ouvre sur le vide ne prouve rien du moteur.
		void NkEditeurNouvelleScene(NkEditeurModele &m);

		// --- Jouer / Pause / Arreter (les etats d'UE5) -----------------------
		void NkEditeurJouer(NkEditeurModele &m);
		void NkEditeurPause(NkEditeurModele &m);
		void NkEditeurArreter(NkEditeurModele &m); ///< rend la scene d'avant Jouer
		void NkEditeurUnPas(NkEditeurModele &m);
		/// Une trame : la scene avance si l'on JOUE ; l'annonce vieillit.
		void NkEditeurAvancer(NkEditeurModele &m, float32 dt);

		// --- Gestes dans la scene -------------------------------------------
		/// Pose l'acteur choisi (ou l'entite simple) en `monde`. Selectionne ce
		/// qu'il a pose.
		ecs::NkEntityId NkEditeurPoser(NkEditeurModele &m, const NkVec2f &monde);
		/// Choisit ce qui est sous `monde` : d'abord la matiere (corps mous),
		/// puis les sprites, puis les formes sans sprite. `centre` recoit le
		/// point de reference de ce qui est choisi (pour le glisser sans saut).
		bool NkEditeurChoisirSous(NkEditeurModele &m, const NkVec2f &monde, NkVec2f *centre = nullptr);
		/// Amene la selection a `centre` : un rigide est TELEPORTE (le solveur
		/// suit), un corps mou est TRANSLATE particule par particule.
		void NkEditeurDeplacer(NkEditeurModele &m, const NkVec2f &centre);
		/// Le centre courant de la selection (transform, ou centre de matiere).
		bool NkEditeurCentreSelection(const NkEditeurModele &m, NkVec2f &centre);
		/// La boite, en metres, de la selection — pour son cadre dans le viseur.
		bool NkEditeurBoiteSelection(NkEditeurModele &m, NkVec2f &mn, NkVec2f &mx);
		void NkEditeurSupprimerSelection(NkEditeurModele &m);
		/// Le clic de l'outil Selection (2026-09-29) : choisit ce qui est sous
		/// `monde` comme NkEditeurChoisirSous, et VIDE la selection s'il n'y a
		/// rien -- cliquer dans le vide deselectionne, comme partout.
		bool NkEditeurCliquerSelection(NkEditeurModele &m, const NkVec2f &monde, NkVec2f *centre = nullptr);
		/// Le glisser d'une entite la deplace-t-il ? En EDITION seulement.
		/// ⚠️ PAS EN JEU NI EN PAUSE, et c'est une decision : la scene qui tourne
		///    est un instant de simulation, que « Arreter » jette de toute facon.
		///    Deplacer la y ferait croire a une modification qui disparaitra, et
		///    teleporter un corps contre le solveur fausse la simulation qu'on
		///    regarde. Pour attraper la matiere en jeu : l'outil Saisir.
		bool NkEditeurPeutDeplacer(const NkEditeurModele &m) noexcept;
		/// Centre la vue sur la selection, et la fait tenir dans le viseur. false
		/// sans selection (rien ne bouge).
		bool NkEditeurCadrerSelection(NkEditeurModele &m);
		/// La zone a cadrer : la boite de la selection, ou celle de TOUTE la
		/// scene quand rien n'est selectionne. `taille` porte deja sa marge.
		/// false si la scene est vide. C'est « F » (et le double-clic de
		/// l'Outliner) ; l'interface y mene la camera en douceur.
		bool NkEditeurZoneACadrer(NkEditeurModele &m, NkVec2f &centre, NkVec2f &taille);

		// --- Les gizmos (deplacer / tourner / mettre a l'echelle) ------------
		/// Tourne la selection de `delta` radians autour de son centre. Rigide :
		/// son corps prend l'orientation (ActualiserCorps) ; matiere : chaque
		/// particule tourne, vitesse comprise.
		bool NkEditeurTourner(NkEditeurModele &m, float32 delta);
		/// Multiplie les dimensions de la selection par `facteur` autour de son
		/// centre : sprite, collisionneur (le corps est refait), ou matiere
		/// (positions ET longueurs de repos des liens -- sans elles, le ressort
		/// ramenerait aussitot la matiere a sa taille d'avant).
		/// ⚠️ L'ECHELLE EST CUITE DANS LES COMPOSANTS, pas posee dans
		///    `NkTransform2D::echelle` : la physique ne lit pas cette echelle (le
		///    collisionneur a ses propres dimensions), et un sprite agrandi sur
		///    une boite de collision inchangee mentirait sur ce qui se touche.
		bool NkEditeurMettreAEchelle(NkEditeurModele &m, const NkVec2f &facteur);
		/// L'accrochage : `v` ramene au multiple de `pas` le plus proche.
		/// `pas` <= 0 : `v` inchange.
		float32 NkEditeurAccrocher(float32 v, float32 pas) noexcept;
		bool NkEditeurEffacerSous(NkEditeurModele &m, const NkVec2f &monde);

		// --- Entites et composants (la facon d'UE5 / Unity) ------------------
		/// Une entite VIDE : un transform et un nom, rien d'autre. On lui ajoute
		/// ensuite ses composants. Selectionnee.
		ecs::NkEntityId NkEditeurCreerEntite(NkEditeurModele &m, const char *nom, const NkVec2f &position);
		bool NkEditeurRenommer(NkEditeurModele &m, ecs::NkEntityId id, const char *nom);
		/// Copie une entite et ses composants, decalee de 0,5 m. La MATIERE d'un
		/// corps mou n'est pas recopiee (rend Invalid et l'annonce).
		ecs::NkEntityId NkEditeurDupliquer(NkEditeurModele &m);

		enum class NkComposantEditeur : uint8 {
			NK_SPRITE = 0,
			NK_COLLISIONNEUR,
			NK_CORPS,	  ///< corps RIGIDE (ajoute un collisionneur s'il manque)
			NK_CORPS_MOU, ///< matiere (le materiau est choisi a l'ajout)
			NK_SOURCE,	  ///< source sonore
			NK_ANIMATION,
			// 2026-09-30, AJOUTES A LA FIN : les valeurs d'avant ne bougent pas.
			NK_LUMIERE,	 ///< lumiere 2D (le type se choisit a l'ajout)
			NK_EMETTEUR, ///< emetteur de particules (le preset se choisit a l'ajout)
			NK_COUNT
		};
		const char *NkComposantEditeurNom(NkComposantEditeur c) noexcept;
		bool NkEditeurAUnComposant(NkEditeurModele &m, ecs::NkEntityId id, NkComposantEditeur c);
		/// Peut-on l'ajouter ? (pas deux fois ; pas un corps rigide ET un corps
		/// mou ; pas de corps mou sans monde de particules).
		bool NkEditeurPeutAjouter(NkEditeurModele &m, ecs::NkEntityId id, NkComposantEditeur c);
		/// `matiere` : pour NK_CORPS_MOU, ce que devient l'entite (blob, eau...).
		bool NkEditeurAjouterComposant(NkEditeurModele &m, ecs::NkEntityId id, NkComposantEditeur c,
									   NkActeurSim matiere = NkActeurSim::NK_BLOB);
		/// Retirer un collisionneur retire aussi le corps rigide qui s'y appuie.
		bool NkEditeurRetirerComposant(NkEditeurModele &m, ecs::NkEntityId id, NkComposantEditeur c);

		/// « mou », « rigide », « decor », « entite » : l'etiquette de type que
		/// montrent la hierarchie et l'inspecteur.
		const char *NkEditeurTypeDe(NkScene &scene, ecs::NkEntityId id);

		// --- Hierarchie et prefabs (2026-09-29) ------------------------------
		/// Rattache `enfant` a `parent` sans qu'il bouge a l'ecran (le glisser de
		/// l'Outliner). Refuse une boucle, et le dit.
		bool NkEditeurRattacher(NkEditeurModele &m, ecs::NkEntityId enfant, ecs::NkEntityId parent);
		/// En fait une racine, a sa place.
		bool NkEditeurDetacher(NkEditeurModele &m, ecs::NkEntityId enfant);
		/// Fait un prefab de la selection (et de sa descendance) et l'ecrit a cote
		/// de la scene, sous « <nom>.nkprefab ». La selection en devient la
		/// premiere instance. Rend l'identifiant du prefab, 0 en cas d'echec.
		uint32 NkEditeurCreerPrefab(NkEditeurModele &m);
		/// Le chemin du .nkprefab de `nom`, dans le dossier de la scene.
		NkString NkEditeurCheminPrefab(NkEditeurModele &m, const char *nom);

		// --- Fichier ---------------------------------------------------------
		const char *NkEditeurChemin(NkEditeurModele &m);
		bool NkEditeurSauver(NkEditeurModele &m);
		bool NkEditeurOuvrir(NkEditeurModele &m);
		void NkEditeurAnnoncer(NkEditeurModele &m, const char *texte);

		/// Le banc de l'editeur (e1..), lance par `--selftest` APRES celui
		/// d'Unkeny. 0 = tout tient.
		int32 NkEditeurLancerBanc();

	} // namespace editeur
} // namespace nkentseu
