#pragma once
// =============================================================================
// Unkeny/Panels/AssetBrowser.h
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
#include "NKFileSystem/NkFileSystem.h"
#include "../Editor/AssetManager.h"

namespace nkentseu {
    namespace Unkeny {

        struct NkAssetBrowserEntry {
            NkString      name;
            NkString      fullPath;
            NkString      relativePath;
            NkAssetType   type;
            bool          isDirectory;
            nk_uint64     thumbnailHandle = 0;
        };

        class AssetBrowser {
        public:
            AssetBrowser() = default;

            void Init(AssetManager* assetMgr,
                      const char* projectDir) noexcept;

            void Render(nkui::NkUIContext& ctx,
                        nkui::NkUIWindowManager& wm,
                        nkui::NkUIDrawList& dl,
                        nkui::NkUIFont& font,
                        nkui::NkUILayoutStack& ls,
                        nkui::NkUIRect rect) noexcept;

            // Chemin sélectionné (pour Import, LoadScene, etc.)
            const NkString& SelectedPath() const noexcept { return mSelectedPath; }
            bool            HasSelection() const noexcept { return !mSelectedPath.IsEmpty(); }

        private:
            void NavigateTo(const char* dir) noexcept;
            void RefreshEntries() noexcept;
            void RenderBreadcrumb(nkui::NkUIContext& ctx,
                                  nkui::NkUIDrawList& dl,
                                  nkui::NkUIFont& font,
                                  nkui::NkUILayoutStack& ls) noexcept;
            void RenderEntry(nkui::NkUIContext& ctx,
                             nkui::NkUIDrawList& dl,
                             nkui::NkUIFont& font,
                             nkui::NkUILayoutStack& ls,
                             const NkAssetBrowserEntry& entry,
                             float32 thumbSize) noexcept;

            AssetManager*  mAssetMgr    = nullptr;
            NkString       mProjectDir;
            NkString       mCurrentDir;
            NkString       mSelectedPath;
            float32        mThumbnailSize = 72.f;

            NkVector<NkAssetBrowserEntry> mEntries;

            // Filtre de recherche
            char     mFilterBuf[128] = {};
        };

    } // namespace Unkeny
} // namespace nkentseu
