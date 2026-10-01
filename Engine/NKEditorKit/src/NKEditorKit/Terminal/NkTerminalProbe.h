#pragma once
// -----------------------------------------------------------------------------
// @File    NkTerminalProbe.h
// @Brief   LE BANC DU TERMINAL PARTAGE -- sans fenetre, lancable en CI.
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
//
//  Appele par NKEditorKitTest (famille 25) et par `UnkenyEditor --selftest`
//  (banc TERMINAL) -- ce dernier tourne sur la CI macOS : c'est lui qui
//  EXECUTE le moteur POSIX, que Windows ne fait que compiler (en unite vide).
//
//  QUATRE FAMILLES :
//    a  l'emulateur, octets fabriques (pur)
//    b  les parseurs : WSL, /etc/shells, ligne de commande, liens, palette (pur)
//    c  la decouverte des shells de CETTE machine
//    d  un VRAI shell : demarrer, `echo nkentseu`, lire, redimensionner, sortir,
//       et fermer SANS ORPHELIN (un petit-processus lance dans le shell doit
//       mourir avec lui)
//
//  CONTRE-EPREUVES (NK_TERMINAL_MUTATION=<nom>, posees DANS LE KIT) :
//    stdhandles  Windows : le shell herite des sorties redirigees -> d2 rouge
//    resize      la taille n'atteint plus le shell             -> d4 rouge
//    orphelins   la descendance survit a la fermeture          -> d6 rouge
//  Les survivants d'une contre-epreuve sont TUES a la fin de d6 : un banc qui
//  laisse des processus derriere lui n'est pas un banc.
// -----------------------------------------------------------------------------

#include "NKEditorKit/NkTheme.h"
#include "NKEditorKit/Terminal/NkTerminalEmulateur.h"
#include "NKEditorKit/Terminal/NkTerminalPty.h"
#include "NKEditorKit/Terminal/NkTerminalShells.h"
#include "NKEditorKit/Terminal/NkTerminalVue.h"
#include "NKTime/NkChrono.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace nkentseu {
	namespace editorkit {
		namespace terminalprobe {

			struct Bilan {
					uint32 ok = 0;
					uint32 total = 0;
			};

			inline void Note(Bilan &b, bool ok, const char *quoi) {
				++b.total;
				if (ok)
					++b.ok;
				std::printf("  [%s] %s\n", ok ? " OK " : "ECHEC", quoi);
				std::fflush(stdout);
			}

			inline void Feed(NkTerm &t, const char *s) {
				t.Feed(s, std::strlen(s));
			}

			/// Fait avancer le shell (sortie -> emulateur, reponses -> shell) jusqu'a
			/// ce que `pred` soit vrai, ou `ms` millisecondes.
			template <class F>
			inline bool Attendre(NkPty &p, NkTerm &t, F pred, int32 ms) {
				NkVector<char> b;
				for (int32 k = 0; k <= ms / 20; ++k) {
					b.Clear();
					p.Drain(b);
					if (b.Size() > 0)
						t.Feed(b.Data(), b.Size());
					b.Clear();
					t.PrendreReponses(b);
					if (b.Size() > 0)
						p.Write(b.Data(), b.Size());
					if (pred())
						return true;
					NkChrono::SleepMilliseconds(20);
				}
				return false;
			}

			/// Une ligne de l'ecran EGALE a `s` (espaces de tete et de fin ignores).
			inline bool LigneEgale(const NkTerm &t, const char *s) {
				for (usize i = 0; i < t.TotalLines(); ++i) {
					const NkString l = t.TexteLigne(i);
					const char *p = l.CStr();
					while (*p == ' ')
						++p;
					if (std::strcmp(p, s) == 0)
						return true;
				}
				return false;
			}

			/// Une ligne de l'ecran qui FINIT par `fin` et contient `aussi`.
			inline bool LigneFinissant(const NkTerm &t, const char *fin, const char *aussi) {
				const usize n = std::strlen(fin);
				for (usize i = 0; i < t.TotalLines(); ++i) {
					const NkString l = t.TexteLigne(i);
					if (l.Size() >= n && std::strcmp(l.CStr() + l.Size() - n, fin) == 0 &&
						(!aussi || std::strstr(l.CStr(), aussi)))
						return true;
				}
				return false;
			}

			// ── a. L'EMULATEUR ────────────────────────────────────────────────
			inline void FamilleEmulateur(Bilan &b) {
				std::printf("  -- a : l'emulateur VT --\n");
				{
					NkTerm t;
					Feed(t, "hello\r\nworld");
					Note(b, t.TexteLigne(0) == NkString("hello") && t.TexteLigne(1) == NkString("world"),
						 "a1 texte et fins de ligne");
				}
				{
					NkTerm t;
					Feed(t, "\x1b[31mR\x1b[1;92mG\x1b[38;5;200mX\x1b[38;2;1;2;3mY\x1b[0mZ\x1b[38:2::4:5:6mW");
					const NkTerm::Line &l = t.LineAt(0);
					const bool ok = l[0].fg == NkTermIndice(1) && l[1].fg == NkTermIndice(10) &&
									(l[1].attr & NK_TERM_GRAS) && l[2].fg == NkTermIndice(200) &&
									l[3].fg == NkTermRgb(1, 2, 3) && l[4].fg == kNkTermDefaut && l[4].attr == 0 &&
									l[5].fg == NkTermRgb(4, 5, 6);
					Note(b, ok, "a2 couleurs en INDICES (16, 256, RGB, RGB a deux-points) : la palette se resout au dessin");
				}
				{
					NkTerm t;
					Feed(t, "\x1b]0;Mon titre\x07\x1b]7;file://hote/C:/a%20b\x1b\\");
					const bool osc7 = t.Titre() == NkString("Mon titre") && t.DossierCourant() == NkString("C:/a b");
					Feed(t, "\x1b]9;9;\"C:\\x\\y\"\x1b\\");
					const bool osc99 = t.DossierCourant() == NkString("C:\\x\\y");
					Feed(t, "\x1b]633;P;Cwd=/home/rihen\x07");
					Note(b, osc7 && osc99 && t.DossierCourant() == NkString("/home/rihen"),
						 "a3 titre (OSC 0) et dossier courant (OSC 7, 9;9, 633)");
				}
				{
					NkTerm t;
					Feed(t, "ab\r\ncdef\x1b[6n\x1b[5n\x1b[c");
					NkVector<char> r;
					t.PrendreReponses(r);
					r.PushBack(0);
					Note(b, std::strcmp(r.Data(), "\x1b[2;5R\x1b[0n\x1b[?1;2c") == 0,
						 "a4 le shell demande, le terminal REPOND (DSR 6n/5n, DA)");
				}
				{
					NkTerm t;
					t.Resize(10, 3);
					char ligne[32];
					for (int32 i = 0; i < 6000; ++i) {
						std::snprintf(ligne, sizeof(ligne), "L%d\r\n", static_cast<int>(i));
						Feed(t, ligne);
					}
					// 6000 lignes + la ligne vide du curseur : l'ecran (3) garde L5998,
					// L5999 et la vide ; l'historique les 5000 precedentes.
					const bool ok = t.TotalLines() == NkTerm::kHistorique + 3 && t.TexteLigne(0) == NkString("L998") &&
									t.TexteLigne(NkTerm::kHistorique + 1) == NkString("L5999");
					Note(b, ok, "a5 historique en ANNEAU : 5000 lignes, les plus anciennes ecrasees sans deplacement");
				}
				{
					NkTerm t;
					Feed(t, "main\x1b[?1049h\x1b[2J\x1b[Halt\x1b[?1049l");
					Note(b, t.TexteLigne(0) == NkString("main") && !t.AltScreen(),
						 "a6 ecran alternatif (vim, htop) : l'ecran principal revient intact");
				}
				{
					NkTerm t;
					Feed(t, "\x1b[?1h\x1b[?2004h\x1b[5 q");
					NkVector<char> c;
					NkTerminalPreparerCollage(t, NkString("a\r\nb\n"), c);
					c.PushBack(0);
					Note(b, t.AppCursorKeys() && t.BracketedPaste() && t.CursorStyle() == NkTermCurseur::Barre &&
								std::strcmp(c.Data(), "\x1b[200~a\rb\r\x1b[201~") == 0,
						 "a7 modes : touches curseur application, collage ENCADRE (fins de ligne en CR), barre");
				}
				{
					NkTerm t;
					t.Resize(5, 3);
					Feed(t, "abcde\x1b[31mf");
					Note(b, t.TexteLigne(0) == NkString("abcde") && t.TexteLigne(1) == NkString("f"),
						 "a8 retour a la ligne DIFFERE : une couleur posee en derniere colonne ne l'annule pas");
				}
				{
					NkTerm t;
					t.Resize(20, 5);
					Feed(t, "1\r\n2\r\n3\r\n4\r\nPS>");
					t.Resize(20, 2);
					const usize n = t.TotalLines();
					Note(b, n == 5 && t.TexteLigne(n - 1) == NkString("PS>") && t.CursorLine() == n - 1,
						 "a9 l'ecran RETRECIT : l'invite reste visible, le haut part dans l'historique");
				}
				{
					NkTerm t;
					Feed(t, "\x1b(0lqk\x1b(B");
					const NkTerm::Line &l = t.LineAt(0);
					Note(b, l[0].cp == 0x250C && l[1].cp == 0x2500 && l[2].cp == 0x2510,
						 "a10 jeu DEC « dessin de lignes » (cadres de tmux, mc)");
				}
			}

			// ── b. PARSEURS, LIENS, PALETTE ───────────────────────────────────
			inline void FamilleParseurs(Bilan &b) {
				std::printf("  -- b : parseurs, liens, palette --\n");
				{
					// UTF-16LE avec BOM, comme le rend `wsl -l -q`.
					const char *noms = "Ubuntu-22.04\r\nDebian\r\n";
					NkVector<char> o;
					o.PushBack(static_cast<char>(0xFF));
					o.PushBack(static_cast<char>(0xFE));
					for (const char *p = noms; *p; ++p) {
						o.PushBack(*p);
						o.PushBack(0);
					}
					NkVector<NkString> d;
					NkTerminalLireListeWsl(o.Data(), o.Size(), d);
					Note(b, d.Size() == 2 && d[0] == NkString("Ubuntu-22.04") && d[1] == NkString("Debian"),
						 "b1 `wsl -l -q` en UTF-16 (BOM) : deux distributions");
				}
				{
					const char *msg = "Le sous-systeme Windows pour Linux n'est pas installe.\r\nwsl.exe --install\r\n";
					NkVector<char> o;
					for (const char *p = msg; *p; ++p) {
						o.PushBack(*p);
						o.PushBack(0);
					}
					NkVector<NkString> d;
					NkTerminalLireListeWsl(o.Data(), o.Size(), d);
					// « wsl.exe » serait un nom valide : c'est pourquoi l'appelant exige
					// AUSSI le code de sortie 0. Le message lui-meme ne passe pas.
					bool message = false;
					for (usize i = 0; i < d.Size(); ++i)
						message = message || d[i].Size() > 20;
					Note(b, !message, "b2 sans WSL : le MESSAGE d'erreur ne devient pas une « distribution »");
				}
				{
					NkVector<NkString> s;
					NkTerminalLireEtcShells("# commentaire\n/bin/sh\n/bin/bash\n/usr/bin/bash\n  /usr/bin/zsh  \n"
											"/usr/sbin/nologin\n/usr/bin/fish\n",
											s);
					Note(b, s.Size() == 4 && s[0] == NkString("/bin/sh") && s[1] == NkString("/bin/bash") &&
								s[2] == NkString("/usr/bin/zsh") && s[3] == NkString("/usr/bin/fish"),
						 "b3 /etc/shells : commentaires, nologin et doublons de nom ecartes");
				}
				{
					NkVector<NkString> a;
					NkTerminalDecouperCommande("\"C:\\Program Files\\Git\\bin\\bash.exe\" --login -i", a);
					Note(b, a.Size() == 3 && a[0] == NkString("C:\\Program Files\\Git\\bin\\bash.exe") &&
								a[2] == NkString("-i"),
						 "b4 ligne de commande : guillemets, antislashs de chemin Windows");
				}
				{
					NkTerm t;
					Feed(t, "src/main.cpp:12:5: error\r\nC:\\x\\y.cpp(34,7): warning\r\nvoir https://nkentseu.dev/doc.\r\nerreur:12\r\n");
					int32 c0 = 0, c1 = 0;
					NkTerminalGrilleResultat l1, l2, l3, l4;
					const bool g = NkTerminalLienSous(t, 0, 3, c0, c1, l1) && l1.lienFichier == NkString("src/main.cpp") &&
								   l1.lienLigne == 12 && l1.lienColonne == 5;
					const bool m = NkTerminalLienSous(t, 1, 2, c0, c1, l2) && l2.lienFichier == NkString("C:\\x\\y.cpp") &&
								   l2.lienLigne == 34 && l2.lienColonne == 7;
					const bool u = NkTerminalLienSous(t, 2, 10, c0, c1, l3) && l3.lienEstUrl &&
								   l3.lienFichier == NkString("https://nkentseu.dev/doc");
					const bool n = !NkTerminalLienSous(t, 3, 2, c0, c1, l4);
					Note(b, g && m && u && n, "b5 liens : fichier:ligne:col, fichier(ligne,col) MSVC, URL ; « erreur:12 » n'en est pas un");
				}
				{
					const NkTheme themes[2] = {NkTheme::Dark(), NkTheme::Light()};
					bool ok = true;
					float32 pire = 99.f;
					for (int32 k = 0; k < 2; ++k) {
						const NkTerminalPalette p = NkTerminalPaletteDuTheme(themes[k]);
						for (int32 i = 1; i < 16; ++i) {
							const float32 c = NkTerminalContraste(p.ansi[i], p.fond);
							if (c < pire)
								pire = c;
							ok = ok && c >= 3.f;
						}
						ok = ok && NkTerminalContraste(p.texte, p.fond) >= 4.5f;
					}
					char m[160];
					std::snprintf(m, sizeof(m),
								  "b6 palette accordee au theme, SOMBRE et CLAIR : 15 couleurs ANSI >= 3:1 contre le fond (pire %.2f)",
								  static_cast<double>(pire));
					Note(b, ok, m);
				}
			}

			// ── c. DECOUVERTE ────────────────────────────────────────────────
			inline void FamilleDecouverte(Bilan &b) {
				std::printf("  -- c : les shells de cette machine (moteur %s) --\n", NkPty::NomMoteur());
				NkVector<NkShellDecouvert> l;
				NkTerminalDecouvrirShells(l, true);
				for (usize i = 0; i < l.Size(); ++i)
					std::printf("       %-24s %s\n", l[i].nom.CStr(), l[i].chemin.CStr());
				if (!NkPty::Disponible()) {
					NkPty p;
					const bool refuse = !p.Start(NkString("sh"), 80, 24) && !p.Erreur().Empty();
					Note(b, l.Empty() && refuse, "c1 plateforme SANS shell systeme : liste vide, refus EXPLIQUE");
					return;
				}
				bool commandes = !l.Empty();
				for (usize i = 0; i < l.Size(); ++i)
					commandes = commandes && !l[i].commande.Empty() && !l[i].nom.Empty();
#if defined(NKENTSEU_TERMINAL_CONPTY)
				bool ps = false, cmd = false;
				for (usize i = 0; i < l.Size(); ++i) {
					ps = ps || l[i].genre == NkShellGenre::WindowsPowerShell;
					cmd = cmd || l[i].genre == NkShellGenre::Cmd;
				}
				Note(b, commandes && ps && cmd && l[0].session, "c1 Windows : Windows PowerShell et cmd trouves, le premier est le defaut");
#else
				Note(b, commandes && l[0].session, "c1 Unix : au moins un shell (le premier est celui de la session)");
#endif
			}

			// ── d. UN VRAI SHELL ─────────────────────────────────────────────
			inline void FamilleShell(Bilan &b) {
				if (!NkPty::Disponible())
					return;
				std::printf("  -- d : un vrai shell --\n");
				const NkShellDecouvert sh = NkTerminalShellDeRepli();
				NkPty p;
				NkTerm t;
				t.Resize(80, 24);
#if defined(NKENTSEU_TERMINAL_CONPTY)
				const char *tmp = std::getenv("TEMP");
				const NkString dossier = tmp ? NkString(tmp) : NkString();
#else
				const NkString dossier("/tmp");
#endif
				const bool parti = p.Start(sh.commande, 80, 24, dossier);
				Note(b, parti && p.Running() && p.Pid() > 0, "d1 demarrer le shell de repli dans un pseudo-terminal");
				if (!parti) {
					std::printf("       erreur : %s\n", p.Erreur().CStr());
					return;
				}
				p.Write("echo nkentseu\r");
				Note(b, Attendre(p, t, [&]() { return LigneEgale(t, "nkentseu"); }, 10000),
					 "d2 ecrire `echo nkentseu`, LIRE « nkentseu » (sorties de l'appelant redirigees ou non)");
#if defined(NKENTSEU_TERMINAL_CONPTY)
				{
					const bool annonce = Attendre(p, t, [&]() { return !t.DossierCourant().Empty(); }, 3000);
					NkString a = t.DossierCourant(), e = dossier;
					auto bas = [](NkString s) {
						NkString o;
						for (usize i = 0; i < s.Size(); ++i) {
							const char c = s.CStr()[i];
							o += (c >= 'A' && c <= 'Z') ? static_cast<char>(c + 32) : c;
						}
						while (o.Size() > 3 && (o.CStr()[o.Size() - 1] == '\\' || o.CStr()[o.Size() - 1] == '/'))
							o = o.SubStr(0, o.Size() - 1);
						return o;
					};
					Note(b, annonce && bas(a) == bas(e), "d3 le shell ANNONCE son dossier courant (OSC 9;9) : c'est le dossier de depart");
				}
#endif
				// Redimensionner : le SHELL doit voir la nouvelle taille.
				t.Resize(100, 30);
				p.Resize(100, 30);
#if defined(NKENTSEU_TERMINAL_CONPTY)
				p.Write("mode con\r");
				Note(b, Attendre(p, t, [&]() { return LigneFinissant(t, " 100", ":"); }, 8000),
					 "d4 redimensionner : `mode con` voit 100 colonnes (ResizePseudoConsole)");
#else
				p.Write("stty size\r");
				Note(b, Attendre(p, t, [&]() { return LigneEgale(t, "30 100"); }, 8000),
					 "d4 redimensionner : `stty size` voit 30 100 (TIOCSWINSZ + SIGWINCH)");
#endif
				// Fermer SANS ORPHELIN : un petit-processus lance dans le shell, qui
				// imprime son PID. ⚠️ Sous Windows il est DETACHE de la console
				// (Start-Process, fenetre cachee) : un processus attache a la
				// pseudo-console meurt de toute facon a sa fermeture, et le banc ne
				// mesurerait rien. Celui-ci, seul le Job le rattrape. Son parent
				// (powershell) sort aussitot : l'arbre des parents ne le retrouve
				// pas non plus -- c'est le cas reel d'un outil qui se « detache ».
#if defined(NKENTSEU_TERMINAL_CONPTY)
				p.Write("powershell -NoProfile -Command \"'NKPID='+(Start-Process ping -ArgumentList '-n','25','127.0.0.1' "
						"-WindowStyle Hidden -PassThru).Id\"\r");
#else
				p.Write("sleep 25 & echo NKPID=$!\r");
#endif
				int64 petit = 0;
				Attendre(p, t, [&]() {
					for (usize i = 0; i < t.TotalLines() && petit == 0; ++i) {
						const NkString l = t.TexteLigne(i);
						if (std::strncmp(l.CStr(), "NKPID=", 6) == 0)
							petit = static_cast<int64>(std::atoll(l.CStr() + 6));
					}
					return petit > 0;
				}, 15000);
				NkVector<int64> groupe;
				p.ProcessusDuGroupe(groupe);
				const int64 pidShell = p.Pid();
				Note(b, petit > 0 && NkPty::ProcessusVivant(petit), "d5 le shell a lance un petit-processus, vivant (son PID est lu)");
				p.Stop();
				bool vivants = false;
				for (int32 k = 0; k < 100; ++k) { // la mort d'un processus n'est pas instantanee
					vivants = NkPty::ProcessusVivant(pidShell) || NkPty::ProcessusVivant(petit);
					for (usize i = 0; i < groupe.Size(); ++i)
						vivants = vivants || NkPty::ProcessusVivant(groupe[i]);
					if (!vivants)
						break;
					NkChrono::SleepMilliseconds(20);
				}
				Note(b, !vivants && !p.Running(), "d6 fermer : le shell ET sa descendance sont morts (aucun orphelin)");
				// Le menage d'une contre-epreuve : rien ne doit survivre au banc.
				if (NkPty::ProcessusVivant(petit))
					NkPty::Tuer(petit);
				for (usize i = 0; i < groupe.Size(); ++i)
					if (NkPty::ProcessusVivant(groupe[i]))
						NkPty::Tuer(groupe[i]);
				// Un shell qui sort de lui-meme.
				{
					NkPty q;
					NkTerm u;
					const bool ok = q.Start(sh.commande, 80, 24);
					if (ok)
						q.Write("exit\r");
					Note(b, ok && Attendre(q, u, [&]() { return !q.Running(); }, 8000) && q.CodeSortie() == 0,
						 "d7 `exit` : le terminal VOIT la fin du shell (code 0)");
				}
				{
					NkPty q;
					const bool refuse = !q.Start(NkString("nk_programme_introuvable_42"), 80, 24);
					Note(b, refuse && !q.Erreur().Empty() && !q.Running(), "d8 programme introuvable : refus EXPLIQUE, rien ne tourne");
				}
			}

			/// Le banc entier. Rend le bilan ; l'appelant l'additionne au sien.
			inline Bilan Sonder() {
				Bilan b;
				FamilleEmulateur(b);
				FamilleParseurs(b);
				FamilleDecouverte(b);
				FamilleShell(b);
				return b;
			}

		} // namespace terminalprobe
	} // namespace editorkit
} // namespace nkentseu
