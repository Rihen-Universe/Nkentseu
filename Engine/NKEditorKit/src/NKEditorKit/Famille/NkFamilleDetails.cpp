// -----------------------------------------------------------------------------
// @File    NkFamilleDetails.cpp
// @Brief   Le panneau Details d'Unreal 5 de la famille (voir NkFamilleDetails.h).
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
// -----------------------------------------------------------------------------

#include "NKEditorKit/Famille/NkFamilleDetails.h"
#include "NKEditorKit/Famille/NkFamillePlacer.h" // NkFamilleContientPlie
#include "NKEditorKit/NkEditorTextField.h"
#include "NKGui/Widgets/NkGuiWidgets.h"

#include <cmath>
#include <cstdio>
#include <cstring>

namespace nkentseu {
	namespace editorkit {

		using nkgui::NkColor;
		using nkgui::NkGuiContext;
		using nkgui::NkRect;
		using nkgui::NkVec2;

		namespace {
			constexpr float32 RANG_H = 24.f;   ///< une rangee : le RYTHME d'Unreal, constant
			constexpr float32 ENTETE_H = 26.f; ///< l'en-tete d'une carte
			constexpr float32 LIGNE_ARBRE = 22.f;

			void Espace(NkGuiContext &ctx, float32 h) {
				(void)ctx.NextItemRect(0.f, h);
			}
		} // namespace

		// =====================================================================
		// LES ICONES
		// =====================================================================
		void NkFamillePeindreIconeCarte(nkgui::NkGuiDrawList &dl, NkFamilleIconeCarte carte, const NkRect &r,
										const NkColor &col) noexcept {
			const float32 cx = r.x + r.w * 0.5f;
			const float32 cy = r.y + r.h * 0.5f;
			auto P = [](float32 x, float32 y) { return NkVec2{x, y}; };
			switch (carte) {
				case NkFamilleIconeCarte::Transform:
					dl.AddLine(P(cx - 5.f, cy + 5.f), P(cx + 6.f, cy + 5.f), NkColor{215, 85, 85, 255}, 1.6f);
					dl.AddLine(P(cx - 5.f, cy + 5.f), P(cx - 5.f, cy - 6.f), NkColor{95, 195, 105, 255}, 1.6f);
					dl.AddCircleFilled(P(cx - 5.f, cy + 5.f), 1.8f, col);
					break;
				case NkFamilleIconeCarte::Sprite:
					dl.AddRect(NkRect{cx - 6.f, cy - 5.f, 12.f, 10.f}, col, 1.2f);
					dl.AddTriangleFilled(P(cx - 5.f, cy + 4.f), P(cx - 1.f, cy - 1.f), P(cx + 2.f, cy + 4.f), col);
					dl.AddCircleFilled(P(cx + 3.f, cy - 2.f), 1.5f, col);
					break;
				case NkFamilleIconeCarte::Collisionneur:
					for (int32 k = 0; k < 4; ++k) {
						const float32 t = -6.f + 4.f * static_cast<float32>(k);
						dl.AddLine(P(cx + t, cy - 6.f), P(cx + t + 2.f, cy - 6.f), col, 1.2f);
						dl.AddLine(P(cx + t, cy + 6.f), P(cx + t + 2.f, cy + 6.f), col, 1.2f);
						dl.AddLine(P(cx - 6.f, cy + t), P(cx - 6.f, cy + t + 2.f), col, 1.2f);
						dl.AddLine(P(cx + 6.f, cy + t), P(cx + 6.f, cy + t + 2.f), col, 1.2f);
					}
					break;
				case NkFamilleIconeCarte::Corps:
					dl.AddCircleFilled(P(cx, cy - 1.5f), 4.2f, col);
					dl.AddLine(P(cx, cy + 3.f), P(cx, cy + 7.f), col, 1.2f);
					dl.AddTriangleFilled(P(cx - 2.2f, cy + 5.f), P(cx + 2.2f, cy + 5.f), P(cx, cy + 7.5f), col);
					break;
				case NkFamilleIconeCarte::CorpsMou:
					dl.AddCircleFilled(P(cx - 1.f, cy + 1.f), 4.2f, col);
					dl.AddCircleFilled(P(cx + 2.6f, cy - 2.f), 2.6f, col);
					break;
				case NkFamilleIconeCarte::Son:
					dl.AddRectFilled(NkRect{cx - 6.f, cy - 2.f, 3.f, 4.f}, col);
					dl.AddTriangleFilled(P(cx - 3.f, cy - 2.f), P(cx + 1.f, cy - 6.f), P(cx + 1.f, cy + 6.f), col);
					dl.AddTriangleFilled(P(cx - 3.f, cy - 2.f), P(cx + 1.f, cy + 6.f), P(cx - 3.f, cy + 2.f), col);
					dl.AddLine(P(cx + 3.5f, cy - 3.f), P(cx + 3.5f, cy + 3.f), col, 1.2f);
					dl.AddLine(P(cx + 6.f, cy - 5.f), P(cx + 6.f, cy + 5.f), col, 1.2f);
					break;
				case NkFamilleIconeCarte::Animation:
					dl.AddRect(NkRect{cx - 6.f, cy - 5.f, 12.f, 10.f}, col, 1.2f);
					for (int32 k = 0; k < 3; ++k) {
						const float32 x = cx - 4.5f + 3.6f * static_cast<float32>(k);
						dl.AddRectFilled(NkRect{x, cy - 3.8f, 1.6f, 1.6f}, col);
						dl.AddRectFilled(NkRect{x, cy + 2.2f, 1.6f, 1.6f}, col);
					}
					break;
				case NkFamilleIconeCarte::Animateur:
					dl.AddCircleFilled(P(cx - 4.f, cy - 3.f), 2.6f, col);
					dl.AddCircleFilled(P(cx + 4.f, cy + 3.f), 2.6f, col);
					dl.AddLine(P(cx - 4.f, cy - 3.f), P(cx + 4.f, cy + 3.f), col, 1.2f);
					dl.AddLine(P(cx - 4.f, cy - 3.f), P(cx + 4.f, cy - 3.f), col, 1.f);
					break;
				case NkFamilleIconeCarte::Lumiere:
					for (int32 k = 0; k < 8; ++k) {
						const float32 a = 0.785398f * static_cast<float32>(k);
						dl.AddLine(P(cx + std::cos(a) * 4.f, cy + std::sin(a) * 4.f), P(cx + std::cos(a) * 7.f, cy + std::sin(a) * 7.f),
								   col, 1.2f);
					}
					dl.AddCircleFilled(P(cx, cy), 3.f, col);
					break;
				case NkFamilleIconeCarte::Emetteur: {
					const NkVec2 pts[4] = {P(cx, cy - 7.f), P(cx + 4.5f, cy + 1.f), P(cx, cy + 6.f), P(cx - 4.5f, cy + 1.f)};
					dl.AddTriangleFilled(pts[0], pts[1], pts[2], col);
					dl.AddTriangleFilled(pts[0], pts[2], pts[3], col);
					break;
				}
				case NkFamilleIconeCarte::Hierarchie:
					dl.AddRectFilled(NkRect{cx - 6.f, cy - 6.f, 5.f, 4.f}, col);
					dl.AddLine(P(cx - 3.5f, cy - 2.f), P(cx - 3.5f, cy + 4.f), col, 1.2f);
					dl.AddLine(P(cx - 3.5f, cy + 0.5f), P(cx + 1.f, cy + 0.5f), col, 1.2f);
					dl.AddLine(P(cx - 3.5f, cy + 4.f), P(cx + 1.f, cy + 4.f), col, 1.2f);
					dl.AddRectFilled(NkRect{cx + 1.f, cy - 1.f, 5.f, 3.f}, col);
					dl.AddRectFilled(NkRect{cx + 1.f, cy + 2.5f, 5.f, 3.f}, col);
					break;
				case NkFamilleIconeCarte::Camera:
					dl.AddRectFilled(NkRect{cx - 7.f, cy - 4.f, 9.f, 8.f}, col, 1.f);
					dl.AddTriangleFilled(P(cx + 2.f, cy), P(cx + 7.f, cy - 4.f), P(cx + 7.f, cy + 4.f), col);
					break;
				case NkFamilleIconeCarte::Maillage: {
					const NkRect f{cx - 6.f, cy - 3.f, 8.f, 8.f};
					dl.AddRect(f, col, 1.2f);
					dl.AddLine(P(f.x, f.y), P(f.x + 4.f, f.y - 4.f), col, 1.2f);
					dl.AddLine(P(f.x + f.w, f.y), P(f.x + f.w + 4.f, f.y - 4.f), col, 1.2f);
					dl.AddLine(P(f.x + f.w, f.y + f.h), P(f.x + f.w + 4.f, f.y + f.h - 4.f), col, 1.2f);
					dl.AddLine(P(f.x + 4.f, f.y - 4.f), P(f.x + f.w + 4.f, f.y - 4.f), col, 1.2f);
					dl.AddLine(P(f.x + f.w + 4.f, f.y - 4.f), P(f.x + f.w + 4.f, f.y + f.h - 4.f), col, 1.2f);
					break;
				}
				case NkFamilleIconeCarte::Materiau:
					dl.AddCircleFilled(P(cx, cy), 6.f, col);
					dl.AddCircleFilled(P(cx - 2.f, cy - 2.f), 2.2f, NkColor{255, 255, 255, 170});
					break;
				default:
					break;
			}
		}

		void NkFamillePeindreIconeActeur(nkgui::NkGuiDrawList &dl, const NkRect &r, const NkColor &col,
										 const NkColor &accent) noexcept {
			const NkRect b{r.x + 2.f, r.y + 2.f, r.w - 4.f, r.h - 4.f};
			dl.AddRect(b, col, 1.4f, 3.f);
			dl.AddRectFilled(NkRect{b.x + 1.f, b.y + 1.f, b.w - 2.f, (b.h - 2.f) * 0.45f}, NkFamilleMelange(col, accent, 0.6f), 2.f);
			dl.AddCircleFilled(NkVec2{b.x + b.w * 0.5f, b.y + b.h * 0.68f}, 1.8f, accent);
		}

		const char *NkFamilleNomCategorie(NkFamilleCategorie c) noexcept {
			static const char *const k[7] = {"Tout", "Général", "Acteur", "Physique", "Rendu", "Animation", "Audio"};
			const int32 i = static_cast<int32>(c);
			return i >= 0 && i < 7 ? k[i] : "";
		}

		// =====================================================================
		// L'INSPECTEUR
		// =====================================================================
		NkFamilleInspecteur::NkFamilleInspecteur(NkFamilleCtx &c, NkFamilleDetailsEtat &e, const NkRect &zone) noexcept
			: mC(c), mE(e), mZone(zone) {
			mHaut = zone.y;
			mFondEntete = NkFamilleMelange(c.pal.entete, c.pal.texte, 0.05f);
			mFondEnteteSurvol = NkFamilleMelange(c.pal.entete, c.pal.texte, 0.12f);
			mFondCorps = NkFamilleMelange(c.pal.panneau, c.pal.entete, 0.45f);
			mE.caseActif = NkRect{0.f, 0.f, 0.f, 0.f};
			mE.liseres = 0;
		}

		bool NkFamilleInspecteur::Survol(const NkRect &r) const {
			const NkVec2 p = mC.ctx.input.mousePos;
			return NkFamilleDans(r, p) && NkFamilleDans(mOuvert ? mCorps : mZone, p);
		}

		bool NkFamilleInspecteur::Clic(const NkRect &r) const {
			return Survol(r) && mC.ctx.input.mouseClicked[0];
		}

		void NkFamilleInspecteur::Vide(const char *ligne1, const char *ligne2) {
			auto &dl = mC.ctx.dl;
			const float32 lh = NkFamilleHauteurLigne(mC.police, 16.f);
			const float32 cy = mZone.y + mZone.h * 0.35f;
			NkFamilleTexteCentre(dl, mC.police, mZone.x + mZone.w * 0.5f, cy, ligne1, mC.pal.attenue);
			if (ligne2 != nullptr) {
				NkFamilleTexteCentre(dl, mC.petite, mZone.x + mZone.w * 0.5f, cy + lh + 6.f, ligne2, mC.pal.attenue);
			}
		}

		bool NkFamilleInspecteur::Entete(uint64 cle, const char *nom, bool *actif, const char *ligneType, bool ajouterOuvert,
										 NkString *nouveauNom) {
			NkGuiContext &ctx = mC.ctx;
			auto &dl = ctx.dl;
			const nkgui::NkGuiInput &in = ctx.input;
			const float32 rangeeH = 26.f;
			const float32 lhP = NkFamilleHauteurLigne(mC.police, 16.f);
			const float32 lhS = NkFamilleHauteurLigne(mC.petite, 12.f);
			const float32 y0 = mZone.y + 8.f;
			// « + Ajouter » (le plus vert d'Unreal) a droite.
			const float32 wA = NkFamilleLargeur(mC.police, "Ajouter") + 36.f;
			const NkRect ajouter{mZone.x + mZone.w - 10.f - wA, y0, wA, rangeeH};
			mE.ajouter = ajouter;
			const bool clicAjouter = NkFamilleBouton(mC, ajouter, "", ajouterOuvert, true);
			{
				const float32 px = ajouter.x + 15.f, py = ajouter.y + rangeeH * 0.5f;
				dl.AddRectFilled(NkRect{px - 5.f, py - 1.1f, 10.f, 2.2f}, mC.pal.vert, 1.f);
				dl.AddRectFilled(NkRect{px - 1.1f, py - 5.f, 2.2f, 10.f}, mC.pal.vert, 1.f);
				NkFamilleTexte(dl, mC.police, ajouter.x + 26.f, ajouter.y + (rangeeH - lhP) * 0.5f, "Ajouter",
							   ajouterOuvert ? mC.pal.surAccent : mC.pal.texte);
			}
			// L'icone du TYPE (l'acteur), comme le cube d'Unreal devant « Floor ».
			const NkRect icone{mZone.x + 10.f, y0 + (rangeeH - 18.f) * 0.5f, 18.f, 18.f};
			NkFamillePeindreIconeActeur(dl, icone, mC.pal.texte, mC.pal.accent);
			// LA CASE « ACTIVE » (GameObject.active de Unity), a gauche du nom.
			float32 xNom = icone.x + icone.w + 8.f;
			if (actif != nullptr) {
				const NkRect caseR{xNom, y0 + (rangeeH - 16.f) * 0.5f, 16.f, 16.f};
				mE.caseActif = caseR;
				const bool survol = NkFamilleDans(caseR, in.mousePos);
				dl.AddRectFilled(caseR, mC.pal.champ, 3.f);
				dl.AddRect(caseR, survol ? mC.pal.accent : mC.pal.bord, 1.f, 3.f);
				if (*actif) {
					const float32 cy = caseR.y + caseR.h * 0.5f;
					dl.AddLine(NkVec2{caseR.x + 3.f, cy}, NkVec2{caseR.x + 6.5f, cy + 3.5f}, mC.pal.accent, 2.f);
					dl.AddLine(NkVec2{caseR.x + 6.5f, cy + 3.5f}, NkVec2{caseR.x + 12.5f, cy - 3.5f}, mC.pal.accent, 2.f);
				}
				if (survol && in.mouseClicked[0]) {
					*actif = !*actif;
				}
				xNom = caseR.x + caseR.w + 8.f;
			}
			// LE NOM : un champ qui suit l'entite choisie hors saisie.
			const NkRect nomR{xNom, y0, ajouter.x - 8.f - xNom, rangeeH};
			if (mE.nomDe != cle) {
				mE.nomDe = cle;
				mE.nomFocus = false;
			}
			if (in.mouseClicked[0]) {
				mE.nomFocus = NkFamilleDans(nomR, in.mousePos);
			}
			if (mE.nomFocus && (in.KeyPressed(nkgui::NkGuiKey::Enter) || in.KeyPressed(nkgui::NkGuiKey::Escape))) {
				mE.nomFocus = false;
			}
			if (!mE.nomFocus) {
				std::snprintf(mE.nom, sizeof(mE.nom), "%s", nom != nullptr ? nom : "");
			}
			dl.AddRectFilled(nomR, mC.pal.champ, 2.f);
			dl.AddRect(nomR, mE.nomFocus ? mC.pal.accent : mC.pal.bord, 1.f, 2.f);
			NkOverlayFieldStyle st;
			st.fond = false;
			st.bord = false;
			st.texte = mC.pal.texte;
			st.utf8 = true;
			NkOverlayTextField(ctx, dl, mC.police, NkRect{nomR.x + 6.f, nomR.y, nomR.w - 8.f, nomR.h}, mE.nom,
							   static_cast<int32>(sizeof(mE.nom)), mE.nomFocus, &st);
			if (nouveauNom != nullptr && mE.nomFocus && mE.nom[0] != '\0' && (nom == nullptr || std::strcmp(mE.nom, nom) != 0)) {
				*nouveauNom = NkString(mE.nom);
			}
			if (ligneType != nullptr) {
				NkFamilleTexte(dl, mC.petite, nomR.x, y0 + rangeeH + 3.f, ligneType, mC.pal.attenue,
							   mZone.x + mZone.w - 10.f - nomR.x);
			}
			mHaut = y0 + rangeeH + 3.f + lhS + 8.f;
			return clicAjouter;
		}

		bool NkFamilleInspecteur::Debut(const char *cle, const char *nomActeur, const NkFamilleComposant *composants,
										int32 n) {
			NkGuiContext &ctx = mC.ctx;
			const NkString recherche(mE.recherche);
			mCherche = !recherche.Empty();
			mNouvelle = !(recherche == mE.rechercheVue);
			if (mNouvelle) {
				mE.cartesTrouvees = 0u;
				mE.rechercheVue = recherche;
			}
			mCorps = NkRect{mZone.x, mHaut, mZone.w, mZone.y + mZone.h - mHaut};
			if (mCorps.h < 30.f || !nkgui::BeginChild(ctx, "famille.details", mCorps, false)) {
				return false;
			}
			mOuvert = true;
			ctx.PushId(cle != nullptr ? cle : "details");
			// Les rangees d'une carte se TOUCHENT : l'espacement vertical de NKGui
			// y ouvrirait des fentes (et casserait le bord des cartes).
			mEspacement = ctx.layout.itemSpacingY;
			ctx.layout.itemSpacingY = 0.f;
			auto &dc = ctx.DL();

			// ── L'ARBRE DES COMPOSANTS (Unreal : « Floor (Instance) » puis ses
			//    composants) : un clic n'affiche que ce composant ─────────────────
			{
				bool present = mE.composant < 0;
				for (int32 k = 0; k < n; ++k) {
					present = present || composants[k].id == mE.composant;
				}
				if (!present) {
					mE.composant = -1;
				}
				const uint32 total = static_cast<uint32>(n) + 1u;
				// QUATRE lignes visibles au plus (Unreal garde l'arbre court).
				const uint32 vus = total < 4u ? total : 4u;
				const int32 maxDefil = static_cast<int32>(total - vus);
				mE.arbreDefil = mE.arbreDefil > maxDefil ? maxDefil : (mE.arbreDefil < 0 ? 0 : mE.arbreDefil);
				const NkRect r0 = ctx.NextItemRect(0.f, LIGNE_ARBRE * static_cast<float32>(vus) + 6.f + 8.f);
				const NkRect boite{r0.x + 4.f, r0.y, r0.w - 8.f, LIGNE_ARBRE * static_cast<float32>(vus) + 6.f};
				dc.AddRectFilled(boite, NkFamilleMelange(mC.pal.panneau, mC.pal.fond, 0.55f), 3.f);
				dc.AddRect(boite, mC.pal.bord, 1.f, 3.f);
				if (maxDefil > 0 && NkFamilleDans(boite, ctx.input.mousePos) && ctx.input.wheel != 0.f) {
					mE.arbreDefil -= ctx.input.wheel > 0.f ? 1 : -1;
					mE.arbreDefil = mE.arbreDefil > maxDefil ? maxDefil : (mE.arbreDefil < 0 ? 0 : mE.arbreDefil);
				}
				const float32 lhP = NkFamilleHauteurLigne(mC.police, 16.f);
				for (uint32 v = 0; v < vus; ++v) {
					const uint32 k = v + static_cast<uint32>(mE.arbreDefil);
					const NkRect ligne{boite.x + 3.f, boite.y + 3.f + static_cast<float32>(v) * LIGNE_ARBRE, boite.w - 6.f, LIGNE_ARBRE};
					const int32 id = k == 0 ? -1 : composants[k - 1].id;
					const bool choisie = id == mE.composant;
					const bool survol = Survol(ligne);
					if (choisie) {
						dc.AddRectFilled(ligne, NkFamilleMelange(mC.pal.accent, mC.pal.panneau, 0.2f), 2.f);
					} else if (survol) {
						dc.AddRectFilled(ligne, mC.pal.boutonSurvol, 2.f);
					}
					const float32 retrait = k == 0 ? 6.f : 24.f;
					const NkRect ic{ligne.x + retrait, ligne.y + (LIGNE_ARBRE - 16.f) * 0.5f, 16.f, 16.f};
					NkString libelle;
					if (k == 0) {
						NkFamillePeindreIconeActeur(dc, ic, mC.pal.texte, mC.pal.accent);
						libelle = NkString(nomActeur != nullptr ? nomActeur : "");
						libelle.Append(" (Instance)");
					} else {
						NkFamillePeindreIconeCarte(dc, composants[k - 1].icone, ic, mC.pal.texte);
						libelle = NkString(composants[k - 1].nom);
					}
					dc.PushClipRect(ligne, true);
					NkFamilleTexte(dc, mC.police, ic.x + ic.w + 7.f, ligne.y + (LIGNE_ARBRE - lhP) * 0.5f, libelle.CStr(),
								   choisie ? mC.pal.surAccent : mC.pal.texte);
					dc.PopClipRect();
					if (survol && ctx.input.mouseClicked[0]) {
						mE.composant = id;
					}
				}
				if (maxDefil > 0) {
					const float32 hb = boite.h - 6.f;
					const float32 hp = hb * static_cast<float32>(vus) / static_cast<float32>(total);
					const float32 yp = boite.y + 3.f + (hb - hp) * static_cast<float32>(mE.arbreDefil) / static_cast<float32>(maxDefil);
					dc.AddRectFilled(NkRect{boite.x + boite.w - 5.f, yp, 3.f, hp}, mC.pal.attenue, 1.5f);
				}
			}

			// ── LA RECHERCHE dans les proprietes, puis les PASTILLES de
			//    categorie (Unreal : Général, Acteur, ... et « Tout » en dernier) ──
			{
				static const int32 kOrdre[7] = {1, 2, 3, 4, 5, 6, 0};
				const float32 ph = 20.f;
				const NkRect rz = ctx.NextItemRect(0.f, 1.f); // la largeur de la zone
				int32 lignes = 1;
				{
					float32 x = rz.x + 4.f;
					for (int32 k = 0; k < 7; ++k) {
						const float32 w =
							NkFamilleLargeur(mC.petite, NkFamilleNomCategorie(static_cast<NkFamilleCategorie>(kOrdre[k]))) + 18.f;
						if (x + w > rz.x + rz.w - 4.f && x > rz.x + 4.f) {
							x = rz.x + 4.f;
							++lignes;
						}
						x += w + 4.f;
					}
				}
				const NkRect rb = ctx.NextItemRect(0.f, 24.f + 6.f + static_cast<float32>(lignes) * (ph + 4.f) + 6.f);
				float32 y = rb.y;
				const NkRect rr{rb.x + 4.f, y, rb.w - 8.f, 24.f};
				// La recherche de la famille, dans la liste de la zone defilable.
				{
					const nkgui::NkGuiInput &in = ctx.input;
					if (in.mouseClicked[0]) {
						mE.rechercheFocus = NkFamilleDans(rr, in.mousePos) && NkFamilleDans(mCorps, in.mousePos);
					}
					if (mE.rechercheFocus && in.KeyPressed(nkgui::NkGuiKey::Escape)) {
						mE.recherche[0] = '\0';
						mE.rechercheFocus = false;
					} else if (mE.rechercheFocus && in.KeyPressed(nkgui::NkGuiKey::Enter)) {
						mE.rechercheFocus = false;
					}
					dc.AddRectFilled(rr, mC.pal.champ, 3.f);
					dc.AddRect(rr, mE.rechercheFocus ? mC.pal.accent : mC.pal.bord, 1.f, 3.f);
					const float32 lx = rr.x + 12.f, ly = rr.y + rr.h * 0.5f - 1.f;
					dc.AddCircle(NkVec2{lx, ly}, 4.5f, mC.pal.attenue, 1.3f);
					dc.AddLine(NkVec2{lx + 3.2f, ly + 3.2f}, NkVec2{lx + 6.5f, ly + 6.5f}, mC.pal.attenue, 1.6f);
					if (mE.recherche[0] == '\0' && !mE.rechercheFocus) {
						NkFamilleTexte(dc, mC.petite, rr.x + 24.f, rr.y + (rr.h - NkFamilleHauteurLigne(mC.petite, 12.f)) * 0.5f,
									   "Rechercher une propriété", mC.pal.attenue);
					}
					NkOverlayFieldStyle st;
					st.fond = false;
					st.bord = false;
					st.texte = mC.pal.texte;
					st.utf8 = true;
					NkOverlayTextField(ctx, dc, mC.police, NkRect{rr.x + 20.f, rr.y, rr.w - 24.f, rr.h}, mE.recherche,
									   static_cast<int32>(sizeof(mE.recherche)), mE.rechercheFocus, &st);
				}
				y += rr.h + 6.f;
				float32 x = rb.x + 4.f;
				const bool dansCorps = NkFamilleDans(mCorps, ctx.input.mousePos);
				for (int32 k = 0; k < 7; ++k) {
					const int32 cat = kOrdre[k];
					const char *nomCat = NkFamilleNomCategorie(static_cast<NkFamilleCategorie>(cat));
					const float32 w = NkFamilleLargeur(mC.petite, nomCat) + 18.f;
					if (x + w > rb.x + rb.w - 4.f && x > rb.x + 4.f) {
						x = rb.x + 4.f;
						y += ph + 4.f;
					}
					const NkRect r{x, y, w, ph};
					const bool sel = mE.categorie == cat;
					if (NkFamilleBouton(mC, r, "", sel, dansCorps, &dc)) {
						mE.categorie = cat;
					}
					NkFamilleTexteDansBoite(dc, mC.petite, r, nomCat, sel ? mC.pal.surAccent : mC.pal.texte);
					x += w + 4.f;
				}
			}
			return true;
		}

		bool NkFamilleInspecteur::Repond(const char *libelle) const {
			if (!mCherche || mCarteRepond) {
				return true;
			}
			return libelle != nullptr && libelle[0] != '\0' && NkFamilleContientPlie(libelle, mE.recherche);
		}

		bool NkFamilleInspecteur::Carte(int32 id, const char *titre, NkFamilleIconeCarte icone, NkFamilleCategorie categorie,
										bool *actif, bool *menu) {
			if (!mOuvert) {
				return false;
			}
			if (menu != nullptr) {
				*menu = false;
			}
			// Les FILTRES : le composant choisi dans l'arbre, la pastille, la recherche.
			if (mE.composant >= 0 && mE.composant != id) {
				return false;
			}
			if (mE.categorie != 0 && static_cast<int32>(categorie) != mE.categorie) {
				return false;
			}
			const uint32 bitTrouve = (id >= 0 && id < 32) ? (1u << static_cast<uint32>(id)) : 0u;
			const bool nomRepond = mCherche && NkFamilleContientPlie(titre, mE.recherche);
			if (mCherche && !mNouvelle && !nomRepond && (mE.cartesTrouvees & bitTrouve) == 0u) {
				return false;
			}
			mCarte = id;
			mCarteRepond = nomRepond;

			NkGuiContext &ctx = mC.ctx;
			auto &dl = ctx.DL();
			Espace(ctx, 6.f);
			const NkRect r0 = ctx.NextItemRect(0.f, ENTETE_H);
			const NkRect r{r0.x + 4.f, r0.y, r0.w - 8.f, r0.h};
			const bool ouverte = (mE.cartesRepliees & bitTrouve) == 0u;
			const float32 cy = r.y + r.h * 0.5f;
			const NkRect rMenu{r.x + r.w - 22.f, r.y + 3.f, 18.f, r.h - 6.f};
			const NkRect rCase{r.x + 44.f, cy - 7.f, 14.f, 14.f};
			const bool surMenu = Survol(rMenu);
			const bool surCase = actif != nullptr && Survol(rCase);
			const bool survol = Survol(r);
			const NkColor fond = survol && !surMenu && !surCase ? mFondEnteteSurvol : mFondEntete;
			dl.AddRectFilled(r, fond, 4.f);
			if (ouverte) {
				// Deplie, le bas de l'en-tete se RACCORDE au corps : coins carres.
				dl.AddRectFilled(NkRect{r.x, r.y + r.h - 5.f, r.w, 5.f}, fond);
			}
			dl.AddRect(r, mC.pal.bord, 1.f, 4.f);
			const NkColor texte = (actif != nullptr && !*actif) ? mC.pal.attenue : mC.pal.texte;
			// Unreal : la categorie sur une BANDE plus claire que les rangees.
			dl.AddRectFilled(NkRect{r.x + 1.f, r.y + 1.f, r.w - 2.f, 1.f}, NkFamilleMelange(mFondEntete, mC.pal.texte, 0.10f));
			const float32 tx = r.x + 12.f;
			if (ouverte) {
				dl.AddTriangleFilled(NkVec2{tx - 4.f, cy - 2.f}, NkVec2{tx + 4.f, cy - 2.f}, NkVec2{tx, cy + 3.f}, mC.pal.attenue);
			} else {
				dl.AddTriangleFilled(NkVec2{tx - 2.f, cy - 4.f}, NkVec2{tx - 2.f, cy + 4.f}, NkVec2{tx + 3.f, cy}, mC.pal.attenue);
			}
			NkFamillePeindreIconeCarte(dl, icone, NkRect{r.x + 22.f, cy - 8.f, 16.f, 16.f}, texte);
			float32 x = r.x + 44.f;
			if (actif != nullptr) {
				dl.AddRectFilled(rCase, mC.pal.champ, 2.f);
				dl.AddRect(rCase, surCase ? mC.pal.accent : mC.pal.bord, 1.f, 2.f);
				if (*actif) {
					dl.AddLine(NkVec2{rCase.x + 3.f, cy}, NkVec2{rCase.x + 6.f, cy + 3.5f}, mC.pal.accent, 2.f);
					dl.AddLine(NkVec2{rCase.x + 6.f, cy + 3.5f}, NkVec2{rCase.x + 11.f, cy - 3.5f}, mC.pal.accent, 2.f);
				}
				x += 20.f;
			}
			const float32 ty = r.y + (r.h - NkFamilleHauteurLigne(mC.police, 16.f)) * 0.5f;
			NkFamilleTexteGras(dl, mC.police, x, ty, titre, texte);
			if (surMenu) {
				dl.AddRectFilled(rMenu, mC.pal.boutonSurvol, 2.f);
			}
			for (int32 k = -1; k <= 1; ++k) {
				dl.AddCircleFilled(NkVec2{rMenu.x + rMenu.w * 0.5f, rMenu.y + rMenu.h * 0.5f + static_cast<float32>(k) * 4.f},
								   1.4f, texte);
			}
			if (Clic(rMenu)) {
				if (menu != nullptr) {
					*menu = true;
				}
			} else if (actif != nullptr && Clic(rCase)) {
				*actif = !*actif;
			} else if (Clic(r)) {
				mE.cartesRepliees ^= bitTrouve;
			}
			if (!ouverte) {
				mCarte = -1;
				mCarteRepond = false;
			}
			return ouverte;
		}

		void NkFamilleInspecteur::FinCarte() {
			NkGuiContext &ctx = mC.ctx;
			auto &dl = ctx.DL();
			const NkRect r0 = ctx.NextItemRect(0.f, 6.f);
			const NkRect r{r0.x + 4.f, r0.y, r0.w - 8.f, r0.h};
			dl.AddRectFilled(r, mFondCorps);
			dl.AddRectFilled(NkRect{r.x, r.y, 1.f, r.h}, mC.pal.bord);
			dl.AddRectFilled(NkRect{r.x + r.w - 1.f, r.y, 1.f, r.h}, mC.pal.bord);
			dl.AddRectFilled(NkRect{r.x, r.y + r.h - 1.f, r.w, 1.f}, mC.pal.bord);
			mCarte = -1;
			mCarteRepond = false;
		}

		bool NkFamilleInspecteur::Rangee(const char *libelle, float32 h, NkRect &champ, NkRect &lib) {
			if (!Repond(libelle)) {
				champ = lib = NkRect{0.f, 0.f, 0.f, 0.f};
				return false;
			}
			if (mCarte >= 0 && mCarte < 32) {
				mE.cartesTrouvees |= 1u << static_cast<uint32>(mCarte);
			}
			NkGuiContext &ctx = mC.ctx;
			auto &dl = ctx.DL();
			const NkRect r0 = ctx.NextItemRect(0.f, h);
			const NkRect r{r0.x + 4.f, r0.y, r0.w - 8.f, r0.h};
			dl.AddRectFilled(r, mFondCorps);
			dl.AddRectFilled(NkRect{r.x, r.y, 1.f, r.h}, mC.pal.bord);
			dl.AddRectFilled(NkRect{r.x + r.w - 1.f, r.y, 1.f, r.h}, mC.pal.bord);
			// La colonne des noms ALIGNEE sur toutes les cartes, et sa CLOISON
			// fine (Unreal) : elle se tire (Fin).
			const float32 colonne = r.w * mE.colonne;
			mE.cloisonX = r.x + colonne - 4.f;
			mE.rangeeX = r.x;
			mE.rangeeW = r.w;
			dl.AddRectFilled(NkRect{r.x + colonne - 4.f, r.y, 1.f, r.h}, NkFamilleMelange(mC.pal.bord, mFondCorps, 0.35f));
			lib = NkRect{r.x + 12.f, r.y, colonne - 20.f, r.h};
			// La colonne des FLECHES DE REMISE est reservee sur TOUTES les rangees.
			champ = NkRect{r.x + colonne, r.y + 2.f, r.w - colonne - 8.f - 24.f, r.h - 4.f};
			if (libelle != nullptr && libelle[0] != '\0') {
				const float32 ty = r.y + (r.h - NkFamilleHauteurLigne(mC.police, 16.f)) * 0.5f;
				dl.PushClipRect(lib, true);
				NkFamilleTexte(dl, mC.police, lib.x, ty, libelle, mC.pal.texte);
				dl.PopClipRect();
			}
			return true;
		}

		bool NkFamilleInspecteur::Frotter(const NkRect &lib, float32 &v, float32 pas, float32 vmin, float32 vmax) {
			NkGuiContext &ctx = mC.ctx;
			const nkgui::NkGuiInput &in = ctx.input;
			const uint32 id = static_cast<uint32>(ctx.GetId("frotte"));
			const bool survol = Survol(lib);
			if (survol || mE.frotteId == id) {
				ctx.wantCursor = nkgui::NkGuiCursor::ResizeEW;
			}
			if (mE.frotteId == 0u && survol && in.mouseClicked[0]) {
				mE.frotteId = id;
				mE.frotteX = in.mousePos.x;
				return false;
			}
			if (mE.frotteId != id) {
				return false;
			}
			if (!in.mouseDown[0]) {
				mE.frotteId = 0u;
				return false;
			}
			const float32 dx = in.mousePos.x - mE.frotteX;
			mE.frotteX = in.mousePos.x;
			if (dx == 0.f) {
				return false;
			}
			const float32 k = in.shiftDown ? 10.f : (in.ctrlDown ? 0.1f : 1.f);
			v += dx * pas * k;
			v = v < vmin ? vmin : (v > vmax ? vmax : v);
			return true;
		}

		bool NkFamilleInspecteur::Remettre(const NkRect &champ, bool modifie) {
			const NkRect r{champ.x + champ.w + 4.f, champ.y, 20.f, champ.h};
			if (!modifie) {
				return false;
			}
			auto &dl = mC.ctx.DL();
			const bool clic = NkFamilleBouton(mC, r, "", false, NkFamilleDans(mCorps, mC.ctx.input.mousePos), &dl);
			// Une fleche qui revient : un arc et sa pointe.
			const float32 cx = r.x + r.w * 0.5f;
			const float32 cy = r.y + r.h * 0.5f;
			NkVec2 arc[10];
			for (int32 k = 0; k < 10; ++k) {
				const float32 t = 0.6f + 4.6f * static_cast<float32>(k) / 9.f;
				arc[k] = NkVec2{cx + 5.f * std::cos(t), cy - 5.f * std::sin(t)};
			}
			dl.AddPolyline(arc, 10, mC.pal.attenue, 1.3f, false);
			dl.AddTriangleFilled(NkVec2{arc[0].x - 3.5f, arc[0].y - 1.f}, NkVec2{arc[0].x + 1.5f, arc[0].y - 3.5f},
								 NkVec2{arc[0].x + 1.f, arc[0].y + 2.f}, mC.pal.attenue);
			return clic;
		}

		bool NkFamilleInspecteur::Nombre(const char *libelle, float32 &v, float32 pas, float32 vmin, float32 vmax) {
			NkGuiContext &ctx = mC.ctx;
			NkRect champ, lib;
			if (!Rangee(libelle, RANG_H, champ, lib)) {
				return false;
			}
			ctx.PushId(libelle);
			bool change = Frotter(lib, v, pas, vmin, vmax);
			ctx.SetNextItemRect(champ);
			change |= nkgui::DragFloat(ctx, "##v", v, pas, vmin, vmax);
			ctx.PopId();
			return change;
		}

		bool NkFamilleInspecteur::Glissiere(const char *libelle, float32 &v, float32 vmin, float32 vmax) {
			NkGuiContext &ctx = mC.ctx;
			NkRect champ, lib;
			if (!Rangee(libelle, RANG_H, champ, lib)) {
				return false;
			}
			ctx.PushId(libelle);
			bool change = Frotter(lib, v, (vmax - vmin) / 300.f, vmin, vmax);
			ctx.SetNextItemRect(champ);
			change |= nkgui::SliderFloat(ctx, "##v", v, vmin, vmax);
			ctx.PopId();
			return change;
		}

		bool NkFamilleInspecteur::Entier(const char *libelle, int32 &v, int32 vmin, int32 vmax) {
			NkGuiContext &ctx = mC.ctx;
			NkRect champ, lib;
			if (!Rangee(libelle, RANG_H, champ, lib)) {
				return false;
			}
			ctx.PushId(libelle);
			float32 f = static_cast<float32>(v);
			bool change = false;
			if (Frotter(lib, f, 0.1f, static_cast<float32>(vmin), static_cast<float32>(vmax))) {
				const int32 n = static_cast<int32>(std::floor(f + 0.5f));
				change = n != v;
				v = n;
			}
			ctx.SetNextItemRect(champ);
			change |= nkgui::DragInt(ctx, "##v", v, 0.25f, vmin, vmax);
			ctx.PopId();
			return change;
		}

		bool NkFamilleInspecteur::Case(const char *libelle, bool &v) {
			NkGuiContext &ctx = mC.ctx;
			NkRect champ, lib;
			if (!Rangee(libelle, RANG_H, champ, lib)) {
				return false;
			}
			ctx.PushId(libelle);
			ctx.SetNextItemRect(NkRect{champ.x, champ.y, champ.h, champ.h});
			const bool change = nkgui::Checkbox(ctx, "##v", v);
			ctx.PopId();
			return change;
		}

		bool NkFamilleInspecteur::Couleur(const char *libelle, float32 *rgba) {
			NkGuiContext &ctx = mC.ctx;
			NkRect champ, lib;
			if (!Rangee(libelle, RANG_H, champ, lib)) {
				return false;
			}
			ctx.PushId(libelle);
			ctx.SetNextItemRect(champ);
			const bool change = nkgui::ColorEdit4(ctx, "##v", rgba);
			ctx.PopId();
			return change;
		}

		bool NkFamilleInspecteur::Vecteur(const char *libelle, float32 *v, int32 composantes, float32 pas, bool *remise,
										  bool modifie, bool *verrou) {
			NkGuiContext &ctx = mC.ctx;
			auto &dl = ctx.DL();
			NkRect champ, lib;
			if (composantes < 1 || composantes > 3 || !Rangee(libelle, RANG_H, champ, lib)) {
				return false;
			}
			const bool clicRemise = Remettre(champ, modifie);
			if (remise != nullptr) {
				*remise = clicRemise;
			}
			if (verrou != nullptr) {
				// Le CADENAS, au bout de la colonne du nom : ferme, les proportions
				// sont gardees.
				const NkRect rv{lib.x + lib.w - 14.f, lib.y + (lib.h - 14.f) * 0.5f, 14.f, 14.f};
				const bool sur = Survol(rv);
				const NkColor cv = *verrou ? mC.pal.accent : (sur ? mC.pal.texte : mC.pal.attenue);
				dl.AddRectFilled(NkRect{rv.x + 2.f, rv.y + 6.f, 10.f, 7.f}, cv, 1.5f);
				NkVec2 anse[7];
				const float32 ox = *verrou ? 0.f : 3.f; // ouvert : l'anse s'ecarte
				for (int32 k = 0; k < 7; ++k) {
					const float32 t = 3.14159f * static_cast<float32>(k) / 6.f;
					anse[k] = NkVec2{rv.x + 7.f + ox - 3.2f * std::cos(t), rv.y + 6.f - 4.f * std::sin(t)};
				}
				dl.AddPolyline(anse, 7, cv, 1.4f, false);
				if (sur && ctx.input.mouseClicked[0]) {
					*verrou = !*verrou;
				}
			}
			static const char *const kId[3] = {"x", "y", "z"};
			const NkColor kCouleurs[3] = {NkColor{220, 95, 95, 255}, NkColor{105, 200, 115, 255}, NkColor{90, 140, 235, 255}};
			ctx.PushId(libelle);
			bool change = false;
			const float32 ecart = 4.f;
			const float32 part = (champ.w - ecart * static_cast<float32>(composantes - 1)) / static_cast<float32>(composantes);
			float32 avant[3] = {v[0], composantes > 1 ? v[1] : 0.f, composantes > 2 ? v[2] : 0.f};
			int32 touchee = -1;
			for (int32 k = 0; k < composantes; ++k) {
				const NkRect cellule{champ.x + static_cast<float32>(k) * (part + ecart), champ.y, part, champ.h};
				ctx.PushId(kId[k]);
				// Le champ PREND toute la cellule ; le LISERE d'axe se pose PAR-DESSUS
				// son bord gauche (Unreal), de toute sa hauteur, et il se TIRE.
				ctx.SetNextItemRect(cellule);
				bool c = nkgui::DragFloat(ctx, "##v", v[k], pas);
				const NkRect lisere{cellule.x, cellule.y, 4.f, cellule.h};
				dl.AddRectFilled(lisere, kCouleurs[k], 2.f);
				++mE.liseres;
				c |= Frotter(NkRect{cellule.x, cellule.y, 6.f, cellule.h}, v[k], pas, -1.0e9f, 1.0e9f);
				if (c) {
					touchee = k;
				}
				change |= c;
				ctx.PopId();
			}
			ctx.PopId();
			// Le verrou ferme : une composante changee entraine les autres en proportion.
			if (verrou != nullptr && *verrou && touchee >= 0 && std::fabs(avant[touchee]) > 1.0e-6f) {
				const float32 f = v[touchee] / avant[touchee];
				for (int32 k = 0; k < composantes; ++k) {
					if (k != touchee) {
						v[k] = avant[k] * f;
					}
				}
			}
			return change;
		}

		bool NkFamilleInspecteur::Choix(const char *libelle, const char *const *noms, int32 n, int32 &valeur) {
			NkRect champ, lib;
			if (n <= 0 || !Rangee(libelle, RANG_H, champ, lib)) {
				return false;
			}
			const float32 w = champ.w / static_cast<float32>(n);
			bool change = false;
			const bool actif = NkFamilleDans(mCorps, mC.ctx.input.mousePos);
			for (int32 i = 0; i < n; ++i) {
				const NkRect r{champ.x + static_cast<float32>(i) * w, champ.y, w - 2.f, champ.h};
				if (NkFamilleBouton(mC, r, "", i == valeur, actif, &mC.ctx.DL()) && i != valeur) {
					valeur = i;
					change = true;
				}
				NkFamilleTexteDansBoite(mC.ctx.DL(), mC.petite, r, noms[i], i == valeur ? mC.pal.surAccent : mC.pal.texte);
			}
			return change;
		}

		void NkFamilleInspecteur::Info(const char *libelle, const char *valeur) {
			NkRect champ, lib;
			if (!Rangee(libelle, RANG_H, champ, lib)) {
				return;
			}
			auto &dl = mC.ctx.DL();
			const float32 ty = champ.y + (champ.h - NkFamilleHauteurLigne(mC.police, 16.f)) * 0.5f;
			dl.PushClipRect(champ, true);
			NkFamilleTexte(dl, mC.police, champ.x + 4.f, ty, valeur, mC.pal.attenue);
			dl.PopClipRect();
		}

		void NkFamilleInspecteur::Ligne(const char *texte, bool vif) {
			NkRect champ, lib;
			if (!Rangee("", RANG_H - 4.f, champ, lib)) {
				return;
			}
			auto &dl = mC.ctx.DL();
			const float32 ty = lib.y + (lib.h - NkFamilleHauteurLigne(mC.petite, 12.f)) * 0.5f;
			NkFamilleTexte(dl, mC.petite, lib.x, ty, texte, vif ? mC.pal.texte : mC.pal.attenue);
		}

		int32 NkFamilleInspecteur::Boutons(const char *a, const char *b, bool actifA, bool actifB) {
			NkRect champ, lib;
			if (!Rangee("", RANG_H + 4.f, champ, lib)) {
				return -1;
			}
			const NkRect ligne{lib.x - 4.f, champ.y, champ.x + champ.w - lib.x + 4.f, champ.h};
			const bool dedans = NkFamilleDans(mCorps, mC.ctx.input.mousePos);
			const int32 n = b != nullptr ? 2 : 1;
			const float32 w = (ligne.w - static_cast<float32>(n - 1) * 6.f) / static_cast<float32>(n);
			int32 clique = -1;
			for (int32 k = 0; k < n; ++k) {
				const NkRect r{ligne.x + static_cast<float32>(k) * (w + 6.f), ligne.y, w, ligne.h};
				const bool actif = k == 0 ? actifA : actifB;
				if (NkFamilleBouton(mC, r, "", false, dedans && actif, &mC.ctx.DL())) {
					clique = k;
				}
				NkFamilleTexteDansBoite(mC.ctx.DL(), mC.petite, r, k == 0 ? a : b, actif ? mC.pal.texte : mC.pal.attenue);
			}
			return clique;
		}

		bool NkFamilleInspecteur::Fin(const char *ajouter, bool ajouterOuvert, NkRect *ajouterRect) {
			if (!mOuvert) {
				return false;
			}
			NkGuiContext &ctx = mC.ctx;
			bool clic = false;
			if (ajouter != nullptr) {
				Espace(ctx, 10.f);
				const NkRect r0 = ctx.NextItemRect(0.f, 28.f);
				const NkRect ajout{r0.x + r0.w * 0.12f, r0.y, r0.w * 0.76f, 26.f};
				if (ajouterRect != nullptr) {
					*ajouterRect = ajout;
				}
				clic = NkFamilleBouton(mC, ajout, "", ajouterOuvert, NkFamilleDans(mCorps, ctx.input.mousePos), &ctx.DL());
				NkFamilleTexteDansBoite(ctx.DL(), mC.police, ajout, ajouter, ajouterOuvert ? mC.pal.surAccent : mC.pal.texte);
			}
			Espace(ctx, 14.f);
			ctx.layout.itemSpacingY = mEspacement;
			ctx.PopId();
			nkgui::EndChild(ctx);
			mOuvert = false;
			// ── LA CLOISON DES NOMS, qui se TIRE (Unreal) ─────────────────────
			const nkgui::NkGuiInput &in = ctx.input;
			const float32 xc = mE.cloisonX;
			const bool sur = xc > 0.f && NkFamilleDans(mCorps, in.mousePos) && in.mousePos.x >= xc - 3.f && in.mousePos.x <= xc + 3.f;
			if (sur && in.mouseClicked[0] && mE.frotteId == 0u) {
				mE.cloisonTenue = true;
			}
			if (!in.mouseDown[0]) {
				mE.cloisonTenue = false;
			}
			if (sur || mE.cloisonTenue) {
				ctx.wantCursor = nkgui::NkGuiCursor::ResizeEW;
			}
			if (mE.cloisonTenue && mE.rangeeW > 1.f) {
				float32 f = (in.mousePos.x + 4.f - mE.rangeeX) / mE.rangeeW;
				f = f < 0.25f ? 0.25f : (f > 0.65f ? 0.65f : f);
				mE.colonne = f;
			}
			return clic;
		}

	} // namespace editorkit
} // namespace nkentseu
