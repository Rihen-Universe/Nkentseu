// =============================================================================
// NkEditeurPanneaux.h — hierarchie, inspecteur, barre d'outils, pied de page
//
// ⚠️ CHAQUE PANNEAU A DEUX FONCTIONS : UNE QUI DESSINE, UNE QUI TESTE
//   `NkDessinerBarre` et `NkBarreClic` calculent la MEME geometrie. C'est le
//   defaut classique — deux geometries qui doivent s'accorder sans que l'une
//   soit la reference de l'autre — et ce depot le documente sous « deux chemins
//   qui doivent s'accorder se valident l'un l'autre ».
//   La parade tenue ici : les deux appellent `NkRectOutil` / `NkRectBouton`, une
//   SEULE fonction de placement. Le jour ou la barre change, les deux suivent.
// =============================================================================
#pragma once

#include "Editeur/NkEditeurAppareils.h"
#include "NKGui/Core/NkGuiContext.h"
#include "NKGui/Core/NkGuiFont.h"
#include "Unkeny/Unkeny.h"

namespace nkentseu {
	namespace editeur {

		using namespace nkentseu::unkeny;
		struct NkDispoEditeur;
		enum class NkOutil : uint8;

		/// Ce qu'un clic sur la barre a declenche. L'appelant agit ; la barre ne
		/// touche pas a la scene — c'est ce qui la garde testable.
		enum class NkActionBarre : uint8 {
			NK_RIEN = 0,
			NK_OUTIL_CHANGE,
			NK_PROFIL_CHANGE,
			NK_SIMULATION_CHANGE,
			NK_AFFICHAGE_CHANGE,
			NK_CADRER,
			NK_SUPPRIMER
		};

		void NkDessinerBarre(nkgui::NkGuiDrawList &dl, const NkDispoEditeur &dispo, nkgui::NkGuiFont *police,
							 const NkTheme &th, NkOutil outil, const NkProfilAppareil &profil, bool paysage,
							 bool simule, bool collisionneurs, bool grille);

		/// Teste un clic sur la barre et MODIFIE les reglages passes par
		/// reference. Rend ce qui a change, pour que l'appelant reagisse.
		NkActionBarre NkBarreClic(const NkDispoEditeur &dispo, const NkTheme &th, const math::NkVec2f &point,
								  NkOutil &outil, int32 &profil, bool &paysage, bool &simule, bool &collisionneurs,
								  bool &grille);

		void NkDessinerHierarchie(nkgui::NkGuiDrawList &dl, const nkgui::NkRect &zone, nkgui::NkGuiFont *police,
								  NkScene &scene, const NkTheme &th, const ecs::NkEntityId *selection);

		bool NkHierarchieClic(const nkgui::NkRect &zone, NkScene &scene, const math::NkVec2f &point,
							  ecs::NkEntityId &choisi);

		void NkDessinerInspecteur(nkgui::NkGuiDrawList &dl, const nkgui::NkRect &zone, nkgui::NkGuiFont *petite,
								  nkgui::NkGuiFont *corps, NkScene &scene, const NkTheme &th,
								  const ecs::NkEntityId *selection);

		void NkDessinerEtat(nkgui::NkGuiDrawList &dl, const nkgui::NkRect &zone, nkgui::NkGuiFont *police,
							const NkTheme &th, NkScene &scene, const NkStatsRendu &stats,
							const NkProfilAppareil &profil);

	} // namespace editeur
} // namespace nkentseu
