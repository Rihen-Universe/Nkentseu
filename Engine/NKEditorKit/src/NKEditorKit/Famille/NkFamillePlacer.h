#pragma once
// -----------------------------------------------------------------------------
// @File    NkFamillePlacer.h
// @Brief   « PLACER DES ACTEURS » (Place Actors d'UE5) de la famille : onglets
//          verticaux a icone (Favoris, Récents, Base, Lumières, Formes, Effets,
//          Volumes, Tout), recherche, liste a icones avec l'etoile des favoris,
//          aide en bas ; CLIC = poser au centre de la vue, GLISSER vers la vue =
//          poser au point lache (avec le fantome qui suit le curseur).
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// D'OU CA VIENT : UnkenyEditor, NkEditeurPlacer.cpp (NkEditeurDessinerPlacer).
// Le CATALOGUE est a l'application -- c'est SA touche (document 02 §0) : des
// formes 2D chez Unkeny, les primitives 3D, la camera et les lumieres chez
// Nogee. Le panneau dit ce qui a ete clique ou lache ; poser est a l'application.
// -----------------------------------------------------------------------------

#include "NKEditorKit/Famille/NkFamilleStyle.h"
#include "NKContainers/Sequential/NkVector.h"

namespace nkentseu {
	namespace editorkit {

		/// Les onglets verticaux, dans l'ordre d'UE5.
		enum class NkFamilleOngletPlacer : uint8 {
			Favoris = 0,
			Recents,
			Base,
			Lumieres,
			Formes,
			Effets,
			Volumes,
			Tout,
			Count
		};
		const char *NkFamilleNomOngletPlacer(NkFamilleOngletPlacer o) noexcept;
		/// Le bit d'un onglet, pour `NkFamilleElementPlacer::onglets`.
		constexpr uint32 NkFamilleBitOnglet(NkFamilleOngletPlacer o) noexcept {
			return 1u << static_cast<uint32>(o);
		}

		/// Un element du catalogue. L'icone est peinte par l'application.
		struct NkFamilleElementPlacer {
				const char *nom = "";
				const char *aide = "";
				uint32 onglets = 0; ///< bits NkFamilleBitOnglet (Tout le contient toujours)
		};

		/// Peint l'icone de l'element `indice` dans le carre `r`.
		using NkFamilleIconePlacerFn = void (*)(void *user, nkgui::NkGuiDrawList &dl, int32 indice, const nkgui::NkRect &r,
												const nkgui::NkColor &texte);

		/// L'etat du panneau, a garder d'une trame a l'autre.
		struct NkFamillePlacer {
				int32 onglet = static_cast<int32>(NkFamilleOngletPlacer::Base);
				char filtre[64] = {};
				bool filtreFocus = false;
				float32 defil = 0.f;
				nk_uint64 favoris = 0;		///< bit k = l'element k (64 premiers)
				NkVector<int32> recents;	///< le plus recent d'abord
				int32 appui = -1;			///< l'element sous l'appui
				bool glisse = false;
				nkgui::NkVec2 depart{0.f, 0.f};
				NkVector<nkgui::NkRect> rects; ///< la rangee de chaque element (vide = hors champ)
				nkgui::NkRect ongletsRects[static_cast<int32>(NkFamilleOngletPlacer::Count)] = {};

				/// Retient `k` en tete des Recents (10 au plus).
				void Retenir(int32 k);
		};

		struct NkFamillePlacerResultat {
				int32 clic = -1;	 ///< clique sans glisser : poser au centre de la vue
				int32 lache = -1;	 ///< glisse puis lache DANS `cible` : poser en `lachePos`
				nkgui::NkVec2 lachePos{0.f, 0.f};
				int32 glisse = -1;	 ///< en cours de glisser (le fantome est peint)
				bool aLaSouris = false;
		};

		/// Le panneau dans `zone`. `cible` : le viseur (la ou un lacher pose).
		NkFamillePlacerResultat NkFamilleDessinerPlacer(NkFamilleCtx &c, const nkgui::NkRect &zone, NkFamillePlacer &p,
														const NkFamilleElementPlacer *catalogue, int32 n,
														NkFamilleIconePlacerFn icone, void *user,
														const nkgui::NkRect &cible);

		/// L'icone d'un onglet vertical (etoile, horloge, bonhomme, ampoule, formes,
		/// flamme, volume en pointilles, grille), centree en (cx, cy).
		void NkFamilleIconeOngletPlacer(nkgui::NkGuiDrawList &dl, NkFamilleOngletPlacer o, float32 cx, float32 cy,
										const nkgui::NkColor &col) noexcept;

		/// La recherche de la famille : sans casse et sans accents (« etoile »
		/// trouve « Étoile »).
		bool NkFamilleContientPlie(const char *texte, const char *motif) noexcept;

	} // namespace editorkit
} // namespace nkentseu
