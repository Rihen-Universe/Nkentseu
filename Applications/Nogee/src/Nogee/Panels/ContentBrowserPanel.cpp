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

				// ── §9 : la carte est SOURCE de glisser-deposer ───────────────
				// Poignee = le pied (seul widget interactif de la carte — la
				// vignette est du dessin brut + ClickIn, pas un widget). Type
				// "asset", charge = chemin RELATIF ENTIER : si le chemin ne tient
				// pas dans DragPayloadMax, on ne declare RIEN plutot que de
				// livrer un chemin tronque. Dossiers et ".." exclus : rien ne
				// sait les consommer aujourd'hui.
				if (!entry.isDirectory && entry.name != ".." && BeginDragSource(ctx)) {
					const char *rel = entry.relativePath.CStr();
					const int32 len = static_cast<int32>(entry.relativePath.Length());
					if (rel && len + 1 <= NkGuiContext::DragPayloadMax)
						SetDragPayload(ctx, "asset", rel, len + 1, entry.name.CStr());
					EndDragSource(ctx);
				}

				// Sonde drag-drop (--dragdrop-test) : premiere carte FICHIER —
				// rect ecran reel du pied + chemin, releves au dessin. Depuis le
				// palier A : une carte MESH visible PREND LE PAS sur une carte d'un
				// autre type deja retenue (c'est le depot mesh qui a un temoin de
				// spawn ; la sonde ecrit son TEMOIN_sonde.obj pour qu'il y en ait une).
				const bool wantThisCard = !mProbeCardValid || (!mProbeCardIsMesh && entry.type == NkAssetType::Mesh);
				if (mProbeEnabled && wantThisCard && !entry.isDirectory && entry.name != "..") {
					// ⚠️ VISIBLE seulement : une carte disposee HORS du clip courant
					// (defilee sous le pli) est reelle mais insurvolable —
					// ItemHoverable la refuse par sa porte de clip, a juste titre.
					// Paye a la premiere execution : la premiere carte fichier etait
					// sous les dossiers, hors vue -> aucun glisser ne pouvait
					// demarrer (portesPied{clipOk=0}, journal a l'appui). On ne
					// retient donc qu'un pied DANS le clip, comme un utilisateur ne
					// glisse que ce qu'il voit.
					const nkgui::NkRect &fr = ctx.lastItemRect;
					const nkgui::NkVec2 c{fr.x + fr.w * 0.5f, fr.y + fr.h * 0.5f};
					const nkgui::NkRect clip = ctx.DL().CurrentClip();
					const bool visible = (c.x >= clip.x && c.x < clip.x + clip.w && c.y >= clip.y &&
										  c.y < clip.y + clip.h);
					if (visible) {
						mProbeCardRect = fr;
						std::snprintf(mProbeCardPath, sizeof(mProbeCardPath), "%s", entry.relativePath.CStr());
						mProbeCardValid = true;
						mProbeCardIsMesh = (entry.type == NkAssetType::Mesh);
						mProbeCardTime = ctx.time; // fraicheur : la sonde compare a ui.time
						// Portes d'ItemHoverable relevees au dessin (diagnostic).
						mProbeGateHoveredWin = ctx.hoveredWindowId;
						mProbeGateCurWin = ctx.curWindowId;
						mProbeGateClipContains = visible;
					}
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
			mProbeCardValid = false; // sonde : re-mesuree a chaque image (le dock peut bouger)
			mProbeCardIsMesh = false;
			if (mProbeEnabled) {
				// Zone visible du panneau (clip courant) : c'est la que la sonde
				// pose sa molette quand aucune carte fichier n'est encore visible.
				mProbeArea = ctx.DL().CurrentClip();
				mProbeAreaValid = true;
			}

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
