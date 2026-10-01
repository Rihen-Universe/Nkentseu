//
// NkEditeurAssets.h
// =============================================================================
// Description :
//   CHAQUE ASSET S'OUVRE (2026-10-01, R33, retour 3 de Rihen : « aujourd'hui
//   rien ne se passe, sauf pour une scene »). Un double-clic dans le Content
//   Browser ouvre un ONGLET a cote de celui de la scene :
//     texture    apercu (damier, zoom), filtrage (lisse / pixel), pivot et
//                pixels par unite par defaut, « appliquer aux sprites » ;
//     police     apercu en plusieurs tailles (une phrase, l'alphabet, chiffres) ;
//     son        lecture / arret, volume, en boucle, duree ;
//     prefab     le MODE PREFAB : la scene est mise de cote, l'editeur entier
//                (vue, Outliner, Details) travaille sur le prefab seul ;
//                « Enregistrer » ecrit le .nkprefab et, au retour a la scene,
//                ses INSTANCES le suivent (surcharges gardees) ;
//     controleur ses etats et parametres, en lecture ; l'editeur de graphe
//                viendra de NKEditorKit (NkEditeurOuvrirAnimateur, le point
//                d'accroche, plus bas).
//   Les reglages d'un asset (texture, son) vivent dans `Contenu/.nkreglages`,
//   un fichier cache (le navigateur ne montre pas les « .xxx ») : une ligne par
//   reglage, « chemin<TAB>cle<TAB>valeur ».
//
// ⚠️ LE FILTRAGE « PIXEL » NE VAUT ENCORE QUE POUR L'APERCU ET LE REGLAGE SAUVE :
//   le televersement de NkCanvasGuiApp n'a pas de filtre par texture (NKCanvas a
//   NkTexture::SetFilter, pas son relais d'images). L'onglet le DIT.
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================

#pragma once

#ifndef __NKENTSEU_UNKENYEDITOR_NKEDITEURASSETS_H__
#define __NKENTSEU_UNKENYEDITOR_NKEDITEURASSETS_H__

#include "Editeur/NkEditeurInterface.h"

namespace nkentseu {
	namespace editeur {

		/// Ce qu'un onglet d'asset sait ouvrir.
		enum class NkGenreAsset : uint8 { NK_AUCUN = 0, NK_TEXTURE, NK_POLICE, NK_SON, NK_PREFAB, NK_CONTROLEUR };
		/// Le genre d'un chemin du navigateur (par sa nature, NkEditeurNatureFichier).
		NkGenreAsset NkEditeurGenreAsset(const char *cheminNav);
		const char *NkNomGenreAsset(NkGenreAsset g) noexcept;

		/// Les reglages d'une TEXTURE (Contenu/.nkreglages).
		struct NkReglagesTexture {
				bool pixel = false;			   ///< filtrage au plus proche (pixel art)
				NkVec2f pivot{0.5f, 0.5f};	   ///< le pivot d'un sprite pose avec elle
				float32 pixelsParUnite = 100.f; ///< 100 px d'image = 1 m (taille du sprite pose)
		};
		/// Les reglages d'un SON.
		struct NkReglagesSon {
				float32 volume = 1.f;
				bool boucle = false;
		};
		bool NkEditeurLireReglagesTexture(NkEditeurModele &m, const char *cheminNav, NkReglagesTexture &r);
		bool NkEditeurEcrireReglagesTexture(NkEditeurModele &m, const char *cheminNav, const NkReglagesTexture &r);
		bool NkEditeurLireReglagesSon(NkEditeurModele &m, const char *cheminNav, NkReglagesSon &r);
		bool NkEditeurEcrireReglagesSon(NkEditeurModele &m, const char *cheminNav, const NkReglagesSon &r);

		/// Un onglet ouvert. Ce qu'il charge lui appartient (police, son, texture).
		struct NkOngletAsset {
				NkString nav;	 ///< « Contenu/... »
				NkString disque; ///< le chemin absolu
				NkGenreAsset genre = NkGenreAsset::NK_AUCUN;
				NkReglagesTexture texture;
				NkReglagesSon son;
				uint32 texId = 0u;
				uint32 sonId = 0u;
				uint32 voix = 0u;
				float32 zoom = 1.f;		 ///< l'apercu d'une texture
				nkgui::NkGuiFont *police = nullptr; ///< l'apercu d'une police (a nous)
				bool policeOk = false;
				uint32 prefab = 0u; ///< l'identifiant NkPrefabs2D du prefab ouvert
				uint64 racineUid = 0u; ///< l'identite de sa racine dans la scene du prefab
				/// L'edition du prefab, gardee quand on revient a la scene sans
				/// enregistrer : on la retrouve en rouvrant l'onglet.
				unkeny::NkScene::NkPhoto prefabPhoto;
				bool prefabPhotoValide = false;
				NkString modele;	///< le nom de modele d'un controleur
				bool modifie = false; ///< reglages ou prefab non enregistres
		};

		/// La scene MISE DE COTE pendant qu'un prefab s'edite dans son onglet.
		struct NkModePrefab {
				unkeny::NkScene::NkPhoto photo;
				NkVue2D camera;
				NkHistoriqueEditeur historique;
				uint64 selection = 0u; ///< identite de la selection (0 : aucune)
				int32 onglet = -1;	   ///< l'onglet du prefab en cours d'edition
				ecs::NkEntityId racine;
				/// Les .nkprefab enregistres pendant le mode : relus au retour, et leurs
				/// instances de la scene suivent.
				NkVector<NkString> aRelire;
		};

		// --- Les onglets -------------------------------------------------------
		/// Ouvre l'asset `cheminNav` dans son onglet (ou y revient). Rend false
		/// pour un genre sans onglet (la scene a son chemin a elle).
		bool NkEditeurOuvrirAsset(NkEditeurCadre &c, const char *cheminNav);
		/// Rend l'onglet `k` actif (-1 = la scene). Entrer dans un prefab met la
		/// scene de cote ; en sortir la rend, et relit les prefabs enregistres.
		void NkEditeurActiverOnglet(NkEditeurCadre &c, int32 k);
		void NkEditeurFermerOnglet(NkEditeurCadre &c, int32 k);
		/// L'onglet actif est-il un asset qui REMPLACE la vue (pas un prefab) ?
		bool NkEditeurAssetALaPlaceDeLaVue(const NkEditeurInterface &ui) noexcept;
		/// Le prefab en cours d'edition, ou nul.
		NkOngletAsset *NkEditeurPrefabOuvert(NkEditeurInterface &ui) noexcept;
		/// « Enregistrer » dans le mode prefab : ecrit le .nkprefab (rend false hors mode).
		bool NkEditeurEnregistrerPrefabOuvert(NkEditeurCadre &c);
		/// L'editeur d'asset, a la place de la vue (barre de vue comprise).
		void NkEditeurDessinerAsset(NkEditeurCadre &c);
		/// Les onglets d'assets, a droite de celui de la scene (NkEditeurDessinerOnglets).
		void NkEditeurDessinerOngletsAssets(NkEditeurCadre &c, float32 x);
		/// Libere ce que les onglets ont charge (fin de l'application, bancs).
		void NkEditeurFermerTousOnglets(NkEditeurCadre &c);

		/// ⚓ LE POINT D'ACCROCHE DU CONTROLEUR D'ANIMATION (2026-10-01). Un autre
		/// chantier (NKEditorKit, branche editorkit/animation-animateur) ecrit la
		/// page ANIMATEUR (graphe des etats, transitions). Quand elle sera fusionnee,
		/// c'est ICI qu'elle se branche : `cheminDisque` est le .nkanimctl ouvert.
		/// Aujourd'hui : rend false (rien de plus que l'onglet en lecture).
		bool NkEditeurOuvrirAnimateur(NkEditeurCadre &c, const char *cheminDisque);

	} // namespace editeur
} // namespace nkentseu

#endif // __NKENTSEU_UNKENYEDITOR_NKEDITEURASSETS_H__
