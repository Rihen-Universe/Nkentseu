#pragma once
// -----------------------------------------------------------------------------
// @File    NkModelerChrome.h
// @Brief   Le CADRE de l'application, autour des panneaux : separateurs
//          glissables, boites de dialogue de fermeture / d'encodage, et barre
//          d'etat.
//
//          Ces surfaces ne dependent d'aucun panneau : elles n'ont besoin que de
//          la disposition et de l'etat. Extrait de NkModelerScreens.h au premier
//          lot de la refonte -- « subdiviser les gros fichiers » (Rihen,
//          13 aout 2026).
// @Author  Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
// -----------------------------------------------------------------------------
#include "NK3DModeler/Shell/NkModelerUI.h"
#include "NK3DModeler/Shell/NkModelerInput.h"
#include "NK3DModeler/Shell/NkModelerWidgets.h"
#include "NK3DModeler/Shell/NkModelerTables.h"
#include "NK3DModeler/Shell/NkModelerCommon.h"

namespace nkentseu {
	namespace nk3d {

		// â”€â”€ SEPARATEURS GLISSABLES â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
		// Ils modifient des FRACTIONS et non des pixels : la disposition se retrouve
		// identique a la reouverture quelle que soit la taille de fenetre.
		//
		// La zone sensible est PLUS LARGE que le trait dessine (6 px contre 1) :
		// viser un trait d'un pixel est un exercice d'adresse, pas une interaction.
		// C'est ce que font tous les gestionnaires de fenetres.
		inline void PaintSplitters(NkModelerPainter &p, const NkLayout &lay, float32 W, float32 H,
								   NkModelerState &st, NkHitRegistry &hit) {
			const float32 grab = S(6.f);
			struct Sp {
					const char *key;
					NkRect zone;
					bool vertical; ///< true = trait vertical, on glisse en X
					float32 *frac;
					float32 span; ///< dimension de reference pour convertir px -> fraction
					float32 sign; ///< sens : +1 si la fraction croit avec la position
			};
			const Sp sps[4] = {
				{"split.left", {lay.left.x + lay.left.w - grab * 0.5f, lay.left.y, grab, lay.left.h},
				 true, &st.leftFrac, W, 1.f},
				{"split.right", {lay.right.x - grab * 0.5f, lay.right.y, grab, lay.right.h}, true,
				 &st.rightFrac, W, -1.f},
				{"split.browser", {0.f, lay.browser.y - grab * 0.5f, W, grab}, false, &st.browserFrac, H,
				 -1.f},
				{"split.props",
				 {lay.propsR.x, lay.propsR.y + lay.propsR.h - grab * 0.5f, lay.propsR.w, grab}, false,
				 &st.propsFrac, lay.right.h, 1.f},
			};

			// Un separateur borde un panneau : si le panneau est masque, il n'y a rien
			// a redimensionner. Le laisser actif donnerait un trait invisible qui
			// change le curseur et modifie une fraction qu'on ne voit pas.
			// Le splitter vue|proprietes MEURT quand le panneau est replie sur
			// ses pastilles (sa zone restait dans l'espace rendu a la vue --
			// le « cote gauche fantome » constate par Rihen). Et le separateur
			// interne Proprietes/Details est OBSOLETE depuis le panneau unique :
			// c'etait la « tete » invisible des sous-blocs.
			const bool alive[4] = {st.showLeft, st.showRight && st.AnyPropOpen(),
								   st.showBrowser, false};
			for (int32 i = 0; i < 4; ++i) {
				if (!alive[i])
					continue;
				const bool over = hit.Add(sps[i].key, sps[i].zone);
				const bool active = (st.dragSplitter == i);
				if (over || active) {
					hit.WantCursor(sps[i].vertical ? NkCursorWant::ResizeWE : NkCursorWant::ResizeNS);
					// Le trait s'ECLAIRE au survol : sans retour visuel, on ne sait pas
					// qu'on a attrape le bon endroit avant d'avoir deja tire.
					p.Fill(sps[i].zone, active ? NkRole::AccentUi : NkRole::Border);
				}
				if (hit.Clicked(sps[i].key)) {
					st.dragSplitter = i;
					st.dragStart = sps[i].vertical ? hit.Mouse().x : hit.Mouse().y;
					st.dragStartFrac = *sps[i].frac;
				}
			}

			// Le glissement se poursuit MEME SI la souris quitte la zone : c'est le
			// bouton enfonce qui commande, pas la position. Sans cela, un glissement
			// rapide lacherait le separateur en pleine course.
			if (st.dragSplitter >= 0) {
				if (!hit.MouseDown()) {
					st.dragSplitter = -1;
				} else {
					const Sp &sp = sps[st.dragSplitter];
					const float32 now = sp.vertical ? hit.Mouse().x : hit.Mouse().y;
					float32 f = st.dragStartFrac + sp.sign * (now - st.dragStart) / sp.span;
					if (f < 0.08f)
						f = 0.08f;
					if (f > 0.60f)
						f = 0.60f;
					*sp.frac = f;
				}
			}
		}

		// â”€â”€ CONFIRMATION DE FERMETURE â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
		// N'apparait QUE si le document a des modifications non enregistrees.
		// Demander confirmation pour un document propre serait une friction inutile,
		// et l'utilisateur finirait par valider sans lire -- ce qui rend la question
		// dangereuse le jour ou elle compte vraiment.
		// ── DEMANDE DE FERMETURE : UNE SEULE POLITIQUE ─────────────────────────────
		// Tous les chemins (croix dessinee, menu Quitter, croix de l'OS) passent
		// ici. Une prise ou un encodage en cours PRIME sur le document modifie :
		// perdre une video de plusieurs minutes est pire que perdre un clic.
		inline void NkRequestClose(NkModelerState &st) {
			const bool busy = demo::Demo3DHostRecActive() || demo::Demo3DHostRecTutoActive() ||
							  demo::Demo3DHostRecEncoding() || demo::Demo3DHostRecTutoEncoding();
			if (busy)
				st.askCloseRec = true;
			// AUCUN PROJET OUVERT (ecran d'accueil) : il n'y a rien a perdre, donc
			// rien a demander. Poser la question « modifications non enregistrees »
			// devant un ecran de demarrage serait incomprehensible.
			// LA CONFIRMATION EST TOUJOURS POSEE quand un projet est ouvert (Rihen) :
			// fermer un modeleur d'un clic mal place est un accident cher, meme
			// quand tout est enregistre -- il faut rouvrir le projet, refaire sa
			// disposition, retrouver sa vue. Seul l'ECRAN D'ACCUEIL sort sans
			// demander : il n'y a alors ni travail ni disposition a perdre, et la
			// question y serait incomprehensible.
			else if (!st.welcome)
				st.askClose = true;
			else
				st.running = false;
		}

		// ── FERMER PENDANT UNE PRISE OU UN ENCODAGE ────────────────────────────────
		// Trois issues (Rihen) : abandonner, finir puis fermer, ou continuer en
		// arriere-plan -- la fenetre se cache, le processus vit jusqu'a la fin.
		inline void PaintCloseRecDialog(NkModelerPainter &p, float32 W, float32 H,
										NkModelerState &st, NkHitRegistry &hit) {
			if (!st.askCloseRec)
				return;
			p.Fill({0.f, 0.f, W, H}, NkColor{0, 0, 0, 150}); // voile
			hit.Add("dlgr.veil", {0.f, 0.f, W, H});

			const bool recording = demo::Demo3DHostRecActive() || demo::Demo3DHostRecTutoActive();
			// QUATRE actions ne tiennent pas sur une rangee : empilees pleine
			// largeur, elles ne peuvent PAS deborder du cadre, et quatre choix
			// se lisent mieux en liste qu'en file (constate par Rihen : les
			// boutons sortaient de la boite).
			const float32 bw = S(470.f), bh = S(238.f);
			const NkRect box{(W - bw) * 0.5f, (H - bh) * 0.5f, bw, bh};
			p.Outline(box, NkRole::Border, NkRole::PanelHeader, 6.f);
			hit.Add("dlgr.box", box);
			p.TextV(box.x + S(20.f), box.y + S(14.f), S(24.f),
					recording ? "Un enregistrement est en cours" : "Un encodage est en cours");
			p.TextV(box.x + S(20.f), box.y + S(44.f), S(22.f),
					recording ? "Quitter maintenant perdrait la prise."
							  : "Quitter maintenant laisserait la video inachevee.",
					NkRole::TextMuted);

			struct B {
					const char *key;
					const char *label;
					bool primary;
			};
			// L'action la plus SURE porte l'accent et vient en PREMIER : finir
			// le travail. Abandonner reste atteignable mais ne se propose pas.
			const B kBtns[4] = {{"dlgr.finish", "Terminer puis quitter", true},
								{"dlgr.daemon", "Continuer en arriere-plan", false},
								{"dlgr.drop", "Abandonner et quitter", false},
								{"dlgr.cancel", "Annuler", false}};
			float32 by = box.y + S(76.f);
			for (int32 i = 0; i < 4; ++i) {
				const NkRect br{box.x + S(20.f), by, bw - S(40.f), S(28.f)};
				const bool over = hit.Add(kBtns[i].key, br);
				if (kBtns[i].primary)
					p.Fill(br, over ? NkRole::AccentSel : NkRole::AccentUi, 4.f);
				else
					p.Outline(br, NkRole::Border, over ? NkRole::PanelBg : NkRole::PanelHeader, 4.f);
				const float32 lw = p.TextW(kBtns[i].label);
				p.TextV(br.x + (br.w - lw) * 0.5f, br.y, br.h, kBtns[i].label,
						kBtns[i].primary ? NkRole::TextOnAccent : NkRole::Text);
				by += S(36.f);
			}

			if (hit.Clicked("dlgr.cancel"))
				st.askCloseRec = false;
			if (hit.Clicked("dlgr.drop")) {
				// Les prises s'abandonnent proprement ; un encodage en cours est
				// simplement laisse : il n'efface son dossier QOI qu'une fois
				// complet, donc rien n'est perdu sur disque.
				if (demo::Demo3DHostRecActive())
					demo::Demo3DHostRecStop(false);
				if (demo::Demo3DHostRecTutoActive())
					demo::Demo3DHostRecTutoStop(false);
				st.running = false;
			}
			const bool finish = hit.Clicked("dlgr.finish");
			const bool daemon = hit.Clicked("dlgr.daemon");
			if (finish || daemon) {
				// On GARDE les prises : leur arret declenche l'encodage, et la
				// boucle fermera l'application quand tout sera fini.
				if (demo::Demo3DHostRecActive())
					demo::Demo3DHostRecStop(true);
				if (demo::Demo3DHostRecTutoActive())
					demo::Demo3DHostRecTutoStop(true);
				st.closeAfterEncode = daemon ? 2 : 1;
				if (daemon)
					st.wantHideWindow = true;
				st.askCloseRec = false;
			}
		}

		// ── NOTIFICATION DE FIN D'ENCODAGE ─────────────────────────────────────────
		// Toujours affichee (Rihen) : la passe finale peut durer des minutes, sa
		// fin merite un signal franc -- le meme langage visuel que la fermeture.
		inline void PaintEncodeDoneDialog(NkModelerPainter &p, float32 W, float32 H,
										  NkModelerState &st, NkHitRegistry &hit) {
			if (!st.encodeDone)
				return;
			p.Fill({0.f, 0.f, W, H}, NkColor{0, 0, 0, 150}); // voile
			hit.Add("dlge.veil", {0.f, 0.f, W, H});
			const float32 bw = S(470.f), bh = S(150.f);
			const NkRect box{(W - bw) * 0.5f, (H - bh) * 0.5f, bw, bh};
			p.Outline(box, NkRole::Border, NkRole::PanelHeader, 6.f);
			hit.Add("dlge.box", box);
			p.TextV(box.x + S(20.f), box.y + S(14.f), S(24.f), "Video terminee");
			p.Clip({box.x + S(20.f), box.y + S(44.f), bw - S(40.f), S(22.f)});
			p.TextV(box.x + S(20.f), box.y + S(44.f), S(22.f), st.encodeDonePath,
					NkRole::TextMuted);
			p.Unclip();
			const char *ok = "D'accord";
			const float32 w = p.TextW(ok) + S(28.f);
			const NkRect br{box.x + box.w - w - S(14.f), box.y + bh - S(44.f), w, S(28.f)};
			const bool over = hit.Add("dlge.ok", br);
			p.Fill(br, over ? NkRole::AccentSel : NkRole::AccentUi, 4.f);
			p.TextV(br.x + (w - p.TextW(ok)) * 0.5f, br.y, br.h, ok, NkRole::TextOnAccent);
			if (hit.Clicked("dlge.ok"))
				st.encodeDone = false;
		}

		inline void PaintCloseDialog(NkModelerPainter &p, float32 W, float32 H, NkModelerState &st,
									 NkHitRegistry &hit) {
			// La croix de l'OS arrive ICI : meme politique que la croix dessinee.
			if (st.wantClose) {
				st.wantClose = false;
				NkRequestClose(st);
			}
			// (La boite « Fermer la scene ? » a DISPARU, et c'est le but du
			// chantier : fermer un onglet ne ferme qu'une VUE, le document reste
			// dans le projet et sa carte dans le navigateur. Demander confirmation
			// laisserait croire qu'il y a quelque chose a perdre.)

			if (!st.askClose)
				return;
			p.Fill({0.f, 0.f, W, H}, NkColor{0, 0, 0, 150}); // voile
			hit.Add("dlg.veil", {0.f, 0.f, W, H});

			const float32 bw = S(420.f), bh = S(150.f);
			const NkRect box{(W - bw) * 0.5f, (H - bh) * 0.5f, bw, bh};
			p.Outline(box, NkRole::Border, NkRole::PanelHeader, 6.f);
			hit.Add("dlg.box", box);
			p.TextV(box.x + S(20.f), box.y + S(14.f), S(24.f), "Quitter NK3DModeler ?");
			// LA BOITE S'OUVRE TOUJOURS (Rihen), mais elle ne raconte pas la meme
			// chose selon ce qu'il reste a perdre : annoncer « modifications non
			// enregistrees » alors que tout est ecrit ferait douter d'un travail
			// pourtant deja sur le disque.
			const bool unsaved = st.dirty;
			p.TextV(box.x + S(20.f), box.y + S(44.f), S(22.f),
					unsaved ? "Des modifications ne sont pas enregistrees."
							: "Tout est enregistre.",
					NkRole::TextMuted);

			struct B {
					const char *key;
					const char *label;
					bool primary;
			};
			// L'ordre compte : l'action la plus SURE est a droite, sous la main, et
			// c'est elle qui porte l'accent. Â« Quitter sans enregistrer Â» reste
			// atteignable mais ne se propose pas.
			// RIEN A ENREGISTRER : deux boutons seulement. Proposer « Enregistrer et
			// quitter » quand il n'y a rien a ecrire ferait croire qu'il reste du
			// travail en attente, et « Quitter SANS enregistrer » serait alarmant
			// pour une sortie parfaitement propre.
			const B kBtns[3] = {
				{"dlg.cancel", "Annuler", false},
				{"dlg.discard", unsaved ? "Quitter sans enregistrer" : "Quitter", !unsaved},
				{"dlg.save", "Enregistrer et quitter", true}};
			const int32 nBtn = unsaved ? 3 : 2;
			float32 bx = box.x + box.w - S(14.f);
			for (int32 i = nBtn - 1; i >= 0; --i) {
				const float32 w = p.TextW(kBtns[i].label) + S(24.f);
				const NkRect br{bx - w, box.y + bh - S(44.f), w, S(28.f)};
				const bool over = hit.Add(kBtns[i].key, br);
				if (kBtns[i].primary)
					p.Fill(br, over ? NkRole::AccentSel : NkRole::AccentUi, 4.f);
				else
					p.Outline(br, NkRole::Border, over ? NkRole::PanelBg : NkRole::PanelHeader, 4.f);
				const float32 lw = p.TextW(kBtns[i].label);
				p.TextV(br.x + (w - lw) * 0.5f, br.y, br.h, kBtns[i].label,
						kBtns[i].primary ? NkRole::TextOnAccent : NkRole::Text);
				bx -= w + S(10.f);
			}

			if (hit.Clicked("dlg.cancel"))
				st.askClose = false;
			if (hit.Clicked("dlg.discard"))
				st.running = false;
			if (hit.Clicked("dlg.save")) {
				// L'enregistrement est demande en DIFFERE (il peut ouvrir
				// « Enregistrer sous... » si le projet n'a pas encore de chemin) et
				// la fermeture attend son resultat. Fermer tout de suite fermerait
				// AVANT d'avoir ecrit.
				//
				// La scene EST desormais enregistree avec le projet -- sauf la
				// geometrie editee sommet par sommet et les modificateurs, que
				// l'ecran d'accueil enumere.
				st.askClose = false;
				st.projPending = 3;
				st.quitAfterSave = true;
			}
		}

		// â”€â”€ BARRE D'ETAT â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
		inline void PaintStatus(NkModelerPainter &p, NkHitRegistry &hit, const NkRect &r,
								NkModelerState &st) {
			char stats[128];
			if (st.mode == NkMode::Object)
				snprintf(stats, sizeof(stats), "Objets 6 - selectionne : %s - 60 ips",
						 st.selectedObject == 1 ? "Cube" : "-");
			else
				snprintf(stats, sizeof(stats), "Sommets 8 - Aretes 12 - Faces 6 - %s - 60 ips",
						 NkModeName(st.mode));
			p.Fill(r, NkRole::PanelHeader);
			p.HLine(r.x, r.y, r.w);
			float32 x = r.x + kPad;
			struct B {
					NkIcon ic;
					const char *label;
			};
			static const B kBtns[] = {{NkIcon::Drawer, "Tiroir"}, {NkIcon::Journal, "Journal"}};
			static const char *const kBtnKeys[2] = {"status.tiroir", "status.journal"};
			for (int32 i = 0; i < 2; ++i) {
				// CES DEUX-LA ETAIENT DE SIMPLES DESSINS : ni zone cliquable, ni
				// effet. Le journal en avait pourtant le plus besoin -- diagnostiquer
				// sans pouvoir lire ce que le moteur ecrit revient a enchainer les
				// hypotheses (Rihen, 13 aout : « rends-le fonctionnel »).
				const float32 bw = 18.f + p.TextW(kBtns[i].label) + 8.f;
				const NkRect bb{x - 4.f, r.y + 3.f, bw, r.h - 6.f};
				const bool ov = hit.Add(kBtnKeys[i], bb);
				const bool actif = (i == 1) && st.journalOpen;
				if (actif)
					p.Fill(bb, NkRole::AccentUi, 3.f);
				else if (ov)
					p.Fill(bb, NkRole::PanelBg, 3.f);
				p.IconV(x, r.y, r.h, kBtns[i].ic,
						actif ? NkRole::TextOnAccent : NkRole::Text, 13.f);
				p.TextV(x + 18.f, r.y, r.h, kBtns[i].label,
						actif ? NkRole::TextOnAccent : NkRole::Text);
				NkHelp(ov, i == 1 ? "Journal : les messages du moteur (diagnostic)"
								  : "Tiroir");
				if (hit.Clicked(kBtnKeys[i]) && i == 1)
					st.journalOpen = !st.journalOpen;
				x += 18.f + p.TextW(kBtns[i].label) + 16.f;
			}
			// TUTORIEL AU FOOTER (regle de Rihen) : il enregistre TOUTE
			// l'application, sa place est donc dans la barre de l'application --
			// la capture de la vue 3D, elle, reste dans la vue. Bouton-menu :
			// Photo prend une image, Video ouvre une prise que la barre
			// d'enregistrement ci-dessous pilote. Le menu s'ouvre VERS LE HAUT.
			{
				const float32 tw = p.TextW("Tutoriel");
				const NkRect tb{x, r.y + 4.f, 18.f + tw + 10.f, r.h - 8.f};
				const bool ovT = hit.Add("status.tuto", tb);
				if (st.tutoMenuOpen)
					p.Fill(tb, NkRole::AccentUi, 3.f);
				else
					p.Outline(tb, ovT ? NkRole::AccentUi : NkRole::Border,
							  NkRole::PanelHeader, 3.f);
				p.IconV(tb.x + 4.f, r.y, r.h, NkIcon::ImageRef,
						st.tutoMenuOpen ? NkRole::TextOnAccent : NkRole::Text, 12.f);
				p.TextV(tb.x + 20.f, r.y, r.h, "Tutoriel",
						st.tutoMenuOpen ? NkRole::TextOnAccent : NkRole::Text);
				if (hit.Clicked("status.tuto"))
					st.tutoMenuOpen = !st.tutoMenuOpen;
				if (st.tutoMenuOpen) {
					// « Video » n'est plus un stub : elle enregistre TOUTE la
					// fenetre, en parallele d'un eventuel enregistrement de la
					// vue -- deux points de vue d'une meme session.
					const bool tRec = demo::Demo3DHostRecTutoActive();
					static const char *const kTuKeys[2] = {"status.tuto.ph", "status.tuto.vid"};
					const char *kTuM[2] = {"Photo", tRec ? "Arreter la video" : "Video"};
					const float32 mh = S(24.f);
					for (int32 m = 0; m < 2; ++m) {
						const NkRect mr{tb.x, r.y - (float32)(m + 1) * (mh + 2.f), S(140.f), mh};
						const bool ovM = hit.Add(kTuKeys[m], mr);
						p.Fill(mr, ovM ? NkRole::AccentUi : NkRole::PanelHeader, 4.f);
						p.TextV(mr.x + 8.f, mr.y, mh, kTuM[m],
								ovM ? NkRole::TextOnAccent : NkRole::Text);
						if (hit.Clicked(kTuKeys[m])) {
							if (m == 0)
								st.capturePending = 2; // TOUTE la fenetre, en image
							else
								// La TAILLE de la fenetre n'est connue que de la
								// boucle principale : on demande, elle execute --
								// meme patron que capturePending.
								st.tutoRecPending = tRec ? 2 : 1;
							st.tutoMenuOpen = false;
						}
					}
					// un clic ailleurs referme
					if (hit.AnyClick() && !hit.IsHovered("status.tuto") &&
						!hit.IsHovered(kTuKeys[0]) && !hit.IsHovered(kTuKeys[1]))
						st.tutoMenuOpen = false;
				}
				x += tb.w + 12.f;
			}
			p.VLine(x, r.y + 6.f, r.h - 12.f);
			x += 10.f;
			p.Fill({x, r.y + (r.h - 20.f) * 0.5f, 240.f, 20.f}, NkRole::InputBg, 2.f);
			p.IconV(x + 6.f, r.y, r.h, NkIcon::Terminal, NkRole::Text, 12.f);
			p.TextV(x + 24.f, r.y, r.h, "Entrer une commande", NkRole::TextMuted);
			const float32 w = p.TextW(stats);
			p.TextV(r.x + r.w - w - kPad, r.y, r.h, stats, NkRole::TextMuted);

			// BARRE D'ENREGISTREMENT -- visible SEULEMENT pendant une prise. Une
			// prise en cours est un etat qu'on oublie : elle doit se signaler
			// d'elle-meme, dire DEPUIS QUAND elle tourne, et offrir les trois
			// seules decisions possibles -- suspendre, garder, jeter. Les deux
			// prises (la vue seule / toute la fenetre) sont independantes :
			// deux barres cote a cote, pas un etat partage.
			float32 rx = r.x + r.w - w - kPad - 14.f;
			{
				bool vOn = false;
				int32 fps = 24, vFirst = 0, vLast = 0, vCodec = 0;
				demo::Demo3DHostOutVideo(&vOn, &fps, &vFirst, &vLast, &vCodec);
				if (fps <= 0)
					fps = 24;
				struct Rec {
						const char *name;
						bool active, paused;
						int32 frames, dropped;
						bool enc; ///< passe finale du MP4 : la prise est finie
						int32 encDone, encTotal;
				};
				Rec kRecs[2] = {
					{"Vue", demo::Demo3DHostRecActive(), demo::Demo3DHostRecPaused(),
					 demo::Demo3DHostRecFrames(), demo::Demo3DHostRecDropped(),
					 demo::Demo3DHostRecEncoding(), 0, 0},
					{"Tutoriel", demo::Demo3DHostRecTutoActive(),
					 demo::Demo3DHostRecTutoPaused(), demo::Demo3DHostRecTutoFrames(),
					 demo::Demo3DHostRecTutoDropped(), demo::Demo3DHostRecTutoEncoding(), 0, 0},
				};
				demo::Demo3DHostRecEncodeProgress(&kRecs[0].encDone, &kRecs[0].encTotal);
				demo::Demo3DHostRecTutoEncodeProgress(&kRecs[1].encDone, &kRecs[1].encTotal);
				// Cles DISTINCTES par prise : deux barres partageant une cle se
				// voleraient les clics.
				static const char *const kRecKeys[2][3] = {
					{"status.rec.v.pause", "status.rec.v.stop", "status.rec.v.drop"},
					{"status.rec.t.pause", "status.rec.t.stop", "status.rec.t.drop"},
				};
				for (int32 k = 1; k >= 0; --k) { // de droite a gauche
					const Rec &R = kRecs[k];
					if (!R.active && !R.enc)
						continue;
					char lbl[96];
					const int32 sec = R.frames / fps;
					// PENDANT L'ENCODAGE la prise est finie : plus de temps qui
					// court, mais un travail qui avance. Le taire ferait passer
					// plusieurs minutes de calcul pour un blocage.
					if (R.enc)
						snprintf(lbl, sizeof(lbl), "%s  encodage %d/%d", R.name, R.encDone,
								 R.encTotal);
					// Les images SAUTEES ne se taisent pas : un enregistrement
					// troue doit se voir pendant qu'on peut encore recommencer.
					else if (R.dropped > 0)
						snprintf(lbl, sizeof(lbl), "%s  %02d:%02d  (%d sautees)", R.name,
								 sec / 60, sec % 60, R.dropped);
					else
						snprintf(lbl, sizeof(lbl), "%s  %02d:%02d", R.name, sec / 60,
								 sec % 60);
					// TROIS ICONES, pas trois libelles (Rihen) : le transport a un
					// dessin universel -- deux barres, un carre, une corbeille se
					// lisent sans mot, et la barre reste etroite dans le pied de
					// page. L'info-bulle porte la phrase complete.
					const NkIcon kAct[3] = {R.paused ? NkIcon::MediaPlay : NkIcon::MediaPause,
											NkIcon::MediaStop, NkIcon::Trash};
					// PENDANT L'ENCODAGE, AUCUN BOUTON : suspendre ou abandonner
					// une passe qui ne fait que relire des images deja prises
					// n'a pas de sens -- un bouton sans effet vaut moins que pas
					// de bouton.
					const int32 nAct = R.enc ? 0 : 3;
					const float32 abw = r.h - 8.f - 4.f; // boutons CARRES
					float32 total =
						12.f + p.TextW(lbl) + 16.f + (float32)nAct * (abw + 4.f);
					const NkRect bar{rx - total, r.y + 4.f, total, r.h - 8.f};
					p.Fill(bar, NkRole::InputBg, 3.f);
					// Pastille : pleine quand ca tourne, grise en pause --
					// l'etat se lit sans avoir a lire le bouton.
					const float32 dr = 7.f;
					const NkRect dot{bar.x + 6.f, bar.y + (bar.h - dr) * 0.5f, dr, dr};
					// La pastille dit L'ETAT : rouge on filme, gris suspendu,
					// ambre l'encodage travaille. Rester rouge apres l'arret
					// ferait croire que la prise court encore.
					p.Fill(dot,
						   R.enc ? NkColor{230, 160, 40, 255}
								 : (R.paused ? NkColor{140, 140, 140, 255}
											 : NkColor{231, 76, 60, 255}),
						   dr * 0.5f);
					float32 bx = bar.x + 6.f + dr + 6.f;
					p.TextV(bx, r.y, r.h, lbl, R.paused ? NkRole::TextMuted : NkRole::Text);
					bx += p.TextW(lbl) + 10.f;
					for (int32 a = 0; a < nAct; ++a) {
						const NkRect ab{bx, bar.y + 2.f, abw, bar.h - 4.f};
						const bool ov = hit.Add(kRecKeys[k][a], ab);
						// ABANDON efface la prise : il porte le rouge, pour qu'on
						// ne le clique pas en croyant arreter.
						if (a == 2)
							p.Fill(ab, ov ? NkColor{231, 76, 60, 255} : NkColor{231, 76, 60, 90},
								   3.f);
						else
							p.Fill(ab, ov ? NkRole::AccentUi : NkRole::PanelHeader, 3.f);
						p.IconV(ab.x + (ab.w - 12.f) * 0.5f, ab.y, ab.h, kAct[a],
								(ov || a == 2) ? NkRole::TextOnAccent : NkRole::Text, 12.f);
						if (ov)
							hit.WantCursor(NkCursorWant::Hand);
						if (hit.Clicked(kRecKeys[k][a])) {
							if (k == 0) {
								if (a == 0)
									demo::Demo3DHostRecPause(!R.paused);
								else
									demo::Demo3DHostRecStop(a == 1);
							} else {
								if (a == 0)
									demo::Demo3DHostRecTutoPause(!R.paused);
								else
									demo::Demo3DHostRecTutoStop(a == 1);
							}
						}
						bx += abw + 4.f;
					}
					rx = bar.x - 10.f;
				}
			}
		}

	} // namespace nk3d
} // namespace nkentseu
