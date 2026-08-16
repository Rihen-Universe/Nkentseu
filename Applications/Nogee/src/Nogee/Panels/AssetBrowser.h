#pragma once
// =============================================================================
// Nogee/Panels/AssetBrowser.h
// =============================================================================
// Navigateur d'assets du projet : dossiers, fichiers, thumbnails.
// Double-clic sur une texture → importe dans la scène.
// Double-clic sur une scène → NkSceneManager::LoadScene().
// =============================================================================

#include "NKCore/NkTypes.h"
#include "NKUI/NKUI.h"
#include "NKUI/NkUIWidgets.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/String/NkString.h"
#include "Nogee/Editor/AssetManager.h"
#include "Nogee/Panels/Model/NkAssetBrowserModel.h" // etat neutre + NkAssetBrowserEntry

namespace nkentseu {
	namespace noge {

		// NkAssetBrowserEntry et l'etat (dossier courant, selection, filtre,
		// budget de vignettes) vivent dans NkAssetBrowserModel, en-tete NEUTRE
		// sans dependance d'interface — pour qu'un portage vers NKGui le partage
		// au lieu de le recopier.
		class AssetBrowser : public NkAssetBrowserModel {
			public:
				AssetBrowser() = default;

				void Init(AssetManager *assetMgr, const char *projectDir) noexcept;

				void Render(nkui::NkUIContext &ctx, nkui::NkUIWindowManager &wm, nkui::NkUIDrawList &dl,
							nkui::NkUIFont &font, nkui::NkUILayoutStack &ls, nkui::NkRect rect) noexcept;

				// SelectedPath() / HasSelection() sont hérités de NkAssetBrowserModel.

			private:
				void NavigateTo(const char *dir) noexcept;
				void RefreshEntries() noexcept;
				void RenderBreadcrumb(nkui::NkUIContext &ctx, nkui::NkUIDrawList &dl, nkui::NkUIFont &font,
									  nkui::NkUILayoutStack &ls) noexcept;
				// entry non-const : le thumbnail est généré paresseusement au
				// premier rendu visible (budget par frame, cf. mThumbBudget).
				void RenderEntry(nkui::NkUIContext &ctx, nkui::NkUIDrawList &dl, nkui::NkUIFont &font,
								 nkui::NkUILayoutStack &ls, NkAssetBrowserEntry &entry, float32 thumbSize) noexcept;

				// L'état (mAssetMgr, mProjectDir, mCurrentDir, mSelectedPath,
				// mThumbnailSize, mEntries, mThumbBudget, mFilterBuf) et la
				// constante THUMB_LOADS_PER_FRAME sont hérités de
				// NkAssetBrowserModel.
		};

	} // namespace noge
} // namespace nkentseu
