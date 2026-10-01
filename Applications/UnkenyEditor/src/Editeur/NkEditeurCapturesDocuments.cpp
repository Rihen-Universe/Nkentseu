// =============================================================================
// NkEditeurCapturesDocuments.cpp — `UnkenyEditor --captures-documents=DOSSIER`
//
// Les captures HORS ECRAN (aucune fenetre, aucune souris reelle : la trame de
// banc peint l'editeur, le rasteriseur de NKGui l'ecrit en PNG) de la barre
// UNIQUE des onglets de document (NkEditeurDocuments.h) et du panneau Details :
//   01  la barre : scene, Blueprint, image, page Animation -- le Blueprint devant
//   02  un clic sur l'onglet de la scene : la scene revient, les onglets restent
//   03  l'image devant (son onglet d'asset)
//   04  la page Animation devant
//   05+ les Details (voir plus bas)
// Sur l'exemple « Portes » (un vrai projet : un Blueprint, une classe C++).
//
// AUTEUR: Rihen
// LICENCE: Proprietary - All Rights Reserved (see LICENSE)
// =============================================================================
#include "Editeur/NkEditeurActions.h"
#include "Editeur/NkEditeurAssets.h"
#include "Editeur/NkEditeurBancTrame.h"
#include "Editeur/NkEditeurDocuments.h"
#include "Editeur/NkEditeurTrame.h"
#include "Script/NkEditeurExemplePortes.h"
#include "Script/NkEditeurGraphe.h"
#include "Script/NkEditeurScripts.h"
#include "Script/NkEditeurScriptsUi.h"

#include "NKFileSystem/NkDirectory.h"
#include "NKFileSystem/NkFile.h"
#include "NKGui/Core/NkGuiDrawListRaster.h"
#include "NKImage/Core/NkImage.h"
#include "Unkeny/Entree/NkUnkenyActionsStandard.h"
#include "Unkeny/Jeu/NkUnkenyControleurs.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace nkentseu {
	namespace editeur {

		namespace {
			bool Png(NkEditeurBancTrame &T, const char *chemin) {
				memory::NkAllocator &tas = memory::NkGetDefaultAllocator();
				nkgui::NkGuiDrawListRaster *ras = tas.New<nkgui::NkGuiDrawListRaster>();
				const int32 w = static_cast<int32>(T.W), h = static_cast<int32>(T.H);
				ras->Init(w, h);
				ras->Effacer(0x141414FFu);
				ras->PoserTexture(T.police->TexId(), T.police->pixels, T.police->atlasW, T.police->atlasH, 1);
				for (uint32 k = 0; k < T.m.textures.Nombre(); ++k) {
					const uint32 id = NkTextures2D::kPremierId + k;
					int32 tw = 0, th = 0;
					if (T.m.textures.Taille(id, tw, th) && T.m.textures.Pixels(id) != nullptr) {
						ras->PoserTexture(id, T.m.textures.Pixels(id), tw, th, 4);
					}
				}
				ras->Rasteriser(T.pctx->dl);
				ras->Rasteriser(T.pctx->dlOverlay);
				NkImage img = NkImage::Wrap(const_cast<uint8 *>(ras->Pixels()), w, h, NkImagePixelFormat::NK_RGBA32);
				const bool ok = img.SavePNG(chemin);
				std::printf("  capture %s : %s\n", chemin, ok ? "ok" : "ECHEC");
				tas.Delete(ras);
				return ok;
			}

			ecs::NkEntityId ParNom(NkScene &s, const char *nom) {
				ecs::NkEntityId t = ecs::NkEntityId::Invalid();
				s.Monde().Query<NkEtiquette>().ForEach([&](ecs::NkEntityId id, NkEtiquette &e) {
					if (!t.IsValid() && std::strcmp(e.nom, nom) == 0) {
						t = id;
					}
				});
				return t;
			}

			/// Une image 64 x 64 a damier, ecrite dans le Contenu (l'onglet d'asset).
			bool EcrireDamier(const NkString &chemin) {
				static uint8 px[64 * 64 * 4];
				for (int32 y = 0; y < 64; ++y) {
					for (int32 x = 0; x < 64; ++x) {
						const bool clair = ((x / 8) + (y / 8)) % 2 == 0;
						uint8 *p = px + (y * 64 + x) * 4;
						p[0] = clair ? 240u : 60u;
						p[1] = clair ? 180u : 90u;
						p[2] = clair ? 60u : 160u;
						p[3] = 255u;
					}
				}
				NkImage img = NkImage::Wrap(px, 64, 64, NkImagePixelFormat::NK_RGBA32);
				return img.SavePNG(chemin.CStr());
			}
		} // namespace

		int32 NkEditeurCapturesDocuments(const char *dossier) {
			memory::NkAllocator &al = memory::NkGetDefaultAllocator();
			NkDirectory::CreateRecursive(dossier);
			const char *tmp = std::getenv("TEMP");
			const NkString racine = NkString(tmp != nullptr ? tmp : "/tmp") + "/unkeny_captures_documents/";
			NkDirectory::Delete(racine.CStr(), true);
			const NkString projet = racine + "Portes/";
			NkString err;
			const NkString scene = NkEditeurEcrireExemplePortes(projet.CStr(), &err);
			if (scene.Empty()) {
				std::printf("[captures] exemple Portes : %s\n", err.CStr());
				return 1;
			}
			EcrireDamier(projet + "Contenu/Damier.png");
			NkEditeurModele *pm = al.New<NkEditeurModele>();
			NkEditeurModele &m = *pm;
			NkCreerRessourcesSim(m.ressources, &m.textures, nullptr);
			NkEditeurNouvelleScene(m);
			m.chemin = scene;
			NkEditeurOuvrir(m);
			NkEditeurScripts *ps = al.New<NkEditeurScripts>();
			NkEditeurScripts &s = *ps;
			s.ouvrirTexteExterne = false;
			NkEditeurBancTrame *pt = al.New<NkEditeurBancTrame>(m);
			NkEditeurBancTrame &T = *pt;
			T.W = 1600.f;
			T.H = 900.f;
			T.pctx->Init(1600, 900);
			NkEditeurInterface &ui = T.Ui();
			ui.sonsMuets = true;
			unkeny::NkActions *pa = al.New<unkeny::NkActions>();
			unkeny::NkLiaisons *pl = al.New<unkeny::NkLiaisons>();
			unkeny::NkLiaisonsStandard(*pl);
			NkEditeurScriptsDemarrer(s, m, pa, pl);
			unkeny::NkAjouterControleurs2D(m.scene, pa);
			NkEditeurScriptsTrame(s, m, &ui, 1.f);
			NkEditeurScriptsSuivreCompilation(s, m, true);
			NkEditeurScriptsTrame(s, m, &ui, 1.f / 60.f);
			int32 erreurs = 0;
			auto Trames = [&](int32 n) {
				for (int32 k = 0; k < n; ++k) {
					T.Trame();
				}
			};
			auto Capture = [&](const char *nom) {
				erreurs += Png(T, NkString::Format("%s/%s", dossier, nom).CStr()) ? 0 : 1;
			};
			ui.cadrageEnAttente = true;
			ui.voirTiroir = true;
			ui.ongletTiroir = 0;
			ui.contenuProjet = true;
			ui.contenuDossier = "Scripts";
			ui.contenuPerime = true;
			m.selection = ParNom(m.scene, "Joueur");
			m.aSelection = m.selection.IsValid();
			Trames(4);
			NkEditeurCadre c = T.Cadre();

			// ── LA BARRE : un Blueprint (double-clic du Contenu), une image, une
			//    page Animation -- puis le Blueprint devant ──
			NkEditeurScriptOuvrirAsset(c, "Contenu/Scripts/PorteBlueprint.nkbp");
			Trames(2);
			NkEditeurOuvrirAsset(c, "Contenu/Damier.png");
			Trames(2);
			NkEditeurExecuter(c, NK_A_ANIM_ANIMATION);
			Trames(2);
			const NkDocuments &D = ui.documents;
			for (uint32 i = 0; i < D.ordre.Size(); ++i) {
				if (D.ordre[i].genre == NkGenreDocument::NK_BLUEPRINT) {
					NkEditeurActiverDocument(m, ui, D.ordre[i]);
				}
			}
			T.pctx->input.mousePos = nkgui::NkVec2{-100.f, -100.f};
			Trames(3);
			Capture("01_barre_scene_blueprint_image_animation.png");

			// ── Un clic sur l'onglet de la SCENE : elle revient ──
			T.Clic(0, ui.ongletScene.x + 24.f, ui.ongletScene.y + ui.ongletScene.h * 0.5f);
			T.pctx->input.mousePos = nkgui::NkVec2{-100.f, -100.f};
			Trames(3);
			Capture("02_retour_a_la_scene.png");

			// ── L'image, puis la page Animation, devant ──
			for (uint32 i = 0; i < D.ordre.Size(); ++i) {
				if (D.ordre[i].genre == NkGenreDocument::NK_ASSET) {
					const nkgui::NkRect r = D.rects[i];
					T.Clic(0, r.x + 30.f, r.y + r.h * 0.5f);
				}
			}
			T.pctx->input.mousePos = nkgui::NkVec2{-100.f, -100.f};
			Trames(3);
			Capture("03_image_devant.png");
			for (uint32 i = 0; i < D.ordre.Size(); ++i) {
				if (D.ordre[i].genre == NkGenreDocument::NK_ANIM) {
					const nkgui::NkRect r = D.rects[i];
					T.Clic(0, r.x + 30.f, r.y + r.h * 0.5f);
				}
			}
			T.pctx->input.mousePos = nkgui::NkVec2{-100.f, -100.f};
			Trames(3);
			Capture("04_animation_devant.png");
			NkEditeurActiverDocument(m, ui, NkDocScene());
			Trames(2);

			// ── LES DETAILS : l'en-tete fixe, les cartes qui defilent ──
			// La Zone bleue (son Blueprint), et assez de composants pour deborder.
			{
				const ecs::NkEntityId zb = ParNom(m.scene, "Zone bleue");
				m.selection = zb;
				m.aSelection = zb.IsValid();
				if (zb.IsValid()) {
					NkEditeurAjouterComposant(m, zb, NkComposantEditeur::NK_SPRITE);
					NkEditeurAjouterComposant(m, zb, NkComposantEditeur::NK_LUMIERE);
					NkEditeurAjouterComposant(m, zb, NkComposantEditeur::NK_SOURCE);
				}
				ui.ongletDroite = 0;
				ui.detailsCategorie = 0;
				ui.detailsComposant = -1;
				T.pctx->input.mousePos = nkgui::NkVec2{-100.f, -100.f};
				Trames(3);
				Capture("05_details_en_haut.png");
				// La molette SUR LES CARTES : elles defilent, l'en-tete reste.
				const nkgui::NkVec2 surCartes{ui.detailsCartes.x + ui.detailsCartes.w * 0.5f, ui.detailsCartes.y + ui.detailsCartes.h * 0.5f};
				for (int32 k = 0; k < 4; ++k) {
					T.pctx->input.mousePos = surCartes;
					T.pctx->input.AddWheelDeferred(-3.f);
					T.Trame();
				}
				T.pctx->input.mousePos = nkgui::NkVec2{-100.f, -100.f};
				Trames(2);
				Capture("06_details_apres_defilement.png");
				// Tout en bas : la carte « Scripts » (plus aucun bloc brut dessous).
				for (int32 k = 0; k < 20; ++k) {
					T.pctx->input.mousePos = surCartes;
					T.pctx->input.AddWheelDeferred(-6.f);
					T.Trame();
				}
				T.pctx->input.mousePos = nkgui::NkVec2{-100.f, -100.f};
				Trames(2);
				Capture("07_details_en_bas_carte_scripts.png");
				// La pastille « Acteur » : Hierarchie, Scripts (le comportement).
				ui.detailsCategorie = 2;
				Trames(3);
				Capture("08_details_pastille_acteur_scripts.png");
				ui.detailsCategorie = 0;
			}

			NkEditeurFermerTousOnglets(c);
			NkEditeurScriptsArreter(s);
			al.Delete(pt);
			al.Delete(pl);
			al.Delete(pa);
			al.Delete(ps);
			al.Delete(pm);
			NkDirectory::Delete(racine.CStr(), true);
			return erreurs == 0 ? 0 : 1;
		}

	} // namespace editeur
} // namespace nkentseu
