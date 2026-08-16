#pragma once
// =============================================================================
// Noge/Panels/InspectorPanel.h
// =============================================================================
// Affiche et édite les composants de l'entité sélectionnée.
// Utilise NkComponentMetaRegistry (NkReflectComponents.h) pour générer
// automatiquement les widgets selon le type de chaque champ.
// =============================================================================

#include "NKCore/NkTypes.h"
#include "NKUI/NKUI.h"
#include "NKUI/NkUIWidgets.h"
#include "NKECS/World/NkWorld.h"
#include "Noge/ECS/Systems/NkReflectComponents.h"
#include "Nogee/Editor/NkSelectionManager.h"
#include "Nogee/Editor/CommandHistory.h"
#include "Nogee/Panels/Model/NkInspectorModel.h" // etat neutre (sans UI)

namespace nkentseu {
	namespace noge {

		// L'etat des sections dépliées vit dans NkInspectorModel, en-tete NEUTRE
		// sans dependance d'interface — pour qu'un portage vers NKGui le partage
		// au lieu de le recopier. Le choix du widget par type de champ, lui,
		// reste ici : il depend de la bibliotheque d'affichage.
		class InspectorPanel : public NkInspectorModel {
			public:
				InspectorPanel() = default;

				void Render(nkui::NkUIContext &ctx, nkui::NkUIWindowManager &wm, nkui::NkUIDrawList &dl,
							nkui::NkUIFont &font, nkui::NkUILayoutStack &ls, ecs::NkWorld &world,
							const NkSelectionManager &sel, CommandHistory *hist, nkui::NkRect rect) noexcept;

			private:
				// Rendu d'un composant complet (section pliable)
				void RenderComponent(nkui::NkUIContext &ctx, nkui::NkUIDrawList &dl, nkui::NkUIFont &font,
									 nkui::NkUILayoutStack &ls, const ecs::NkComponentMeta &meta, void *compPtr,
									 ecs::NkEntityId id, CommandHistory *hist) noexcept;

				// Rendu d'un champ individuel selon son type
				bool RenderField(nkui::NkUIContext &ctx, nkui::NkUIDrawList &dl, nkui::NkUIFont &font,
								 nkui::NkUILayoutStack &ls, const ecs::NkFieldMeta &field, void *compPtr) noexcept;

				// Bouton "Add Component"
				void RenderAddComponentMenu(nkui::NkUIContext &ctx, nkui::NkUIDrawList &dl, nkui::NkUIFont &font,
											nkui::NkUILayoutStack &ls, ecs::NkWorld &world,
											ecs::NkEntityId id) noexcept;

				// Sections ouvertes (par nom de composant) et IsSectionOpen/
				// SetSectionOpen sont hérités de NkInspectorModel.
		};

	} // namespace noge
} // namespace nkentseu
