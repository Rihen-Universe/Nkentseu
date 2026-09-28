#pragma once
// -----------------------------------------------------------------------------
// @File    PanneauCatalogue.h
// @Brief   LA BIBLIOTHÈQUE DES COMPOSANTS `.nkgui` — par famille, par catégorie,
//          et l'extrait se copie.
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// =============================================================================
//  POURQUOI CE PANNEAU EXISTE
// =============================================================================
//  Rodolf, 28/09 : « ce que tu fais avec les icônes doit être fait avec les
//  composants, et classé par catégorie — widget, conteneur, fenêtre — et
//  sous-catégorie : bouton, vbox, hbox, onglet, etc. »
//
//  Même raison que pour les icônes : un vocabulaire qu'on ne peut pas VOIR
//  n'est utilisable que par celui qui l'a écrit. Ici il y a 54 rôles.
//
// =============================================================================
//  ⚠️ IL LIT LE CATALOGUE DU NOYAU, IL N'EN TIENT PAS UN SECOND
// =============================================================================
//  `NkGuiCatalogue()` vit dans NKGui parce que les rôles ne sont pas ceux de
//  NKUIDesign : ce sont ceux du FORMAT. Une liste écrite ici serait fausse au
//  premier rôle ajouté au monteur — et rien ne le dirait.
//
//  Et le catalogue lui-même se vérifie DANS LES DEUX SENS
//  (`NkGuiCatalogueVerifier`, mesuré par `--sonde-catalogue`) : tout nom doit se
//  résoudre, et tout rôle doit avoir son entrée.
//
// ⚠️ IL COPIE, IL NE POSE PAS. Comme la bibliothèque d'icônes : poser un rôle
//    sur la toile demanderait que `NkUINode` sache le tenir, ce qu'il ne fait
//    pas encore pour la plupart. Copier l'extrait est ce qui est VRAI
//    aujourd'hui. **CONDITION DE RETRAIT :** le jour où le modèle porte les
//    rôles du format, le clic posera l'élément.
// -----------------------------------------------------------------------------

#include "NKGui/Doc/NkGuiCatalogue.h" // le catalogue des roles : la VERITE vit dans le noyau

namespace nkuidesign {

	class PanneauCatalogue : public nkentseu::editorkit::NkEditorPanel {
		public:
			explicit PanneauCatalogue(DesignState &st) noexcept
				: nkentseu::editorkit::NkEditorPanel(
					  "nkuidesign_panneau_catalogue", "Catalogue",
					  nkentseu::editorkit::NkEditorDockSide::NK_LEFT),
				  mSt(st) {
			}

			void OnUI(NkEditorFrameContext &ec) override {
				using namespace nkentseu;
				auto &ctx = ec.Ui();
				// ⚠️ LE REGISTRE, PAS UNE TABLE : un greffon peut y ajouter ses
				//    composants et ses catégories (Rodolf, 28/09). Relire le
				//    registre à CHAQUE image est ce qui fait qu'un greffon chargé
				//    en cours de session apparaît sans redémarrage.
				nkgui::NkGuiCatalogueRegistre &reg = nkgui::NkGuiCatalogueGlobal();
				const uint32 n = reg.Taille();
				if (n == 0u) {
					nkgui::Text(ctx, "Catalogue vide — le registre ne contient rien.");
					return;
				}

				// ── LE FILTRE, parce que 54 rôles ne se parcourent pas à l'œil ──
				// ⚠️ `InputText`, PAS `TextField` : `TextField` est un RÔLE du
				//    format (ce que le catalogue décrit), pas une fonction de
				//    NKGui. Le widget, lui, s'appelle `InputText` — les deux noms
				//    se ressemblent assez pour tromper, et la compilation l'a dit.
				(void)nkgui::InputText(ctx, "##cat.filtre", mFiltre, (int32)sizeof(mFiltre));

				// LE CHOIX DE L'IMPORT (Rodolf, 28/09 : « importer un .nkgui peut
				// mettre ses composants en bibliothèque ou pas, en fonction du
				// choix de l'utilisateur »). ⚠️ IL VIT ICI, là où la bibliothèque
				// se regarde — et il est consulté par `AppliquerImportNkgui`.
				(void)nkgui::Checkbox(ctx, "Importer aussi les composants d'un document ouvert",
									  mSt.importerComposants);
				nkgui::Text(ctx, "Cliquer un rôle copie son extrait `.nkgui`.");

				// ⚠️ ON PARCOURT LE CATALOGUE DANS SON ORDRE, ET ON POSE UN TITRE
				//    QUAND LA FAMILLE OU LA CATÉGORIE CHANGE. Trier ici donnerait
				//    un second ordre à tenir d'accord avec celui du noyau ; le
				//    catalogue est déjà écrit groupé, c'est SON ordre qui fait foi.
				NkString familleVue, categorieVue;
				uint32 montres = 0u;
				for (uint32 i = 0; i < n; ++i) {
					const nkgui::NkGuiRoleInfo *p = reg.At(i);
					if (!p || !Correspond(*p))
						continue;
					const nkgui::NkGuiRoleInfo &r = *p;
					// ⚠️ ON COMPARE LE NOM DE FAMILLE, PAS L'ÉNUMÉRATION : une
					//    famille apportée par un greffon vaut toujours
					//    `Greffon` dans l'énumération, et deux greffons auraient
					//    alors partagé un seul titre.
					const NkString fam(r.Famille());
					if (familleVue.Compare(fam) != 0) {
						familleVue = fam;
						categorieVue = NkString();
						nkgui::Separator(ctx);
						TitreFamille(ctx, fam.CStr());
					}
					if (categorieVue.Compare(r.categorie) != 0) {
						categorieVue = r.categorie;
						TitreCategorie(ctx, r.categorie.CStr());
					}
					Ligne(ctx, r);
					++montres;
				}
				// ⚠️ UN FILTRE QUI NE REND RIEN LE DIT. Une liste vide se lit comme
				//    une panne du panneau ; la phrase dit que c'est le filtre.
				if (montres == 0u)
					nkgui::Text(ctx, "Aucun rôle ne correspond à ce filtre.");
			}

		private:
			bool Correspond(const nkgui::NkGuiRoleInfo &r) const {
				if (!mFiltre[0])
					return true;
				return Contient(r.nom.CStr(), mFiltre) || Contient(r.categorie.CStr(), mFiltre)
					   || Contient(r.resume.CStr(), mFiltre)
					   || Contient(r.provenance.CStr(), mFiltre);
			}

			/// Recherche insensible à la casse, écrite ici faute d'une maison.
			static bool Contient(const char *foin, const char *aiguille) {
				if (!foin || !aiguille || !*aiguille)
					return false;
				auto bas = [](char c) { return (c >= 'A' && c <= 'Z') ? (char)(c + 32) : c; };
				for (const char *p = foin; *p; ++p) {
					const char *a = aiguille;
					const char *q = p;
					while (*a && *q && bas(*q) == bas(*a)) {
						++a;
						++q;
					}
					if (!*a)
						return true;
				}
				return false;
			}

			void TitreFamille(nkgui::NkGuiContext &ctx, const char *titre) {
				const nkgui::NkRect r = ctx.NextItemRect(-1.f, 20.f);
				auto &F = costume::Fontes();
				costume::TexteGras(ctx.dl, F.px9, r.x + 6.f, r.y + 8.f, titre, ctx.theme.accent,
								   0.6f);
			}

			void TitreCategorie(nkgui::NkGuiContext &ctx, const char *titre) {
				const nkgui::NkRect r = ctx.NextItemRect(-1.f, 18.f);
				auto &F = costume::Fontes();
				costume::Texte(ctx.dl, F.px9, r.x + 14.f, r.y + 5.f, titre, ctx.theme.textMuted);
			}

			void Ligne(nkgui::NkGuiContext &ctx, const nkgui::NkGuiRoleInfo &r) {
				using namespace nkentseu;
				const nkgui::NkRect rect = ctx.NextItemRect(-1.f, 34.f);
				const bool survol = nkgui::NkGuiRectContains(rect, ctx.input.mousePos);
				if (survol)
					ctx.DL().AddRectFilled(rect, ctx.theme.rowHover, 3.f);
				auto &F = costume::Fontes();
				costume::TexteGras(ctx.dl, F.px10, rect.x + 22.f, rect.y + 4.f, r.nom.CStr(),
								   ctx.theme.text, 0.f);
				costume::Texte(ctx.dl, F.px9, rect.x + 22.f, rect.y + 18.f, r.resume.CStr(),
							   ctx.theme.textMuted);
				// ⚠️ CE QUI VIENT D'UN GREFFON LE DIT, ET SON PARENT AUSSI. Sans
				//    ça, un composant apporté se confond avec un rôle du format —
				//    et le jour où le greffon est retiré, l'utilisateur ne sait pas
				//    pourquoi son document ne monte plus.
				if (!r.integre || r.herite.Size() > 0u) {
					char marque[128];
					nkentseu::NkSnprintf(marque, sizeof(marque), "%s%s%s",
										 r.integre ? "" : r.provenance.CStr(),
										 (!r.integre && r.herite.Size() > 0u) ? " · " : "",
										 r.herite.Size() > 0u ? "hérite de " : "");
					const float32 xd = rect.x + rect.w - 8.f;
					char plein[192];
					nkentseu::NkSnprintf(plein, sizeof(plein), "%s%s", marque,
										 r.herite.Size() > 0u ? r.herite.CStr() : "");
					const float32 lg = costume::Largeur(F.px9, plein);
					costume::Texte(ctx.dl, F.px9, xd - lg, rect.y + 10.f, plein,
								   ctx.theme.accent);
				}
				if (!survol)
					return;
				nkentseu::editorkit::NkTooltip(ctx, true, r.extrait.CStr());
				if (!ctx.input.mouseClicked[0])
					return;
				ctx.SetClipboard(r.extrait.CStr());
				char msg[320];
				nkentseu::NkSnprintf(msg, sizeof(msg), "Copié : %s", r.extrait.CStr());
				mSt.DireAuPied(msg);
			}

			DesignState &mSt;
			char mFiltre[48] = {};
	};

} // namespace nkuidesign

// =============================================================================
// Copyright (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================
