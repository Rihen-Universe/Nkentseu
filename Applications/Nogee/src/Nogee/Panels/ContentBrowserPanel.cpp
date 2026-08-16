// =============================================================================
// Nogee/Panels/ContentBrowserPanel.cpp — portage NKUI -> NKGui, vise sur §9 et
// sur la reference de cartes de NK3DModeler (cf. .h)
// =============================================================================
#include "ContentBrowserPanel.h"
#include "NKGui/NKGui.h"
#include "NKMath/NKMath.h"
#include <cstdio>
#include <cstring>

namespace nkentseu {
	namespace noge {

		using namespace nkgui;

		namespace {
			// Geometrie NK3DModeler : pied 34 px, bande de type 3 px, espacement
			// 14, ombre (+2,+3) noir alpha 90.
			constexpr float32 kFootH = 34.f;
			constexpr float32 kBandH = 3.f;
			constexpr float32 kGap = 14.f;
			constexpr NkColor kShadow{0, 0, 0, 90};
			constexpr NkColor kAccent{70, 130, 220, 255};
			constexpr NkColor kMuted{150, 150, 150, 255};
			constexpr NkColor kText{220, 220, 220, 255};
		} // namespace

		// =====================================================================
		// Point de passage unique type -> (couleur, nom). Couleurs semantiques
		// alignees sur la cible (§9 : orange=Mesh, violet=Material via Font ici
		// absent, vert=Sound/Texture...) et sur l'existant NKUI.
		// =====================================================================
		NkColor ContentBrowserPanel::AssetColor(NkAssetType t, bool isDirectory) noexcept {
			if (isDirectory)
				return {220, 180, 60, 255}; // chemise jaune
			switch (t) {
				case NkAssetType::Texture:
					return {80, 180, 80, 255};
				case NkAssetType::Mesh:
					return {235, 140, 60, 255}; // orange (cible §9)
				case NkAssetType::Font:
					return {200, 80, 200, 255};
				case NkAssetType::Scene:
					return {90, 150, 240, 255};
				default:
					return {150, 150, 150, 255};
			}
		}

		const char *ContentBrowserPanel::AssetKindName(NkAssetType t, bool isDirectory) noexcept {
			if (isDirectory)
				return "Dossier";
			switch (t) {
				case NkAssetType::Texture:
					return "Texture";
				case NkAssetType::Mesh:
					return "Mesh";
				case NkAssetType::Font:
					return "Police";
				case NkAssetType::Scene:
					return "Scene";
				default:
					return "Fichier";
			}
		}

		// =====================================================================
		// Fil d'ariane §9 : `Assets > sous > dossier`, chaque segment cliquable.
		// =====================================================================
		void ContentBrowserPanel::RenderBreadcrumb(NkGuiContext &ctx) noexcept {
			if (Button(ctx, "Assets##cb_root")) {
				NavigateTo(mProjectDir.CStr());
				return;
			}

			NkString rel = mCurrentDir;
			if (rel.StartsWith(mProjectDir.CStr()))
				rel = rel.SubStr(mProjectDir.Length());

			NkString acc = mProjectDir;
			NkString tmp = rel;
			while (!tmp.Empty()) {
				// segment jusqu'au prochain separateur
				nk_usize cut = tmp.Find('/');
				if (cut == NkString::npos)
					cut = tmp.Find('\\');
				NkString seg = (cut == NkString::npos) ? tmp : tmp.SubStr(0, cut);
				tmp = (cut == NkString::npos) ? NkString() : tmp.SubStr(cut + 1);
				if (seg.Empty())
					continue;

				ctx.SameLine();
				Text(ctx, ">");
				ctx.SameLine();

				acc = acc + "/" + seg;
				char id[192];
				std::snprintf(id, sizeof(id), "%s##cb_%s", seg.CStr(), acc.CStr());
				if (Button(ctx, id)) {
					NavigateTo(acc.CStr());
					return; // la liste vient de changer : ne pas continuer dessus
				}
			}
		}

		// =====================================================================
		// Une carte, geometrie NK3DModeler.
		// =====================================================================
		bool ContentBrowserPanel::RenderCard(NkGuiContext &ctx, NkAssetBrowserEntry &entry, const NkRect &card,
											 float32 thumb) noexcept {
			NkGuiDrawList &dl = ctx.DL();
			const NkColor typeCol = AssetColor(entry.type, entry.isDirectory);
			const bool isSelected = (mSelectedPath == entry.fullPath);

			// Vignette paresseuse (budget PARTAGE du modele, comme cote NKUI).
			if (!entry.isDirectory && entry.type == NkAssetType::Texture && !entry.thumbnailTried && mAssetMgr &&
				mThumbBudget > 0) {
				--mThumbBudget;
				entry.thumbnailTried = true;
				entry.thumbnailHandle = mAssetMgr->GetThumbnail(entry.relativePath.CStr()).id;
			}

			// Selection ACTIVE (NK3DModeler) : APLAT accent debordant de 2 px.
			if (isSelected)
				dl.AddRectFilled({card.x - 2.f, card.y - 2.f, card.w + 4.f, card.h + 4.f}, kAccent, 3.f);

			// Ombre portee (+2,+3), rayon 3 — « comme Unreal ».
			dl.AddRectFilled({card.x + 2.f, card.y + 3.f, card.w, card.h}, kShadow, 3.f);

			// Vignette : image si disponible, sinon aplat de la couleur de type
			// (le damier « fond vide » de NK3DModeler demande les jetons de theme
			// InputBg/WindowBg — non exposes par NKGui ici ; aplat en repli).
			const NkRect thumbR{card.x, card.y, card.w, thumb};
			if (entry.thumbnailHandle)
				dl.AddImage((uint32)entry.thumbnailHandle, thumbR, {0.f, 0.f}, {1.f, 1.f},
							{255, 255, 255, 255});
			else
				dl.AddRectFilled(thumbR, {(nk_uint8)(typeCol.r / 2), (nk_uint8)(typeCol.g / 2),
										  (nk_uint8)(typeCol.b / 2), 255});

			// Bande de type 3 px sous la vignette.
			dl.AddRectFilled({card.x, card.y + thumb, card.w, kBandH}, typeCol);

			// Pied 2 lignes : nom EDITABLE EN PLACE (clippe a la carte), type en
			// TextMuted. La validation recopie vers l'entree — jamais par frame :
			// une copie par frame ecraserait l'edition en cours (regle NK3DModeler,
			// payee la-bas sur le renommage des onglets).
			const float32 footY = card.y + thumb + kBandH;
			{
				char idStr[220];
				std::snprintf(idStr, sizeof(idStr), "cb_name_%s", entry.fullPath.CStr());
				std::snprintf(mNameEdit, sizeof(mNameEdit), "%s", entry.name.CStr());

				// Ligne 1 : nom. Les widgets NKGui suivent le curseur de
				// disposition — pour poser un widget a une position de carte, on
				// regle la region sur le pied, comme les widgets de NKGui le font
				// eux-memes par cellule (`NkGuiWidgets.cpp:3867`,
				// `ctx.BeginLayout(cell)`). L'appelant restaure apres la grille.
				ctx.BeginLayout({card.x, footY, card.w, 16.f});
				const bool renamed = SelectableEditable(ctx, idStr, mNameEdit,
														static_cast<int32>(sizeof(mNameEdit)), isSelected,
														!entry.isDirectory && entry.name != "..");
				if (renamed && entry.name != NkString(mNameEdit) && mNameEdit[0] != '\0') {
					// Renommage DISQUE non cable (poste AssetManager) : on rend
					// visible la demande sans pretendre l'avoir faite.
					entry.name = NkString(mNameEdit);
				}

				// Ligne 2 : type, en retrait.
				TextAt(ctx, {card.x + 2.f, footY + 17.f}, AssetKindName(entry.type, entry.isDirectory), kMuted);
			}

			// Interactions sur la vignette (le pied a les siennes).
			if (ctx.ClickIn(thumbR))
				mSelectedPath = entry.fullPath;
			const bool dbl = ctx.input.mouseDoubleClicked[0] && ctx.IsHovered(thumbR);
			return dbl;
		}

		// =====================================================================
		// Corps du panneau.
		// =====================================================================
		void ContentBrowserPanel::OnUI(editorkit::NkEditorFrameContext &ec) {
			NkGuiContext &ctx = ec.Ui();

			if (mProjectDir.Empty()) {
				Text(ctx, "Aucun projet lie (Init non appele).");
				return;
			}

			// ── §9 : recherche + slider de taille ─────────────────────────────
			InputText(ctx, "Rechercher##cb_filter", mFilterBuf, static_cast<int32>(sizeof(mFilterBuf)));
			ctx.SameLine();
			SliderFloat(ctx, "Taille##cb_thumb", mThumbnailSize, 48.f, 128.f);

			// ── §9 : fil d'ariane ─────────────────────────────────────────────
			RenderBreadcrumb(ctx);
			Separator(ctx);

			// ── Grille enveloppante, largeur de carte FIXE (NK3DModeler) ─────
			const float32 thumb = mThumbnailSize;
			const float32 cardW = thumb;
			const float32 cardH = thumb + kBandH + kFootH;
			const float32 wrapW = ctx.ContentWidth();

			mThumbBudget = THUMB_LOADS_PER_FRAME;

			// Zone reservee ligne par ligne via NextItemRect ; les cartes se
			// posent au curseur, et repartent a la ligne quand tx+tw > wrapW.
			const NkRect origin = ctx.NextItemRect(-1.f, 0.f);
			float32 tx = origin.x;
			float32 ty = origin.y + 4.f;
			float32 usedH = 4.f;

			const char *navigateTo = nullptr;
			static NkString sNavTarget; // survit a la boucle, pas a la frame

			for (nk_usize i = 0; i < mEntries.Size(); ++i) {
				NkAssetBrowserEntry &e = mEntries[i];
				if (mFilterBuf[0] != '\0' && !ContainsInsensitive(e.name, mFilterBuf))
					continue;

				if (tx + cardW > origin.x + wrapW && tx > origin.x) {
					tx = origin.x;
					ty += cardH + kGap;
					usedH += cardH + kGap;
				}

				const NkRect card{tx, ty, cardW, cardH};
				if (RenderCard(ctx, e, card, thumb)) {
					if (e.isDirectory) {
						sNavTarget = e.fullPath;
						navigateTo = sNavTarget.CStr();
					}
					// Fichier : ouvrir l'asset — meme TODO que cote NKUI.
				}
				tx += cardW + kGap;
			}

			// Restaurer la region de disposition APRES la grille (chaque pied de
			// carte l'a reglee sur lui-meme), puis reserver la hauteur consommee
			// pour que le defilement du dock suive.
			ctx.BeginLayout({origin.x, ty + cardH + kGap, wrapW, 1.0e6f});
			ctx.NextItemRect(-1.f, usedH + cardH);

			// Navigation APRES la boucle : NavigateTo() remplace mEntries, et on
			// etait en train d'iterer dessus.
			if (navigateTo)
				NavigateTo(navigateTo);
		}

	} // namespace noge
} // namespace nkentseu
