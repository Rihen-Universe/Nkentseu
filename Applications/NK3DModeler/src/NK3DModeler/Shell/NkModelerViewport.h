#pragma once
// -----------------------------------------------------------------------------
// @File    NkModelerViewport.h
// @Brief   LA VUE 3D et ses surcouches : gizmo de navigation, boutons flottants,
//          popups de vue et de matcap, et la peinture de la vue elle-meme.
//
//          Extrait de NkModelerScreens.h pendant la refonte d'interface --
//          « subdiviser les gros fichiers » (Rihen, 13 aout 2026).
// @Author  Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
// -----------------------------------------------------------------------------
#include "NK3DModeler/Shell/NkModelerUI.h"
#include "NK3DModeler/Shell/NkModelerInput.h"
#include "NK3DModeler/Shell/NkModelerWidgets.h"
#include "NK3DModeler/Shell/NkModelerTables.h"
#include "NK3DModeler/Shell/NkModelerCommon.h"
#include "NK3DModeler/Viewport/NkViewport3D.h"
#include "NK3DModeler/Viewport/NkDemo3DHost.h"
#include "NK3DModeler/Viewport/NkOutCompose.h"
#include "NK3DModeler/Shell/NkModelerMeshMenu.h" // commandes de maillage : une decl., N chemins

namespace nkentseu {
	namespace nk3d {

		// ── SURCOUCHES DE LA VUE 3D ─────────────────────────────────────────
		// Gizmo de navigation, boutons flottants, popups de vue et matcap, puis
		// la vue elle-meme. Tout ce qui se dessine DANS le cadre de la scene.
		inline void PaintNavGizmo(NkModelerPainter &p, NkHitRegistry &hit, NkModelerState &st,
								  float32 cx, float32 cy, float32 radius) {
			// IL TOURNE AVEC LA VUE, comme celui de Blender. La version precedente
			// avait une projection isometrique FIGEE : elle affichait toujours la
			// meme orientation, donc elle ne disait rien de ce qu'on regardait --
			// c'etait un decor. On projette maintenant les axes du MONDE avec le
			// meme repere que la scene : X, Y et Z se placent exactement la ou ils
			// se trouvent dans l'image.
			// PORTAGE : le repere vient de la CAMERA DE LA DEMO (l'ancienne facade
			// est dormante et repondait un repere fige -- le gizmo restait statique,
			// bug signale par Rihen).
			float32 rgt[3], upv[3], fwd[3];
			demo::Demo3DHostCameraAxes(rgt, upv, fwd);

			struct Half {
					float32 wx, wy, wz;	 ///< direction MONDE du demi-axe
					NkRole role;
					const char *label;
					bool positive;
			};
			static const Half kHalf[6] = {
				{1.f, 0.f, 0.f, NkRole::AxisX, "X", true},
				{-1.f, 0.f, 0.f, NkRole::AxisX, "", false},
				{0.f, 1.f, 0.f, NkRole::AxisY, "Y", true},
				{0.f, -1.f, 0.f, NkRole::AxisY, "", false},
				{0.f, 0.f, 1.f, NkRole::AxisZ, "Z", true},
				{0.f, 0.f, -1.f, NkRole::AxisZ, "", false},
			};
			// Chaque demi-axe amene sa vue : cliquer +X regarde DEPUIS +X.
			// Correspondance avec Viewport3DAxisView : 0 face (-Z monde), 1 droite
			// (+X), 2 dessus (+Y).
			struct View {
					int32 which;
					bool opposite;
			};
			static const View kViews[6] = {
				{1, false}, {1, true},	// +X droite / -X gauche
				{2, false}, {2, true},	// +Y dessus / -Y dessous
				{0, true},	{0, false}, // +Z arriere / -Z face
			};

			const float32 ball = radius * 0.28f;
			struct Proj {
					float32 sx, sy, depth;
			};
			Proj pr[6];
			for (int32 i = 0; i < 6; ++i) {
				const float32 wx = kHalf[i].wx, wy = kHalf[i].wy, wz = kHalf[i].wz;
				// Projection sur le repere camera : droite -> +x ecran, haut -> -y
				// ecran (l'ecran descend), avant -> profondeur.
				pr[i].sx = wx * rgt[0] + wy * rgt[1] + wz * rgt[2];
				pr[i].sy = -(wx * upv[0] + wy * upv[1] + wz * upv[2]);
				// Profondeur : positif = vers l'observateur, donc l'OPPOSE de l'axe
				// Â« avant Â» de la camera, qui pointe vers la scene.
				pr[i].depth = -(wx * fwd[0] + wy * fwd[1] + wz * fwd[2]);
			}

			// Ordre de PROFONDEUR reel : ce qui est derriere se peint d'abord. Un
			// tri complet plutot qu'un tableau fige -- l'orientation change, donc
			// l'ordre aussi.
			int32 order[6] = {0, 1, 2, 3, 4, 5};
			for (int32 a = 0; a < 6; ++a)
				for (int32 b = a + 1; b < 6; ++b)
					if (pr[order[b]].depth < pr[order[a]].depth) {
						const int32 t = order[a];
						order[a] = order[b];
						order[b] = t;
					}

			// ── PASTILLE DE FOND, TRANSLUCIDE (Rihen) ───────────────────────────
			// Peinte AVANT tout le reste, donc sous les tiges et les boules. Elle
			// donne au gizmo une assise : sur une scene claire, les axes clairs se
			// perdaient dans le decor. Assez transparente pour qu'on voie la scene
			// au travers, un peu plus opaque au survol pour dire que le corps est
			// saisissable. Le noir tient sur un fond clair comme sur un fond
			// sombre, ce qu'une couleur de theme ne garantirait pas.
			const bool navHot = hit.IsHovered("nav.body");
			const NkColor navBg{0, 0, 0, (uint8)(navHot ? 82 : 48)};
			// Le TROU des demi-axes negatifs se pose SUR la pastille : un peu plus
			// dense qu'elle, il se lit comme un creux sans jamais devenir opaque.
			const NkColor navHole{0, 0, 0, (uint8)(navHot ? 150 : 120)};
			p.DiscColor(cx, cy, radius * 1.06f, navBg);
			// ROTATION LIBRE : tirer le CORPS du gizmo fait tourner la vue. Les
			// boules restent des raccourcis vers les six vues d'axe, mais elles ne
			// suffisent pas -- Blender permet aussi de le faire pivoter a la main,
			// et c'est souvent le geste le plus rapide pour se replacer.
			// La zone du corps est declaree AVANT les boules : elles la recouvrent,
			// donc viser une boule reste un clic sur la boule.
			{
				const NkRect body{cx - radius, cy - radius, radius * 2.f, radius * 2.f};
				const bool overBody = hit.Add("nav.body", body);
				if (overBody)
					hit.WantCursor(NkCursorWant::Hand);
				if (hit.Clicked("nav.body")) {
					st.navDragMode = 2;
					st.navDragLastX = hit.Mouse().x;
					st.navDragLastY = hit.Mouse().y;
				}
			}

			char key[24];
			for (int32 k = 0; k < 6; ++k) {
				const int32 i = order[k];
				const float32 ex = cx + pr[i].sx * (radius - ball);
				const float32 ey = cy + pr[i].sy * (radius - ball);
				snprintf(key, sizeof(key), "nav.axis.%d", i);
				const NkRect br{ex - ball, ey - ball, ball * 2.f, ball * 2.f};
				const bool over = hit.Add(key, br);
				// La tige ne part que des demi-axes POSITIFS : six tiges feraient une
				// etoile illisible.
				if (kHalf[i].positive)
					p.Line(cx, cy, ex, ey, kHalf[i].role, 2.f);
				if (kHalf[i].positive) {
					p.Disc(ex, ey, over ? ball + 2.f : ball, kHalf[i].role);
					const float32 lw = p.TextW(kHalf[i].label);
					p.Text(ex - lw * 0.5f, ey - p.LineH() * 0.5f, kHalf[i].label,
						   NkRole::TextOnAccent);
				} else {
					// Creuse : le trou se pose sur la PASTILLE, plus la couleur du
					// fond de vue -- opaque, elle perçait un rond plein dedans.
					p.RingColor(ex, ey, over ? ball + 2.f : ball, p.C(kHalf[i].role), navHole);
				}
				if (over)
					hit.WantCursor(NkCursorWant::Hand);
				if (hit.Clicked(key))
					demo::Demo3DHostAxisView(kViews[i].which, kViews[i].opposite);
			}
		}

		// â”€â”€ COLONNE DE BOUTONS DE VUE â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
		// Zoom, deplacement lateral, camera, bascule orthographique/perspective.
		// VERTICALE et sous le gizmo, comme chez Blender : ce sont des commandes de
		// NAVIGATION, pas d'edition, et les tenir a l'ecart des outils evite de
		// changer d'outil en croyant deplacer la vue.
		inline void PaintViewButtons(NkModelerPainter &p, NkHitRegistry &hit, NkModelerState &st,
									 float32 x, float32 y) {
			// ── CAPTURE : un BOUTON-MENU (regle de Rihen). Le clic devoile les
			// types, et cliquer une entree EXECUTE directement sa capture :
			//   Capture  -> la vue 3D seule, sans interface ;
			//   Tutoriel -> TOUTE la fenetre, interface comprise.
			// PNG numerotes dans captures/ du projet. La liste s'agrandira.
			// EN HAUT de la colonne : empile en bas, le bouton finissait SOUS
			// la boule de navigation, invisible.
			// ── CADENAS D'ORBITE (vue camera seulement, au-dessus de Capture,
			// regle de Rihen) : actif, la rotation ORBITE la camera autour d'un
			// centre (selection, sinon le point vise) au lieu de tourner sur
			// place -- comme Blender.
			if (demo::Demo3DHostCameraView() >= 0) {
				const float32 d0 = 26.f;
				const NkRect lb{x, y, d0, d0};
				const bool lockOn = demo::Demo3DHostCamOrbitLock();
				const bool overL = hit.Add("view.camlock", lb);
				if (lockOn)
					p.Fill(lb, NkRole::AccentUi, 4.f);
				else
					p.Outline(lb, overL ? NkRole::AccentUi : NkRole::Border,
							  NkRole::PanelHeader, 4.f);
				p.IconV(lb.x + (d0 - 14.f) * 0.5f, lb.y, d0,
						lockOn ? NkIcon::Lock : NkIcon::Unlock,
						lockOn ? NkRole::TextOnAccent : NkRole::Text, 14.f);
				if (overL)
					hit.WantCursor(NkCursorWant::Hand);
				if (hit.Clicked("view.camlock"))
					demo::Demo3DHostSetCamOrbitLock(!lockOn);
				y += d0 + 6.f;
			}
			{
				const float32 d0 = 26.f;
				const NkRect cb{x, y, d0, d0};
				const bool overC = hit.Add("view.shotgo", cb);
				if (st.captureMenuOpen)
					p.Fill(cb, NkRole::AccentUi, 4.f);
				else
					p.Outline(cb, overC ? NkRole::AccentUi : NkRole::Border, NkRole::PanelHeader, 4.f);
				p.IconV(cb.x + (d0 - 14.f) * 0.5f, cb.y, d0, NkIcon::ImageRef,
						st.captureMenuOpen ? NkRole::TextOnAccent : NkRole::Text, 14.f);
				if (overC)
					hit.WantCursor(NkCursorWant::Hand);
				if (hit.Clicked("view.shotgo"))
					st.captureMenuOpen = !st.captureMenuOpen;
				if (st.captureMenuOpen) {
					// CAPTURE = la VUE 3D seule (regle de Rihen). Photo prend une
					// image, Video ouvre une prise -- le stub Â« a venir Â» n'avait
					// plus lieu d'etre des lors que l'enregistrement existe. Le
					// TUTORIEL, lui, concerne toute l'application : il vit au
					// footer, et les deux peuvent tourner en meme temps.
					const bool vRec = demo::Demo3DHostRecActive();
					const char *kCapM[2] = {"Photo", vRec ? "Arreter la video" : "Video"};
					static const char *const kCapKeys[2] = {"view.shot.vue", "view.shot.vid"};
					for (int32 m = 0; m < 2; ++m) {
						const NkRect mr{x + d0 + 6.f, y + (float32)m * (d0 + 2.f), 128.f, d0};
						const bool ovM = hit.Add(kCapKeys[m], mr);
						p.Fill(mr, ovM ? NkRole::AccentUi : NkRole::PanelHeader, 4.f);
						p.TextV(mr.x + 8.f, mr.y, d0, kCapM[m],
								ovM ? NkRole::TextOnAccent : NkRole::Text);
						if (ovM)
							hit.WantCursor(NkCursorWant::Hand);
						if (hit.Clicked(kCapKeys[m])) {
							if (m == 0)
								st.capturePending = 1; // la vue 3D
							else if (vRec)
								demo::Demo3DHostRecStop(true);
							else
								demo::Demo3DHostRecStart();
							st.captureMenuOpen = false;
						}
					}
				}
				y += d0 + 6.f;
			}
			// QUATRE COMMANDES DE NAVIGATION, cablees sur la DEMO PORTEE :
			//   Loupe  -> glisser = zoom (le meme chemin que sa molette) ;
			//            double-clic = pose d'ouverture ;
			//   Main   -> glisser = deplacement lateral (Â« grab Â») ;
			//   Camera -> bascule editeur <-> vol (sa touche F) ;
			//   Ortho  -> perspective / orthographique (son pave 5).
			struct VB {
					NkIcon ic;
					const char *key;
					const char *tip;
					bool enabled;
			};
			const VB kBtns[4] = {
				{NkIcon::Zoom, "view.frame", "Zoom (glisser) / recadrer (double-clic)", true},
				{NkIcon::Pan, "view.center", "Deplacer la vue (glisser)", true},
				{NkIcon::Camera, "view.cam", "Camera de vol (WASD + clic droit)", true},
				{NkIcon::Ortho, "view.ortho", "Perspective / orthographique", true},
			};
			const float32 d = 26.f;
			// Le RECADRAGE reste accessible : double-clic sur la loupe. Le
			// glissement regle le zoom, le double-clic cadre tout -- deux besoins
			// differents sur le meme bouton, comme dans la plupart des editeurs.
			if (hit.DoubleClicked("view.frame"))
				demo::Demo3DHostResetView();
			for (int32 i = 0; i < 4; ++i) {
				const NkRect br{x, y + (float32)i * (d + 6.f), d, d};
				const bool over = kBtns[i].enabled && hit.Add(kBtns[i].key, br);
				const bool on = (i == 3 && demo::Demo3DHostIsOrtho()) ||
								(i == 2 && demo::Demo3DHostIsFlyCam());
				if (on)
					p.Fill(br, NkRole::AccentUi, 4.f);
				else
					p.Outline(br, over ? NkRole::AccentUi : NkRole::Border, NkRole::PanelHeader, 4.f);
				p.IconV(br.x + (d - 14.f) * 0.5f, br.y, d, kBtns[i].ic,
						!kBtns[i].enabled ? NkRole::TextMuted
										  : (on ? NkRole::TextOnAccent : NkRole::Text),
						14.f);
				if (over)
					hit.WantCursor(NkCursorWant::Hand);
				if (kBtns[i].enabled && hit.Clicked(kBtns[i].key)) {
					if (i == 0 || i == 1) {
						// LOUPE ET MAIN SONT DES GLISSEMENTS, pas des clics : on
						// attrape le bouton et on tire. C'est ce que fait Blender, et
						// c'est le seul acces a la navigation sur un portable sans
						// molette ni bouton du milieu. Un clic simple ne pourrait
						// exprimer ni la quantite ni la direction.
						st.navDragMode = i;
						st.navDragLastX = hit.Mouse().x;
						st.navDragLastY = hit.Mouse().y;
					} else if (i == 2) {
						demo::Demo3DHostToggleFlyCam();
					} else if (i == 3) {
						const bool o = !demo::Demo3DHostIsOrtho();
						demo::Demo3DHostSetOrtho(o);
						st.projection = o ? 1 : 0;
						st.lastProjection = st.projection;
					}
				}
			}
		}

		// â”€â”€ COULEURS DE FOND â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
		// Les cinq prereglages + la couleur PERSONNALISEE (index 5) reglee au
		// picker. Une seule table : le bouton-temoin, le menu et le picker
		// lisent la meme source.
		inline void NkBgColorOf(const NkModelerState &st, int32 choice, float32 *out) {
			static const float32 kBgCol[5][3] = {{0.05f, 0.05f, 0.07f},
												 {0.01f, 0.01f, 0.012f},
												 {0.24f, 0.24f, 0.25f},
												 {0.62f, 0.63f, 0.65f},
												 {0.05f, 0.07f, 0.13f}};
			if (choice >= 5 || choice < 0) {
				out[0] = st.bgCustom[0];
				out[1] = st.bgCustom[1];
				out[2] = st.bgCustom[2];
			} else {
				out[0] = kBgCol[choice][0];
				out[1] = kBgCol[choice][1];
				out[2] = kBgCol[choice][2];
			}
		}

		// â”€â”€ MENU DE VUE (actions) â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
		inline void PaintViewMenuPopup(NkModelerPainter &p, NkHitRegistry &hit, NkModelerState &st,
									   const NkRect &view, float32 barY, float32 barH) {
			if (!st.viewMenuOpen)
				return;
			int32 n = 0;
			const char *const *items = NkViewMenuItems(n);
			const NkIcon *icons = NkViewMenuIcons();
			const float32 itemH = S(24.f);
			float32 w = 0.f;
			for (int32 i = 0; i < n; ++i)
				if (p.TextW(items[i]) > w)
					w = p.TextW(items[i]);
			w += S(10.f) + S(19.f) + S(14.f);
			const NkRect box{view.x + S(10.f), barY + barH + S(4.f), w,
							 itemH * (float32)n + S(6.f)};
			p.Fill({box.x + 2.f, box.y + 2.f, box.w, box.h}, NkRole::WindowBg, 4.f);
			p.Outline(box, NkRole::Border, NkRole::PanelHeader, 4.f);
			hit.Add("vp.menu.panel", box);
			char k[32];
			for (int32 i = 0; i < n; ++i) {
				const NkRect ir{box.x + 2.f, box.y + S(3.f) + (float32)i * itemH, box.w - 4.f, itemH};
				snprintf(k, sizeof(k), "vp.menu.i%d", i);
				const bool over = hit.Add(k, ir);
				if (over)
					p.Fill(ir, NkRole::AccentUi, 3.f);
				p.IconV(ir.x + S(10.f), ir.y, itemH, icons[i], over ? NkRole::TextOnAccent : NkRole::Text,
						13.f);
				p.TextV(ir.x + S(10.f) + S(19.f), ir.y, itemH, items[i],
						over ? NkRole::TextOnAccent : NkRole::Text);
				if (hit.Clicked(k)) {
					if (i == 0)
						demo::Demo3DHostStoreCamera();
					else if (i == 1)
						demo::Demo3DHostRecallCamera();
					else if (i == 2)
						demo::Demo3DHostResetView();
					else {
						// Panneaux : tout montrer si l'un manque, sinon tout cacher.
						const bool anyHidden = !st.showLeft || !st.showRight || !st.showBrowser;
						st.showLeft = st.showRight = st.showBrowser = anyHidden;
					}
					st.viewMenuOpen = false;
				}
			}
			if (hit.AnyClick() && !hit.IsHovered("vp.menu.panel") && !hit.IsHovered("vp.menu"))
				st.viewMenuOpen = false;
		}

		// â”€â”€ FOND : prereglages + picker de couleur personnalisee â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
		inline void PaintBgPopup(NkModelerPainter &p, NkHitRegistry &hit, NkModelerState &st,
								 const NkRect &view, float32 barY, float32 barH) {
			if (!st.bgMenuOpen)
				return;
			static const char *const kNames[6] = {"Fond sombre", "Fond noir",	 "Fond gris",
												  "Fond clair",  "Fond bleu nuit", "Personnalisee..."};
			const float32 itemH = S(24.f);
			float32 w = 0.f;
			for (int32 i = 0; i < 6; ++i)
				if (p.TextW(kNames[i]) > w)
					w = p.TextW(kNames[i]);
			w += S(10.f) + S(22.f) + S(26.f) + S(10.f);
			// Ancre sous le 4e bouton de la barre gauche (menu, proj, ombrage, fond).
			const float32 ax = view.x + S(10.f) + S(3.f) + 3.f * (S(28.f) + 2.f);
			const NkRect box{ax, barY + barH + S(4.f), w, itemH * 6.f + S(6.f)};
			p.Fill({box.x + 2.f, box.y + 2.f, box.w, box.h}, NkRole::WindowBg, 4.f);
			p.Outline(box, NkRole::Border, NkRole::PanelHeader, 4.f);
			hit.Add("vp.bg.panel", box);
			char k[32];
			for (int32 i = 0; i < 6; ++i) {
				const NkRect ir{box.x + 2.f, box.y + S(3.f) + (float32)i * itemH, box.w - 4.f, itemH};
				snprintf(k, sizeof(k), "vp.bg.i%d", i);
				const bool over = hit.Add(k, ir);
				const bool cur = (st.bgChoice == i);
				if (over)
					p.Fill(ir, NkRole::AccentUi, 3.f);
				// Pastille de la COULEUR REELLE, picker compris : c'est elle qui
				// permet de choisir sans essayer.
				float32 c[3];
				NkBgColorOf(st, i, c);
				p.Fill({ir.x + S(8.f), ir.y + (itemH - S(12.f)) * 0.5f, S(14.f), S(12.f)},
					   NkColor{(uint8)(c[0] * 255.f), (uint8)(c[1] * 255.f), (uint8)(c[2] * 255.f), 255},
					   2.f);
				p.TextV(ir.x + S(10.f) + S(22.f), ir.y, itemH, kNames[i],
						over ? NkRole::TextOnAccent : NkRole::Text);
				if (cur)
					p.IconV(ir.x + ir.w - S(20.f), ir.y, itemH, NkIcon::Check,
							over ? NkRole::TextOnAccent : NkRole::AccentUi, 13.f);
				if (hit.Clicked(k)) {
					st.bgChoice = i;
					if (i == 5)
						st.bgPickerOpen = true; // la personnalisee OUVRE son picker
					else
						st.bgMenuOpen = st.bgPickerOpen = false;
				}
			}
			// â”€â”€ LE PICKER : trois barres R/V/B + temoin. Un vrai choix de
			// couleur, pas une roue complete -- elle viendra avec le theme.
			if (st.bgPickerOpen) {
				const float32 pw = S(210.f), ph = 3.f * S(24.f) + S(40.f);
				const NkRect pk{box.x + box.w + S(6.f), box.y, pw, ph};
				p.Fill({pk.x + 2.f, pk.y + 2.f, pk.w, pk.h}, NkRole::WindowBg, 4.f);
				p.Outline(pk, NkRole::Border, NkRole::PanelHeader, 4.f);
				hit.Add("vp.bg.picker", pk);
				static const char *const kCh[3] = {"R", "V", "B"};
				for (int32 c2 = 0; c2 < 3; ++c2) {
					const NkRect bar{pk.x + S(24.f), pk.y + S(8.f) + (float32)c2 * S(24.f) + S(4.f),
									 pw - S(36.f), S(12.f)};
					p.TextV(pk.x + S(8.f), bar.y - S(2.f), S(16.f), kCh[c2], NkRole::TextMuted);
					snprintf(k, sizeof(k), "vp.bg.ch%d", c2);
					hit.Add(k, bar);
					p.Fill(bar, NkRole::InputBg, 3.f);
					p.Fill({bar.x, bar.y, bar.w * st.bgCustom[c2], bar.h}, NkRole::AccentUi, 3.f);
					// GLISSEMENT : la barre suit la souris tant que le bouton est
					// tenu, meme sortie de la barre (c'est le canal qui possede le
					// geste, pas la position).
					if (hit.MouseDown() && (hit.IsHovered(k) || st.bgDragChannel == c2)) {
						if (st.bgDragChannel < 0 && hit.IsHovered(k))
							st.bgDragChannel = c2;
						if (st.bgDragChannel == c2) {
							float32 v = (hit.Mouse().x - bar.x) / bar.w;
							if (v < 0.f)
								v = 0.f;
							if (v > 1.f)
								v = 1.f;
							st.bgCustom[c2] = v;
							st.bgChoice = 5;
						}
					}
				}
				if (!hit.MouseDown())
					st.bgDragChannel = -1;
				// Temoin en pied : la couleur composee, en grand.
				p.Fill({pk.x + S(8.f), pk.y + ph - S(26.f), pw - S(16.f), S(18.f)},
					   NkColor{(uint8)(st.bgCustom[0] * 255.f), (uint8)(st.bgCustom[1] * 255.f),
							   (uint8)(st.bgCustom[2] * 255.f), 255},
					   3.f);
			}
			const bool overAll = hit.IsHovered("vp.bg.panel") || hit.IsHovered("vp.bg.picker") ||
								 hit.IsHovered("vp.bg");
			if (hit.AnyClick() && !overAll && st.bgDragChannel < 0)
				st.bgMenuOpen = st.bgPickerOpen = false;
		}

		// â”€â”€ MATCAPS PAR CATEGORIE, avec defilement V et H â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
		// Les 34 boules de la bibliotheque, groupees par famille (les plages
		// suivent kPresets de NkMatcapLibrary.cpp). Le panneau defile dans les
		// deux sens des que le contenu depasse -- demande de Rihen.
		inline void PaintMatcapPopup(NkModelerPainter &p, NkHitRegistry &hit, NkModelerState &st) {
			if (!st.matcapOpen)
				return;
			struct Cat {
					const char *name;
					int32 first, count;
			};
			static const Cat kCats[] = {
				{"Studio", 0, 6},		 {"Controle", 6, 5},  {"Argile", 11, 5}, {"Organique", 16, 4},
				{"Metal et verre", 20, 8}, {"Stylise", 28, 2}, {"Historiques", 30, 4},
			};
			const int32 nCats = (int32)(sizeof(kCats) / sizeof(kCats[0]));
			const int32 total = demo::Demo3DHostMatcapCount();
			const float32 headH = S(20.f), cellH = S(22.f), cellW = S(150.f);
			const int32 cols = 2;
			// Taille du CONTENU (avant defilement).
			float32 contentH = S(6.f);
			for (int32 c = 0; c < nCats; ++c) {
				int32 cnt = kCats[c].count;
				if (kCats[c].first + cnt > total)
					cnt = total > kCats[c].first ? total - kCats[c].first : 0;
				contentH += headH + cellH * (float32)((cnt + cols - 1) / cols);
			}
			const float32 contentW = S(8.f) + cellW * (float32)cols;
			// Boite : ancree au bouton qui l'a ouverte, bornee a la fenetre --
			// elle peut donc s'ouvrir depuis la barre de la vue comme depuis le
			// panneau Proprietes.
			float32 boxW = contentW + S(12.f);
			if (boxW > NkPopupBoundsW() - S(40.f))
				boxW = NkPopupBoundsW() - S(40.f);
			float32 boxH = contentH + S(12.f);
			const float32 maxH = NkPopupBoundsH() - S(90.f);
			if (boxH > maxH)
				boxH = maxH;
			const NkRect box = NkFitPopup(st.matcapAnchor, boxW, boxH);
			p.Fill({box.x + 2.f, box.y + 2.f, box.w, box.h}, NkRole::WindowBg, 4.f);
			p.Outline(box, NkRole::Border, NkRole::PanelHeader, 4.f);
			hit.Add("vp.mc.panel", box);
			const bool needV = contentH > box.h - S(8.f);
			const bool needH = contentW > box.w - S(8.f);
			const float32 viewH = box.h - S(8.f) - (needH ? S(8.f) : 0.f);
			const float32 viewW = box.w - S(8.f) - (needV ? S(8.f) : 0.f);
			// Bornes du defilement.
			float32 maxSy = contentH - viewH;
			if (maxSy < 0.f)
				maxSy = 0.f;
			float32 maxSx = contentW - viewW;
			if (maxSx < 0.f)
				maxSx = 0.f;
			// Molette = vertical (Maj = horizontal), comme partout.
			if (hit.IsHovered("vp.mc.panel") && hit.WheelDelta() != 0.f) {
				if (hit.ShiftDown())
					st.matcapScrollX -= hit.WheelDelta() * S(24.f);
				else
					st.matcapScrollY -= hit.WheelDelta() * S(24.f);
			}
			if (st.matcapScrollY < 0.f)
				st.matcapScrollY = 0.f;
			if (st.matcapScrollY > maxSy)
				st.matcapScrollY = maxSy;
			if (st.matcapScrollX < 0.f)
				st.matcapScrollX = 0.f;
			if (st.matcapScrollX > maxSx)
				st.matcapScrollX = maxSx;

			// Contenu (borne a la boite : pas de clip pixel, on SAUTE les lignes
			// hors champ -- suffisant pour des rangees regulieres).
			const int32 cur = demo::Demo3DHostMatcap();
			float32 y = box.y + S(4.f) - st.matcapScrollY;
			const float32 x0 = box.x + S(4.f) - st.matcapScrollX;
			char k[32];
			for (int32 c = 0; c < nCats; ++c) {
				int32 cnt = kCats[c].count;
				if (kCats[c].first + cnt > total)
					cnt = total > kCats[c].first ? total - kCats[c].first : 0;
				if (cnt <= 0)
					continue;
				if (y + headH > box.y && y < box.y + viewH)
					p.TextV(x0 + S(6.f), y, headH, kCats[c].name, NkRole::TextMuted);
				y += headH;
				for (int32 i = 0; i < cnt; ++i) {
					const int32 id = kCats[c].first + i;
					const float32 cx = x0 + (float32)(i % cols) * cellW;
					const float32 cy = y + (float32)(i / cols) * cellH;
					if (cy + cellH < box.y || cy > box.y + viewH) {
						continue;
					}
					const NkRect ir{cx, cy, cellW - S(4.f), cellH - 2.f};
					snprintf(k, sizeof(k), "vp.mc.%d", id);
					const bool over = hit.Add(k, ir);
					const bool sel = (id == cur);
					if (sel)
						p.Fill(ir, NkRole::AccentUi, 3.f);
					else if (over)
						p.Fill(ir, NkRole::PanelHeader, 3.f);
					// La VIGNETTE REELLE de la boule, pas un pictogramme generique.
					p.Image(4300u + (uint32)id,
							{ir.x + S(3.f), ir.y + S(2.f), ir.h - S(4.f), ir.h - S(4.f)});
					p.TextV(ir.x + ir.h + S(4.f), ir.y, ir.h, demo::Demo3DHostMatcapName(id),
							sel ? NkRole::TextOnAccent : NkRole::Text);
					if (hit.Clicked(k))
						demo::Demo3DHostSetMatcap(id);
				}
				y += cellH * (float32)((cnt + cols - 1) / cols);
			}

			// â”€â”€ ASCENSEURS : VERTICAL puis HORIZONTAL, glissables â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
			if (needV) {
				const NkRect track{box.x + box.w - S(8.f), box.y + S(4.f), S(5.f), viewH};
				p.Fill(track, NkRole::InputBg, 2.f);
				const float32 thH = viewH * (viewH / contentH) < S(20.f)
									   ? S(20.f)
									   : viewH * (viewH / contentH);
				const float32 thY = track.y + (track.h - thH) * (maxSy > 0.f ? st.matcapScrollY / maxSy : 0.f);
				const NkRect th{track.x, thY, track.w, thH};
				hit.Add("vp.mc.sbv", th);
				p.Fill(th, NkRole::AccentUi, 2.f);
				if (hit.MouseDown() && (hit.IsHovered("vp.mc.sbv") || st.matcapDragBar == 0)) {
					if (st.matcapDragBar < 0 && hit.IsHovered("vp.mc.sbv")) {
						st.matcapDragBar = 0;
						st.matcapDragOff = hit.Mouse().y - thY;
					}
					if (st.matcapDragBar == 0 && track.h > thH)
						st.matcapScrollY =
							(hit.Mouse().y - st.matcapDragOff - track.y) / (track.h - thH) * maxSy;
				}
			}
			if (needH) {
				const NkRect track{box.x + S(4.f), box.y + box.h - S(8.f), viewW, S(5.f)};
				p.Fill(track, NkRole::InputBg, 2.f);
				const float32 thW = viewW * (viewW / contentW) < S(20.f)
									   ? S(20.f)
									   : viewW * (viewW / contentW);
				const float32 thX = track.x + (track.w - thW) * (maxSx > 0.f ? st.matcapScrollX / maxSx : 0.f);
				const NkRect th{thX, track.y, thW, track.h};
				hit.Add("vp.mc.sbh", th);
				p.Fill(th, NkRole::AccentUi, 2.f);
				if (hit.MouseDown() && (hit.IsHovered("vp.mc.sbh") || st.matcapDragBar == 1)) {
					if (st.matcapDragBar < 0 && hit.IsHovered("vp.mc.sbh")) {
						st.matcapDragBar = 1;
						st.matcapDragOff = hit.Mouse().x - thX;
					}
					if (st.matcapDragBar == 1 && track.w > thW)
						st.matcapScrollX =
							(hit.Mouse().x - st.matcapDragOff - track.x) / (track.w - thW) * maxSx;
				}
			}
			if (!hit.MouseDown())
				st.matcapDragBar = -1;
			// Le panneau s'ouvre depuis DEUX boutons (barre de la vue, panneau
			// Proprietes) : la fermeture au clic exterieur doit les connaitre
			// tous les deux -- sinon le clic d'OUVERTURE du second refermait le
			// panneau dans la meme frame (constate par Rihen).
			if (hit.AnyClick() && !hit.IsHovered("vp.mc.panel") && !hit.IsHovered("vp.matcap") &&
				!hit.IsHovered("props.matcap"))
				st.matcapOpen = false;
		}

		// â”€â”€ VUE 3D (centre) â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
		inline void PaintViewport(NkModelerPainter &p, const NkRect &r, NkModelerState &st,
								  NkHitRegistry &hit, NkWidgetState &ws,
								  const nkgui::NkGuiInput &in, NkComboPending &combo,
								  NkCheckPending &checks, const NkShortcutTable &sc,
								  nkgui::NkGuiContext *guiCtx = nullptr) {
			// Le bloc de surcouche est RE-ARME chaque frame par qui en a besoin
			// (badge vue camera...) : on repart de zero ici, sinon un bloc
			// perime survivrait au changement d'onglet et refuserait des clics
			// sans raison visible.
			hit.SetBlock(NkRect{0.f, 0.f, 0.f, 0.f}, false);
			const bool editMode = (st.mode != NkMode::Object);

			// â”€â”€ TAB BAR D'ESPACES DE TRAVAIL â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
			// Un seul espace aujourd'hui (Modelisation) ; Sculpt, Texturing et
			// NkAnima s'y rangeront. L'en-tete s'ESCAMOTE d'un clic sur le
			// chevron -- replie, seul un petit chevron discret le rappelle.
			float32 wsBarH = 0.f;
			if (st.wsBarOpen) {
				wsBarH = S(24.f);
				const NkRect wb{r.x, r.y, r.w, wsBarH};
				p.Fill(wb, NkRole::PanelHeader);
				p.HLine(r.x, r.y + wsBarH - 1.f, r.w);
				// LES ESPACES SONT LES MODES (regle de Rihen) : Objet, Edition,
				// Sculpture 2.5D, Sculpture, Texturing -- l'onglet actif EST le
				// mode courant. Le deroulant equivalent de la barre d'outils a
				// ete retire : un seul endroit pour changer de mode.
				static const NkIcon kWsIc[7] = {NkIcon::Mesh,	NkIcon::Edit,
												NkIcon::Layers, NkIcon::Ruler,
												NkIcon::Overlay, NkIcon::ViewUV,
												NkIcon::Picker};
				static const char *const kWsNames[7] = {
					"Objet",	 "Edition", "Sculpture 2.5D",	"Sculpture",
					"Texturing", "Patron",	"Texture painting"};
				float32 tx5 = r.x + S(8.f);
				char wk5[16];
				for (int32 t5 = 0; t5 < 7; ++t5) {
					const float32 tw5 = p.TextW(kWsNames[t5]);
					const NkRect t0{tx5, r.y + S(2.f), tw5 + S(32.f), wsBarH - S(4.f)};
					snprintf(wk5, sizeof(wk5), "ws.tab.%d", t5);
					const bool overT = hit.Add(wk5, t0);
					const bool onT = (int32)st.mode == t5;
					if (onT)
						p.Fill(t0, NkRole::AccentUi, 3.f);
					else if (overT)
						p.Fill(t0, NkRole::InputBg, 3.f);
					p.IconV(t0.x + S(6.f), t0.y, t0.h, kWsIc[t5],
							onT ? NkRole::TextOnAccent : NkRole::TextMuted, 12.f);
					p.TextV(t0.x + S(24.f), t0.y, t0.h, kWsNames[t5],
							onT ? NkRole::TextOnAccent : NkRole::Text);
					if (hit.Clicked(wk5))
						st.mode = (NkMode)t5;
					tx5 += t0.w + S(4.f);
					// MARQUEUR DE SEPARATION entre onglets (regle de Rihen) :
					// sans lui, sept libelles cote a cote se lisaient comme une
					// seule phrase.
					if (t5 < 6) {
						p.VLine(tx5, r.y + S(5.f), wsBarH - S(10.f));
						tx5 += S(5.f);
					}
				}
				const NkRect ch{r.x + r.w - S(26.f), r.y + S(2.f), S(20.f), wsBarH - S(4.f)};
				HoverFill(p, ch, hit.Add("ws.hide", ch), 2.f);
				p.IconV(ch.x + S(3.f), ch.y, ch.h, NkIcon::ChevronUp, NkRole::TextMuted, 12.f);
				if (hit.Clicked("ws.hide"))
					st.wsBarOpen = false;
			} else {
				// Replie : une POIGNEE VISIBLE, comme celles des panneaux -- le
				// chevron seul de 20 px etait introuvable (constate par Rihen).
				const NkRect ch{r.x + r.w * 0.5f - S(52.f), r.y + S(2.f), S(104.f), S(16.f)};
				const bool overC = hit.Add("ws.show", ch);
				p.Outline(ch, overC ? NkRole::AccentUi : NkRole::Border, NkRole::PanelHeader, 3.f);
				p.IconV(ch.x + S(6.f), ch.y, ch.h, NkIcon::ChevronDown, NkRole::Text, 11.f);
				p.TextV(ch.x + S(22.f), ch.y, ch.h, "Espaces", NkRole::TextMuted);
				if (hit.Clicked("ws.show"))
					st.wsBarOpen = true;
			}
			// LA VUE EST RECADREE sous la barre d'espaces : sans cela elle
			// coupait le haut de l'image (le texte du HUD, constate par Rihen).
			NkRect vr = r;
			vr.y += wsBarH;
			vr.h -= wsBarH;

			// â”€â”€ LA VUE 3D REELLE â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
			// Ce qui etait peint ici jusqu'a present -- un sol en fuyantes dessine a
			// la main -- etait un DECOR. Il donnait l'impression d'une perspective
			// sans camÃ©ra, sans profondeur et sans le moindre objet : rien de ce
			// qu'on y voyait ne pouvait etre selectionne, tourne ni modifie.
			//
			// A la place, la scene est rendue par NKRenderer dans une cible hors
			// ecran, sur le meme device et le meme command buffer que l'interface,
			// et on pose ici SA TEXTURE. Aucune relecture CPU, aucune seconde pile
			// GPU : l'image ne quitte jamais la carte.
			// Fond de la ZONE IMAGE seulement (vr, pas r) : peindre r entier
			// recouvrait la barre d'espaces peinte juste au-dessus.
			p.Fill(vr, NkRole::ViewportTop); // visible tant que la 3D n'est pas prete
			// ONGLET EDITEUR : seul MODEL s'edite dans un vrai viewport (meme
			// interface qu'une scene, regle de Rihen). Materiau, texture,
			// blueprint et dataset = CONTENU VIDE tant que leur design n'est
			// pas defini (peinture, nodes proceduraux, NKGraphe a venir).
			if (st.sceneTabKind[st.activeTab] != 0 &&
				st.sceneTabKind[st.activeTab] != 7) {
				const uint8 ek = (uint8)(st.sceneTabKind[st.activeTab] - 1);
				const char *en = (ek == 0)	 ? "Editeur de Graphe"
								 : (ek == 2) ? "Editeur de Materiau"
								 : (ek == 3) ? "Editeur de Texture"
								 : (ek == 4) ? "Editeur de Dataset IA"
								 : (ek == 6) ? "Editeur de Model"
											 : "Editeur";
				p.Fill(vr, NkRole::WindowBg);
				const float32 cxE = vr.x + vr.w * 0.5f;
				const float32 cyE = vr.y + vr.h * 0.5f;
				p.TextV(cxE - p.TextW(en) * 0.5f, cyE - S(36.f), kRowH, en,
						NkRole::Text);
				const int32 aiE = st.sceneTabAsset[st.activeTab] - 1;
				if (aiE >= 0 && aiE < st.browserCount)
					p.TextV(cxE - p.TextW(st.browserNames[aiE]) * 0.5f,
							cyE - S(12.f), kRowH, st.browserNames[aiE],
							NkRole::TextMuted);
				p.TextV(cxE - p.TextW("Interface a definir -- NKGraphe, peinture, "
									  "procedural a venir") *
								 0.5f,
						cyE + S(12.f), kRowH,
						"Interface a definir -- NKGraphe, peinture, procedural a venir",
						NkRole::TextMuted);
				st.viewRect = {0.f, 0.f, 0.f, 0.f}; // pas de depot 3D ici
				return;
			}
			// PORTAGE INTEGRAL de --demo=2 : la texture vient desormais de la demo
			// portee (NkDemo3D.cpp), sous le MEME id 4096. L'ancienne vue est
			// dormante ; c'est donc l'hote de la demo qui dit Â« pret Â».
			if (demo::Demo3DHostReady()) {
				p.Image(nk3d::kViewportTexId, vr);
				st.viewRect = vr; // depot d'assets : importer un clone en scene
				// ── PASSE-PARTOUT (Rihen) : en vue camera, ce qui deborde du
				// CADRE de la camera est voile -- couleur/opacite PAR camera
				// (noir 60 % par defaut, panneau de la camera). Cadre 16:9
				// (Full HD) en v1 ; la pastille Output pilotera le format.
				// Habillage UI : il n'apparait PAS dans les captures, qui
				// figent la cible hors ecran en dessous.
				if (st.camViewNode > 0) {
					float32 pp[4];
					demo::Demo3DHostCamPasse(st.camViewNode - 1, pp);
					if (pp[3] > 0.003f && vr.w > 8.f && vr.h > 8.f) {
						// LE CADRE VIENT DE L'HOTE : c'est la MEME verite que le
						// rendu (qui zoome pour que l'image exacte de la camera
						// occupe ce cadre) et que la capture (qui recadre
						// dessus). Le voile epouse donc les VRAIS bords de la
						// camera (exigence de Rihen).
						float32 fr4[4];
						demo::Demo3DHostCameraFrame(fr4);
						const float32 fx = vr.x + fr4[0] * vr.w;
						const float32 fy = vr.y + fr4[1] * vr.h;
						const float32 fw = fr4[2] * vr.w;
						const float32 fh = fr4[3] * vr.h;
						const NkColor pc{(uint8)(pp[0] * 255.f), (uint8)(pp[1] * 255.f),
										 (uint8)(pp[2] * 255.f), (uint8)(pp[3] * 255.f)};
						if (fy > vr.y)
							p.Fill({vr.x, vr.y, vr.w, fy - vr.y}, pc, 0.f);
						if (fy + fh < vr.y + vr.h)
							p.Fill({vr.x, fy + fh, vr.w, vr.y + vr.h - fy - fh}, pc, 0.f);
						if (fx > vr.x)
							p.Fill({vr.x, fy, fx - vr.x, fh}, pc, 0.f);
						if (fx + fw < vr.x + vr.w)
							p.Fill({fx + fw, fy, vr.x + vr.w - fx - fw, fh}, pc, 0.f);
						// Lisere fin du cadre, discret.
						const NkColor fr{255, 255, 255, 70};
						p.Fill({fx, fy, fw, 1.f}, fr, 0.f);
						p.Fill({fx, fy + fh - 1.f, fw, 1.f}, fr, 0.f);
						p.Fill({fx, fy, 1.f, fh}, fr, 0.f);
						p.Fill({fx + fw - 1.f, fy, 1.f, fh}, fr, 0.f);
						// ── GUIDES DE COMPOSITION + ZONES SURES (par camera) ──
						// Traces DANS le cadre : habillage de la vue, jamais dans
						// les captures (elles figent la cible en dessous).
						const int32 gd2 = demo::Demo3DHostCamGuides(st.camViewNode - 1);
						if (gd2) {
							const NkColor glc{255, 255, 255, 55};
							if (gd2 & 1) { // tiers
								p.Fill({fx + fw / 3.f, fy, 1.f, fh}, glc, 0.f);
								p.Fill({fx + 2.f * fw / 3.f, fy, 1.f, fh}, glc, 0.f);
								p.Fill({fx, fy + fh / 3.f, fw, 1.f}, glc, 0.f);
								p.Fill({fx, fy + 2.f * fh / 3.f, fw, 1.f}, glc, 0.f);
							}
							if (gd2 & 2) { // centre
								p.Fill({fx + fw * 0.5f, fy, 1.f, fh}, glc, 0.f);
								p.Fill({fx, fy + fh * 0.5f, fw, 1.f}, glc, 0.f);
							}
							if (gd2 & 4) { // diagonales
								p.Line(fx, fy, fx + fw, fy + fh, glc, 1.f);
								p.Line(fx + fw, fy, fx, fy + fh, glc, 1.f);
							}
							if (gd2 & 8) { // nombre d'or (0,382 / 0,618)
								p.Fill({fx + fw * 0.382f, fy, 1.f, fh}, glc, 0.f);
								p.Fill({fx + fw * 0.618f, fy, 1.f, fh}, glc, 0.f);
								p.Fill({fx, fy + fh * 0.382f, fw, 1.f}, glc, 0.f);
								p.Fill({fx, fy + fh * 0.618f, fw, 1.f}, glc, 0.f);
							}
							if (gd2 & 16) { // zones sures : action 3,5 %, titre 10/5 %
								const NkColor zsa{120, 220, 130, 80};
								const NkColor zst{220, 140, 120, 80};
								const float32 mrg[2][2] = {{0.035f, 0.035f}, {0.10f, 0.05f}};
								for (int32 z2 = 0; z2 < 2; ++z2) {
									const NkColor &zc = z2 == 0 ? zsa : zst;
									const float32 sx = fx + fw * mrg[z2][0];
									const float32 sy = fy + fh * mrg[z2][1];
									const float32 sw = fw * (1.f - 2.f * mrg[z2][0]);
									const float32 sh = fh * (1.f - 2.f * mrg[z2][1]);
									p.Fill({sx, sy, sw, 1.f}, zc, 0.f);
									p.Fill({sx, sy + sh - 1.f, sw, 1.f}, zc, 0.f);
									p.Fill({sx, sy, 1.f, sh}, zc, 0.f);
									p.Fill({sx + sw - 1.f, sy, 1.f, sh}, zc, 0.f);
								}
							}
						}
					}
					// ── APERCU DES INCRUSTATIONS (Rihen) ────────────────────
					// Les miniatures posees sur l'image de sortie se voient DANS
					// le cadre, a leur place et a leur forme. Sans cet apercu il
					// faudrait lancer un rendu pour savoir ou elles tombent : on
					// les placerait a l'aveugle. Comme les guides, c'est de
					// l'habillage -- la capture fige la cible en dessous et n'en
					// garde rien.
					// HORS du bloc du passe-partout, et non dedans : un voile a
					// zero pour cent ne doit pas faire disparaitre l'apercu.
					// SEULEMENT DEPUIS LA SOURCE PRINCIPALE (Rihen) : les
					// miniatures appartiennent a la composition de l'image
					// principale. Les montrer alors qu'on regarde par une camera
					// qui n'est PAS cette source -- par exemple celle qui
					// alimente une miniature -- faisait croire qu'elles se
					// composeraient aussi dans cette vue-la. Depuis une source
					// secondaire, on doit voir ce que cette camera voit, rien de
					// plus.
					int32 mainSrc = -1;
					demo::Demo3DHostOutMain(&mainSrc, nullptr, nullptr, nullptr, nullptr,
											nullptr);
					const int32 curCam = st.camViewNode > 0 ? st.camViewNode - 1 : -1;
					if (curCam == mainSrc) {
						float32 fr5[4];
						demo::Demo3DHostCameraFrame(fr5);
						const float32 fx = vr.x + fr5[0] * vr.w;
						const float32 fy = vr.y + fr5[1] * vr.h;
						const float32 fw = fr5[2] * vr.w;
						const float32 fh = fr5[3] * vr.h;
						const int32 nMaxI = demo::Demo3DHostOutInsetMax();
						for (int32 q2 = 0; q2 < nMaxI; ++q2) {
							int32 iSrc = -1, iShape = 0;
							float32 iXY[2] = {0.f, 0.f}, iSz[2] = {0.25f, 0.25f}, iBrd = 2.f,
									iCol[3] = {1.f, 1.f, 1.f}, iOpa = 1.f;
							if (!demo::Demo3DHostOutInset(q2, &iSrc, &iShape, iXY, iSz, &iBrd,
														  iCol, &iOpa))
								continue;
							// MEME regle de cadre que le rendu : chaque forme a
							// ses dimensions, et celles a un seul cote se ferment
							// sur un carre. Une autre formule ici mentirait sur
							// le resultat.
							const float32 iw = fw * iSz[0];
							const float32 ih = (nk3d::NkInsetDimCount(iShape) == 1)
												   ? iw
												   : fh * iSz[1];
							const float32 ix = fx + iXY[0] * fw;
							const float32 iy = fy + iXY[1] * fh;
							const NkColor bc{(uint8)(iCol[0] * 255.f), (uint8)(iCol[1] * 255.f),
											 (uint8)(iCol[2] * 255.f), (uint8)(200.f * iOpa)};
							const NkColor fill{(uint8)(iCol[0] * 255.f), (uint8)(iCol[1] * 255.f),
											   (uint8)(iCol[2] * 255.f), (uint8)(38.f * iOpa)};
							if (iShape == 2 || iShape == 3) {
								// Rond : disque tres pale borde d'un anneau.
								const float32 rr2 = (iw < ih ? iw : ih) * 0.5f;
								p.DiscColor(ix + iw * 0.5f, iy + ih * 0.5f, rr2, fill);
								p.RingColor(ix + iw * 0.5f, iy + ih * 0.5f, rr2, bc, fill);
							} else if (iShape == 5) {
								const float32 cx2 = ix + iw * 0.5f, cy2 = iy + ih * 0.5f;
								p.Line(cx2, iy, ix + iw, cy2, bc, 1.f);
								p.Line(ix + iw, cy2, cx2, iy + ih, bc, 1.f);
								p.Line(cx2, iy + ih, ix, cy2, bc, 1.f);
								p.Line(ix, cy2, cx2, iy, bc, 1.f);
							} else {
								const float32 rnd = (iShape == 4) ? ih * 0.18f : 0.f;
								p.Fill({ix, iy, iw, ih}, fill, rnd);
								p.Fill({ix, iy, iw, 1.f}, bc, 0.f);
								p.Fill({ix, iy + ih - 1.f, iw, 1.f}, bc, 0.f);
								p.Fill({ix, iy, 1.f, ih}, bc, 0.f);
								p.Fill({ix + iw - 1.f, iy, 1.f, ih}, bc, 0.f);
							}
							// Numero et source : c'est ce qui permet de savoir
							// laquelle on regarde quand elles se recouvrent.
							char cn2[24] = {};
							if (iSrc >= 0)
								NkHierNodeName(st, iSrc, cn2, sizeof(cn2));
							char il[48];
							snprintf(il, sizeof(il), "%d - %s", (int)(q2 + 1),
									 iSrc < 0 ? "Vue 3D" : (cn2[0] ? cn2 : "Camera"));
							p.Clip({ix, iy, iw, ih});
							p.TextV(ix + S(6.f), iy + S(2.f), S(18.f), il, NkRole::Text);
							p.Unclip();
						}
					}
				}
				// ── VUE CAMERA (Rihen) ──────────────────────────────────────
				// Bascule entre la vue 3D libre et CE QUE VOIT une camera de la
				// scene. Le selecteur liste les cameras du document actif.
				{
					// SYNC AVEC L'HOTE : la bascule CLAVIER (pave 0 / Ctrl+0)
					// change la vue cote hote -- le libelle du selecteur suit.
					{
						const int32 hostCv = demo::Demo3DHostCameraView();
						if (hostCv + 1 != st.camViewNode)
							st.camViewNode = hostCv >= 0 ? hostCv + 1 : 0;
					}
					if (st.camViewNode > 0 &&
						(NkHierNodeSkip(st.camViewNode - 1) ||
						 demo::Demo3DHostUserSub(st.camViewNode - 1) != 10)) {
						// la camera regardee a disparu (ou change de document) :
						// retour vue libre par le chemin unique (pose restituee)
						st.camViewNode = 0;
						demo::Demo3DHostViewCamera(-1);
					}
					// LEVEE du bloc AVANT de peindre le badge : ses propres clics
					// doivent repondre (Clicked refuse tout clic dans l'emprise
					// bloquee). Il sera re-arme en fin de section avec l'emprise
					// badge + liste.
					hit.SetBlock(NkRect{0.f, 0.f, 0.f, 0.f}, false);
					char vlb[48];
					if (st.camViewNode > 0) {
						char cnm[24];
						NkHierNodeName(st, st.camViewNode - 1, cnm, sizeof(cnm));
						snprintf(vlb, sizeof(vlb), "Vue camera : %s", cnm);
					} else {
						snprintf(vlb, sizeof(vlb), "Vue 3D");
					}
					const float32 vw = p.TextW(vlb) + S(34.f);
					// SOUS la barre d'icones du viewport : posee a la meme place,
					// elle RECOUVRAIT le selecteur (constate a l'ecran).
					const NkRect vb{vr.x + S(8.f), vr.y + S(44.f), vw, S(20.f)};
					const bool ovV = hit.Add("view.pick", vb);
					// Fond PLEIN : pose sur l'image 3D, un simple contour se perd.
					p.Fill(vb, NkColor{0, 0, 0, 150}, 4.f);
					p.Outline(vb, ovV || st.camPickOpen ? NkRole::AccentUi : NkRole::Border,
							  NkRole::PanelHeader, 4.f);
					p.IconV(vb.x + S(5.f), vb.y, vb.h, NkIcon::Camera,
							st.camViewNode > 0 ? NkRole::AccentUi : NkRole::TextMuted,
							12.f);
					p.TextV(vb.x + S(22.f), vb.y, vb.h, vlb);
					// LA CLE DU CLIC EST CELLE DU RECT ENREGISTRE (« view.pick »).
					// Elle testait « view.cam » -- la cle du bouton camera de VOL
					// de la colonne de navigation : le selecteur ne s'ouvrait
					// jamais par lui-meme, et le bouton de vol l'ouvrait par
					// accident (LE probleme de camera constate par Rihen).
					if (hit.Clicked("view.pick"))
						st.camPickOpen = !st.camPickOpen;
					if (st.camPickOpen) {
						// Liste : la vue 3D, puis TOUTES les cameras du document.
						int32 cams[16];
						int32 nCam = 0;
						const int32 ncT = demo::Demo3DHostNodeCount();
						for (int32 c1 = 0; c1 < ncT && nCam < 16; ++c1)
							if (!NkHierNodeSkip(c1) &&
								demo::Demo3DHostUserKind(c1) == 4 &&
								demo::Demo3DHostUserSub(c1) == 10)
								cams[nCam++] = c1;
						const NkRect lb{vb.x, vb.y + vb.h + 2.f, vb.w < S(150.f) ? S(150.f)
																			 : vb.w,
										kRowH * (float32)(nCam + 1)};
						p.Fill({lb.x + 2.f, lb.y + 2.f, lb.w, lb.h}, NkColor{0, 0, 0, 90}, 4.f);
						p.Outline(lb, NkRole::AccentUi, NkRole::PanelHeader, 4.f);
						const NkRect i0{lb.x, lb.y, lb.w, kRowH};
						HoverFill(p, i0, hit.Add("view.cam.free", i0), 0.f);
						p.TextV(i0.x + S(10.f), i0.y, kRowH, "Vue 3D");
						if (hit.Clicked("view.cam.free")) {
							// RETOUR vue libre par le CHEMIN UNIQUE de l'hote --
							// c'est lui qui memorise et restitue la pose, pour que
							// selecteur et pave 0 restent d'accord.
							demo::Demo3DHostViewCamera(-1);
							st.camViewNode = 0;
							st.camPickOpen = false;
						}
						for (int32 c2 = 0; c2 < nCam; ++c2) {
							const NkRect ic{lb.x, lb.y + kRowH * (float32)(c2 + 1), lb.w,
											kRowH};
							char ck2[32], cnm2[24];
							snprintf(ck2, sizeof(ck2), "view.cam.%d", cams[c2]);
							HoverFill(p, ic, hit.Add(ck2, ic), 0.f);
							NkHierNodeName(st, cams[c2], cnm2, sizeof(cnm2));
							p.IconV(ic.x + S(8.f), ic.y, kRowH, NkIcon::Camera,
									st.camViewNode == cams[c2] + 1 ? NkRole::AccentUi
																  : NkRole::TextMuted,
									12.f);
							p.TextV(ic.x + S(26.f), ic.y, kRowH, cnm2);
							if (hit.Clicked(ck2)) {
								// CHEMIN UNIQUE de l'hote : il memorise la pose
								// libre une seule fois, regarde cette camera et la
								// rend ACTIVE (facon Blender) -- le pave 0 y
								// reviendra directement.
								demo::Demo3DHostViewCamera(cams[c2]);
								st.camViewNode = cams[c2] + 1;
								st.camPickOpen = false;
							}
						}
						if (hit.AnyClick() && !NkHitRegistry::Contains(lb, hit.Mouse()) &&
							!hit.IsHovered("view.pick"))
							st.camPickOpen = false;
						// SURCOUCHE BLOQUANTE (patron NKCode, mecanisme SetBlock
						// deja porte ici mais jamais arme) : l'emprise badge +
						// liste refuse ses clics au reste de l'application --
						// la scene comprise, qui recevait selection et
						// deselection fantomes a travers la liste (Rihen).
						const float32 bw3 = (lb.w > vb.w ? lb.w : vb.w);
						hit.SetBlock({vb.x, vb.y, bw3, (lb.y + lb.h) - vb.y}, true);
					}
					if (!st.camPickOpen)
						hit.SetBlock(vb, true); // badge seul : son clic ne traverse pas non plus
				}
				if (!st.wsBarOpen) {
					// La poignee « Espaces » se REPEINT par-dessus l'image : elle
					// etait recouverte, donc introuvable barre fermee (Rihen).
					const NkRect ch2{r.x + r.w * 0.5f - S(52.f), r.y + S(2.f), S(104.f),
									 S(16.f)};
					p.Outline(ch2, NkRole::Border, NkRole::PanelHeader, 4.f);
					p.IconV(ch2.x + S(6.f), ch2.y, ch2.h, NkIcon::ChevronDown,
							NkRole::TextMuted, 12.f);
					p.TextV(ch2.x + S(22.f), ch2.y, ch2.h, "Espaces", NkRole::TextMuted);
				}
				// ── AJUSTER LA CREATION (facon Blender), bas-droit de la vue ──
				// Chaque nature a SES champs (regles de Rihen) : sphere =
				// segments/anneaux/rayon ; icosphere = subdivisions/rayon ; tore =
				// deux rayons ; cube = largeur/hauteur/profondeur... Valider par
				// « Appliquer » ou par un clic dans la vue.
				if (st.addAdjustNode >= 0) {
					int32 sgA = 0, rgA = 0;
					float32 axA = 0.15f;
					const bool hasParams =
						demo::Demo3DHostMeshParams(st.addAdjustNode, &sgA, &rgA, &axA);
					if (demo::Demo3DHostNodeDeleted(st.addAdjustNode)) {
						st.addAdjustNode = -1;
					} else {
						const int32 ukA = demo::Demo3DHostUserKind(st.addAdjustNode);
						const int32 sbA = demo::Demo3DHostUserSub(st.addAdjustNode);
						const bool isSph = ukA == 1 && sbA == 0;
						const bool isIco = ukA == 1 && sbA == 1;
						const bool isTor = ukA == 1 && sbA == 2;
						const bool isCap = ukA == 1 && sbA == 3;
						const bool isCyl = ukA == 2 && (sbA == 1 || sbA == 2);
						const bool isPln = ukA == 3;
						const bool isCir = ukA == 10;
						const bool showSeg = hasParams;
						const bool showRing = isSph || isTor || isCap;
						const bool showRay = isSph || isIco || isTor || isCap || isCyl || isCir;
						const bool showHaut = isCap || isCyl;
						const bool showAux = isTor;
						const bool showLHP = !showRay && ukA != 5; // cube, plan, vides...
						const char *segLbl =
							isIco ? "Subdivisions" : (isPln ? "Divisions" : "Segments");
						int32 tyP = -1;
						float32 rgP = 0.f, inP = 0.f, outP = 0.f, awP = 0.f, ahP = 0.f;
						bool shP = true;
						if (ukA == 5)
							demo::Demo3DHostLightEx(st.addAdjustNode, &rgP, &inP, &outP, &awP,
													&ahP, &shP, &tyP);
						const int32 rowsN = 2 + (showSeg ? 1 : 0) + (showRing ? 1 : 0) +
											(showRay ? 1 : 0) + (showHaut ? 1 : 0) +
											(showAux ? 1 : 0) + (showLHP ? (isPln ? 2 : 3) : 0) +
											(ukA == 5 ? 1 + (tyP != 0 ? 1 : 0) +
															(tyP == 2 ? 2 : 0) +
															(tyP == 3 ? 2 : 0)
													  : 0);
						const float32 pw = S(232.f);
						const float32 ph = kRowH * (float32)rowsN + S(10.f);
						const NkRect aj{vr.x + vr.w - pw - S(10.f), vr.y + vr.h - ph - S(10.f),
										pw, ph};
						if (hit.AnyClick() && NkHitRegistry::Contains(vr, hit.Mouse()) &&
							!NkHitRegistry::Contains(aj, hit.Mouse())) {
							st.addAdjustNode = -1; // un clic dans la vue VALIDE
						} else {
							p.Outline(aj, NkRole::Border, NkRole::PanelHeader, 4.f);
							hit.Add("vp.adjust", aj);
							char nmA[32];
							NkHierNodeName(st, st.addAdjustNode, nmA, sizeof(nmA));
							char tA[48];
							snprintf(tA, sizeof(tA), "Creation : %s", nmA);
							p.TextV(aj.x + S(8.f), aj.y + S(3.f), kRowH, tA);
							float32 ay = aj.y + S(3.f) + kRowH;
							auto AdjRow = [&](const char *lbl, const char *key2, float32 &val,
											  float32 step, const char *fmt2) {
								p.TextV(aj.x + S(8.f), ay, kRowH, lbl, NkRole::TextMuted);
								const bool ch2 = DragFloat(
									p, hit, ws, in, key2,
									{aj.x + S(104.f), ay + S(3.f), pw - S(112.f), kRowH - S(4.f)},
									val, step, NkRole::AccentUi, fmt2);
								ay += kRowH;
								return ch2;
							};
							float32 fsg = (float32)sgA, frg = (float32)rgA, fax = axA;
							bool prmCh = false;
							if (showSeg)
								prmCh |= AdjRow(segLbl, "vp.adj.seg", fsg, 0.2f, "%.0f");
							if (showRing)
								prmCh |= AdjRow("Anneaux", "vp.adj.ring", frg, 0.2f, "%.0f");
							float32 epA[3], erA[3], esA[3];
							demo::Demo3DHostEmptyTransform(st.addAdjustNode, epA, erA, esA);
							const float32 es0[3] = {esA[0], esA[1], esA[2]};
							if (showRay) {
								float32 rayA = esA[0];
								if (AdjRow(isTor ? "Rayon externe" : "Rayon", "vp.adj.ray",
										   rayA, 0.01f, "%.2f")) {
									esA[0] = rayA;
									esA[2] = rayA;
									if (isSph || isIco)
										esA[1] = rayA; // une sphere reste une sphere
								}
							}
							if (showHaut)
								AdjRow("Hauteur", "vp.adj.h", esA[1], 0.01f, "%.2f");
							if (showAux)
								prmCh |= AdjRow("Rayon interne", "vp.adj.aux", fax, 0.005f,
												"%.2f");
							if (showLHP) {
								AdjRow("Largeur", "vp.adj.lx", esA[0], 0.01f, "%.2f");
								if (!isPln)
									AdjRow("Hauteur", "vp.adj.ly", esA[1], 0.01f, "%.2f");
								AdjRow("Profondeur", "vp.adj.lz", esA[2], 0.01f, "%.2f");
							}
							if (ukA == 5) {
								// une LUMIERE aussi a ses proprietes DES la creation
								float32 ulc2[3], uli2 = 1.f;
								if (demo::Demo3DHostUserLightParams(st.addAdjustNode, ulc2,
																   &uli2)) {
									const float32 i0 = uli2;
									AdjRow("Puissance", "vp.adj.pw", uli2, 0.05f, "%.2f");
									if (uli2 != i0)
										demo::Demo3DHostSetUserLightParams(st.addAdjustNode,
																		   ulc2, uli2);
								}
								const float32 w0[5] = {rgP, inP, outP, awP, ahP};
								if (tyP != 0)
									AdjRow("Portee", "vp.adj.rg", rgP, 0.05f, "%.2f");
								if (tyP == 2) {
									AdjRow("Cone interne", "vp.adj.ci", inP, 0.2f, "%.1f");
									AdjRow("Cone externe", "vp.adj.co", outP, 0.2f, "%.1f");
								}
								if (tyP == 3) {
									AdjRow("Largeur", "vp.adj.aw", awP, 0.01f, "%.2f");
									AdjRow("Hauteur", "vp.adj.ah", ahP, 0.01f, "%.2f");
								}
								if (rgP != w0[0] || inP != w0[1] || outP != w0[2] ||
									awP != w0[3] || ahP != w0[4])
									demo::Demo3DHostSetLightEx(st.addAdjustNode, rgP, inP,
															   outP, awP, ahP, shP);
							}
							if (hasParams && (prmCh || (int32)(fsg + 0.5f) != sgA ||
											  (int32)(frg + 0.5f) != rgA))
								demo::Demo3DHostSetMeshParams(st.addAdjustNode,
															  (int32)(fsg + 0.5f),
															  (int32)(frg + 0.5f), fax);
							if (esA[0] != es0[0] || esA[1] != es0[1] || esA[2] != es0[2])
								demo::Demo3DHostSetEmptyTransform(st.addAdjustNode, epA, erA,
																  esA);
							const NkRect ab{aj.x + S(8.f), ay + S(2.f), pw - S(16.f),
											kRowH - S(4.f)};
							hit.Add("vp.adj.apply", ab);
							p.Fill(ab, NkRole::AccentUi, 3.f);
							p.TextV(ab.x + S(8.f), ay + S(2.f), kRowH - S(4.f), "Appliquer",
									NkRole::TextOnAccent);
							if (hit.Clicked("vp.adj.apply"))
								st.addAdjustNode = -1;
						}
					}
				}
			} else if (const char *e = demo::Demo3DHostError()) {
				// UN ECHEC SE DIT. Un viewport reste noir ne distingue pas Â« la carte
				// a refuse la cible Â» de Â« la scene est vide Â», et on cherche le
				// probleme du mauvais cote pendant une heure.
				char msg[128];
				snprintf(msg, sizeof(msg), "Vue 3D indisponible : %s", e);
				p.TextV(r.x + S(16.f), r.y, r.h, msg, NkRole::TextMuted);
			}

			// â”€â”€ ZONE DE LA SCENE â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
			// La DEMO PORTEE gere elle-meme orbite, molette, zones de selection,
			// curseur 3D et pick : cette zone ne sert plus qu'au SURVOL -- c'est lui
			// qui autorise ses raccourcis et sa souris (voir Demo3DHostSetView).
			{
				hit.Add("view.nav", vr);
			}

			// â”€â”€ GLISSEMENT DE NAVIGATION EN COURS (loupe, main, gizmo de nav) â”€â”€
			// Il se poursuit MEME SI la souris quitte le bouton : c'est le bouton
			// ENFONCE qui commande, pas la position.
			if (st.navDragMode >= 0) {
				if (!hit.MouseDown()) {
					st.navDragMode = -1;
				} else {
					const math::NkVec2 mm = hit.Mouse();
					const float32 ndx = mm.x - st.navDragLastX;
					const float32 ndy = mm.y - st.navDragLastY;
					if (st.navDragMode == 2)
						demo::Demo3DHostOrbit(ndx, ndy);
					else if (st.navDragMode == 1)
						demo::Demo3DHostPan(ndx, ndy);
					else
						// Tirer vers le HAUT rapproche : la meme convention que la
						// molette (0,02 cran par pixel).
						demo::Demo3DHostZoomWheel(-ndy * 0.02f);
					st.navDragLastX = mm.x;
					st.navDragLastY = mm.y;
					hit.WantCursor(st.navDragMode == 1 ? NkCursorWant::Hand
													   : NkCursorWant::ResizeNS);
				}
			}

			// â”€â”€ BARRE FLOTTANTE GAUCHE : ce qu'on REGARDE â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
			// Trois listes deroulantes, chacune avec son icone d'etat. Les menus de
			// commandes vivent dans la barre d'outils principale : les dupliquer ici
			// donnerait deux endroits a tenir a jour pour une seule liste.
			const float32 barH = S(26.f), barY = r.y + wsBarH + S(10.f);
			{
				int32 nP = 0, nS = 0, nO = 0;
				const char *const *proj = NkProjectionItems(nP);
				const char *const *shad = NkShadingItems(nS);
				const char *const *ovl = NkOverlayItems(nO);
				const float32 ib = S(28.f);
				// La COULEUR et le MATCAP ne concernent que les modes non eclaires
				// (Solide, Fil de fer) : c'est la que la demo les applique.
				const bool unlitZone = (st.shading == 1 || st.shading == 2);
				// LE COMPTE EXACT des boutons dimensionne le fond -- il etait a 4
				// pour 5 boutons de base, et le dernier (le matcap) flottait sans
				// fond : le defaut signale par Rihen.
				const int32 nBtn = 5 + (unlitZone ? 2 : 0);
				const float32 groupW = S(6.f) + (float32)nBtn * (ib + 2.f);
				float32 bx = r.x + S(10.f);
				p.Fill({bx, barY, groupW, barH}, NkRole::PanelBg, 5.f);
				bx += S(3.f);

				// MENU DE VUE : des ACTIONS (memoriser/rappeler la camera, reset,
				// panneaux). Pas un Combo -- un menu d'actions n'a pas de Â« valeur
				// courante Â» a afficher.
				{
					const NkRect br{bx, barY + 2.f, ib, barH - 4.f};
					const bool over = hit.Add("vp.menu", br);
					if (over || st.viewMenuOpen)
						p.Fill(br, st.viewMenuOpen ? NkRole::AccentUi : NkRole::PanelHeader, 3.f);
					p.IconV(br.x + (ib - S(14.f)) * 0.5f, br.y, br.h, NkIcon::Menu,
							st.viewMenuOpen ? NkRole::TextOnAccent : NkRole::Text, 14.f);
					p.Fill({br.x + br.w - S(6.f), br.y + br.h - S(6.f), S(3.f), S(3.f)},
						   st.viewMenuOpen ? NkRole::TextOnAccent : NkRole::Text);
					if (hit.Clicked("vp.menu")) {
						st.viewMenuOpen = !st.viewMenuOpen;
						st.bgMenuOpen = st.bgPickerOpen = st.matcapOpen = false;
					}
				}
				bx += ib + 2.f;
				p.VLine(bx - 1.f, barY + S(5.f), barH - S(10.f));

				Combo(p, hit, ws, "vp.proj", {bx, barY + 2.f, ib, barH - 4.f}, proj, NkProjectionIcons(),
					  nP, st.projection, combo, true, false, false);
				bx += ib + 2.f;
				Combo(p, hit, ws, "vp.shade", {bx, barY + 2.f, ib, barH - 4.f}, shad, NkShadingIcons(), nS,
					  st.shading, combo, true, false, false);
				bx += ib + 2.f;

				// â”€â”€ FOND DE LA VUE : le bouton EST un temoin de couleur â”€â”€â”€â”€â”€â”€â”€â”€â”€
				// Cinq prereglages + Â« Personnalisee Â» qui ouvre un PICKER (trois
				// barres R/V/B) -- demande de Rihen. Le temoin montre la couleur
				// REELLE : aucune icone ne dirait mieux l'etat.
				{
					const NkRect br{bx, barY + 2.f, ib, barH - 4.f};
					const bool over = hit.Add("vp.bg", br);
					if (over || st.bgMenuOpen)
						p.Fill(br, NkRole::PanelHeader, 3.f);
					float32 bc[3];
					NkBgColorOf(st, st.bgChoice, bc);
					p.Fill({br.x + S(5.f), br.y + S(4.f), br.w - S(10.f), br.h - S(8.f)},
						   NkColor{(uint8)(bc[0] * 255.f), (uint8)(bc[1] * 255.f),
								   (uint8)(bc[2] * 255.f), 255},
						   2.f);
					p.Fill({br.x + br.w - S(6.f), br.y + br.h - S(6.f), S(3.f), S(3.f)},
						   st.bgMenuOpen ? NkRole::TextOnAccent : NkRole::Text);
					if (hit.Clicked("vp.bg")) {
						st.bgMenuOpen = !st.bgMenuOpen;
						st.viewMenuOpen = st.matcapOpen = false;
						if (!st.bgMenuOpen)
							st.bgPickerOpen = false;
					}
					// Applique CHAQUE image : le moteur a une garde d'egalite, et
					// c'est ce qui rend le picker vivant pendant le glissement.
					// La LUMINOSITE (proprietes de la scene) module la couleur sans
					// en changer la teinte.
					float32 lum[3];
					for (int32 c2 = 0; c2 < 3; ++c2) {
						lum[c2] = bc[c2] * st.bgBrightness;
						if (lum[c2] > 1.f)
							lum[c2] = 1.f;
					}
					demo::Demo3DHostSetBackground(lum[0], lum[1], lum[2]);
				}
				bx += ib + 2.f;

				CheckCombo(p, hit, ws, "vp.ovl", {bx, barY + 2.f, ib, barH - 4.f}, ovl,
						   NkOverlayIcons(), nO, st.overlayMask, NkIcon::Eye, checks);
				bx += ib + 2.f;

				if (unlitZone) {
					// SOURCE DE COULEUR des modes non eclaires (la touche B de la
					// demo) : materiau, gris d'atelier, couleur choisie.
					int32 nL = 0;
					const char *const *lights = NkSolidLightItems(nL);
					Combo(p, hit, ws, "vp.solidlight", {bx, barY + 2.f, ib, barH - 4.f}, lights,
						  NkSolidLightIcons(), nL, st.solidLight, combo, true, false, false);
					bx += ib + 2.f;
					// MATCAP : bouton dedie -> panneau par CATEGORIES avec defilement
					// (34 boules ne tiennent pas dans une liste plate).
					{
						const NkRect br{bx, barY + 2.f, ib, barH - 4.f};
						const bool over = hit.Add("vp.matcap", br);
						if (over || st.matcapOpen)
							p.Fill(br, st.matcapOpen ? NkRole::AccentUi : NkRole::PanelHeader, 3.f);
						p.IconV(br.x + (ib - S(14.f)) * 0.5f, br.y, br.h, NkIcon::Matcap,
								st.matcapOpen ? NkRole::TextOnAccent : NkRole::Text, 14.f);
						p.Fill({br.x + br.w - S(6.f), br.y + br.h - S(6.f), S(3.f), S(3.f)},
							   st.matcapOpen ? NkRole::TextOnAccent : NkRole::Text);
						if (hit.Clicked("vp.matcap")) {
							st.matcapOpen = !st.matcapOpen;
							st.matcapAnchor = br; // le panneau s'ancre a SON bouton
							st.viewMenuOpen = st.bgMenuOpen = st.bgPickerOpen = false;
						}
					}
				}
			}

			// â”€â”€ BARRE FLOTTANTE DROITE : ce qu'on FAIT â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
			// TROIS GROUPES SEPARES PAR DU VIDE, et c'est ce que Rihen demandait :
			//   1. les sous-modes de selection (mode edition seulement) ;
			//   2. les outils -- Â« que fait mon clic ? Â», un seul actif ;
			//   3. les reglages : repere, vitesse de camera, aimantations.
			// Un seul bloc continu obligeait a compter les boutons pour retrouver le
			// sien. L'espace fait le tri sans qu'on ait a lire.
			const float32 btn = S(24.f);
			const float32 grp = S(10.f); // vide entre deux groupes
			// LES ANCIENS TROIS AIMANTS (grille/angle/echelle + leurs valeurs)
			// SONT RETIRES (regle de Rihen) : UN aimant-bascule, UNE cible --
			// et quand la cible est INCREMENT ou GRILLE, les PAS des trois
			// transformations deviennent des champs editables ici meme.
			// (les pas se reglent DANS le panneau d'aimantation, plus rien
			// dans la barre -- regle de Rihen)

			// Largeurs, calculees d'abord pour caler le tout a droite.
			const bool editMode2 = demo::Demo3DHostInEditMode();
			const float32 wSub = editMode2 ? (S(8.f) + 3.f * (btn + 2.f)) : 0.f;
			// OUTILS EN DEUX BLOCS dans le meme cadre : [Selection | Curseur] puis
			// un vide, puis [Deplacer | Rotation | Echelle | Multigizmo] -- la
			// disposition demandee par Rihen (celle de Blender).
			const float32 wTools = S(8.f) + 6.f * (btn + 2.f) + S(10.f);
			// Orientation, pivot, aimant + SON chevron, edition proportionnelle
			// + LE SIEN, vitesse. Sans compter les deux derniers, le groupe
			// restait trop etroit et ils tombaient HORS du cadre, a droite de
			// l'ecran : invisibles dans les deux modes (constate par Rihen).
			const float32 wSet = S(8.f) + 7.f * (btn + 2.f) + S(6.f);
			float32 tx = r.x + r.w - S(10.f) - wSet;

			// Groupe 3 : reglages (a droite).
			{
				p.Fill({tx, barY, wSet, barH}, NkRole::PanelBg, 5.f);
				float32 cx = tx + S(4.f);
				int32 nOr = 0, nCam = 0;
				const char *const *orients = NkOrientItems(nOr);
				const char *const *cams = NkCamSpeedItems(nCam);
				Combo(p, hit, ws, "vp.orient", {cx, barY + 1.f, btn, barH - 2.f}, orients, NkOrientIcons(),
					  nOr, st.orientation, combo, true, false, false);
				cx += btn + 2.f;
				// ── POINT DE PIVOT (Blender : « Transform Pivot Point », capture
				// de Rihen) : les cinq modes du gizmo, dans l'ordre du menu de
				// Blender. MEMOIRE-DU-POUSSE : le combo ecrit en fin de frame,
				// et la touche « . » peut changer le pivot cote moteur -- on
				// suit le moteur quand c'est lui qui a bouge, on pousse quand
				// c'est le combo.
				{
					static const char *const kPvItems[5] = {
						"Centre boite englobante", "Curseur 3D",
						"Origines individuelles", "Point median", "Element actif"};
					static const int32 kPvVal[5] = {1, 2, 3, 0, 4}; // menu -> moteur
					static const NkIcon kPvIc[5] = {NkIcon::Cube3D, NkIcon::Cursor,
													NkIcon::Layers, NkIcon::Dot,
													NkIcon::Check};
					const int32 engPv = demo::Demo3DHostPivotMode();
					int32 engMenu = 3;
					for (int32 i = 0; i < 5; ++i)
						if (kPvVal[i] == engPv)
							engMenu = i;
					static int32 sPvSel = 3;
					static int32 sPvEngPrev = -1;
					if (engMenu != sPvEngPrev) {
						sPvSel = engMenu; // le moteur a bouge (touche .) : on suit
						sPvEngPrev = engMenu;
					}
					Combo(p, hit, ws, "vp.pivot", {cx, barY + 1.f, btn, barH - 2.f},
						  kPvItems, kPvIc, 5, sPvSel, combo, true, false, false);
					if (sPvSel != engMenu) {
						demo::Demo3DHostSetPivotMode(kPvVal[sPvSel]);
						sPvEngPrev = sPvSel;
					}
					cx += btn + 2.f;
				}
				// ── L'AIMANT : LA bascule d'aimantation (Blender). Un clic
				// l'active, un re-clic la coupe -- et l'etat se VOIT (fond
				// accent), c'est ce qui manquait (Rihen : « je ne sais pas si
				// c'est active »). Verite moteur relue chaque image ; Ctrl
				// pendant le geste INVERSE, comme Blender.
				{
					const bool snapOn2 = demo::Demo3DHostSnapEnabled();
					const NkRect mb2{cx, barY + 2.f, btn, barH - 4.f};
					const bool ovM2 = hit.Add("vp.magnet", mb2);
					if (snapOn2)
						p.Fill(mb2, NkRole::AccentUi, 3.f);
					else
						HoverFill(p, mb2, ovM2, 3.f);
					// AIMANT EN FER A CHEVAL, pas un quadrillage : le quadrillage
					// disait « grille », or la bascule aimante aussi sur sommets,
					// aretes et faces. Le dessin doit decrire l'action (Rihen).
					p.IconV(cx + (btn - S(13.f)) * 0.5f, barY, barH, NkIcon::Magnet,
							snapOn2 ? NkRole::TextOnAccent : NkRole::TextMuted, 13.f);
					if (hit.Clicked("vp.magnet"))
						demo::Demo3DHostSetSnap(!snapOn2, st.snapStepT, st.snapStepR,
												st.snapStepS);
					cx += btn + 2.f;
				}
				// ── CIBLE D'AIMANTATION : un BOUTON-PANNEAU (Blender). Le
				// panneau porte la liste des cibles ET les pas d'increment --
				// c'est la qu'ils se reglent, pas dans la barre (Rihen).
				{
					const NkRect sb2{cx, barY + 1.f, btn, barH - 2.f};
					const bool ovS2 = hit.Add("vp.snapto", sb2);
					if (st.snapMenuOpen)
						p.Fill(sb2, NkRole::AccentUi, 3.f);
					else
						HoverFill(p, sb2, ovS2, 3.f);
					p.IconV(cx + (btn - S(13.f)) * 0.5f, barY, barH, NkIcon::ChevronDown,
							st.snapMenuOpen ? NkRole::TextOnAccent : NkRole::TextMuted,
							13.f);
					if (hit.Clicked("vp.snapto")) {
						st.snapMenuOpen = !st.snapMenuOpen;
						st.snapMenuAnchor = sb2;
					}
					cx += btn + S(6.f); // respiration avant la paire suivante
				}
				// ── EDITION PROPORTIONNELLE : sa bascule, puis SON chevron ───
				// Chaque bascule garde son chevron A COTE d'elle -- glissee entre
				// l'aimant et le sien, elle brouillait qui ouvre quoi (Rihen).
				// Elle vaut dans les DEUX modes : sommets voisins en Edition,
				// OBJETS voisins en mode Objet (disposer une foret, incurver une
				// rangee de batiments sans les toucher un a un).
				{
					bool peOn = false;
					float32 peR = 1.f;
					int32 peF = 0;
					demo::Demo3DHostPropEdit(&peOn, &peR, &peF);
					const NkRect pb{cx, barY + 2.f, btn, barH - 4.f};
					const bool ovP = hit.Add("vp.prop", pb);
					if (peOn)
						p.Fill(pb, NkRole::AccentUi, 3.f);
					else
						HoverFill(p, pb, ovP, 3.f);
					p.IconV(cx + (btn - S(13.f)) * 0.5f, barY, barH, NkIcon::Proportional,
							peOn ? NkRole::TextOnAccent : NkRole::TextMuted, 13.f);
					if (hit.Clicked("vp.prop"))
						demo::Demo3DHostSetPropEdit(!peOn, peR, peF);
					cx += btn + 2.f;
					const NkRect pm{cx, barY + 1.f, btn, barH - 2.f};
					const bool ovPM = hit.Add("vp.propmenu", pm);
					if (st.propMenuOpen)
						p.Fill(pm, NkRole::AccentUi, 3.f);
					else
						HoverFill(p, pm, ovPM, 3.f);
					p.IconV(cx + (btn - S(13.f)) * 0.5f, barY, barH, NkIcon::ChevronDown,
							st.propMenuOpen ? NkRole::TextOnAccent : NkRole::TextMuted, 13.f);
					if (hit.Clicked("vp.propmenu")) {
						st.propMenuOpen = !st.propMenuOpen;
						st.propMenuAnchor = pm;
					}
					cx += btn + 2.f;
				}
				// VITESSE DE CAMERA : son icone etait la camera -- le meme dessin
				// que la projection perspective ET que le bouton de vol. Un dessin
				// dedie, sinon la barre a trois boutons jumeaux.
				static const NkIcon kCamIc[4] = {NkIcon::Speed, NkIcon::Speed, NkIcon::Speed,
												 NkIcon::Speed};
				Combo(p, hit, ws, "vp.cam", {cx, barY + 1.f, btn, barH - 2.f}, cams, kCamIc, nCam,
					  st.camSpeed, combo, true, false, false);
				cx += btn + S(8.f);
				p.VLine(cx - S(4.f), barY + S(6.f), barH - S(12.f));

				// (les pas vivent dans le panneau d'aimantation ci-dessous)
			}
			// ── PANNEAU DE L'EDITION PROPORTIONNELLE : rayon + attenuation ──
			// Meme facture que celui de l'aimantation : ancre a son chevron,
			// bloquant, et ferme au clic exterieur -- l'emprise du bouton
			// exclue, sinon le clic d'ouverture le refermerait dans la meme
			// image (le piege deja paye deux fois).
			if (st.propMenuOpen) {
				bool peOn = false;
				float32 peR = 1.f;
				int32 peF = 0;
				demo::Demo3DHostPropEdit(&peOn, &peR, &peF);
				const float32 r0p = peR;
				const int32 f0p = peF;
				static const char *const kFall[8] = {"Lisse",		 "Sphere",	 "Racine",
													 "Carre inverse", "Net",	 "Lineaire",
													 "Constant",	  "Aleatoire"};
				const float32 rowH3 = S(22.f);
				const float32 pw3 = S(206.f);
				const float32 ph3 = S(10.f) + rowH3 * 10.f;
				float32 px3 = st.propMenuAnchor.x + st.propMenuAnchor.w - pw3;
				if (px3 < r.x + S(4.f))
					px3 = r.x + S(4.f);
				const NkRect pr3{px3, st.propMenuAnchor.y + st.propMenuAnchor.h + S(4.f), pw3,
								 ph3};
				hit.Add("vp.propmenu.box", pr3);
				p.Fill(pr3, NkRole::PanelBg, 4.f);
				p.OutlineSharp(pr3, NkRole::Border);
				float32 yy3 = pr3.y + S(4.f);
				p.TextV(px3 + S(10.f), yy3, rowH3, "Rayon", NkRole::TextMuted);
				DragFloat(p, hit, ws, in, "vp.prop.rad",
						  {px3 + S(84.f), yy3 + S(3.f), pw3 - S(94.f), rowH3 - S(6.f)}, peR,
						  0.02f, NkRole::AccentUi, "%.2f m");
				yy3 += rowH3;
				p.TextV(px3 + S(10.f), yy3, rowH3, "Attenuation", NkRole::TextMuted);
				yy3 += rowH3;
				char fk3[24];
				for (int32 i3 = 0; i3 < 8; ++i3) {
					snprintf(fk3, sizeof(fk3), "vp.prop.f%d", i3);
					const NkRect ir3{px3 + S(4.f), yy3, pw3 - S(8.f), rowH3};
					const bool ov3 = hit.Add(fk3, ir3);
					const bool on3 = (peF == i3);
					if (on3)
						p.Fill(ir3, NkRole::AccentUi, 3.f);
					else if (ov3)
						p.Fill(ir3, NkRole::PanelHeader, 3.f);
					p.TextV(ir3.x + S(10.f), yy3, rowH3, kFall[i3],
							on3 ? NkRole::TextOnAccent : NkRole::Text);
					if (hit.Clicked(fk3))
						peF = i3;
					yy3 += rowH3;
				}
				if (peR != r0p || peF != f0p)
					demo::Demo3DHostSetPropEdit(peOn, peR, peF);
				hit.SetBlock(pr3, true);
				if (hit.AnyClick() && !NkHitRegistry::Contains(pr3, in.mousePos) &&
					!NkHitRegistry::Contains(st.propMenuAnchor, in.mousePos))
					st.propMenuOpen = false;
			}
			// ── PANNEAU D'AIMANTATION : cibles + pas (Blender, capture de
			// Rihen). Il s'ancre a son bouton, BLOQUE les evenements sous lui
			// (SetBlock, meme patron que le badge vue camera) et se referme au
			// clic exterieur.
			if (st.snapMenuOpen) {
				static const char *const kSnItems[9] = {
					"Increment",	  "Grille",
					"Sommet",		  "Arete",
					"Face",			  "Volume",
					"Centre d'arete", "Arete perpendiculaire",
					"Centre de face"};
				const int32 curTgt = demo::Demo3DHostSnapTarget();
				const bool showSteps = curTgt <= 1;
				// La BASE d'accroche n'a de sens que pour les cibles
				// geometriques : sur Increment et Grille, c'est le pas qui
				// decide, pas un point de l'objet.
				const bool showBase = curTgt >= 2;
				const float32 rowH2 = S(22.f);
				const float32 pw = S(226.f);
				const float32 ph = S(8.f) + rowH2 * (1.f + 9.f) +
								   (showSteps ? S(6.f) + rowH2 * 3.f : 0.f) +
								   (showBase ? S(6.f) + rowH2 * 5.f : 0.f) + S(6.f);
				float32 px = st.snapMenuAnchor.x + st.snapMenuAnchor.w - pw;
				if (px < r.x + S(4.f))
					px = r.x + S(4.f);
				const float32 py = st.snapMenuAnchor.y + st.snapMenuAnchor.h + S(4.f);
				const NkRect pr{px, py, pw, ph};
				// PAS de SetBlock ICI : arme avant nos propres lignes, il
				// bloquait leurs clics (Clicked() refuse tout clic dans
				// l'emprise) -- « je n'arrive pas a selectionner », Rihen. Le
				// bloc s'arme EN FIN de panneau, pour la scene en dessous ; le
				// reset de debut de frame l'a deja leve pour nous.
				hit.Add("vp.snapmenu", pr);
				p.Fill(pr, NkRole::PanelBg, 4.f);
				p.OutlineSharp(pr, NkRole::Border);
				float32 yy2 = py + S(4.f);
				p.TextV(px + S(10.f), yy2, rowH2, "Aimanter sur", NkRole::TextMuted);
				yy2 += rowH2;
				char sk2[24];
				for (int32 i9 = 0; i9 < 9; ++i9) {
					snprintf(sk2, sizeof(sk2), "vp.snapto.%d", i9);
					const NkRect ir9{px + S(4.f), yy2, pw - S(8.f), rowH2};
					const bool ov9 = hit.Add(sk2, ir9);
					const bool on9 = curTgt == i9;
					// Volume et Arete perpendiculaire sont livres : plus de stub.
					const bool stub9 = false;
					if (on9)
						p.Fill(ir9, NkRole::AccentUi, 3.f);
					else if (ov9 && !stub9)
						p.Fill(ir9, NkRole::PanelHeader, 3.f);
					p.TextV(ir9.x + S(8.f), yy2, rowH2, kSnItems[i9],
							stub9 ? NkRole::TextMuted
								  : (on9 ? NkRole::TextOnAccent : NkRole::Text));
					if (hit.Clicked(sk2) && !stub9)
						demo::Demo3DHostSetSnapTarget(i9);
					yy2 += rowH2;
				}
				if (showBase) {
					p.HLine(px + S(6.f), yy2 + S(2.f), pw - S(12.f));
					yy2 += S(6.f);
					p.TextV(px + S(10.f), yy2, rowH2, "Base d'accroche", NkRole::TextMuted);
					yy2 += rowH2;
					// « Centre » et « Median » de Blender fusionnent en
					// « Pivot » : le pivot du gizmo suit deja le mode de pivot
					// de l'application -- deux entrees pour un meme point
					// seraient un faux choix.
					static const char *const kSb[3] = {"Le plus proche", "Pivot",
													   "Objet actif"};
					const int32 curBase = demo::Demo3DHostSnapBase();
					for (int32 b9 = 0; b9 < 3; ++b9) {
						snprintf(sk2, sizeof(sk2), "vp.snapbase.%d", b9);
						const NkRect ir9{px + S(4.f), yy2, pw - S(8.f), rowH2};
						const bool ov9 = hit.Add(sk2, ir9);
						const bool on9 = curBase == b9;
						if (on9)
							p.Fill(ir9, NkRole::AccentUi, 3.f);
						else if (ov9)
							p.Fill(ir9, NkRole::PanelHeader, 3.f);
						p.TextV(ir9.x + S(8.f), yy2, rowH2, kSb[b9],
								on9 ? NkRole::TextOnAccent : NkRole::Text);
						if (hit.Clicked(sk2))
							demo::Demo3DHostSetSnapBase(b9);
						yy2 += rowH2;
					}
					// ALIGNER LA ROTATION SUR LA CIBLE : n'agit que quand une
					// FACE (ou un centre de face) accroche -- les autres cibles
					// n'ont pas de normale stable.
					{
						const bool al = demo::Demo3DHostSnapAlignRot();
						const NkRect cb{px + S(8.f), yy2 + S(4.f), rowH2 - S(8.f),
										rowH2 - S(8.f)};
						const bool ova = hit.Add("vp.snapalign", cb);
						if (al)
							p.Fill(cb, NkRole::AccentUi, 3.f);
						else
							p.Outline(cb, NkRole::Border,
									  ova ? NkRole::PanelHeader : NkRole::PanelBg, 3.f);
						if (al)
							p.IconV(cb.x + S(1.f), yy2, rowH2, NkIcon::Check,
									NkRole::TextOnAccent, 11.f);
						p.TextV(cb.x + cb.w + S(8.f), yy2, rowH2, "Aligner la rotation",
								NkRole::Text);
						if (hit.Clicked("vp.snapalign"))
							demo::Demo3DHostSetSnapAlignRot(!al);
						yy2 += rowH2;
					}
				}
				if (showSteps) {
					p.HLine(px + S(6.f), yy2 + S(2.f), pw - S(12.f));
					yy2 += S(6.f);
					static const char *const kSt2[3] = {"Pas deplacement", "Pas angle",
														"Pas echelle"};
					float32 *const sv2[3] = {&st.snapStepT, &st.snapStepR, &st.snapStepS};
					static const char *const kSf2[3] = {"%.2f", "%.0f", "%.2f"};
					bool ch2 = false;
					for (int32 i9 = 0; i9 < 3; ++i9) {
						p.TextV(px + S(10.f), yy2, rowH2, kSt2[i9], NkRole::TextMuted);
						snprintf(sk2, sizeof(sk2), "vp.snapstep.%d", i9);
						ch2 |= DragFloat(p, hit, ws, in, sk2,
										 {px + S(120.f), yy2 + S(3.f), pw - S(130.f),
										  rowH2 - S(6.f)},
										 *sv2[i9], i9 == 1 ? 0.5f : 0.01f,
										 NkRole::AccentUi, kSf2[i9]);
						yy2 += rowH2;
					}
					if (ch2) {
						if (st.snapStepT < 0.01f)
							st.snapStepT = 0.01f;
						if (st.snapStepR < 1.f)
							st.snapStepR = 1.f;
						if (st.snapStepS < 0.01f)
							st.snapStepS = 0.01f;
						demo::Demo3DHostSetSnap(demo::Demo3DHostSnapEnabled(),
												st.snapStepT, st.snapStepR,
												st.snapStepS);
					}
				}
				// Le bloc s'arme APRES nos widgets : il ne vaut que pour la
				// scene et les zones peintes avant nous.
				hit.SetBlock(pr, true);
				// Un clic HORS de l'EMPRISE du panneau et de son bouton
				// referme -- l'emprise geometrique, pas le survol de zone : au
				// clic sur une ligne, la ligne est la zone du dessus et le
				// panneau n'etait « pas survole ».
				if (hit.AnyClick() && !NkHitRegistry::Contains(pr, in.mousePos) &&
					!hit.IsHovered("vp.snapto"))
					st.snapMenuOpen = false;
			}

			// Groupe 2 : outils -- [Selection | Curseur]  [Deplacer | Rotation |
			// Echelle | Multigizmo].
			tx -= grp + wTools;
			{
				p.Fill({tx, barY, wTools, barH}, NkRole::PanelBg, 5.f);
				float32 cx = tx + S(4.f);

				// SELECTION : une liste de formes (rectangle / cercle / lasso).
				{
					const NkRect br{cx, barY + 2.f, btn, barH - 4.f};
					const bool on = (st.tool == NkTool::Select);
					int32 nS2 = 0;
					const char *const *shapes = NkSelShapeItems(nS2);
					if (on)
						p.Fill(br, NkRole::AccentUi, 3.f);
					Combo(p, hit, ws, "vp.selshape", br, shapes, NkSelShapeIcons(), nS2,
						  st.selShape, combo, true, false, false);
					if (hit.Clicked("vp.selshape"))
						st.tool = NkTool::Select; // choisir une forme active l'outil
					cx += btn + 2.f;
				}
				// CURSEUR : le clic gauche pose le curseur 3D de la demo.
				{
					const NkRect br{cx, barY + 2.f, btn, barH - 4.f};
					const bool over = hit.Add("vp.t.cursor", br);
					const bool on = (st.tool == NkTool::Cursor);
					if (on)
						p.Fill(br, NkRole::AccentUi, 3.f);
					else
						HoverFill(p, br, over);
					if (hit.Clicked("vp.t.cursor"))
						st.tool = NkTool::Cursor;
					p.IconV(cx + (btn - S(14.f)) * 0.5f, barY, barH, NkIcon::Cursor,
							on ? NkRole::TextOnAccent : NkRole::Text, 14.f);
					cx += btn + 2.f;
				}
				// L'ESPACE entre les deux blocs : selection et transformation sont
				// deux familles de gestes, le vide fait le tri sans qu'on lise.
				cx += S(10.f);
				p.VLine(cx - S(5.f), barY + S(6.f), barH - S(12.f));

				struct TB {
						NkIcon ic;
						NkTool tool;
						const char *key;
				};
				static const TB kXf[4] = {
					{NkIcon::Move, NkTool::Move, "vp.t.move"},
					{NkIcon::Rotate, NkTool::Rotate, "vp.t.rot"},
					{NkIcon::Scale, NkTool::Scale, "vp.t.scale"},
					// MULTIGIZMO = le mode COMBINE de la demo : T+R+S en un seul
					// gizmo (sa touche C). Il manquait a la barre.
					{NkIcon::Gizmo, NkTool::MultiGizmo, "vp.t.multi"},
				};
				for (int32 i = 0; i < 4; ++i) {
					const NkRect br{cx, barY + 2.f, btn, barH - 4.f};
					const bool over = hit.Add(kXf[i].key, br);
					const bool on = (st.tool == kXf[i].tool);
					if (on)
						p.Fill(br, NkRole::AccentUi, 3.f);
					else
						HoverFill(p, br, over);
					if (hit.Clicked(kXf[i].key))
						st.tool = kXf[i].tool;
					p.IconV(cx + (btn - S(14.f)) * 0.5f, barY, barH, kXf[i].ic,
							on ? NkRole::TextOnAccent : NkRole::Text, 14.f);
					cx += btn + 2.f;
				}
			}

			// Groupe 1 : sous-modes V/E/F, en mode edition seulement -- cables sur
			// le masque de selection REEL de la demo (ses touches 1/2/3).
			if (editMode2) {
				tx -= grp + wSub;
				p.Fill({tx, barY, wSub, barH}, NkRole::PanelBg, 5.f);
				float32 cx = tx + S(4.f);
				const NkIcon kSub[3] = {NkIcon::Dot, NkIcon::Ruler, NkIcon::Square};
				static const char *const kKeys[3] = {"vp.sub.0", "vp.sub.1", "vp.sub.2"};
				const int32 mask = demo::Demo3DHostEditSelMask();
				for (int32 i = 0; i < 3; ++i) {
					const NkRect br{cx, barY + 2.f, btn, barH - 4.f};
					const bool over = hit.Add(kKeys[i], br);
					const bool on = (mask & (1 << i)) != 0;
					if (on)
						p.Fill(br, NkRole::AccentUi, 3.f);
					else
						HoverFill(p, br, over);
					if (hit.Clicked(kKeys[i])) {
						// Maj+clic COMBINE les modes (comme Maj+1/2/3 dans la demo),
						// le clic simple remplace.
						const int32 nm = hit.ShiftDown() ? (mask ^ (1 << i)) : (1 << i);
						demo::Demo3DHostSetEditSelMask(nm);
						st.subMode = (NkSubMode)i;
					}
					p.IconV(cx + (btn - S(14.f)) * 0.5f, barY, barH, kSub[i],
							on ? NkRole::TextOnAccent : NkRole::Text, 14.f);
					cx += btn + 2.f;
				}
			}

			// â”€â”€ GIZMO DE NAVIGATION + BOUTONS DE VUE, en bas a gauche.
			// Chez Blender ils sont en haut a droite ; ici la place y est prise par
			// les outils, et Rihen a demande de garder le coin bas-gauche.
			// Les boutons de navigation sont AU-DESSUS du gizmo, comme chez Blender :
			// on descend du plus abstrait (l'orientation) vers le plus direct (zoom,
			// deplacement). Les mettre dessous les collait au bord de la fenetre.
			const float32 gz = 34.f;
			// La colonne compte SES rangs : capture + 4 boutons de navigation,
			// et le CADENAS d'orbite s'ajoute en vue camera -- avec la hauteur
			// figee a 5, la rangee en plus chevauchait la boule (Rihen).
			const float32 navRows = demo::Demo3DHostCameraView() >= 0 ? 6.f : 5.f;
			const float32 navH = navRows * 26.f + (navRows - 1.f) * 6.f;
			const float32 navY = r.y + r.h - 22.f - gz * 2.f - 14.f - navH;
			PaintViewButtons(p, hit, st, r.x + 14.f, navY);
			PaintNavGizmo(p, hit, st, r.x + 12.f + gz, r.y + r.h - 22.f - gz, gz);

			// â”€â”€ PANNEAU DE DERNIERE OPERATION. Il FLOTTE au-dessus de la scene et n'est
			// pas encastre dans un bord : il appartient a la vue, pas au cadre.
			if (editMode) {
				const float32 pw = 214.f, ph = 4.f * kRowH + 6.f;
				const float32 px = r.x + 12.f, py = r.y + r.h - ph - 80.f;
				p.Fill({px, py, pw, ph}, NkRole::PanelHeader, 4.f);
				p.IconV(px + 6.f, py, kRowH, NkIcon::ChevronDown, NkRole::Text, 11.f);
				p.TextV(px + 22.f, py, kRowH, "Extruder la region");
				float32 ry = py + kRowH;
				static const char *const kL[] = {"Distance", "Decalage"};
				static const char *const kV[] = {"0,25", "0,00"};
				for (int32 i = 0; i < 2; ++i) {
					p.TextV(px + kPad, ry, kRowH, kL[i], NkRole::TextMuted);
					p.Fill({px + 112.f, ry + 3.f, 92.f, 16.f}, NkRole::InputBg, 2.f);
					p.TextV(px + 118.f, ry, kRowH, kV[i]);
					ry += kRowH;
				}
				p.Fill({px + kPad, ry + 5.f, 12.f, 12.f}, NkRole::AccentUi, 2.f);
				p.TextV(px + kPad + 18.f, ry, kRowH, "Decalage pair", NkRole::TextMuted);
			}

			// Le raccourci de l'operation courante est LU dans la table via sa CLE DE
			// COMMANDE, jamais recopie : rebinder la touche changera cet affichage tout
			// seul. La cle est stable, l'index ne l'est pas.
			{
				const char *cmd = editMode ? "edit.extruder" : "objet.deplacer";
				char keys[32];
				if (sc.FormatFor(cmd, keys, sizeof(keys))) {
					char hint[96];
					snprintf(hint, sizeof(hint), "%s   %s", editMode ? "Extruder" : "Deplacer", keys);
					p.TextV(r.x + 12.f, r.y + r.h - 24.f, 20.f, hint, NkRole::TextMuted);
				}
			}

			// â”€â”€ POPUPS DE LA VUE : peints en DERNIER, par-dessus les barres â”€â”€â”€â”€
			PaintViewMenuPopup(p, hit, st, r, barY, barH);
			PaintBgPopup(p, hit, st, r, barY, barH);
			// Le panneau des matcaps est peint depuis main, APRES tous les
			// panneaux : il peut s'ouvrir depuis le panneau Proprietes aussi.

			// ── MENU CONTEXTUEL DU MAILLAGE (clic droit) ────────────────────
			// Composant du KIT, pas une reecriture : `NkCtxMenuDraw` porte deja
			// le grisage, le defilement, la recherche et -- depuis aujourd'hui --
			// la colonne de RACCOURCIS alignee a droite.
			//
			// ⚠ LE CLIC DROIT EST DEJA PRIS, DEUX FOIS, dans le viseur : il ANNULE
			// une operation modale, et Maj+clic droit place le curseur 3D. Ouvrir
			// le menu sans le savoir aurait fait DEUX choses d'un seul clic. On ne
			// s'ouvre donc que sur un clic droit NU, hors modale.
			if (guiCtx && editMode && demo::Demo3DHostReady()) {
				const bool dansVue = st.viewRect.w > 0.f && in.mousePos.x >= st.viewRect.x &&
									 in.mousePos.x < st.viewRect.x + st.viewRect.w &&
									 in.mousePos.y >= st.viewRect.y &&
									 in.mousePos.y < st.viewRect.y + st.viewRect.h;
				if (dansVue && in.mouseClicked[1] && !in.shiftDown && !demo::Demo3DHostModalActive() &&
					!st.meshMenu.open) {
					st.meshMenu.open = true;
					st.meshMenu.pos = in.mousePos;
				}
				if (st.meshMenu.open) {
					const char *labels[kMeshMenuCap];
					const char *shorts[kMeshMenuCap];
					bool enabled[kMeshMenuCap];
					NkMeshCmd ids[kMeshMenuCap];
					char keybuf[kMeshMenuCap][32];
					const int32 n =
						NkMeshMenuBuild(demo::Demo3DHostEditSelMask(), demo::Demo3DHostEditSelCount(), sc,
										labels, shorts, enabled, ids, keybuf);
					const int32 choisi = editorkit::NkCtxMenuDraw(*guiCtx, st.meshMenu, labels, enabled, n,
																 nullptr, nullptr, nullptr, nullptr, 0,
																 nullptr, shorts);
					// UNE COMMANDE, PLUSIEURS ENTREES : on ne fait ici que NOMMER
					// la commande. Ce qu'elle fait vit dans le repartiteur, partage
					// avec la barre de menu et le clavier.
					if (choisi >= 0 && choisi < n)
						(void)NkMeshMenuRun(ids[choisi]);
				}
			} else if (st.meshMenu.open) {
				st.meshMenu.open = false; // sortie du mode edition : le menu ne survit pas
			}
		}

		// â”€â”€ LIGNE DE TRANSFORMATION â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
		// TROIS CADRES DE LARGEUR IDENTIQUE, puis DEUX COLONNES CARREES pour les
		// icones. La largeur egale n'est pas cosmetique : trois champs de tailles
		// differentes se lisent comme trois choses differentes, alors que X, Y et Z
		// sont la meme grandeur sur trois axes.
		//
		// Les deux colonnes d'icones sont CARREES et RESERVEES meme quand la ligne
		// n'a qu'une icone : sans reservation, les champs de Â« Rotation Â» seraient
		// plus larges que ceux de Â« Position Â», et les trois lignes ne s'aligneraient
		// plus verticalement.
		//
		// LES VALEURS SE MODIFIENT EN GLISSANT, comme dans Blender et Unreal. C'est le
		// geste le plus utilise d'un modeleur : bien plus souvent qu'on ne tape un
		// nombre, on veut Â« un peu plus, un peu moins Â» en regardant le resultat.

	} // namespace nk3d
} // namespace nkentseu
