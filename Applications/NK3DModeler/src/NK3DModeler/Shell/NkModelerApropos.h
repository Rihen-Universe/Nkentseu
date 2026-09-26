#pragma once
// -----------------------------------------------------------------------------
// @File    NkModelerApropos.h
// @Brief   « A PROPOS » — la mention des tiers, LISIBLE DEPUIS L'APPLICATION.
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// 🔴 POURQUOI CET ECRAN EXISTE, ET CE N'EST PAS UN CHOIX D'INTERFACE
//   62 des 104 icones de `data/icons/` sont une copie prouvee de
//   **vscode-codicons** (Microsoft), sous **CC BY 4.0**. Cette licence EXIGE
//   l'attribution. `TIERS.md` la porte — mais :
//
//     ⚠️ UNE ATTRIBUTION QUE SEUL UN DEVELOPPEUR PEUT LIRE NE REMPLIT PAS LA
//        CONDITION. Elle doit etre ATTEIGNABLE DEPUIS L'APPLICATION.
//
//   Rodolf vend ce produit et la branche est poussee. C'est donc une obligation
//   juridique, pas un confort.
//
// ⚠️ PERSISTANT ET FERMABLE — ET C'EST LE POINT JURIDIQUE
//   La tentation etait de router « A propos » vers un bandeau de la vue : le
//   puits d'ecran existe, il aurait suffi d'un `logger.Info`. Ce serait FAUX.
//   Un bandeau s'efface en quelques secondes ; une notice de licence
//   ACCOMPAGNE l'oeuvre, elle ne passe pas. *Mieux vaut une condition
//   ouvertement non remplie qu'une conformite apparente.*
//
// ⚠️ IL LIT LE FICHIER, IL NE RECOPIE PAS SON TEXTE
//   `TIERS.md` est ENGENDRE depuis la mesure de provenance, precisement pour
//   rester vrai quand une icone naitra. Un texte recopie en dur se perimerait
//   au premier ajout — et il mentirait alors sur une licence. C'est la faute
//   que ce depot a deja payee quatre fois (les tables recopiees ailleurs :
//   combos de la sortie, `kVidExt` fige a 3, `kHdrNames` reste a six, le
//   nombre d'entrees de `kFile`).
//
// ⚠️ ET S'IL NE TROUVE PAS LE FICHIER : UN REFUS NOMME, PAS UN ECRAN VIDE
//   L'ecran dit alors CE QU'IL CHERCHAIT et OU il a regarde. Un cadre vide
//   ferait croire que la mention n'existe pas, alors qu'elle n'a pas ete
//   TROUVEE — deux choses tres differentes quand une licence en depend.
// -----------------------------------------------------------------------------

#include "NK3DModeler/Shell/NkModelerUI.h"
#include "NK3DModeler/Shell/NkModelerInput.h"
#include "NK3DModeler/Shell/NkModelerWidgets.h"
#include "NK3DModeler/NkModelerData.h"		   // NkDataFile / NkDataRoots
#include "NKEditorKit/NkEditorScrollbar.h"	   // la barre MANIPULABLE du kit
#include "NKFileSystem/NkFile.h"

namespace nkentseu {
	namespace nk3d {

		/// Le chemin, relatif, du fichier de mentions. Il voyage AVEC le produit :
		/// `TIERS.md` est depose en DEUX exemplaires identiques — a la racine du
		/// depot et dans `data/icons/` — et c'est celui-ci que l'application lit,
		/// parce que c'est celui qui suit le binaire.
		inline const char *NkAproposFichier() {
			return "data/icons/TIERS.md";
		}

		/// Le contenu, lu UNE FOIS et garde. Vide si le fichier est introuvable —
		/// l'appelant distingue alors « pas encore lu » de « absent » par le
		/// second drapeau.
		struct NkAproposTexte {
				NkString contenu;
				NkString ouCherche; ///< les racines essayees, pour le refus nomme
				bool lu = false;
				bool trouve = false;
		};

		inline NkAproposTexte &NkApropos() {
			static NkAproposTexte t;
			return t;
		}

		/// Charge a la PREMIERE ouverture, jamais avant : un fichier lu au
		/// demarrage serait relu a chaque lancement pour un ecran que presque
		/// personne n'ouvre.
		inline void NkAproposCharger() {
			NkAproposTexte &t = NkApropos();
			if (t.lu)
				return;
			t.lu = true;
			const NkString chemin = NkDataFile(NkAproposFichier());
			if (!chemin.Empty()) {
				t.contenu = NkFile::ReadAllText(chemin.CStr());
				t.trouve = !t.contenu.Empty();
			}
			if (!t.trouve) {
				// LE REFUS NOMME : ce qu'on cherchait, et les trois endroits ou
				// l'on a regarde. Sans cela, « la mention est vide » et « la
				// mention est introuvable » se ressemblent.
				NkString c[3];
				const uint32 n = NkDataRoots(NkAproposFichier(), c);
				for (uint32 i = 0; i < n; ++i) {
					if (i)
						t.ouCherche.Append("\n");
					t.ouCherche.Append(c[i].CStr());
				}
				NkLog::Instance().Warnf(
					"[nk3d] A PROPOS : mentions des tiers INTROUVABLES (%s). La notice CC BY 4.0 "
					"des icones vscode-codicons n'est donc pas atteignable depuis l'application.\n",
					NkAproposFichier());
			}
		}

		/// L'ECRAN. Peint APRES tout le reste, sur la couche des modales.
		/// Rend vrai s'il occupe l'ecran (l'appelant lui laisse alors l'entree).
		inline bool PaintApropos(NkModelerPainter &p, NkHitRegistry &hit, NkModelerState &st,
								 float32 W, float32 H) {
			if (!st.aproposOpen)
				return false;
			NkAproposCharger();
			const NkAproposTexte &t = NkApropos();

			// ── LE VOILE. Il dit que le reste est suspendu, et il RECOIT le clic :
			//    cliquer a cote ferme, comme toute boite de ce produit.
			p.Fill({0.f, 0.f, W, H}, NkColor{0, 0, 0, 150}, 0.f);
			hit.Add("apropos.voile", {0.f, 0.f, W, H});

			const float32 bw = W * 0.62f < S(520.f) ? S(520.f) : (W * 0.62f > S(900.f) ? S(900.f) : W * 0.62f);
			const float32 bh = H * 0.72f < S(320.f) ? S(320.f) : H * 0.72f;
			const NkRect box{(W - bw) * 0.5f, (H - bh) * 0.5f, bw, bh};
			p.Fill(box, NkRole::PanelBg, S(6.f));
			p.OutlineSharp(box, NkRole::Border);
			hit.Add("apropos.boite", box);

			const float32 titreH = S(38.f), pad = S(14.f), lh = p.LineH();
			p.Fill({box.x, box.y, box.w, titreH}, NkRole::PanelHeader, S(6.f));
			p.TextV(box.x + pad, box.y, titreH, "A propos — mentions des tiers");
			p.HLine(box.x, box.y + titreH, box.w);

			// LA CROIX. Un ecran qu'on ne peut pas fermer devient un meuble.
			const float32 cx = box.x + box.w - S(30.f);
			const NkRect rx{cx, box.y + S(7.f), S(24.f), S(24.f)};
			const bool ovX = hit.Add("apropos.x", rx);
			p.IconV(rx.x + S(5.f), rx.y, rx.h, NkIcon::WinClose,
					ovX ? NkRole::Text : NkRole::TextMuted);

			const float32 sbW = S(12.f);
			const NkRect zone{box.x + pad, box.y + titreH + S(8.f), box.w - pad * 2.f - sbW - S(4.f),
							  box.h - titreH - S(16.f) - pad};

			if (!t.trouve) {
				// ── LE REFUS, A L'ECRAN ────────────────────────────────────────
				// Il nomme le fichier ET les trois racines : un cadre vide ferait
				// croire que la mention n'existe pas.
				p.TextV(zone.x, zone.y, lh, "Mentions introuvables.", NkRole::StatusErr);
				p.TextV(zone.x, zone.y + lh * 1.6f, lh, "Fichier cherche :", NkRole::TextMuted);
				p.TextV(zone.x, zone.y + lh * 2.6f, lh, NkAproposFichier());
				p.TextV(zone.x, zone.y + lh * 4.0f, lh, "Racines essayees :", NkRole::TextMuted);
				float32 yy = zone.y + lh * 5.0f;
				const char *s = t.ouCherche.CStr();
				char ligne[512];
				uint32 k = 0;
				for (const char *c = s; ; ++c) {
					if (*c == '\n' || *c == '\0') {
						ligne[k] = '\0';
						if (k)
							p.TextV(zone.x, yy, lh, ligne, NkRole::TextMuted);
						yy += lh;
						k = 0;
						if (!*c)
							break;
						continue;
					}
					if (k + 1 < sizeof(ligne))
						ligne[k++] = *c;
				}
			} else {
				// ── LE TEXTE, DEFILABLE ────────────────────────────────────────
				// Decoupe en lignes a la volee : `TIERS.md` fait 83 lignes, en
				// garder une copie decoupee ferait une seconde verite a maintenir.
				p.Clip(zone);
				hit.PushClip(zone);
				float32 y = zone.y - st.aproposScroll;
				uint32 nLignes = 0;
				const char *s = t.contenu.CStr();
				char ligne[600];
				uint32 k = 0;
				for (const char *c = s; ; ++c) {
					if (*c == '\n' || *c == '\0') {
						ligne[k] = '\0';
						if (y + lh >= zone.y && y <= zone.y + zone.h)
							p.TextV(zone.x, y, lh, ligne);
						y += lh;
						++nLignes;
						k = 0;
						if (!*c)
							break;
						continue;
					}
					if (*c == '\r')
						continue;
					if (k + 1 < sizeof(ligne))
						ligne[k++] = *c;
				}
				hit.PopClip();
				p.Unclip();

				// LA BARRE, MANIPULABLE — celle du KIT, la meme que le journal.
				// En reecrire une ferait deux barres a corriger.
				const float32 contenu = (float32)nLignes * lh;
				if (nkgui::NkGuiContext *gc = NkUiCtx()) {
					const NkRect piste{zone.x + zone.w + S(4.f), zone.y, sbW, zone.h};
					editorkit::NkVScrollbar(*gc, gc->dl, piste, st.aproposScroll, contenu, zone.h,
											0xF00A1202u, lh);
					hit.Add("apropos.sb", piste);
				}
				const float32 maxSc = contenu > zone.h ? contenu - zone.h : 0.f;
				if (st.aproposScroll > maxSc)
					st.aproposScroll = maxSc;
				if (st.aproposScroll < 0.f)
					st.aproposScroll = 0.f;
			}

			// ── FERMETURE : la croix, un clic sur le voile, ou Echap ────────────
			// ⚠️ SEULEMENT SI L'ECRAN ETAIT DEJA OUVERT A L'ENTREE DE L'IMAGE.
			//    Sans cette garde, le clic qui l'OUVRE (sur « Aide -> A propos »,
			//    couche 50) le referme aussitot : le voile, declare en couche 150,
			//    gagne le survol et `hit.AnyClick()` est encore vrai -- c'est le
			//    MEME clic, rien ne l'a consomme. L'ecran naissait et mourait sans
			//    etre peint. C'est mot pour mot le defaut de la barre de menus du
			//    25/09, refait le lendemain a une couche pres.
			if (st.aproposOuvertAvantImage &&
				(hit.Clicked("apropos.x") || (hit.AnyClick() && hit.IsHovered("apropos.voile"))))
				st.aproposOpen = false;
			return true;
		}

	} // namespace nk3d
} // namespace nkentseu
