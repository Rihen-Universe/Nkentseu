#pragma once
// -----------------------------------------------------------------------------
// @File    NkModelerJournal.h
// @Brief   LE JOURNAL : les messages du moteur, lisibles DANS l'application.
//
// POURQUOI IL FALLAIT LE FAIRE MAINTENANT
//   Le bouton « Journal » de la barre d'etat n'etait qu'un dessin : ni zone
//   cliquable, ni etat, ni panneau. Or diagnostiquer sans journal, c'est
//   enchainer les hypotheses -- ce qui a deja coute plusieurs correctifs
//   inutiles sur ce projet. Un message ecrit par le moteur et que personne ne
//   peut lire ne sert a rien.
//
// COMMENT IL CAPTE LES MESSAGES
//   Par un SINK, le point d'extension prevu par NKLogger : le logger pousse
//   chaque message a tous ses sinks (console, fichier...). On en ajoute un qui
//   garde les dernieres lignes en memoire. Aucune interception, aucun detour --
//   c'est le mecanisme officiel, et il capte donc TOUT ce que le moteur ecrit,
//   pas seulement ce que l'application penserait a lui transmettre.
//
//   L'anneau est BORNE : un journal qui grossit sans fin finit par manger la
//   memoire d'une session longue. Les plus vieilles lignes tombent.
// @Author  Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
// -----------------------------------------------------------------------------
#include "NK3DModeler/Shell/NkModelerUI.h"
#include "NK3DModeler/Shell/NkModelerInput.h"
#include "NK3DModeler/Shell/NkModelerTables.h"
#include "NK3DModeler/Shell/NkModelerWidgets.h" // NkUiCtx (presse-papier), NkHelp
#include "NKEditorKit/NkEditorScrollbar.h"	   // la barre MANIPULABLE du kit
#include "NKLogger/NkLog.h"
#include "NKLogger/NkSink.h"
#include "NKLogger/NkLogMessage.h"
#include "NKThreading/NkMutex.h"
#include "NKMemory/NkSharedPtr.h"

namespace nkentseu {
	namespace nk3d {

		/// Nombre de lignes gardees. 600 couvre largement le demarrage complet
		/// (ou tout se joue) sans peser : ~60 Ko.
		inline constexpr int32 kJournalMax = 600;

		struct NkJournalLigne {
				char texte[220] = {};
				int32 niveau = 0; ///< 0 info/trace, 1 avertissement, 2 erreur
		};

		/// L'anneau, et le verrou qui le protege : le logger peut ecrire depuis
		/// UN AUTRE FIL que celui du dessin. Sans verrou, on lirait une ligne en
		/// cours d'ecriture -- un plantage rare, donc le pire a diagnostiquer.
		struct NkJournalBuf {
				NkJournalLigne lignes[kJournalMax];
				int32 debut = 0; ///< index de la plus ancienne
				int32 nb = 0;
				threading::NkMutex mtx;
		};

		inline NkJournalBuf &NkJournal() {
			static NkJournalBuf j;
			return j;
		}

		/// Le puits branche sur le logger du moteur.
		class NkJournalSink : public NkISink {
			public:
				void Log(const NkLogMessage &m) override {
					if (!ShouldLog(m.level))
						return;
					NkJournalBuf &j = NkJournal();
					j.mtx.Lock();
					const int32 idx = (j.debut + j.nb) % kJournalMax;
					NkJournalLigne &l = j.lignes[idx];
					const char *src = m.message.CStr();
					uint32 k = 0;
					for (; src && src[k] && k < sizeof(l.texte) - 1u; ++k)
						l.texte[k] = src[k];
					l.texte[k] = '\0';
					l.niveau = (m.level >= NkLogLevel::NK_ERROR)
								   ? 2
								   : (m.level == NkLogLevel::NK_WARN ? 1 : 0);
					if (j.nb < kJournalMax)
						++j.nb;
					else
						j.debut = (j.debut + 1) % kJournalMax; // la plus vieille tombe
					j.mtx.Unlock();
				}
				void Flush() override {}
				// Le journal montre le message BRUT : ni horodatage, ni nom de
				// logger, ni couleur ANSI. Ces decorations servent a un fichier ou
				// une console, pas a un panneau ou la place est comptee.
				void SetFormatter(memory::NkUniquePtr<NkLoggerFormatter> f) override {
					(void)f;
				}
				void SetPattern(const NkString &pattern) override {
					(void)pattern;
				}
				NkLoggerFormatter *GetFormatter() const override {
					return nullptr;
				}
				NkString GetPattern() const override {
					return NkString();
				}
		};

		/// Copie des lignes `a`..`b` (bornes incluses, ordre indifferent) dans le
		/// presse-papier. `a < 0` = TOUT le journal.
		inline void NkJournalCopier(int32 a, int32 b) {
			nkgui::NkGuiContext *gc = NkUiCtx();
			if (!gc)
				return;
			NkJournalBuf &j = NkJournal();
			j.mtx.Lock();
			int32 d = a, f = b;
			if (d < 0) {
				d = 0;
				f = j.nb - 1;
			}
			if (d > f) {
				const int32 t = d;
				d = f;
				f = t;
			}
			if (d < 0)
				d = 0;
			if (f >= j.nb)
				f = j.nb - 1;
			NkString out;
			for (int32 i = d; i <= f; ++i) {
				out += j.lignes[(j.debut + i) % kJournalMax].texte;
				out += '\n';
			}
			j.mtx.Unlock();
			gc->SetClipboard(out.CStr());
		}

		/// A appeler UNE FOIS au demarrage, avant tout ce qu'on veut pouvoir
		/// relire. Les messages emis avant sont perdus pour le panneau -- ils
		/// restent dans la console et le fichier.
		inline void NkJournalInstall() {
			static bool fait = false;
			if (fait)
				return;
			fait = true;
			// Construit DIRECTEMENT un pointeur sur l'interface : `NkSharedPtr`
			// n'a pas de conversion derivee -> base, donc passer par
			// NkMakeShared<NkJournalSink> ne se convertirait pas en
			// NkSharedPtr<NkISink>.
			NkLog::Instance().AddSink(
				memory::NkSharedPtr<NkISink>(new NkJournalSink()));
		}

		/// L'EMPRISE DU JOURNAL, calculee A PART : la boucle principale en a besoin
		/// AVANT de peindre les panneaux, pour leur interdire ce rectangle. Le
		/// journal est peint en dernier, or un panneau teste ses clics AU MOMENT ou
		/// il se peint -- donc avant que le journal ait declare quoi que ce soit.
		/// Le navigateur ouvrait ainsi son menu contextuel a travers le journal
		/// (Rihen, 13 aout : « les evenements traversent »).
		inline NkRect NkJournalRect(const NkRect &full) {
			const float32 hJ = S(190.f);
			return {full.x, full.y + full.h - hJ, full.w, hJ};
		}

		// ── LE PANNEAU ──────────────────────────────────────────────────────────
		// Ancre EN BAS, au-dessus de la barre d'etat : c'est de la barre d'etat
		// qu'il s'ouvre, et un panneau doit apparaitre la ou on l'appelle.
		inline void PaintJournal(NkModelerPainter &p, NkHitRegistry &hit, NkModelerState &st,
								 const nkgui::NkGuiInput &in, const NkRect &full) {
			if (!st.journalOpen)
				return;
			const NkRect r = NkJournalRect(full);
			p.Fill(r, NkRole::PanelBg);
			p.HLine(r.x, r.y, r.w);
			// L'emprise ENTIERE est declaree : sans elle, un clic dans le journal
			// atteindrait la vue 3D ou le navigateur qui se trouvent dessous.
			hit.Add("journal.panel", r);

			// En-tete : titre, compteur, vider, fermer.
			const float32 hdr = S(24.f);
			p.Fill({r.x, r.y, r.w, hdr}, NkRole::PanelHeader);
			p.IconV(r.x + S(6.f), r.y, hdr, NkIcon::Journal, NkRole::Text, 12.f);
			p.TextV(r.x + S(24.f), r.y, hdr, "Journal", NkRole::Text);

			NkJournalBuf &j = NkJournal();
			j.mtx.Lock();
			const int32 nb = j.nb, debut = j.debut;
			j.mtx.Unlock();

			char cpt[48];
			snprintf(cpt, sizeof(cpt), "%d ligne%s", nb, nb > 1 ? "s" : "");
			p.TextV(r.x + S(86.f), r.y, hdr, cpt, NkRole::TextMuted);

			// COPIER : la selection si elle existe, tout le journal sinon. C'est le
			// geste le plus frequent -- coller une trace dans un message -- et il ne
			// doit pas exiger de selectionner d'abord.
			const NkRect bCopie{r.x + r.w - S(228.f), r.y + S(3.f), S(72.f), hdr - S(6.f)};
			const bool ovC = hit.Add("journal.copy", bCopie);
			p.Outline(bCopie, ovC ? NkRole::AccentUi : NkRole::Border, NkRole::PanelBg, 3.f);
			p.TextV(bCopie.x + S(8.f), bCopie.y, bCopie.h, "Copier", NkRole::Text);
			NkHelp(ovC, "Copier la selection, ou tout le journal si rien n'est choisi");
			if (hit.Clicked("journal.copy"))
				NkJournalCopier(st.journalAncre, st.journalTete);

			const NkRect bVider{r.x + r.w - S(150.f), r.y + S(3.f), S(66.f), hdr - S(6.f)};
			const bool ovV = hit.Add("journal.clear", bVider);
			p.Outline(bVider, ovV ? NkRole::AccentUi : NkRole::Border, NkRole::PanelBg, 3.f);
			p.TextV(bVider.x + S(8.f), bVider.y, bVider.h, "Vider", NkRole::Text);
			if (hit.Clicked("journal.clear")) {
				j.mtx.Lock();
				j.nb = 0;
				j.debut = 0;
				j.mtx.Unlock();
				st.journalScroll = 0.f;
			}
			const NkRect bFerm{r.x + r.w - S(76.f), r.y + S(3.f), S(66.f), hdr - S(6.f)};
			const bool ovF = hit.Add("journal.close", bFerm);
			p.Outline(bFerm, ovF ? NkRole::AccentUi : NkRole::Border, NkRole::PanelBg, 3.f);
			p.TextV(bFerm.x + S(8.f), bFerm.y, bFerm.h, "Fermer", NkRole::Text);
			if (hit.Clicked("journal.close"))
				st.journalOpen = false;

			// ── LES LIGNES ─────────────────────────────────────────────────────
			// La gouttiere est RESERVEE a droite : la barre est manipulable, elle a
			// donc besoin de sa place, pas d'un filet pose sur le texte.
			const float32 sbW = S(12.f);
			const NkRect zone{r.x + S(4.f), r.y + hdr + S(2.f), r.w - S(8.f) - sbW,
							  r.h - hdr - S(6.f)};
			p.Fill(zone, NkRole::InputBg, 3.f);
			p.Clip(zone);
			hit.PushClip(zone);
			const float32 lh = S(15.f);
			const float32 contenu = (float32)nb * lh;
			const float32 maxSc = contenu > zone.h ? contenu - zone.h : 0.f;
			// SUIVI AUTOMATIQUE : tant qu'on est en bas, on y reste quand une
			// ligne arrive. Des qu'on remonte, le journal cesse de sauter -- lire
			// une trace en train de defiler est impossible autrement.
			if (st.journalSuivre)
				st.journalScroll = maxSc;
			if (hit.IsHovered("journal.panel") && in.wheel != 0.f) {
				st.journalScroll -= in.wheel * lh * 3.f;
				st.journalSuivre = false;
			}
			if (st.journalScroll > maxSc)
				st.journalScroll = maxSc;
			if (st.journalScroll < 0.f)
				st.journalScroll = 0.f;
			if (st.journalScroll >= maxSc - 1.f)
				st.journalSuivre = true; // revenu en bas : on re-suit

			// ── SELECTION PAR LIGNES ───────────────────────────────────────────
			// Par LIGNES et non par caracteres : dans un journal on copie des
			// messages entiers, et une selection caractere par caractere couterait
			// dix fois le code pour un geste qu'on ne fait pas ici.
			const bool dansZone = nkgui::NkGuiRectContains(zone, {in.mousePos.x, in.mousePos.y});
			const int32 ligneSousSouris =
				dansZone ? (int32)((in.mousePos.y - zone.y + st.journalScroll) / lh) : -1;
			const bool ligneValide = ligneSousSouris >= 0 && ligneSousSouris < nb;
			if (ligneValide && hit.Clicked("journal.panel")) {
				if (in.shiftDown && st.journalAncre >= 0)
					st.journalTete = ligneSousSouris; // etend depuis l'ancre
				else {
					st.journalAncre = ligneSousSouris;
					st.journalTete = ligneSousSouris;
					st.journalDrag = true;
				}
				st.journalMenu = false;
			}
			if (st.journalDrag) {
				if (!hit.MouseDown())
					st.journalDrag = false;
				else if (ligneValide)
					st.journalTete = ligneSousSouris;
			}
			// CLIC DROIT : ouvre le menu SANS casser la selection, et la pose sur
			// la ligne visee si on cliquait hors d'elle -- comme un explorateur.
			if (dansZone && hit.RightClicked("journal.panel")) {
				if (ligneValide) {
					const int32 lo = st.journalAncre < st.journalTete ? st.journalAncre : st.journalTete;
					const int32 hi = st.journalAncre < st.journalTete ? st.journalTete : st.journalAncre;
					if (ligneSousSouris < lo || ligneSousSouris > hi) {
						st.journalAncre = ligneSousSouris;
						st.journalTete = ligneSousSouris;
					}
				}
				st.journalMenu = true;
				st.journalMenuX = in.mousePos.x;
				st.journalMenuY = in.mousePos.y;
			}
			// Ctrl+C / Ctrl+A : l'application pose deja ces drapeaux pour les champs
			// de saisie ; le journal n'a qu'a les lire quand la souris est chez lui.
			if (dansZone || st.journalDrag) {
				if (in.wantSelectAll) {
					st.journalAncre = 0;
					st.journalTete = nb - 1;
				}
				if (in.wantCopy)
					NkJournalCopier(st.journalAncre, st.journalTete);
			}
			const int32 selLo = (st.journalAncre < 0 || st.journalTete < 0)
									? -1
									: (st.journalAncre < st.journalTete ? st.journalAncre : st.journalTete);
			const int32 selHi = (st.journalAncre < 0 || st.journalTete < 0)
									? -2
									: (st.journalAncre < st.journalTete ? st.journalTete : st.journalAncre);

			float32 y = zone.y - st.journalScroll;
			j.mtx.Lock();
			for (int32 i = 0; i < nb; ++i) {
				if (y + lh >= zone.y && y <= zone.y + zone.h) {
					const NkJournalLigne &l = j.lignes[(debut + i) % kJournalMax];
					if (i >= selLo && i <= selHi)
						p.Fill({zone.x, y, zone.w, lh}, NkRole::AccentSel, 0.f);
					// LE THEME N'A NI « danger » NI « avertissement ». On emprunte le
					// rouge des axes pour l'erreur -- detournement assume et signale
					// ici : le jour ou ces roles existeront, c'est cette ligne a
					// changer, pas une couleur en dur eparpillee.
					const NkRole col = l.niveau == 2	 ? NkRole::AxisX
									   : l.niveau == 1	 ? NkRole::AccentUi
														 : NkRole::Text;
					p.TextV(zone.x + S(6.f), y, lh, l.texte, col);
				}
				y += lh;
			}
			j.mtx.Unlock();
			hit.PopClip();
			p.Unclip();

			// ── LA BARRE, MANIPULABLE ──────────────────────────────────────────
			// Celle du KIT (NkVScrollbar) : poignee glissable, fleches, molette et
			// clic dans la gouttiere. C'etait un simple filet indicatif -- « la
			// scrollbar doit etre manipulable » (Rihen, 13 aout). Le kit la porte
			// deja pour le selecteur de fichiers ; la reecrire aurait fait deux
			// barres a corriger.
			if (nkgui::NkGuiContext *gc = NkUiCtx()) {
				const NkRect piste{zone.x + zone.w + S(2.f), zone.y, sbW, zone.h};
				editorkit::NkVScrollbar(*gc, gc->dl, piste, st.journalScroll, contenu, zone.h,
										0xF00A1201u, lh);
				// Glisser la barre, c'est vouloir REGARDER : le suivi automatique
				// s'arrete, sinon la prochaine ligne ramenerait en bas.
				if (st.journalScroll < maxSc - 1.f)
					st.journalSuivre = false;
				hit.Add("journal.sb", piste);
			}

			// ── MENU CONTEXTUEL ────────────────────────────────────────────────
			if (st.journalMenu) {
				static const char *const kIt[3] = {"Copier", "Tout copier", "Vider"};
				static const char *const kKey[3] = {"journal.m.cp", "journal.m.all",
													"journal.m.clr"};
				const float32 mw = S(140.f), mh = S(24.f);
				const NkRect mr{st.journalMenuX, st.journalMenuY, mw, mh * 3.f};
				p.Fill(mr, NkRole::PanelHeader, 4.f);
				p.OutlineSharp(mr, NkRole::Border);
				for (int32 m = 0; m < 3; ++m) {
					const NkRect it{mr.x, mr.y + (float32)m * mh, mw, mh};
					const bool ovI = hit.Add(kKey[m], it);
					if (ovI)
						p.Fill(it, NkRole::AccentUi, 3.f);
					p.TextV(it.x + S(8.f), it.y, mh, kIt[m],
							ovI ? NkRole::TextOnAccent : NkRole::Text);
					if (hit.Clicked(kKey[m])) {
						if (m == 0)
							NkJournalCopier(st.journalAncre, st.journalTete);
						else if (m == 1)
							NkJournalCopier(-1, -1);
						else {
							j.mtx.Lock();
							j.nb = 0;
							j.debut = 0;
							j.mtx.Unlock();
							st.journalAncre = st.journalTete = -1;
							st.journalScroll = 0.f;
						}
						st.journalMenu = false;
					}
				}
				// Un clic AILLEURS le referme -- y compris le clic droit suivant,
				// qui le rouvrira ailleurs juste apres.
				if (hit.AnyClick() && !nkgui::NkGuiRectContains(mr, {in.mousePos.x, in.mousePos.y}))
					st.journalMenu = false;
			}
		}

	} // namespace nk3d
} // namespace nkentseu
