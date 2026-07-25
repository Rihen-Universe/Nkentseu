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

namespace nkentseu {
	namespace noge {

		struct NkAssetBrowserEntry {
				NkString name;
				NkString fullPath;
				NkString relativePath;
				NkAssetType type;
				bool isDirectory;
				nk_uint64 thumbnailHandle = 0;
				// [PERF 2026-07-25] Thumbnail paresseux : true quand une
				// génération a été tentée (réussie ou non) — évite de
				// re-tenter chaque frame un fichier illisible.
				bool thumbnailTried = false;
		};

		class AssetBrowser {
			public:
				AssetBrowser() = default;

				void Init(AssetManager *assetMgr, const char *projectDir) noexcept;

				void Render(nkui::NkUIContext &ctx, nkui::NkUIWindowManager &wm, nkui::NkUIDrawList &dl,
							nkui::NkUIFont &font, nkui::NkUILayoutStack &ls, nkui::NkRect rect) noexcept;

				// Chemin sélectionné (pour Import, LoadScene, etc.)
				const NkString &SelectedPath() const noexcept {
					return mSelectedPath;
				}

				bool HasSelection() const noexcept {
					return !mSelectedPath.Empty();
				}

			private:
				void NavigateTo(const char *dir) noexcept;
				void RefreshEntries() noexcept;
				void RenderBreadcrumb(nkui::NkUIContext &ctx, nkui::NkUIDrawList &dl, nkui::NkUIFont &font,
									  nkui::NkUILayoutStack &ls) noexcept;
				// entry non-const : le thumbnail est généré paresseusement au
				// premier rendu visible (budget par frame, cf. mThumbBudget).
				void RenderEntry(nkui::NkUIContext &ctx, nkui::NkUIDrawList &dl, nkui::NkUIFont &font,
								 nkui::NkUILayoutStack &ls, NkAssetBrowserEntry &entry, float32 thumbSize) noexcept;

				AssetManager *mAssetMgr = nullptr;
				NkString mProjectDir;
				NkString mCurrentDir;
				NkString mSelectedPath;
				float32 mThumbnailSize = 72.f;

				NkVector<NkAssetBrowserEntry> mEntries;

				// [PERF 2026-07-25] Budget de thumbnails générés PAR FRAME.
				// Avant : RefreshEntries() chargeait synchroneusement TOUTES
				// les textures du dossier (CPU load + upload GPU) → démarrage
				// de plusieurs secondes quand le dossier contient beaucoup
				// d'images. Maintenant : génération étalée au rendu (2/frame).
				static constexpr int32 THUMB_LOADS_PER_FRAME = 2;
				int32 mThumbBudget = 0;

				// Filtre de recherche
				char mFilterBuf[128] = {};
		};

	} // namespace noge
} // namespace nkentseu
