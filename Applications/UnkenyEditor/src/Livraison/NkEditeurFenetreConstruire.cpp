//
// NkEditeurFenetreConstruire.cpp
// =============================================================================
// Description :
//   La fenetre « Construire » (voir l'en-tete) et le suivi de la construction.
//
// Caracteristiques :
//   - Le Journal de l'editeur ne garde que la FIN de ce que Jenga ecrit (une
//     construction complete du moteur en ecrit des milliers de lignes) ; le
//     fichier `construire.log` du jeu garde tout, et la fenetre dit ou il est.
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================

#include "Livraison/NkEditeurFenetreConstruire.h"

#include "Editeur/NkEditeurActions.h"
#include "NKCanvas/App/NkCanvasTexte.h"
#include "NKEditorKit/NkThemeToGui.h"
#include "NKEditorKit/NkEditorTextField.h"
#include "NKFileSystem/NkFile.h"
#include "NKFileSystem/NkPath.h"

#include <cmath>
#include <cstdio>

namespace nkentseu {
	namespace editeur {

		using editorkit::NkRole;
		using nkgui::NkColor;
		using nkgui::NkRect;

		namespace {
			/// Le Journal garde les 300 dernieres lignes : au-dela, la fenetre
			/// renvoie au fichier complet.
			constexpr usize NK_JOURNAL_MAX = 300u;

			void Journal(NkEditeurInterface &ui, const NkString &ligne) {
				ui.journal.PushBack(ligne);
				while (ui.journal.Size() > NK_JOURNAL_MAX) {
					ui.journal.RemoveAt(0);
				}
			}

			/// Une ligne de l'editeur lui-meme : datee, comme les annonces.
			void JournalDate(NkEditeurInterface &ui, const NkString &ligne) {
				const int32 s = static_cast<int32>(ui.temps);
				Journal(ui, NkString::Format("[%02d:%02d]  %s", s / 60, s % 60, ligne.CStr()));
			}

			/// Au fichier `construire.log` du jeu, qui garde TOUT. Rien tant que le
			/// plan n'a pas de dossier (une preparation refusee tot).
			void AuFichier(const NkEditeurConstruction &k, const NkString &ligne) {
				if (!k.plan.journal.Empty()) {
					NkFile::AppendAllText(k.plan.journal.CStr(), (ligne + "\n").CStr());
				}
			}

			void Copier(char *dst, usize taille, const char *src) {
				std::snprintf(dst, taille, "%s", src != nullptr ? src : "");
			}

			void Echec(NkEditeurConstruction &k, NkEditeurModele &m, NkEditeurInterface &ui, const NkString &pourquoi) {
				k.etat = NkEtatConstruction::NK_ECHOUEE;
				k.annonce = pourquoi + " -- journal complet : " + k.plan.journal;
				JournalDate(ui, k.annonce);
				NkEditeurAnnoncer(m, "Construction en echec : voir le Journal");
			}

			/// Un rond de choix : plein quand il est choisi.
			void Rond(nkgui::NkGuiDrawList &dl, float32 cx, float32 cy, bool plein, const NkColor &c) {
				dl.AddCircle(nkgui::NkVec2{cx, cy}, 6.f, c, 1.4f);
				if (plein) {
					dl.AddCircleFilled(nkgui::NkVec2{cx, cy}, 3.4f, c);
				}
			}

			/// Un champ de saisie de la fenetre : cadre, focus au clic, edition.
			void Champ(NkEditeurCadre &c, NkEditeurConstruction &k, int32 indice, const NkRect &r, char *tampon, int32 taille) {
				auto &dl = c.ctx.dlOverlay;
				const nkgui::NkGuiInput &in = c.ctx.input;
				if (in.mouseClicked[0] && NkEditeurDans(r, in.mousePos)) {
					k.focus = indice;
				}
				const bool focus = k.focus == indice && k.etat != NkEtatConstruction::NK_EN_COURS;
				dl.AddRectFilled(r, c.pal.champ, 2.f);
				dl.AddRect(r, focus ? c.pal.accent : c.pal.bord, 1.f, 2.f);
				editorkit::NkOverlayFieldStyle st;
				st.fond = false;
				st.bord = false;
				st.texte = c.pal.texte;
				editorkit::NkOverlayTextField(c.ctx, dl, c.police, NkRect{r.x + 6.f, r.y, r.w - 8.f, r.h}, tampon, taille, focus, &st);
			}
			/// Le pied de l'onglet Journal : ou est le texte brut, et les boutons
			/// (Reglages, Construire de nouveau, Arreter / Fermer).
			void PiedJournal(NkEditeurCadre &c, NkEditeurConstruction &k, const NkRect &boite, float32 by, float32 bh, bool occupe) {
				auto &dl = c.ctx.dlOverlay;
				const char *lReglages = "Réglages";
				const char *lFermer = occupe ? "Arrêter" : "Fermer";
				const float32 wR = renderer::NkTexteLargeur(c.police, lReglages) + 28.f;
				const float32 wC = renderer::NkTexteLargeur(c.police, "Construire") + 28.f;
				const float32 wF = renderer::NkTexteLargeur(c.police, lFermer) + 28.f;
				const NkRect rFermer{boite.x + boite.w - 12.f - wF, by, wF, bh};
				const NkRect rConstruire{rFermer.x - 8.f - wC, by, wC, bh};
				const NkRect rReglages{rConstruire.x - 8.f - wR, by, wR, bh};
				if (!k.plan.journal.Empty()) {
					const NkString brut = NkString("Texte brut complet : ") + k.plan.journal;
					renderer::NkTexte(dl, c.petite, boite.x + 16.f, by + 6.f, brut.CStr(), c.pal.attenue, rReglages.x - 12.f - (boite.x + 16.f));
				}
				if (NkEditeurBouton(c, rReglages, lReglages, false, true, &dl)) {
					k.onglet = 0;
				}
				const bool peut = !occupe && k.dispo[static_cast<int32>(k.demande.plateforme)].disponible;
				if (NkEditeurBouton(c, rConstruire, "Construire", peut, peut, &dl) && peut) {
					NkEditeurLancerConstruction(k, c.m, c.ui);
				}
				if (NkEditeurBouton(c, rFermer, lFermer, false, true, &dl)) {
					if (occupe) {
						k.deroulement.Arreter();
					} else {
						k.ouverte = false;
					}
				}
				if (!occupe && c.ctx.input.KeyPressed(nkgui::NkGuiKey::Escape)) {
					k.ouverte = false;
				}
			}
		} // namespace

		// =====================================================================
		void NkEditeurOuvrirConstruire(NkEditeurConstruction &k, const NkEditeurModele &m) {
			if (k.nom[0] == '\0') {
				// Le nom de la scene ouverte (« niveau1.nkscene » -> niveau1), sinon
				// MonJeu : une scene jamais enregistree n'a pas de nom a proposer.
				const NkString stem = m.chemin.Empty() ? NkString() : NkPath(m.chemin.CStr()).GetFileNameWithoutExtension();
				Copier(k.nom, sizeof(k.nom), stem.Empty() || stem == NkString("scene") ? "MonJeu" : stem.CStr());
			}
			if (k.sortie[0] == '\0') {
				Copier(k.sortie, sizeof(k.sortie), NkSortieParDefaut().CStr());
			}
			NkDetecterPlateformes(k.dispo);
			// La plateforme choisie doit etre construisible : sinon la premiere
			// qui l'est (Windows d'abord), et aucune si rien ne l'est.
			if (!k.dispo[static_cast<int32>(k.demande.plateforme)].disponible) {
				for (int32 i = 0; i < NK_NB_PLATEFORMES; ++i) {
					if (k.dispo[i].disponible) {
						k.demande.plateforme = static_cast<NkPlateformeJeu>(i);
						break;
					}
				}
			}
			k.focus = -1;
			k.ouverte = true;
			if (k.etat != NkEtatConstruction::NK_EN_COURS && k.etat != NkEtatConstruction::NK_VERIFIE) {
				k.etat = NkEtatConstruction::NK_REPOS;
				k.annonce = NkString();
			}
		}

		bool NkEditeurLancerConstruction(NkEditeurConstruction &k, NkEditeurModele &m, NkEditeurInterface &ui) {
			if (k.etat == NkEtatConstruction::NK_EN_COURS || k.etat == NkEtatConstruction::NK_VERIFIE) {
				return false;
			}
			k.demande.nom = NkString(k.nom);
			k.demande.sortie = NkString(k.sortie);
			k.demande.iconeSombre = NkString(k.iconeSombre);
			k.demande.iconeClaire = NkString(k.iconeClaire);
			ui.voirTiroir = true;
			ui.ongletTiroir = 1;
			JournalDate(ui, NkString::Format("Construire « %s » pour %s (%s)", k.demande.nom.CStr(),
											 NkPlateformeJeuNom(k.demande.plateforme),
											 k.demande.profil == NkProfilJeu::NK_DEVELOPPEMENT ? "Developpement" : "Expedition"));
			NkVector<NkString> lignes;
			const bool pret = NkPreparerConstruction(m, k.demande, ui.viseur.w, ui.viseur.h, k.plan, lignes);
			for (usize i = 0; i < lignes.Size(); ++i) {
				JournalDate(ui, lignes[i]);
			}
			if (!pret || k.plan.etapes.Empty()) {
				// Le refus va AUSSI a l'onglet Journal, en rouge : on y regarde.
				k.deroulement.journal.Vider();
				for (usize i = 0; i < lignes.Size(); ++i) {
					AuFichier(k, lignes[i]);
					k.deroulement.journal.Annonce(lignes[i], NkJournalConstruction::NiveauDe(lignes[i]), 0.f);
				}
				k.lignesVues = k.deroulement.journal.lignes.Size();
				k.onglet = 1;
				Echec(k, m, ui, lignes.Empty() ? NkString("rien a construire") : lignes[lignes.Size() - 1u]);
				return false;
			}
			// Le MEME deroulement que `--construire=` : commandes, verrou et sceau
			// du moteur, rangement, verification.
			k.deroulement.Commencer(k.demande, k.plan, lignes);
			k.lignesVues = k.deroulement.journal.lignes.Size();
			k.etat = NkEtatConstruction::NK_EN_COURS;
			k.annonce = NkString("construction...");
			// La fenetre montre le Journal, colle a la derniere ligne.
			k.onglet = 1;
			k.filtre = 0;
			k.suivre = true;
			return true;
		}

		void NkEditeurAvancerConstruction(NkEditeurConstruction &k, NkEditeurModele &m, NkEditeurInterface &ui) {
			if (k.etat != NkEtatConstruction::NK_EN_COURS && k.etat != NkEtatConstruction::NK_VERIFIE) {
				return;
			}
			const bool encore = k.deroulement.Avancer();
			// Le tiroir Journal de l'editeur recoit les lignes comme avant (il ne
			// garde que la fin) ; l'onglet Journal de la fenetre a tout.
			const NkVector<NkLigneJournal> &lignes = k.deroulement.journal.lignes;
			for (; k.lignesVues < lignes.Size(); ++k.lignesVues) {
				const NkLigneJournal &l = lignes[k.lignesVues];
				if (l.niveau == NkNiveauLigne::NK_ETAPE) {
					JournalDate(ui, l.texte);
				} else {
					Journal(ui, NkString("  ") + l.texte);
				}
			}
			k.annonce = k.deroulement.annonce;
			if (encore) {
				k.etat = k.deroulement.Verification() ? NkEtatConstruction::NK_VERIFIE : NkEtatConstruction::NK_EN_COURS;
				return;
			}
			if (k.deroulement.Reussi()) {
				k.etat = NkEtatConstruction::NK_REUSSIE;
				NkEditeurAnnoncer(m, "Jeu construit : voir le Journal");
				return;
			}
			k.etat = NkEtatConstruction::NK_ECHOUEE;
			k.annonce = k.deroulement.annonce + " -- journal complet : " + k.plan.journal;
			NkEditeurAnnoncer(m, "Construction en echec : voir le Journal");
		}

		// =====================================================================
		// LA FENETRE
		// =====================================================================
		void NkEditeurDessinerConstruire(NkEditeurCadre &c, NkEditeurConstruction &k) {
			if (!k.ouverte) {
				return;
			}
			NkEditeurInterface &ui = c.ui;
			auto &dl = c.ctx.dlOverlay;
			const nkgui::NkGuiInput &in = c.ctx.input;
			const bool occupe = k.etat == NkEtatConstruction::NK_EN_COURS || k.etat == NkEtatConstruction::NK_VERIFIE;
			// Un clic hors des champs leur retire le focus (reposé par Champ).
			if (in.mouseClicked[0]) {
				k.focus = -1;
			}

			dl.AddRectFilled(ui.ecran, NkColor{0, 0, 0, 120});
			// Le Journal (2026-10-01) veut de la place : les phases, la barre et
			// des lignes de compilateur entieres.
			const float32 wMax = k.onglet == 1 ? 1100.f : 660.f;
			const float32 hMax = k.onglet == 1 ? 720.f : 620.f;
			const float32 w = ui.ecran.w - 40.f < wMax ? ui.ecran.w - 40.f : wMax;
			const float32 h = ui.ecran.h - 40.f < hMax ? ui.ecran.h - 40.f : hMax;
			const NkRect boite{(ui.ecran.w - w) * 0.5f, (ui.ecran.h - h) * 0.45f, w, h};
			dl.AddRectFilled(NkRect{boite.x + 4.f, boite.y + 6.f, boite.w, boite.h}, NkColor{0, 0, 0, 110}, 3.f);
			dl.AddRectFilled(boite, c.pal.entete, 3.f);
			dl.AddRect(boite, c.pal.bord, 1.f, 3.f);
			dl.AddRectFilled(NkRect{boite.x, boite.y, boite.w, 2.f}, c.pal.accent);

			const float32 lh = renderer::NkTexteHauteurLigne(c.police, 16.f);
			const float32 gauche = boite.x + 16.f;
			const float32 colonne = boite.x + 150.f;
			const float32 largeurChamp = boite.x + boite.w - 16.f - colonne;
			float32 y = boite.y + 14.f;
			renderer::NkTexte(dl, c.police, gauche, y, "Construire le jeu", c.pal.texte);
			y += lh + 2.f;
			renderer::NkTexte(dl, c.petite, gauche, y,
							  "Le joueur autonome d'Unkeny et la scène cuite, compilés par Jenga.", c.pal.attenue, boite.w - 32.f);
			y += lh + 10.f;

			// ── Les onglets (2026-10-01) : Reglages, Journal ──────────────────
			{
				static const char *kOnglets[2] = {"Réglages", "Journal"};
				const float32 hb = lh + 10.f;
				dl.AddRectFilled(NkRect{boite.x + 1.f, y + hb - 1.f, boite.w - 2.f, 1.f}, c.pal.bord);
				float32 x = gauche;
				for (int32 o = 0; o < 2; ++o) {
					const float32 wo = renderer::NkTexteLargeur(c.police, kOnglets[o]) + 28.f + (o == 1 ? 16.f : 0.f);
					const NkRect r{x, y, wo, hb};
					const bool actif = k.onglet == o;
					const bool survol = NkEditeurDans(r, in.mousePos);
					if (actif) {
						dl.AddRectFilled(r, c.pal.panneau, 3.f);
						dl.AddRectFilled(NkRect{r.x, r.y, r.w, 2.f}, c.pal.accent);
					} else if (survol) {
						dl.AddRectFilled(r, c.pal.boutonSurvol, 3.f);
					}
					renderer::NkTexte(dl, c.police, r.x + 14.f, r.y + 5.f, kOnglets[o], actif ? c.pal.texte : c.pal.attenue);
					if (o == 1 && k.etat != NkEtatConstruction::NK_REPOS) {
						// La pastille du Journal dit l'etat sans l'ouvrir : bleu qui
						// bat (en cours), rouge (echec), vert (construit).
						NkColor p = c.pal.accent;
						if (k.etat == NkEtatConstruction::NK_ECHOUEE) {
							p = editorkit::NkThemeUnpack(c.theme.GetOuRepli(NkRole::StatusErr, NkRole::AccentSel));
						} else if (k.etat == NkEtatConstruction::NK_REUSSIE) {
							p = editorkit::NkThemeUnpack(c.theme.GetOuRepli(NkRole::StatusOk, NkRole::AccentUi));
						} else {
							p.a = static_cast<uint8>(150.f + 105.f * std::sin(ui.temps * 5.f));
						}
						dl.AddCircleFilled(nkgui::NkVec2{r.x + r.w - 14.f, r.y + hb * 0.5f}, 4.f, p);
					}
					if (in.mouseClicked[0] && survol) {
						k.onglet = o;
					}
					x += wo + 4.f;
				}
				y += hb + 12.f;
			}
			if (k.onglet == 1) {
				const float32 bh = 26.f;
				const float32 by = boite.y + boite.h - bh - 12.f;
				NkEditeurDessinerJournalConstruction(c, k, NkRect{gauche, y, boite.w - 32.f, by - 12.f - y});
				PiedJournal(c, k, boite, by, bh, occupe);
				return;
			}

			// ── La plateforme : TOUTES, chacune avec sa raison ──────────────────
			renderer::NkTexte(dl, c.police, gauche, y, "Plateforme", c.pal.texte);
			for (int32 i = 0; i < NK_NB_PLATEFORMES; ++i) {
				const NkPlateformeJeu p = static_cast<NkPlateformeJeu>(i);
				const NkRect ligne{colonne, y - 2.f, largeurChamp, lh + 4.f};
				const bool choisie = k.demande.plateforme == p;
				const bool dispo = k.dispo[i].disponible;
				if (!occupe && dispo && in.mouseClicked[0] && NkEditeurDans(ligne, in.mousePos)) {
					k.demande.plateforme = p;
				}
				const NkColor teinte = dispo ? c.pal.texte : c.pal.attenue;
				Rond(dl, colonne + 7.f, y + lh * 0.5f, choisie && dispo, dispo ? c.pal.accent : c.pal.attenue);
				renderer::NkTexte(dl, c.police, colonne + 20.f, y, NkPlateformeJeuNom(p), teinte);
				const NkColor raison = dispo ? c.pal.attenue : NkColor{215, 125, 110, 255};
				renderer::NkTexte(dl, c.petite, colonne + 110.f, y + 1.f, k.dispo[i].raison.CStr(), raison, largeurChamp - 114.f);
				y += lh + 5.f;
			}
			y += 6.f;

			// ── Le profil ────────────────────────────────────────────────────
			renderer::NkTexte(dl, c.police, gauche, y + 3.f, "Profil", c.pal.texte);
			{
				const float32 bw = 130.f;
				const NkRect rDev{colonne, y, bw, lh + 8.f};
				const NkRect rExp{colonne + bw + 8.f, y, bw, lh + 8.f};
				const bool dev = k.demande.profil == NkProfilJeu::NK_DEVELOPPEMENT;
				if (NkEditeurBouton(c, rDev, "Développement", dev, !occupe, &dl)) {
					k.demande.profil = NkProfilJeu::NK_DEVELOPPEMENT;
				}
				if (NkEditeurBouton(c, rExp, "Expédition", !dev, !occupe, &dl)) {
					k.demande.profil = NkProfilJeu::NK_EXPEDITION;
				}
				renderer::NkTexte(dl, c.petite, rExp.x + bw + 10.f, y + 4.f,
								  dev ? "Debug, l'exécutable seul" : "Release, et le paquet (zip, apk...)", c.pal.attenue,
								  boite.x + boite.w - 16.f - (rExp.x + bw + 10.f));
			}
			y += lh + 16.f;

			// ── Le moteur (2026-10-01) : precompile, ou depuis les sources ────
			renderer::NkTexte(dl, c.police, gauche, y + 3.f, "Moteur", c.pal.texte);
			{
				const float32 bw = 130.f;
				const NkRect rKit{colonne, y, bw, lh + 8.f};
				const NkRect rSrc{colonne + bw + 8.f, y, bw, lh + 8.f};
				const bool kit = k.demande.moteur == NkModeMoteur::NK_PRECOMPILE;
				if (NkEditeurBouton(c, rKit, "Précompilé", kit, !occupe, &dl)) {
					k.demande.moteur = NkModeMoteur::NK_PRECOMPILE;
				}
				if (NkEditeurBouton(c, rSrc, "Sources", !kit, !occupe, &dl)) {
					k.demande.moteur = NkModeMoteur::NK_SOURCES;
				}
				renderer::NkTexte(dl, c.petite, rSrc.x + bw + 10.f, y + 4.f,
								  kit ? "compilé une fois, partagé par tous les jeux" : "recompilé dans le dossier du jeu",
								  c.pal.attenue, boite.x + boite.w - 16.f - (rSrc.x + bw + 10.f));
			}
			y += lh + 16.f;

			// ── Le nom ───────────────────────────────────────────────────────
			renderer::NkTexte(dl, c.police, gauche, y + 3.f, "Nom du jeu", c.pal.texte);
			Champ(c, k, 0, NkRect{colonne, y, 260.f, lh + 8.f}, k.nom, static_cast<int32>(sizeof(k.nom)));
			const NkString id = NkIdentifiantJeu(k.nom);
			renderer::NkTexte(dl, c.petite, colonne + 270.f, y + 4.f, (NkString("exécutable : ") + id).CStr(), c.pal.attenue);
			y += lh + 14.f;

			// ── L'icone, sombre ET claire (R17) ──────────────────────────────
			renderer::NkTexte(dl, c.police, gauche, y + 3.f, "Icône sombre", c.pal.texte);
			Champ(c, k, 2, NkRect{colonne, y, largeurChamp, lh + 8.f}, k.iconeSombre, static_cast<int32>(sizeof(k.iconeSombre)));
			y += lh + 12.f;
			renderer::NkTexte(dl, c.police, gauche, y + 3.f, "Icône claire", c.pal.texte);
			Champ(c, k, 3, NkRect{colonne, y, largeurChamp, lh + 8.f}, k.iconeClaire, static_cast<int32>(sizeof(k.iconeClaire)));
			y += lh + 10.f;
			{
				renderer::NkTexte(dl, c.petite, colonne, y + 4.f, "L'exécutable porte :", c.pal.attenue);
				const float32 bx = colonne + renderer::NkTexteLargeur(c.petite, "L'exécutable porte :") + 10.f;
				const NkRect rS{bx, y, 80.f, lh + 6.f};
				const NkRect rC{bx + 86.f, y, 80.f, lh + 6.f};
				if (NkEditeurBouton(c, rS, "la sombre", k.demande.executableSombre, !occupe, &dl)) {
					k.demande.executableSombre = true;
				}
				if (NkEditeurBouton(c, rC, "la claire", !k.demande.executableSombre, !occupe, &dl)) {
					k.demande.executableSombre = false;
				}
				renderer::NkTexte(dl, c.petite, rC.x + 90.f, y + 4.f, "l'autre est livrée à côté. Vides : Unkeny fabrique les deux.",
								  c.pal.attenue, boite.x + boite.w - 16.f - (rC.x + 90.f));
			}
			y += lh + 16.f;

			// ── La sortie ────────────────────────────────────────────────────
			renderer::NkTexte(dl, c.police, gauche, y + 3.f, "Dossier de sortie", c.pal.texte);
			Champ(c, k, 1, NkRect{colonne, y, largeurChamp, lh + 8.f}, k.sortie, static_cast<int32>(sizeof(k.sortie)));
			y += lh + 10.f;
			const NkString ou = NkString("le jeu ira dans : ") + k.sortie + "/" + id + "/";
			renderer::NkTexte(dl, c.petite, colonne, y, ou.CStr(), c.pal.attenue, largeurChamp);
			y += lh + 10.f;

			// ── L'etat, et les boutons ───────────────────────────────────────
			if (!k.annonce.Empty()) {
				NkColor teinte = c.pal.texte;
				if (k.etat == NkEtatConstruction::NK_ECHOUEE) {
					teinte = NkColor{230, 120, 110, 255};
				} else if (k.etat == NkEtatConstruction::NK_REUSSIE) {
					teinte = NkColor{110, 205, 130, 255};
				}
				renderer::NkTexte(dl, c.petite, gauche, y, k.annonce.CStr(), teinte, boite.w - 32.f);
			}
			const float32 bh = 26.f;
			const float32 by = boite.y + boite.h - bh - 12.f;
			const float32 wC = renderer::NkTexteLargeur(c.police, "Construire") + 28.f;
			const float32 wF = renderer::NkTexteLargeur(c.police, occupe ? "Arrêter" : "Fermer") + 28.f;
			const NkRect rFermer{boite.x + boite.w - 12.f - wF, by, wF, bh};
			const NkRect rConstruire{rFermer.x - 8.f - wC, by, wC, bh};
			const bool peut = !occupe && k.dispo[static_cast<int32>(k.demande.plateforme)].disponible;
			bool presse = false;
			if (k.lancementAuto > 0) {
				--k.lancementAuto;
				presse = k.lancementAuto == 0;
			}
			if ((NkEditeurBouton(c, rConstruire, "Construire", true, peut, &dl) || presse) && peut) {
				k.focus = -1;
				NkEditeurLancerConstruction(k, c.m, ui);
			}
			if (NkEditeurBouton(c, rFermer, occupe ? "Arrêter" : "Fermer", false, true, &dl)) {
				if (occupe) {
					k.deroulement.Arreter();
				} else {
					k.ouverte = false;
				}
			}
			if (!occupe && k.focus < 0 && in.KeyPressed(nkgui::NkGuiKey::Escape)) {
				k.ouverte = false;
			}
		}

	} // namespace editeur
} // namespace nkentseu
