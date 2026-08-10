#pragma once
// =============================================================================
// NkModelerWelcome.h — l'ECRAN D'ACCUEIL (demande de Rihen).
//
// Affiche au lancement TANT QU'AUCUN PROJET N'EST OUVERT, en plein ecran DANS
// la fenetre du modeleur : pas une seconde fenetre, pas un splash. Il disparait
// des qu'un projet est ouvert ou cree.
//
// CE QU'IL MONTRE, DANS L'ORDRE D'IMPORTANCE VOULU
//   1. les PROJETS RECENTS, avec leur image de couverture -- c'est la raison
//      d'etre de l'ecran : neuf ouvertures sur dix reprennent un projet ;
//   2. les deux actions, Nouveau et Ouvrir ;
//   3. les liens externes.
//
// ⚠ LE LOGO N'EXISTE PAS. Il n'y a aucune image de marque dans le depot
// (verifie). Le titre est donc COMPOSE typographiquement -- une marque
// geometrique dessinee au painter (un cube isometrique, ce que fait
// l'application) plus le mot en trois poids -- avec les ROLES DE COULEUR du
// theme, jamais des valeurs en dur : un theme clair le repeint tout seul.
// **Un vrai logo reste a fournir**, et ce commentaire doit disparaitre le jour
// ou il arrive.
//
// ⚠ LES URL N'EXISTENT PAS ENCORE. Elles sont declarees vides ci-dessous, en
// tete de fichier, marquees « a renseigner ». Un lien dont l'URL est vide
// s'affiche GRISE ET INACTIF -- jamais un lien mort : la regle du depot est
// qu'une commande affichee fait ce qu'elle annonce.
// =============================================================================

#include "NK3DModeler/Shell/NkModelerUI.h"
#include "NK3DModeler/Shell/NkModelerInput.h"
#include "NK3DModeler/Shell/NkModelerWidgets.h"
#include "NK3DModeler/Project/NkModelerProject.h"
#include "NK3DModeler/Project/NkModelerScene.h"	 // lecteur HERITE (tout dans le .nk3dm)
#include "NK3DModeler/Project/NkModelerAssets.h" // un fichier par asset (disposition 3)
#include "NKWindow/Core/NkLauncher.h" // ouvre le navigateur du systeme
#include "NKWindow/Core/NkDialogs.h"  // selecteurs natifs DEJA presents dans le depot
#include "NKEditorKit/NkIEditorRenderer.h"
#include "NKImage/NKImage.h"
#include "NKFileSystem/NkFile.h"
#include "NKFileSystem/NkDirectory.h"

#include <cstdio>

namespace nkentseu {
	namespace nk3d {

		// ── LIENS EXTERNES — A RENSEIGNER ───────────────────────────────────────
		// Les adresses n'existent pas encore. Tant qu'une chaine est VIDE, son lien
		// est grise et ne repond pas : c'est le seul comportement honnete. Remplir
		// la chaine suffit a l'activer, il n'y a rien d'autre a toucher.
		static const char *const kUrlSite = ""; // A RENSEIGNER — site NK3DModeler
		static const char *const kUrlTutos = ""; // A RENSEIGNER — tutoriels
		static const char *const kUrlCommu = ""; // A RENSEIGNER — communaute
		static const char *const kUrlDocs = ""; // A RENSEIGNER — documentation

		// ── IMAGE DE VERSION (façon Blender) — decision de Rihen, 5 aout ────────
		// Chaque version du logiciel est accompagnee d'une IMAGE realisee avec
		// lui, creditee a son auteur. Elle ne demande AUCUN serveur : elle est
		// livree dans `data/splash/`, ce qui la distingue de tout le reste du
		// contenu « editorial » (nouveautes, communaute, marketplace) qui, lui,
		// suppose un service en ligne inexistant. C'est le seul contenu de ce
		// genre qu'on puisse honnetement afficher aujourd'hui.
		//
		// Le fichier de credit `data/splash/splash.txt` porte DEUX lignes :
		//   1. le titre de l'oeuvre
		//   2. le nom de son auteur
		// Sans image, la bande ne s'affiche PAS DU TOUT -- pas de cadre vide,
		// pas d'image d'emprunt.
		static const char *const kSplashImage = "data/splash/splash.png";
		static const char *const kSplashCredits = "data/splash/splash.txt";
		static const uint32 kSplashTexId = 4599u; ///< juste sous les couvertures
		// Version affichee. Elle DOIT suivre `appversion(...)` du .jenga : deux
		// numeros differents sur le meme binaire feraient mentir l'un des deux.
		static const char *const kAppVersion = "0.1.0";

		// Plage d'identifiants de texture des VIGNETTES de couverture. Elle suit
		// celle des matcaps (4300+) sans la recouvrir : les identifiants sont
		// attribues a la main dans cette application, une collision passerait
		// inapercue jusqu'a ce qu'une image en remplace une autre.
		static const uint32 kCoverTexBase = 4600u;
		static const int32 kCoverMax = 16; ///< au-dela, la liste defile sans vignette

		// Etat de l'image de version, resolu UNE fois au demarrage (decoder un
		// PNG a chaque image couterait plus cher que tout l'ecran).
		struct NkSplashArt {
				bool tried = false;   ///< tentative faite (succes ou non)
				bool valid = false;   ///< image chargee et publiee
				uint32 w = 0, h = 0;  ///< dimensions natives, pour le rapport
				char title[96] = {};  ///< 1re ligne de splash.txt
				char author[96] = {}; ///< 2e ligne
		};

		// Charge l'image de version et ses credits. Idempotent : la deuxieme
		// tentative ne fait rien, meme si la premiere a echoue -- une image
		// absente est un etat normal, pas une erreur a reessayer chaque frame.
		inline void NkSplashLoad(editorkit::NkIEditorRenderer &renderer, NkSplashArt &art) {
			if (art.tried)
				return;
			art.tried = true;
			NkImage img;
			if (!img.Load(kSplashImage, 4) || !img.IsValid())
				return; // pas d'image livree : la bande n'existera pas
			art.w = (uint32)img.Width();
			art.h = (uint32)img.Height();
			renderer.UploadImageRGBA(kSplashTexId, (const uint8 *)img.Pixels(), (int32)art.w,
									 (int32)art.h);
			art.valid = true;
			// Credits : deux lignes, facultatives. Une image sans credit
			// s'affiche quand meme -- mais un credit vide ne s'invente pas.
			const NkString txt = NkFile::ReadAllText(NkPath(kSplashCredits));
			if (txt.Empty())
				return;
			const char *s = txt.CStr();
			int32 line = 0, k = 0;
			char *dst = art.title;
			for (usize i = 0; s[i] && line < 2; ++i) {
				if (s[i] == '\n' || s[i] == '\r') {
					if (k == 0)
						continue; // saute les fins de ligne consecutives
					dst[k] = 0;
					++line;
					k = 0;
					dst = art.author;
					continue;
				}
				if (k < 94)
					dst[k++] = s[i];
			}
			dst[k] = 0;
		}

		// ── VIGNETTES : CHARGEMENT ──────────────────────────────────────────────
		// Appele par la boucle principale quand la liste change (`texDirty`), pas
		// a chaque image : decoder un PNG par frame couterait plus cher que tout
		// le reste de l'ecran.
		inline void NkWelcomeUploadCovers(editorkit::NkIEditorRenderer &renderer,
										  NkRecentList &rec) {
			if (!rec.texDirty)
				return;
			rec.texDirty = false;
			const int32 n = (int32)rec.items.Size();
			for (int32 i = 0; i < n; ++i) {
				rec.items[(usize)i].tex = 0;
				if (i >= kCoverMax)
					continue;
				const NkString abs = rec.items[(usize)i].CoverAbs();
				if (abs.Empty() || !NkFile::Exists(abs.CStr()))
					continue;
				NkImage img;
				if (!img.Load(abs.CStr(), 4) || !img.IsValid())
					continue;
				// Reduite AVANT le televersement : une couverture 4K occuperait
				// 32 Mo de VRAM pour une vignette de 160 px.
				NkImage *small = img.Resize(256, 144);
				NkImage *use = (small && small->IsValid()) ? small : &img;
				const uint32 tex = kCoverTexBase + (uint32)i;
				if (renderer.UploadImageRGBA(tex, use->Pixels(), use->Width(), use->Height()))
					rec.items[(usize)i].tex = tex;
				if (small)
					small->Free();
			}
		}

		// ── LA MARQUE, COMPOSEE AU PAINTER ──────────────────────────────────────
		// Un cube isometrique : trois losanges, trois valeurs. C'est ce que fait
		// l'application, et c'est reconnaissable a n'importe quelle taille sans
		// dependre d'une image. Les couleurs viennent des roles du theme --
		// la sarcelle des noeuds de donnees et les deux accents.
		inline void PaintBrandMark(NkModelerPainter &p, float32 x, float32 y, float32 s) {
			const float32 hx = s * 0.5f, qy = s * 0.25f;
			const NkVec2 top{x + hx, y};
			const NkVec2 rgt{x + s, y + qy};
			const NkVec2 lft{x, y + qy};
			const NkVec2 mid{x + hx, y + qy * 2.f};
			const NkVec2 bot{x + hx, y + s};
			const NkVec2 rgb{x + s, y + qy * 3.f};
			const NkVec2 lfb{x, y + qy * 3.f};

			const NkColor cTop = p.C(NkRole::NodeDataHeader); // sarcelle : la face eclairee
			const NkColor cLft = p.C(NkRole::AccentUi);
			const NkColor cRgt = p.C(NkRole::AccentSel);
			// Chaque losange = deux triangles ; le painter n'expose que le triangle
			// tricolore, ce qui suffit et evite d'ajouter une primitive pour un
			// seul dessin.
			p.TriColor(top, rgt, mid, cTop, cTop, cTop);
			p.TriColor(top, mid, lft, cTop, cTop, cTop);
			p.TriColor(lft, mid, bot, cLft, cLft, cLft);
			p.TriColor(lft, bot, lfb, cLft, cLft, cLft);
			p.TriColor(mid, rgt, rgb, cRgt, cRgt, cRgt);
			p.TriColor(mid, rgb, bot, cRgt, cRgt, cRgt);
		}

		// ── UN CHAMP DE SAISIE DE BOITE MODALE ──────────────────────────────────
		// Le champ en place de NkModelerWidgets s'ouvre au DOUBLE-clic, ce qui a du
		// sens dans une liste (le simple clic y selectionne) mais pas dans une
		// boite ou il n'y a rien d'autre a faire que taper. Meme brique de saisie
		// (celle de NKEditorKit), simple clic pour entrer dedans.
		inline void WelcomeField(NkModelerPainter &p, NkHitRegistry &hit, NkWidgetState &ws,
								 const nkgui::NkGuiInput &in, const char *key, const NkRect &r,
								 char *buf, uint32 cap) {
			const bool over = hit.Add(key, r);
			if (!ws.IsEditing(key)) {
				p.Outline(r, over ? NkRole::AccentUi : NkRole::Border, NkRole::InputBg, 3.f);
				p.Clip({r.x + S(4.f), r.y, r.w - S(8.f), r.h});
				p.TextV(r.x + S(6.f), r.y, r.h, buf, buf[0] ? NkRole::Text : NkRole::TextMuted);
				p.Unclip();
				if (hit.Clicked(key))
					ws.BeginEdit(key, buf);
				return;
			}
			p.Outline(r, NkRole::AccentUi, NkRole::InputBg, 3.f);
			if (nkgui::NkGuiContext *gc = NkUiCtx()) {
				editorkit::NkOverlayTextField(*gc, gc->dl, p.FontPtr(), r, ws.editBuf,
											  (int32)(cap < 259u ? cap : 259u), true);
				uint32 n = 0;
				while (ws.editBuf[n])
					++n;
				ws.editLen = n;
			}
			// RECOPIE CONTINUE, et c'est voulu : le bouton « Creer » et la ligne
			// « sera cree ici » doivent montrer ce qui est tape MAINTENANT. Attendre
			// une validation ferait un bouton qui agit sur un nom perime.
			{
				uint32 i = 0;
				for (; ws.editBuf[i] && i + 1u < cap; ++i)
					buf[i] = ws.editBuf[i];
				buf[i] = 0;
			}
			if (in.KeyPressed(nkgui::NkGuiKey::Enter) || in.KeyPressed(nkgui::NkGuiKey::Escape))
				ws.EndEdit();
			else if (hit.AnyClick() && !hit.IsHovered(key))
				ws.EndEdit();
		}

		// Bouton plein / bouton contour, la paire de l'ecran. Renvoie true au clic.
		inline bool WelcomeButton(NkModelerPainter &p, NkHitRegistry &hit, const char *key,
								  const NkRect &r, const char *label, NkIcon ic, bool primary,
								  bool enabled = true) {
			const bool over = enabled && hit.Add(key, r);
			if (!enabled) {
				// GRISE ET INACTIF : la zone n'est meme pas declaree, donc il n'y a
				// rien a cliquer -- pas seulement rien qui se passe.
				p.Outline(r, NkRole::Border, NkRole::PanelBg, 4.f);
				if (ic != NkIcon::None)
					p.IconV(r.x + S(12.f), r.y, r.h, ic, NkRole::TextMuted, 14.f);
				p.TextV(r.x + (ic != NkIcon::None ? S(34.f) : S(14.f)), r.y, r.h, label,
						NkRole::TextMuted);
				return false;
			}
			if (primary)
				p.Fill(r, over ? NkRole::AccentSel : NkRole::AccentUi, 4.f);
			else
				p.Outline(r, over ? NkRole::AccentUi : NkRole::Border,
						  over ? NkRole::PanelHeader : NkRole::PanelBg, 4.f);
			const NkRole txt = primary ? NkRole::TextOnAccent : NkRole::Text;
			if (ic != NkIcon::None)
				p.IconV(r.x + S(12.f), r.y, r.h, ic, txt, 14.f);
			p.TextV(r.x + (ic != NkIcon::None ? S(34.f) : S(14.f)), r.y, r.h, label, txt);
			if (over)
				hit.WantCursor(NkCursorWant::Hand);
			return hit.Clicked(key);
		}

		// ── BOITE « NOUVEAU PROJET » ────────────────────────────────────────────
		// Un projet = un DOSSIER + un .nk3dm a sa racine : creer demande donc un
		// emplacement ET un nom. Le chemin exact qui sera cree est affiche en
		// toutes lettres -- sans quoi personne ne sait ou son travail atterrit.
		inline void PaintNewProjectDialog(NkModelerPainter &p, float32 W, float32 H,
										  NkModelerState &st, NkHitRegistry &hit,
										  NkWidgetState &ws, const nkgui::NkGuiInput &in) {
			if (!st.newProjOpen)
				return;
			p.Fill({0.f, 0.f, W, H}, NkColor{0, 0, 0, 150});
			hit.Add("np.veil", {0.f, 0.f, W, H});

			const float32 bw = S(560.f), bh = S(260.f);
			const NkRect box{(W - bw) * 0.5f, (H - bh) * 0.5f, bw, bh};
			p.Outline(box, NkRole::Border, NkRole::PanelHeader, 6.f);
			hit.Add("np.box", box);
			p.TextV(box.x + S(20.f), box.y + S(12.f), S(24.f), "Nouveau projet");

			const float32 lx = box.x + S(20.f), fx = box.x + S(120.f);
			const float32 fw = bw - S(140.f) - S(20.f);
			float32 y = box.y + S(52.f);

			p.TextV(lx, y, S(26.f), "Nom", NkRole::TextMuted);
			WelcomeField(p, hit, ws, in, "np.name", {fx, y, fw, S(26.f)}, st.newProjName,
						 (uint32)sizeof(st.newProjName));
			y += S(36.f);

			p.TextV(lx, y, S(26.f), "Emplacement", NkRole::TextMuted);
			const float32 browW = S(96.f);
			WelcomeField(p, hit, ws, in, "np.dir", {fx, y, fw - browW - S(8.f), S(26.f)},
						 st.newProjDir, (uint32)sizeof(st.newProjDir));
			if (WelcomeButton(p, hit, "np.browse", {fx + fw - browW, y, browW, S(26.f)},
							  "Parcourir", NkIcon::Folder, false))
				st.projPending = 5; // selecteur de dossier : apres la frame
			y += S(40.f);

			// LE CHEMIN REEL, ecrit noir sur blanc. C'est la seule facon de ne pas
			// se tromper de dossier -- et ca rend visible la regle « un projet est
			// un dossier », au lieu de la faire deviner.
			char full[512];
			snprintf(full, sizeof(full), "%s/%s/%s.nk3dm", st.newProjDir, st.newProjName,
					 st.newProjName);
			p.TextV(lx, y, S(20.f), "Sera cree :", NkRole::TextMuted);
			p.Clip({lx, y + S(20.f), bw - S(40.f), S(20.f)});
			p.TextV(lx, y + S(20.f), S(20.f), full, NkRole::Text);
			p.Unclip();

			if (st.projError[0]) {
				p.Clip({lx, box.y + bh - S(78.f), bw - S(40.f), S(20.f)});
				p.TextV(lx, box.y + bh - S(78.f), S(20.f), st.projError, NkRole::AccentSel);
				p.Unclip();
			}

			const float32 by = box.y + bh - S(46.f);
			const bool canCreate = st.newProjName[0] != 0 && st.newProjDir[0] != 0;
			if (WelcomeButton(p, hit, "np.cancel", {box.x + bw - S(240.f), by, S(100.f), S(30.f)},
							  "Annuler", NkIcon::None, false)) {
				st.newProjOpen = false;
				st.projError[0] = 0;
				ws.EndEdit();
			}
			if (WelcomeButton(p, hit, "np.create", {box.x + bw - S(130.f), by, S(110.f), S(30.f)},
							  "Creer", NkIcon::None, true, canCreate)) {
				ws.EndEdit();
				st.projPending = 6; // creation reelle : apres la frame, comme le reste
			}
		}

		// ── L'ECRAN ─────────────────────────────────────────────────────────────
		inline void PaintWelcome(NkModelerPainter &p, float32 W, float32 H, NkModelerState &st,
								 NkHitRegistry &hit, NkWidgetState &ws,
								 const nkgui::NkGuiInput &in, NkRecentList &rec,
								 const NkSplashArt &art) {
			if (!st.welcome)
				return;
			// FOND OPAQUE : l'application est peinte dessous (on ne demonte pas sa
			// boucle pour un ecran de demarrage), elle ne doit pas transparaitre.
			p.Fill({0.f, 0.f, W, H}, NkRole::WindowBg);
			hit.Add("wel.bg", {0.f, 0.f, W, H});

			// ── BARRE DE FENETRE ────────────────────────────────────────────────
			// La fenetre du modeleur n'a pas de chrome systeme : sans cette bande,
			// l'ecran d'accueil ne pourrait etre ni deplace ni ferme.
			const float32 barH = S(32.f);
			{
				const NkRect bar{0.f, 0.f, W, barH};
				p.Fill(bar, NkRole::PanelHeader);
				hit.Add("win.drag", bar);
				const float32 bw = S(30.f), bh = S(22.f), by = (barH - bh) * 0.5f;
				const NkIcon kWin[3] = {NkIcon::WinMin,
										st.maximized ? NkIcon::WinRestore : NkIcon::WinMax,
										NkIcon::WinClose};
				static const char *const kKeys[3] = {"wel.min", "wel.max", "wel.close"};
				for (int32 i = 0; i < 3; ++i) {
					const float32 bx = W - S(8.f) - (float32)(3 - i) * (bw + S(6.f));
					const NkRect br{bx, by, bw, bh};
					const bool over = hit.Add(kKeys[i], br);
					if (i == 2)
						p.Fill(br, over ? NkColor{240, 100, 85, 255} : NkColor{231, 76, 60, 255},
							   3.f);
					else
						p.Outline(br, NkRole::Border, over ? NkRole::PanelBg : NkRole::PanelHeader,
								  3.f);
					p.IconV(bx + (bw - S(13.f)) * 0.5f, by, bh, kWin[i],
							i == 2 ? NkRole::TextOnAccent : NkRole::Text, 13.f);
				}
				if (hit.Clicked("win.drag")) {
					st.wantDragMove = true;
					st.dragFracX = W > 1.f ? (hit.Mouse().x / W) : 0.5f;
				}
				if (hit.Clicked("wel.min"))
					st.wantMinimize = true;
				if (hit.Clicked("wel.max"))
					st.wantMaxRestore = true;
				// AUCUN PROJET N'EST OUVERT : il n'y a rien a perdre, donc rien a
				// demander. La confirmation de fermeture n'a de sens qu'avec un
				// document dessous.
				if (hit.Clicked("wel.close"))
					st.running = false;
			}

			// ── COLONNES ────────────────────────────────────────────────────────
			// La colonne des RECENTS prend la plus grande part : c'est elle qu'on
			// vient chercher. La colonne de gauche est bornee -- au-dela, ses
			// boutons s'etirent sans rien gagner.
			float32 leftW = W * 0.36f;
			if (leftW > S(400.f))
				leftW = S(400.f);
			if (leftW < S(260.f))
				leftW = S(260.f);
			const float32 pad = S(36.f);
			const float32 top = barH + S(28.f);

			// ══ GAUCHE : marque, actions, liens ═════════════════════════════════
			{
				float32 x = pad, y = top;
				const float32 mark = S(56.f);
				PaintBrandMark(p, x, y, mark);
				// Le mot en TROIS POIDS : « NK » porte l'accent d'interface, « 3D »
				// la sarcelle, « Modeler » le texte courant. Une seule taille de
				// police existe dans l'application -- c'est le CONTRASTE de couleur
				// qui fait la hierarchie, pas le corps.
				{
					const float32 tx = x + mark + S(16.f);
					const float32 ty = y + S(10.f);
					const float32 wNK = p.TextW("NK"), w3D = p.TextW("3D");
					p.Text(tx, ty, "NK", NkRole::AccentUi);
					p.Text(tx + wNK, ty, "3D", NkRole::NodeDataHeader);
					p.Text(tx + wNK + w3D, ty, "Modeler", NkRole::Text);
					p.Text(tx, ty + p.LineH() + S(4.f), "Modelisation 3D — Nkentseu",
						   NkRole::TextMuted);
				}
				y += mark + S(28.f);

				p.TextV(x, y, S(22.f), "Demarrer", NkRole::TextMuted);
				p.HLine(x, y + S(24.f), leftW - pad);
				y += S(34.f);

				const float32 bw = leftW - pad;
				if (WelcomeButton(p, hit, "wel.new", {x, y, bw, S(34.f)}, "Nouveau projet",
								  NkIcon::Add, true)) {
					st.projPending = 1;
				}
				y += S(42.f);
				if (WelcomeButton(p, hit, "wel.open", {x, y, bw, S(34.f)}, "Ouvrir un projet",
								  NkIcon::FolderOpen, false)) {
					st.projPending = 2;
				}
				y += S(56.f);

				p.TextV(x, y, S(22.f), "Liens", NkRole::TextMuted);
				p.HLine(x, y + S(24.f), bw);
				y += S(32.f);

				// COMMUNAUTE EN TUILES (maquette de Rihen) : quatre destinations
				// nommees valent mieux qu'une ligne « Communaute » qui ne dit pas
				// ou elle mene. Deux par rangee.
				struct L {
						const char *key;
						const char *label;
						const char *url;
						NkIcon icon;
				};
				static const L kLinks[4] = {
					{"wel.l0", "Site officiel", kUrlSite, NkIcon::Globe},
					{"wel.l1", "Tutoriels", kUrlTutos, NkIcon::Journal},
					{"wel.l2", "Communaute", kUrlCommu, NkIcon::Layers},
					{"wel.l3", "Documentation", kUrlDocs, NkIcon::Edit}};
				{
					const float32 tgap = S(6.f);
					const float32 tw2 = (bw - tgap) * 0.5f, th2 = S(44.f);
					for (int32 i = 0; i < 4; ++i) {
						const NkRect lr{x + (float32)(i % 2) * (tw2 + tgap),
										y + (float32)(i / 2) * (th2 + tgap), tw2, th2};
						// UNE TUILE SANS ADRESSE N'EST PAS CLIQUABLE : sa zone
						// n'est meme pas declaree. Elle reste VISIBLE, grisee,
						// pour dire ce qui viendra -- mais ne fait jamais semblant.
						const bool live = kLinks[i].url && *kLinks[i].url;
						const bool over = live && hit.Add(kLinks[i].key, lr);
						p.Outline(lr, over ? NkRole::AccentUi : NkRole::Border,
								  over ? NkRole::PanelHeader : NkRole::PanelBg, 4.f);
						const NkRole role = !live ? NkRole::TextMuted
												  : (over ? NkRole::AccentUi : NkRole::Text);
						p.IconV(lr.x + S(10.f), lr.y, lr.h, kLinks[i].icon, role, 15.f);
						p.TextV(lr.x + S(32.f), lr.y, live ? lr.h : lr.h - S(12.f),
								kLinks[i].label, role);
						if (!live)
							p.TextV(lr.x + S(32.f), lr.y + S(16.f), lr.h - S(12.f), "a venir",
									NkRole::TextMuted);
						if (over) {
							hit.WantCursor(NkCursorWant::Hand);
							if (hit.Clicked(kLinks[i].key))
								NkLauncher::OpenURL(kLinks[i].url);
						}
					}
					y += (S(44.f) + S(6.f)) * 2.f;
				}
			}

			// ══ DROITE : image de version, puis les projets recents ═════════════
			{
				const float32 x = leftW + S(12.f);
				const float32 w = W - x - pad;
				float32 y = top;

				// ── L'IMAGE DE LA VERSION (façon Blender) ───────────────────
				// Une bande COUCHEE, pas une colonne : elle se montre sans voler
				// la place aux projets, qui restent ce qu'on vient chercher. Le
				// credit et la version se posent DESSUS, en bas -- comme Blender
				// le fait, et parce qu'une legende sous l'image aurait mange une
				// ligne de plus.
				if (art.valid) {
					// Hauteur bornee : au-dela d'un quart de l'ecran, l'image
					// commence a repousser les projets sous la ligne de flottaison.
					float32 bh = w * (float32)art.h / (float32)(art.w ? art.w : 1u);
					const float32 bhMax = H * 0.26f;
					if (bh > bhMax)
						bh = bhMax;
					if (bh > S(90.f)) { // en dessous, l'image n'apprend plus rien
						const NkRect br{x, y, w, bh};
						p.Image(kSplashTexId, br);
						// Bandeau sombre en pied d'image : le texte doit rester
						// lisible quelle que soit l'oeuvre -- une legende posee a
						// nu disparait sur un fond clair.
						const float32 lh = S(26.f);
						p.Fill({br.x, br.y + br.h - lh, br.w, lh}, NkColor{0, 0, 0, 150});
						char vbuf[64];
						snprintf(vbuf, sizeof(vbuf), "NK3DModeler %s", kAppVersion);
						p.TextV(br.x + S(10.f), br.y + br.h - lh, lh, vbuf,
								NkRole::TextOnAccent);
						if (art.title[0] || art.author[0]) {
							char cbuf[200];
							if (art.title[0] && art.author[0])
								snprintf(cbuf, sizeof(cbuf), "%s — %s", art.title, art.author);
							else
								snprintf(cbuf, sizeof(cbuf), "%s",
										 art.title[0] ? art.title : art.author);
							const float32 cw = p.TextW(cbuf);
							p.Clip({br.x + S(10.f), br.y + br.h - lh, br.w - S(20.f), lh});
							p.TextV(br.x + br.w - S(10.f) - cw, br.y + br.h - lh, lh, cbuf,
									NkRole::TextOnAccent);
							p.Unclip();
						}
						y += bh + S(22.f);
					}
				}

				p.TextV(x, y, S(24.f), "Projets recents");
				p.HLine(x, y + S(26.f), w);
				y += S(38.f);

				// GRILLE DE GRANDES VIGNETTES plutot qu'une liste (maquette de
				// Rihen, 5 aout) : un projet se reconnait a son IMAGE bien avant
				// son nom. Une liste montre huit lignes de texte ; une grille
				// montre six images qu'on identifie d'un regard.
				// Les colonnes s'adaptent a la largeur : la carte garde une
				// taille lisible au lieu de s'etirer sur un ecran large.
				const NkRect area{x, y, w, H - y - S(64.f)};
				const float32 gap = S(12.f);
				const float32 cardWmin = S(210.f);
				int32 cols = (int32)((area.w + gap) / (cardWmin + gap));
				if (cols < 1)
					cols = 1;
				if (cols > 5)
					cols = 5;
				const float32 cardW = (area.w - gap * (float32)(cols - 1)) / (float32)cols;
				// 16:9 pour l'image + deux lignes de texte dessous.
				const float32 thumbH = cardW * 9.f / 16.f;
				const float32 cardH = thumbH + S(48.f);
				const int32 n = (int32)rec.items.Size();
				if (n == 0) {
					// ETAT VIDE HONNETE : aucune carte de demonstration. La liste
					// dit qu'elle est vide et ce qui la remplira.
					p.TextV(x, y, S(24.f), "Aucun projet ouvert pour l'instant.",
							NkRole::TextMuted);
					p.TextV(x, y + S(26.f), S(24.f),
							"Creez-en un, ou ouvrez un fichier .nk3dm existant.",
							NkRole::TextMuted);
				} else {
					const int32 rows = (n + cols - 1) / cols;
					const float32 contentH = (float32)rows * (cardH + gap);
					hit.WheelIn(area, st.welcomeScroll, contentH, area.h);
					if (st.welcomeScroll < 0.f)
						st.welcomeScroll = 0.f;
					const float32 maxOff = contentH > area.h ? (contentH - area.h) : 0.f;
					if (st.welcomeScroll > maxOff)
						st.welcomeScroll = maxOff;

					p.Clip(area);
					char key[40];
					for (int32 i = 0; i < n; ++i) {
						const int32 cx = i % cols, cy = i / cols;
						const NkRect cr{area.x + (float32)cx * (cardW + gap),
										area.y + (float32)cy * (cardH + gap) - st.welcomeScroll,
										cardW, cardH};
						if (cr.y + cr.h < area.y || cr.y > area.y + area.h)
							continue;
						NkRecentEntry &e = rec.items[(usize)i];
						snprintf(key, sizeof(key), "wel.rec.%d", i);
						const bool over = hit.Add(key, cr);
						p.Outline(cr, over ? NkRole::AccentUi : NkRole::Border,
								  over ? NkRole::PanelHeader : NkRole::PanelBg, 5.f);
						if (over)
							hit.WantCursor(NkCursorWant::Hand);

						// VIGNETTE : la derniere image du projet, PLEINE LARGEUR de
						// la carte. Repli sur un aplat neutre -- un projet sans
						// rendu n'a rien a montrer, et une image d'emprunt ferait
						// croire au contenu d'un autre.
						const NkRect tr{cr.x + S(1.f), cr.y + S(1.f), cr.w - S(2.f), thumbH};
						if (e.tex)
							p.Image(e.tex, tr);
						else {
							p.Fill(tr, NkRole::InputBg, 4.f);
							p.IconV(tr.x + (tr.w - S(20.f)) * 0.5f, tr.y, tr.h,
									NkIcon::ImageRef, NkRole::TextMuted, 20.f);
						}

						// Nom et date SOUS l'image, sur deux lignes serrees : le
						// chemin complet n'a pas sa place ici, il noierait le nom.
						const float32 txx = cr.x + S(10.f);
						p.Clip({txx, cr.y + thumbH, cr.w - S(66.f), S(46.f)});
						p.TextV(txx, cr.y + thumbH + S(4.f), S(22.f), e.name.CStr());
						p.TextV(txx, cr.y + thumbH + S(24.f), S(20.f),
								e.date.Empty() ? "date inconnue" : e.date.CStr(),
								NkRole::TextMuted);
						p.Unclip();

						// EPINGLE et RETRAIT, comme NKCode. L'epingle garde le projet
						// en tete quoi qu'il arrive ; le retrait ne touche QUE la
						// liste, jamais le disque -- il faut que ce soit sans danger.
						// Poses SUR l'image, en bas a droite : ils ne volent pas de
						// place au nom et restent a portee.
						snprintf(key, sizeof(key), "wel.pin.%d", i);
						const NkRect pr{cr.x + cr.w - S(56.f), cr.y + thumbH + S(12.f), S(24.f),
										S(24.f)};
						const bool ovP = hit.Add(key, pr);
						p.IconV(pr.x, pr.y, pr.h, NkIcon::Pin,
								e.pinned ? NkRole::AccentSel
										 : (ovP ? NkRole::Text : NkRole::TextMuted),
								14.f);
						if (hit.Clicked(key))
							rec.TogglePin((usize)i);

						snprintf(key, sizeof(key), "wel.del.%d", i);
						const NkRect dr{cr.x + cr.w - S(30.f), cr.y + thumbH + S(12.f), S(24.f),
										S(24.f)};
						const bool ovD = hit.Add(key, dr);
						p.IconV(dr.x, dr.y, dr.h, NkIcon::Trash,
								ovD ? NkRole::Text : NkRole::TextMuted, 14.f);
						if (hit.Clicked(key)) {
							rec.Remove((usize)i);
							break; // la liste a change : on ne continue pas dessus
						}

						snprintf(key, sizeof(key), "wel.rec.%d", i);
						if (hit.Clicked(key)) {
							st.projRecent = i;
							st.projPending = 7;
						}
					}
					p.Unclip();
					p.VScroll(area, contentH, st.welcomeScroll);
				}
			}

			// ── PIED DE PAGE : version, compte, astuce ──────────────────────────
			// Repris de la maquette de Rihen. L'ASTUCE tourne a chaque ouverture :
			// elle enseigne les gestes qu'on ne decouvre pas seul, et c'est le
			// seul contenu « editorial » qui n'ait besoin d'aucun serveur.
			{
				const float32 fy = H - S(46.f);
				p.HLine(pad, fy - S(6.f), W - pad * 2.f);
				// LA SIGNATURE CEDE LA PLACE A L'ERREUR. Les deux occupaient la
				// MEME ligne : superposees, aucune des deux n'etait lisible. Entre
				// un nom d'application decoratif et un message qui dit pourquoi
				// l'action de l'utilisateur a echoue, c'est le message qui compte.
				const bool showErr = !st.newProjOpen && st.projError[0] != 0;
				if (!showErr)
					p.TextV(pad, fy, S(22.f), "NK3DModeler — Nkentseu", NkRole::TextMuted);
				// Rotation par le NOMBRE DE PROJETS connus : stable pendant la
				// session (l'astuce ne saute pas d'une image a l'autre) et
				// differente d'une ouverture a l'autre.
				static const char *const kTips[6] = {
					"Astuce : Tab bascule entre le mode Objet et le mode Edition.",
					"Astuce : Ctrl pendant un deplacement inverse l'aimantation.",
					"Astuce : la molette sur un champ numerique l'ajuste finement.",
					"Astuce : G, R et S transforment tout de suite, sans changer d'outil.",
					"Astuce : Maj+D duplique ; Ctrl+C / Ctrl+V copie entre les scenes.",
					"Astuce : le point-virgule ouvre les cibles d'aimantation."};
				const int32 ti = (int32)(rec.items.Size() % 6u);
				const char *tip = kTips[ti];
				p.TextV(W - pad - p.TextW(tip), fy, S(22.f), tip, NkRole::TextMuted);
			}

			// ── CE QUI EST ENREGISTRE, ET CE QUI NE L'EST PAS ───────────────────
			// La scene EST desormais sauvegardee, mais pas entierement. Annoncer
			// ce qui manque vaut mieux qu'un silence : quelqu'un qui sculpte une
			// heure doit savoir AVANT de fermer que son maillage edite ne sera pas
			// repris. La liste de reference vit dans NkModelerScene.h.
			p.TextV(pad, H - S(24.f), S(20.f),
					"Enregistre : objets, materiaux, lumieres, cameras, scenes. Pas encore : "
					"geometrie editee sommet par sommet, modificateurs.",
					NkRole::TextMuted);

			PaintNewProjectDialog(p, W, H, st, hit, ws, in);

			// L'erreur de la derniere action projet, quand aucune boite n'est la
			// pour la porter. Elle reste affichee jusqu'a la prochaine action :
			// une erreur qui disparait toute seule n'a servi a personne.
			if (!st.newProjOpen && st.projError[0]) {
				// Meme ligne que la signature, qui s'efface pour elle (cf. plus
				// haut). La decoupe borne le texte a la largeur utile : un message
				// long deborderait sur l'astuce, a droite.
				p.Clip({pad, H - S(46.f), W * 0.62f, S(22.f)});
				p.TextV(pad, H - S(46.f), S(22.f), st.projError, NkRole::AccentSel);
				p.Unclip();
			}
		}

		// ── EXECUTION DES ACTIONS PROJET, APRES LA FRAME ────────────────────────
		// Appelee par la boucle principale une fois la peinture terminee. C'est le
		// SEUL endroit ou l'on ouvre un selecteur de fichiers : ces dialogues
		// entrent dans une boucle modale de l'OS, et les appeler depuis la peinture
		// reentrerait dans la frame en cours (meme piege que BeginDragMove).
		//
		// Les selecteurs viennent de NKWindow (`NkDialogs`) et non d'un Win32 ecrit
		// ici : le depot en a deja un, portable (Win32, zenity, osascript, stubs).
		inline void NkProjectHandlePending(NkModelerState &st, NkProjectState &proj,
										   NkRecentList &rec) {
			// ── SUPPRESSIONS COTE DISQUE ────────────────────────────────────
			// Supprimer une carte doit retirer SON fichier (demande de Rihen).
			// C'est fait ICI, apres la frame : c'est le seul endroit ou la racine
			// du projet est connue, et on ne touche pas au disque en peignant.
			//
			// CORBEILLE D'ABORD, effacement seulement si elle n'est pas disponible.
			// Un fichier detruit sans retour serait la pire des surprises -- c'est
			// la meme regle que « fermer un onglet ne supprime rien ».
			if (st.delPendCount > 0) {
				if (proj.open && !proj.root.Empty()) {
					for (int32 i = 0; i < st.delPendCount; ++i) {
						const NkString rel(st.delPendFile[i]);
						if (rel.Empty())
							continue;
						const bool isDir = rel[rel.Size() - 1u] == '/';
						const NkString abs = NkScToAbs(proj.root, rel.CStr());
						if (isDir) {
							// Un dossier ne part que s'il est VIDE : un fichier
							// qu'on n'a pas mis la n'a pas a disparaitre avec lui.
							if (NkDirectory::Exists(abs.CStr()) &&
								!NkDirectory::MoveToTrash(abs.CStr()))
								(void)NkDirectory::Delete(abs.CStr(), false);
						} else if (NkFile::Exists(abs.CStr()) &&
								   !NkFile::MoveToTrash(abs.CStr())) {
							(void)NkFile::Delete(abs.CStr());
						}
					}
				}
				// La file se vide dans TOUS les cas : la garder ferait retenter la
				// suppression a chaque frame, y compris sans projet ouvert.
				st.delPendCount = 0;
			}

			// ── BASCULE D'ONGLET : reglages Rendu PAR SCENE (Rihen, 10 aout).
			// Requete posee par NkActivateTab, consommee ICI en differe : au
			// moment ou l'on capture, l'etat vivant est encore celui du
			// document quitte (il est global a la vue, la bascule de scene ne
			// l'a pas touche) ; on applique ensuite l'instantane de l'active.
			// Un instantane VIDE n'applique rien : le document herite des
			// reglages courants, puis les possede a la premiere bascule.
			if (st.renduSwitchFrom >= 0 || st.renduSwitchTo >= 0) {
				if (st.renduSwitchFrom >= 0 && st.renduSwitchFrom < 32) {
					NkArchive snap;
					NkAsRenduCapture(snap, proj.root);
					st.docRendu[st.renduSwitchFrom] = snap;
				}
				if (st.renduSwitchTo >= 0 && st.renduSwitchTo < 32 &&
					st.renduSwitchTo != st.renduSwitchFrom)
					NkAsRenduRestore(st.docRendu[st.renduSwitchTo], proj.root, st);
				st.renduSwitchFrom = -1;
				st.renduSwitchTo = -1;
			}

			const int32 action = st.projPending;
			if (action == 0)
				return;
			st.projPending = 0;

			auto put = [](char *dst, uint32 cap, const char *src) {
				uint32 i = 0;
				for (; src && src[i] && i + 1u < cap; ++i)
					dst[i] = src[i];
				dst[i] = 0;
			};
			// Une erreur se dit LA OU L'ON REGARDE : dans la boite « Nouveau » si
			// elle est ouverte, sinon dans une boite systeme -- jamais seulement
			// dans le journal, que personne ne lit au moment ou ca rate.
			auto fail = [&](const NkString &e) {
				put(st.projError, (uint32)sizeof(st.projError), e.CStr());
				if (!st.newProjOpen)
					NkDialogs::OpenMessageBox(e.Empty() ? NkString("action impossible") : e,
											  "NK3DModeler", 2);
			};
			auto opened = [&]() {
				rec.Touch(proj);
				st.welcome = false;
				st.newProjOpen = false;
				st.projError[0] = 0;
				// Un projet qu'on vient d'ouvrir ou de creer n'a rien de modifie.
				NkClearDirty(st);
			};

			// ── OUVRIR UN PROJET : DEUX DISPOSITIONS, UN SEUL POINT DE PASSAGE ──
			// Disposition 3 : un fichier par asset, le .nk3dm ne porte que l'arbre.
			// Dispositions 1 et 2 : tout etait dans le .nk3dm. On les RELIT encore
			// -- le projet de quelqu'un ne devient pas illisible parce que la
			// disposition a change -- et le prochain enregistrement l'ecrit en
			// fichiers separes. C'est une migration, pas une rupture.
			auto restore = [&](const NkArchive &sc, NkString &e) -> bool {
				bool ok;
				if (NkScInt(sc, "disposition", 0) >= 3) {
					ok = NkProjectTreeRestore(sc, proj.root, st, &e);
				} else {
					ok = NkSceneRestore(sc, proj.root, st, &e);
					if (ok)
						NkBrowserSyncScenes(st); // l'ancien format ne portait pas les cartes
				}
				// LE DISQUE FAIT FOI SUR CE QUI EXISTE : l'arbre relu dit l'ordre,
				// le balayage dit ce qui est reellement la. Un fichier ajoute ou
				// efface pendant que l'application etait fermee est donc pris en
				// compte a l'ouverture, sans attendre le surveillant.
				if (ok)
					(void)NkProjectRescan(proj.root, st);
				return ok;
			};

			NkString err;
			switch (action) {
				case 1: // ouvrir la boite « Nouveau projet »
					if (!st.newProjDir[0]) {
						const NkString home = NkDirectory::GetHomeDirectory().ToString();
						put(st.newProjDir, (uint32)sizeof(st.newProjDir),
							(home + "/NK3DModeler").CStr());
					}
					if (!st.newProjName[0])
						put(st.newProjName, (uint32)sizeof(st.newProjName), "MonProjet");
					st.projError[0] = 0;
					st.newProjOpen = true;
					break;

				case 5: { // « Parcourir » : le dossier PARENT du futur projet
					const NkDialogResult r =
						NkDialogs::OpenFolderDialog("Emplacement du nouveau projet");
					if (r.confirmed && !r.path.Empty())
						put(st.newProjDir, (uint32)sizeof(st.newProjDir), r.path.CStr());
					break;
				}

				case 6: // creation reelle
					if (NkProjectCreate(st.newProjDir, st.newProjName, proj, &err))
						opened();
					else
						fail(err);
					break;

				case 2: { // Ouvrir...
					// FILTRE « TOUS LES FICHIERS », a regret et pour une raison
					// precise : `Win32PrepareFilter` (NKWindow/NkDialogs.cpp)
					// construit sa chaine avec `result += ")\0"`, or un littoral
					// C s'arrete au premier zero -- les separateurs nuls que
					// OPENFILENAME attend ne sont jamais ecrits, et le motif
					// arrive VIDE. Passer un filtre reel ferait donc un
					// selecteur qui ne montre rien. La nature est dite dans le
					// titre, et NkProjectLoad refuse proprement un fichier qui
					// n'est pas un projet. (Bug NKWindow a corriger a part.)
					const NkDialogResult r = NkDialogs::OpenFileDialog(
						"*.*", "Ouvrir un projet NK3DModeler (.nk3dm)");
					if (!r.confirmed || r.path.Empty())
						break;
					NkArchive sc;
					if (NkProjectLoad(r.path.CStr(), proj, &err, &sc)) {
						// La scene est restituee APRES le chargement : `proj.root`
						// doit deja etre a jour, c'est elle qui resout les chemins
						// relatifs. Une scene absente n'est PAS une erreur (projet
						// vide, ou ecrit par une version qui ne la sauvait pas).
						if (!restore(sc, err) && !err.Empty())
							fail(err);
						else {
							opened();
							// UN CHARGEMENT PARTIEL SE DIT QUAND MEME. Il reussit --
							// donc pas de boite d'erreur -- mais taire « 2 textures
							// introuvables » ou « 1 scene orpheline recuperee »
							// laisserait l'utilisateur decouvrir l'ecart tout seul,
							// bien plus tard.
							if (!err.Empty())
								put(st.projError, (uint32)sizeof(st.projError), err.CStr());
						}
					} else
						fail(err);
					break;
				}

				case 7: { // une carte de l'ecran d'accueil
					if (st.projRecent >= 0 && (usize)st.projRecent < rec.items.Size()) {
						const NkString path = rec.items[(usize)st.projRecent].path;
						NkArchive sc;
						if (NkProjectLoad(path.CStr(), proj, &err, &sc)) {
							if (!restore(sc, err) && !err.Empty())
								fail(err);
							else {
								opened();
								if (!err.Empty())
									put(st.projError, (uint32)sizeof(st.projError), err.CStr());
							}
						} else
							// On NE RETIRE PAS l'entree : un disque externe
							// debranche n'est pas un projet supprime, et la
							// retirer d'office ferait perdre l'epingle.
							fail(err);
					}
					st.projRecent = -1;
					break;
				}

				case 3:	  // Enregistrer — LE FICHIER ACTIF
				case 8: { // Enregistrer tout — TOUT LE PROJET
					if (!proj.open || proj.file.Empty()) {
						// Jamais enregistre : « Enregistrer » DOIT se comporter comme
						// « Enregistrer sous... », sinon il echoue sans rien dire.
						st.projPending = 4;
						return;
					}
					// CTRL+S ENREGISTRE CE QU'ON REGARDE, pas tout le projet (Rihen).
					// Depuis qu'un asset est un fichier, « Enregistrer » doit se
					// comporter comme partout ailleurs ; « Enregistrer tout » existe
					// a cote pour le reste.
					const int32 card = (action == 3) ? NkActiveCard(st) : -1;
					{
						// LES ASSETS D'ABORD, chacun dans SON fichier ; le .nk3dm
						// ensuite, qui n'en porte que les liens. L'ordre compte :
						// c'est l'ecriture des assets qui fixe le chemin de chaque
						// carte, et c'est ce chemin que l'arbre enregistre.
						//
						// Tout est ecrit RELATIVEMENT A LA RACINE OU CA S'ECRIT : un
						// projet dont les chemins visent un autre dossier s'ouvre vide.
						if (!NkProjectWriteAssets(proj.root, st, &err, card)) {
							fail(err);
							st.quitAfterSave = false;
							break;
						}
						// L'ARBRE EST TOUJOURS REECRIT, meme pour un seul fichier :
						// il porte l'ordre, les dossiers et le chemin des cartes, et
						// un fichier ecrit dont l'arbre ignorerait le chemin serait
						// introuvable a la reouverture.
						NkArchive sc;
						NkProjectTreeCapture(sc, st);
						if (!NkProjectSave(proj, &err, &sc)) {
							fail(err);
							st.quitAfterSave = false;
							break;
						}
					}
					rec.Touch(proj);
					if (action == 8)
						NkClearDirty(st);
					else
						NkClearDirtyDoc(st, st.TabDoc(st.activeTab));
					// QUITTER enregistre TOUT : partir en n'ayant ecrit qu'un fichier
					// laisserait le reste du travail derriere soi.
					if (st.quitAfterSave) {
						if (action == 3) {
							st.quitAfterSave = true;
							st.projPending = 8;
							return; // on repasse ici, cette fois pour tout
						}
						st.running = false;
					}
					st.quitAfterSave = false;
					break;
				}

				case 4: { // Enregistrer sous...
					const NkDialogResult r =
						NkDialogs::SaveFileDialog("nk3dm", "Enregistrer le projet sous...");
					if (r.confirmed && !r.path.Empty()) {
						// Racine de DESTINATION, pas l'actuelle : « Enregistrer
						// sous... » deplace le projet, et les fichiers d'assets
						// doivent atterrir dans le NOUVEAU dossier. NkProjectRootFor
						// applique la meme normalisation que l'ecriture elle-meme.
						const NkString dest = NkProjectRootFor(r.path.CStr());
						if (!NkProjectWriteAssets(dest, st, &err)) {
							fail(err);
							st.quitAfterSave = false;
							break;
						}
						NkArchive sc;
						NkProjectTreeCapture(sc, st);
						if (NkProjectSaveAs(proj, r.path.CStr(), &err, &sc)) {
							opened();
							if (st.quitAfterSave)
								st.running = false;
						} else
							fail(err);
					}
					st.quitAfterSave = false;
					break;
				}

				default:
					break;
			}
		}

	} // namespace nk3d
} // namespace nkentseu
