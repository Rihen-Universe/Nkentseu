#pragma once
// =============================================================================
// NkBancApparences.h — LE BANC DE LA REVERSIBILITE (01/10, exigence de Rihen :
// « revenir = rechoisir »).
//
// Lance par NK_BANC_APPARENCES=1 (NKCode SANS fenetre : NK_FENETRE_CACHEE=1),
// dans le VRAI NKCode, par la porte d'apres-image : aucune entree injectee.
//
//   image 60   : PHOTO A de tout ce que l'apparence et le jeu d'icones ecrivent
//                (theme du dessin, coloration, NkCol, theme du kit, disposition,
//                ilots, table des icones, reglages) ;
//   image 61   : on change d'apparence ET de jeu d'icones (Trait, installe pour
//                l'occasion depuis le catalogue) ;
//   image 90   : PHOTO B — elle DOIT differer de A (sinon le banc ne prouve rien) ;
//   image 91   : on revient aux choix d'avant ;
//   image 120  : PHOTO C — elle DOIT etre IDENTIQUE a A, octet pour octet.
//   + une icone ABSENTE du jeu (Claude) retombe sur les PNG de base ; une icone
//     du jeu (FicCpp) vient bien du jeu ;
//   + installer puis desinstaller une extension rend la liste d'avant.
//
// CONTRE-EPREUVES (le banc doit savoir rougir) :
//   NK_BANC_MUTATION=residu    l'applicateur oublie un champ au retour : C != A ;
//   NK_BANC_MUTATION=sansrepli la chaine perd les PNG de base : Claude introuvable.
// Le verdict s'ecrit sur la sortie : « [banc-apparences] VERDICT : VERT » ou ROUGE.
// =============================================================================
#include "NKCode/Shell/NkAppCommands.h"
#include "NKCode/Shell/NkAppIcons.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace nkentseu {
	namespace nkcode {


		struct NkPhotoEtat {
				NkVector<uint8> octets;
				void Ajouter(const void *p, usize n) {
					const uint8 *b = static_cast<const uint8 *>(p);
					for (usize i = 0; i < n; ++i)
						octets.PushBack(b[i]);
				}
				void Couleur(const NkColor &c) {
					const uint8 v[4] = {c.r, c.g, c.b, c.a};
					Ajouter(v, 4);
				}
				void Texte(const NkString &s) {
					Ajouter(s.CStr(), s.Size() + 1);
				}
				bool operator==(const NkPhotoEtat &o) const {
					if (octets.Size() != o.octets.Size())
						return false;
					for (usize i = 0; i < octets.Size(); ++i)
						if (octets[i] != o.octets[i])
							return false;
					return true;
				}
		};

		inline NkPhotoEtat NkPhotographier(NkGuiContext &ctx, NkHomeState &H, editorkit::NkEditorShell *sh) {
			NkPhotoEtat P;
			// 1) Le theme du dessin, jeton par jeton (couleurs ET scalaires), + le drapeau.
			int32 n = 0;
			const NkGuiTokenDesc *tk = NkGuiThemeTokens(&n);
			for (int32 i = 0; i < n; ++i) {
				if (tk[i].type == NkGuiTokenType::Color)
					P.Couleur(*NkGuiThemeColor(ctx.theme, tk[i].name));
				else {
					const float32 f = *NkGuiThemeScalar(ctx.theme, tk[i].name);
					P.Ajouter(&f, sizeof(f));
				}
			}
			P.Ajouter(&ctx.theme.tabActiveIsWindowBg, 1);
			// 2) La coloration.
			const NkGuiSyntax &s = ctx.syntax;
			const NkColor sy[12] = {s.text, s.keyword, s.type,	  s.string,	 s.comment,	 s.number,
									s.preproc, s.heading, s.mdcode, s.function, s.constant, s.oper};
			for (const NkColor &c : sy)
				P.Couleur(c);
			// 3) La palette NkCol.
			const NkColor col[18] = {NkCol::background, NkCol::foreground, NkCol::border,	 NkCol::input,
									 NkCol::surface,	NkCol::primary,	   NkCol::primaryFg, NkCol::secondary,
									 NkCol::secondaryFg, NkCol::accent,	   NkCol::sidebar,	 NkCol::sidebarFg,
									 NkCol::muted,		NkCol::mutedFg,	   NkCol::success,	 NkCol::danger,
									 NkCol::hover,		NkCol::selection};
			for (const NkColor &c : col)
				P.Couleur(c);
			// 4) Le theme du KIT (panneau IA, terminal partage...).
			if (sh)
				for (uint16 r = 0; r < (uint16)editorkit::NkRole::Count; ++r) {
					const editorkit::NkThemeColor v = sh->KitTheme().Get((editorkit::NkRole)r);
					P.Ajouter(&v, sizeof(v));
				}
			// 5) La disposition et les ilots.
			const NkApparenceDispo &d = NkApparenceCourante().dispo;
			const uint8 b[11] = {d.barreOutils, d.styleFamille, d.activiteGauche, d.activiteDroite, d.ongletsLateraux,
								 d.selectionPleine, d.synthese, d.ilots, d.titreCentre, d.menusVisibles,
								 (uint8)d.enteteExplorateur};
			P.Ajouter(b, sizeof(b));
			const float32 f[11] = {d.titreH, d.bandeH, d.logoCoin, d.barreEtatH, d.ligneArbre, d.ilotRayon, d.dockEcart,
								   d.barreActiviteL, ctx.dockGap, ctx.dockIlotRayon, (float32)ctx.dockSeparateurVisible};
			P.Ajouter(f, sizeof(f));
			P.Couleur(ctx.dockFond);
			// 6) La table des icones : les emplacements de l'interface, les fichiers, les dossiers.
			const NkIcons &ic = H.icons;
			const uint32 slots[] = {ic.files, ic.search, ic.sourceControl, ic.bug, ic.liveShare, ic.puzzle, ic.chart, ic.gear,
									ic.claude, ic.codeC, ic.accueil, ic.sparkles, ic.folderM, ic.folderMOpen, ic.folderRoot,
									ic.folderRootOpen, ic.newFile2, ic.newFolder, ic.collapseAll, ic.jenga, ic.defaultFile};
			P.Ajouter(slots, sizeof(slots));
			static const char *kFichiers[] = {"a.cpp", "a.h", "a.hpp", "a.c", "x.jenga", "x.md", "x.json", "x.py",
											  "x.png", "x.txt", "x.cfg", ".gitignore", "x.nksl", "x.sh", "x.zip"};
			for (const char *fn : kFichiers) {
				const uint32 t = ic.ForFile(fn);
				P.Ajouter(&t, sizeof(t));
				const uint8 m = ic.IsMono(t) ? 1 : 0;
				P.Ajouter(&m, 1);
			}
			static const char *kDossiers[] = {"src", "tests", "docs", "data", "build", ".git", "tools", "include"};
			for (const char *dn : kDossiers) {
				const uint32 a = ic.ForDir(dn, false), o = ic.ForDir(dn, true);
				P.Ajouter(&a, sizeof(a));
				P.Ajouter(&o, sizeof(o));
			}
			// 7) Les reglages concernes.
			P.Ajouter(&H.settings.apparence, sizeof(int32));
			P.Ajouter(&H.settings.theme, sizeof(int32));
			P.Texte(NkString(H.settings.accent));
			P.Texte(NkString(H.settings.jeuIcones));
			P.Ajouter(&H.settings.menusVisibles, 1);
			return P;
		}

		/// Le banc, appele APRES chaque image (main.cpp, porte d'apres-image).
		inline void NkBancApparencesImage(NkGuiContext &ctx, NkHomeState &H, editorkit::NkEditorShell *sh, int32 image) {
			static NkPhotoEtat A, B;
			static int32 apparenceAvant = 0;
			static NkString jeuAvant;
			static bool rouge = false;
			auto dire = [&](const char *quoi, bool ok) {
				std::printf("[banc-apparences] %-62s %s\n", quoi, ok ? "OK" : "ECHEC");
				std::fflush(stdout);
				if (!ok)
					rouge = true;
			};
			NkJeuxIcones &J = NkCodeJeuxIcones();
			if (image == 60) {
				A = NkPhotographier(ctx, H, sh);
				apparenceAvant = H.settings.apparence;
				jeuAvant = NkString(H.settings.jeuIcones);
				std::printf("[banc-apparences] photo A : %d octets (apparence %d, jeu « %s »)\n", (int)A.octets.Size(),
							apparenceAvant, jeuAvant.CStr());
				// Installer / desinstaller rend la liste d'avant.
				usize installes = 0;
				for (usize k = 0; k < J.jeux.Size(); ++k)
					installes += J.jeux[k].installe ? 1u : 0u;
				const bool ins = J.Installer("actuel");
				usize apres = 0;
				for (usize k = 0; k < J.jeux.Size(); ++k)
					apres += J.jeux[k].installe ? 1u : 0u;
				const bool des = J.Desinstaller("actuel");
				usize fin = 0;
				for (usize k = 0; k < J.jeux.Size(); ++k)
					fin += J.jeux[k].installe ? 1u : 0u;
				dire("installer « actuel » l'ajoute aux jeux installes", ins && apres == installes + 1);
				dire("le desinstaller rend la liste d'avant", des && fin == installes);
				// Une icone ABSENTE du jeu retombe sur les PNG de base ; une icone du jeu vient du jeu.
				(void)J.Installer("trait");
				const NkVector<NkMaillonIcones> ch = J.Chaine("trait", "sombre");
				NkVector<NkMaillonIcones> chBanc = ch;
				if (const char *m = std::getenv("NK_BANC_MUTATION"))
					if (m[0] == 's' && chBanc.Size() > 1)
						chBanc.PopBack(); // contre-epreuve : plus de PNG de base
				const NkIconeTrouvee claude = J.Resoudre(chBanc, "Claude");
				dire("Claude (absent du Trait) retombe sur les PNG de base",
					 !claude.chemin.Empty() && claude.jeu == -1 && claude.chemin.StartsWith(J.dossierBase.CStr()));
				const NkIconeTrouvee cpp = J.Resoudre(chBanc, "FicCpp");
				const int32 iTrait = J.Index("trait");
				dire("FicCpp vient bien du jeu Trait (variante sombre)",
					 !cpp.chemin.Empty() && cpp.jeu == iTrait && cpp.chemin.EndsWith("sombre/FicCpp.svg"));
			} else if (image == 61) {
				// Le changement : une autre apparence, un autre jeu.
				H.settings.apparence = (apparenceAvant == NK_APPARENCE_FAMILLE) ? NK_APPARENCE_CLASSIQUE : NK_APPARENCE_FAMILLE;
				NkStrCopy(H.settings.jeuIcones, sizeof(H.settings.jeuIcones), "trait");
			} else if (image == 90) {
				B = NkPhotographier(ctx, H, sh);
				dire("la photo B (autre apparence, autre jeu) differe de A", !(B == A));
			} else if (image == 91) {
				// Le retour : rechoisir.
				H.settings.apparence = apparenceAvant;
				NkStrCopy(H.settings.jeuIcones, sizeof(H.settings.jeuIcones), jeuAvant.CStr());
				if (const char *m = std::getenv("NK_BANC_MUTATION"))
					if (m[0] == 'r')
						NkBancResidu() = true; // contre-epreuve : un champ n'est plus reecrit
			} else if (image == 120) {
				const NkPhotoEtat C = NkPhotographier(ctx, H, sh);
				usize diff = 0, premier = (usize)-1;
				for (usize i = 0; i < A.octets.Size() && i < C.octets.Size(); ++i)
					if (A.octets[i] != C.octets[i]) {
						++diff;
						if (premier == (usize)-1)
							premier = i;
					}
				if (diff)
					std::printf("[banc-apparences] %d octet(s) differents, le premier a l'octet %d\n", (int)diff, (int)premier);
				dire("revenir rend EXACTEMENT l'etat d'avant (photo C == photo A)", C == A);
				std::printf("[banc-apparences] VERDICT : %s\n", rouge ? "ROUGE" : "VERT");
				std::fflush(stdout);
			}
		}

	} // namespace nkcode
} // namespace nkentseu
