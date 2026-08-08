#pragma once
// =============================================================================
// NkcMetricsPanel — la campagne, et ce qu'elle repond.
//
// « Une regle n'est pas defendue par argument : elle est refutee ou confirmee
// par simulation de masse » (REGLES §1). Ce panneau lance les parties et
// presente les trois chiffres qui tranchent le palier 0 :
//
//   winrate par camp          la regle avantage-t-elle quelqu'un ?
//   victoires du siege 1      quel avantage au PREMIER joueur ? (mesure a part,
//                             grace a l'inversion des cotes une partie sur deux)
//   parties coupees           « un taux non nul signale une pathologie des
//                             regles, pas un reglage a monter » (REGLES §12.3)
//
// plus l'usage des actions (une action jamais jouee est une action a supprimer
// ou a rendre attractive — c'est ce qui a tue SAUTER, REGLES §2.1) et la
// distribution des durees de partie.
//
// Les chiffres sont affiches en TUILES et en HISTOGRAMMES, jamais en lignes de
// texte : on cherche une forme, pas une valeur.
// =============================================================================

#include "NKEditorKit/NkEditorKit.h"

#include "ConquerorLab/NkcBatch.h"
#include "ConquerorLab/NkcSession.h"
#include "ConquerorLab/NkcPlayersPanel.h"   // NkcDifficultyName
#include "ConquerorLab/NkcLabTheme.h"
#include "ConquerorLab/NkcDraw.h"

#include <cstdio>

namespace nkentseu {
	namespace conqueror {

		using namespace nkentseu::editorkit;
		using namespace nkentseu::nkgui;

		class NkcMetricsPanel : public NkEditorPanel {
			public:
				explicit NkcMetricsPanel(NkcSession *s) noexcept
					: NkEditorPanel("Metriques", NkEditorDockSide::NK_BOTTOM), mS(s) {}

				void OnUI(NkEditorFrameContext &ec) override {
					NkGuiContext &ctx = ec.Ui();
					mBatch.Poll();

					if (!mS || !mS->Ready()) {
						Text(ctx, "Aucun moteur de regles charge.");
						return;
					}

					DrawSetup(ctx);
					Separator(ctx);
					DrawProgress(ctx);
					DrawOutcome(ctx);
					DrawPlots(ctx);
				}

			private:
				// -------------------------------------------------------------
				void DrawSetup(NkGuiContext &ctx) noexcept {
					NkcModuleHost *host = mS->Host();
					const bool	   busy = mBatch.Running();

					ctx.BeginDisabled(busy);

					int32 games = static_cast<int32>(mCfg.games);
					if (DragInt(ctx, "Parties", games, 8.f, 2, 200000)) mCfg.games = static_cast<uint32>(games);

					int32 th = static_cast<int32>(mCfg.threads);
					if (InputInt(ctx, "Threads", th, 1, 1, kBatchMaxThreads))
						mCfg.threads = static_cast<uint32>(th);

					int32 budget = static_cast<int32>(mCfg.budgetMs);
					if (DragInt(ctx, "Budget IA (ms)", budget, 2.f, 1, 5000))
						mCfg.budgetMs = static_cast<uint32>(budget);

					int32 pcount = static_cast<int32>(mCfg.playerCount);
					if (InputInt(ctx, "Joueurs", pcount, 1, 2, static_cast<int32>(kMaxPlayers)))
						mCfg.playerCount = static_cast<uint8>(pcount);

					// L'inversion des cotes est le garde-fou de la mesure. On peut la
					// couper — pour observer justement l'effet du siege — mais on
					// devait pouvoir le decider, pas le subir.
					Checkbox(ctx, "Inverser les cotes une partie sur deux", mCfg.swapSides);

					const NkVector<NkcAIEntry> &ais = host ? host->Ais() : mEmpty;
					for (uint8 p = 0; p < mCfg.playerCount; ++p) {
						ctx.PushId(&mSeatIds[p]);
						char lab[48];
						std::snprintf(lab, sizeof(lab), "Camp %u", static_cast<unsigned>(p));

						const char *preview = "(aucune)";
						if (mCfg.aiModule[p] >= 0 && static_cast<usize>(mCfg.aiModule[p]) < ais.Size())
							preview = ais[static_cast<usize>(mCfg.aiModule[p])].label.CStr();
						if (BeginCombo(ctx, lab, preview, static_cast<int32>(ais.Size()))) {
							for (usize i = 0; i < ais.Size(); ++i) {
								ctx.BeginDisabled(!ais[i].Usable());
								if (Selectable(ctx, ais[i].label.CStr(),
											   mCfg.aiModule[p] == static_cast<int32>(i))) {
									mCfg.aiModule[p] = static_cast<int32>(i);
									ctx.ClosePopup();
								}
								ctx.EndDisabled();
							}
							EndCombo(ctx);
						}
						std::snprintf(lab, sizeof(lab), "Palier %u", static_cast<unsigned>(p));
						if (BeginCombo(ctx, lab, NkcDifficultyName(mCfg.diff[p]),
									   static_cast<int32>(NkcDifficulty::Count))) {
							for (int32 d = 0; d < static_cast<int32>(NkcDifficulty::Count); ++d) {
								const NkcDifficulty dd = static_cast<NkcDifficulty>(d);
								if (Selectable(ctx, NkcDifficultyName(dd), dd == mCfg.diff[p])) {
									mCfg.diff[p] = dd;
									ctx.ClosePopup();
								}
							}
							EndCombo(ctx);
						}
						ctx.PopId();
					}

					ctx.EndDisabled();

					if (!busy) {
						if (Button(ctx, "Lancer la campagne")) {
							mCfg.seed = mS->Seed();
							// La campagne joue LES REGLES COURANTES : l'instance de la
							// session sert de modele, ses parametres sont recopies dans
							// chaque worker. Regler puis lancer, dans cet ordre.
							if (!mBatch.Start(mS->Vt(), mS->RulesInst(), mS->Host(), mCfg)) mShowError = true;
							else							                                mShowError = false;
						}
					} else {
						if (Button(ctx, "Interrompre")) mBatch.Cancel();
					}

					if (mShowError && !mBatch.Message().Empty()) {
						const NkRect r = ctx.NextItemRect(0.f, ctx.ItemHeight());
						ctx.DL().AddRectFilled(r, NkcFade(NkcPalette::Error(), 0.25f), ctx.theme.rounding);
						NkcTextCenter(ctx, r, mBatch.Message().CStr(), NkcPalette::Text());
					}
				}

				// -------------------------------------------------------------
				void DrawProgress(NkGuiContext &ctx) noexcept {
					const NkcBatchStats &st = mBatch.Stats();
					const uint32 played = st.played.Load();
					char		 buf[128];
					std::snprintf(buf, sizeof(buf), "%u / %u parties", played, mBatch.Target());
					ProgressBar(ctx, mBatch.Progress(), buf);
				}

				/// Une tuile = un chiffre qui se lit d'un coup d'oeil. Le texte brut
				/// pour les memes donnees demanderait de comparer a la main.
				void Tile(NkGuiContext &ctx, const NkRect &r, const char *label, const char *value,
						  const NkColor &accent) noexcept {
					NkGuiDrawList &dl = ctx.DL();
					dl.AddRectFilled(r, NkcPalette::Panel(), ctx.theme.rounding);
					dl.AddRectFilled({r.x, r.y, ctx.S(3.f), r.h}, accent, 0.f);
					NkcText(ctx, r.x + ctx.S(12.f), r.y + ctx.S(6.f), label, NkcPalette::TextDim(),
							r.w - ctx.S(18.f));
					NkcText(ctx, r.x + ctx.S(12.f), r.y + ctx.S(8.f) + NkcLineH(ctx), value, accent,
							r.w - ctx.S(18.f));
				}

				void DrawOutcome(NkGuiContext &ctx) noexcept {
					const NkcBatchStats &st		= mBatch.Stats();
					const uint32		 played = st.played.Load();
					if (played == 0) return;

					const float32 inv = 100.f / static_cast<float32>(played);
					const int32	  n	  = static_cast<int32>(mCfg.playerCount) + 3;
					const float32 gap = ctx.S(6.f);
					const float32 h	  = NkcLineH(ctx) * 2.f + ctx.S(16.f);
					const NkRect  band = ctx.NextItemRect(0.f, h);
					const float32 tw  = (band.w - gap * static_cast<float32>(n - 1)) / static_cast<float32>(n);

					char lab[48], val[48];
					float32 x = band.x;
					for (uint8 p = 0; p < mCfg.playerCount; ++p) {
						std::snprintf(lab, sizeof(lab), "Camp %u", static_cast<unsigned>(p));
						std::snprintf(val, sizeof(val), "%.1f %%",
									  static_cast<double>(static_cast<float32>(st.wins[p].Load()) * inv));
						Tile(ctx, {x, band.y, tw, h}, lab, val, NkcPalette::Player(p));
						x += tw + gap;
					}
					std::snprintf(val, sizeof(val), "%.1f %%",
								  static_cast<double>(static_cast<float32>(st.draws.Load()) * inv));
					Tile(ctx, {x, band.y, tw, h}, "Nuls", val, NkcPalette::TextDim());
					x += tw + gap;

					// Avantage au premier siege : la question du palier 0 (REGLES §15).
					std::snprintf(val, sizeof(val), "%.1f %%",
								  static_cast<double>(static_cast<float32>(st.firstPlayerWins.Load()) * inv));
					Tile(ctx, {x, band.y, tw, h}, "Siege 1 gagne", val, NkcPalette::Accent());
					x += tw + gap;

					// Coupees par max_tours : un taux non nul est un SYMPTOME.
					const uint32 guard = st.guardStops.Load();
					std::snprintf(val, sizeof(val), "%.1f %%",
								  static_cast<double>(static_cast<float32>(guard) * inv));
					Tile(ctx, {x, band.y, tw, h}, "Coupees (max_tours)", val,
						 guard > 0 ? NkcPalette::Error() : NkcPalette::Ok());

					char buf[192];
					const uint32 moves = st.totalMoves.Load();
					std::snprintf(buf, sizeof(buf), "duree moyenne %.1f coups   —   %u coup(s) illegal(aux)",
								  static_cast<double>(static_cast<float32>(moves) /
													  static_cast<float32>(played)),
								  st.illegal.Load());
					Text(ctx, buf);
				}

				void DrawPlots(NkGuiContext &ctx) noexcept {
					const NkcBatchStats &st = mBatch.Stats();
					if (st.played.Load() == 0) return;

					// Usage des actions. Les cinq entrees de NkcMoveKind, dont
					// « None » qu'on garde visible : une valeur non nulle y serait un
					// bug du generateur de coups, pas un detail a masquer.
					for (int32 i = 0; i < 5; ++i) mAction[i] = static_cast<float32>(st.action[i].Load());
					Text(ctx, "Usage des actions  (aucune / dupliquer / fusionner / pouvoir / passer)");
					PlotHistogram(ctx, "actions", mAction, 5, 0.f, 0.f, ctx.S(70.f));

					for (int32 i = 0; i < kLenBuckets; ++i)
						mLens[i] = static_cast<float32>(st.lens[i].Load());
					Text(ctx, "Duree des parties  (une barre = 10 coups)");
					PlotHistogram(ctx, "durees", mLens, kLenBuckets, 0.f, 0.f, ctx.S(70.f));
				}

			private:
				NkcSession			 *mS = nullptr;
				NkcBatch			  mBatch;
				NkcBatchConfig		  mCfg;
				NkVector<NkcAIEntry>  mEmpty;
				uint8				  mSeatIds[kMaxPlayers] = {0, 1, 2, 3};
				bool				  mShowError			= false;
				float32				  mAction[5]			= {};
				float32				  mLens[kLenBuckets]	= {};
		};

	} // namespace conqueror
} // namespace nkentseu
