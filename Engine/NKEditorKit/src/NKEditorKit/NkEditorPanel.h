#pragma once
// -----------------------------------------------------------------------------
// @File    NkEditorPanel.h
// @Brief   Classe de base d'un panneau d'editeur dockable.
// @Author  Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// Un NkEditorPanel = une fenetre logique de l'editeur (Explorateur, Inspecteur,
// Console, Viewport...). Le shell la dessine chaque frame quand elle est ouverte,
// flottante ou ancree (le shell gere la bascule). L'application derive cette
// classe et implemente OnUI(). Aucune dependance a NKReflection : le contenu est
// dessine imperativement (les panneaux pilotes par reflexion viendront plus tard,
// quand NKReflection<->NKSerialization sera murie — cf. ECOSYSTEM.md).
// -----------------------------------------------------------------------------

#include "NKEditorKit/NkEditorExport.h"
#include "NKEditorKit/NkEditorContext.h"

namespace nkentseu {
	namespace editorkit {

		class NKEDITORKIT_API NkEditorPanel {
			public:
				explicit NkEditorPanel(const char *title,
									   NkEditorDockSide defaultSide = NkEditorDockSide::NK_CENTER) noexcept;

				// ⚠️ L'IDENTIFIANT STABLE, ET POURQUOI IL EXISTE (2026-09-17)
				//    Jusqu'ici un panneau n'avait qu'une identite : son TITRE AFFICHE. La
				//    coquille s'y accrochait trois fois -- le retour de disposition
				//    (`StrEqual(Title(), nom)`), l'identite de la fenetre NKGui
				//    (`GetId(Title())`, qui hache la chaine entiere), et la serialisation.
				//    Consequence mesuree : **renommer un panneau -- ou le traduire -- perd sa
				//    disposition, en silence.** Et les libelles de ce depot sont francais et
				//    accentues : la traduction n'est pas une hypothese.
				//
				// ⚠️ STRICTEMENT ADDITIF : par defaut l'identifiant VAUT le titre. Une
				//    application qui n'en donne pas se comporte exactement comme avant, et
				//    les cinq peuvent migrer une par une.
				NkEditorPanel(const char *id, const char *title,
							  NkEditorDockSide defaultSide = NkEditorDockSide::NK_CENTER) noexcept;
				virtual ~NkEditorPanel() = default;

				NkEditorPanel(const NkEditorPanel &) = delete;
				NkEditorPanel &operator=(const NkEditorPanel &) = delete;

				// ── Identite ────────────────────────────────────────────────────────
				const char *Title() const noexcept {
					return mTitle;
				}

				/// L'IDENTIFIANT STABLE -- ce par quoi la disposition connait ce panneau.
				/// Vaut le titre tant que l'application n'en declare pas d'autre.
				const char *Id() const noexcept {
					return mId;
				}

				// ── Etat d'ouverture (pilote le menu « Affichage ») ─────────────────
				bool *OpenPtr() noexcept {
					return &mOpen;
				}

				bool IsOpen() const noexcept {
					return mOpen;
				}

				void SetOpen(bool o) noexcept {
					mOpen = o;
				}

				// ── Docking ─────────────────────────────────────────────────────────
				bool Dockable() const noexcept {
					return mDockable;
				}

				void SetDockable(bool d) noexcept {
					mDockable = d;
				}

				NkEditorDockSide DefaultSide() const noexcept {
					return mDefaultSide;
				}

				void SetDefaultSide(NkEditorDockSide s) noexcept {
					mDefaultSide = s;
				}

				// ── Contenu (implemente par l'application) ──────────────────────────
				// Appele par le shell entre Begin/End (flottant) ou BeginDocked/EndDocked
				// (ancre). Dessiner via les helpers de `ec` (ec.Text, ec.Button, ...).
				virtual void OnUI(NkEditorFrameContext &ec) = 0;

				// Actions du panneau dessinees sur la BARRE D'ONGLETS du dock (a droite),
				// quand ce panneau est l'onglet ACTIF. Defaut : rien. (Ex. Terminal : +/combo.)
				virtual void OnTabBarActions(nkgui::NkGuiContext &ctx, const nkgui::NkRect &tabBar) noexcept {
					(void)ctx;
					(void)tabBar;
				}

			protected:
				char mTitle[64] = {};
				char mId[64] = {}; ///< identifiant stable ; par defaut, le titre
				bool mOpen = true;
				bool mDockable = true;
				NkEditorDockSide mDefaultSide = NkEditorDockSide::NK_CENTER;
		};

	} // namespace editorkit
} // namespace nkentseu
