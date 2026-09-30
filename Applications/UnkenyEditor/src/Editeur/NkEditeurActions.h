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
		/// Choisit ce qui est sous `monde` (NkEditeurPrendreSous) et le
		/// selectionne. `centre` recoit le point de reference de ce qui est
		/// choisi (pour le glisser sans saut). Rien dessous : plus de selection.
		bool NkEditeurChoisirSous(NkEditeurModele &m, const NkVec2f &monde, NkVec2f *centre = nullptr);
		/// Amene la selection a `centre` : un rigide est TELEPORTE (le solveur
		/// suit), un corps mou est TRANSLATE particule par particule.
		void NkEditeurDeplacer(NkEditeurModele &m, const NkVec2f &centre);
		/// Le centre courant de la selection (transform, ou centre de matiere).
		bool NkEditeurCentreSelection(const NkEditeurModele &m, NkVec2f &centre);
		/// La boite, en metres, de la selection — pour son cadre dans le viseur.
		bool NkEditeurBoiteSelection(NkEditeurModele &m, NkVec2f &mn, NkVec2f &mx);
		void NkEditeurSupprimerSelection(NkEditeurModele &m);
		/// A combien de PIXELS d'un objet un clic le prend encore. Au-dela, un
		/// clic a cote d'un objet est un clic dans le vide (il deselectionne).
		constexpr float32 NK_PRISE_TOLERANCE_PX = 6.f;
		/// Le rayon, en pixels, du marqueur d'une entite SANS visuel (ni sprite,
		/// ni forme, ni matiere) : le viseur le dessine, la prise le vise.
		constexpr float32 NK_MARQUEUR_RAYON_PX = 7.f;
		/// La PRISE au clic dans la vue (2026-09-30) : ce que l'utilisateur VOIT
		/// sous le curseur, a NK_PRISE_TOLERANCE_PX pres (converti en metres par
		/// le zoom de la camera de la scene -- celle du viseur).
		///   1. Un COUP AU BUT (le point est DANS ce qui est dessine) l'emporte,
		///      dans l'ordre du dessin : marqueurs, matiere, sprites (la couche la
		///      plus haute), formes ; a egalite, le plus PETIT (il est pose sur
		///      l'autre, sinon on ne le verrait pas).
		///   2. Sinon, l'objet le PLUS PROCHE dans la tolerance.
		/// Les entites verrouillees, et les cachees en EDITION, ne se prennent pas.
		/// Ne touche PAS a la selection.
		/// ⚠️ POURQUOI ELLE REMPLACE L'ANCIENNE -- mesure du 30/09 (temoin e41).
		///    L'ancienne avait des marges FIXES en metres : 0,25 m autour de
		///    chaque particule, 0,1 m autour des formes, 0 autour des sprites, et
		///    elle testait la matiere AVANT tout le reste. Resultat : a fort zoom,
		///    un clic sur une caisse posee contre un blob prenait le blob (0,25 m,
		///    c'est 30 px a 122 px/m) ; a faible zoom, un clic a 3 px du bord d'une
		///    caisse texturee ne prenait rien ; et le milieu d'un ballon (a plus de
		///    0,25 m de son anneau) ne se prenait pas du tout.
		bool NkEditeurPrendreSous(NkEditeurModele &m, const NkVec2f &monde, ecs::NkEntityId &sortie, NkVec2f *centre = nullptr);
		/// Le clic de l'outil Selection : NkEditeurChoisirSous, et `deplace` est
		/// coupe s'il n'y a rien -- cliquer dans le vide deselectionne, comme partout.
		bool NkEditeurCliquerSelection(NkEditeurModele &m, const NkVec2f &monde, NkVec2f *centre = nullptr);
		/// Une entite sans rien a dessiner : le viseur lui donne un marqueur.
		bool NkEditeurSansVisuel(NkEditeurModele &m, ecs::NkEntityId id);

		// --- L'oeil et le cadenas de l'Outliner (NkDrapeauxEditeur) ----------
		bool NkEditeurEstCache(NkEditeurModele &m, ecs::NkEntityId id);
		bool NkEditeurEstVerrouille(NkEditeurModele &m, ecs::NkEntityId id);
		/// Cachee ET dans l'etat ou l'oeil compte (EDITION) : ni dessinee, ni prise.
		bool NkEditeurCacheDansLaVue(NkEditeurModele &m, ecs::NkEntityId id);
		void NkEditeurCacher(NkEditeurModele &m, ecs::NkEntityId id, bool cache);
		void NkEditeurVerrouiller(NkEditeurModele &m, ecs::NkEntityId id, bool verrou);
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
