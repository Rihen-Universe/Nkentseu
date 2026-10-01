#pragma once
// -----------------------------------------------------------------------------
// @File    Engine/NKEditorKit/src/NKEditorKit/NkAiReglagesVue.h
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @Brief   LA FENETRE DES REGLAGES D'UN FOURNISSEUR DE MODELES : genre, nom,
//          adresse, modele (liste remplie PAR LE SERVEUR), cle, temperature,
//          contexte, outils ; « Actualiser », « Tester la connexion »,
//          « Enregistrer ». Partagee : Unkeny d'abord, NKCraft et NKCode ensuite.
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// MEME CONTRAT QUE NkAiPanneau.h : CETTE VUE N'APPELLE AUCUN SERVICE.
//   Elle TIENT les champs, les peint, et DIT ce que l'utilisateur a voulu
//   (`NkAiReglagesSorties`). L'hote execute -- lister, tester, enregistrer --
//   avec la couche des fournisseurs (Kernel/System/NKConverse,
//   NkConverseFournisseurs.h), et rend ses verdicts par `message` / `ton`.
//   C'est ce qui permet au kit de NE PAS dependre de NKNetwork : une
//   application qui n'a pas d'IA n'en tire rien.
//
// ⚠️ LA CLE (Rihen, 01/10 : « cle saisie par l'utilisateur et rangee hors du
//    depot »). Le panneau du kit disait « aucune cle n'est saisie ici ». C'est
//    toujours vrai du PANNEAU ; cette fenetre-ci, elle, en recoit une :
//    - elle ne l'AFFICHE jamais : des points pendant la frappe, et pour une cle
//      deja rangee, le masque que l'hote fournit (`cleInfo` : « sk-a…9f2c ») ;
//    - elle ne l'ECRIT nulle part : `cleSaisie` part a l'hote, qui la range
//      dans le dossier de l'UTILISATEUR (NkIaEcrireCle), jamais dans un projet.
// -----------------------------------------------------------------------------

#include "NKEditorKit/NkEditorTextField.h"
#include "NKGui/NKGui.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/String/NkString.h"

#include <cstdio>
#include <cstring>

namespace nkentseu {
	namespace editorkit {

		/// Les couleurs, LUES dans le theme de l'hote : aucune n'est en dur ici.
		struct NkAiReglagesCouleurs {
				nkgui::NkColor fond{20, 20, 20, 255};
				nkgui::NkColor panneau{33, 33, 33, 255};
				nkgui::NkColor entete{43, 43, 43, 255};
				nkgui::NkColor bord{60, 60, 60, 255};
				nkgui::NkColor champ{15, 15, 15, 255};
				nkgui::NkColor bouton{50, 50, 50, 255};
				nkgui::NkColor boutonSurvol{70, 70, 70, 255};
				nkgui::NkColor texte{220, 220, 220, 255};
				nkgui::NkColor attenue{150, 150, 150, 255};
				nkgui::NkColor accent{0, 112, 224, 255};
				nkgui::NkColor surAccent{255, 255, 255, 255};
				nkgui::NkColor ok{86, 196, 108, 255};
				nkgui::NkColor echec{232, 84, 72, 255};
				nkgui::NkColor attente{240, 180, 64, 255};
		};

		/// Ce que l'utilisateur a voulu, cette image.
		struct NkAiReglagesSorties {
				bool fermer = false;
				bool enregistrer = false;
				bool tester = false;
				bool actualiser = false;
				bool utiliser = false;	///< « Utiliser ce fournisseur » : le panneau IA le prend
				bool ajouter = false;
				bool supprimer = false;
				bool effacerCle = false;
				int32 choisir = -1;		///< un autre fournisseur de la liste de gauche
				bool genreChange = false;
				bool modeleChoisi = false; ///< un modele de la liste du serveur
				bool champFocus = false;   ///< un champ a le clavier : l'hote tait ses raccourcis
				bool clicPris = false;	   ///< le clic est a la fenetre : rien dessous ne le recoit
		};

		/// L'etat de la fenetre : l'hote declare les listes et les verdicts, la
		/// vue tient les champs.
		struct NkAiReglagesVue {
				// ════════ CE QUE L'HOTE DECLARE ════════
				NkVector<NkString> fournisseurs; ///< les noms, liste de gauche
				int32 choisi = 0;
				int32 actif = -1;			  ///< celui que le panneau utilise (pastille verte)
				NkVector<NkString> genres;	  ///< les genres (Ollama, OpenAI...)
				int32 genre = 0;
				NkVector<NkString> modeles;	  ///< LA LISTE DU SERVEUR (jamais en dur)
				NkVector<NkString> modelesDetail;
				NkVector<NkString> modesOutils; ///< « Auto », « Natifs », « Texte »
				int32 modeOutils = 0;
				NkString aide;				  ///< une phrase sur le genre choisi
				NkString cleInfo;			  ///< la cle RANGEE, masquee, et d'ou elle vient
				bool cleUtile = true;		  ///< le genre a-t-il une cle ?
				bool adresseUtile = true;	  ///< le genre a-t-il une adresse ?
				NkString message;			  ///< le dernier verdict (test, liste, enregistrement)
				uint8 ton = 0;				  ///< 0 neutre, 1 ok, 2 echec, 3 en cours
				bool occupe = false;		  ///< une sonde tourne : Tester / Actualiser attendent
				NkString titre = NkString("Fournisseurs de modèles (IA)");

				// ════════ LES CHAMPS ════════
				char nom[96] = {};
				char adresse[256] = {};
				char modele[160] = {};
				char temperature[16] = {};
				char contexte[16] = {};
				char sortie[16] = {};
				/// LA CLE TAPEE, en memoire seulement. Vide = « inchangee ».
				NkString cleSaisie;

				// ════════ CE QUE LA VUE PUBLIE (pour les bancs et les captures) ════════
				nkgui::NkRect rect{0.f, 0.f, 0.f, 0.f};
				nkgui::NkRect btnTester{0.f, 0.f, 0.f, 0.f};
				nkgui::NkRect btnActualiser{0.f, 0.f, 0.f, 0.f};
				nkgui::NkRect btnEnregistrer{0.f, 0.f, 0.f, 0.f};
				nkgui::NkRect btnUtiliser{0.f, 0.f, 0.f, 0.f};
				nkgui::NkRect btnFermer{0.f, 0.f, 0.f, 0.f};
				nkgui::NkRect champModele{0.f, 0.f, 0.f, 0.f};
				nkgui::NkRect champCle{0.f, 0.f, 0.f, 0.f};
				/// -1 aucun ; 0 nom, 1 adresse, 2 modele, 3 temperature, 4 contexte,
				/// 5 sortie, 6 cle.
				int32 focus = -1;
				/// La liste deroulante ouverte : 0 aucune, 1 genre, 2 modele, 3 outils.
				int32 liste = 0;
				float32 listeDefile = 0.f;

				void PoserChamp(char *dst, usize cap, const char *src) {
					usize i = 0;
					for (; src && src[i] && i + 1 < cap; ++i)
						dst[i] = src[i];
					dst[i] = 0;
				}
				void OuvrirListeModeles() {
					liste = 2;
					listeDefile = 0.f;
				}
		};

		namespace aireglagesdetail {
			inline bool Dans(const nkgui::NkRect &r, const nkgui::NkVec2 &p) {
				return p.x >= r.x && p.x < r.x + r.w && p.y >= r.y && p.y < r.y + r.h;
			}
			inline void Texte(nkgui::NkGuiDrawList &dl, const nkgui::NkGuiFont *f, float32 x, float32 yMilieu,
							  const char *t, const nkgui::NkColor &c) {
				if (!f || !f->Valid() || !t)
					return;
				dl.AddText(f->Face(), f->TexId(), {x, yMilieu - f->LineHeight() * 0.5f + f->Ascent()}, t, c);
			}
			/// Un texte tronque a `w` pixels (« … » au bout).
			inline void TexteBorne(nkgui::NkGuiDrawList &dl, const nkgui::NkGuiFont *f, float32 x, float32 yMilieu,
								   float32 w, const char *t, const nkgui::NkColor &c) {
				if (!f || !f->Valid() || !t)
					return;
				if (f->MeasureWidth(t) <= w) {
					Texte(dl, f, x, yMilieu, t, c);
					return;
				}
				char b[512];
				usize n = 0;
				for (; t[n] && n + 4 < sizeof(b); ++n) {
					b[n] = t[n];
					b[n + 1] = 0;
					if (f->MeasureWidth(b) > w - f->MeasureWidth("…"))
						break;
				}
				while (n > 0 && ((unsigned char)b[n] & 0xC0u) == 0x80u)
					--n;
				b[n] = 0;
				std::strcat(b, "…");
				Texte(dl, f, x, yMilieu, b, c);
			}
			/// Les lignes d'un texte enveloppe a `w` pixels, mot a mot.
			inline void Envelopper(const nkgui::NkGuiFont *f, const char *t, float32 w, NkVector<NkString> &out) {
				out.Clear();
				if (!f || !f->Valid() || !t)
					return;
				NkString ligne;
				const char *p = t;
				while (*p) {
					const char *m = p;
					while (*m && *m != ' ' && *m != '\n')
						++m;
					NkString mot(p, static_cast<NkString::SizeType>(m - p));
					NkString essai = ligne.Empty() ? mot : ligne + " " + mot;
					if (!ligne.Empty() && f->MeasureWidth(essai.CStr()) > w) {
						out.PushBack(ligne);
						ligne = mot;
					} else
						ligne = essai;
					if (*m == '\n') {
						out.PushBack(ligne);
						ligne = NkString();
					}
					p = *m ? m + 1 : m;
				}
				if (!ligne.Empty())
					out.PushBack(ligne);
			}
			inline bool Bouton(nkgui::NkGuiContext &ctx, nkgui::NkGuiDrawList &dl, const nkgui::NkGuiFont *f,
							   const nkgui::NkRect &r, const char *t, const NkAiReglagesCouleurs &c, bool actif = true,
							   bool principal = false) {
				const bool survol = actif && Dans(r, ctx.input.mousePos);
				nkgui::NkColor fond = principal ? c.accent : (survol ? c.boutonSurvol : c.bouton);
				if (principal && survol)
					fond = nkgui::NkColor{static_cast<uint8>(c.accent.r + (255 - c.accent.r) / 6),
										  static_cast<uint8>(c.accent.g + (255 - c.accent.g) / 6),
										  static_cast<uint8>(c.accent.b + (255 - c.accent.b) / 6), 255};
				if (!actif)
					fond = c.panneau;
				dl.AddRectFilled(r, fond, 3.f);
				dl.AddRect(r, c.bord, 1.f);
				if (f && f->Valid()) {
					const float32 tw = f->MeasureWidth(t);
					Texte(dl, f, r.x + (r.w - tw) * 0.5f, r.y + r.h * 0.5f, t,
						  actif ? (principal ? c.surAccent : c.texte) : c.attenue);
				}
				if (survol && ctx.input.mouseClicked[0]) {
					ctx.input.mouseClicked[0] = false;
					return true;
				}
				return false;
			}
		} // namespace aireglagesdetail

		/// LA FENETRE. `ecran` : ou la centrer. `police` : le texte. Se peint
		/// dans `dl` (l'hote passe sa couche du dessus).
		inline NkAiReglagesSorties NkAiDessinerReglages(nkgui::NkGuiContext &ctx, nkgui::NkGuiDrawList &dl,
														const nkgui::NkGuiFont *police, NkAiReglagesVue &v,
														const nkgui::NkRect &ecran, const NkAiReglagesCouleurs &c) {
			using namespace aireglagesdetail;
			NkAiReglagesSorties out;
			nkgui::NkGuiInput &in = ctx.input;
			const float32 W = ecran.w - 40.f < 760.f ? ecran.w - 40.f : 760.f;
			const float32 H = ecran.h - 40.f < 520.f ? ecran.h - 40.f : 520.f;
			const nkgui::NkRect r{ecran.x + (ecran.w - W) * 0.5f, ecran.y + (ecran.h - H) * 0.5f, W, H};
			v.rect = r;
			const bool dansFenetre = Dans(r, in.mousePos);
			// L'ombre, le cadre, l'en-tete.
			dl.AddRectFilled({r.x + 4.f, r.y + 6.f, r.w, r.h}, nkgui::NkColor{0, 0, 0, 110}, 6.f);
			dl.AddRectFilled(r, c.panneau, 5.f);
			dl.AddRect(r, c.bord, 1.f);
			const nkgui::NkRect entete{r.x, r.y, r.w, 32.f};
			dl.AddRectFilled(entete, c.entete, 5.f);
			Texte(dl, police, r.x + 12.f, entete.y + 16.f, v.titre.CStr(), c.texte);
			v.btnFermer = {r.x + r.w - 30.f, r.y + 4.f, 24.f, 24.f};
			if (Bouton(ctx, dl, police, v.btnFermer, "✕", c))
				out.fermer = true;

			// ── LA LISTE DES FOURNISSEURS, a gauche ──
			const float32 gauche = 220.f;
			const nkgui::NkRect liste{r.x + 8.f, r.y + 40.f, gauche - 8.f, r.h - 92.f};
			dl.AddRectFilled(liste, c.fond, 3.f);
			for (usize i = 0; i < v.fournisseurs.Size(); ++i) {
				const nkgui::NkRect l{liste.x + 2.f, liste.y + 2.f + static_cast<float32>(i) * 28.f, liste.w - 4.f, 26.f};
				if (l.y + l.h > liste.y + liste.h)
					break;
				const bool choisi = static_cast<int32>(i) == v.choisi;
				const bool survol = v.liste == 0 && Dans(l, in.mousePos);
				if (choisi || survol)
					dl.AddRectFilled(l, choisi ? c.accent : c.boutonSurvol, 3.f);
				if (static_cast<int32>(i) == v.actif)
					dl.AddCircleFilled({l.x + 9.f, l.y + l.h * 0.5f}, 3.5f, c.ok);
				TexteBorne(dl, police, l.x + 18.f, l.y + l.h * 0.5f, l.w - 22.f, v.fournisseurs[i].CStr(),
						   choisi ? c.surAccent : c.texte);
				if (survol && in.mouseClicked[0] && !choisi) {
					in.mouseClicked[0] = false;
					out.choisir = static_cast<int32>(i);
				}
			}
			const float32 yBas = r.y + r.h - 44.f;
			if (Bouton(ctx, dl, police, {liste.x, yBas, 100.f, 28.f}, "+ Ajouter", c))
				out.ajouter = true;
			if (Bouton(ctx, dl, police, {liste.x + 106.f, yBas, liste.w - 106.f, 28.f}, "Supprimer", c,
					   v.fournisseurs.Size() > 1u))
				out.supprimer = true;

			// ── LE FORMULAIRE, a droite ──
			const float32 x0 = r.x + gauche + 16.f;
			const float32 xChamp = x0 + 112.f;
			const float32 wChamp = r.x + r.w - 16.f - xChamp;
			float32 y = r.y + 44.f;
			const float32 hL = 26.f, pas = 32.f;
			auto Etiquette = [&](const char *t) { Texte(dl, police, x0, y + hL * 0.5f, t, c.attenue); };
			nkgui::NkRect ancreGenre{xChamp, y, wChamp, hL}, ancreModele{0, 0, 0, 0}, ancreOutils{0, 0, 0, 0};
			bool clicChamp = false;
			auto Champ = [&](int32 id, const nkgui::NkRect &cr, char *buf, int32 cap, bool actif) {
				if (!actif) {
					dl.AddRectFilled(cr, c.panneau, 3.f);
					dl.AddRect(cr, c.bord, 1.f);
					TexteBorne(dl, police, cr.x + 8.f, cr.y + cr.h * 0.5f, cr.w - 12.f, buf[0] ? buf : "(sans objet)",
							   c.attenue);
					return;
				}
				if (v.liste == 0 && in.mouseClicked[0] && Dans(cr, in.mousePos)) {
					v.focus = id;
					clicChamp = true;
				}
				NkOverlayFieldStyle st;
				st.utf8 = true;
				st.texte = c.texte;
				NkOverlayTextField(ctx, dl, police, cr, buf, cap, v.focus == id, &st);
			};
			// Genre
			Etiquette("Genre");
			{
				const char *g = (v.genre >= 0 && v.genre < static_cast<int32>(v.genres.Size())) ? v.genres[(usize)v.genre].CStr() : "?";
				if (Bouton(ctx, dl, police, ancreGenre, "", c))
					v.liste = v.liste == 1 ? 0 : 1;
				TexteBorne(dl, police, ancreGenre.x + 8.f, ancreGenre.y + hL * 0.5f, ancreGenre.w - 28.f, g, c.texte);
				Texte(dl, police, ancreGenre.x + ancreGenre.w - 16.f, ancreGenre.y + hL * 0.5f, "▾", c.attenue);
			}
			y += pas;
			Etiquette("Nom");
			Champ(0, {xChamp, y, wChamp, hL}, v.nom, (int32)sizeof(v.nom), true);
			y += pas;
			Etiquette("Adresse");
			Champ(1, {xChamp, y, wChamp, hL}, v.adresse, (int32)sizeof(v.adresse), v.adresseUtile);
			y += pas;
			Etiquette("Modèle");
			{
				const float32 wBtn = 104.f;
				const nkgui::NkRect cm{xChamp, y, wChamp - wBtn - 34.f, hL};
				v.champModele = cm;
				Champ(2, cm, v.modele, (int32)sizeof(v.modele), true);
				ancreModele = {cm.x + cm.w + 4.f, y, 26.f, hL};
				if (Bouton(ctx, dl, police, ancreModele, "▾", c, v.modeles.Size() > 0u))
					v.liste = v.liste == 2 ? 0 : 2;
				v.btnActualiser = {ancreModele.x + ancreModele.w + 4.f, y, wBtn, hL};
				if (Bouton(ctx, dl, police, v.btnActualiser, v.occupe ? "…" : "Actualiser", c, !v.occupe))
					out.actualiser = true;
			}
			y += pas;
			Etiquette("Clé");
			{
				const nkgui::NkRect ck{xChamp, y, wChamp - 84.f, hL};
				v.champCle = ck;
				if (!v.cleUtile) {
					dl.AddRectFilled(ck, c.panneau, 3.f);
					dl.AddRect(ck, c.bord, 1.f);
					Texte(dl, police, ck.x + 8.f, ck.y + hL * 0.5f, "(aucune clé pour ce genre)", c.attenue);
				} else {
					if (v.liste == 0 && in.mouseClicked[0] && Dans(ck, in.mousePos)) {
						v.focus = 6;
						clicChamp = true;
					}
					const bool f = v.focus == 6;
					dl.AddRectFilled(ck, c.champ, 3.f);
					dl.AddRect(ck, f ? c.accent : c.bord, 1.f);
					if (f) {
						// LA FRAPPE : des caracteres, l'effacement, le collage -- jamais
						// affiches en clair.
						for (int32 k = 0; k < in.charCount; ++k) {
							const uint32 cp = in.chars[k];
							if (cp >= 32u && cp < 127u)
								v.cleSaisie.Append(static_cast<char>(cp));
						}
						in.charCount = 0;
						if (in.KeyPressedRepeat(nkgui::NkGuiKey::Backspace) && !v.cleSaisie.Empty())
							v.cleSaisie.PopBack();
						if (in.wantPaste) {
							NkString cb = ctx.GetClipboard();
							cb.Trim();
							v.cleSaisie.Append(cb);
							in.wantPaste = false;
						}
					}
					NkString vu;
					if (!v.cleSaisie.Empty()) {
						const usize n = v.cleSaisie.Length() < 28u ? v.cleSaisie.Length() : 28u;
						for (usize k = 0; k < n; ++k)
							vu.Append("•");
						vu.Append("  (nouvelle, non enregistrée)");
					} else
						vu = v.cleInfo.Empty() ? NkString(f ? "" : "aucune clé — cliquez pour la saisir") : v.cleInfo;
					TexteBorne(dl, police, ck.x + 8.f, ck.y + hL * 0.5f, ck.w - 12.f, vu.CStr(),
							   v.cleSaisie.Empty() && v.cleInfo.Empty() ? c.attenue : c.texte);
					if (f && (static_cast<int32>(ctx.time * 2.f) & 1) == 0) {
						// Le curseur apres les POINTS (jamais apres un texte en clair).
						NkString points;
						const usize n = v.cleSaisie.Length() < 28u ? v.cleSaisie.Length() : 28u;
						for (usize k = 0; k < n; ++k)
							points.Append("•");
						const float32 cx = ck.x + 8.f + (police && police->Valid() ? police->MeasureWidth(points.CStr()) : 0.f);
						dl.AddRectFilled({cx, ck.y + 5.f, 1.f, hL - 10.f}, c.texte);
					}
					if (Bouton(ctx, dl, police, {ck.x + ck.w + 4.f, y, 80.f, hL}, "Effacer", c,
							   !v.cleInfo.Empty() || !v.cleSaisie.Empty())) {
						v.cleSaisie = NkString();
						out.effacerCle = true;
					}
				}
			}
			y += pas;
			Etiquette("Température");
			Champ(3, {xChamp, y, 64.f, hL}, v.temperature, (int32)sizeof(v.temperature), true);
			Texte(dl, police, xChamp + 76.f, y + hL * 0.5f, "Contexte", c.attenue);
			Champ(4, {xChamp + 146.f, y, 78.f, hL}, v.contexte, (int32)sizeof(v.contexte), true);
			Texte(dl, police, xChamp + 236.f, y + hL * 0.5f, "Réponse max", c.attenue);
			Champ(5, {xChamp + 326.f, y, wChamp - 326.f > 60.f ? 78.f : wChamp - 326.f, hL}, v.sortie, (int32)sizeof(v.sortie), true);
			y += pas;
			Etiquette("Outils");
			ancreOutils = {xChamp, y, 200.f, hL};
			{
				const char *o = (v.modeOutils >= 0 && v.modeOutils < static_cast<int32>(v.modesOutils.Size()))
									? v.modesOutils[(usize)v.modeOutils].CStr()
									: "?";
				if (Bouton(ctx, dl, police, ancreOutils, "", c))
					v.liste = v.liste == 3 ? 0 : 3;
				TexteBorne(dl, police, ancreOutils.x + 8.f, y + hL * 0.5f, ancreOutils.w - 28.f, o, c.texte);
				Texte(dl, police, ancreOutils.x + ancreOutils.w - 16.f, y + hL * 0.5f, "▾", c.attenue);
			}
			y += pas + 4.f;
			// L'aide du genre, enveloppee.
			{
				NkVector<NkString> lignes;
				Envelopper(police, v.aide.CStr(), r.x + r.w - 16.f - x0, lignes);
				for (usize i = 0; i < lignes.Size() && i < 3u; ++i) {
					Texte(dl, police, x0, y + 9.f, lignes[i].CStr(), c.attenue);
					y += 18.f;
				}
			}
			y += 8.f;
			v.btnTester = {x0, y, 180.f, 28.f};
			if (Bouton(ctx, dl, police, v.btnTester, v.occupe ? "Test en cours…" : "Tester la connexion", c, !v.occupe))
				out.tester = true;
			y += 36.f;
			// LE VERDICT : une pastille de couleur et la phrase (le geste d'abord).
			if (!v.message.Empty()) {
				const nkgui::NkColor tc = v.ton == 1 ? c.ok : (v.ton == 2 ? c.echec : (v.ton == 3 ? c.attente : c.attenue));
				dl.AddCircleFilled({x0 + 6.f, y + 9.f}, 5.f, tc);
				NkVector<NkString> lignes;
				Envelopper(police, v.message.CStr(), r.x + r.w - 16.f - (x0 + 18.f), lignes);
				for (usize i = 0; i < lignes.Size() && i < 4u; ++i) {
					Texte(dl, police, x0 + 18.f, y + 9.f, lignes[i].CStr(), v.ton == 2 ? c.echec : c.texte);
					y += 18.f;
				}
			}
			v.btnEnregistrer = {r.x + r.w - 136.f, yBas, 124.f, 28.f};
			v.btnUtiliser = {r.x + r.w - 136.f - 196.f, yBas, 188.f, 28.f};
			if (Bouton(ctx, dl, police, v.btnUtiliser, "Utiliser ce fournisseur", c, v.choisi != v.actif))
				out.utiliser = true;
			if (Bouton(ctx, dl, police, v.btnEnregistrer, "Enregistrer", c, true, true))
				out.enregistrer = true;

			// ── LA LISTE DEROULANTE OUVERTE, par-dessus ──
			if (v.liste != 0) {
				const NkVector<NkString> *items = v.liste == 1 ? &v.genres : (v.liste == 2 ? &v.modeles : &v.modesOutils);
				const NkVector<NkString> *details = v.liste == 2 ? &v.modelesDetail : nullptr;
				const nkgui::NkRect ancre = v.liste == 1 ? ancreGenre : (v.liste == 2 ? v.champModele : ancreOutils);
				const float32 hLigne = details ? 40.f : 26.f;
				const usize n = items->Size();
				const usize visibles = n < 7u ? n : 7u;
				const nkgui::NkRect lr{ancre.x, ancre.y + ancre.h + 2.f, v.liste == 2 ? wChamp - 34.f : ancre.w,
									   static_cast<float32>(visibles) * hLigne + 4.f};
				dl.AddRectFilled({lr.x + 3.f, lr.y + 4.f, lr.w, lr.h}, nkgui::NkColor{0, 0, 0, 90}, 4.f);
				dl.AddRectFilled(lr, c.fond, 3.f);
				dl.AddRect(lr, c.accent, 1.f);
				if (Dans(lr, in.mousePos) && in.wheel != 0.f) {
					v.listeDefile -= in.wheel;
					in.wheel = 0.f;
				}
				const float32 maxDef = n > visibles ? static_cast<float32>(n - visibles) : 0.f;
				if (v.listeDefile < 0.f)
					v.listeDefile = 0.f;
				if (v.listeDefile > maxDef)
					v.listeDefile = maxDef;
				const usize debut = static_cast<usize>(v.listeDefile);
				const int32 sel = v.liste == 1 ? v.genre : (v.liste == 3 ? v.modeOutils : -1);
				for (usize k = 0; k < visibles; ++k) {
					const usize i = debut + k;
					if (i >= n)
						break;
					const nkgui::NkRect l{lr.x + 2.f, lr.y + 2.f + static_cast<float32>(k) * hLigne, lr.w - 4.f, hLigne};
					const bool survol = Dans(l, in.mousePos);
					const bool estSel = static_cast<int32>(i) == sel || (v.liste == 2 && (*items)[i] == v.modele);
					if (survol || estSel)
						dl.AddRectFilled(l, estSel ? c.accent : c.boutonSurvol, 3.f);
					if (details && i < details->Size()) {
						TexteBorne(dl, police, l.x + 8.f, l.y + 12.f, l.w - 12.f, (*items)[i].CStr(), estSel ? c.surAccent : c.texte);
						TexteBorne(dl, police, l.x + 8.f, l.y + 29.f, l.w - 12.f, (*details)[i].CStr(), estSel ? c.surAccent : c.attenue);
					} else
						TexteBorne(dl, police, l.x + 8.f, l.y + l.h * 0.5f, l.w - 12.f, (*items)[i].CStr(), estSel ? c.surAccent : c.texte);
					if (survol && in.mouseClicked[0]) {
						in.mouseClicked[0] = false;
						if (v.liste == 1) {
							out.genreChange = v.genre != static_cast<int32>(i);
							v.genre = static_cast<int32>(i);
						} else if (v.liste == 2) {
							v.PoserChamp(v.modele, sizeof(v.modele), (*items)[i].CStr());
							out.modeleChoisi = true;
						} else
							v.modeOutils = static_cast<int32>(i);
						v.liste = 0;
					}
				}
				if (in.mouseClicked[0] && !Dans(lr, in.mousePos)) {
					v.liste = 0;
					in.mouseClicked[0] = false;
				}
			}
			// Un clic dans la fenetre hors d'un champ rend le clavier.
			if (in.mouseClicked[0] && dansFenetre && !clicChamp)
				v.focus = -1;
			if (in.KeyPressed(nkgui::NkGuiKey::Escape)) {
				if (v.liste != 0)
					v.liste = 0;
				else if (v.focus >= 0)
					v.focus = -1;
				else
					out.fermer = true;
			}
			out.champFocus = v.focus >= 0;
			out.clicPris = dansFenetre;
			if (dansFenetre)
				in.mouseClicked[0] = false; // rien sous la fenetre ne recoit ce clic
			return out;
		}

	} // namespace editorkit
} // namespace nkentseu
