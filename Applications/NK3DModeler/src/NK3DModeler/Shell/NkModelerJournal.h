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

		// ── LE PANNEAU ──────────────────────────────────────────────────────────
		// Ancre EN BAS, au-dessus de la barre d'etat : c'est de la barre d'etat
		// qu'il s'ouvre, et un panneau doit apparaitre la ou on l'appelle.
		inline void PaintJournal(NkModelerPainter &p, NkHitRegistry &hit, NkModelerState &st,
								 const nkgui::NkGuiInput &in, const NkRect &full) {
			if (!st.journalOpen)
				return;
			const float32 hJ = S(190.f);
			const NkRect r{full.x, full.y + full.h - hJ, full.w, hJ};
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
			const NkRect zone{r.x + S(4.f), r.y + hdr + S(2.f), r.w - S(8.f),
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

			float32 y = zone.y - st.journalScroll;
			j.mtx.Lock();
			for (int32 i = 0; i < nb; ++i) {
				if (y + lh >= zone.y && y <= zone.y + zone.h) {
					const NkJournalLigne &l = j.lignes[(debut + i) % kJournalMax];
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

			if (maxSc > 0.f)
				p.Fill({zone.x + zone.w - S(4.f),
						zone.y + (st.journalScroll / contenu) * zone.h, S(3.f),
						zone.h * (zone.h / contenu)},
					   NkRole::TextMuted, 2.f);
		}

	} // namespace nk3d
} // namespace nkentseu
