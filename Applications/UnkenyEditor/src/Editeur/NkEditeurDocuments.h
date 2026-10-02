//
// NkEditeurDocuments.h
// =============================================================================
// Description :
//   LES ONGLETS DE DOCUMENT (2026-10-02) : UNE barre, comme Unreal. Rihen :
//   « des qu'on a ouvert un Blueprint, on ne peut plus ouvrir de scene : je
//   n'arrive plus a acceder a la scene ». Le graphe d'un Blueprint REMPLACAIT
//   la vue, sans onglet ni retour ; les pages Animation / Animateur et les
//   assets avaient chacun LEUR systeme d'onglets, chacun son « actif ».
//
//   Desormais :
//     - la SCENE est toujours le premier onglet ; un clic la ramene ;
//     - a sa droite, dans l'ordre d'ouverture : les pages Animation et
//       Animateur, les assets ouverts (texture, police, son, prefab...) et
//       CHAQUE Blueprint ouvert ;
//     - un clic sur un onglet le met au premier plan ; sa croix (ou le clic
//       du milieu) le ferme -- la scene ne se ferme pas par la barre (sa
//       croix a elle « ferme la scene » : une scene neuve prend sa place) ;
//     - ouvrir deux fois le meme document REACTIVE son onglet.
//
// LE PREMIER PLAN EST UN SEUL ETAT : `NkDocuments::actif`. Les trois systemes
// d'avant (`pagesAnim.actif`, `ongletActif`, « le graphe ouvert ») sont
// retires : chacun lit le premier plan ici (NkEditeurDocAnimActif,
// NkEditeurOngletAssetActif, NkEditeurBlueprintDevant). Les CONTENUS restent
// ou ils etaient (NkPagesAnim::docs, NkEditeurInterface::onglets,
// NkEditeurScripts::graphes) : la barre ne fait que les ranger et choisir.
//
// ⚠️ QUITTER ET ENTRER : passer d'un document a l'autre n'est pas qu'un
//    indice. Quitter un PREFAB rend la scene (NkScene::Restaurer) ; quitter un
//    son le fait taire ; entrer dans un prefab met la scene de cote ; entrer
//    dans un Blueprint en fait le graphe que la page montre et que « Compiler »
//    vise. TOUT passage passe par NkEditeurActiverDocument -- c'est la porte.
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================

#pragma once

#ifndef __NKENTSEU_UNKENYEDITOR_NKEDITEURDOCUMENTS_H__
#define __NKENTSEU_UNKENYEDITOR_NKEDITEURDOCUMENTS_H__

#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/String/NkString.h"
#include "NKGui/Core/NkGuiTypes.h"

namespace nkentseu {
	namespace editeur {

		struct NkEditeurCadre;
		struct NkEditeurInterface;
		struct NkEditeurModele;

		/// Ce qu'un onglet de la barre montre.
		enum class NkGenreDocument : uint8 {
			NK_SCENE = 0, ///< la scene (toujours la ; `cle` n'est pas lue)
			NK_ANIM,	  ///< une page Animation ou Animateur (cle = NkDocAnim::id)
			NK_ASSET,	  ///< un asset ouvert (cle = NkOngletAsset::id)
			NK_BLUEPRINT, ///< la page du graphe d'un Blueprint (cle = NkEditeurGrapheEtat::id)
			NK_MAILLAGE	  ///< (2026-10-02, R31) la fenetre d'edition d'un maillage 2D (cle = NkDocMaillage::id)
		};

		/// Un document de la barre : son genre et son identite DANS son genre.
		struct NkDocumentOuvert {
				NkGenreDocument genre = NkGenreDocument::NK_SCENE;
				nk_uint64 cle = 0;

				bool operator==(const NkDocumentOuvert &o) const noexcept {
					return genre == o.genre && (genre == NkGenreDocument::NK_SCENE || cle == o.cle);
				}
				bool operator!=(const NkDocumentOuvert &o) const noexcept {
					return !(*this == o);
				}
		};

		inline NkDocumentOuvert NkDocScene() noexcept {
			return NkDocumentOuvert{};
		}
		inline NkDocumentOuvert NkDoc(NkGenreDocument genre, nk_uint64 cle) noexcept {
			NkDocumentOuvert d;
			d.genre = genre;
			d.cle = cle;
			return d;
		}

		/// L'etat de la barre : un membre de NkEditeurInterface.
		struct NkDocuments {
				/// Les documents a droite de la scene, dans l'ordre d'ouverture.
				NkVector<NkDocumentOuvert> ordre;
				/// LE premier plan (NK_SCENE : la scene).
				NkDocumentOuvert actif;
				/// L'identite du prochain asset ouvert (NkOngletAsset::id).
				nk_uint64 prochainAsset = 1;
				/// Releves a la derniere trame (les bancs y visent) : un rectangle par
				/// entree de `ordre`, et sa croix.
				NkVector<nkgui::NkRect> rects;
				NkVector<nkgui::NkRect> croix;
				/// Combien d'onglets ont du raccourcir leur nom pour tenir dans la barre.
				int32 tasses = 0;
		};

		// --- Le premier plan -------------------------------------------------------
		NkDocumentOuvert NkEditeurDocumentActif(const NkEditeurInterface &ui) noexcept;
		bool NkEditeurSceneDevant(const NkEditeurInterface &ui) noexcept;
		/// Un Blueprint est-il au premier plan ? (sa page prend la place de la vue)
		bool NkEditeurBlueprintDevant(const NkEditeurInterface &ui) noexcept;
		/// Le document existe-t-il encore (la scene : toujours) ?
		bool NkEditeurDocumentExiste(NkEditeurModele &m, const NkEditeurInterface &ui, const NkDocumentOuvert &d) noexcept;

		/// LA PORTE : met `d` au premier plan. Celui d'avant est QUITTE (un son se
		/// tait, un prefab rend la scene), celui-ci ENTRE (un prefab met la scene
		/// de cote, un Blueprint devient le graphe courant). Un document neuf entre
		/// dans la barre, a droite. Faux s'il n'existe pas, ou s'il refuse d'entrer
		/// (un prefab pendant le jeu) : la scene est alors devant.
		bool NkEditeurActiverDocument(NkEditeurModele &m, NkEditeurInterface &ui, const NkDocumentOuvert &d);
		/// Ferme `d` (jamais la scene). S'il etait devant, son voisin de gauche (a
		/// defaut de droite, a defaut la scene) passe devant AVANT qu'il ne parte.
		bool NkEditeurFermerDocument(NkEditeurModele &m, NkEditeurInterface &ui, const NkDocumentOuvert &d);
		/// La barre suit ce qui est ouvert : un document disparu en sort (et, s'il
		/// etait devant, la scene revient) ; un document ouvert sans elle (un banc
		/// sans interface) y entre. Appelee en tete de chaque trame.
		void NkEditeurSynchroniserDocuments(NkEditeurModele &m, NkEditeurInterface &ui);

		/// Le libelle d'un onglet (« Animation : Heros », « Blueprint : Porte »...).
		NkString NkEditeurLibelleDocument(NkEditeurModele &m, NkEditeurInterface &ui, const NkDocumentOuvert &d);

		/// LA BARRE, a droite de l'onglet de la scene (dessine par
		/// NkEditeurDessinerOnglets) : un onglet par document, clic = premier plan,
		/// croix ou clic du milieu = fermer. Si la barre est pleine, les noms se
		/// TASSENT (ils ne debordent jamais de la fenetre).
		void NkEditeurDessinerOngletsDocuments(NkEditeurCadre &c, float32 x);

		/// Ctrl+W : ferme le document au premier plan (rien si c'est la scene).
		bool NkEditeurFermerDocumentDevant(NkEditeurModele &m, NkEditeurInterface &ui);

		/// `--captures-documents=DOSSIER` : les captures HORS ECRAN de la barre et
		/// des Details (NkEditeurCapturesDocuments.cpp). 0 = toutes ecrites.
		int32 NkEditeurCapturesDocuments(const char *dossier);

	} // namespace editeur
} // namespace nkentseu

#endif // __NKENTSEU_UNKENYEDITOR_NKEDITEURDOCUMENTS_H__
