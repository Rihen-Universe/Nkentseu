//
// NkEditeurBancTrame.h
// =============================================================================
// Description :
//   LA VRAIE TRAME DE L'EDITEUR, SANS FENETRE, pour les bancs : un
//   NkGuiContext, une police, l'interface, les entrees du jeu, la fenetre
//   « Construire » et LE selecteur de fichiers, la souris passee par
//   NkEditeurSouris comme depuis l'OS, le clavier par NkGuiInput, le
//   presse-papiers par un tampon du banc. Tout ce qu'un clic, une frappe ou un
//   glisser fait a l'ecran se mesure ici, trame apres trame.
//
// Caracteristiques :
//   - (2026-10-01) Le harnais de `NkBancTrame` (NkEditeurBanc.cpp, e48..e56),
//     sorti dans un en-tete pour le banc de l'etape 2 d'Unreal
//     (NkEditeurBancUe5.cpp). ⚠️ NkEditeurBanc.cpp garde SA copie le temps que
//     les branches paralleles (formes, terminal) soient fusionnees : y toucher
//     maintenant ferait des conflits. Il pourra ensuite inclure celui-ci.
//   - Le presse-papiers du contexte est relie a `pressePapiers` : Ctrl+C / X / V
//     des champs se mesurent sans l'OS.
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================

#pragma once

#ifndef __NKENTSEU_UNKENYEDITOR_NKEDITEURBANCTRAME_H__
#define __NKENTSEU_UNKENYEDITOR_NKEDITEURBANCTRAME_H__

#include "Editeur/NkEditeurInterface.h"
#include "Editeur/NkEditeurSouris.h"
#include "Editeur/NkEditeurTrame.h"

#include "NKGui/Core/NkGuiFont.h"

namespace nkentseu {
	namespace editeur {

		struct NkEditeurBancTrame {
				memory::NkAllocator &alloc = memory::NkGetDefaultAllocator();
				NkEditeurModele &m;
				NkEditeurInterface *pui;
				nkgui::NkGuiContext *pctx;
				nkgui::NkGuiFont *police;
				NkEditeurEntrees *pent;
				NkEditeurConstruction *pcons;
				NkEditeurSelecteurEtat *psel;
				editorkit::NkTheme theme = editorkit::NkTheme::Dark();
				NkPaletteEditeur pal;
				NkEditeurSouris souris;
				float32 W = 1280.f, H = 760.f;
				/// Le presse-papiers du banc (Ctrl+C / X / V des champs).
				NkString pressePapiers;

				explicit NkEditeurBancTrame(NkEditeurModele &modele) : m(modele) {
					pui = alloc.New<NkEditeurInterface>();
					pctx = alloc.New<nkgui::NkGuiContext>();
					police = alloc.New<nkgui::NkGuiFont>();
					pent = alloc.New<NkEditeurEntrees>();
					pcons = alloc.New<NkEditeurConstruction>();
					psel = alloc.New<NkEditeurSelecteurEtat>();
					NkEditeurEntreesParDefaut(*pent);
					const bool policeOk = police->LoadEmbedded(NkEmbeddedFontId::DroidSans, 13.f, false);
					pctx->Init(static_cast<int32>(W), static_cast<int32>(H));
					pctx->font = policeOk ? police : nullptr;
					pal = NkEditeurPalette(theme);
					pctx->input.mousePos = nkgui::NkVec2{-100.f, -100.f};
					pctx->clipboardUser = this;
					pctx->clipboardGetFn = [](void *u, NkString &s) { s = static_cast<NkEditeurBancTrame *>(u)->pressePapiers; };
					pctx->clipboardSetFn = [](void *u, const char *t) {
						static_cast<NkEditeurBancTrame *>(u)->pressePapiers = NkString(t != nullptr ? t : "");
					};
				}
				~NkEditeurBancTrame() {
					alloc.Delete(psel);
					alloc.Delete(pcons);
					alloc.Delete(pent);
					alloc.Delete(police);
					alloc.Delete(pctx);
					alloc.Delete(pui);
				}
				NkEditeurBancTrame(const NkEditeurBancTrame &) = delete;
				NkEditeurBancTrame &operator=(const NkEditeurBancTrame &) = delete;

				NkEditeurInterface &Ui() {
					return *pui;
				}
				nkgui::NkGuiContext &Ctx() {
					return *pctx;
				}
				NkEditeurCadre Cadre() {
					return NkEditeurCadre{*pctx, m, *pui, theme, pal, police, police};
				}
				void Trame() {
					pui->dt = 1.f / 60.f;
					NkEditeurPlanifier(*pui, W, H);
					pctx->BeginFrame(1.f / 60.f);
					pctx->BeginLayout(nkgui::NkRect{0.f, 0.f, W, H});
					NkEditeurCadre c = Cadre();
					NkEditeurDessinerTrame(c, *pent, *pcons, psel);
					pctx->EndFrame();
					souris.FinDeTrame(pctx->input);
				}
				/// Un clic comme l'OS le livre : appui, une trame, relachement, une trame.
				void Clic(int32 b, float32 x, float32 y) {
					pctx->input.mousePos = nkgui::NkVec2{x, y};
					souris.Appui(pctx->input, b);
					Trame();
					souris.Relache(pctx->input, b);
					Trame();
				}
				/// Deux clics au meme endroit, en moins de 0,4 s : le double-clic que
				/// NKGui detecte lui-meme (comme deux vrais clics de l'utilisateur).
				void DoubleClic(float32 x, float32 y) {
					Clic(0, x, y);
					Clic(0, x, y);
				}
				/// Appuyer en (x0, y0), glisser jusqu'a (x1, y1) en `pas` trames, lacher.
				void Glisser(float32 x0, float32 y0, float32 x1, float32 y1, int32 pas = 4) {
					pctx->input.mousePos = nkgui::NkVec2{x0, y0};
					souris.Appui(pctx->input, 0);
					Trame();
					for (int32 k = 1; k <= pas; ++k) {
						const float32 t = static_cast<float32>(k) / static_cast<float32>(pas);
						pctx->input.mousePos = nkgui::NkVec2{x0 + (x1 - x0) * t, y0 + (y1 - y0) * t};
						Trame();
					}
					souris.Relache(pctx->input, 0);
					Trame();
				}
				/// Une touche, enfoncee une trame puis relachee, avec ses modificateurs.
				/// Ctrl+A / C / X / V posent l'intention du champ comme l'application
				/// (NkEditeurApp::OnEvent).
				void Touche(nkgui::NkGuiKey k, bool ctrl = false, bool maj = false) {
					nkgui::NkGuiInput &in = pctx->input;
					in.ctrlDown = ctrl;
					in.shiftDown = maj;
					in.SetKey(k, true);
					if (ctrl) {
						in.wantCopy = k == nkgui::NkGuiKey::C;
						in.wantCut = k == nkgui::NkGuiKey::X;
						in.wantPaste = k == nkgui::NkGuiKey::V;
						in.wantSelectAll = k == nkgui::NkGuiKey::A;
					}
					Trame();
					in.SetKey(k, false);
					in.ctrlDown = false;
					in.shiftDown = false;
					Trame();
				}
				/// Taper du texte (UTF-8), comme les NkTextInputEvent de l'OS.
				void Taper(const char *texte) {
					for (const unsigned char *q = reinterpret_cast<const unsigned char *>(texte); *q != 0u;) {
						uint32 cp = *q;
						int32 n = 1;
						if (cp >= 0xF0u) {
							cp &= 0x07u;
							n = 4;
						} else if (cp >= 0xE0u) {
							cp &= 0x0Fu;
							n = 3;
						} else if (cp >= 0xC0u) {
							cp &= 0x1Fu;
							n = 2;
						}
						for (int32 k = 1; k < n && q[k] != 0u; ++k) {
							cp = (cp << 6) | (q[k] & 0x3Fu);
						}
						pctx->input.PushChar(cp);
						q += n;
					}
					Trame();
				}
				void Fermer() {
					pui->menu = NkMenuEditeur::NK_AUCUN;
					pui->sousMenu = NkMenuEditeur::NK_AUCUN;
					pctx->input.mousePos = nkgui::NkVec2{-100.f, -100.f};
					Trame();
				}
				/// L'indice de la carte dont le chemin est `chemin` (a l'ecran), -1 sinon.
				int32 Carte(const char *chemin) {
					for (uint32 k = 0; k < pui->contenu.entries.Size() && k < pui->contenuCartes.Size(); ++k) {
						if (pui->contenu.entries[k].path == NkString(chemin) && pui->contenuCartes[k].w > 0.f) {
							return static_cast<int32>(k);
						}
					}
					return -1;
				}
				/// Cliquer la carte `chemin` (son tiers haut, la vignette).
				bool CliquerCarte(const char *chemin) {
					const int32 k = Carte(chemin);
					if (k >= 0) {
						const nkgui::NkRect r = pui->contenuCartes[static_cast<uint32>(k)];
						Clic(0, r.x + r.w * 0.5f, r.y + r.h * 0.3f);
					}
					return k >= 0;
				}
		};

	} // namespace editeur
} // namespace nkentseu

#endif // __NKENTSEU_UNKENYEDITOR_NKEDITEURBANCTRAME_H__
