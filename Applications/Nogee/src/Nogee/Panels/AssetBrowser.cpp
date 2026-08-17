#include "AssetBrowser.h"
#include "NKFileSystem/NkDirectory.h"
#include "NKFileSystem/NkPath.h"
#include "NKMath/NKMath.h"
#include "NKLogger/NkLog.h"
#include <cstring>
#include <cstdio>
#include <cctype>

using namespace nkentseu::nkui;

namespace nkentseu {
	namespace noge {

		// Init / NavigateTo / RefreshEntries / ContainsInsensitive vivent
		// desormais dans `Model/NkAssetBrowserModel.h` (logique fichiers pure,
		// partagee avec le panneau NKGui — 2026-08-17). Corps inchanges, a un
		// detail pres : le Warnf mal forme du dossier inaccessible (marqueur
		// `{}` en famille printf) a disparu avec le deplacement.

		void AssetBrowser::Render(NkUIContext &ctx, NkUIWindowManager &wm, NkUIDrawList &dl, NkUIFont &font,
								  NkUILayoutStack &ls, NkRect rect) noexcept {
			NkUIWindow::SetNextWindowPos({rect.x, rect.y});
			NkUIWindow::SetNextWindowSize({rect.w, rect.h});

			if (!NkUIWindow::Begin(ctx, wm, dl, font, ls, "Assets##browser", nullptr,
								   NkUIWindowFlags::NK_NO_MOVE | NkUIWindowFlags::NK_NO_RESIZE)) {
				NkUIWindow::End(ctx, wm, dl, ls);
				return;
			}

			// ── Barre de filtre + taille des thumbnails ───────────────────────
			NkUI::BeginRow(ctx, ls, 22.f);
			NkUI::SetNextGrow(ctx, ls);
			if (NkUI::InputText(ctx, ls, dl, font, "##assetfilter", mFilterBuf, (int32)sizeof(mFilterBuf))) {
				// filtre actif — les entrées ne correspondant pas sont masquées
			}
			NkUI::SameLine(ctx, ls, 4.f);
			NkUI::SetNextWidth(ctx, ls, 80.f);
			NkUI::SliderFloat(ctx, ls, dl, font, "##thumbsize", mThumbnailSize, 32.f, 128.f);
			NkUI::EndRow(ctx, ls);

			// ── Breadcrumb ────────────────────────────────────────────────────
			RenderBreadcrumb(ctx, dl, font, ls);
			NkUI::Separator(ctx, ls, dl);

			// ── Grille d'assets ───────────────────────────────────────────────
			float32 panelW = rect.w - ctx.GetPaddingX() * 2.f;
			int32 columns = math::NkMax(1, (int32)(panelW / (mThumbnailSize + 8.f)));

			// Budget de thumbnails paresseux pour cette frame
			mThumbBudget = THUMB_LOADS_PER_FRAME;

			if (NkUI::BeginGrid(ctx, ls, columns)) {
				for (nk_usize i = 0; i < mEntries.Size(); ++i) {
					auto &e = mEntries[i];

					// Filtre texte
					if (mFilterBuf[0] != '\0' && !ContainsInsensitive(e.name, mFilterBuf))
						continue;

					RenderEntry(ctx, dl, font, ls, e, mThumbnailSize);
				}
				NkUI::EndGrid(ctx, ls);
			}

			NkUIWindow::End(ctx, wm, dl, ls);
		}

		void AssetBrowser::RenderBreadcrumb(NkUIContext &ctx, NkUIDrawList &dl, NkUIFont &font,
											NkUILayoutStack &ls) noexcept {
			NkUI::BeginRow(ctx, ls, 20.f);

			// Découper le chemin en segments
			NkString rel = mCurrentDir;
			if (rel.StartsWith(mProjectDir.CStr()))
				rel = rel.SubStr(mProjectDir.Length());

			// Bouton "Assets" → racine
			if (NkUI::ButtonSmall(ctx, ls, dl, font, "Assets")) {
				NavigateTo(mProjectDir.CStr());
			}

			// Segments intermédiaires
			NkString acc = mProjectDir;
			NkVector<NkString> parts;
			// Découpage simple par '/'
			NkString tmp = rel;
			while (!tmp.Empty()) {
				nk_usize slash = tmp.Find('/');
				if (slash == NkString::npos) {
					if (!tmp.Empty())
						parts.PushBack(tmp);
					break;
				}
				NkString seg = tmp.SubStr(0, slash);
				if (!seg.Empty())
					parts.PushBack(seg);
				tmp = tmp.SubStr(slash + 1);
			}

			for (nk_usize i = 0; i < parts.Size(); ++i) {
				NkUI::Text(ctx, ls, dl, font, "/", NkColor{100, 100, 100, 255});
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

		void AssetBrowser::RenderEntry(NkUIContext &ctx, NkUIDrawList &dl, NkUIFont &font, NkUILayoutStack &ls,
									   NkAssetBrowserEntry &entry, float32 ts) noexcept {
			(void)font;

			// [PERF 2026-07-25] Génération paresseuse du thumbnail : uniquement
			// pour les entrées effectivement rendues, avec budget par frame.
			if (!entry.isDirectory && entry.type == NkAssetType::Texture && !entry.thumbnailTried && mAssetMgr &&
				mThumbBudget > 0) {
				--mThumbBudget;
				entry.thumbnailTried = true;
				NkTextureHandle th = mAssetMgr->GetThumbnail(entry.relativePath.CStr());
				entry.thumbnailHandle = th.id;
			}

			bool isSelected = (mSelectedPath == entry.fullPath);

			// Fond coloré si sélectionné
			if (isSelected)
				ctx.PushStyleColor(NkStyleVar::NK_BUTTON_BG, NkColor{60, 100, 160, 200});

			char btnId[192];
			snprintf(btnId, sizeof(btnId), "##asset_%s", entry.name.CStr());

			bool clicked = NkUI::InvisibleButton(ctx, ls, btnId, {ts, ts + 18.f});
			bool dblClicked =
				ctx.input.mouseDblClick[0] && ctx.IsHovered({ctx.GetCursor().x, ctx.GetCursor().y, ts, ts + 18.f});

			// Dessin du thumbnail ou icône
			NkVec2 p = ctx.GetCursor();
			NkRect iconRect = {p.x, p.y - ts - 18.f, ts, ts};

			if (entry.thumbnailHandle) {
				dl.AddImage((uint32)entry.thumbnailHandle, iconRect);
			} else {
				// Icône colorée par type
				NkColor iconCol = entry.isDirectory					   ? NkColor{220, 180, 60, 255}
								  : entry.type == NkAssetType::Texture ? NkColor{80, 180, 80, 255}
								  : entry.type == NkAssetType::Mesh	   ? NkColor{80, 140, 220, 255}
								  : entry.type == NkAssetType::Font	   ? NkColor{200, 80, 200, 255}
								  : entry.type == NkAssetType::Scene   ? NkColor{220, 120, 60, 255}
																	   : NkColor{150, 150, 150, 255};
				dl.AddRectFilled(iconRect, iconCol, 4.f);
			}

			// Nom du fichier (tronqué)
			NkRect nameRect = {p.x, p.y - 18.f, ts, 18.f};
			dl.AddTextWrapped(nameRect, entry.name.CStr(), NkColor{210, 210, 210, 255}, 11.f);

			if (isSelected)
				ctx.PopStyle();

			// Interactions
			if (clicked)
				mSelectedPath = entry.fullPath;
			if (dblClicked) {
				if (entry.isDirectory)
					NavigateTo(entry.fullPath.CStr());
				// Sinon : ouvrir l'asset (TODO : events)
			}
		}

	} // namespace noge
} // namespace nkentseu
