// -----------------------------------------------------------------------------
// FICHIER: UnkenyEditor/Script/NkEditeurCapturesScripts.cpp
// DESCRIPTION: Les captures HORS ECRAN des scripts (`--captures-scripts=DOSSIER`) :
//              aucune fenetre, aucune souris reelle ; la trame de banc peint
//              l'editeur et le rasteriseur de NKGui l'ecrit en PNG.
//
// AUTEUR: Rihen
// LICENCE: Proprietary - All Rights Reserved (see LICENSE)
// -----------------------------------------------------------------------------
#include "Editeur/NkEditeurActions.h"
#include "Editeur/NkEditeurBancTrame.h"
#include "Editeur/NkEditeurDocuments.h"
#include "Script/NkBpCatalogue.h"
#include "Script/NkEditeurExemplePortes.h"
#include "Script/NkEditeurGraphe.h"
#include "Script/NkEditeurScripts.h"

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
		} // namespace

		int32 NkEditeurCapturesScripts(const char *dossier) {
			memory::NkAllocator &al = memory::NkGetDefaultAllocator();
			NkDirectory::CreateRecursive(dossier);
			const char *tmp = std::getenv("TEMP");
			NkString racine = NkString(tmp != nullptr ? tmp : "/tmp") + "/unkeny_captures_scripts/";
			NkDirectory::Delete(racine.CStr(), true);
			const NkString projet = racine + "Portes/";
			NkString err;
			const NkString scene = NkEditeurEcrireExemplePortes(projet.CStr(), &err);
			if (scene.Empty()) {
				std::printf("[captures] exemple Portes : %s\n", err.CStr());
				return 1;
			}
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
			ui.cadrageEnAttente = true;
			Trames(4);

			// 01 : la page du graphe de la porte bleue.
			NkEditeurOuvrirGraphe(s, m, (projet + "Contenu/Scripts/PorteBlueprint.nkbp").CStr(), &ui);
			Trames(3);
			erreurs += Png(T, NkString::Format("%s/01_page_graphe_porte_blueprint.png", dossier).CStr()) ? 0 : 1;

			// 02 : une erreur designee sur son noeud (une force sous Tick).
			{
				graph::NkNodeGraph &g = s.Graphe().doc.graphes[0].graphe;
				const graph::NkNodeId ev = NkBpCreerNoeud(g, "bp.ev.tick", 0.f, 520.f);
				const graph::NkNodeId f = NkBpCreerNoeud(g, "bp.natif:unkeny.corps.force", 300.f, 520.f);
				NkBpPoserDefaut(g, f, "force", "0 10");
				g.Connect(ev, "suite", f, "exec");
				const NkString chemin = s.Graphe().chemin;
				s.Graphe().chemin = racine + "erreur.nkbp"; // la vraie porte reste intacte sur le disque
				NkEditeurCompilerGraphe(s, m);
				s.Graphe().chemin = chemin;
				Trames(3);
				erreurs += Png(T, NkString::Format("%s/02_graphe_erreur_sur_son_noeud.png", dossier).CStr()) ? 0 : 1;
				NkEditeurFermerDocument(m, ui, NkDoc(NkGenreDocument::NK_BLUEPRINT, s.Graphe().id));
			}

			// 03 : les Details de la Zone rouge, son script C++ et sa variable.
			{
				m.selection = ParNom(m.scene, "Zone rouge");
				m.aSelection = m.selection.IsValid();
				ui.voirTiroir = true;
				ui.ongletTiroir = 1;
				Trames(4);
				// La molette sur les Details : le bloc « Scripts » est en bas.
				for (int32 k = 0; k < 12; ++k) {
					T.pctx->input.mousePos = nkgui::NkVec2{ui.details.x + ui.details.w * 0.5f, ui.details.y + ui.details.h * 0.6f};
					T.pctx->input.AddWheelDeferred(-6.f);
					T.Trame();
				}
				T.pctx->input.mousePos = nkgui::NkVec2{-100.f, -100.f};
				Trames(2);
				erreurs += Png(T, NkString::Format("%s/03_details_scripts_zone_rouge.png", dossier).CStr()) ? 0 : 1;
			}

			// 04 : en JEU, les deux portes ouvertes (Blueprint a gauche, C++ a droite).
			{
				m.aSelection = false;
				NkEditeurJouer(m);
				for (int32 k = 0; k < 480; ++k) {
					pa->NouvelleTrame();
					pa->Poser(unkeny::NK_ACTION_AVANCER, k < 150 ? -1.f : 1.f);
					NkEditeurAvancer(m, 1.f / 60.f);
					NkEditeurScriptsTrame(s, m, &ui, 1.f / 60.f);
				}
				Trames(3);
				erreurs += Png(T, NkString::Format("%s/04_jeu_les_deux_portes_ouvertes.png", dossier).CStr()) ? 0 : 1;
			}

			// 05 : le C++ casse pendant Jouer -> l'erreur au Journal (fichier:ligne).
			{
				const NkString source = projet + "Contenu/Scripts/PorteCpp.cpp";
				NkString texte(NkEditeurSourcePorteCpp());
				const char *q = std::strstr(texte.CStr(), "etat.ouverte = true;");
				const NkString casse = NkString(texte.CStr(), static_cast<usize>(q - texte.CStr())) + "etat.ouverte = vrai;" + NkString(q + 20);
				NkFile::WriteAllText(source.CStr(), casse.CStr());
				NkEditeurScriptsReleverCpp(s, m);
				NkEditeurScriptsCompiler(s, m);
				NkEditeurScriptsSuivreCompilation(s, m, true);
				NkEditeurScriptsTrame(s, m, &ui, 1.f / 60.f);
				ui.voirTiroir = true;
				ui.ongletTiroir = 1;
				Trames(4);
				erreurs += Png(T, NkString::Format("%s/05_journal_erreur_cpp_ancienne_version_active.png", dossier).CStr()) ? 0 : 1;
				NkFile::WriteAllText(source.CStr(), NkEditeurSourcePorteCpp());
			}

			// 06 : « + Ajouter » du Contenu : Script C++ et Blueprint.
			{
				NkEditeurArreter(m);
				NkEditeurScriptsTrame(s, m, &ui, 1.f / 60.f);
				ui.ongletTiroir = 0;
				ui.contenuProjet = true;
				ui.contenuDossier = "Scripts";
				ui.contenuPerime = true;
				Trames(3);
				NkEditeurCadre c = T.Cadre();
				const nkgui::NkRect ancre{ui.tiroir.x + 8.f, ui.tiroir.y + 30.f, 90.f, 24.f};
				NkEditeurOuvrirMenu(c, NkMenuEditeur::NK_CONTENU_AJOUTER, ancre);
				Trames(2);
				erreurs += Png(T, NkString::Format("%s/06_contenu_ajouter_script_cpp_blueprint.png", dossier).CStr()) ? 0 : 1;
			}

			NkEditeurScriptsArreter(s);
			al.Delete(pt);
			al.Delete(pl);
			al.Delete(pa);
			al.Delete(ps);
			al.Delete(pm);
			return erreurs == 0 ? 0 : 1;
		}

	} // namespace editeur
} // namespace nkentseu
