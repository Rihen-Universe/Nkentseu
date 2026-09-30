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
#include "NKEditorKit/NkEditorTextField.h"
#include "NKFileSystem/NkFile.h"
#include "NKFileSystem/NkPath.h"

#include <cstdio>

namespace nkentseu {
	namespace editeur {

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

			void LancerEtape(NkEditeurConstruction &k, NkEditeurInterface &ui) {
				const NkEtapeConstruction &e = k.plan.etapes[k.etape];
				JournalDate(ui, NkString("[etape] ") + e.libelle);
				Journal(ui, NkString("  ") + e.commande);
				AuFichier(k, NkString("[etape] ") + e.commande);
				k.annonce = e.libelle + "...";
				k.processus.Environnement("JENGA_NO_IDE_CONFIG", NK_CONSTRUIRE_SANS_IDE);
				if (!k.processus.Lancer(e.commande, k.plan.dossierJeu)) {
					k.etat = NkEtatConstruction::NK_ECHOUEE;
					k.annonce = NkString("lancement impossible : ") + e.commande;
					JournalDate(ui, k.annonce);
				}
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
				AuFichier(k, lignes[i]);
			}
			if (!pret || k.plan.etapes.Empty()) {
				Echec(k, m, ui, lignes.Empty() ? NkString("rien a construire") : lignes[lignes.Size() - 1u]);
				return false;
			}
			k.etape = 0;
			k.etat = NkEtatConstruction::NK_EN_COURS;
			LancerEtape(k, ui);
			return k.etat == NkEtatConstruction::NK_EN_COURS;
		}

		void NkEditeurAvancerConstruction(NkEditeurConstruction &k, NkEditeurModele &m, NkEditeurInterface &ui) {
			if (k.etat != NkEtatConstruction::NK_EN_COURS && k.etat != NkEtatConstruction::NK_VERIFIE) {
				return;
			}
			// EnCours AVANT la recolte : le fil pousse toutes ses lignes avant de
			// se dire fini, donc une recolte faite apres un « fini » a tout.
			const bool encore = k.processus.EnCours();
			NkVector<NkString> lignes;
			k.processus.Recolter(lignes);
			for (usize i = 0; i < lignes.Size(); ++i) {
				AuFichier(k, lignes[i]);
				const NkString propre = NkLigneDeJenga(lignes[i]);
				if (!propre.Empty()) {
					Journal(ui, NkString("  ") + propre);
				}
			}
			if (encore) {
				return;
			}
			const int32 code = k.processus.Code();
			if (k.etat == NkEtatConstruction::NK_VERIFIE) {
				if (code != 0) {
					Echec(k, m, ui, NkString::Format("le jeu construit ne relit pas la scene de l'editeur (code %d)", code));
					return;
				}
				k.etat = NkEtatConstruction::NK_REUSSIE;
				k.annonce = NkString("Construit et verifie : ") + k.plan.resultat;
				JournalDate(ui, k.annonce);
				NkEditeurAnnoncer(m, "Jeu construit : voir le Journal");
				return;
			}
			if (code != 0) {
				Echec(k, m, ui, NkString::Format("echec de l'etape « %s » (code %d)", k.plan.etapes[k.etape].libelle.CStr(), code));
				return;
			}
			++k.etape;
			if (k.etape < k.plan.etapes.Size()) {
				LancerEtape(k, ui);
				return;
			}
			NkVector<NkString> fin;
			const bool range = NkAcheverConstruction(k.demande, k.plan, fin);
			for (usize i = 0; i < fin.Size(); ++i) {
				JournalDate(ui, fin[i]);
				AuFichier(k, fin[i]);
			}
			if (!range) {
				Echec(k, m, ui, fin.Empty() ? NkString("rien de produit") : fin[fin.Size() - 1u]);
				return;
			}
			if (!k.plan.verification.Empty() && k.processus.Lancer(k.plan.verification, k.plan.dossierJeu)) {
				// Le jeu produit relit SES donnees sans fenetre (temoin l1 sur le
				// vrai produit) : c'est la derniere etape.
				k.etat = NkEtatConstruction::NK_VERIFIE;
				k.annonce = NkString("le jeu construit relit ses donnees...");
				JournalDate(ui, NkString("[verifier] ") + k.plan.verification);
				return;
			}
			k.etat = NkEtatConstruction::NK_REUSSIE;
			k.annonce = NkString("Construit : ") + k.plan.resultat;
			JournalDate(ui, k.annonce);
			NkEditeurAnnoncer(m, "Jeu construit : voir le Journal");
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
			const float32 w = ui.ecran.w - 40.f < 660.f ? ui.ecran.w - 40.f : 660.f;
			const float32 h = ui.ecran.h - 40.f < 540.f ? ui.ecran.h - 40.f : 540.f;
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
					k.processus.Arreter();
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
