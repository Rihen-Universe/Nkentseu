#pragma once

/*
 * NKUI_MAINTENANCE_GUIDE
 * Responsibility: File browser widget v3 — liste/grille, icônes SVG, unicode,
 *                 actions toolbar complètes, Content Browser style Unreal.
 * Change this file when: provider contract, view modes, or asset types evolve.
 */

#include "NKUI/NkUIContext.h"
#include "NKUI/NkUIFont.h"
#include "NKUI/NkUILayout.h"

namespace nkentseu {
    namespace nkui {

        // =====================================================================
        //  Enums
        // =====================================================================

        enum class NkUIFileType : uint8 {
            NK_FT_UNKNOWN = 0,
            NK_FT_FILE,
            NK_FT_DIRECTORY,
            NK_FT_SYMLINK,
        };

        enum class NkUIFBMode : uint8 {
            NK_FBM_OPEN = 0,
            NK_FBM_SAVE,
            NK_FBM_SELECT_DIR,
            NK_FBM_MULTI,
        };

        enum class NkUIFBViewMode : uint8 {
            NK_FBVM_LIST = 0,      ///< liste détaillée (colonnes)
            NK_FBVM_ICONS_SMALL,   ///< petites icônes + nom
            NK_FBVM_ICONS_LARGE,   ///< grandes icônes + miniature
            NK_FBVM_TILES,         ///< tuiles (icône + info)
        };

        enum class NkUIFBSortCol : uint8 {
            NK_FBS_NAME = 0,
            NK_FBS_SIZE,
            NK_FBS_DATE,
            NK_FBS_TYPE,
        };

        // =====================================================================
        //  Entrée fichier
        // =====================================================================

        struct NKUI_API NkUIFileEntry {
            char         name[256]    = {};
            char         path[512]    = {};
            NkUIFileType type         = NkUIFileType::NK_FT_UNKNOWN;
            uint64       size         = 0;
            uint64       modifiedTime = 0;
            bool         isHidden     = false;
            bool         isReadOnly   = false;
        };

        // =====================================================================
        //  Provider callbacks (ABI stable)
        // =====================================================================

        using NkUIFSListFn   = int32(*)(const char*, NkUIFileEntry*, int32, void*);
        using NkUIFSReadFn   = int64(*)(const char*, void*, uint64, void*);
        using NkUIFSWriteFn  = bool (*)(const char*, const void*, uint64, void*);
        using NkUIFSMkdirFn  = bool (*)(const char*, void*);
        using NkUIFSMoveFn   = bool (*)(const char*, const char*, void*);
        using NkUIFSDeleteFn = bool (*)(const char*, void*);
        using NkUIFSStatFn   = bool (*)(const char*, NkUIFileEntry*, void*);

        struct NKUI_API NkUIFSProvider {
            NkUIFSListFn   list    = nullptr;
            NkUIFSReadFn   read    = nullptr;
            NkUIFSWriteFn  write   = nullptr;
            NkUIFSMkdirFn  mkdir_  = nullptr;
            NkUIFSMoveFn   move    = nullptr;
            NkUIFSDeleteFn delete_ = nullptr;
            NkUIFSStatFn   stat    = nullptr;
            void*          userData= nullptr;

            static NkUIFSProvider NativeProvider() noexcept;
            static NkUIFSProvider Null() noexcept;
        };

        // =====================================================================
        //  Événements
        // =====================================================================

        enum class NkUIFileBrowserEvent : uint8 {
            NK_FB_NONE = 0,
            NK_FB_FILE_SELECTED,
            NK_FB_FILES_SELECTED,
            NK_FB_SAVE_CONFIRMED,
            NK_FB_DIR_SELECTED,
            NK_FB_DIR_CHANGED,
            NK_FB_RENAME_COMMITTED,
            NK_FB_DELETE_REQUESTED,
            NK_FB_DELETE_PERMANENT,  ///< suppression définitive (sans corbeille)
            NK_FB_CREATE_DIR,
            NK_FB_CREATE_FILE,
            NK_FB_DND_DROP,
            NK_FB_CANCELLED,
        };

        struct NKUI_API NkUIFileBrowserResult {
            NkUIFileBrowserEvent event       = NkUIFileBrowserEvent::NK_FB_NONE;
            char                 path[512]   = {};
            char                 target[512] = {};
            static constexpr int32 MAX_SELECTED = 64;
            char                 paths[MAX_SELECTED][512] = {};
            int32                numPaths = 0;
        };

        // =====================================================================
        //  Bookmark
        // =====================================================================

        struct NKUI_API NkUIFBBookmark {
            char label[64]  = {};
            char path[512]  = {};
            bool isSpecial  = false;   ///< icône spéciale (Home, Bureau…)
        };

        // =====================================================================
        //  Configuration
        // =====================================================================

        struct NKUI_API NkUIFileBrowserConfig {
            NkUIFBMode     mode              = NkUIFBMode::NK_FBM_OPEN;
            NkUIFBViewMode viewMode          = NkUIFBViewMode::NK_FBVM_LIST;
            bool           showHidden        = false;
            bool           showFileSize      = true;
            bool           showModifiedDate  = true;
            bool           showTypeCol       = false;
            bool           allowRename       = true;
            bool           allowDelete       = true;
            bool           allowPermanentDelete = true;
            bool           allowCreateDir    = true;
            bool           allowCreateFile   = true;
            bool           allowDnD          = true;
            bool           showBookmarks     = true;
            bool           showConfirmButtons= true;
            bool           showViewToggle    = true;   ///< boutons liste/grille
            char           filterExt[128]    = {};
            char           filterLabel[64]   = {};
            char           rootPath[512]     = "/";

            // Dimensions
            float32 bookmarkPanelW   = 150.f;
            float32 rowHeight        = 26.f;
            float32 toolbarHeight    = 34.f;
            float32 breadcrumbHeight = 26.f;
            float32 statusBarHeight  = 26.f;
            float32 iconGridSize     = 80.f;   ///< taille des icônes en mode grille large
            float32 iconSmallSize    = 48.f;   ///< mode grille petite

            static constexpr int32 MAX_BOOKMARKS = 20;
            NkUIFBBookmark bookmarks[MAX_BOOKMARKS] = {};
            int32          numBookmarks = 0;

            char saveFilename[256] = {};
        };

        // =====================================================================
        //  État runtime
        // =====================================================================

        struct NKUI_API NkUIFileBrowserState {
            static constexpr int32 MAX_ENTRIES    = 2048;
            static constexpr int32 MAX_SELECTED   = 64;
            static constexpr int32 MAX_HIST_DEPTH = 64;

            char          currentPath[512]             = {};
            NkUIFileEntry entries[MAX_ENTRIES]          = {};
            int32         numEntries                    = 0;
            bool          needsRefresh                  = true;

            int32         selected[MAX_SELECTED]        = {};
            int32         numSelected                   = 0;
            int32         hoveredIdx                    = -1;
            int32         lastClickedIdx                = -1;

            char          history[MAX_HIST_DEPTH][512]  = {};
            int32         historyDepth                  = 0;
            int32         historyPos                    = 0;

            bool          renaming                      = false;
            int32         renamingIdx                   = -1;
            char          renameBuffer[256]             = {};

            char          saveFilenameBuffer[256]       = {};
            char          searchBuffer[128]             = {};
            bool          searchFocused                 = false;

            NkUIFBSortCol sortCol                       = NkUIFBSortCol::NK_FBS_NAME;
            bool          sortAscending                 = true;

            float32       scrollY                       = 0.f;
            float32       scrollX                       = 0.f;
            float32       contentH                      = 0.f;
            float32       contentW                      = 0.f;

            bool          dndActive                     = false;
            int32         dndSourceIdx                  = -1;
            NkVec2        dndStartPos                   = {};

            bool          confirmDeletePending          = false;
            bool          confirmPermanentDelete        = false;
            char          confirmDeletePath[512]        = {};

            bool          creatingDir                   = false;
            char          newDirBuffer[256]             = {};
            bool          creatingFile                  = false;
            char          newFileBuffer[256]            = {};

            // Layout adaptatif
            float32       colNameW  = 0.f;
            float32       colSizeW  = 70.f;
            float32       colDateW  = 118.f;
            float32       colTypeW  = 55.f;

            // Mode icônes : scroll propre
            float32       gridScrollY = 0.f;
        };

        // =====================================================================
        //  NkUIFileBrowser
        // =====================================================================

        struct NKUI_API NkUIFileBrowser {

            static NkUIFileBrowserResult Draw(
                NkUIContext&           ctx,
                NkUIDrawList&          dl,
                NkUIFont&              font,
                NkUIID                 id,
                NkRect                 rect,
                NkUIFileBrowserConfig& config,
                NkUIFileBrowserState&  state,
                const NkUIFSProvider&  provider) noexcept;

            static void Navigate(NkUIFileBrowserState& state,
                                  const char* path) noexcept;
            static void Refresh(NkUIFileBrowserState& state,
                                 const NkUIFSProvider& provider,
                                 const NkUIFileBrowserConfig& config) noexcept;
            static const char* GetSelectedPath(const NkUIFileBrowserState& s) noexcept;
            static bool MatchesFilter(const NkUIFileEntry& e,
                                       const char* filter) noexcept;
            static void AddBookmark(NkUIFileBrowserConfig& cfg,
                                     const char* label,
                                     const char* path,
                                     bool special = false) noexcept;
            static void RemoveBookmark(NkUIFileBrowserConfig& cfg, int32 idx) noexcept;
            static void AddDefaultBookmarks(NkUIFileBrowserConfig& cfg) noexcept;
        private:
            struct Layout {
                NkRect toolbar, breadcrumb, bookmarkPanel;
                NkRect fileList, header, statusBar, saveRow;
                bool   hasBookmarks, hasSave;
            };

            static Layout    ComputeLayout(NkRect r,
                                            const NkUIFileBrowserConfig& cfg) noexcept;
            static void      DrawBackground(NkUIDrawList& dl, NkRect r,
                                             const NkUIContext& ctx) noexcept;
            static void      DrawToolbar(NkUIContext& ctx, NkUIDrawList& dl,
                                          NkUIFont& font, const Layout& ly,
                                          NkUIFileBrowserConfig& cfg,
                                          NkUIFileBrowserState& state,
                                          const NkUIFSProvider& prov) noexcept;
            static void      DrawBreadcrumb(NkUIContext& ctx, NkUIDrawList& dl,
                                             NkUIFont& font, const Layout& ly,
                                             NkUIFileBrowserState& state) noexcept;
            static void      DrawBookmarkPanel(NkUIContext& ctx, NkUIDrawList& dl,
                                                NkUIFont& font, const Layout& ly,
                                                const NkUIFileBrowserConfig& cfg,
                                                NkUIFileBrowserState& state) noexcept;
            static void      DrawColumnHeaders(NkUIContext& ctx, NkUIDrawList& dl,
                                                NkUIFont& font, const Layout& ly,
                                                const NkUIFileBrowserConfig& cfg,
                                                NkUIFileBrowserState& state) noexcept;

            // Modes d'affichage
            static NkUIFileBrowserResult DrawListView(NkUIContext& ctx,
                                                       NkUIDrawList& dl,
                                                       NkUIFont& font,
                                                       NkUIID id,
                                                       const Layout& ly,
                                                       NkUIFileBrowserConfig& cfg,
                                                       NkUIFileBrowserState& state,
                                                       const NkUIFSProvider& prov) noexcept;
            static NkUIFileBrowserResult DrawGridView(NkUIContext& ctx,
                                                       NkUIDrawList& dl,
                                                       NkUIFont& font,
                                                       NkUIID id,
                                                       const Layout& ly,
                                                       NkUIFileBrowserConfig& cfg,
                                                       NkUIFileBrowserState& state,
                                                       const NkUIFSProvider& prov) noexcept;
            static NkUIFileBrowserResult DrawEntryRow(NkUIContext& ctx,
                                                       NkUIDrawList& dl,
                                                       NkUIFont& font,
                                                       NkRect row,
                                                       int32 idx,
                                                       bool selected,
                                                       bool hovered,
                                                       const NkUIFileBrowserConfig& cfg,
                                                       NkUIFileBrowserState& state,
                                                       const NkUIFSProvider& prov) noexcept;
            static NkUIFileBrowserResult DrawGridCell(NkUIContext& ctx,
                                                       NkUIDrawList& dl,
                                                       NkUIFont& font,
                                                       NkRect cell,
                                                       int32 idx,
                                                       bool selected,
                                                       bool hovered,
                                                       float32 iconSize,
                                                       NkUIFileBrowserState& state,
                                                       const NkUIFSProvider& prov) noexcept;
            static void      DrawStatusBar(NkUIContext& ctx, NkUIDrawList& dl,
                                            NkUIFont& font, const Layout& ly,
                                            const NkUIFileBrowserConfig& cfg,
                                            const NkUIFileBrowserState& state) noexcept;
            static NkUIFileBrowserResult DrawSaveRow(NkUIContext& ctx,
                                                      NkUIDrawList& dl,
                                                      NkUIFont& font,
                                                      NkUIID id,
                                                      const Layout& ly,
                                                      NkUIFileBrowserConfig& cfg,
                                                      NkUIFileBrowserState& state) noexcept;
            static NkUIFileBrowserResult DrawConfirmButtons(NkUIContext& ctx,
                                                             NkUIDrawList& dl,
                                                             NkUIFont& font,
                                                             const Layout& ly,
                                                             const NkUIFileBrowserConfig& cfg,
                                                             NkUIFileBrowserState& state) noexcept;
            static void      DrawDeleteOverlay(NkUIContext& ctx, NkUIDrawList& dl,
                                                NkUIFont& font, NkRect rect,
                                                NkUIFileBrowserState& state,
                                                NkUIFileBrowserResult& result) noexcept;
            static void      DrawDndGhost(NkUIDrawList& dl, NkUIFont& font,
                                           const NkUIContext& ctx,
                                           const NkUIFileBrowserState& state) noexcept;
            static void      DrawCreateInlineFields(NkUIContext& ctx,
                                                     NkUIDrawList& dl,
                                                     NkUIFont& font,
                                                     NkRect area,
                                                     NkUIFileBrowserState& state,
                                                     const NkUIFSProvider& prov,
                                                     NkUIFileBrowserResult& result) noexcept;

            // Icônes dessinées en primitives (pas de texture)
            static void      DrawIconFolder(NkUIDrawList& dl, NkRect r,
                                             NkColor col, bool open = false) noexcept;
            static void      DrawIconFile(NkUIDrawList& dl, NkRect r, NkColor col) noexcept;
            static void      DrawIconSymlink(NkUIDrawList& dl, NkRect r,
                                              NkColor col) noexcept;
            static void      DrawIconTrash(NkUIDrawList& dl, NkRect r,
                                            NkColor col, bool full = false) noexcept;
            static void      DrawIconNewFolder(NkUIDrawList& dl, NkRect r,
                                                NkColor col) noexcept;
            static void      DrawIconNewFile(NkUIDrawList& dl, NkRect r,
                                              NkColor col) noexcept;
            static void      DrawIconRefresh(NkUIDrawList& dl, NkRect r,
                                              NkColor col) noexcept;
            static void      DrawIconList(NkUIDrawList& dl, NkRect r,
                                           NkColor col) noexcept;
            static void      DrawIconGrid(NkUIDrawList& dl, NkRect r,
                                           NkColor col) noexcept;
            static void      DrawIconSearch(NkUIDrawList& dl, NkRect r,
                                             NkColor col) noexcept;
            static void      DrawEntryIcon(NkUIDrawList& dl, NkRect r,
                                            const NkUIFileEntry& e,
                                            float32 scale = 1.f) noexcept;

            // Miniatures (couleur par type d'extension)
            static void      DrawFileThumbnail(NkUIDrawList& dl, NkRect r,
                                                const NkUIFileEntry& e) noexcept;

            static NkColor   IconColorForEntry(const NkUIFileEntry& e) noexcept;
            static NkColor   ExtensionAccentColor(const char* ext) noexcept;

            static void      PushHistory(NkUIFileBrowserState& s, const char* p) noexcept;
            static void      FormatSize(uint64 b, char* out, int32 sz) noexcept;
            static void      FormatDate(uint64 ts, char* out, int32 sz) noexcept;
            static bool      ExtMatchesFilter(const char* ext, const char* f) noexcept;
            static void      SortEntries(NkUIFileBrowserState& s,
                                          NkUIFBSortCol col, bool asc) noexcept;

            // Gestion de l'interaction sélection unifiée
            static void      HandleSelectionClick(NkUIFileBrowserConfig& cfg,
                                                   NkUIFileBrowserState& state,
                                                   int32 idx,
                                                   bool ctrl, bool shift) noexcept;
            static void      HandleEntryActivate(NkUIFileBrowserState& state,
                                                  int32 idx,
                                                  NkUIFileBrowserResult& result) noexcept;

            friend struct NkUIContentBrowser;
        };

        // =====================================================================
        //  Content Browser (style Unreal Engine)
        // =====================================================================

        enum class NkUIAssetType : uint8 {
            NK_ASSET_UNKNOWN = 0,
            NK_ASSET_TEXTURE,
            NK_ASSET_MESH,
            NK_ASSET_AUDIO,
            NK_ASSET_MATERIAL,
            NK_ASSET_SCENE,
            NK_ASSET_SCRIPT,
            NK_ASSET_FONT,
            NK_ASSET_ANIMATION,
            NK_ASSET_PREFAB,
            NK_ASSET_FOLDER,
        };

        struct NKUI_API NkUIAssetEntry {
            char          name[128]    = {};
            char          path[512]    = {};
            NkUIAssetType type         = NkUIAssetType::NK_ASSET_UNKNOWN;
            uint64        size         = 0;
            uint32        thumbnailTexId = 0;   ///< handle GPU, 0 = pas de miniature
            bool          isFolder     = false;
            bool          hasUnsavedChanges = false;
        };

        struct NKUI_API NkUIContentBrowserConfig {
            float32 thumbnailSize    = 96.f;     ///< taille des tuiles
            float32 minThumbnailSize = 48.f;
            float32 maxThumbnailSize = 192.f;
            float32 treeWidth        = 200.f;    ///< panneau arbre gauche
            bool    showAssetType    = true;
            bool    showAssetSize    = false;
            bool    showTree         = true;
            bool    allowDrag        = true;
            char    rootPath[512]    = "/";
            uint8   filterMask       = 0xFF;     ///< bits = NkUIAssetType
        };

        struct NKUI_API NkUIContentBrowserState {
            static constexpr int32 MAX_ASSETS    = 2048;
            static constexpr int32 MAX_SEL       = 64;
            static constexpr int32 MAX_TREE_NODES= 256;

            char          currentPath[512]          = {};
            NkUIAssetEntry assets[MAX_ASSETS]        = {};
            int32         numAssets                  = 0;
            int32         selected[MAX_SEL]          = {};
            int32         numSelected                = 0;
            int32         hoveredIdx                 = -1;
            char          searchBuffer[128]          = {};
            bool          searchFocused              = false;
            bool          needsRefresh               = true;
            float32       thumbnailSizeCurrent       = 96.f;
            float32       scrollY                    = 0.f;

            // Arbre de dossiers (panel gauche)
            struct TreeNode {
                char    name[128] = {};
                char    path[512] = {};
                int32   parent    = -1;
                bool    expanded  = false;
                bool    hasChildren = false;
            };
            TreeNode treeNodes[MAX_TREE_NODES]       = {};
            int32    numTreeNodes                    = 0;
            int32    selectedTreeNode                = -1;
            float32  treeScrollY                     = 0.f;

            bool     dndActive                       = false;
            int32    dndSourceIdx                    = -1;
        };

        enum class NkUIContentBrowserEvent : uint8 {
            NK_CB_NONE = 0,
            NK_CB_ASSET_SELECTED,
            NK_CB_ASSETS_SELECTED,
            NK_CB_ASSET_DOUBLE_CLICKED,
            NK_CB_ASSET_DRAGGED,
            NK_CB_FOLDER_CHANGED,
            NK_CB_ASSET_RENAMED,
            NK_CB_ASSET_DELETED,
        };

        struct NKUI_API NkUIContentBrowserResult {
            NkUIContentBrowserEvent event = NkUIContentBrowserEvent::NK_CB_NONE;
            char path[512]  = {};
            char target[512]= {};
            static constexpr int32 MAX_PATHS = 64;
            char paths[MAX_PATHS][512] = {};
            int32 numPaths = 0;
        };

        struct NKUI_API NkUIContentBrowser {

            /// Point d'entrée principal — dessine le Content Browser complet.
            static NkUIContentBrowserResult Draw(
                NkUIContext&                ctx,
                NkUIDrawList&               dl,
                NkUIFont&                   font,
                NkUIID                      id,
                NkRect                      rect,
                NkUIContentBrowserConfig&   config,
                NkUIContentBrowserState&    state,
                const NkUIFSProvider&       provider) noexcept;

            /// Charge les assets d'un dossier depuis le filesystem.
            static void LoadFolder(NkUIContentBrowserState& state,
                                    const char* path,
                                    const NkUIFSProvider& provider) noexcept;

            /// Déduit le type d'asset depuis l'extension.
            static NkUIAssetType GuessAssetType(const char* filename) noexcept;

            /// Retourne le label lisible du type.
            static const char* AssetTypeLabel(NkUIAssetType t) noexcept;

        private:
            static void DrawTree(NkUIContext& ctx, NkUIDrawList& dl,
                                  NkUIFont& font, NkRect rect,
                                  NkUIContentBrowserConfig& cfg,
                                  NkUIContentBrowserState& state,
                                  const NkUIFSProvider& prov,
                                  NkUIContentBrowserResult& result) noexcept;

            static NkUIContentBrowserResult DrawAssetGrid(NkUIContext& ctx,
                                                           NkUIDrawList& dl,
                                                           NkUIFont& font,
                                                           NkUIID id,
                                                           NkRect rect,
                                                           NkUIContentBrowserConfig& cfg,
                                                           NkUIContentBrowserState& state) noexcept;

            static void DrawAssetTile(NkUIContext& ctx, NkUIDrawList& dl,
                                       NkUIFont& font, NkRect cell,
                                       int32 idx, bool selected, bool hovered,
                                       float32 thumbSize,
                                       NkUIContentBrowserState& state,
                                       NkUIContentBrowserResult& result) noexcept;

            static void DrawToolbar(NkUIContext& ctx, NkUIDrawList& dl,
                                     NkUIFont& font, NkRect rect,
                                     NkUIContentBrowserConfig& cfg,
                                     NkUIContentBrowserState& state,
                                     const NkUIFSProvider& prov) noexcept;

            static void DrawAssetTypeIcon(NkUIDrawList& dl, NkRect r,
                                           NkUIAssetType t, bool large = false) noexcept;
            static NkColor AssetTypeColor(NkUIAssetType t) noexcept;

            static void BuildTree(NkUIContentBrowserState& state,
                                   const char* rootPath,
                                   const NkUIFSProvider& prov) noexcept;
        };

    } // namespace nkui
} // namespace nkentseu