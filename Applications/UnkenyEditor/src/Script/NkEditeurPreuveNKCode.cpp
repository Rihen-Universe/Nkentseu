// -----------------------------------------------------------------------------
// FICHIER: UnkenyEditor/Script/NkEditeurPreuveNKCode.cpp
// DESCRIPTION: `--preuve-nkcode=DOSSIER` -- le circuit « script C++ dans NKCode,
//              compile par Jenga, recharge a chaud dans l'editeur » DE BOUT EN
//              BOUT, sans fenetre de l'editeur et sans toucher a la souris ni au
//              clavier de personne.
//
//   1. L'exemple Portes (le meme que `--exemple=portes`), ecrit dans un dossier
//      temporaire ; l'editeur l'ouvre : compilation directe, workspace Jenga.
//   2. Le DOUBLE-CLIC sur Contenu/Scripts/PorteCpp.cpp (NkEditeurScriptOuvrirAsset,
//      la fonction meme du navigateur) : NKCode s'ouvre SUR Portes.jenga et le
//      script. Il est lance en SONDE : fenetre nee hors de l'ecran, sans focus,
//      sourde aux entrees humaines, son etat (recents, reglages) range dans
//      DOSSIER au lieu du dossier personnel. Son propre script d'evenements
//      (NK_EVENEMENTS, rejoue DANS son processus) appuie Ctrl+B : « Construire »,
//      donc `jenga build` du workspace. Il ecrit sa propre image (lecture du
//      tampon DX11, NK_CAPTURE_IMAGE) puis se ferme (NK_AGENT_EXIT).
//   3. L'editeur voit la DLL de Jenga changer, la RECHARGE A CHAUD, puis Jouer :
//      le Joueur marche a droite, la porte rouge (C++) s'ouvre.
//   Images : DOSSIER/10_nkcode_*.png (NKCode), 11_* et 12_* (l'editeur, peint
//   hors ecran). Journal : DOSSIER/preuve_nkcode.txt.
//
// AUTEUR: Rihen
// LICENCE: Proprietary - All Rights Reserved (see LICENSE)
// -----------------------------------------------------------------------------
#include "Editeur/NkEditeurActions.h"
#include "Editeur/NkEditeurBancTrame.h"
#include "Script/NkEditeurExemplePortes.h"
#include "Script/NkEditeurScripts.h"
#include "Script/NkEditeurScriptsUi.h"
#include "Script/NkEditeurWorkspaceCpp.h"

#include "NKFileSystem/NkDirectory.h"
#include "NKFileSystem/NkFile.h"
#include "NKGui/Core/NkGuiDrawListRaster.h"
#include "NKImage/Core/NkImage.h"
#include "Unkeny/Entree/NkUnkenyActionsStandard.h"
#include "Unkeny/Jeu/NkUnkenyControleurs.h"

#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace nkentseu {
	namespace editeur {

		namespace {
			FILE *gRapport = nullptr;
			void Dire(const char *format, ...) {
				char tampon[2048];
				va_list a;
				va_start(a, format);
				std::vsnprintf(tampon, sizeof(tampon), format, a);
				va_end(a);
				std::printf("%s\n", tampon);
				std::fflush(stdout);
				if (gRapport != nullptr) {
					std::fprintf(gRapport, "%s\n", tampon);
					std::fflush(gRapport);
				}
			}
			NkString Oblique(const NkString &s) {
				NkString r = s;
				char *p = const_cast<char *>(r.CStr());
				for (usize i = 0; i < r.Length(); ++i) {
					if (p[i] == '\\') {
						p[i] = '/';
					}
				}
				return r;
			}
			/// Une variable d'environnement de CE processus -- donc de NKCode, qu'il lance.
			void Poser(const char *nom, const NkString &valeur) {
#if defined(_WIN32)
				::SetEnvironmentVariableA(nom, valeur.CStr());
				_putenv_s(nom, valeur.CStr());
#else
				setenv(nom, valeur.CStr(), 1);
#endif
			}
			void Dormir(uint32 ms) {
#if defined(_WIN32)
				::Sleep(ms);
#else
				usleep(ms * 1000u);
#endif
			}
			bool Png(NkEditeurBancTrame &T, const NkString &chemin) {
				memory::NkAllocator &tas = memory::NkGetDefaultAllocator();
				nkgui::NkGuiDrawListRaster *ras = tas.New<nkgui::NkGuiDrawListRaster>();
				const int32 w = static_cast<int32>(T.W), h = static_cast<int32>(T.H);
				ras->Init(w, h);
				ras->Effacer(0x141414FFu);
				ras->PoserTexture(T.police->TexId(), T.police->pixels, T.police->atlasW, T.police->atlasH, 1);
				ras->Rasteriser(T.pctx->dl);
				ras->Rasteriser(T.pctx->dlOverlay);
				NkImage img = NkImage::Wrap(const_cast<uint8 *>(ras->Pixels()), w, h, NkImagePixelFormat::NK_RGBA32);
				const bool ok = img.SavePNG(chemin.CStr());
				Dire("  image %s : %s", chemin.CStr(), ok ? "ecrite" : "ECHEC");
				tas.Delete(ras);
				return ok;
			}
			float32 YDe(NkScene &s, const char *nom) {
				float32 y = -999.f;
				s.Monde().Query<NkEtiquette>().ForEach([&](ecs::NkEntityId id, NkEtiquette &e) {
					if (y == -999.f && std::strcmp(e.nom, nom) == 0) {
						const NkTransform2D *t = s.Monde().Get<NkTransform2D>(id);
						y = t != nullptr ? t->position.y : -999.f;
					}
				});
				return y;
			}
			bool DansJournal(const NkEditeurInterface &ui, const char *motif, uint32 depuis = 0u) {
				for (uint32 i = depuis; i < ui.journal.Size(); ++i) {
					if (std::strstr(ui.journal[i].CStr(), motif) != nullptr) {
						return true;
					}
				}
				return false;
			}
		} // namespace

		int32 NkEditeurPreuveNKCode(const char *dossierSortie) {
			memory::NkAllocator &al = memory::NkGetDefaultAllocator();
			NkString dossier = Oblique(NkString(dossierSortie != nullptr ? dossierSortie : "preuve_nkcode"));
			while (dossier.Length() > 1u && dossier.CStr()[dossier.Length() - 1u] == '/') {
				dossier = NkString(dossier.CStr(), dossier.Length() - 1u);
			}
			NkDirectory::CreateRecursive(dossier.CStr());
			gRapport = std::fopen((dossier + "/preuve_nkcode.txt").CStr(), "wb");
			Dire("UnkenyEditor -- preuve de bout en bout : script C++ -> NKCode -> Jenga -> rechargement a chaud");
			int32 fautes = 0;

			// ── 1. L'exemple Portes, ouvert comme par --exemple=portes ──────────
			const char *tmp = std::getenv("TEMP");
			const NkString racine = Oblique(NkString(tmp != nullptr ? tmp : "/tmp")) + "/unkeny_preuve_nkcode/";
			NkDirectory::Delete(racine.CStr(), true);
			const NkString projet = racine + "Portes/";
			NkString err;
			const NkString scene = NkEditeurEcrireExemplePortes(projet.CStr(), &err);
			if (scene.Empty()) {
				Dire("ECHEC : exemple Portes : %s", err.CStr());
				std::fclose(gRapport);
				return 1;
			}
			Dire("1. exemple Portes ecrit et ouvert : %s", projet.CStr());
			NkEditeurModele *pm = al.New<NkEditeurModele>();
			NkEditeurModele &m = *pm;
			NkCreerRessourcesSim(m.ressources, &m.textures, nullptr);
			NkEditeurNouvelleScene(m);
			m.chemin = scene;
			NkEditeurOuvrir(m);
			NkEditeurScripts *ps = al.New<NkEditeurScripts>();
			NkEditeurScripts &s = *ps;
			s.ouvrirTexteExterne = true; // c'est TOUT l'objet de la preuve : NKCode s'ouvre
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
			const NkString ws = s.workspace;
			const bool wsOk = !ws.Empty() && NkFile::Exists(ws.CStr());
			Dire("   compilation directe (clang++) : %s ; workspace : %s", s.etat == NkEtatCompilation::NK_REUSSIE ? "reussie" : "NON",
				 wsOk ? ws.CStr() : "ABSENT");
			fautes += wsOk ? 0 : 1;
			ui.cadrageEnAttente = true;
			for (int32 k = 0; k < 4; ++k) {
				T.Trame();
			}

			// ── 2. NKCode, en sonde hors ecran, construit par Jenga ─────────────
			const NkString exe = NkEditeurTrouverNKCode();
			if (exe.Empty()) {
				Dire("ECHEC : NKCode n'est pas construit (jenga build --target NKCode --config Debug --platform windows)");
				++fautes;
			}
			const NkString maison = dossier + "/nkcode_maison";
			NkDirectory::CreateRecursive((maison + "/.nkcode").CStr());
			NkDirectory::CreateRecursive((maison + "/AppData/Roaming").CStr());
			// Une geometrie connue : sans elle, NKCode MAXIMISE sa fenetre au premier
			// lancement -- et Windows la ramenerait sur un moniteur.
			NkFile::WriteAllText((maison + "/.nkcode/window.cfg").CStr(), "win=0|0|1440|900\nmaximized=0\n");
			const NkString imageNKCode = dossier + "/10_nkcode_workspace_portes_construit_par_jenga.png";
			NkFile::Delete(imageNKCode.CStr());
			const int32 imageCapture = 1500, imageConstruire = 480;
			Poser("NK_SONDE", NkString("1"));
			Poser("NK_SONDE_HORS_ECRAN", NkString("1"));
			Poser("NK_ETAT_SONDE", dossier + "/nkcode_etat");
			Poser("USERPROFILE", maison);
			Poser("HOME", maison);
			Poser("APPDATA", maison + "/AppData/Roaming");
			Poser("NK_EVENEMENTS", NkString::Format("%d:k:ctrl+b", imageConstruire));
			Poser("NK_CAPTURE_IMAGE", NkString::Format("%d:%s", imageCapture, imageNKCode.CStr()));
			Poser("NK_AGENT_EXIT", NkString::Format("%d", imageCapture + 10));
			const NkLancementNKCode l = NkEditeurLancementNKCode(ws.CStr(), (projet + "Contenu/Scripts/PorteCpp.cpp").CStr());
			Dire("2. double-clic sur Contenu/Scripts/PorteCpp.cpp -> %s %s", l.exe.CStr(), l.arguments.CStr());
			Dire("   dossier de travail de NKCode : %s", l.dossier.CStr());
			const uint32 debutJournal = ui.journal.Size();
			{
				NkEditeurCadre c = T.Cadre();
				NkEditeurScriptOuvrirAsset(c, "Contenu/Scripts/PorteCpp.cpp");
			}
			NkEditeurScriptsTrame(s, m, &ui, 0.f);
			const bool lance = DansJournal(ui, "NKCode ouvert sur le workspace", debutJournal);
			for (uint32 i = debutJournal; i < ui.journal.Size(); ++i) {
				Dire("   journal : %s", ui.journal[i].CStr());
			}
			fautes += lance ? 0 : 1;

			// ── 3. L'editeur attend la DLL de Jenga, la recharge ────────────────
			Dire("3. NKCode charge le workspace, Ctrl+B a son image %d (jenga build) ; l'editeur surveille %s", imageConstruire,
				 NkEditeurWorkspaceDll(projet.CStr()).CStr());
			bool recharge = false, image = false;
			float32 attente = 0.f;
			for (; attente < 150.f && lance && !(recharge && image); attente += 0.1f) {
				Dormir(100);
				const uint32 avant = ui.journal.Size();
				NkEditeurScriptsTrame(s, m, &ui, 0.1f);
				for (uint32 i = avant; i < ui.journal.Size(); ++i) {
					Dire("   journal (%.1f s) : %s", static_cast<double>(attente), ui.journal[i].CStr());
				}
				recharge = recharge || s.rechargesJenga > 0u;
				image = image || NkFile::Exists(imageNKCode.CStr());
			}
			Dire("   apres %.1f s : DLL de Jenga rechargee : %s ; module charge : %s ; image de NKCode : %s", static_cast<double>(attente),
				 recharge ? "OUI" : "NON", s.modules.Copie().CStr(), image ? "ecrite" : "ABSENTE");
			const bool copieJenga = std::strstr(s.modules.Copie().CStr(), NK_WS_SORTIE) != nullptr;
			fautes += (recharge && copieJenga) ? 0 : 1;
			fautes += image ? 0 : 1;

			// ── 4. Jouer : la DLL de Jenga ouvre la porte rouge ─────────────────
			const float32 y0 = YDe(m.scene, "Porte rouge");
			const uint32 avantJeu = ui.journal.Size();
			NkEditeurJouer(m);
			for (int32 k = 0; k < 330; ++k) {
				pa->NouvelleTrame();
				pa->Poser(unkeny::NK_ACTION_AVANCER, 1.f);
				NkEditeurAvancer(m, 1.f / 60.f);
				NkEditeurScriptsTrame(s, m, &ui, 1.f / 60.f);
			}
			const float32 dy = YDe(m.scene, "Porte rouge") - y0;
			const bool ouverte = DansJournal(ui, "La porte rouge s'ouvre (C++)", avantJeu) && dy > 0.5f;
			Dire("4. Jouer, le Joueur va a droite : porte rouge +%.2f m, « La porte rouge s'ouvre (C++) » au Journal : %s",
				 static_cast<double>(dy), ouverte ? "OUI" : "NON");
			fautes += ouverte ? 0 : 1;
			ui.voirTiroir = true;
			ui.ongletTiroir = 1;
			for (int32 k = 0; k < 4; ++k) {
				T.Trame();
			}
			fautes += Png(T, dossier + "/11_editeur_jouer_porte_cpp_ouverte_par_la_dll_de_jenga.png") ? 0 : 1;
			NkEditeurArreter(m);
			NkEditeurScriptsTrame(s, m, &ui, 1.f / 60.f);

			Dire("%s (%d faute(s))", fautes == 0 ? "PREUVE REUSSIE" : "PREUVE EN ECHEC", fautes);
			NkEditeurScriptsArreter(s);
			al.Delete(pt);
			al.Delete(pl);
			al.Delete(pa);
			al.Delete(ps);
			al.Delete(pm);
			std::fclose(gRapport);
			gRapport = nullptr;
			return fautes == 0 ? 0 : 1;
		}

	} // namespace editeur
} // namespace nkentseu
