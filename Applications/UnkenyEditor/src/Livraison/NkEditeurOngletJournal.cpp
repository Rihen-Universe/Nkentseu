//
// NkEditeurOngletJournal.cpp
// =============================================================================
// Description :
//   L'onglet JOURNAL de la fenetre « Construire » (voir
//   NkEditeurFenetreConstruire.h) : ce que Rihen voulait voir au lieu d'un
//   defilement de console (demande du 2026-10-01).
//
// Caracteristiques :
//   - A GAUCHE, les phases (Preparer ... Empaqueter) : en attente, en cours
//     (un arc qui tourne), faite, en echec, sautee -- chacune avec sa duree et
//     un detail (« cache trouvé (9294b70a) », « NKCanvas · 12/31 »).
//   - A DROITE, comme l'Output Log d'Unreal Engine 5 : l'etat et une barre de
//     progression (lue dans les lignes de Jenga : projets et fichiers
//     compiles / total), les filtres (tout / avertissements / erreurs) avec
//     leurs comptes, « Copier le journal », « Ouvrir le dossier », le RESUME
//     des erreurs en tete quand il y en a (« fichier:ligne: message »), puis
//     le journal ligne par ligne, horodate, colore par niveau.
//   - Les couleurs sont les ROLES du theme (StatusErr, StatusWarn, StatusOk,
//     CodeBg) et la palette de l'editeur ; les polices, celles de l'editeur.
//     Aucun caractere de cadre (╔═║) ni symbole de Jenga (✓ ✗ ⚠) n'est
//     ecrit : l'etat se dessine.
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================

#include "Livraison/NkEditeurFenetreConstruire.h"

#include "NKCanvas/App/NkCanvasTexte.h"
#include "NKEditorKit/NkThemeToGui.h"
#include "NKTime/NkChrono.h"
#include "NKWindow/Core/NkLauncher.h"

#include <cmath>

namespace nkentseu {
	namespace editeur {

		using editorkit::NkRole;
		using nkgui::NkColor;
		using nkgui::NkRect;
		using nkgui::NkVec2;

		namespace {
			struct NkTeintes {
					NkColor erreur;
					NkColor avertissement;
					NkColor succes;
					NkColor fondJournal;
					NkColor info;
			};

			NkTeintes Teintes(const NkEditeurCadre &c) {
				NkTeintes t;
				t.erreur = editorkit::NkThemeUnpack(c.theme.GetOuRepli(NkRole::StatusErr, NkRole::AccentSel));
				t.avertissement = editorkit::NkThemeUnpack(c.theme.GetOuRepli(NkRole::StatusWarn, NkRole::AccentSel));
				t.succes = editorkit::NkThemeUnpack(c.theme.GetOuRepli(NkRole::StatusOk, NkRole::AccentUi));
				t.fondJournal = editorkit::NkThemeUnpack(c.theme.GetOuRepli(NkRole::CodeBg, NkRole::InputBg));
				t.info = editorkit::NkThemeMix(c.pal.texte, c.pal.attenue, 0.25f);
				return t;
			}

			NkColor Alpha(NkColor c, uint8 a) noexcept {
				c.a = a;
				return c;
			}

			/// « 12,4 s », « 1 min 23 ».
			NkString Duree(float64 s) {
				if (s < 60.0) {
					return NkString::Format("%.1f s", s);
				}
				const int32 m = static_cast<int32>(s / 60.0);
				return NkString::Format("%d min %02d", static_cast<int>(m), static_cast<int>(s - m * 60.0));
			}

			/// « 01:23 » : l'horodatage des lignes.
			NkString Horloge(float32 s) {
				const int32 t = static_cast<int32>(s);
				return NkString::Format("%02d:%02d", static_cast<int>(t / 60), static_cast<int>(t % 60));
			}

			/// L'icone d'une phase : rond creux (attente), arc qui tourne (en
			/// cours), pastille cochee (faite), pastille barree (echec), tiret
			/// (sautee).
			void Icone(nkgui::NkGuiDrawList &dl, float32 cx, float32 cy, NkEtatPhase e, float32 temps, const NkEditeurCadre &c,
					   const NkTeintes &t) {
				const float32 r = 7.f;
				switch (e) {
					case NkEtatPhase::NK_EN_COURS: {
						dl.AddCircle(NkVec2{cx, cy}, r, Alpha(c.pal.accent, 70), 2.f);
						NkVec2 arc[20];
						const float32 debut = temps * 5.5f;
						for (int32 i = 0; i < 20; ++i) {
							const float32 a = debut + static_cast<float32>(i) * (4.4f / 19.f);
							arc[i] = NkVec2{cx + std::cos(a) * r, cy + std::sin(a) * r};
						}
						dl.AddPolyline(arc, 20, c.pal.accent, 2.f);
						break;
					}
					case NkEtatPhase::NK_FAITE:
						dl.AddCircleFilled(NkVec2{cx, cy}, r, t.succes);
						dl.AddLine(NkVec2{cx - 3.4f, cy + 0.2f}, NkVec2{cx - 0.9f, cy + 2.8f}, c.pal.fond, 1.8f);
						dl.AddLine(NkVec2{cx - 0.9f, cy + 2.8f}, NkVec2{cx + 3.6f, cy - 2.6f}, c.pal.fond, 1.8f);
						break;
					case NkEtatPhase::NK_ECHEC:
						dl.AddCircleFilled(NkVec2{cx, cy}, r, t.erreur);
						dl.AddLine(NkVec2{cx - 2.8f, cy - 2.8f}, NkVec2{cx + 2.8f, cy + 2.8f}, c.pal.fond, 1.8f);
						dl.AddLine(NkVec2{cx + 2.8f, cy - 2.8f}, NkVec2{cx - 2.8f, cy + 2.8f}, c.pal.fond, 1.8f);
						break;
					case NkEtatPhase::NK_SAUTEE:
						dl.AddLine(NkVec2{cx - 4.f, cy}, NkVec2{cx + 4.f, cy}, c.pal.attenue, 1.6f);
						break;
					default:
						dl.AddCircle(NkVec2{cx, cy}, r - 1.f, c.pal.attenue, 1.4f);
						break;
				}
			}

			/// Ce que Jenga dit de la commande en cours : « NKCanvas · 12/31 ».
			NkString Avancement(const NkJournalConstruction &j) {
				if (j.projet.Empty()) {
					return NkString();
				}
				NkString s = j.projet;
				if (j.fichiersTotal > 0) {
					s += NkString::Format(" · %d/%d fichiers", static_cast<int>(j.fichiersFaits), static_cast<int>(j.fichiersTotal));
				}
				if (j.projetsTotal > 1) {
					s += NkString::Format(" · projet %d/%d", static_cast<int>(j.projetsFaits + 1 > j.projetsTotal ? j.projetsTotal : j.projetsFaits + 1),
										  static_cast<int>(j.projetsTotal));
				}
				return s;
			}

			bool Montree(const NkLigneJournal &l, int32 filtre) noexcept {
				return NkEditeurNiveauMontre(l.niveau, filtre);
			}
		} // namespace

		// =====================================================================
		// LES PIECES DE L'OUTPUT LOG, PARTAGEES (2026-10-01, retour 7 de Rihen) :
		// cet onglet et le tiroir « Journal » de l'editeur (NkEditeurTiroir.cpp).
		// =====================================================================
		bool NkEditeurNiveauMontre(NkNiveauLigne n, int32 filtre) noexcept {
			if (filtre == 2) {
				return n == NkNiveauLigne::NK_ERREUR;
			}
			if (filtre == 1) {
				return n == NkNiveauLigne::NK_ERREUR || n == NkNiveauLigne::NK_AVERTISSEMENT;
			}
			return true;
		}

		NkColor NkEditeurCouleurNiveau(const NkEditeurCadre &c, NkNiveauLigne n) {
			const NkTeintes t = Teintes(c);
			switch (n) {
				case NkNiveauLigne::NK_ERREUR:
					return t.erreur;
				case NkNiveauLigne::NK_AVERTISSEMENT:
					return t.avertissement;
				case NkNiveauLigne::NK_SUCCES:
					return t.succes;
				case NkNiveauLigne::NK_NOTE:
					return c.pal.attenue;
				case NkNiveauLigne::NK_ETAPE:
					return c.pal.texte;
				default:
					return t.info;
			}
		}

		void NkEditeurBandeNiveau(const NkEditeurCadre &c, nkgui::NkGuiDrawList &dl, const NkRect &r, NkNiveauLigne n) {
			if (n != NkNiveauLigne::NK_ERREUR && n != NkNiveauLigne::NK_AVERTISSEMENT) {
				return;
			}
			const NkColor teinte = NkEditeurCouleurNiveau(c, n);
			dl.AddRectFilled(r, Alpha(teinte, n == NkNiveauLigne::NK_ERREUR ? 26 : 20));
			dl.AddRectFilled(NkRect{r.x, r.y, 2.f, r.h}, teinte);
		}

		float32 NkEditeurPucesNiveau(NkEditeurCadre &c, nkgui::NkGuiDrawList &dl, float32 x, float32 y, float32 h, int32 tout,
									 int32 avertissements, int32 erreurs, int32 &filtre, bool *change, NkRect *rects) {
			const float32 lp = renderer::NkTexteHauteurLigne(c.petite, 12.f);
			const NkString lTout = NkString::Format("Tout  %d", static_cast<int>(tout));
			const NkString lAvt = NkString::Format("Avertissements  %d", static_cast<int>(avertissements));
			const NkString lErr = NkString::Format("Erreurs  %d", static_cast<int>(erreurs));
			const char *etiquettes[3] = {lTout.CStr(), lAvt.CStr(), lErr.CStr()};
			for (int32 f = 0; f < 3; ++f) {
				const float32 w = renderer::NkTexteLargeur(c.petite, etiquettes[f]) + 34.f;
				const NkRect r{x, y, w, h};
				if (rects != nullptr) {
					rects[f] = r;
				}
				if (NkEditeurBouton(c, r, "", filtre == f, true, &dl)) {
					filtre = f;
					if (change != nullptr) {
						*change = true;
					}
				}
				// La pastille de couleur du niveau, puis le libelle.
				const NkColor pastille = f == 0 ? c.pal.attenue : NkEditeurCouleurNiveau(c, f == 1 ? NkNiveauLigne::NK_AVERTISSEMENT : NkNiveauLigne::NK_ERREUR);
				dl.AddCircleFilled(NkVec2{r.x + 13.f, r.y + h * 0.5f}, 3.5f, pastille);
				renderer::NkTexte(dl, c.petite, r.x + 22.f, r.y + (h - lp) * 0.5f, etiquettes[f], filtre == f ? c.pal.surAccent : c.pal.texte);
				x += w + 6.f;
			}
			return x;
		}

		void NkEditeurDessinerJournalConstruction(NkEditeurCadre &c, NkEditeurConstruction &k, const NkRect &zone) {
			auto &dl = c.ctx.dlOverlay;
			const nkgui::NkGuiInput &in = c.ctx.input;
			const NkJournalConstruction &j = k.deroulement.journal;
			const NkTeintes t = Teintes(c);
			const bool occupe = k.etat == NkEtatConstruction::NK_EN_COURS || k.etat == NkEtatConstruction::NK_VERIFIE;
			const bool echec = k.etat == NkEtatConstruction::NK_ECHOUEE;
			const bool reussi = k.etat == NkEtatConstruction::NK_REUSSIE;
			const float32 temps = c.ui.temps;
			const float64 maintenant = NkChrono::Now().ToSeconds();
			const float32 lh = renderer::NkTexteHauteurLigne(c.police, 16.f);
			const float32 lp = renderer::NkTexteHauteurLigne(c.petite, 14.f);

			// ── Les phases ─────────────────────────────────────────────────────
			const float32 largeurPhases = 248.f;
			const NkRect colonne{zone.x, zone.y, largeurPhases, zone.h};
			dl.AddRectFilled(colonne, c.pal.panneau, 3.f);
			dl.AddRect(colonne, c.pal.bord, 1.f, 3.f);
			renderer::NkTexte(dl, c.petite, colonne.x + 14.f, colonne.y + 10.f, "ÉTAPES", c.pal.attenue);
			float32 y = colonne.y + 30.f;
			const float32 hauteurPhase = lh + lp + 14.f;
			for (int32 i = 0; i < NK_NB_PHASES; ++i) {
				const NkPhaseSuivie &s = k.plan.phases[i];
				const NkRect ligne{colonne.x + 6.f, y, colonne.w - 12.f, hauteurPhase - 4.f};
				if (s.etat == NkEtatPhase::NK_EN_COURS) {
					dl.AddRectFilled(ligne, editorkit::NkThemeMix(c.pal.panneau, c.pal.accent, 0.14f), 3.f);
					dl.AddRectFilled(NkRect{ligne.x, ligne.y + 3.f, 2.f, ligne.h - 6.f}, c.pal.accent);
				} else if (s.etat == NkEtatPhase::NK_ECHEC) {
					dl.AddRectFilled(ligne, Alpha(t.erreur, 34), 3.f);
					dl.AddRectFilled(NkRect{ligne.x, ligne.y + 3.f, 2.f, ligne.h - 6.f}, t.erreur);
				}
				Icone(dl, ligne.x + 18.f, ligne.y + 6.f + lh * 0.5f, s.etat, temps, c, t);
				const bool eteinte = s.etat == NkEtatPhase::NK_ATTENTE || s.etat == NkEtatPhase::NK_SAUTEE;
				const NkColor nom = s.etat == NkEtatPhase::NK_ECHEC ? t.erreur : (eteinte ? c.pal.attenue : c.pal.texte);
				renderer::NkTexte(dl, c.police, ligne.x + 34.f, ligne.y + 5.f, NkPhaseNom(static_cast<NkPhaseConstruction>(i)), nom,
								  ligne.w - 34.f - 64.f);
				NkString duree;
				if (s.etat == NkEtatPhase::NK_EN_COURS && s.debut > 0.0) {
					duree = Duree(maintenant - s.debut);
				} else if (s.etat == NkEtatPhase::NK_FAITE || s.etat == NkEtatPhase::NK_ECHEC) {
					duree = Duree(s.duree);
				}
				if (!duree.Empty()) {
					renderer::NkTexteADroite(dl, c.petite, ligne.x + ligne.w - 8.f, ligne.y + 7.f, duree.CStr(),
											 s.etat == NkEtatPhase::NK_EN_COURS ? c.pal.texte : c.pal.attenue);
				}
				// Le detail : en cours, ce que Jenga dit ; sinon celui de la phase.
				NkString detail = s.detail;
				if (s.etat == NkEtatPhase::NK_EN_COURS &&
					(i == static_cast<int32>(NkPhaseConstruction::NK_MOTEUR) || i == static_cast<int32>(NkPhaseConstruction::NK_COMPILER))) {
					const NkString a = Avancement(j);
					if (!a.Empty()) {
						detail = a;
					}
				}
				renderer::NkTexte(dl, c.petite, ligne.x + 34.f, ligne.y + 6.f + lh, detail.CStr(),
								  s.etat == NkEtatPhase::NK_ECHEC ? Alpha(t.erreur, 210) : c.pal.attenue, ligne.w - 42.f);
				y += hauteurPhase;
			}
			// Le pied de la colonne : le moteur retenu et le temps total.
			{
				const float32 yp = colonne.y + colonne.h - lp * 2.f - 18.f;
				dl.AddRectFilled(NkRect{colonne.x + 12.f, yp - 8.f, colonne.w - 24.f, 1.f}, c.pal.bord);
				const NkString moteur = k.plan.moteur == NkModeMoteur::NK_PRECOMPILE
											? NkString("Moteur précompilé · ") + NkString(k.plan.cache.empreinte.SubStr(0, 8))
											: NkString("Moteur depuis les sources");
				renderer::NkTexte(dl, c.petite, colonne.x + 14.f, yp, moteur.CStr(), c.pal.attenue, colonne.w - 28.f);
				const NkString total = NkString("Durée totale  ") + Duree(k.deroulement.Temps());
				renderer::NkTexte(dl, c.petite, colonne.x + 14.f, yp + lp + 4.f, total.CStr(), c.pal.attenue, colonne.w - 28.f);
			}

			// ── A droite : l'etat et la barre ──────────────────────────────────
			const float32 dx = zone.x + largeurPhases + 14.f;
			const float32 dw = zone.x + zone.w - dx;
			y = zone.y + 2.f;
			NkString etat;
			NkColor teinteEtat = c.pal.texte;
			if (reussi) {
				etat = k.plan.phases[static_cast<int32>(NkPhaseConstruction::NK_VERIFIER)].etat == NkEtatPhase::NK_FAITE ? "Construit et vérifié"
																														  : "Construit";
				teinteEtat = t.succes;
			} else if (echec) {
				etat = NkString("Échec : ") + k.deroulement.annonce;
				teinteEtat = t.erreur;
			} else if (occupe) {
				etat = k.deroulement.annonce;
			} else {
				etat = NkString("Pas de construction en cours");
				teinteEtat = c.pal.attenue;
			}
			renderer::NkTexte(dl, c.police, dx, y, etat.CStr(), teinteEtat, dw - 120.f);
			const float32 progression = reussi ? 1.f : k.deroulement.Progression();
			const NkString pourcent = NkString::Format("%d %%", static_cast<int>(progression * 100.f + 0.5f));
			renderer::NkTexteADroite(dl, c.police, dx + dw, y, pourcent.CStr(), teinteEtat);
			y += lh + 6.f;
			const NkRect barre{dx, y, dw, 6.f};
			dl.AddRectFilled(barre, c.pal.champ, 3.f);
			const NkColor remplissage = echec ? t.erreur : (reussi ? t.succes : c.pal.accent);
			if (progression > 0.f) {
				dl.AddRectFilled(NkRect{barre.x, barre.y, barre.w * (progression > 1.f ? 1.f : progression), barre.h}, remplissage, 3.f);
			}
			if (occupe) {
				// Un reflet qui court sur la partie faite : la barre vit meme quand
				// Jenga se tait (le chargement du workspace, l'edition de liens).
				const float32 faite = barre.w * progression;
				const float32 px = barre.x + std::fmod(temps * 160.f, faite + 60.f) - 30.f;
				if (faite > 8.f && px > barre.x && px < barre.x + faite - 4.f) {
					dl.AddRectFilled(NkRect{px, barre.y, 24.f < barre.x + faite - px ? 24.f : barre.x + faite - px, barre.h},
									 Alpha(c.pal.surAccent, 70), 3.f);
				}
			}
			y += 10.f;
			const NkString avancement = occupe ? Avancement(j) : NkString();
			const NkString sousTitre = avancement.Empty() ? NkString("Durée ") + Duree(k.deroulement.Temps())
														  : avancement + "  ·  " + Duree(k.deroulement.Temps());
			renderer::NkTexte(dl, c.petite, dx, y, sousTitre.CStr(), c.pal.attenue, dw);
			y += lp + 10.f;

			// ── Les filtres et les actions ────────────────────────────────────
			int32 nErreurs = 0;
			int32 nAvertissements = 0;
			for (usize i = 0; i < j.lignes.Size(); ++i) {
				nErreurs += j.lignes[i].niveau == NkNiveauLigne::NK_ERREUR ? 1 : 0;
				nAvertissements += j.lignes[i].niveau == NkNiveauLigne::NK_AVERTISSEMENT ? 1 : 0;
			}
			{
				const float32 bh = lh + 8.f;
				bool change = false;
				(void)NkEditeurPucesNiveau(c, dl, dx, y, bh, static_cast<int32>(j.lignes.Size()), nAvertissements, nErreurs, k.filtre, &change);
				if (change) {
					k.suivre = true;
				}
				const char *lOuvrir = "Ouvrir le dossier";
				const char *lCopier = "Copier le journal";
				const float32 wO = renderer::NkTexteLargeur(c.petite, lOuvrir) + 22.f;
				const float32 wC = renderer::NkTexteLargeur(c.petite, lCopier) + 22.f;
				const NkRect rO{dx + dw - wO, y, wO, bh};
				const NkRect rC{rO.x - 6.f - wC, y, wC, bh};
				if (NkEditeurBouton(c, rC, lCopier, false, !j.lignes.Empty(), &dl)) {
					NkString tout;
					uint32 n = 0u;
					for (usize i = 0; i < j.lignes.Size(); ++i) {
						if (Montree(j.lignes[i], k.filtre)) {
							tout += Horloge(j.lignes[i].temps) + "  " + j.lignes[i].texte + "\n";
							++n;
						}
					}
					c.ctx.SetClipboard(tout.CStr());
					k.retour = NkString::Format("%u ligne(s) copiée(s)", static_cast<unsigned>(n));
					k.retourJusqua = temps + 2.5f;
				}
				if (NkEditeurBouton(c, rO, lOuvrir, false, !k.plan.dossierJeu.Empty(), &dl)) {
					NkLauncher::OpenFolder(k.plan.dossierJeu.CStr());
				}
				if (!k.retour.Empty() && temps < k.retourJusqua) {
					renderer::NkTexteADroite(dl, c.petite, rC.x - 10.f, y + (bh - lp) * 0.5f, k.retour.CStr(), t.succes);
				}
				y += bh + 8.f;
			}

			// ── Le resume des erreurs, en tete ────────────────────────────────
			const NkVector<NkString> resume = NkResumeErreurs(j, 5u);
			if (!resume.Empty()) {
				const float32 h = 14.f + lp + 6.f + static_cast<float32>(resume.Size()) * (lp + 3.f);
				const NkRect boite{dx, y, dw, h};
				dl.AddRectFilled(boite, Alpha(t.erreur, 28), 3.f);
				dl.AddRect(boite, Alpha(t.erreur, 120), 1.f, 3.f);
				dl.AddRectFilled(NkRect{boite.x, boite.y, 3.f, boite.h}, t.erreur);
				const NkString titre = NkString::Format("%d erreur(s) — la première d'abord", static_cast<int>(j.erreurs > 0 ? j.erreurs : nErreurs));
				renderer::NkTexte(dl, c.petite, boite.x + 14.f, boite.y + 7.f, titre.CStr(), t.erreur);
				float32 yr = boite.y + 7.f + lp + 6.f;
				for (usize i = 0; i < resume.Size(); ++i) {
					// « fichier:ligne: » en clair, le message en texte.
					const NkString &l = resume[i];
					const usize deux = l.Find(": ");
					if (deux != NkString::npos) {
						const NkString lieu(l.SubStr(0, deux + 1u));
						renderer::NkTexte(dl, c.petite, boite.x + 14.f, yr, lieu.CStr(), c.pal.texte);
						const float32 wl = renderer::NkTexteLargeur(c.petite, lieu.CStr()) + 6.f;
						renderer::NkTexte(dl, c.petite, boite.x + 14.f + wl, yr, NkString(l.SubStr(deux + 2u)).CStr(), t.info,
										  boite.w - 28.f - wl);
					} else {
						renderer::NkTexte(dl, c.petite, boite.x + 14.f, yr, l.CStr(), t.info, boite.w - 28.f);
					}
					yr += lp + 3.f;
				}
				y += h + 8.f;
			}

			// ── Le journal ─────────────────────────────────────────────────────
			const NkRect liste{dx, y, dw, zone.y + zone.h - y};
			dl.AddRectFilled(liste, t.fondJournal, 3.f);
			dl.AddRect(liste, c.pal.bord, 1.f, 3.f);
			NkVector<uint32> montrees;
			for (usize i = 0; i < j.lignes.Size(); ++i) {
				if (Montree(j.lignes[i], k.filtre)) {
					montrees.PushBack(static_cast<uint32>(i));
				}
			}
			const float32 pas = lp + 4.f;
			const int32 visibles = static_cast<int32>((liste.h - 10.f) / pas);
			const int32 n = static_cast<int32>(montrees.Size());
			const int32 dernier = n - visibles > 0 ? n - visibles : 0;
			if (in.wheel != 0.f && NkEditeurDans(liste, in.mousePos)) {
				k.defilement -= in.wheel * 3.f;
				k.suivre = false;
			}
			if (k.suivre) {
				k.defilement = static_cast<float32>(dernier);
			}
			if (k.defilement < 0.f) {
				k.defilement = 0.f;
			}
			if (k.defilement >= static_cast<float32>(dernier)) {
				k.defilement = static_cast<float32>(dernier);
				k.suivre = true;
			}
			dl.PushClipRect(liste, true);
			if (n == 0) {
				renderer::NkTexteCentre(dl, c.petite, liste.x + liste.w * 0.5f, liste.y + liste.h * 0.45f,
										k.filtre == 0 ? "Le journal est vide : « Construire » le remplit."
													  : (k.filtre == 1 ? "Aucun avertissement." : "Aucune erreur."),
										c.pal.attenue);
			}
			const int32 premiere = static_cast<int32>(k.defilement);
			float32 yl = liste.y + 5.f;
			for (int32 i = premiere; i < n && i < premiere + visibles + 1; ++i) {
				const NkLigneJournal &l = j.lignes[montrees[static_cast<usize>(i)]];
				const NkColor teinte = NkEditeurCouleurNiveau(c, l.niveau);
				NkEditeurBandeNiveau(c, dl, NkRect{liste.x + 1.f, yl - 1.f, liste.w - 2.f, pas}, l.niveau);
				renderer::NkTexte(dl, c.petite, liste.x + 10.f, yl, Horloge(l.temps).CStr(), Alpha(c.pal.attenue, 170));
				float32 xt = liste.x + 54.f;
				if (l.niveau == NkNiveauLigne::NK_ETAPE) {
					// Une phase ou une commande : un chevron d'accent la detache.
					const float32 cy = yl + lp * 0.5f;
					dl.AddTriangleFilled(NkVec2{xt, cy - 4.f}, NkVec2{xt + 6.f, cy}, NkVec2{xt, cy + 4.f}, c.pal.accent);
					xt += 12.f;
				}
				renderer::NkTexte(dl, c.petite, xt, yl, l.texte.CStr(), teinte, liste.x + liste.w - 14.f - xt);
				yl += pas;
			}
			dl.PopClipRect();
			// La glissiere, quand tout ne tient pas.
			if (n > visibles && visibles > 0) {
				const float32 hp = liste.h - 8.f;
				const float32 hb = hp * static_cast<float32>(visibles) / static_cast<float32>(n) < 18.f
									   ? 18.f
									   : hp * static_cast<float32>(visibles) / static_cast<float32>(n);
				const float32 yb = liste.y + 4.f + (hp - hb) * (dernier > 0 ? k.defilement / static_cast<float32>(dernier) : 0.f);
				dl.AddRectFilled(NkRect{liste.x + liste.w - 7.f, yb, 4.f, hb}, Alpha(c.pal.attenue, k.suivre ? 90 : 160), 2.f);
			}
		}

	} // namespace editeur
} // namespace nkentseu
