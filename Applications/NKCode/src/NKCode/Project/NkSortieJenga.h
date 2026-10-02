#pragma once
// =============================================================================
// NkSortieJenga.h — LA SORTIE D'UNE CONSTRUCTION, MISE EN FORME POUR LE TERMINAL
// (maquette D, que Rihen veut « exactement comme ca » le 01/10).
//
// La sortie brute de `jenga build` (bandeau ASCII, ordre de construction, un
// cadre par projet, une ligne par fichier compile) devient :
//
//   PS C:\...\Nkentseu> jenga build --config Debug
//   Jenga 2.8.8 · workspace Nkentseu · Windows x64 · clang-mingw
//     • NKCore           à jour
//     • NKEditorKit      2 fichiers compilés
//     ! NKCode           NkEmbeddedJenga.cpp, NkLsp.cpp
//         avertissement NkLsp.cpp:412:9 variable « debut » inutilisée [-Wunused-variable]
//         lien → Build/Bin/Debug-Windows/NKCode/NKCode.exe
//   Construction réussie en 7,2 s · 0 erreur · 1 avertissement
//
// ⚠️ FONCTION PURE, AJOUT SEUL : la meme entree donne les memes lignes, et une
//    ligne emise ne change plus (un projet n'est ecrit qu'a sa FIN, quand son
//    etat est connu). Le terminal peut donc nourrir son ecran au fil de l'eau
//    sans jamais reecrire ce qu'il a deja affiche.
// ⚠️ La sortie BRUTE reste entiere dans le panneau Sortie : rien n'est perdu.
// =============================================================================
#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/String/NkString.h"
#include "NKContainers/String/NkFormat.h"
#include "NKCode/Project/NkDiagParse.h" // NkParseDiagLine : un avertissement clang/gcc/MSVC
#include <cstdio>

namespace nkentseu {
	namespace nkcode {

		struct NkSortieJengaEtat {
				NkVector<NkString> lignes; ///< lignes VT (couleurs ANSI), sans fin de ligne
				bool fini = false;
				bool reussi = false;
				int32 erreurs = 0, avertissements = 0;
		};

		namespace sortie_detail {
			/// Retire les sequences ANSI (ESC [ ... lettre) et les caracteres de cadre.
			inline NkString Nettoyer(const NkString &s) {
				NkString o;
				for (const char *c = s.CStr(); *c;) {
					if ((unsigned char)*c == 0x1b) {
						++c;
						if (*c == '[')
							++c;
						while (*c && !(*c >= '@' && *c <= '~'))
							++c;
						if (*c)
							++c;
						continue;
					}
					o += *c;
					++c;
				}
				return o;
			}
			inline bool Commence(const char *s, const char *p) {
				while (*p)
					if (*s++ != *p++)
						return false;
				return true;
			}
			inline const char *Trouve(const char *s, const char *p) {
				for (; *s; ++s)
					if (Commence(s, p))
						return s;
				return nullptr;
			}
			inline NkString Trim(const char *s) {
				while (*s == ' ' || *s == '\t')
					++s;
				NkString r(s);
				while (r.Size() && (r[r.Size() - 1] == ' ' || r[r.Size() - 1] == '\r' || r[r.Size() - 1] == '\t'))
					r = r.SubStr(0, r.Size() - 1);
				return r;
			}
			/// Saute les octets UTF-8 non ASCII de tete (« ✓ », « ℹ », « │ », « ║ »...).
			inline const char *SauterSymboles(const char *s) {
				while (*s && (((unsigned char)*s) >= 0x80 || *s == ' '))
					++s;
				return s;
			}
			inline NkString NomFichier(const NkString &p) {
				const char *s = p.CStr();
				const char *d = s;
				for (const char *c = s; *c; ++c)
					if (*c == '/' || *c == '\\')
						d = c + 1;
				return NkString(d);
			}
			inline NkString Barres(const NkString &p) { // chemins raccourcis, barres '/'
				NkString r;
				for (usize i = 0; i < p.Size(); ++i)
					r += p[i] == '\\' ? '/' : p[i];
				return r;
			}
			inline NkString Bourrer(const NkString &s, int32 n) {
				NkString r = s;
				int32 l = 0;
				for (const char *c = s.CStr(); *c; ++c)
					if (((unsigned char)*c & 0xC0) != 0x80)
						++l;
				for (; l < n; ++l)
					r += ' ';
				return r;
			}
			/// Les CADRES de Jenga (« ║ ... ║ ») coupent les longues lignes a la largeur
			/// du cadre : un segment PLEIN (sans espace avant la bordure) continue sur
			/// la ligne suivante. On recolle donc les lignes logiques avant de lire.
			inline NkVector<NkString> LignesLogiques(const NkVector<NkString> &brut) {
				NkVector<NkString> out;
				NkString cumul;
				bool enCours = false;
				for (usize i = 0; i < brut.Size(); ++i) {
					const NkString ln = Nettoyer(brut[i]);
					const char *s = ln.CStr();
					// une ligne de cadre : commence par « ║ » (E2 95 91) et finit par « ║ »
					const bool cadre = (unsigned char)s[0] == 0xE2 && (unsigned char)s[1] == 0x95 &&
									   (unsigned char)s[2] == 0x91;
					if (!cadre) {
						if (enCours) {
							out.PushBack(cumul);
							cumul.Clear();
							enCours = false;
						}
						out.PushBack(ln);
						continue;
					}
					// Le contenu entre les deux bordures, sans la marge d'UN espace de
					// chaque cote. Un segment PLEIN (coupe par le cadre) n'a, au plus, qu'un
					// espace final -- celui du texte d'origine ; une fin de ligne logique
					// est bourree d'espaces jusqu'a la bordure.
					NkString brutL(s + 3);
					while (brutL.Size() && (brutL[brutL.Size() - 1] == '\r' || brutL[brutL.Size() - 1] == ' '))
						brutL = brutL.SubStr(0, brutL.Size() - 1);
					if (brutL.Size() >= 3 && (unsigned char)brutL[brutL.Size() - 3] == 0xE2 &&
						(unsigned char)brutL[brutL.Size() - 2] == 0x95 && (unsigned char)brutL[brutL.Size() - 1] == 0x91)
						brutL = brutL.SubStr(0, brutL.Size() - 3);
					if (brutL.Size() && brutL[0] == ' ')
						brutL = brutL.SubStr(1);
					if (brutL.Size() && brutL[brutL.Size() - 1] == ' ')
						brutL = brutL.SubStr(0, brutL.Size() - 1);
					NkString contenu = brutL;
					while (contenu.Size() && contenu[contenu.Size() - 1] == ' ')
						contenu = contenu.SubStr(0, contenu.Size() - 1);
					const bool plein = contenu.Size() > 0 && brutL.Size() - contenu.Size() <= 1;
					const char *c = plein ? brutL.CStr() : contenu.CStr();
					if (!enCours)
						while (*c == ' ')
							++c;
					cumul += c;
					enCours = true;
					if (!plein) {
						out.PushBack(cumul);
						cumul.Clear();
						enCours = false;
					}
				}
				if (enCours)
					out.PushBack(cumul);
				return out;
			}
		} // namespace sortie_detail

		// Couleurs (indices ANSI : la palette du terminal les resout selon le theme).
#define NK_SJ_BLEU "\x1b[34m"
#define NK_SJ_VERT "\x1b[32m"
#define NK_SJ_JAUNE "\x1b[33m"
#define NK_SJ_ROUGE "\x1b[31m"
#define NK_SJ_GRIS "\x1b[90m"
#define NK_SJ_GRAS "\x1b[1m"
#define NK_SJ_SOUL "\x1b[4m"
#define NK_SJ_RAZ "\x1b[0m"

		/// La commande telle qu'on la TAPERAIT : sans les arguments que NKCode ajoute
		/// pour lui-meme (--jenga-file, --platform, --target du projet de depart).
		inline NkString NkSortieCommandeCourte(const NkString &cmd) {
			NkVector<NkString> mots;
			NkString cur;
			bool guil = false;
			for (const char *c = cmd.CStr();; ++c) {
				if (*c == '"')
					guil = !guil;
				if ((*c == ' ' && !guil) || *c == '\0') {
					if (!cur.Empty())
						mots.PushBack(cur);
					cur.Clear();
					if (!*c)
						break;
				} else
					cur += *c;
			}
			NkString out;
			for (usize i = 0; i < mots.Size(); ++i) {
				const NkString &m = mots[i];
				if ((m == NkString("--jenga-file") || m == NkString("--platform") || m == NkString("--target")) &&
					i + 1 < mots.Size()) {
					++i;
					continue;
				}
				if (!out.Empty())
					out += ' ';
				out += m;
			}
			return out;
		}

		/// L'invite, telle que le terminal de NKCode l'ecrit.
		inline NkString NkSortieInvite(const NkString &racineWindows) {
			return NkString(NK_SJ_BLEU "PS ") + racineWindows + ">" NK_SJ_RAZ " ";
		}

		/// Met en forme `brut` (la sortie de la commande, dans l'ordre). `secondes` :
		/// la duree mesuree par NKCode (affichee a la fin). `fini` : la commande est
		/// terminee ; `code` : son code de retour.
		inline NkSortieJengaEtat NkFormaterSortieJenga(const NkVector<NkString> &brut, const NkString &invite,
														const NkString &commande, const NkString &versionJenga,
														const NkString &workspace, float32 secondes, bool fini,
														bool reussiDeclare) {
			using namespace sortie_detail;
			NkSortieJengaEtat E;
			E.lignes.PushBack(invite + NK_SJ_GRAS + "jenga" NK_SJ_RAZ + commande.SubStr(5 < commande.Size() ? 5 : commande.Size()));
			NkString cfg, cible, chaine;
			NkString version = versionJenga;
			struct Projet {
					NkString nom;
					int32 compiles = 0;
					bool aJour = false, fin = false, echec = false;
					NkVector<NkString> fichiers;
					NkVector<NkString> messages; ///< lignes « avertissement » / « erreur » deja formees
					NkString lien;
			};
			Projet cur;
			bool dansProjet = false, enTete = false;
			auto ecrireEnTete = [&]() {
				if (enTete)
					return;
				enTete = true;
				NkString t = NkString(NK_SJ_GRIS "Jenga ") + (version.Empty() ? NkString("?") : version);
				if (!workspace.Empty())
					t += NkString(" \xC2\xB7 workspace ") + workspace;
				if (!cible.Empty())
					t += NkString(" \xC2\xB7 ") + cible;
				if (!chaine.Empty())
					t += NkString(" \xC2\xB7 ") + chaine;
				t += NK_SJ_RAZ;
				E.lignes.PushBack(t);
			};
			auto fermer = [&]() {
				if (!dansProjet)
					return;
				dansProjet = false;
				ecrireEnTete();
				NkString l;
				if (cur.echec)
					l = NkString("  " NK_SJ_ROUGE "\xE2\x9C\x97" NK_SJ_RAZ " ");
				else if (!cur.messages.Empty())
					l = NkString("  " NK_SJ_JAUNE "!" NK_SJ_RAZ " ");
				else
					l = NkString("  " NK_SJ_VERT "\xE2\x80\xA2" NK_SJ_RAZ " ");
				l += Bourrer(cur.nom, 16) + " ";
				if (!cur.messages.Empty() || cur.echec) {
					NkString liste;
					for (usize i = 0; i < cur.fichiers.Size() && i < 4; ++i) {
						if (i)
							liste += ", ";
						liste += cur.fichiers[i];
					}
					if (cur.fichiers.Size() > 4)
						liste += NkPrintf(" (+%d)", (int32)cur.fichiers.Size() - 4);
					l += liste.Empty() ? NkString(NK_SJ_GRIS "\xC3\xA0 jour" NK_SJ_RAZ) : liste;
				} else if (cur.compiles > 0)
					l += NkPrintf(NK_SJ_GRIS "%d fichier%s compil\xC3\xA9%s" NK_SJ_RAZ, cur.compiles, cur.compiles > 1 ? "s" : "",
								  cur.compiles > 1 ? "s" : "");
				else
					l += NK_SJ_GRIS "\xC3\xA0 jour" NK_SJ_RAZ;
				E.lignes.PushBack(l);
				for (usize i = 0; i < cur.messages.Size(); ++i)
					E.lignes.PushBack(cur.messages[i]);
				if (!cur.lien.Empty())
					E.lignes.PushBack(NkString("      " NK_SJ_GRIS "lien \xE2\x86\x92" NK_SJ_RAZ " ") + cur.lien);
				cur = Projet();
			};
			bool echecGlobal = false;
			const NkVector<NkString> logiques = LignesLogiques(brut);
			for (usize i = 0; i < logiques.Size(); ++i) {
				const NkString &ln = logiques[i];
				const char *s = ln.CStr();
				const char *p = SauterSymboles(s);
				if (const char *v = Trouve(s, "Build System v")) { // le bandeau de Jenga porte sa version
					NkString ver;
					for (const char *c = v + 14; *c && *c != ' '; ++c)
						ver += *c;
					if (!ver.Empty())
						version = ver;
				} else if (Commence(p, "Configuration:"))
					cfg = Trim(p + 14);
				else if (Commence(p, "Target:")) {
					cible = Trim(p + 7);
					// « Windows x86_64 » -> « Windows x64 »
					const char *x = Trouve(cible.CStr(), "x86_64");
					if (x)
						cible = cible.SubStr(0, (usize)(x - cible.CStr())) + "x64";
				} else if (Commence(p, "Toolchain:"))
					chaine = Trim(p + 10);
				else if (const char *pj = Trouve(s, "Project:")) {
					fermer();
					dansProjet = true;
					NkString nom;
					for (const char *c = pj + 8; *c; ++c) {
						if (*c == ' ' && nom.Size())
							break;
						if (*c != ' ')
							nom += *c;
					}
					cur.nom = nom;
				} else if (dansProjet && Trouve(s, "All files up to date"))
					cur.aJour = true;
				else if (dansProjet && (Trouve(s, "Compiled:") || Trouve(s, "Compiled with warnings:"))) {
					++cur.compiles;
					const char *q = Trouve(s, "Compiled with warnings:");
					cur.fichiers.PushBack(Trim(q ? q + 23 : Trouve(s, "Compiled:") + 9));
				} else if (NkDiagInfo di; dansProjet && NkParseDiagLine(s, di)) {
					const bool err = di.sev == NkDiagSev::Error;
					if (err)
						++E.erreurs;
					else
						++E.avertissements;
					NkString m = err ? NkString("      " NK_SJ_ROUGE "erreur" NK_SJ_RAZ " ")
									 : NkString("      " NK_SJ_JAUNE "avertissement" NK_SJ_RAZ " ");
					const NkString lieu = NkPrintf("%s:%d:%d", NomFichier(di.file).CStr(), di.line, di.col);
					m += NkString(NK_SJ_SOUL) + lieu + NK_SJ_RAZ " " + Trim(di.msg.CStr());
					cur.messages.PushBack(m);
					if (err)
						cur.echec = true;
				} else if (dansProjet && Trouve(s, "Built:")) {
					const NkString chemin = Barres(Trim(Trouve(s, "Built:") + 6));
					const char *e = chemin.CStr() + chemin.Size();
					// le lien n'est montre que pour ce qu'on LANCE (exe, apk...), pas les .lib
					const bool lancable = chemin.EndsWith(".exe") || chemin.EndsWith(".apk") || chemin.EndsWith(".app") ||
										  chemin.EndsWith(".html") || Trouve(chemin.CStr(), "/Bin/");
					(void)e;
					if (lancable && !chemin.EndsWith(".lib") && !chemin.EndsWith(".a"))
						cur.lien = chemin;
				} else if (dansProjet && (Trouve(s, "Build Successful") || Trouve(s, "Build Failed") || Trouve(s, "Build FAILED"))) {
					if (!Trouve(s, "Successful"))
						cur.echec = true, echecGlobal = true;
					cur.fin = true;
					fermer();
				} else if (Trouve(s, "BUILD FAILED") || Trouve(s, "FAILURE"))
					echecGlobal = true;
			}
			if (fini) {
				fermer();
				ecrireEnTete();
				E.fini = true;
				E.reussi = reussiDeclare && !echecGlobal && E.erreurs == 0;
				char dur[32];
				{
					const int32 d10 = (int32)(secondes * 10.f + 0.5f);
					snprintf(dur, sizeof(dur), "%d,%d s", d10 / 10, d10 % 10);
				}
				NkString fin = E.reussi ? NkString(NK_SJ_VERT "Construction r\xC3\xA9ussie" NK_SJ_RAZ " en ")
										: NkString(NK_SJ_ROUGE "\xC3\x89" "chec de la construction" NK_SJ_RAZ " en ");
				fin += dur;
				fin += NkPrintf(" \xC2\xB7 %d erreur%s \xC2\xB7 %d avertissement%s", E.erreurs, E.erreurs > 1 ? "s" : "",
								E.avertissements, E.avertissements > 1 ? "s" : "");
				E.lignes.PushBack(fin);
			}
			(void)cfg;
			return E;
		}

	} // namespace nkcode
} // namespace nkentseu
