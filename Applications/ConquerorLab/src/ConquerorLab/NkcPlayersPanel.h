#pragma once
// =============================================================================
// NkcPlayersPanel — qui tient chaque siege, et avec quelle force.
//
// Un siege est soit HUMAIN, soit un module d'IA detecte, a un palier de
// difficulte. La liste des IA vient du catalogue : quand un stagiaire depose son
// `.cpp`, son IA apparait ici sans qu'on touche a ce fichier.
//
// Le panneau montre aussi le COMPTE-RENDU de la derniere reflexion (score,
// simulations, profondeur, temps, budget depasse). C'est le seul endroit ou A2
// voit si son IA respecte son budget — et « depasser le budget fige l'interface,
// c'est un echec » (ConquerorAIABI.h).
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

		inline const char *NkcDifficultyName(NkcDifficulty d) noexcept {
			switch (d) {
				case NkcDifficulty::Easy:	return "Facile";
				case NkcDifficulty::Normal: return "Normal";
				case NkcDifficulty::Hard:	return "Difficile";
				case NkcDifficulty::Expert: return "Expert";
				case NkcDifficulty::Apex:	return "Apex";
				default:					return "?";
			}
		}

		class NkcPlayersPanel : public NkEditorPanel {
			public:
				explicit NkcPlayersPanel(NkcSession *s) noexcept
					: NkEditorPanel("Joueurs", NkEditorDockSide::NK_RIGHT), mS(s) {}

				void OnUI(NkEditorFrameContext &ec) override {
					NkGuiContext &ctx = ec.Ui();
					if (!mS || !mS->Ready()) {
						Text(ctx, "Aucun moteur de regles charge.");
						return;
					}
					NkcModuleHost *host = mS->Host();

					// ---- reglages de partie ----------------------------------
					int32 pc = mS->PlayerCount();
					if (InputInt(ctx, "Joueurs", pc, 1, 2, static_cast<int32>(kMaxPlayers))) {
						mS->SetPlayerCount(static_cast<uint8>(pc));
						mS->NewGame();
					}

					// La graine est le premier outil de reproduction d'un bug :
					// « meme graine + meme suite de coups = meme etat » (REGLES §17.3).
					int32 seed = static_cast<int32>(mS->Seed() & 0x7FFFFFFFull);
					if (DragInt(ctx, "Graine", seed, 8.f, 1, 2000000000)) {
						mS->SetSeed(static_cast<uint64>(seed));
						mS->NewGame();
					}
					ctx.SameLine();
					if (Button(ctx, "Nouvelle")) {
						mS->SetSeed(mS->Seed() * 6364136223846793005ull + 1442695040888963407ull);
						mS->NewGame();
					}

					float32 speed = mS->Speed();
					if (SliderFloat(ctx, "Cadence (s)", speed, 0.f, 2.f)) mS->Speed() = speed;

					bool autoPlay = mS->AutoPlay();
					if (Checkbox(ctx, "Lecture automatique", autoPlay)) mS->SetAutoPlay(autoPlay);
					ctx.SameLine();
					if (Button(ctx, "Un coup")) mS->StepOnce();

					Separator(ctx);

					// ---- un bloc par siege -----------------------------------
					for (uint8 p = 0; p < mS->PlayerCount(); ++p) DrawSeat(ctx, host, p);

					Separator(ctx);
					DrawLastThought(ctx);
				}

			private:
				void DrawSeat(NkGuiContext &ctx, NkcModuleHost *host, uint8 p) noexcept {
					ctx.PushId(&mSeatIds[p]);	// identite stable, independante du libelle

					NkcPlayerCfg &cfg = mS->Player(p);

					// Pastille de couleur du joueur : la meme que sur le plateau,
					// sinon il faut un aller-retour mental a chaque lecture.
					const NkRect row = ctx.NextItemRect(0.f, ctx.ItemHeight());
					ctx.DL().AddCircleFilled({row.x + ctx.S(8.f), row.y + row.h * 0.5f}, ctx.S(7.f),
											 NkcPalette::Player(p));
					char title[64];
					std::snprintf(title, sizeof(title), "Joueur %u", static_cast<unsigned>(p));
					NkcText(ctx, row.x + ctx.S(22.f), row.y + (row.h - NkcLineH(ctx)) * 0.5f, title,
							NkcPalette::Text());

					// --- pilote ---
					const NkVector<NkcAIEntry> &ais = host ? host->Ais() : mEmptyAis;
					const char *preview = "Humain";
					if (cfg.aiModule >= 0 && static_cast<usize>(cfg.aiModule) < ais.Size())
						preview = ais[static_cast<usize>(cfg.aiModule)].label.CStr();

					if (BeginCombo(ctx, "Pilote", preview, static_cast<int32>(ais.Size()) + 1)) {
						if (Selectable(ctx, "Humain", cfg.aiModule < 0)) {
							cfg.aiModule = -1;
							ctx.ClosePopup();
						}
						for (usize i = 0; i < ais.Size(); ++i) {
							// Un module casse reste VISIBLE mais non selectionnable :
							// le faire disparaitre laisserait le stagiaire croire que
							// son fichier n'a pas ete vu.
							ctx.BeginDisabled(!ais[i].Usable());
							if (Selectable(ctx, ais[i].label.CStr(), cfg.aiModule == static_cast<int32>(i))) {
								cfg.aiModule = static_cast<int32>(i);
								ctx.ClosePopup();
							}
							ctx.EndDisabled();
						}
						EndCombo(ctx);
					}

					if (cfg.aiModule >= 0) {
						if (BeginCombo(ctx, "Palier", NkcDifficultyName(cfg.diff),
									   static_cast<int32>(NkcDifficulty::Count))) {
							for (int32 d = 0; d < static_cast<int32>(NkcDifficulty::Count); ++d) {
								const NkcDifficulty dd = static_cast<NkcDifficulty>(d);
								if (Selectable(ctx, NkcDifficultyName(dd), dd == cfg.diff)) {
									cfg.diff = dd;
									ctx.ClosePopup();
								}
							}
							EndCombo(ctx);
						}
						int32 budget = static_cast<int32>(cfg.budgetMs);
						if (DragInt(ctx, "Budget (ms)", budget, 4.f, 1, 20000))
							cfg.budgetMs = static_cast<uint32>(budget);
					}

					Separator(ctx);
					ctx.PopId();
				}

				void DrawLastThought(NkGuiContext &ctx) noexcept {
					Text(ctx, "Derniere reflexion");
					if (!mS->LastAiOk()) {
						Text(ctx, "  (aucune)");
						return;
					}
					const NkcAIResult &r = mS->LastAiResult();
					char			   buf[192];
					std::snprintf(buf, sizeof(buf), "  score %d milliemes   profondeur %u", r.scoreMilli,
								  r.depthReached);
					Text(ctx, buf);
					std::snprintf(buf, sizeof(buf), "  %u simulations en %u ms%s", r.simulations, r.elapsedMs,
								  r.hitBudget ? "   (coupee par le budget)" : "");
					Text(ctx, buf);

					if (mS->IllegalCount() > 0) {
						std::snprintf(buf, sizeof(buf), "  %u coup(s) ILLEGAL(aux) proposes",
									  mS->IllegalCount());
						const NkRect r2 = ctx.NextItemRect(0.f, ctx.ItemHeight());
						ctx.DL().AddRectFilled(r2, NkcFade(NkcPalette::Error(), 0.25f), ctx.theme.rounding);
						NkcText(ctx, r2.x + ctx.S(8.f), r2.y + (r2.h - NkcLineH(ctx)) * 0.5f, buf,
								NkcPalette::Error());
					}
				}

			private:
				NkcSession				  *mS = nullptr;
				uint8					   mSeatIds[kMaxPlayers] = {0, 1, 2, 3};
				NkVector<NkcAIEntry>	   mEmptyAis;
		};

	} // namespace conqueror
} // namespace nkentseu
