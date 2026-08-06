#pragma once
// =============================================================================
// NkcJournalPanel — la suite des coups, et le curseur de rejeu.
//
// Le journal n'est pas un decor : c'est l'outil de reproduction. « graine + liste
// de coups -> etat final identique » est une exigence non negociable du moteur
// (REGLES §17.3). Quand une partie part de travers, on copie le journal, on le
// colle dans un rapport, et le bug se rejoue a l'identique — sur une autre
// machine, avec un autre compilateur.
//
// Le rejeu RECONSTRUIT la position (Setup + N coups) au lieu de « defaire » le
// dernier coup : le contrat n'impose au moteur aucune operation inverse, et une
// annulation approximative donnerait un etat qui n'a jamais existe.
//
// L'empreinte de chaque coup est affichee : c'est elle qui permet de dire OU
// deux rejeux ont diverge, plutot que de constater qu'ils different.
// =============================================================================

#include "NKEditorKit/NkEditorKit.h"

#include "ConquerorLab/NkcSession.h"
#include "ConquerorLab/NkcLabTheme.h"
#include "ConquerorLab/NkcDraw.h"

#include <cstdio>

namespace nkentseu {
	namespace conqueror {

		using namespace nkentseu::editorkit;
		using namespace nkentseu::nkgui;

		inline const char *NkcMoveKindName(NkcMoveKind k) noexcept {
			switch (k) {
				case NkcMoveKind::Duplicate: return "DUPLIQUER";
				case NkcMoveKind::Fuse:		 return "FUSIONNER";
				case NkcMoveKind::Power:	 return "POUVOIR";
				case NkcMoveKind::Pass:		 return "PASSER";
				default:					 return "-";
			}
		}

		class NkcJournalPanel : public NkEditorPanel {
			public:
				explicit NkcJournalPanel(NkcSession *s) noexcept
					: NkEditorPanel("Journal", NkEditorDockSide::NK_BOTTOM), mS(s) {}

				void OnUI(NkEditorFrameContext &ec) override {
					NkGuiContext &ctx = ec.Ui();
					if (!mS || !mS->Ready()) {
						Text(ctx, "Aucune partie.");
						return;
					}
					const NkVector<NkcJournalEntry> &j = mS->Journal();
					const int32						n = static_cast<int32>(j.Size());

					char buf[192];
					std::snprintf(buf, sizeof(buf), "%d coups   empreinte %llu", n,
								  static_cast<unsigned long long>(mS->Hash()));
					Text(ctx, buf);

					// ---- transport ------------------------------------------
					if (Button(ctx, "Debut")) mS->SetCursor(0);
					ctx.SameLine();
					if (Button(ctx, "<")) mS->SetCursor(CursorOr(n) - 1);
					ctx.SameLine();
					if (Button(ctx, ">")) mS->SetCursor(CursorOr(n) + 1);
					ctx.SameLine();
					if (Button(ctx, "Position vivante")) mS->LiveView();
					ctx.SameLine();
					if (Button(ctx, "Copier")) CopyJournal(ctx);

					if (n > 0) {
						int32 cur = mS->Cursor() < 0 ? n - 1 : mS->Cursor();
						if (DragInt(ctx, "Coup", cur, 0.25f, 0, n - 1)) mS->SetCursor(cur);
					}

					if (mS->Cursor() >= 0) {
						const NkRect r = ctx.NextItemRect(0.f, ctx.ItemHeight());
						ctx.DL().AddRectFilled(r, NkcFade(NkcPalette::Accent(), 0.24f), ctx.theme.rounding);
						NkcTextCenter(ctx, r, "REJEU — la partie est en pause", NkcPalette::Text());
					}
					Separator(ctx);

					// ---- liste ----------------------------------------------
					for (int32 i = n - 1; i >= 0; --i) {   // le plus recent en haut
						const NkcJournalEntry &e = j[static_cast<usize>(i)];
						ctx.PushId(&j[static_cast<usize>(i)]);

						const float32 h	  = NkcLineH(ctx) + ctx.S(6.f);
						const NkRect  row = ctx.NextItemRect(0.f, h);
						NkGuiDrawList &dl = ctx.DL();

						const bool sel	   = (mS->Cursor() == i);
						const bool hovered = ctx.InputHits(row) && ctx.popupDepth == 0;
						if (sel)		  dl.AddRectFilled(row, NkcFade(NkcPalette::Accent(), 0.22f), 3.f);
						else if (hovered) dl.AddRectFilled(row, NkcPalette::ButtonHover(), 3.f);

						dl.AddCircleFilled({row.x + ctx.S(9.f), row.y + h * 0.5f}, ctx.S(5.f),
										   NkcPalette::Player(e.player));

						char line[256];
						if (e.move.kind == NkcMoveKind::Pass) {
							std::snprintf(line, sizeof(line), "%3d  %-9s %s", i + 1,
										  NkcMoveKindName(e.move.kind), e.byAi ? "(IA)" : "");
						} else {
							std::snprintf(line, sizeof(line), "%3d  %-9s (%d,%d) -> (%d,%d)%s %s", i + 1,
										  NkcMoveKindName(e.move.kind), e.move.from.q, e.move.from.r,
										  e.move.to.q, e.move.to.r,
										  e.flipped > 0 ? FlipTag(e.flipped) : "", e.byAi ? "(IA)" : "");
						}
						NkcText(ctx, row.x + ctx.S(20.f), row.y + ctx.S(3.f), line,
								sel ? NkcPalette::Text() : NkcPalette::TextDim(), row.w - ctx.S(120.f));

						std::snprintf(line, sizeof(line), "%llu",
									  static_cast<unsigned long long>(e.hash));
						NkcTextRight(ctx, {row.x, row.y, row.w - ctx.S(8.f), h}, line,
									 NkcFade(NkcPalette::TextDim(), 0.7f));

						if (ctx.ClickIn(row)) mS->SetCursor(i);
						ctx.PopId();
					}
				}

			private:
				int32 CursorOr(int32 n) const noexcept {
					return mS->Cursor() < 0 ? n - 1 : mS->Cursor();
				}

				const char *FlipTag(int16 f) noexcept {
					std::snprintf(mTag, sizeof(mTag), "  x%d", static_cast<int32>(f));
					return mTag;
				}

				/// Copie une trace REJOUABLE : graine, nombre de joueurs, empreinte
				/// finale, puis un coup par ligne. C'est le format qu'on colle dans
				/// un rapport de bug.
				void CopyJournal(NkGuiContext &ctx) noexcept {
					NkString out;
					char	 buf[256];
					std::snprintf(buf, sizeof(buf), "# ConquerorLab — graine %llu, %u joueurs\n",
								  static_cast<unsigned long long>(mS->Seed()),
								  static_cast<unsigned>(mS->PlayerCount()));
					out += buf;
					const NkVector<NkcJournalEntry> &j = mS->Journal();
					for (usize i = 0; i < j.Size(); ++i) {
						std::snprintf(buf, sizeof(buf), "%u %s %d %d %d %d %llu\n",
									  static_cast<unsigned>(j[i].player),
									  NkcMoveKindName(j[i].move.kind), j[i].move.from.q, j[i].move.from.r,
									  j[i].move.to.q, j[i].move.to.r,
									  static_cast<unsigned long long>(j[i].hash));
						out += buf;
					}
					std::snprintf(buf, sizeof(buf), "# empreinte finale %llu\n",
								  static_cast<unsigned long long>(mS->Hash()));
					out += buf;
					ctx.SetClipboard(out.CStr());
				}

			private:
				NkcSession *mS = nullptr;
				char		mTag[16] = {};
		};

	} // namespace conqueror
} // namespace nkentseu
