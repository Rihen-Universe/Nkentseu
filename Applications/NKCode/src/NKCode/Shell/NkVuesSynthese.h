#pragma once
// =============================================================================
// NkVuesSynthese.h — DEUX VUES de l'ilot de gauche que les segments de la
// Synthese (maquette D) designent et qui n'existaient pas encore :
//
//   « Jenga »      les projets du workspace (le vrai `jenga info`) : un clic
//                  choisit la CIBLE (la meme que la barre du haut), avec son
//                  type ; construire / demarrer la cible.
//   « Extensions » les extensions de DONNEES (aujourd'hui : les jeux d'icones,
//                  Shell/NkJeuxIcones.h) : installees (Pastilles, integre) et
//                  catalogue ; Installer, Desinstaller, Utiliser — facon VS Code.
//                  Elle remplace la maquette d'avant (ScaffoldPanel « Extensions »).
// =============================================================================
#include "NKCode/Shell/NkSynthese.h"
#include "NKCode/Shell/NkSyntheseSegments.h"
#include "NKCode/Shell/NkJeuxIcones.h"

namespace nkentseu {
	namespace nkcode {

		/// Une ligne cliquable de vue (rend vrai au clic).
		inline bool NkVueLigne(NkGuiContext &ctx, const NkRect &r, bool actif) {
			const NkApparencePalette &a = NkApparenceCourante().pal;
			const NkVec2 m = ctx.input.mousePos;
			const bool hov = ctx.popupDepth == 0 && ctx.PointReachable(m) && NkGuiRectContains(r, m);
			const float32 arr = NkApparenceCourante().dispo.ilots ? ctx.S(7.f) : 0.f;
			if (actif)
				ctx.DL().AddRectFilled(r, ctx.theme.selection, arr);
			else if (hov)
				ctx.DL().AddRectFilled(r, a.survol, arr);
			return hov && ctx.input.mouseClicked[0];
		}

		// ═══════════════════════════════════════════════════════════════════════
		//  « Jenga » : les projets du workspace
		// ═══════════════════════════════════════════════════════════════════════
		class NkJengaPanel : public NkEditorPanel {
			public:
				NkJengaPanel(NkCodeState *s) : NkEditorPanel("Jenga", NkEditorDockSide::NK_LEFT), mS(s) {
					SetOpen(false);
				}

				void OnUI(NkEditorFrameContext &ec) override {
					auto &ctx = ec.Ui();
					NkSegmentsEnTete(ctx, "Jenga");
					const NkApparencePalette &a = NkApparenceCourante().pal;
					NkUi u = NkUiDe(ctx);
					const float32 S = u.S;
					const NkRect clip = ctx.DL().CurrentClip();
					float32 y = ctx.layout.cursor.y + 4.f * S;
					const float32 x = clip.x + 16.f * S, w = clip.w - 32.f * S;
					if (!mS || !mS->HasWorkspace()) {
						u.Text(x, y, "Aucun workspace Jenga ouvert.", a.fg3);
						return;
					}
					const char *ws = (mS->wsIdx >= 0 && mS->wsIdx < (int32)mS->wsNames.Size()) ? mS->wsNames[mS->wsIdx].CStr()
																								  : "Workspace";
					u.Text(x, y, ws, a.fg);
					u.Text(x + 0.6f * S, y, ws, a.fg);
					y += u.Lh();
					u.Text(x, y, NkPrintf("%d projet%s \xC2\xB7 la cible se choisit ici ou en haut", (int32)mS->projects.Size(),
										 mS->projects.Size() > 1 ? "s" : "").CStr(),
						   a.fg3);
					y += u.Lh() + 12.f * S;
					u.Text(x, y, "PROJETS", a.fg2);
					y += u.Lh() + 6.f * S;
					const float32 rh = 27.f * S;
					for (usize i = 0; i < mS->projects.Size(); ++i) {
						const NkRect r = {clip.x + 8.f * S, y, clip.w - 16.f * S, rh};
						if (r.y > clip.y + clip.h)
							break;
						const bool actif = !mS->AllProjects() && (int32)i == mS->projIdx;
						if (NkVueLigne(ctx, r, actif)) {
							mS->projIdx = (int32)i;
							ctx.input.mouseClicked[0] = false;
						}
						const NkString k = i < mS->projKinds.Size() ? mS->projKinds[i] : NkString();
						const uint32 tex = NkKindTex(mS->icons, k.CStr());
						if (tex)
							NkDrawIcon(u, tex, {r.x + 8.f * S, r.y + (rh - 16.f * S) * 0.5f, 16.f * S, 16.f * S}, a.fg2);
						u.TextV(r.x + 32.f * S, r.y, rh, mS->projects[i].CStr(), a.fg);
						if (!k.Empty()) {
							const float32 kw = u.TextW(k.CStr());
							u.TextV(r.x + r.w - 10.f * S - kw, r.y, rh, k.CStr(), a.fg3);
						}
						y += rh;
					}
					// Construire / demarrer LA cible.
					y += 10.f * S;
					const bool occupe = mS->IsBuilding();
					struct B {
							const char *t;
							int32 code;
					};
					const B bs[2] = {{"Construire", 1}, {"D\xC3\xA9marrer", 2}};
					float32 bx = x;
					for (const B &b : bs) {
						const float32 bw = u.TextW(b.t) + 24.f * S;
						const NkRect br = {bx, y, bw, 28.f * S};
						const NkVec2 m = ctx.input.mousePos;
						const bool hov = !occupe && ctx.popupDepth == 0 && NkGuiRectContains(br, m);
						u.Rect(br, b.code == 1 ? (hov ? NkMelange(a.plein, NkColor{255, 255, 255, 255}, 0.1f) : a.plein)
											   : (hov ? a.survol : a.haut),
							   7.f * S);
						u.TextV(br.x + 12.f * S, br.y, br.h, b.t, b.code == 1 ? NkColor{255, 255, 255, 255} : a.fg);
						if (hov && ctx.input.mouseClicked[0]) {
							if (b.code == 1)
								mS->DoBuildAction("build");
							else
								mS->DoRun();
							ctx.input.mouseClicked[0] = false;
						}
						bx += bw + 8.f * S;
					}
					(void)w;
				}

			private:
				NkCodeState *mS = nullptr;
		};

		// ═══════════════════════════════════════════════════════════════════════
		//  « Extensions » : les extensions de donnees (jeux d'icones)
		// ═══════════════════════════════════════════════════════════════════════
		class NkExtensionsPanel : public NkEditorPanel {
			public:
				NkExtensionsPanel(NkHomeState *home) : NkEditorPanel("Extensions", NkEditorDockSide::NK_LEFT), mH(home) {
					SetOpen(false);
				}

				void OnUI(NkEditorFrameContext &ec) override {
					auto &ctx = ec.Ui();
					NkSegmentsEnTete(ctx, "Extensions");
					const NkApparencePalette &a = NkApparenceCourante().pal;
					NkUi u = NkUiDe(ctx);
					const float32 S = u.S;
					const NkRect clip = ctx.DL().CurrentClip();
					NkJeuxIcones &J = NkCodeJeuxIcones();
					float32 y = ctx.layout.cursor.y + 4.f * S;
					const float32 x = clip.x + 16.f * S;
					u.Text(x, y, "Extensions", a.fg);
					u.Text(x + 0.6f * S, y, "Extensions", a.fg);
					y += u.Lh();
					u.Text(x, y, "Extensions de donn\xC3\xA9" "es : jeux d'ic\xC3\xB4nes", a.fg3);
					y += u.Lh() + 12.f * S;
					const char *effectif = nullptr;
					NkString eff = mH ? J.Effectif(mH->settings.jeuIcones) : NkString();
					effectif = eff.CStr();
					int32 action = 0;
					usize cible = 0;
					auto section = [&](const char *titre, bool installees) {
						u.Text(x, y, titre, a.fg2);
						y += u.Lh() + 6.f * S;
						for (usize k = 0; k < J.jeux.Size(); ++k) {
							const NkJeuIcones &j = J.jeux[k];
							if (j.installe != installees)
								continue;
							const float32 rh = 66.f * S;
							const NkRect r = {clip.x + 8.f * S, y, clip.w - 16.f * S, rh};
							const bool utilise = j.cle == NkString(effectif);
							(void)NkVueLigne(ctx, r, utilise);
							// l'icone : la piece du puzzle, teintee
							if (mH && mH->icons.puzzle)
								NkDrawIcon(u, mH->icons.puzzle, {r.x + 10.f * S, r.y + 10.f * S, 18.f * S, 18.f * S},
										   utilise ? a.accent : a.fg2);
							const float32 tx = r.x + 38.f * S;
							// Les boutons, a droite : Utiliser / Desinstaller ou Installer.
							float32 bx = r.x + r.w - 8.f * S;
							auto bouton = [&](const char *lib, int32 code, bool plein) {
								const float32 bw = u.TextW(lib) + 18.f * S;
								const NkRect br = {bx - bw, r.y + rh - 30.f * S, bw, 24.f * S};
								bx = br.x - 6.f * S;
								const NkVec2 m = ctx.input.mousePos;
								const bool hov = ctx.popupDepth == 0 && NkGuiRectContains(br, m);
								u.Rect(br, plein ? (hov ? NkMelange(a.plein, NkColor{255, 255, 255, 255}, 0.1f) : a.plein)
												 : (hov ? a.survol : a.haut),
									   6.f * S);
								u.TextV(br.x + 9.f * S, br.y, br.h, lib, plein ? NkColor{255, 255, 255, 255} : a.fg);
								if (hov && ctx.input.mouseClicked[0]) {
									action = code;
									cible = k;
									ctx.input.mouseClicked[0] = false;
								}
							};
							if (installees) {
								if (!j.integre)
									bouton("D\xC3\xA9sinstaller", 2, false);
								if (!utilise)
									bouton("Utiliser", 3, true);
								else
									u.Text(bx - u.TextW("utilis\xC3\xA9") - 4.f * S, r.y + rh - 26.f * S, "utilis\xC3\xA9", a.accent);
							} else
								bouton("Installer", 1, true);
							NkString t = j.titre;
							if (!j.version.Empty())
								t += NkString("  ") + j.version;
							u.TextEllipsis(tx, r.y + 6.f * S, r.x + r.w - 10.f * S - tx, t.CStr(), a.fg);
							u.TextEllipsis(tx, r.y + 6.f * S + u.Lh(), r.x + r.w - 10.f * S - tx, j.description.CStr(), a.fg3);
							const char *etat = j.integre ? "int\xC3\xA9gr\xC3\xA9" "e \xC2\xB7 jeu d'ic\xC3\xB4nes"
														 : (installees ? "install\xC3\xA9" "e \xC2\xB7 jeu d'ic\xC3\xB4nes"
																	   : "catalogue \xC2\xB7 jeu d'ic\xC3\xB4nes");
							u.TextEllipsis(tx, r.y + 6.f * S + 2.f * u.Lh(), bx - tx - 6.f * S, etat, a.fg3);
							y += rh + 4.f * S;
						}
						y += 10.f * S;
					};
					section("INSTALL\xC3\x89" "ES", true);
					section("CATALOGUE", false);
					u.TextEllipsis(x, y, clip.w - 32.f * S,
								   "Une extension = un dossier avec extension.cfg, d\xC3\xA9pos\xC3\xA9 dans %APPDATA%\\NKCode\\extensions\\",
								   a.fg3);
					if (action && mH && cible < J.jeux.Size()) {
						const NkString cle = J.jeux[cible].cle;
						if (action == 1)
							(void)J.Installer(cle.CStr());
						else if (action == 2) {
							(void)J.Desinstaller(cle.CStr());
							if (cle == NkString(mH->settings.jeuIcones)) { // desinstalle en cours d'usage : Pastilles
								NkStrCopy(mH->settings.jeuIcones, sizeof(mH->settings.jeuIcones), "pastilles");
								mH->settings.Save();
							}
						} else if (action == 3) {
							NkStrCopy(mH->settings.jeuIcones, sizeof(mH->settings.jeuIcones), cle.CStr());
							mH->settings.Save();
						}
					}
				}

			private:
				NkHomeState *mH = nullptr;
		};

	} // namespace nkcode
} // namespace nkentseu
