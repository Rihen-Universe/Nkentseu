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
		/// Un ANCETRE a-t-il l'oeil ferme / le cadenas ferme ? (La hierarchie :
		/// le drapeau d'un parent vaut pour toute sa descendance, comme dans UE5.)
		bool NkEditeurCacheHerite(NkEditeurModele &m, ecs::NkEntityId id);
		bool NkEditeurVerrouHerite(NkEditeurModele &m, ecs::NkEntityId id);
		/// Cachee (elle ou un ancetre) ET dans l'etat ou l'oeil compte (EDITION) :
		/// ni dessinee, ni prise.
		bool NkEditeurCacheDansLaVue(NkEditeurModele &m, ecs::NkEntityId id);
		/// Verrouillee, elle ou un ancetre : ni prise ni deplacee dans la vue.
		bool NkEditeurVerrouilleDansLaVue(NkEditeurModele &m, ecs::NkEntityId id);
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
		/// L'echelle affichee (NkEchelleEditeur) ; 1 x 1 sans composant.
		NkVec2f NkEditeurEchelle(NkEditeurModele &m, ecs::NkEntityId id);
		/// Amene l'echelle affichee a `facteur` (chaque axe >= 0,05) en CUISANT le
		/// rapport dans les composants, comme le gizmo.
		bool NkEditeurPoserEchelle(NkEditeurModele &m, ecs::NkEntityId id, const NkVec2f &facteur);
		/// L'accrochage : `v` ramene au multiple de `pas` le plus proche.
		/// `pas` <= 0 : `v` inchange.
		float32 NkEditeurAccrocher(float32 v, float32 pas) noexcept;
		bool NkEditeurEffacerSous(NkEditeurModele &m, const NkVec2f &monde);

		// --- Entites et composants (la facon d'UE5 / Unity) ------------------
		/// Une entite VIDE : un transform et un nom, rien d'autre. On lui ajoute
		/// ensuite ses composants. Selectionnee.
		ecs::NkEntityId NkEditeurCreerEntite(NkEditeurModele &m, const char *nom, const NkVec2f &position);
		bool NkEditeurRenommer(NkEditeurModele &m, ecs::NkEntityId id, const char *nom);

		// --- L'historique (2026-10-01, NkHistoriqueEditeur) ----------------------
		/// Photographie la scene AVANT un geste : il devient annulable. Vide
		/// « refaire » (une nouvelle branche). Rien hors EDITION.
		void NkEditeurRetenir(NkEditeurModele &m);
		/// Ctrl+Z : rend la scene d'avant le dernier geste retenu ; la selection
		/// suit par identite (les poignees changent a la restauration).
		bool NkEditeurAnnuler(NkEditeurModele &m);
		/// Ctrl+Y : rend celle que Ctrl+Z a quittee.
		bool NkEditeurRefaire(NkEditeurModele &m);
		/// Oublie l'historique (nouvelle scene, ouverture).
		void NkEditeurOublierHistorique(NkEditeurModele &m);

		// --- L'entite active (2026-10-01, la case de l'en-tete des Details) -------
		/// Allume / eteint l'entite DANS LE JEU (NkScene::Activer, NkUnkenyActif.h) :
		/// elle et sa descendance ne sont plus ni rendues, ni simulees, ni animees,
		/// en edition comme en jeu, et c'est sauve. Retenu : Ctrl+Z l'annule.
		/// ⚠️ CE N'EST PAS L'OEIL DE L'OUTLINER (NkEditeurCacher) : l'oeil ne cache
		///    qu'en EDITION et ne touche pas au jeu ; les deux coexistent.
		bool NkEditeurActiverEntite(NkEditeurModele &m, ecs::NkEntityId id, bool actif);
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

		// --- Les CARTES de l'inspecteur (2026-09-30, a la maniere d'Unity) ------
		/// Une carte des Details : le Transform, chaque composant, l'Animateur et
		/// la Hierarchie. Sprite..Animation SUIVENT NkComposantEditeur (carte - 1) ;
		/// Lumiere et Emetteur, venus apres, passent par NkComposantDeCarte.
		/// AJOUTES A LA FIN : les valeurs existantes ne bougent pas.
		enum class NkCarteEditeur : uint8 {
			NK_TRANSFORM = 0,
			NK_SPRITE,
			NK_COLLISIONNEUR,
			NK_CORPS,
			NK_CORPS_MOU,
			NK_SOURCE,
			NK_ANIMATION,
			NK_ANIMATEUR,
			NK_HIERARCHIE,
			NK_LUMIERE,	 ///< NkComposantEditeur::NK_LUMIERE (2026-09-30, eclairage 2D)
			NK_EMETTEUR, ///< NkComposantEditeur::NK_EMETTEUR (effets)
			NK_COUNT
		};
		/// Le composant d'une carte (false : Transform, Animateur, Hierarchie).
		bool NkComposantDeCarte(NkCarteEditeur c, NkComposantEditeur &sortie) noexcept;
		const char *NkCarteEditeurNom(NkCarteEditeur c) noexcept;
		/// L'entite porte-t-elle ce qu'affiche la carte ? (La hierarchie : toujours.)
		bool NkEditeurAUneCarte(NkEditeurModele &m, ecs::NkEntityId id, NkCarteEditeur c);
		/// « Copier les valeurs » : les octets du composant, et sa carte.
		struct NkPressePapierComposant {
				int32 carte = -1; ///< NkCarteEditeur ; -1 = vide
				uint32 taille = 0;
				uint8 octets[512] = {};
		};
		/// Ce que Reinitialiser, Copier et Coller savent faire : ni la matiere
		/// (ses reglages sont ceux de SON materiau), ni l'animateur (son modele est
		/// un fichier), ni la hierarchie (ce n'est pas un composant).
		bool NkEditeurCarteSeCopie(NkCarteEditeur c) noexcept;
		/// « Reinitialiser » : les valeurs par defaut, SANS toucher a ce qui fait
		/// l'identite du composant -- le corps du solveur, la texture, le son, et la
		/// case « actif » de l'en-tete (Unity ne la remet pas non plus).
		bool NkEditeurReinitialiserCarte(NkEditeurModele &m, ecs::NkEntityId id, NkCarteEditeur c);
		bool NkEditeurCopierCarte(NkEditeurModele &m, ecs::NkEntityId id, NkCarteEditeur c, NkPressePapierComposant &pp);
		/// Colle sur la carte de MEME nature (false sinon). Le corps du solveur et
		/// la voix d'une source restent ceux de l'entite : on colle des VALEURS,
		/// pas une identite -- deux caisses ne partagent pas un corps.
		bool NkEditeurCollerCarte(NkEditeurModele &m, ecs::NkEntityId id, const NkPressePapierComposant &pp);
		/// La case « actif » de l'en-tete d'une carte : les composants qui ont
		/// leur drapeau (sprite et matiere visibles, animation et animateur qui
		/// jouent, lumiere et emetteur actifs) le suivent ; les autres s'eteignent
		/// par NkEteintsEditeur. Le Transform et la Hierarchie ne s'eteignent pas
		/// (ce ne sont pas des composants qu'on retire du jeu).
		bool NkEditeurCarteAUneCase(NkCarteEditeur c) noexcept;
		bool NkEditeurCarteActive(NkEditeurModele &m, ecs::NkEntityId id, NkCarteEditeur c);
		bool NkEditeurActiverCarte(NkEditeurModele &m, ecs::NkEntityId id, NkCarteEditeur c, bool actif);

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
		/// L'appareil simule de la scene (`<scene>.nkappareil`, document 03) :
		/// ecrit par NkEditeurSauver, relu par NkEditeurOuvrir. Charger un
		/// fichier absent rend false et ne change rien.
		bool NkEditeurAppareilEnregistrer(const NkEditeurModele &m, const char *cheminScene);
		bool NkEditeurAppareilCharger(NkEditeurModele &m, const char *cheminScene);
		void NkEditeurAnnoncer(NkEditeurModele &m, const char *texte);

		/// Le banc de l'editeur (e1..), lance par `--selftest` APRES celui
		/// d'Unkeny. 0 = tout tient.
		int32 NkEditeurLancerBanc();

	} // namespace editeur
} // namespace nkentseu
