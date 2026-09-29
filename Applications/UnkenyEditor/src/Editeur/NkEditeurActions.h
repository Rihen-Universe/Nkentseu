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
