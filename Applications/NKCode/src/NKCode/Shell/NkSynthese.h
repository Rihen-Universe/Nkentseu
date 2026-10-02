#pragma once
// =============================================================================
// NkSynthese.h — L'APPARENCE « SYNTHESE » DE NKCODE (maquette D, choix de Rihen
// le 01/10 : « je kiffe tout » ; apparence PAR DEFAUT).
//
// Sources exactes : References/Captures/nkcode-propositions/_sources/d.html
// (+ ilots.css, base.css, commun.js) et les D_*.png du meme dossier.
//
// Ce fichier dessine ce que la coquille ne sait pas dessiner seule :
//   - LA BARRE UNIQUE en haut (dans la barre de titre, a la place des menus) :
//     ≡ (tous les menus), Workspace, Branche, Partager, palette, Cible ▶ deboguer
//     construire arreter, Reglages, Compte ;
//   - LA BANDE VERTICALE de gauche : extensions EN HAUT, fenetres d'outils EN BAS
//     (Assistant IA, Terminal, Sortie, Problemes, Execution, Profilage) ;
//   - LES ILOTS : coins arrondis et contour des feuilles du dock (l'ecart, le fond
//     et les marges sont des portes de la coquille / de NKGui) ;
//   - LA BARRE D'ETAT : Tutoriel / Capture / Direct en pied, puis l'etat ;
//   - LES SEGMENTS de vues en tete de l'ilot de gauche.
//
// ⚠️ CE QUI N'A PAS ENCORE DE FONCTION (collaboration en direct, compte,
//    tutoriel / capture / diffusion) EST A SA PLACE, DESACTIVE, avec l'info-bulle
//    « a venir » : pas de faux fonctionnement.
// =============================================================================
#include "NKCode/Shell/NkApparence.h"
#include "NKCode/Shell/NkMenuBar.h"
#include "NKCode/Shell/NkHome.h"
#include "NKCode/Shell/Panels.h"
#include "NKCode/Project/NkEmbeddedJenga.h"
#include "NKEditorKit/NkEditorTooltip.h"
#include "NKContainers/String/NkFormat.h"

namespace nkentseu {
	namespace nkcode {

		// ── Etat de la Synthese (un seul par processus) ─────────────────────────
		struct NkSyntheseEtat {
				NkMenuBarCtx *mb = nullptr;
				NkHomeState *home = nullptr;
				int32 deroule = 0;	///< 0 aucun, 1 workspace, 3 cible, 4 reglages
				NkRect ancre{};		///< la pilule qui a ouvert le deroulant
				bool justeOuvert = false;
				// La branche git (lue dans .git/HEAD, sans lancer git) et sa racine.
				NkString branche, brancheRacine;
				float32 brancheAge = 99.f;
				NkRect boutonMenu{}; ///< le bouton ≡ de cette image (pour les sondes)
		};
		inline NkSyntheseEtat &NkSynthese() {
			static NkSyntheseEtat e;
			return e;
		}

		/// LE MENU PRINCIPAL ≡ (variante 2 de la maquette D, choix de Rihen) : un
		/// panneau avec la recherche en tete, les onze categories en colonne, le
		/// CONTENU de la categorie survolee a cote (les MEMES menus de NKCode : ses
		/// sous-menus s'ouvrent dans la colonne suivante), un pied de raccourcis.
		struct NkMenuSyntheseEtat {
				bool ouvert = false;
				bool justeOuvert = false;
				int32 cat = 0;
				NkRect ancre{};
				/// La hauteur mesuree du contenu de chaque categorie (layout.maxY) : le
				/// panneau prend celle de la plus longue deja vue, sans sortir de la fenetre.
				float32 hContenu[11] = {};
		};
		inline NkMenuSyntheseEtat &NkMenuSynthese() {
			static NkMenuSyntheseEtat m;
			return m;
		}

		inline bool NkSyntheseActive() {
			return NkApparenceCourante().dispo.synthese;
		}

		/// NkUi sur un contexte NKGui nu (la bande, les coins : pas de cadre d'image).
		inline NkUi NkUiDe(NkGuiContext &ctx, bool overlay = false) {
			NkUi u;
			u.ctx = &ctx;
			u.f = ctx.font;
			u.dl = overlay ? &ctx.dlOverlay : &ctx.DL();
			u.mp = ctx.input.mousePos;
			u.click = ctx.input.mouseClicked[0];
			u.down = ctx.input.mouseDown[0];
			u.S = ctx.S(1.f);
			return u;
		}

		// ── Les glyphes que l'atlas n'a pas (dessines au trait, 1,35 px) ────────
		inline void NkSynChevronBas(const NkUi &u, float32 cx, float32 cy, const NkColor &c) {
			const float32 a = u.s(3.5f);
			u.dl->AddLine({cx - a, cy - a * 0.5f}, {cx, cy + a * 0.5f}, c, u.s(1.4f));
			u.dl->AddLine({cx, cy + a * 0.5f}, {cx + a, cy - a * 0.5f}, c, u.s(1.4f));
		}
		inline void NkSynChevronDroit(const NkUi &u, float32 cx, float32 cy, const NkColor &c) {
			const float32 a = u.s(3.5f);
			u.dl->AddLine({cx - a * 0.5f, cy - a}, {cx + a * 0.5f, cy}, c, u.s(1.4f));
			u.dl->AddLine({cx + a * 0.5f, cy}, {cx - a * 0.5f, cy + a}, c, u.s(1.4f));
		}
		inline void NkSynPiles(const NkUi &u, const NkRect &r, const NkColor &c) { // ≡
			const float32 cx = r.x + r.w * 0.5f, cy = r.y + r.h * 0.5f, w = u.s(6.5f);
			for (int32 k = -1; k <= 1; ++k)
				u.dl->AddLine({cx - w, cy + k * u.s(4.5f)}, {cx + w, cy + k * u.s(4.5f)}, c, u.s(1.5f));
		}
		inline void NkSynTroisPoints(const NkUi &u, const NkRect &r, const NkColor &c) {
			const float32 cx = r.x + r.w * 0.5f, cy = r.y + r.h * 0.5f;
			for (int32 k = -1; k <= 1; ++k)
				u.dl->AddCircleFilled({cx + k * u.s(5.f), cy}, u.s(1.3f), c);
		}
		inline void NkSynLien(const NkUi &u, const NkRect &r, const NkColor &c) {
			const float32 cx = r.x + r.w * 0.5f, cy = r.y + r.h * 0.5f, s = r.w * 0.22f;
			u.dl->AddRect({cx - s * 1.9f, cy - s * 0.9f, s * 2.2f, s * 1.8f}, c, u.s(1.3f), s * 0.9f);
			u.dl->AddRect({cx - s * 0.3f, cy - s * 0.9f, s * 2.2f, s * 1.8f}, c, u.s(1.3f), s * 0.9f);
		}
		inline void NkSynVideo(const NkUi &u, const NkRect &r, const NkColor &c) {
			const float32 x = r.x, y = r.y, w = r.w, h = r.h;
			u.dl->AddRect({x + w * 0.08f, y + h * 0.26f, w * 0.6f, h * 0.48f}, c, u.s(1.2f), u.s(1.5f));
			u.dl->AddTriangleFilled({x + w * 0.72f, y + h * 0.5f}, {x + w * 0.94f, y + h * 0.3f},
									{x + w * 0.94f, y + h * 0.7f}, c);
		}
		inline void NkSynPhoto(const NkUi &u, const NkRect &r, const NkColor &c) {
			const float32 x = r.x, y = r.y, w = r.w, h = r.h;
			u.dl->AddRect({x + w * 0.08f, y + h * 0.28f, w * 0.84f, h * 0.56f}, c, u.s(1.2f), u.s(1.5f));
			u.dl->AddCircle({x + w * 0.5f, y + h * 0.56f}, w * 0.16f, c, u.s(1.2f));
			u.dl->AddLine({x + w * 0.34f, y + h * 0.28f}, {x + w * 0.4f, y + h * 0.16f}, c, u.s(1.2f));
			u.dl->AddLine({x + w * 0.4f, y + h * 0.16f}, {x + w * 0.6f, y + h * 0.16f}, c, u.s(1.2f));
			u.dl->AddLine({x + w * 0.6f, y + h * 0.16f}, {x + w * 0.66f, y + h * 0.28f}, c, u.s(1.2f));
		}
		inline void NkSynDirect(const NkUi &u, const NkRect &r, const NkColor &c) {
			const float32 cx = r.x + r.w * 0.5f, cy = r.y + r.h * 0.5f;
			u.dl->AddCircleFilled({cx, cy}, r.w * 0.11f, c);
			u.dl->AddCircle({cx, cy}, r.w * 0.28f, c, u.s(1.2f));
			u.dl->AddCircle({cx, cy}, r.w * 0.44f, c, u.s(1.2f));
		}
		inline void NkSynSortie(const NkUi &u, const NkRect &r, const NkColor &c) { // lignes de journal
			const float32 x = r.x + r.w * 0.16f, w = r.w * 0.68f;
			for (int32 k = 0; k < 4; ++k) {
				const float32 y = r.y + r.h * (0.24f + 0.17f * k);
				u.dl->AddLine({x, y}, {x + (k == 3 ? w * 0.55f : w), y}, c, u.s(1.4f));
			}
		}

		/// Le CONTOUR d'un ilot (le « box-shadow:0 0 0 1px » de la maquette).
		inline void NkSynContour(const NkUi &u, const NkRect &r, float32 rayon, const NkColor &c) {
			u.dl->AddRect(r, c, 1.f, rayon);
		}

		// ── Une pilule de la barre (.p) : rend vrai au clic ─────────────────────
		struct NkSynPil {
				uint32 icone = 0;		///< texture (0 : glyphe dessine)
				void (*glyphe)(const NkUi &, const NkRect &, const NkColor &) = nullptr;
				NkColor teinteIcone{0, 0, 0, 0}; ///< a = 0 : couleur du texte
				const char *lib = nullptr;		 ///< « Workspace : » (fg2)
				const char *val = nullptr;		 ///< « Nkentseu » (fg)
				bool chevron = false;
				bool fond = true;	 ///< fond d'ilot + contour (.p.fond / .p.bord)
				bool plein = false;	 ///< bouton principal (Partager)
				bool on = false;	 ///< deroule ouvert
				bool off = false;	 ///< desactive (a venir) : dessine attenue, aucun clic
				bool carre = false;	 ///< 32 de large, l'icone seule
				const char *bulle = nullptr; ///< info-bulle
		};

		inline float32 NkSynPiluleLargeur(const NkUi &u, const NkSynPil &p) {
			if (p.carre)
				return u.s(32.f);
			float32 w = u.s(10.f) + u.s(10.f);
			bool premier = true;
			auto ajout = [&](float32 x) {
				w += (premier ? 0.f : u.s(7.f)) + x;
				premier = false;
			};
			if (p.icone || p.glyphe)
				ajout(u.s(16.f));
			if (p.lib && p.val) {
				ajout(u.TextW(p.lib) + u.s(4.f) + u.TextW(p.val));
			} else if (p.lib)
				ajout(u.TextW(p.lib));
			else if (p.val)
				ajout(u.TextW(p.val));
			if (p.chevron)
				ajout(u.s(10.f));
			return w;
		}

		inline bool NkSynPilule(const NkUi &u, const NkRect &r, const NkSynPil &p) {
			const NkApparencePalette &a = NkApparenceCourante().pal;
			const bool hov = !p.off && u.Hit(r);
			const float32 rad = u.s(8.f);
			NkColor fg = p.plein ? NkColor{255, 255, 255, 255} : (p.off ? a.fg3 : a.fg);
			if (p.plein) {
				NkColor c = a.plein;
				if (p.off)
					c = NkMelange(a.gouttiere, a.plein, 0.55f);
				else if (hov)
					c = NkMelange(a.plein, NkColor{255, 255, 255, 255}, 0.1f);
				u.Rect(r, c, rad);
			} else if (p.on)
				u.Rect(r, NkMelange(a.gouttiere, a.plein, a.clair ? 0.11f : 0.22f), rad);
			else if (p.fond) {
				u.Rect(r, hov ? a.haut : a.panneau, rad);
				NkSynContour(u, r, rad, a.trait);
			} else if (hov)
				u.Rect(r, a.survol, rad);
			float32 x = r.x + (p.carre ? (r.w - u.s(16.f)) * 0.5f : u.s(10.f));
			const float32 cy = r.y + r.h * 0.5f;
			if (p.icone || p.glyphe) {
				const NkRect ir = {x, cy - u.s(8.f), u.s(16.f), u.s(16.f)};
				NkColor tc = p.teinteIcone.a ? p.teinteIcone : (p.carre ? (p.off ? a.fg3 : a.fg2) : fg);
				if (p.on)
					tc = a.accent;
				if (p.icone)
					NkDrawIcon(u, p.icone, ir, tc);
				else
					p.glyphe(u, ir, tc);
				x += u.s(16.f) + u.s(7.f);
			}
			if (p.lib) {
				u.TextV(x, r.y, r.h, p.lib, p.plein ? fg : a.fg2);
				x += u.TextW(p.lib) + (p.val ? u.s(4.f) : u.s(7.f));
			}
			if (p.val) {
				u.TextV(x, r.y, r.h, p.val, fg);
				x += u.TextW(p.val) + u.s(7.f);
			}
			if (p.chevron)
				NkSynChevronBas(u, x + u.s(4.f), cy, p.plein ? fg : a.fg2);
			if (p.bulle && u.ctx)
				editorkit::NkTooltip(*u.ctx, u.Hit(r) && u.ctx->popupDepth == 0, p.bulle);
			return hov && u.click;
		}

		/// La branche courante du workspace : .git/HEAD lu directement (pas de
		/// processus git par image), relu toutes les 2 s.
		inline const NkString &NkSyntheseBranche(NkCodeState *s, float32 dt) {
			NkSyntheseEtat &E = NkSynthese();
			const NkString racine = (s && s->HasWorkspace()) ? s->root.ToString() : NkString();
			E.brancheAge += dt;
			if (!(racine == E.brancheRacine) || E.brancheAge > 2.f) {
				E.brancheRacine = racine;
				E.brancheAge = 0.f;
				E.branche.Clear();
				if (!racine.Empty()) {
					const NkString head = (NkPath(racine.CStr()) / ".git" / "HEAD").ToString();
					if (NkFile::Exists(head.CStr())) {
						const NkString t = NkFile::ReadAllText(NkPath(head.CStr()));
						const char *k = "ref: refs/heads/";
						const char *p = t.CStr();
						bool ok = true;
						for (int32 i = 0; k[i]; ++i)
							if (p[i] != k[i]) {
								ok = false;
								break;
							}
						if (ok)
							for (const char *c = p + 16; *c && *c != '\n' && *c != '\r'; ++c)
								E.branche += *c;
						else if (t.Size() >= 7) // tete detachee : le debut du commit
							E.branche = t.SubStr(0, 7);
					}
				}
			}
			return E.branche;
		}

		/// « NKCode · Debug · Windows x64 » : la cible reelle (toolbar d'avant).
		inline NkString NkSyntheseCible(NkCodeState *s) {
			if (!s || !s->HasWorkspace())
				return NkString("\xE2\x80\x94"); // —
			int32 nSys = 0;
			const NkCodeState::SysDef *sysd = NkCodeState::Systems(&nSys);
			const int32 si = (s->sysIdx >= 0 && s->sysIdx < nSys) ? s->sysIdx : 0;
			static const char *kCfg[] = {"Debug", "Release", "Toutes"};
			const int32 ci = (s->cfgIdx >= 0 && s->cfgIdx < 3) ? s->cfgIdx : 0;
			const char *proj = s->projects.Empty() ? "\xE2\x80\xA6" : (s->AllProjects() ? NkT("tb.allprojects")
																						   : s->SelectedProject());
			NkString arch;
			if (s->archIdx >= 0 && s->archIdx < sysd[si].nArch) {
				arch = sysd[si].archs[s->archIdx];
				if (arch == NkString("x86_64"))
					arch = "x64";
			}
			return NkPrintf("%s \xC2\xB7 %s \xC2\xB7 %s%s%s", proj, kCfg[ci], sysd[si].name, arch.Empty() ? "" : " ",
							arch.CStr());
		}

		inline const char *NkSyntheseWorkspace(NkCodeState *s) {
			if (!s || !s->HasWorkspace())
				return "aucun";
			if (s->wsIdx >= 0 && s->wsIdx < (int32)s->wsNames.Size())
				return s->wsNames[s->wsIdx].CStr();
			return "Workspace";
		}

		// ═══════════════════════════════════════════════════════════════════════
		//  LA BARRE UNIQUE (dans la barre de titre, a la place des menus)
		// ═══════════════════════════════════════════════════════════════════════
		/// La barre dans `barre`. `puits` : les menus sont dans le ≡ (variante 2,
		/// barre de titre) ; sinon ils sont dans la barre de titre au-dessus
		/// (variante 3) et la barre commence au bord.
		inline void NkSyntheseBarre(NkEditorFrameContext &ec, NkMenuBarCtx *mb, const NkRect &barre, bool puits) {
			auto &ctx = ec.Ui();
			NkSyntheseEtat &E = NkSynthese();
			E.mb = mb;
			NkCodeState *s = (mb && mb->dlg) ? mb->dlg->st : nullptr;
			NkHomeState *home = mb ? mb->home : nullptr;
			const NkApparencePalette &a = NkApparenceCourante().pal;
			const NkRect bar = barre;
			const NkUi u = NkUiDe(ctx);
			const float32 ph = u.s(30.f);
			const float32 py = bar.y + (bar.h - ph) * 0.5f;
			const float32 gap = u.s(6.f);

			// ── ≡ : TOUS les menus de NKCode, dans le « puits » de la barre NKGui. Le
			// premier titre ne tient pas (limite = la largeur du puits) : les onze
			// menus deviennent des lignes de sous-menu, sous-menus et raccourcis
			// compris -- les MEMES qu'avant, aucune action redeclaree.
			const float32 x0 = bar.x + u.s(puits ? 2.f : 6.f);
			bool menuOuvert = false;
			if (puits) {
				// Le preambule de la barre de menus (mises a jour, fichier choisi au
				// picker) tourne ici, une fois par image ; AUCUN menu sous titre : ils
				// sont tous dans le panneau ≡ (NkSyntheseMenuPanneau).
				const int32 av = mb ? mb->menuSeul : -1;
				if (mb)
					mb->menuSeul = 99;
				DrawMainMenuBar(ec, mb);
				if (mb)
					mb->menuSeul = av;
				menuOuvert = NkMenuSynthese().ouvert;
				const NkRect b = {x0, py, 30.f, ph};
				NkSynthese().boutonMenu = b;
				const bool hov = u.Hit(b);
				if (menuOuvert)
					u.Rect(b, NkMelange(a.gouttiere, a.plein, a.clair ? 0.11f : 0.22f), u.s(8.f));
				else if (hov)
					u.Rect(b, a.survol, u.s(8.f));
				NkSynPiles(u, b, menuOuvert ? a.accent : a.fg2);
				editorkit::NkTooltip(ctx, hov && !menuOuvert, "Menu principal (Fichier, \xC3\x89" "dition, Affichage\xE2\x80\xA6)");
				if (hov && u.click) {
					NkMenuSyntheseEtat &M = NkMenuSynthese();
					M.ouvert = !M.ouvert;
					M.justeOuvert = M.ouvert;
					M.ancre = b;
					ctx.input.mouseClicked[0] = false;
				}
			}

			const NkIcons *ic = s ? s->icons : nullptr;
			auto TEX = [&](uint32 t) { return ic ? t : 0u; };
			auto consomme = [&]() { ctx.input.mouseClicked[0] = false; };
			float32 x = puits ? x0 + 30.f + gap : x0;

			// ── Workspace : X ▾ ──
			{
				NkSynPil p;
				p.icone = TEX(ic ? ic->folderRoot : 0);
				p.teinteIcone = {255, 255, 255, 255};
				p.lib = "Workspace :";
				p.val = NkSyntheseWorkspace(s);
				p.chevron = true;
				p.on = (E.deroule == 1);
				const NkRect r = {x, py, NkSynPiluleLargeur(u, p), ph};
				if (NkSynPilule(u, r, p)) {
					E.deroule = (E.deroule == 1) ? 0 : 1;
					E.ancre = r;
					E.justeOuvert = true;
					consomme();
				}
				x += r.w + gap;
			}
			// ── Branche : transit ▾ ── (seulement dans un depot git)
			const NkString &br = NkSyntheseBranche(s, ec.dt);
			if (!br.Empty()) {
				NkSynPil p;
				p.icone = TEX(ic ? ic->sourceControl : 0);
				p.lib = "Branche :";
				p.val = br.CStr();
				p.chevron = true;
				p.bulle = "Contr\xC3\xB4le de version";
				const NkRect r = {x, py, NkSynPiluleLargeur(u, p), ph};
				if (NkSynPilule(u, r, p) && mb && mb->shell) {
					int32 n = 0;
					const char *const *g = SideLeftGroup(n);
					OpenSideExclusive(mb->shell, g, n, "Controle de version");
					consomme();
				}
				x += r.w + gap;
			}
			// ── Partager (collaboration en direct : a venir) ──
			if (s && s->HasWorkspace()) {
				NkSynPil p;
				p.glyphe = &NkSynLien;
				p.lib = "Partager";
				p.plein = true;
				p.off = true;
				p.bulle = "\xC3\x80 venir : la collaboration en direct (session, invit\xC3\xA9s, droits)";
				const NkRect r = {x + u.s(4.f), py, NkSynPiluleLargeur(u, p), ph};
				(void)NkSynPilule(u, r, p);
				editorkit::NkTooltip(ctx, u.Hit(r) && ctx.popupDepth == 0, p.bulle);
				x = r.x + r.w + gap;
			}
			const float32 finGauche = x;

			// ── Droite, de droite a gauche : Compte, Reglages, Cible ▶ ... ──
			float32 xr = bar.x + bar.w - u.s(4.f);
			{
				NkSynPil p;
				p.icone = TEX(ic ? ic->blame : 0); // Account
				p.carre = true;
				p.fond = false;
				p.off = true;
				p.bulle = "\xC3\x80 venir : le compte (synchronisation, collaboration)";
				const NkRect r = {xr - u.s(32.f), py, u.s(32.f), ph};
				(void)NkSynPilule(u, r, p);
				editorkit::NkTooltip(ctx, u.Hit(r) && ctx.popupDepth == 0, p.bulle);
				xr = r.x - gap;
			}
			{
				NkSynPil p;
				p.icone = TEX(ic ? ic->gear : 0);
				p.val = "R\xC3\xA9glages";
				p.chevron = true;
				p.on = (E.deroule == 4);
				const float32 w = NkSynPiluleLargeur(u, p);
				const NkRect r = {xr - w, py, w, ph};
				if (NkSynPilule(u, r, p)) {
					E.deroule = (E.deroule == 4) ? 0 : 4;
					E.ancre = r;
					E.justeOuvert = true;
					consomme();
				}
				xr = r.x - gap;
			}
			// Le groupe d'execution : [Cible : X ▾] | ▶ deboguer construire arreter
			{
				const bool ws = s && s->HasWorkspace();
				const NkString cible = NkSyntheseCible(s);
				NkSynPil pc;
				pc.icone = TEX(ic ? ic->pkg : 0);
				pc.lib = "Cible :";
				pc.val = cible.CStr();
				pc.chevron = true;
				pc.fond = false;
				pc.off = !ws;
				pc.on = (E.deroule == 3);
				const float32 wc = NkSynPiluleLargeur(u, pc);
				const float32 wb = u.s(32.f);
				const float32 wg = wc + u.s(1.f) + 4.f * wb;
				const NkRect g = {xr - wg, py, wg, ph};
				u.Rect(g, a.panneau, u.s(8.f));
				NkSynContour(u, g, u.s(8.f), a.trait);
				float32 gx = g.x;
				const NkRect rc = {gx, py, wc, ph};
				if (NkSynPilule(u, rc, pc) && ws) {
					E.deroule = (E.deroule == 3) ? 0 : 3;
					E.ancre = rc;
					E.justeOuvert = true;
					consomme();
				}
				gx += wc;
				u.Rect({gx, py + (ph - u.s(16.f)) * 0.5f, u.s(1.f), u.s(16.f)}, a.trait);
				gx += u.s(1.f);
				struct Act {
						uint32 tex;
						bool on;
						const char *bulle;
						NkColor teinte;
				};
				const bool occupe = s && s->IsBuilding();
				const Act acts[4] = {
					{TEX(ic ? ic->play : 0), ws, "D\xC3\xA9marrer (jenga run)", NkColor{76, 195, 138, 255}},
					{TEX(ic ? ic->bug : 0), ws, "D\xC3\xA9" "boguer", NkColor{0, 0, 0, 0}},
					{TEX(ic ? ic->hammer : 0), ws, "Construire (jenga build) \xC2\xB7 Ctrl+B", NkColor{0, 0, 0, 0}},
					{TEX(ic ? ic->stop : 0), false, "\xC3\x80 venir : arr\xC3\xAAter la construction ou l'ex\xC3\xA9" "cution",
					 NkColor{0, 0, 0, 0}},
				};
				for (int32 k = 0; k < 4; ++k) {
					NkSynPil p;
					p.icone = acts[k].tex;
					p.carre = true;
					p.fond = false;
					p.off = !acts[k].on || (occupe && k < 3);
					if (acts[k].teinte.a && !p.off)
						p.teinteIcone = acts[k].teinte;
					p.bulle = acts[k].bulle;
					const NkRect r = {gx, py, wb, ph};
					if (NkSynPilule(u, r, p) && s) {
						if (k == 0 || k == 1)
							s->DoRun();
						else if (k == 2)
							s->DoBuildAction("build");
						consomme();
					}
					if (p.off)
						editorkit::NkTooltip(ctx, u.Hit(r) && ctx.popupDepth == 0, acts[k].bulle);
					gx += wb;
				}
				xr = g.x - gap;
			}
			// ── La palette, CENTREE dans la fenetre (380 px), si elle a la place ──
			{
				const float32 W = (float32)ctx.viewW;
				float32 pw = u.s(380.f);
				float32 px = W * 0.5f - pw * 0.5f;
				if (px < finGauche + gap)
					px = finGauche + gap;
				if (px + pw > xr - gap)
					pw = xr - gap - px;
				if (pw > u.s(160.f)) {
					const NkRect r = {px, py, pw, ph};
					const bool hov = u.Hit(r);
					u.Rect(r, hov ? a.haut : a.panneau, u.s(8.f));
					NkSynContour(u, r, u.s(8.f), a.trait);
					if (ic && ic->search)
						NkDrawIcon(u, ic->search, {r.x + u.s(12.f), r.y + (ph - u.s(15.f)) * 0.5f, u.s(15.f), u.s(15.f)},
								   a.fg3);
					u.TextV(r.x + u.s(36.f), r.y, ph, "Rechercher partout\xE2\x80\xA6", a.fg3);
					// La touche RÉELLE de la palette de commandes de la coquille.
					const char *k = "Ctrl+P";
					const float32 kw = u.TextW(k) * 0.86f + u.s(10.f);
					const NkRect kr = {r.x + r.w - u.s(10.f) - kw, r.y + (ph - u.s(18.f)) * 0.5f, kw, u.s(18.f)};
					u.Rect(kr, a.haut, u.s(4.f));
					NkSynContour(u, kr, u.s(4.f), a.trait);
					u.TextV(kr.x + u.s(5.f), kr.y, kr.h, k, a.fg2);
					if (hov && u.click && mb && mb->shell) {
						mb->shell->OpenCommandPalette();
						consomme();
					}
				}
			}
			// Les ecarts restent des zones de glissement de la fenetre ; les pilules
			// ont consomme leur clic.
			if (puits)
				ctx.menuBarX = x0 + 30.f;
			(void)home;
		}

		/// Le panneau ≡ (dessine dans l'overlay, au-dessus des ilots).
		inline void NkSyntheseMenuPanneau(NkEditorFrameContext &ec) {
			NkMenuSyntheseEtat &M = NkMenuSynthese();
			NkSyntheseEtat &E = NkSynthese();
			if (!M.ouvert || !E.mb || !NkSyntheseActive())
				return;
			auto &ctx = ec.Ui();
			NkMenuBarCtx *mb = E.mb;
			const NkApparencePalette &a = NkApparenceCourante().pal;
			const NkUi u = NkUi::From(ec, true);
			const float32 S = u.S;
			const float32 colCat = 190.f * S, colItems = 360.f * S;
			const float32 hRech = 44.f * S, hPied = 36.f * S, ligne = 30.f * S;
			// La colonne des actions prend la hauteur dont les menus longs (Edition) ont
			// besoin, sans sortir de la fenetre.
			const float32 y0 = M.ancre.y + M.ancre.h + 6.f * S;
			float32 hCats = 11.f * ligne + 12.f * S;
			const float32 hMax = (float32)ctx.viewH - y0 - hRech - hPied - 16.f * S;
			for (int32 k = 0; k < 11; ++k)
				if (M.hContenu[k] + 10.f * S > hCats)
					hCats = M.hContenu[k] + 10.f * S;
			if (hCats > hMax)
				hCats = hMax;
			const NkRect P = {M.ancre.x - 4.f * S, y0, colCat + colItems, hRech + hCats + hPied};
			ctx.PushOcclusion(P, 2);
			// L'ombre, le fond, le contour : le panneau se DETACHE nettement (« jamais cache ni coupe »).
			u.Rect({P.x - 2.f * S, P.y + 6.f * S, P.w + 4.f * S, P.h + 6.f * S},
				   NkColor{0, 0, 0, (uint8)(a.clair ? 46 : 140)}, 14.f * S);
			u.Rect(P, a.panneau, 12.f * S);
			NkSynContour(u, P, 12.f * S,
						 NkMelange(a.panneau, a.clair ? NkColor{15, 23, 42, 255} : NkColor{255, 255, 255, 255},
								   a.clair ? 0.16f : 0.12f));
			const NkVec2 m = ctx.input.mousePos;
			const bool dans = NkGuiRectContains(P, m);
			// ── La recherche : la palette de commandes (actions ET menus) ──
			{
				const NkRect r = {P.x, P.y, P.w, hRech};
				const bool hov = NkGuiRectContains(r, m);
				if (hov)
					u.Rect({r.x + 4.f * S, r.y + 4.f * S, r.w - 8.f * S, r.h - 8.f * S}, a.survol, 8.f * S);
				if (mb->dlg && mb->dlg->st && mb->dlg->st->icons && mb->dlg->st->icons->search)
					NkDrawIcon(u, mb->dlg->st->icons->search, {r.x + 14.f * S, r.y + (hRech - 15.f * S) * 0.5f, 15.f * S, 15.f * S},
							   a.fg3);
				u.TextV(r.x + 38.f * S, r.y, hRech, "Rechercher une action ou un menu\xE2\x80\xA6", a.fg3);
				const char *k = "Ctrl+P";
				const float32 kw = u.TextW(k) * 0.86f + 10.f * S;
				const NkRect kr = {r.x + r.w - 14.f * S - kw, r.y + (hRech - 18.f * S) * 0.5f, kw, 18.f * S};
				u.Rect(kr, a.haut, 4.f * S);
				u.TextV(kr.x + 5.f * S, kr.y, kr.h, k, a.fg2);
				u.Rect({P.x, r.y + hRech - 1.f, P.w, 1.f}, a.trait);
				if (hov && u.click && mb->shell) {
					mb->shell->OpenCommandPalette();
					M.ouvert = false;
					ctx.input.mouseClicked[0] = false;
					return;
				}
			}
			// ── Les onze categories ──
			const NkIcons *ic = (mb->dlg && mb->dlg->st) ? mb->dlg->st->icons : nullptr;
			auto icone = [&](int32 k) -> uint32 {
				if (!ic)
					return 0u;
				switch (k) {
					case 0: return ic->newFile2;
					case 1: return ic->editer;
					case 2: return ic->oeilOuvert;
					case 3: return ic->forward;
					case 4: return ic->play;
					case 5: return ic->bug;
					case 6: return ic->sourceControl;
					case 7: return ic->sparkles;
					case 8: return ic->toolchains;
					case 9: return ic->split;
					default: return ic->rondI;
				}
			};
			for (int32 k = 0; k < 11; ++k) {
				const NkRect r = {P.x + 6.f * S, P.y + hRech + 6.f * S + (float32)k * ligne, colCat - 12.f * S, ligne - 2.f * S};
				const bool hov = NkGuiRectContains(r, m);
				if (hov && M.cat != k)
					M.cat = k;
				if (M.cat == k)
					u.Rect(r, NkMelange(a.panneau, a.plein, a.clair ? 0.11f : 0.22f), 7.f * S);
				const uint32 t = icone(k);
				if (t)
					NkDrawIcon(u, t, {r.x + 10.f * S, r.y + (r.h - 15.f * S) * 0.5f, 15.f * S, 15.f * S}, a.fg2);
				u.TextV(r.x + 34.f * S, r.y, r.h, NkT(kNkMenusCles[k]), a.fg);
				NkSynChevronDroit(u, r.x + r.w - 12.f * S, r.y + r.h * 0.5f, a.fg3);
			}
			u.Rect({P.x + colCat, P.y + hRech, 1.f, hCats}, a.trait);
			// ── Le contenu de la categorie : les MEMES menus, dans un popup NKGui ──
			{
				const NkGuiId id = ctx.GetId("##menu-synthese");
				const NkRect zone = {P.x + colCat + 1.f, P.y + hRech + 1.f, colItems - 2.f * S, hCats - 2.f};
				if (M.justeOuvert || !ctx.IsPopupOpen(id))
					if (M.justeOuvert)
						ctx.OpenPopup(id);
				if (BeginPopupId(ctx, id, zone, P)) {
					const int32 av = mb->menuSeul;
					const bool avP = mb->sansPreambule;
					mb->menuSeul = M.cat;
					mb->sansPreambule = true;
					DrawMainMenuBar(ec, mb);
					if (M.cat >= 0 && M.cat < 11)
						M.hContenu[M.cat] = ctx.layout.maxY - zone.y;
					mb->menuSeul = av;
					mb->sansPreambule = avP;
					EndPopup(ctx);
				} else if (!M.justeOuvert)
					M.ouvert = false; // une action choisie (la chaine de menus s'est fermee) ou un clic dehors
			}
			// ── Le pied : quelques raccourcis reels ──
			{
				const NkRect r = {P.x, P.y + hRech + hCats, P.w, hPied};
				u.Rect({P.x, r.y, P.w, 1.f}, a.trait);
				float32 x = r.x + 14.f * S;
				u.TextV(x, r.y, r.h, "Raccourcis :", a.fg2);
				x += u.TextW("Raccourcis :") + 12.f * S;
				const char *rac[3] = {"Construire (Jenga) \xC2\xB7 Ctrl+B", "Palette \xC2\xB7 Ctrl+P", "Apparence\xE2\x80\xA6"};
				for (int32 i = 0; i < 3; ++i) {
					const NkRect zr = {x - 4.f * S, r.y + 6.f * S, u.TextW(rac[i]) + 8.f * S, r.h - 12.f * S};
					const bool hov = NkGuiRectContains(zr, m);
					u.TextV(x, r.y, r.h, rac[i], hov ? a.fg : a.fg3);
					if (hov && u.click) {
						if (i == 0 && mb->dlg && mb->dlg->st)
							mb->dlg->st->DoBuildAction("build");
						else if (i == 1 && mb->shell)
							mb->shell->OpenCommandPalette();
						else if (i == 2 && mb->dlg) {
							mb->dlg->showPrefs = true;
							if (mb->home)
								mb->home->settings.cat = 3;
						}
						M.ouvert = false;
						ctx.input.mouseClicked[0] = false;
					}
					x += u.TextW(rac[i]) + 18.f * S;
				}
			}
			if (ctx.input.KeyPressed(NkGuiKey::Escape))
				M.ouvert = false;
			if (dans) {
				ctx.input.mouseClicked[0] = false;
				ctx.input.wheel = 0.f;
			}
			M.justeOuvert = false;
		}

		inline void NkSyntheseBarreTitre(NkEditorFrameContext &ec, NkMenuBarCtx *mb) {
			NkSyntheseBarre(ec, mb, ec.Ui().menuBarRect, true);
		}

		// ═══════════════════════════════════════════════════════════════════════
		//  LES DEROULANTS DE LA BARRE (dessines dans l'overlay, apres les ilots)
		// ═══════════════════════════════════════════════════════════════════════
		inline void NkSyntheseDeroulants(NkEditorFrameContext &ec) {
			NkSyntheseEtat &E = NkSynthese();
			if (!E.deroule || !E.mb || !E.mb->dlg)
				return;
			auto &ctx = ec.Ui();
			NkCodeState *s = E.mb->dlg->st;
			NkHomeState *home = E.mb->home;
			const NkApparencePalette &a = NkApparenceCourante().pal;
			const NkUi u = NkUi::From(ec, true);
			struct L {
					NkString texte;
					int32 code; ///< action
					bool coche, titre, sep;
			};
			NkVector<L> lignes;
			auto ligne = [&](const NkString &t, int32 code, bool coche = false) {
				lignes.PushBack({t, code, coche, false, false});
			};
			auto titre = [&](const char *t) { lignes.PushBack({NkString(t), -1, false, true, false}); };
			auto sep = [&]() { lignes.PushBack({NkString(), -1, false, false, true}); };
			if (E.deroule == 1) {
				titre("Workspaces de ce dossier");
				if (s)
					for (usize i = 0; i < s->wsNames.Size(); ++i)
						ligne(s->wsNames[i], 100 + (int32)i, (int32)i == s->wsIdx);
				sep();
				ligne(NkString("Ouvrir un dossier\xE2\x80\xA6"), 1);
				ligne(NkString("Nouveau workspace Jenga\xE2\x80\xA6"), 2);
			} else if (E.deroule == 3 && s) {
				titre("Projet");
				for (usize i = 0; i < s->projects.Size(); ++i)
					ligne(s->projects[i], 1000 + (int32)i, (int32)i == s->projIdx);
				ligne(NkString(NkT("tb.allprojects")), 1000 + (int32)s->projects.Size(), s->AllProjects());
				sep();
				titre("Configuration");
				ligne(NkString("Debug"), 2000, s->cfgIdx == 0);
				ligne(NkString("Release"), 2001, s->cfgIdx == 1);
				sep();
				titre("Plateforme");
				int32 nSys = 0;
				const NkCodeState::SysDef *sysd = NkCodeState::Systems(&nSys);
				for (int32 i = 0; i < nSys; ++i)
					ligne(NkString(sysd[i].name), 3000 + i, i == s->sysIdx);
			} else if (E.deroule == 4 && home) {
				ligne(NkString("R\xC3\xA9glages\xE2\x80\xA6"), 10);
				sep();
				titre("Apparence");
				for (int32 k = 0; k < NK_APPARENCE_COUNT; ++k)
					ligne(NkString(NkApparenceNom(k)), 4000 + k, home->settings.apparence == k);
				if (home->settings.apparence == NK_APPARENCE_SYNTHESE)
					ligne(NkString("Barre de menus toujours visible"), 11, home->settings.menusVisibles);
				sep();
				titre("Th\xC3\xA8me");
				for (int32 k = 0; k < NK_THEME_COUNT; ++k)
					ligne(NkString(NkThemeNames()[k]), 5000 + k, home->settings.theme == k);
				sep();
				titre("Jeu d'ic\xC3\xB4nes");
				const NkJeuxIcones &J = NkCodeJeuxIcones();
				for (usize k = 0; k < J.jeux.Size(); ++k)
					if (J.jeux[k].installe)
						ligne(J.jeux[k].titre, 6000 + (int32)k, J.jeux[k].cle == J.Effectif(home->settings.jeuIcones));
			}
			if (lignes.Empty()) {
				E.deroule = 0;
				return;
			}
			const float32 ih = u.s(30.f), th = u.s(24.f), sh = u.s(11.f);
			float32 w = u.s(220.f), h = u.s(12.f);
			for (usize i = 0; i < lignes.Size(); ++i) {
				h += lignes[i].sep ? sh : lignes[i].titre ? th : ih;
				const float32 tw = u.TextW(lignes[i].texte.CStr()) + u.s(60.f);
				if (tw > w)
					w = tw;
			}
			float32 dx = E.ancre.x;
			if (dx + w > (float32)ctx.viewW - u.s(8.f))
				dx = E.ancre.x + E.ancre.w - w;
			const NkRect dd = {dx, E.ancre.y + E.ancre.h + u.s(6.f), w, h};
			ctx.PushOcclusion(dd, 2);
			u.Rect({dd.x, dd.y + u.s(4.f), dd.w, dd.h}, NkColor{0, 0, 0, (uint8)(a.clair ? 40 : 120)}, u.s(12.f));
			u.Rect(dd, a.panneau, u.s(12.f));
			NkSynContour(u, dd, u.s(12.f), NkMelange(a.panneau, a.clair ? NkColor{15, 23, 42, 255} : NkColor{255, 255, 255, 255}, a.clair ? 0.16f : 0.12f));
			float32 y = dd.y + u.s(6.f);
			int32 choisi = -1;
			for (usize i = 0; i < lignes.Size(); ++i) {
				const L &l = lignes[i];
				if (l.sep) {
					u.Rect({dd.x + u.s(8.f), y + sh * 0.5f, dd.w - u.s(16.f), 1.f}, a.trait);
					y += sh;
					continue;
				}
				if (l.titre) {
					u.TextV(dd.x + u.s(14.f), y, th, l.texte.CStr(), a.fg3);
					y += th;
					continue;
				}
				const NkRect r = {dd.x + u.s(6.f), y, dd.w - u.s(12.f), ih};
				const bool hov = NkGuiRectContains(r, u.mp);
				if (hov || l.coche)
					u.Rect(r, hov ? a.survol : NkMelange(a.panneau, a.plein, a.clair ? 0.11f : 0.22f), u.s(7.f));
				u.TextV(r.x + u.s(10.f), y, ih, l.texte.CStr(), a.fg);
				if (l.coche) { // ✓
					const float32 cx = r.x + r.w - u.s(16.f), cy = y + ih * 0.5f;
					u.dl->AddLine({cx - u.s(4.f), cy}, {cx - u.s(1.f), cy + u.s(3.f)}, a.accent, u.s(1.6f));
					u.dl->AddLine({cx - u.s(1.f), cy + u.s(3.f)}, {cx + u.s(5.f), cy - u.s(4.f)}, a.accent, u.s(1.6f));
				}
				if (hov && u.click)
					choisi = l.code;
				y += ih;
			}
			const bool dedans = NkGuiRectContains(dd, u.mp);
			if (choisi >= 0) {
				NkCodeDialogs *d = E.mb->dlg;
				if (choisi == 1)
					d->OpenFolderDialog();
				else if (choisi == 2) {
					d->showNewWs = true;
					d->wsAddAsRoot = true;
				} else if (choisi == 10) {
					d->showPrefs = true;
					if (home)
						home->settings.cat = 3;
				} else if (choisi == 11 && home) {
					home->settings.menusVisibles = !home->settings.menusVisibles;
					home->settings.Save();
				} else if (choisi >= 100 && choisi < 1000 && s) {
					s->wsIdx = choisi - 100;
					s->RequestReload();
				} else if (choisi >= 1000 && choisi < 2000 && s)
					s->projIdx = choisi - 1000;
				else if (choisi >= 2000 && choisi < 3000 && s)
					s->cfgIdx = choisi - 2000;
				else if (choisi >= 3000 && choisi < 4000 && s) {
					s->sysIdx = choisi - 3000;
					s->archIdx = 0;
				} else if (choisi >= 4000 && choisi < 5000 && home) {
					home->settings.apparence = choisi - 4000;
					home->settings.Save();
				} else if (choisi >= 5000 && choisi < 6000 && home) {
					home->settings.theme = choisi - 5000;
					home->settings.Save();
				} else if (choisi >= 6000 && home) {
					const NkJeuxIcones &J = NkCodeJeuxIcones();
					const usize k = (usize)(choisi - 6000);
					if (k < J.jeux.Size()) {
						NkStrCopy(home->settings.jeuIcones, sizeof(home->settings.jeuIcones), J.jeux[k].cle.CStr());
						home->settings.Save();
					}
				}
				E.deroule = 0;
			} else if (u.click && !dedans && !E.justeOuvert)
				E.deroule = 0;
			if (dedans) {
				ctx.input.mouseClicked[0] = false;
				ctx.input.wheel = 0.f;
			}
			E.justeOuvert = false;
		}

		// ═══════════════════════════════════════════════════════════════════════
		//  LA BANDE VERTICALE : extensions en haut, fenetres d'outils en bas
		// ═══════════════════════════════════════════════════════════════════════
		struct NkSynFenetre {
				const char *titre; ///< titre du panneau (cle de la coquille)
				const char *nom;   ///< info-bulle
				uint32 tex;
				void (*glyphe)(const NkUi &, const NkRect &, const NkColor &);
		};

		inline void NkSyntheseBande(NkGuiContext &ctx, const NkRect &r, void *user) {
			auto *home = static_cast<NkHomeState *>(user);
			NkEditorShell *sh = (home && home->dlg) ? home->dlg->shell : nullptr;
			NkCodeState *s = (home && home->dlg) ? home->dlg->st : nullptr;
			const NkApparencePalette &a = NkApparenceCourante().pal;
			const NkUi u = NkUiDe(ctx);
			const NkIcons *ic = s ? s->icons : nullptr;
			const float32 b = u.s(34.f), pas = u.s(38.f);
			const float32 cx = r.x + u.s(8.f) + u.s(20.f); // bande de 40 apres 8 de marge
			// Le nombre de problemes (badge), comme le panneau Problemes les compte.
			int32 nProb = 0;
			if (s)
				nProb = (int32)s->buildDiags.Size();
			auto bouton = [&](float32 y, uint32 tex, void (*gl)(const NkUi &, const NkRect &, const NkColor &),
							  bool on, int32 badge, const char *bulle, bool off) -> bool {
				const NkRect br = {cx - b * 0.5f, y, b, b};
				const bool hov = !off && u.Hit(br);
				if (on) {
					u.Rect(br, a.panneau, u.s(9.f));
					NkSynContour(u, br, u.s(9.f), a.trait);
					u.Rect({br.x - u.s(6.f), br.y + u.s(9.f), u.s(3.f), b - u.s(18.f)}, a.accent, u.s(2.f));
				} else if (hov)
					u.Rect(br, a.survol, u.s(9.f));
				const NkColor c = on ? a.accent : (off ? a.fg3 : (hov ? a.fg : a.fg2));
				const NkRect ir = {cx - u.s(9.f), y + (b - u.s(18.f)) * 0.5f, u.s(18.f), u.s(18.f)};
				if (tex)
					NkDrawIcon(u, tex, ir, c);
				else if (gl)
					gl(u, ir, c);
				if (badge > 0) {
					const NkString t = NkPrintf("%d", badge);
					const float32 bw = u.TextW(t.CStr()) * 0.75f + u.s(6.f);
					const NkRect bb = {br.x + br.w - u.s(1.f) - (bw > u.s(14.f) ? bw : u.s(14.f)), br.y + u.s(1.f),
									   bw > u.s(14.f) ? bw : u.s(14.f), u.s(14.f)};
					u.Rect(bb, a.plein, u.s(7.f));
					u.TextV(bb.x + (bb.w - u.TextW(t.CStr())) * 0.5f, bb.y, bb.h, t.CStr(), NkColor{255, 255, 255, 255});
				}
				editorkit::NkTooltip(ctx, u.Hit(br) && ctx.popupDepth == 0, bulle);
				return hov && u.click;
			};
			// ── EN BAS : les fenetres d'outils, des INTERRUPTEURS (plusieurs ouvertes) ──
			static const NkSynFenetre kFen[] = {
				{"Assistant IA", "Assistant IA", 0, nullptr}, {nullptr, nullptr, 0, nullptr},
				{"Terminal", "Terminal", 0, nullptr},		   {"Sortie", "Sortie", 0, &NkSynSortie},
				{"Probl\xC3\xA8mes", "Probl\xC3\xA8mes", 0, nullptr}, {"Ex\xC3\xA9" "cution", "Ex\xC3\xA9" "cution", 0, nullptr},
				{"Profiler", "Profilage", 0, nullptr},
			};
			const int32 n = (int32)(sizeof(kFen) / sizeof(kFen[0]));
			float32 h = 0.f;
			for (int32 k = 0; k < n; ++k)
				h += kFen[k].titre ? pas : u.s(9.f);
			float32 y = r.y + r.h - u.s(4.f) - h;
			for (int32 k = 0; k < n; ++k) {
				if (!kFen[k].titre) { // le trait entre l'IA et les autres
					u.Rect({cx - u.s(10.f), y + u.s(4.f), u.s(20.f), 1.f}, a.trait);
					y += u.s(9.f);
					continue;
				}
				uint32 tex = 0;
				if (ic) {
					if (k == 0)
						tex = ic->sparkles;
					else if (k == 2)
						tex = ic->kConsole;
					else if (k == 4)
						tex = ic->warning;
					else if (k == 5)
						tex = ic->play;
					else if (k == 6)
						tex = ic->chart;
				}
				bool on = sh && sh->IsPanelOpen(kFen[k].titre);
				if (k == 0 && sh) { // l'IA : n'importe lequel des quatre assistants
					static const char *kAi[4] = {"Assistant IA", "Claude Code", "Codex", "NkAI"};
					for (int32 q = 0; q < 4 && !on; ++q)
						on = sh->IsPanelOpen(kAi[q]);
				}
				if (bouton(y + (pas - b) * 0.5f, tex, kFen[k].glyphe, on, k == 4 ? nProb : 0, kFen[k].nom, false) && sh) {
					if (k == 0) {
						int32 gn = 0;
						const char *const *g = SideRightGroup(gn);
						static const char *kAi[4] = {"Assistant IA", "Claude Code", "Codex", "NkAI"};
						bool fermer = false;
						for (int32 q = 0; q < 4; ++q)
							if (sh->IsPanelOpen(kAi[q])) {
								sh->ClosePanel(kAi[q]);
								sh->DetachPanel(kAi[q]);
								fermer = true;
							}
						if (!fermer)
							OpenSideExclusive(sh, g, gn, "Assistant IA");
					} else if (on)
						sh->ClosePanel(kFen[k].titre);
					else
						sh->FocusPanel(kFen[k].titre);
					ctx.input.mouseClicked[0] = false;
				}
				y += pas;
			}
			// ── EN HAUT : les extensions qui contribuent une VUE ou une FENETRE. Les
			// seules extensions installables aujourd'hui sont des jeux d'icones (sans
			// vue) : le groupe du haut est vide tant qu'aucune n'en apporte -- pas de
			// fausse extension pour remplir la maquette.
		}

		// ═══════════════════════════════════════════════════════════════════════
		//  LES ILOTS : coins arrondis + contour de chaque feuille du dock
		// ═══════════════════════════════════════════════════════════════════════
		inline void NkSyntheseCoins(NkGuiContext &ctx) {
			const NkApparenceEtat &e = NkApparenceCourante();
			if (!e.dispo.ilots || ctx.appFullScreen)
				return;
			const NkApparencePalette &a = e.pal;
			NkGuiDrawList &dl = ctx.dlOverlay;
			const float32 R = ctx.S(e.dispo.ilotRayon);
			const int32 N = 8;
			for (usize i = 0; i < ctx.dockNodes.Size(); ++i) {
				const NkGuiDockNode &nd = ctx.dockNodes[i];
				if (nd.kind != 2 || nd.winCount <= 0 || nd.rect.w < 2.f * R || nd.rect.h < 2.f * R)
					continue;
				const NkRect r = nd.rect;
				// Quatre coins : le coin P, le centre C de l'arc, l'angle de depart.
				const NkVec2 P[4] = {{r.x, r.y}, {r.x + r.w, r.y}, {r.x + r.w, r.y + r.h}, {r.x, r.y + r.h}};
				const NkVec2 C[4] = {{r.x + R, r.y + R}, {r.x + r.w - R, r.y + R}, {r.x + r.w - R, r.y + r.h - R},
									 {r.x + R, r.y + r.h - R}};
				const float32 kPi = 3.14159265f;
				const float32 a0[4] = {kPi, 1.5f * kPi, 0.f, 0.5f * kPi};
				for (int32 c = 0; c < 4; ++c) {
					NkVec2 prev = {C[c].x + R * math::NkCos(a0[c]), C[c].y + R * math::NkSin(a0[c])};
					for (int32 k = 1; k <= N; ++k) {
						const float32 t = a0[c] + (0.5f * kPi) * ((float32)k / (float32)N);
						const NkVec2 cur = {C[c].x + R * math::NkCos(t), C[c].y + R * math::NkSin(t)};
						dl.AddTriangleFilled(P[c], prev, cur, a.gouttiere);
						prev = cur;
					}
				}
				dl.AddRect(r, a.trait, 1.f, R);
			}
		}

		// ═══════════════════════════════════════════════════════════════════════
		//  LA BARRE D'ETAT : Tutoriel / Capture / Direct, puis l'etat
		// ═══════════════════════════════════════════════════════════════════════
		/// Le langage du fichier, tel que la barre d'etat le nomme.
		inline const char *NkSynLangage(const NkPath &p) {
			const NkString e = p.GetExtension();
			const char *x = e.CStr();
			struct M {
					const char *ext, *nom;
			};
			static const M kM[] = {{".cpp", "C++"},	   {".cc", "C++"},	  {".cxx", "C++"},	  {".h", "C++"},
								   {".hpp", "C++"},	   {".inl", "C++"},	  {".c", "C"},		  {".py", "Python"},
								   {".jenga", "Jenga"}, {".md", "Markdown"}, {".json", "JSON"},	  {".lua", "Lua"},
								   {".rs", "Rust"},	   {".zig", "Zig"},	  {".nksl", "NKSL"},  {".cs", "C#"},
								   {".js", "JavaScript"}, {".ts", "TypeScript"}, {".txt", "Texte"}, {".cfg", "Config"}};
			for (const M &m : kM)
				if (StrEq(x, m.ext))
					return m.nom;
			return "Texte";
		}

		inline void NkSyntheseBarreEtat(NkEditorFrameContext &ec, void *user) {
			auto *home = static_cast<NkHomeState *>(user);
			auto &ctx = ec.Ui();
			const NkRect r = ctx.layout.region;
			const NkApparencePalette &a = NkApparenceCourante().pal;
			const NkUi u = NkUi::From(ec);
			NkCodeState *s = (home && home->dlg) ? home->dlg->st : nullptr;
			const NkIcons *ic = s ? s->icons : nullptr;
			u.Rect(r, a.barreEtat);
			float32 x = r.x + u.s(10.f);
			const float32 cy = r.y + r.h * 0.5f;
			// Les trois pastilles de pied (regle de la famille) : a venir.
			struct Eb {
					const char *t;
					void (*gl)(const NkUi &, const NkRect &, const NkColor &);
					bool chevron;
					const char *bulle;
			};
			const Eb ebs[3] = {
				{"Tutoriel", &NkSynVideo, true, "\xC3\x80 venir : enregistrer un tutoriel vid\xC3\xA9o de toute l'application"},
				{"Capture", &NkSynPhoto, false, "\xC3\x80 venir : photo de la fen\xC3\xAAtre ou de l'\xC3\xA9" "diteur"},
				{"Direct", &NkSynDirect, false, "\xC3\x80 venir : diffuser en direct (YouTube, Twitch, RTMP)"},
			};
			for (const Eb &e : ebs) {
				const float32 w = u.s(8.f) + u.s(13.f) + u.s(5.f) + u.TextW(e.t) + (e.chevron ? u.s(14.f) : 0.f) + u.s(8.f);
				const NkRect br = {x, cy - u.s(11.f), w, u.s(22.f)};
				u.Rect(br, a.panneau, u.s(6.f));
				NkSynContour(u, br, u.s(6.f), a.trait);
				e.gl(u, {br.x + u.s(8.f), cy - u.s(6.5f), u.s(13.f), u.s(13.f)}, a.fg2);
				u.TextV(br.x + u.s(26.f), br.y, br.h, e.t, a.fg2);
				if (e.chevron)
					NkSynChevronBas(u, br.x + br.w - u.s(12.f), cy, a.fg3);
				editorkit::NkTooltip(ctx, u.Hit(br) && ctx.popupDepth == 0, e.bulle);
				x += w + u.s(4.f);
			}
			x += u.s(4.f);
			auto item = [&](uint32 tex, const char *t, const NkColor &c) {
				if (tex) {
					NkDrawIcon(u, tex, {x, cy - u.s(6.5f), u.s(13.f), u.s(13.f)}, c);
					x += u.s(18.f);
				}
				u.TextV(x, r.y, r.h, t, c);
				x += u.TextW(t) + u.s(14.f);
			};
			const bool aFichier = s && s->HasActive();
			if (home)
				home->settings.EnsureDetected(); // version de Jenga (fil de fond, une fois)
			NkString jv = (home && !home->settings.jengaVersion.Empty()) ? home->settings.jengaVersion
																		 : NkEmbeddedJenga::EmbeddedVersion();
			if (aFichier && s->HasWorkspace()) {
				const NkString &br = NkSyntheseBranche(s, 0.f);
				if (!br.Empty())
					item(ic ? ic->sourceControl : 0, br.CStr(), a.fg2);
				int32 nW = 0, nE = 0;
				for (usize i = 0; i < s->buildDiags.Size(); ++i)
					(s->buildDiags[i].sev == NkDiagSev::Error ? nE : nW)++;
				if (nE > 0)
					item(ic ? ic->warning : 0, NkPrintf("%d", nE).CStr(), NkColor{248, 81, 73, 255});
				if (nW > 0)
					item(ic ? ic->warning : 0, NkPrintf("%d", nW).CStr(), a.clair ? NkColor{166, 110, 0, 255} : NkColor{224, 179, 90, 255});
				static const char *kCfg[] = {"Debug", "Release", "Toutes"};
				const int32 ci = (s->cfgIdx >= 0 && s->cfgIdx < 3) ? s->cfgIdx : 0;
				const char *proj = s->projects.Empty() ? "" : (s->AllProjects() ? NkT("tb.allprojects") : s->SelectedProject());
				item(ic ? ic->jenga : 0,
					 NkPrintf("Jenga %s \xC2\xB7 %s \xC2\xB7 %s", jv.Empty() ? "?" : jv.CStr(), proj, kCfg[ci]).CStr(), a.fg2);
				if (s->IsBuilding())
					item(0, s->status.CStr(), a.accent);
				// A droite : position, indentation, encodage, fins de ligne, langage.
				const OpenFile &f = s->files[s->active];
				const NkString droite[5] = {NkPrintf("Ln %d, Col %d", f.doc.curLine + 1, f.doc.curCol + 1),
											NkString("Espaces : 4"), NkString("UTF-8"), NkString("CRLF"),
											NkString(NkSynLangage(f.path))};
				float32 xr = r.x + r.w - u.s(10.f);
				for (int32 k = 4; k >= 0; --k) {
					const float32 w = u.TextW(droite[k].CStr());
					xr -= w;
					u.TextV(xr, r.y, r.h, droite[k].CStr(), a.fg2);
					xr -= u.s(14.f);
				}
			} else {
				item(0, NkPrintf("NKCode %s", NkCodeVersion()).CStr(), a.fg2);
				if (!jv.Empty())
					item(0, NkPrintf("Jenga %s", jv.CStr()).CStr(), a.fg2);
				const char *z = "Zoom 100 %";
				u.TextV(r.x + r.w - u.s(10.f) - u.TextW(z), r.y, r.h, z, a.fg2);
			}
		}

		// ═══════════════════════════════════════════════════════════════════════
		//  LES BRANCHEMENTS (a chaque image, depuis l'applicateur d'apparence)
		// ═══════════════════════════════════════════════════════════════════════
		inline void NkSyntheseToolbarThunk(NkEditorFrameContext &ec, void *) {
			// Variante 3 (« barre de menus toujours visible ») : la barre Synthese
			// descend dans la rangee d'outils.
			NkSyntheseEtat &E = NkSynthese();
			if (!E.mb)
				return;
			const NkRect r = ec.Ui().layout.region;
			NkUi::From(ec).Rect(r, NkApparenceCourante().pal.cadre);
			NkSyntheseBarre(ec, E.mb, r, false);
		}

		inline void NkSyntheseBranchements(NkEditorShell *sh, NkHomeState *home, const NkApparenceEtat &e) {
			if (!sh)
				return;
			if (e.dispo.synthese) {
				sh->SetActivityBarFn(&NkSyntheseBande, home);
				sh->SetStatusBarFn(&NkSyntheseBarreEtat, home);
				// Variante 3 : la barre Synthese dans la rangee d'outils, sous les menus.
				if (e.dispo.menusVisibles) {
					NkBarreOutilsVisible() = false; // l'IDE bat par la barre de menus
					sh->SetToolbar(&NkSyntheseToolbarThunk, nullptr);
				}
			} else {
				sh->SetActivityBarFn(nullptr, nullptr);
				sh->SetStatusBarFn(nullptr, nullptr);
				// Famille (maquette B) : la rangee de widgets aux boutons bordes, sous les
				// menus, a cote du logo au coin -- la barre de la Synthese, sans le ≡.
				NkCodeState *st = (home && home->dlg) ? home->dlg->st : nullptr;
				if (e.dispo.styleFamille && st && st->HasWorkspace()) {
					NkBarreOutilsVisible() = false; // l'IDE bat par la barre de menus
					sh->SetToolbar(&NkSyntheseToolbarThunk, nullptr);
				}
			}
		}

		/// La barre de menus de NKCode, toutes apparences : en Synthese (variante 2),
		/// la barre unique ; sinon les menus d'avant.
		inline void NkCodeMenuBarThunk(NkEditorFrameContext &ec, void *user) {
			auto *mb = static_cast<NkMenuBarCtx *>(user);
			NkSynthese().mb = mb;
			const NkApparenceDispo &d = NkApparenceCourante().dispo;
			// Synthese : une construction qui DEMARRE amene le Terminal (sa session
			// « jenga build » y montre la sortie mise en forme).
			if (mb && mb->dlg && mb->dlg->st && mb->shell && !mb->dlg->st->focusPanelReq.Empty()) {
				// (01/10) la Sortie n'est plus seule a les lire : elle peut etre fermee
				mb->shell->FocusPanel(mb->dlg->st->focusPanelReq.CStr());
				mb->dlg->st->focusPanelReq = NkString();
			}
			if (d.synthese && mb && mb->dlg && mb->dlg->st && mb->shell) {
				static bool sBat = false;
				const bool bat = mb->dlg->st->IsBuilding();
				if (bat && !sBat)
					mb->shell->FocusPanel("Terminal");
				sBat = bat;
			}
			if (d.synthese && !d.menusVisibles) {
				if (!NkBarreOutilsVisible() && mb && mb->dlg && mb->dlg->st)
					NkCodeBattementIde(ec, mb->dlg->st);
				NkSyntheseBarreTitre(ec, mb);
				return;
			}
			MainMenuBarThunk(ec, user);
		}

	} // namespace nkcode
} // namespace nkentseu
