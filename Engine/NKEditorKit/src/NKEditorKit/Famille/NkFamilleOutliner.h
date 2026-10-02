#pragma once
// -----------------------------------------------------------------------------
// @File    NkFamilleOutliner.h
// @Brief   L'OUTLINER d'Unreal 5 de la famille : en-tete « Outliner » et
//          « + Entité », recherche, colonnes Nom | Type, l'ARBRE DU KIT avec ses
//          colonnes oeil et cadenas, renommage en place (F2, clic lent), glisser
//          pour rattacher, pied « N entités (M sél.) ».
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// D'OU CA VIENT : UnkenyEditor, NkEditeurOutliner.cpp (le panneau ET son
// peintre d'icones, NkPeintreOutliner). Le panneau ne connait ni scene ni ECS :
// l'application remplit `arbre.nodes` a chaque trame (la racine d'abord, puis
// les noeuds en ordre PREFIXE, un parent avant sa descendance), pose
// `arbre.active` / `arbre.chosen` d'apres SA selection, et recoit les gestes
// par des rappels.
//
// L'ICONE D'UN NOEUD porte sa NATURE et ses quatre drapeaux (oeil ferme, cadenas
// ferme, et leurs heritages d'un parent) : NkFamilleIconeNoeud. `locked` du
// noeud n'est JAMAIS pose -- pour le kit il rend la ligne inselectionnable, alors
// que le cadenas d'un editeur ne fige que la VUE (lecon d'UnkenyEditor).
// -----------------------------------------------------------------------------

#include "NKEditorKit/Famille/NkFamilleStyle.h"
#include "NKEditorKit/Components/NkComponentInstance.h"
#include "NKEditorKit/Components/NkTreeViewModel.h"

namespace nkentseu {
	namespace editorkit {

		/// La nature d'une ligne, en une forme : losange (entite nue), carre
		/// (rigide), bande (decor), bulle (matiere molle), soleil (lumiere),
		/// losange plein (emetteur), camera, cube (maillage), haut-parleur (son).
		enum class NkFamilleNature : uint16 {
			Entite = 0,
			Rigide,
			Decor,
			Mou,
			Lumiere,
			Emetteur,
			Camera,
			Maillage,
			Son,
			Count
		};

		/// L'icone d'un noeud de l'Outliner (a poser dans `NkTreeNode::icon`).
		uint16 NkFamilleIconeNoeud(NkFamilleNature nature, bool cache, bool verrou, bool cacheHerite,
								   bool verrouHerite) noexcept;

		/// L'identifiant reserve a la racine (« Scène »).
		constexpr nk_uint64 kNkFamilleRacine = 1u;

		/// L'etat du panneau, a garder d'une trame a l'autre.
		struct NkFamilleOutliner {
				NkTreeViewModel arbre;
				NkComponentInstance reglages;
				bool pret = false;
				bool filtreFocus = false;
				/// Le glisser d'une ligne (rattacher).
				bool appui = false;
				bool glisse = false;
				nkgui::NkVec2 depart{0.f, 0.f};
				/// Le clic LENT (UE5) : un clic sur le nom d'une ligne DEJA choisie
				/// arme le renommage ; il part si rien ne l'annule dans la demi-seconde.
				nk_uint64 clicLentNoeud = 0;
				float32 clicLentAge = 0.f;
				nkgui::NkVec2 clicLentPos{0.f, 0.f};
				/// « Renommer » (F2, menus) : la saisie s'ouvre a la trame suivante,
				/// AVANT le dessin, sur la ligne active.
				bool renommerDemande = false;
				/// Rien n'est tape dans l'Outliner (les raccourcis de l'editeur passent).
				bool Libre() const noexcept {
					return !filtreFocus && arbre.renaming == 0;
				}
		};

		/// Les gestes, rapportes a l'application (`index` = indice dans `arbre.nodes` ;
		/// 0 = la racine ou le vide).
		struct NkFamilleOutlinerRappels {
				void *user = nullptr;
				/// Double-clic : cadrer la ligne (UE5).
				void (*activer)(void *user, int32 index) = nullptr;
				/// L'oeil (`oeil` vrai) ou le cadenas d'une ligne a ete clique.
				void (*drapeau)(void *user, int32 index, bool oeil) = nullptr;
				void (*renommer)(void *user, int32 index, const char *nom) = nullptr;
				/// Clic droit : la ligne (ou 0) et le point de l'ecran.
				void (*menu)(void *user, int32 index, float32 x, float32 y) = nullptr;
				/// « + Entité ».
				void (*nouvelle)(void *user) = nullptr;
				/// Une ligne lachee sur une autre (0 : la racine ou le vide = detacher).
				void (*rattacher)(void *user, int32 source, int32 cible) = nullptr;
		};

		struct NkFamilleOutlinerResultat {
				bool selectionChangee = false;
				int32 actif = -1; ///< indice de la ligne active (0 = la racine : rien)
				bool aLaSouris = false;
		};

		/// Le panneau dans `zone`. `pied` : « 7 entités (1 sél.) ».
		NkFamilleOutlinerResultat NkFamilleDessinerOutliner(NkFamilleCtx &c, const nkgui::NkRect &zone,
															NkFamilleOutliner &o, const NkFamilleOutlinerRappels &rappels,
															const char *pied, float32 dt, const char *titre = "Outliner",
															const char *plus = "+ Entité");

	} // namespace editorkit
} // namespace nkentseu
