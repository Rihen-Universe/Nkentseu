//
// NkEditeurPagesAnim.h
// =============================================================================
// Description :
//   LES PAGES ANIMATION ET ANIMATEUR (2026-10-01, feuille de route R21, R22,
//   R35). Rihen : « on doit avoir la page Animation ou l'on met les keyframes
//   et tout, et la page Animateur ou l'on fait les relations entre les
//   animations ». Elles s'ouvrent comme des ONGLETS DE DOCUMENT, a cote de
//   l'onglet de la scene ; l'onglet choisi occupe le corps de l'editeur.
//
//   - ANIMATION (NkEditeurPageAnimation.cpp) : la FRISE du kit
//     (NKEditorKit/Components/NkTimelineModel.h) sur les proprietes d'une
//     entite et de ses descendants -- position, rotation, echelle, couleur,
//     image du sprite, lumiere, et TOUTE propriete decrite ou reflechie
//     (Unkeny/Anim/NkUnkenyProprietes.h). Le document est un `.nkanim` (le
//     clip de NKAnima, pistes de proprietes en v4) ; l'apercu montre l'entite
//     au curseur, et la rend telle quelle quand on le quitte.
//   - ANIMATEUR (NkEditeurPageAnimateur.cpp) : le GRAPHE D'ETATS du kit
//     (NkStateGraphModel.h) sur un `.nkanimctl` (la HFSM de NKAnima) : etats
//     crees depuis les animations, transitions, conditions, parametres ; en
//     Jouer, l'etat actif de l'entite observee s'allume et ses parametres se
//     reglent en direct.
//
// Ce qui vit ICI et nulle part ailleurs : l'etat des documents ouverts
// (NkPagesAnim, un membre de NkEditeurInterface), et les CONVERSIONS
// frise <-> clip, graphe <-> machine. Les fichiers partages de l'editeur n'en
// portent que des appels d'une ligne.
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================

#pragma once

#ifndef __NKENTSEU_UNKENYEDITOR_NKEDITEURPAGESANIM_H__
#define __NKENTSEU_UNKENYEDITOR_NKEDITEURPAGESANIM_H__

#include "NKAnima/Clip/NkAnimation.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/String/NkString.h"
#include "NKEditorKit/Components/NkStateGraphModel.h"
#include "NKEditorKit/Components/NkTimelineModel.h"
#include "NKGui/Core/NkGuiTypes.h"
#include "Unkeny/Anim/NkUnkenyProprietes.h"

namespace nkentseu {
	namespace editeur {

		struct NkEditeurCadre;
		struct NkEditeurInterface;
		struct NkEditeurModele;

		enum class NkGenreDocAnim : uint8 {
			NK_ANIMATION = 0, ///< un clip de proprietes (.nkanim)
			NK_ANIMATEUR	  ///< un controleur (.nkanimctl)
		};

		/// La valeur d'une propriete AVANT l'apercu : elle est rendue a l'entite
		/// quand l'apercu cesse (onglet quitte, ferme, Jouer).
		struct NkValeurRetenue {
				NkString cible;
				NkString propriete;
				math::NkVec4f valeur{0.f, 0.f, 0.f, 0.f};
		};

		/// Une proposition du choix « + Propriete » : une propriete d'un objet.
		struct NkProposition {
				NkString cible;	 ///< chemin depuis l'entite animee (« » = elle)
				NkString objet;	 ///< son nom, pour l'en-tete
				unkeny::NkProprieteAnimable propriete;
				nkgui::NkRect rect{0.f, 0.f, 0.f, 0.f}; ///< a l'ecran (le banc y vise)
				/// (01/10 soir) Une PISTE DE CLIPS (NLA) plutot qu'une propriete...
				bool pisteClips = false;
				/// ...ou, dans le choix d'un clip a poser, le clip propose (et sa duree).
				NkString clip;
				float32 duree = 1.f;
				/// L'en-tete du groupe (vide = celui de l'objet).
				NkString entete;
		};

		/// Un document ouvert. Les deux genres partagent la forme (et l'onglet).
		struct NkDocAnim {
				nk_uint64 id = 0;
				NkGenreDocAnim genre = NkGenreDocAnim::NK_ANIMATION;
				NkString chemin; ///< ABSOLU ; vide = pas encore enregistre
				NkString nom;	 ///< celui du clip / du modele (le nom du fichier sans extension)
				/// L'entite animee (Animation) ou observee (Animateur), par son IDENTITE
				/// (NkIdentite2D) : Jouer puis Arreter changent les poignees, pas elle.
				uint64 cibleUid = 0;
				uint32 revisionEnregistree = 0;
				bool modifie = false;

				// --- Animation -------------------------------------------------------
				editorkit::NkTimelineModel frise;
				/// Le clip que la frise decrit (pistes d'os et autres GARDEES : on ne
				/// remplace que les pistes de proprietes).
				anim::NkAnimationClip clip;
				uint32 revisionClip = 0xFFFFFFFFu;
				bool apercu = true; ///< l'entite montre la frise au curseur
				NkVector<NkValeurRetenue> retenues;
				bool choix = false; ///< le choix « + Propriete » est ouvert
				NkVector<NkProposition> propositions;
				nkgui::NkRect choixRect{0.f, 0.f, 0.f, 0.f};
				float32 choixDefil = 0.f;
				/// (01/10 soir) Le choix ouvert est celui d'un CLIP a poser sur la piste
				/// de clips `choixPiste`, a `choixTemps`.
				bool choixClips = false;
				nk_uint64 choixPiste = 0;
				float32 choixTemps = 0.f;

				// --- Animateur -------------------------------------------------------
				editorkit::NkStateGraphModel graphe;
				/// L'etat i de la machine COMPILEE (ou relue) -> le noeud du graphe.
				NkVector<nk_uint64> noeudDeEtat;
				int32 etatVu = -1; ///< le dernier etat vu en Jouer (pour le tir)
				bool renomme = false;
				nk_uint64 renommeNoeud = 0;
				int32 renommeParam = -1;
				char tampon[64] = {};
				bool renommeChoisir = false; ///< tout le nom choisi a la premiere trame du champ
				nkgui::NkRect renommeRect{0.f, 0.f, 0.f, 0.f};
				bool grapheCadre = false; ///< la vue a ete cadree une fois
		};

		/// L'etat des pages : un membre de NkEditeurInterface.
		struct NkPagesAnim {
				NkVector<NkDocAnim> docs;
				nk_uint64 actif = 0; ///< 0 = la scene
				nk_uint64 prochainId = 1;
				/// Releves a la derniere trame (le banc y vise) : un rectangle par
				/// document, dans l'ordre de `docs`, et leur croix.
				NkVector<nkgui::NkRect> onglets;
				NkVector<nkgui::NkRect> croix;
				nkgui::NkRect page{0.f, 0.f, 0.f, 0.f};
				nkgui::NkRect apercu{0.f, 0.f, 0.f, 0.f};
				/// Les boutons de l'en-tete de la page (NkBoutonPage).
				nkgui::NkRect boutons[8] = {};
				/// Les derniers resultats des composants du kit (le banc y lit la
				/// geometrie de la frise et du graphe).
				editorkit::NkTimelineResult frise;
				editorkit::NkStateGraphResult graphe;
		};

		/// Les boutons de l'en-tete d'une page.
		enum class NkBoutonPage : uint8 {
			NK_ENREGISTRER = 0,
			NK_SELECTION,  ///< « Utiliser la selection » : l'entite animee / observee
			NK_APERCU,	   ///< Animation : l'apercu, allume / eteint
			NK_ATTACHER,   ///< « Jouer en jeu » : le clip (ou le controleur) pose sur l'entite
			NK_JOUER,	   ///< Animateur : Jouer / Arreter la scene
			NK_DUREE_MOINS,
			NK_DUREE_PLUS,
			NK_FPS
		};

		// --- Ouvrir, fermer ------------------------------------------------------
		/// Ouvre (ou montre, s'il l'est deja) un CONTROLEUR d'animation (.nkanimctl,
		/// chemin ABSOLU) dans la page Animateur. Chemin vide : un controleur neuf.
		/// L'entite observee est la selection, s'il y en a une. Faux si le fichier
		/// ne se lit pas (annonce au journal).
		bool NkEditeurOuvrirAnimateur(NkEditeurModele &m, NkEditeurInterface &ui, const char *chemin);
		bool NkEditeurOuvrirAnimateur(NkEditeurCadre &c, const char *chemin);
		/// Ouvre un CLIP (.nkanim) dans la page Animation ; l'entite animee est la
		/// selection. Chemin vide : un clip neuf pour la selection.
		bool NkEditeurOuvrirAnimation(NkEditeurModele &m, NkEditeurInterface &ui, const char *chemin);
		bool NkEditeurOuvrirAnimation(NkEditeurCadre &c, const char *chemin);
		/// Ferme un document (l'apercu rend l'entite). Faux s'il n'existe pas.
		bool NkEditeurFermerDocAnim(NkEditeurModele &m, NkEditeurInterface &ui, nk_uint64 id);
		/// Le document au premier plan, ou nul (la scene).
		NkDocAnim *NkEditeurDocAnimActif(NkEditeurInterface &ui);
		bool NkEditeurPageAnimOuverte(const NkEditeurInterface &ui) noexcept;
		/// Enregistre le document (choisit un chemin dans Contenu/Animations s'il
		/// n'en a pas). Rend vrai si le fichier est ecrit.
		bool NkEditeurEnregistrerDocAnim(NkEditeurModele &m, NkDocAnim &d);

		// --- Les appels des fichiers partages (une ligne chacun) ---------------
		/// Les onglets des documents, a droite de celui de la scene
		/// (NkEditeurDessinerOnglets) ; un clic sur l'onglet de la scene la ramene.
		void NkEditeurDessinerOngletsAnim(NkEditeurCadre &c);
		/// La page du document actif dans le corps (NkEditeurDessinerTrame). Faux
		/// si c'est la scene : le corps habituel se dessine.
		bool NkEditeurDessinerPageAnim(NkEditeurCadre &c);
		/// Le clavier quand une page est au premier plan (NkEditeurRaccourcis).
		/// Vrai = la touche est prise (la scene ne la voit pas).
		bool NkEditeurPageAnimAuClavier(NkEditeurCadre &c);
		/// Les actions NK_A_ANIM_* (NkEditeurExecuter). Faux si `action` n'en est pas une.
		bool NkEditeurActionAnim(NkEditeurCadre &c, int32 action);

		// --- Les pages (une par fichier) ----------------------------------------
		void NkEditeurDessinerPageAnimation(NkEditeurCadre &c, NkDocAnim &d, const nkgui::NkRect &zone);
		void NkEditeurDessinerPageAnimateur(NkEditeurCadre &c, NkDocAnim &d, const nkgui::NkRect &zone);
		/// L'apercu d'un document Animation cesse : l'entite reprend ses valeurs.
		void NkEditeurRendreApercu(NkEditeurModele &m, NkDocAnim &d);
		/// La trame d'un document qui n'est PAS au premier plan (apercu rendu,
		/// controleur suivi en Jouer).
		void NkEditeurEntretenirDocAnim(NkEditeurCadre &c, NkDocAnim &d, bool auPremierPlan);
		/// L'Animateur suit l'entite observee EN JEU : etat actif, fondu, derniere
		/// transition tiree, valeurs des parametres (NkEditeurPageAnimateur.cpp).
		void NkEditeurSuivreAnimateur(NkEditeurCadre &c, NkDocAnim &d);
		/// L'entite du document (invalide si elle n'existe plus).
		ecs::NkEntityId NkEditeurCibleDoc(NkEditeurModele &m, const NkDocAnim &d);
		/// L'apercu de l'entite (le rendu du jeu, cadre sur elle) dans `r`.
		void NkEditeurDessinerApercuAnim(NkEditeurCadre &c, ecs::NkEntityId cible, const nkgui::NkRect &r);

		// --- Les conversions (testees par le banc) ------------------------------
		/// La frise d'un clip : une piste par piste de propriete.
		void NkFriseDepuisClip(const anim::NkAnimationClip &clip, editorkit::NkTimelineModel &frise);
		/// Les pistes de proprietes du clip depuis la frise (les autres pistes du
		/// clip restent), et sa duree, ses images par seconde, sa boucle.
		void NkClipDepuisFrise(const editorkit::NkTimelineModel &frise, anim::NkAnimationClip &clip);
		/// Le graphe d'une machine (`noeudDeEtat` : etat -> noeud).
		void NkGrapheDepuisMachine(const anim::NkAnimStateMachine &m, editorkit::NkStateGraphModel &g,
								   NkVector<nk_uint64> &noeudDeEtat);
		/// La machine d'un graphe (une machine NEUVE). Faux si le graphe n'a aucun etat.
		bool NkMachineDepuisGraphe(const editorkit::NkStateGraphModel &g, anim::NkAnimStateMachine &m,
								   NkVector<nk_uint64> &noeudDeEtat);
		/// Les animations proposees au graphe : les .nkanim du Contenu (leur nom).
		void NkEditeurAnimationsDuContenu(NkEditeurModele &m, NkVector<NkString> &noms);

		// --- Banc et captures (NkEditeurBancAnimation.cpp) ----------------------
		int32 NkEditeurLancerBancAnimation();
		/// Des captures HORS ECRAN des deux pages dans `dossier` (PNG).
		int32 NkEditeurCapturesAnimation(const char *dossier);

	} // namespace editeur
} // namespace nkentseu

#endif // __NKENTSEU_UNKENYEDITOR_NKEDITEURPAGESANIM_H__
