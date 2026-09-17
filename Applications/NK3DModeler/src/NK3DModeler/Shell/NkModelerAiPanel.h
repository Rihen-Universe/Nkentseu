#pragma once
// -----------------------------------------------------------------------------
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @File    Applications/NK3DModeler/src/NK3DModeler/Shell/NkModelerAiPanel.h
// @License Proprietary - All Rights Reserved (see LICENSE)
// -----------------------------------------------------------------------------
//
// =============================================================================
//  (b9) L'ASSISTANT : UN CHAMP, UNE LISTE, ET UN REFUS QUI SE VOIT
// =============================================================================
//  Le pont existe et il est mesure (`sonde_pont_ia.ps1`, 8 criteres) : un verbe
//  et ses parametres deviennent une operation reelle, bornee, annulable. Ce
//  fichier ne fait que lui donner une porte d'entree a la main.
//
//  ⚠️ IL N'APPELLE PAS LE PONT LUI-MEME. Il ECRIT la demande dans
//     `st.aiPending` ; la boucle l'EXECUTE, au meme endroit et au meme moment
//     que `st.pendingAction`. Un panneau qui appellerait `NkVpPoserAction`
//     directement serait un SECOND CHEMIN vers le meme etat -- le motif que ce
//     depot a paye deux fois cette semaine (TAB traite a deux endroits, puis
//     les sous-modes qui ecrivaient dans un miroir que la boucle reecrivait).
//
//  ⚠️ CE QUE CE PANNEAU N'EST PAS : une conversation avec un modele. Le
//     transport existe (`Kernel/System/NKConverse`) et le dorsal local aussi
//     (`Applications/NKDesignLLM`), mais rien n'est branche ici : ce qu'on tape
//     va DIRECTEMENT au pont. C'est volontaire et c'est utile tout de suite --
//     et ca rend le jour du branchement trivial, parce que la seule chose qui
//     changera est l'origine du texte, pas ce qu'on en fait.
//
//  ⚠️ LE ZERO EST UN CAS, PAS UNE EVIDENCE : champ vide -> rien ne part. Sans
//     cette garde, un clic distrait sur « Envoyer » soumettrait la chaine vide,
//     que la table refuserait en disant « je ne connais pas "" » -- un refus
//     exact et incomprehensible.
//
//  CE QUE LES BRIQUES SONT, ET D'OU ELLES VIENNENT (rien n'est invente ici) :
//    - la liste qui defile          : le gabarit de `PaintJournal`
//    - le champ de saisie           : `editorkit::NkOverlayTextField`, deja
//                                     utilise 4 fois dans ce shell
//    - le texte enveloppe           : `p.TextWrap`
//    - les zones cliquables         : `NkHitRegistry`
// -----------------------------------------------------------------------------

#include "NK3DModeler/Shell/NkModelerUI.h"
#include "NK3DModeler/Shell/NkModelerInput.h"
#include "NK3DModeler/Shell/NkModelerCommon.h"
#include "NKEditorKit/NkEditorKit.h"

namespace nkentseu {
	namespace nk3d {

		// Copie bornee, sans dependre de strncpy : le depot ecrit ses propres
		// primitives de chaine (zero-STL), et une troncature doit etre VOULUE.
		inline void NkAiCopie(char *dst, uint32 cap, const char *src) {
			if (!dst || cap == 0u)
				return;
			uint32 i = 0;
			if (src)
				for (; src[i] && i + 1u < cap; ++i)
					dst[i] = src[i];
			dst[i] = 0;
		}

		inline bool NkAiVide(const char *s) {
			if (!s)
				return true;
			for (uint32 i = 0; s[i]; ++i)
				if (s[i] != ' ' && s[i] != '\t')
					return false; // au moins un caractere qui n'est pas du blanc
			return true;
		}

		// ── LA SOUMISSION, ET ELLE EST LA SEULE ────────────────────────────────
		// Le bouton l'appelle, la touche Entree l'appellera, et le crochet de
		// mesure `NK_AI_DEMANDE` entre EXACTEMENT ICI. Ce n'est pas un raccourci
		// pour la sonde : c'est le meme point d'entree, sinon la sonde mesurerait
		// un chemin que Rodolf n'emprunte jamais.
		inline bool NkAiSoumettre(NkModelerState &st, const char *texte) {
			if (NkAiVide(texte)) {
				// LE ZERO. On ne soumet rien, et on DIT pourquoi : un bouton qui
				// ne reagit pas se lit comme un bouton casse.
				NkAiCopie(st.aiMotif, sizeof(st.aiMotif),
						  "Rien a faire : la demande est vide.");
				st.aiMotifEstRefus = true;
				return false;
			}
			NkAiCopie(st.aiPending, sizeof(st.aiPending), texte);
			return true;
		}

		// Journalise le tour DANS LE PANNEAU (pas sur la sortie standard) :
		// l'historique est ce qui permet de relire ce qu'on a demande, et c'est
		// la premiere chose qui manque quand on essaie un outil de ce genre.
		inline void NkAiNoter(NkModelerState &st, const char *texte, bool ok) {
			if (st.aiHistN >= NkModelerState::kAiHist) {
				// Fenetre glissante : on perd le plus ancien, jamais le plus recent.
				for (int32 i = 1; i < NkModelerState::kAiHist; ++i) {
					NkAiCopie(st.aiHist[i - 1], sizeof(st.aiHist[0]), st.aiHist[i]);
					st.aiHistOk[i - 1] = st.aiHistOk[i];
				}
				st.aiHistN = NkModelerState::kAiHist - 1;
			}
			NkAiCopie(st.aiHist[st.aiHistN], sizeof(st.aiHist[0]), texte);
			st.aiHistOk[st.aiHistN] = ok ? 1u : 0u;
			++st.aiHistN;
		}

		// ── LE PANNEAU ─────────────────────────────────────────────────────────
		// `yy` AVANCE, comme dans toutes les pastilles de ce shell.
		inline void PaintAiPanel(NkModelerPainter &p, NkHitRegistry &hit, NkModelerState &st,
								 NkWidgetState &ws, nkgui::NkGuiContext *guiCtx,
								 const NkRect &r, float32 &yy) {
			const float32 kRowH = S(22.f);
			const float32 pad = S(6.f);

			// ── L'HISTORIQUE, comme le Journal ─────────────────────────────────
			for (int32 i = 0; i < st.aiHistN; ++i) {
				const bool ok = st.aiHistOk[i] != 0u;
				// Une pastille de couleur AVANT le texte : on doit pouvoir lire le
				// resultat d'un coup d'oeil, sans relire la phrase.
				p.Fill({r.x, yy + S(6.f), S(4.f), kRowH - S(12.f)},
					   ok ? NkRole::AccentUi : NkRole::AxisX, 2.f);
				p.TextV(r.x + S(10.f), yy, kRowH, st.aiHist[i],
						ok ? NkRole::Text : NkRole::TextMuted);
				yy += kRowH;
			}
			if (st.aiHistN == 0) {
				p.TextV(r.x, yy, kRowH, "Selectionnez, puis demandez.", NkRole::TextMuted);
				yy += kRowH;
				// ⚠️ ON DIT CE QU'ON SAIT FAIRE. Un champ libre sans exemple se
				//    repond par des phrases que rien ne comprend, et l'utilisateur
				//    conclut que l'outil ne marche pas -- alors qu'il n'a jamais su
				//    ce qu'on attendait de lui.
				p.TextV(r.x, yy, kRowH, "Ex. : subdivide:3  ·  bevel:0.2:4  ·  undo",
						NkRole::TextMuted);
				yy += kRowH;
			}

			// ── LE MOTIF DU DERNIER REFUS, A L'ECRAN ───────────────────────────
			// Le journal sert aux sondes. Rodolf n'a pas de console.
			if (st.aiMotif[0]) {
				const NkRect box{r.x, yy, r.w, kRowH * 2.f};
				p.Fill(box, NkRole::InputBg, 3.f);
				p.TextWrap(r.x + S(6.f), yy + S(3.f), r.w - S(12.f), st.aiMotif,
						   st.aiMotifEstRefus ? NkRole::AxisX : NkRole::TextMuted);
				yy += kRowH * 2.f + S(2.f);
			}

			// ── LE CHAMP, puis le bouton ───────────────────────────────────────
			const float32 bw = S(74.f);
			const NkRect champ{r.x, yy, r.w - bw - pad, kRowH};
			p.Outline(champ, NkRole::Border, NkRole::InputBg, 3.f);
			if (guiCtx) {
				editorkit::NkOverlayTextField(*guiCtx, guiCtx->dl, p.FontPtr(), champ,
											  st.aiSaisie, (int32)sizeof(st.aiSaisie) - 1, true);
			} else {
				// Repli nomme (pas de contexte) : on AFFICHE, on n'edite pas. Un
				// champ muet qui a l'air editable est pire qu'un champ eteint.
				p.TextV(champ.x + S(4.f), yy, kRowH, st.aiSaisie, NkRole::TextMuted);
			}
			const NkRect bt{r.x + r.w - bw, yy, bw, kRowH};
			const bool over = hit.Add("ai.envoyer", bt);
			const bool actif = !NkAiVide(st.aiSaisie);
			// Le bouton dit lui-meme s'il fera quelque chose : eteint quand le
			// champ est vide. C'est le ZERO, rendu visible avant d'etre clique.
			p.Fill(bt, actif ? (over ? NkRole::AccentUi : NkRole::PanelHeader) : NkRole::InputBg,
				   3.f);
			p.TextV(bt.x + (bw - p.TextW("Envoyer")) * 0.5f, yy, kRowH, "Envoyer",
					actif ? NkRole::Text : NkRole::TextMuted);
			if (hit.Clicked("ai.envoyer")) {
				if (NkAiSoumettre(st, st.aiSaisie))
					st.aiSaisie[0] = 0; // le champ se vide : la demande est PARTIE
			}
			yy += kRowH + pad;
		}

	} // namespace nk3d
} // namespace nkentseu
