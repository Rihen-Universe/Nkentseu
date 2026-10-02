// -----------------------------------------------------------------------------
// @File    NkFamilleJournal.cpp
// @Brief   Le tiroir « Journal » de la famille (voir NkFamilleJournal.h).
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
// -----------------------------------------------------------------------------

#include "NKEditorKit/Famille/NkFamilleJournal.h"
#include "NKEditorKit/Famille/NkFamillePlacer.h" // NkFamilleContientPlie
#include "NKEditorKit/NkEditorTextField.h"

namespace nkentseu {
	namespace editorkit {

		using nkgui::NkColor;
		using nkgui::NkRect;
		using nkgui::NkVec2;

		namespace {
			struct NkTeintes {
					NkColor erreur, avertissement, succes, info;
			};

			NkTeintes Teintes(const NkFamilleCtx &c) {
				NkTeintes t;
				t.erreur = NkThemeUnpack(c.theme.GetOuRepli(NkRole::StatusErr, NkRole::AccentSel));
				t.avertissement = NkThemeUnpack(c.theme.GetOuRepli(NkRole::StatusWarn, NkRole::AccentSel));
				t.succes = NkThemeUnpack(c.theme.GetOuRepli(NkRole::StatusOk, NkRole::AccentUi));
				t.info = NkThemeMix(c.pal.texte, c.pal.attenue, 0.25f);
				return t;
			}

			NkColor Couleur(const NkTeintes &t, NkFamilleNiveau n) {
				switch (n) {
					case NkFamilleNiveau::Erreur:        return t.erreur;
					case NkFamilleNiveau::Avertissement: return t.avertissement;
					case NkFamilleNiveau::Succes:        return t.succes;
					default:                             return t.info;
				}
			}

			bool Montre(NkFamilleNiveau n, int32 filtre) {
				if (filtre == 1) {
					return n == NkFamilleNiveau::Avertissement;
				}
				if (filtre == 2) {
					return n == NkFamilleNiveau::Erreur;
				}
				return true;
			}
		} // namespace

		void NkFamilleJournal::Ajouter(const char *texte, NkFamilleNiveau niveau) {
			lignes.PushBack(NkString(texte != nullptr ? texte : ""));
			niveaux.PushBack(static_cast<uint8>(niveau));
			while (lignes.Size() > maximum) {
				lignes.RemoveAt(0);
				niveaux.RemoveAt(0);
			}
		}

		void NkFamilleJournal::Effacer() {
			lignes.Clear();
			niveaux.Clear();
			defil = 0.f;
		}

		void NkFamilleDessinerJournal(NkFamilleCtx &c, const NkRect &zone, NkFamilleJournal &j, float32 dt) {
			auto &dl = c.ctx.dl;
			const nkgui::NkGuiInput &in = c.ctx.input;
			const NkTeintes t = Teintes(c);
			dl.AddRectFilled(zone, c.pal.panneau);
			j.retourAge += dt;
			int32 nAvt = 0, nErr = 0;
			for (uint32 i = 0; i < j.niveaux.Size(); ++i) {
				nAvt += j.niveaux[i] == static_cast<uint8>(NkFamilleNiveau::Avertissement) ? 1 : 0;
				nErr += j.niveaux[i] == static_cast<uint8>(NkFamilleNiveau::Erreur) ? 1 : 0;
			}
			// ── La barre : puces, recherche, Copier, Effacer ──────────────────
			const float32 bh = 22.f;
			const float32 by = zone.y + 5.f;
			const float32 lp = NkFamilleHauteurLigne(c.petite, 12.f);
			float32 x = zone.x + 8.f;
			{
				const NkString lTout = NkString::Format("Tout  %d", static_cast<int>(j.lignes.Size()));
				const NkString lAvt = NkString::Format("Avertissements  %d", static_cast<int>(nAvt));
				const NkString lErr = NkString::Format("Erreurs  %d", static_cast<int>(nErr));
				const char *etiquettes[3] = {lTout.CStr(), lAvt.CStr(), lErr.CStr()};
				for (int32 f = 0; f < 3; ++f) {
					const float32 w = NkFamilleLargeur(c.petite, etiquettes[f]) + 34.f;
					const NkRect r{x, by, w, bh};
					if (NkFamilleBouton(c, r, "", j.filtre == f, true, &dl)) {
						j.filtre = f;
					}
					const NkColor pastille =
						f == 0 ? c.pal.attenue : Couleur(t, f == 1 ? NkFamilleNiveau::Avertissement : NkFamilleNiveau::Erreur);
					dl.AddCircleFilled(NkVec2{r.x + 13.f, r.y + bh * 0.5f}, 3.5f, pastille);
					NkFamilleTexte(dl, c.petite, r.x + 22.f, r.y + (bh - lp) * 0.5f, etiquettes[f],
								   j.filtre == f ? c.pal.surAccent : c.pal.texte);
					x += w + 6.f;
				}
			}
			const float32 wE = NkFamilleLargeur(c.petite, "Effacer") + 22.f;
			const float32 wC = NkFamilleLargeur(c.petite, "Copier") + 22.f;
			const NkRect rEffacer{zone.x + zone.w - 8.f - wE, by, wE, bh};
			const NkRect rCopier{rEffacer.x - 6.f - wC, by, wC, bh};
			const float32 xr = x + 6.f;
			const NkRect rr{xr, by, rCopier.x - 10.f - xr, bh};
			if (rr.w > 40.f) {
				NkFamilleRecherche(c, rr, j.recherche, static_cast<int32>(sizeof(j.recherche)), j.rechercheFocus,
								   "Rechercher dans le journal", true, c.petite);
			}
			// ── Les lignes MONTREES : la plus RECENTE en haut ─────────────────
			NkVector<uint32> montrees;
			for (int32 i = static_cast<int32>(j.lignes.Size()) - 1; i >= 0; --i) {
				const uint32 k = static_cast<uint32>(i);
				if (Montre(static_cast<NkFamilleNiveau>(j.niveaux[k]), j.filtre) &&
					NkFamilleContientPlie(j.lignes[k].CStr(), j.recherche)) {
					montrees.PushBack(k);
				}
			}
			if (NkFamilleBouton(c, rCopier, "Copier", false, !montrees.Empty(), nullptr, c.petite)) {
				NkString tout;
				for (int32 i = static_cast<int32>(montrees.Size()) - 1; i >= 0; --i) {
					tout.Append(j.lignes[montrees[static_cast<uint32>(i)]].CStr());
					tout.Append('\n');
				}
				c.ctx.SetClipboard(tout.CStr());
				j.retour = NkString::Format("%u ligne(s) copiée(s)", static_cast<unsigned>(montrees.Size()));
				j.retourAge = 0.f;
			}
			if (NkFamilleBouton(c, rEffacer, "Effacer", false, !j.lignes.Empty(), nullptr, c.petite)) {
				j.Effacer();
				montrees.Clear();
			}
			// ── La liste ──────────────────────────────────────────────────────
			const NkRect liste{zone.x, by + bh + 6.f, zone.w, zone.y + zone.h - (by + bh + 6.f)};
			const float32 lh = NkFamilleHauteurLigne(c.police, 16.f) + 3.f;
			if (NkFamilleDans(liste, in.mousePos) && in.wheel != 0.f) {
				j.defil -= in.wheel * 3.f;
			}
			const float32 maxi = static_cast<float32>(montrees.Size()) - liste.h / lh + 1.f;
			j.defil = j.defil > maxi ? maxi : j.defil;
			j.defil = j.defil < 0.f ? 0.f : j.defil;
			if (!j.retour.Empty() && j.retourAge < 2.5f) {
				NkFamilleTexteADroite(dl, c.petite, rCopier.x - 10.f, by + (bh - lp) * 0.5f, j.retour.CStr(), t.succes);
			}
			if (montrees.Empty()) {
				const char *vide = j.lignes.Empty()				? "Le journal est vide : les annonces de l'éditeur s'y inscrivent."
								   : j.recherche[0] != '\0' ? "Aucune ligne ne correspond à la recherche."
								   : j.filtre == 1			? "Aucun avertissement."
															: "Aucune erreur.";
				NkFamilleTexteCentre(dl, c.police, liste.x + liste.w * 0.5f, liste.y + liste.h * 0.4f, vide, c.pal.attenue);
				return;
			}
			dl.PushClipRect(liste, true);
			float32 y = liste.y + 2.f;
			for (uint32 i = static_cast<uint32>(j.defil); i < montrees.Size() && y < liste.y + liste.h; ++i) {
				const uint32 k = montrees[i];
				const NkFamilleNiveau n = static_cast<NkFamilleNiveau>(j.niveaux[k]);
				const NkRect bande{liste.x + 4.f, y - 1.f, liste.w - 8.f, lh};
				NkColor teinte = Couleur(t, n);
				if (n == NkFamilleNiveau::Erreur || n == NkFamilleNiveau::Avertissement) {
					NkColor fond = teinte;
					fond.a = n == NkFamilleNiveau::Erreur ? 26 : 20;
					dl.AddRectFilled(bande, fond);
					dl.AddRectFilled(NkRect{bande.x, bande.y, 2.f, bande.h}, teinte);
				} else if (n == NkFamilleNiveau::Info) {
					// La plus recente vive, les autres attenuees.
					teinte = k + 1u == j.lignes.Size() ? c.pal.texte : c.pal.attenue;
				}
				NkFamilleTexte(dl, c.police, liste.x + 10.f, y, j.lignes[k].CStr(), teinte);
				y += lh;
			}
			dl.PopClipRect();
		}

	} // namespace editorkit
} // namespace nkentseu
