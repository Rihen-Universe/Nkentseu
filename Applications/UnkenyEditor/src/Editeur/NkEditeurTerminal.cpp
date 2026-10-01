//
// NkEditeurTerminal.cpp
// =============================================================================
// Description :
//   L'onglet Terminal du tiroir : le panneau du kit (NkTerminalPanneau), pose
//   dans le corps du tiroir. Voir NkEditeurTerminal.h.
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================

#include "Editeur/NkEditeurTerminal.h"

#include "Editeur/NkEditeurActions.h"
#include "Editeur/NkEditeurContenu.h"
#include "Editeur/NkEditeurInterface.h"

#include "NKEditorKit/Components/NkContentBrowserDisque.h"
#include "NKEditorKit/Terminal/NkTerminalPanneau.h"
#include "NKEditorKit/Terminal/NkTerminalProbe.h"
#include "NKWindow/Core/NkLauncher.h"

#include <cstdio>

namespace nkentseu {
	namespace editeur {

		namespace {
			/// LE panneau, unique pour l'editeur. Statique : ses shells vivent tant
			/// que l'application vit, onglet du tiroir visible ou non. A la sortie,
			/// son destructeur les ferme -- et le Job du moteur les tue de toute
			/// facon si l'application meurt sans passer par la.
			editorkit::NkTerminalPanneau &Panneau() {
				static editorkit::NkTerminalPanneau p;
				return p;
			}

			nkgui::NkGuiFont gPolice;
			bool gPoliceChargee = false;

			// Les demandes de capture (NkEditeurTerminalArgument), jouees une fois.
			NkVector<NkString> gOuvrir;
			NkString gTaper;
			bool gMenu = false;
			bool gFocus = false;
			bool gDemandesJouees = false;

			NkString Minuscules(const NkString &s) {
				NkString o;
				for (usize i = 0; i < s.Size(); ++i) {
					const char ch = s.CStr()[i];
					o += (ch >= 'A' && ch <= 'Z') ? static_cast<char>(ch - 'A' + 'a') : ch;
				}
				return o;
			}

			/// Le shell que designe un mot de la ligne de commande (« powershell »,
			/// « pwsh », « cmd », « gitbash », « msys2 », « wsl », « bash »...), ou
			/// -1 s'il n'est pas sur cette machine.
			int32 ShellDuMot(const NkVector<editorkit::NkShellDecouvert> &l, const NkString &mot) {
				using editorkit::NkShellGenre;
				const NkString m = Minuscules(mot);
				struct NkMot {
						const char *mot;
						NkShellGenre genre;
				};
				static const NkMot kMots[] = {
					{"pwsh", NkShellGenre::PowerShell7}, {"powershell", NkShellGenre::WindowsPowerShell},
					{"cmd", NkShellGenre::Cmd},			 {"gitbash", NkShellGenre::GitBash},
					{"msys2", NkShellGenre::Msys2},		 {"wsl", NkShellGenre::Wsl},
					{"bash", NkShellGenre::Bash},		 {"zsh", NkShellGenre::Zsh},
					{"fish", NkShellGenre::Fish},		 {"sh", NkShellGenre::Sh},
				};
				for (const NkMot &k : kMots)
					if (m == NkString(k.mot))
						for (usize i = 0; i < l.Size(); ++i)
							if (l[i].genre == k.genre)
								return static_cast<int32>(i);
				for (usize i = 0; i < l.Size(); ++i) // sinon : une partie du nom
					if (Minuscules(l[i].nom).Find(m.CStr()) != NkString::npos)
						return static_cast<int32>(i);
				return -1;
			}

			/// Le dossier du PROJET : le parent du dossier Contenu (tant qu'il n'y
			/// a pas de projet, celui de la scene en tient lieu -- NkEditeurContenu).
			NkString DossierProjet(NkEditeurModele &m) {
				const NkString contenu = NkEditeurCheminContenuAbsolu(m, NK_CONTENU_RACINE);
				const NkString parent = editorkit::NkDisqueParent(contenu.CStr());
				return parent.Empty() ? contenu : parent;
			}

			void JouerDemandes(editorkit::NkTerminalPanneau &p) {
				if (gDemandesJouees)
					return;
				gDemandesJouees = true;
				if (!gOuvrir.Empty()) {
					p.ouvrirAuPremierAffichage = false;
					for (usize i = 0; i < gOuvrir.Size(); ++i) {
						const int32 k = ShellDuMot(p.Shells(), gOuvrir[i]);
						if (k >= 0)
							p.Ouvrir(k);
						else
							std::printf("[terminal] --terminal=%s : ce shell n'est pas sur cette machine\n", gOuvrir[i].CStr());
					}
					p.Activer(0);
				}
				if (!gTaper.Empty())
					p.Taper((gTaper + "\r").CStr());
				if (gMenu)
					p.OuvrirMenuShells();
			}
		} // namespace

		nkgui::NkGuiFont &NkEditeurTerminalPolice(float32 px) {
			if (!gPoliceChargee) {
				// ⚠️ texId DISTINCT des trois polices de la coquille (NkCanvasGuiApp.h,
				//    piege n.1) : « NKTE ».
				gPolice.texId = 0x4E4B5445u;
				gPoliceChargee = gPolice.LoadEmbedded(NkEmbeddedFontId::DejaVuSansMono, px, false) ||
								 gPolice.LoadEmbedded(NkEmbeddedFontId::Cousine, px, false);
			}
			return gPolice;
		}

		bool NkEditeurTerminalArgument(NkEditeurInterface &ui, const NkString &arg) {
			if (arg.StartsWith("--tiroir-onglet=")) {
				const NkString o(arg.SubStr(16));
				ui.ongletTiroir = (o == NkString("terminal")) ? NK_TIROIR_TERMINAL : (o == NkString("journal") ? 1 : 0);
				ui.voirTiroir = true;
				return true;
			}
			if (arg.StartsWith("--terminal=")) {
				const NkString liste(arg.SubStr(11));
				NkString mot;
				for (usize i = 0; i <= liste.Size(); ++i) {
					const char ch = i < liste.Size() ? liste.CStr()[i] : ',';
					if (ch == ',') {
						if (!mot.Empty())
							gOuvrir.PushBack(mot);
						mot = NkString();
					} else {
						mot += ch;
					}
				}
				ui.ongletTiroir = NK_TIROIR_TERMINAL;
				ui.voirTiroir = true;
				return true;
			}
			if (arg == NkString("--terminal-menu")) {
				gMenu = true;
				return true;
			}
			if (arg.StartsWith("--terminal-taper=")) {
				gTaper = NkString(arg.SubStr(17));
				return true;
			}
			if (arg == NkString("--terminal-focus")) {
				gFocus = true;
				return true;
			}
			return false;
		}

		void NkEditeurDessinerTerminal(NkEditeurCadre &c, const nkgui::NkRect &zone) {
			editorkit::NkTerminalPanneau &p = Panneau();
			p.dossierDepart = DossierProjet(c.m);
			if (gPoliceChargee && gPolice.Valid())
				p.police = &gPolice;
			JouerDemandes(p);
			p.Dessiner(c.ctx, c.ctx.dl, zone, c.theme);
			if (gFocus) {
				gFocus = false;
				p.PrendreFocus();
			}
			// Ctrl+clic sur un lien de la sortie : une URL part dans le navigateur,
			// un `fichier:ligne` s'ouvre avec l'application du systeme (l'editeur
			// n'a pas d'editeur de texte a lui).
			NkString fichier;
			int32 ligne = 0, colonne = 0;
			bool url = false;
			if (p.LienClique(fichier, ligne, colonne, url)) {
				if (url) {
					NkLauncher::OpenURL(fichier.CStr());
				} else {
					NkString chemin = fichier;
					if (!NkFile::Exists(chemin.CStr()))
						chemin = p.dossierDepart + "/" + fichier;
					NkLauncher::OpenFile(chemin.CStr());
					NkEditeurAnnoncer(c.m, NkString::Format("Terminal : %s, ligne %d", fichier.CStr(), static_cast<int>(ligne)).CStr());
				}
			}
		}

		bool NkEditeurTerminalAuClavier(NkEditeurCadre &c) {
			editorkit::NkTerminalPanneau &p = Panneau();
			p.Pomper(); // les shells avancent, onglet visible ou non
			const bool visible = c.ui.voirTiroir && c.ui.ongletTiroir == NK_TIROIR_TERMINAL;
			if (!visible) {
				p.PerdreFocus();
				return false;
			}
			return p.AFocus();
		}

		int32 NkEditeurLancerBancTerminal() {
			std::printf("\nUnkenyEditor — banc du terminal partage (NKEditorKit/Terminal, moteur %s)\n\n",
						editorkit::NkPty::NomMoteur());
			const editorkit::terminalprobe::Bilan b = editorkit::terminalprobe::Sonder();
			const uint32 echecs = b.total - b.ok;
			std::printf("\n%s : %u reussis, %u echec%s\n", echecs == 0 ? "BANC TERMINAL REUSSI" : "BANC TERMINAL EN ECHEC",
						b.ok, echecs, echecs > 1 ? "s" : "");
			return echecs == 0 ? 0 : 1;
		}

	} // namespace editeur
} // namespace nkentseu
