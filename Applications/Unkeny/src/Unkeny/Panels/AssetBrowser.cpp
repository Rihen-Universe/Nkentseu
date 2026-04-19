#include "AssetBrowser.h"
#include "NKFileSystem/NkDirectory.h"
#include "NKLogger/NkLog.h"
#include <cstring>
#include <cstdio>

using namespace nkentseu::nkui;

namespace nkentseu {
    namespace Unkeny {

        void AssetBrowser::Init(AssetManager* mgr,
                                const char* projectDir) noexcept {
            mAssetMgr   = mgr;
            mProjectDir = NkString(projectDir ? projectDir : ".");
            NavigateTo(mProjectDir.CStr());
        }

        void AssetBrowser::NavigateTo(const char* dir) noexcept {
            mCurrentDir = NkString(dir);
            RefreshEntries();
        }

        void AssetBrowser::RefreshEntries() noexcept {
            mEntries.Clear();

            // Lister le contenu du dossier courant via NKFileSystem
            NkDirectory directory;
            if (!directory.Open(mCurrentDir.CStr())) {
                logger.Warnf("[AssetBrowser] Dossier inaccessible: {}\n",
                             mCurrentDir.CStr());
                return;
            }

            NkVector<NkString> names;
            directory.GetEntries(names);
            directory.Close();

            // Dossier parent (..)
            if (mCurrentDir != mProjectDir) {
                NkAssetBrowserEntry parent;
                parent.name        = "..";
                parent.fullPath    = NkPath::GetDirectory(mCurrentDir.CStr());
                parent.isDirectory = true;
                parent.type        = NkAssetType::Unknown;
                mEntries.PushBack(parent);
            }

            for (nk_usize i = 0; i < names.Size(); ++i) {
                const NkString& name = names[i];
                NkString full = mCurrentDir + "/" + name;

                NkAssetBrowserEntry e;
                e.name         = name;
                e.fullPath     = full;
                e.relativePath = mAssetMgr
                    ? mAssetMgr->RelPath(full.CStr())
                    : name; // fallback
                e.isDirectory  = NkDirectory::Exists(full.CStr());
                e.type         = e.isDirectory
                    ? NkAssetType::Unknown
                    : AssetManager::DetectType(name.CStr());

                // Charger thumbnail pour les textures
                if (!e.isDirectory && e.type == NkAssetType::Texture && mAssetMgr) {
                    NkTextureHandle th = mAssetMgr->GetThumbnail(e.relativePath.CStr());
                    e.thumbnailHandle = th.id;
                }

                mEntries.PushBack(e);
            }
        }

        void AssetBrowser::Render(NkUIContext& ctx,
                                  NkUIWindowManager& wm,
                                  NkUIDrawList& dl,
                                  NkUIFont& font,
                                  NkUILayoutStack& ls,
                                  NkUIRect rect) noexcept {
            NkUIWindow::SetNextWindowPos({rect.x, rect.y});
            NkUIWindow::SetNextWindowSize({rect.w, rect.h});

            if (!NkUIWindow::Begin(ctx, wm, dl, font, ls, "Assets##browser",
                                   nullptr,
                                   NkUIWindowFlags::NK_NO_MOVE |
                                   NkUIWindowFlags::NK_NO_RESIZE)) {
                NkUIWindow::End(ctx, wm, dl, ls);
                return;
            }

            // ── Barre de filtre + taille des thumbnails ───────────────────────
            NkUI::BeginRow(ctx, ls, 22.f);
            NkUI::SetNextGrow(ctx, ls);
            if (NkUI::InputText(ctx, ls, dl, font, "##assetfilter",
                                mFilterBuf, sizeof(mFilterBuf))) {
                // filtre actif — les entrées ne correspondant pas sont masquées
            }
            NkUI::SameLine(ctx, ls, 4.f);
            NkUI::SetNextWidth(ctx, ls, 80.f);
            NkUI::SliderFloat(ctx, ls, dl, font, "##thumbsize",
                              mThumbnailSize, 32.f, 128.f);
            NkUI::EndRow(ctx, ls);

            // ── Breadcrumb ────────────────────────────────────────────────────
            RenderBreadcrumb(ctx, dl, font, ls);
            NkUI::Separator(ctx, ls, dl);

            // ── Grille d'assets ───────────────────────────────────────────────
            float32 panelW   = rect.w - ctx.GetPaddingX() * 2.f;
            int32   columns  = NkMax(1, (int32)(panelW / (mThumbnailSize + 8.f)));

            if (NkUI::BeginGrid(ctx, ls, dl, font, "##assetgrid", columns)) {
                for (nk_usize i = 0; i < mEntries.Size(); ++i) {
                    const auto& e = mEntries[i];

                    // Filtre texte
                    if (mFilterBuf[0] != '\0' &&
                        !e.name.ContainsInsensitive(mFilterBuf)) continue;

                    RenderEntry(ctx, dl, font, ls, e, mThumbnailSize);
                }
                NkUI::EndGrid(ctx, ls);
            }

            NkUIWindow::End(ctx, wm, dl, ls);
        }

        void AssetBrowser::RenderBreadcrumb(NkUIContext& ctx,
                                             NkUIDrawList& dl,
                                             NkUIFont& font,
                                             NkUILayoutStack& ls) noexcept {
            NkUI::BeginRow(ctx, ls, 20.f);

            // Découper le chemin en segments
            NkString rel = mCurrentDir;
            if (rel.StartsWith(mProjectDir))
                rel = rel.Substring(mProjectDir.Length());

            // Bouton "Assets" → racine
            if (NkUI::ButtonSmall(ctx, ls, dl, font, "Assets")) {
                NavigateTo(mProjectDir.CStr());
            }

            // Segments intermédiaires
            NkString acc = mProjectDir;
            NkVector<NkString> parts;
            // Découpage simple par '/'
            NkString tmp = rel;
            while (!tmp.IsEmpty()) {
                nk_usize slash = tmp.Find('/');
                if (slash == NkString::npos) {
                    if (!tmp.IsEmpty()) parts.PushBack(tmp);
                    break;
                }
                NkString seg = tmp.Substring(0, slash);
                if (!seg.IsEmpty()) parts.PushBack(seg);
                tmp = tmp.Substring(slash + 1);
            }

            for (nk_usize i = 0; i < parts.Size(); ++i) {
                NkUI::Text(ctx, ls, dl, font, "/", NkColor{100,100,100,255});
                NkUI::SameLine(ctx, ls, 2.f);
                acc = acc + "/" + parts[i];
                NkString accCopy = acc;
                if (NkUI::ButtonSmall(ctx, ls, dl, font, parts[i].CStr())) {
                    NavigateTo(accCopy.CStr());
                }
                NkUI::SameLine(ctx, ls, 2.f);
            }

            NkUI::EndRow(ctx, ls);
        }

        void AssetBrowser::RenderEntry(NkUIContext& ctx,
                                       NkUIDrawList& dl,
                                       NkUIFont& font,
                                       NkUILayoutStack& ls,
                                       const NkAssetBrowserEntry& entry,
                                       float32 ts) noexcept {
            bool isSelected = (mSelectedPath == entry.fullPath);

            // Fond coloré si sélectionné
            if (isSelected)
                ctx.PushStyleColor(NkStyleVar::NK_STYLE_BG_ACTIVE,
                                   NkColor{60,100,160,200});

            char btnId[128];
            snprintf(btnId, sizeof(btnId), "##asset_%s", entry.name.CStr());

            bool clicked     = NkUI::InvisibleButton(ctx, ls, btnId, {ts, ts + 18.f});
            bool dblClicked  = ctx.input.mouseDblClick[0] &&
                               ctx.IsHovered({ctx.GetCursor().x, ctx.GetCursor().y, ts, ts + 18.f});

            // Dessin du thumbnail ou icône
            NkVec2 p = ctx.GetCursor();
            NkRect iconRect = {p.x, p.y - ts - 18.f, ts, ts};

            if (entry.thumbnailHandle) {
                dl.AddImage((nk_uint32)entry.thumbnailHandle, iconRect);
            } else {
                // Icône colorée par type
                NkColor iconCol =
                    entry.isDirectory    ? NkColor{220,180, 60,255} :
                    entry.type == NkAssetType::Texture  ? NkColor{ 80,180, 80,255} :
                    entry.type == NkAssetType::Mesh     ? NkColor{ 80,140,220,255} :
                    entry.type == NkAssetType::Font     ? NkColor{200, 80,200,255} :
                    entry.type == NkAssetType::Scene    ? NkColor{220,120, 60,255} :
                                                          NkColor{150,150,150,255};
                dl.AddRectFilled(iconRect, iconCol, 4.f);
            }

            // Nom du fichier (tronqué)
            NkRect nameRect = {p.x, p.y - 18.f, ts, 18.f};
            dl.AddTextWrapped(nameRect, entry.name.CStr(),
                              NkColor{210,210,210,255}, 11.f);

            if (isSelected)
                ctx.PopStyle();

            // Interactions
            if (clicked) mSelectedPath = entry.fullPath;
            if (dblClicked) {
                if (entry.isDirectory) NavigateTo(entry.fullPath.CStr());
                // Sinon : ouvrir l'asset (TODO : events)
            }
        }

    } // namespace Unkeny
} // namespace nkentseu
