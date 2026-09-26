#pragma once
// -----------------------------------------------------------------------------
// @File    NogeeCoquilleDocument.h
// @Brief   Nogee branche `NKGui/Doc/NkGuiCoquille.h` sur les crochets de
//          `NkEditorShell` : sa barre d'état et un panneau, décrits par des
//          documents `.nkgui`.
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// =============================================================================
//  CE FICHIER EST DE LA COLLE, ET RIEN D'AUTRE
// =============================================================================
//  Tout ce qui monte un document vit dans `NKGui/Doc/NkGuiCoquille.h`, partagé
//  avec NkAnimaEditor. Ici : les crochets de la coquille d'éditeur, que le
//  fichier partagé ne peut pas connaître — NKGui est SOUS NKEditorKit dans le
//  graphe de dépendances, et l'inverse serait une inversion de couches.
//
// =============================================================================
//  CE QU'ON PREND, CE QU'ON NE PREND PAS -- ET LES DEUX SONT MESURÉS
// =============================================================================
//  | prise               | état chez Nogee                  | on la prend ? |
//  |---------------------|----------------------------------|---------------|
//  | `SetMenuBar`        | `NogeeChrome::BarreDeMenus`      | ❌ non        |
//  | `SetToolbar`        | `NogeeChrome::BarreDOutils`      | ❌ non        |
//  | `SetStatusBarFn`    | **libre** (0 occurrence, mesuré) | ✅ oui        |
//  | un `NkEditorPanel`  | ouvert                           | ✅ oui        |
//
//  ⚠️ ON NE REMPLACE PAS `NogeeChrome`, ET CE N'EST PAS DE LA PRUDENCE : c'est
//     une limite du format, écrite. Il grise les entrées dont l'outil n'existe
//     pas encore ET DIT POURQUOI. Le format porte `enabled = false` mais **pas
//     le motif** ; un document rendrait donc une barre grise MUETTE. On aurait
//     échangé 609 lignes qui expliquent contre moins qui se taisent.
//     *On ne migre pas vers moins.*
//
//     Et la barre de menus a un second obstacle, distinct : le menu
//     « Affichage » de la coquille est construit par `DrawPanelsMenuItems()`,
//     qui **énumère les panneaux à l'exécution**. `ListBox` a un `bind`,
//     `Menu` n'en a pas : le format ne sait pas exprimer une liste engendrée.
//
//     **CONDITION DE RETRAIT :** le jour où le format porte le motif du grisage
//     et la liste liée, ces deux lignes-ci tombent et la barre peut migrer.
// -----------------------------------------------------------------------------

#include "NKEditorKit/NkEditorKit.h"
#include "NKGui/Doc/NkGuiCoquille.h"

namespace nogee {

	using nkentseu::float32;
	using nkentseu::uint32;
	using nkentseu::NkString;
	using nkentseu::editorkit::NkEditorDockSide;
	using nkentseu::editorkit::NkEditorFrameContext;
	using nkentseu::editorkit::NkEditorPanel;
	using nkentseu::nkgui::NkActionNommee;
	using nkentseu::nkgui::NkBandeDocument;
	using nkentseu::nkgui::NkZoneNommee;

	/// Les deux bandes de Nogee qui viennent d'un document.
	class NogeeCoquilleDocument {
		public:
			NkBandeDocument barreEtat;
			NkBandeDocument panneau;
			NkString dossier;

			bool ChargerDepuisDossier(const char *d) noexcept {
				dossier = NkString(d);
				const bool a = barreEtat.ChargerDepuisFichier(Joindre("barre_etat.nkgui").CStr());
				const bool b = panneau.ChargerDepuisFichier(Joindre("panneau_scene.nkgui").CStr());
				return a && b;
			}

			void PoserTables(const NkActionNommee *act, uint32 nAct, const NkZoneNommee *zon,
							 uint32 nZon) noexcept {
				NkBandeDocument *b[2] = {&barreEtat, &panneau};
				for (uint32 i = 0; i < 2u; ++i) {
					b[i]->actions = act;
					b[i]->nbActions = nAct;
					b[i]->zones = zon;
					b[i]->nbZones = nZon;
				}
			}

			/// Le crochet de la barre d'état, de la signature exacte du kit.
			static void MonterBarreEtat(NkEditorFrameContext &ec, void *user) noexcept {
				((NogeeCoquilleDocument *)user)->barreEtat.Monter(ec.Ui());
			}

			uint32 RefusTotal() const noexcept {
				return (barreEtat.lu ? 0u : 1u) + (panneau.lu ? 0u : 1u);
			}

			NkString Joindre(const char *nom) const noexcept {
				NkString s = dossier;
				if (s.Size() > 0u) {
					const char d = s.Data()[s.Size() - 1u];
					if (d != '/' && d != '\\')
						s += NkString("/");
				}
				s += NkString(nom);
				return s;
			}
	};

	/// Le panneau dont le CONTENU vient d'un document.
	///
	/// ⚠️ SON TITRE, NON : c'est le littéral ci-dessous. `NkEditorPanel` garde le
	///    sien dans un `char mTitle[64]` posé à la construction et n'expose aucun
	///    `SetTitle` ; le panneau se construit avant que le document ne soit lu.
	///    Même limite que chez NkAnimaEditor, et elle est dite aux deux endroits
	///    plutôt que laissée à deviner.
	class PanneauDocumentNogee : public NkEditorPanel {
		public:
			explicit PanneauDocumentNogee(NkBandeDocument &bande) noexcept
				: NkEditorPanel("panneau_document", "Scene", NkEditorDockSide::NK_LEFT),
				  mBande(bande) {
			}

			void OnUI(NkEditorFrameContext &ec) override {
				mBande.Monter(ec.Ui());
			}

		private:
			NkBandeDocument &mBande;
	};

} // namespace nogee

// =============================================================================
// Copyright (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================
