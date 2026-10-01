// -----------------------------------------------------------------------------
// FICHIER: Editeur/NkEditeurFormesUi.cpp
// DESCRIPTION: La section « Calques de collision » de l'onglet Monde et la
//              fenetre des reglages du projet (voir NkEditeurPlacer.h).
//
// (2026-10-01, R33) LES BLOCS SONT DEVENUS DES CARTES
//   Les blocs « Forme 2D » et « Collision » des Details (ecrits ici avec les
//   widgets bruts de NKGui, appeles sous les cartes) sont des CARTES de
//   NkEditeurDetails.cpp (CarteForme, CarteCollisionneur, CarteCorps) : retour
//   de Rihen, « rien hors cadre ». Il ne reste ici que les calques.
//
// AUTEUR: Rihen
// LICENCE: Proprietary - All Rights Reserved (see LICENSE)
// -----------------------------------------------------------------------------
#include "Editeur/NkEditeurPlacer.h"

#include "NKCanvas/App/NkCanvasTexte.h"
#include "NKGui/Widgets/NkGuiWidgets.h"

#include <cstdio>
#include <cstring>

namespace nkentseu {
	namespace editeur {

		using nkgui::NkGuiContext;
		using nkgui::NkRect;

		namespace {
			const char *NomCalque(const NkCalquesCollision2D &k, int32 i, char *tampon, uint32 taille) {
				if (i < 0) {
					return "(aucun)";
				}
				if (k.noms[i][0] != '\0') {
					return k.noms[i];
				}
				std::snprintf(tampon, taille, i == 0 ? "Défaut" : "Calque %d", i);
				return tampon;
			}
		} // namespace
		// =====================================================================
		namespace {
			/// Les calques : noms et matrice. `large` (la fenetre des reglages) : la
			/// ligne de la matrice porte le NOM du calque, pas seulement son numero.
			void Calques(NkEditeurCadre &c, bool large) {
				NkGuiContext &ctx = c.ctx;
				NkScene &s = c.m.scene;
				NkCalquesCollision2D &kc = s.Calques();
				ctx.PushId(large ? "calques.reglages" : "calques");
				if (!large) {
					nkgui::Separator(ctx);
					nkgui::Text(ctx, "Calques de collision (qui touche qui)");
				}
				nkgui::TextWrapped(ctx, "Chaque collisionneur a un calque (Détails > Collision). Une case vide : ces deux calques "
										"se traversent. Gardé dans la scène et dans le jeu construit.");
				nkgui::Checkbox(ctx, "les 16 calques", c.ui.calquesTous);
				const uint32 n = c.ui.calquesTous ? NK_CALQUES_COLLISION : 8u;
				// Les noms.
				for (uint32 i = 0; i < n; ++i) {
					char lib[24];
					std::snprintf(lib, sizeof(lib), "calque %u##nom%u", static_cast<unsigned>(i), static_cast<unsigned>(i));
					nkgui::InputText(ctx, lib, kc.noms[i], static_cast<int32>(sizeof(kc.noms[i])));
				}
				// La MATRICE, en triangle comme celle d'Unity : la ligne i, les colonnes
				// j <= i. Une case par paire (la matrice est symetrique).
				const float32 cel = large ? 22.f : 16.f;
				const float32 marge = large ? 132.f : 26.f;
				const float32 haut = 22.f;
				const NkRect zone = ctx.NextItemRect(0.f, haut + cel * static_cast<float32>(n) + 6.f);
				auto &dl = ctx.DL();
				const nkgui::NkGuiInput &in = ctx.input;
				bool change = false;
				for (uint32 i = 0; i < n; ++i) {
					char num[32];
					std::snprintf(num, sizeof(num), "%u", static_cast<unsigned>(i));
					char nom[32];
					const char *ligne = large ? NomCalque(kc, static_cast<int32>(i), nom, sizeof(nom)) : num;
					const NkRect lib{zone.x + 2.f, zone.y + haut + cel * static_cast<float32>(i), marge - 6.f, cel};
					dl.PushClipRect(lib, true);
					renderer::NkTexte(dl, c.petite, lib.x, lib.y + 2.f, ligne, c.pal.attenue);
					dl.PopClipRect();
					renderer::NkTexte(dl, c.petite, zone.x + marge + cel * static_cast<float32>(i) + 3.f, zone.y + 4.f, num, c.pal.attenue);
					for (uint32 j = 0; j <= i; ++j) {
						const NkRect r{zone.x + marge + cel * static_cast<float32>(j), zone.y + haut + cel * static_cast<float32>(i), cel - 2.f,
									   cel - 2.f};
						const bool oui = kc.Touche(i, j);
						const bool survol = NkEditeurDans(r, in.mousePos);
						dl.AddRectFilled(r, oui ? c.pal.accent : c.pal.champ, 2.f);
						dl.AddRect(r, survol ? c.pal.texte : c.pal.bord, 1.f, 2.f);
						if (survol && in.mouseClicked[0]) {
							NkEditeurRetenir(c.m);
							kc.Poser(i, j, !oui);
							change = true;
						}
					}
				}
				if (nkgui::Button(ctx, "Tout touche tout")) {
					for (uint32 i = 0; i < NK_CALQUES_COLLISION; ++i) {
						kc.matrice[i] = 0xFFFFu;
					}
					change = true;
				}
				if (change) {
					// Les corps naissent avec la matrice : on les refait (vitesses gardees).
					const uint32 refaits = s.AppliquerCalques();
					NkEditeurAnnoncer(c.m, NkString::Format("Calques de collision appliqués (%u corps)", refaits).CStr());
				}
				ctx.PopId();
			}
		} // namespace

		void NkEditeurSectionCalques(NkEditeurCadre &c) {
			Calques(c, false);
		}

		// =====================================================================
		void NkEditeurDessinerReglagesCollision(NkEditeurCadre &c) {
			NkEditeurInterface &ui = c.ui;
			if (!ui.reglagesCollision) {
				ui.reglagesCollisionRect = NkRect{0.f, 0.f, 0.f, 0.f};
				return;
			}
			auto &dl = c.ctx.dl;
			const float32 w = ui.ecran.w - 80.f < 600.f ? ui.ecran.w - 80.f : 600.f;
			const float32 h = ui.ecran.h - 120.f < 700.f ? ui.ecran.h - 120.f : 700.f;
			const NkRect r{ui.ecran.x + (ui.ecran.w - w) * 0.5f, ui.ecran.y + (ui.ecran.h - h) * 0.5f, w, h};
			ui.reglagesCollisionRect = r;
			const float32 enteteH = 34.f;
			dl.AddRectFilled(r, c.pal.panneau, 4.f);
			dl.AddRect(r, c.pal.accent, 1.f, 4.f);
			dl.AddRectFilled(NkRect{r.x, r.y, r.w, enteteH}, c.pal.entete, 4.f);
			renderer::NkTexte(dl, c.police, r.x + 12.f, r.y + 8.f, "Réglages du projet — calques de collision", c.pal.texte);
			// La croix (comme celle du panneau Entrees).
			const NkRect fermer{r.x + r.w - 30.f, r.y + 6.f, 22.f, 22.f};
			const bool actif = ui.menu == NkMenuEditeur::NK_AUCUN && ui.confirmation == NK_A_AUCUNE;
			if (NkEditeurBouton(c, fermer, "", false, actif)) {
				ui.reglagesCollision = false;
			}
			dl.AddLine(nkgui::NkVec2{fermer.x + 6.f, fermer.y + 6.f}, nkgui::NkVec2{fermer.x + 16.f, fermer.y + 16.f}, c.pal.texte, 1.6f);
			dl.AddLine(nkgui::NkVec2{fermer.x + 16.f, fermer.y + 6.f}, nkgui::NkVec2{fermer.x + 6.f, fermer.y + 16.f}, c.pal.texte, 1.6f);
			const NkRect corps{r.x + 8.f, r.y + enteteH + 6.f, r.w - 16.f, r.h - enteteH - 12.f};
			if (nkgui::BeginChild(c.ctx, "reglages.collision", corps, false)) {
				Calques(c, true);
				nkgui::EndChild(c.ctx);
			}
		}

	} // namespace editeur
} // namespace nkentseu
