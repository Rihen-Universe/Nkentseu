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
				virtual ~NkEditorPanel() = default;

				NkEditorPanel(const NkEditorPanel &) = delete;
				NkEditorPanel &operator=(const NkEditorPanel &) = delete;

				// ── Identite ────────────────────────────────────────────────────────
				const char *Title() const noexcept {
					return mTitle;
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

				// ── PANNEAU QUI REMPLIT SON CORPS (2026-09-13) ──────────────────────
				// Faux (defaut) : contenu qui COULE — le panneau empile des elements,
				// deborde, et le dock lui donne un ascenseur. C'est le cas de
				// l'explorateur, de l'inspecteur, de la console : leur contenu peut
				// etre plus grand que ce qu'on en voit.
				//
				// Vrai : le panneau EST sa surface. Une vue 3D, un canevas, un apercu
				// video n'ont pas de « contenu plus grand que la fenetre » — ils se
				// REDIMENSIONNENT. Leur donner un ascenseur est une contradiction, et
				// la gouttiere qu'il reserve mange la surface de l'image.
				//
				// Ce n'est pas un reglage de gout : un viewport qui defile est une
				// faute de mecanisme. Le drapeau vit ICI, dans le kit, parce que la
				// vue 3D ne sera pas le seul panneau de ce genre — l'apercu de
				// materiau et le lecteur video posent exactement la meme question.
				virtual bool RemplitSonCorps() const noexcept {
					return false;
				}

				// Actions du panneau dessinees sur la BARRE D'ONGLETS du dock (a droite),
				// quand ce panneau est l'onglet ACTIF. Defaut : rien. (Ex. Terminal : +/combo.)
				virtual void OnTabBarActions(nkgui::NkGuiContext &ctx, const nkgui::NkRect &tabBar) noexcept {
					(void)ctx;
					(void)tabBar;
				}

			protected:
				char mTitle[64] = {};
				bool mOpen = true;
				bool mDockable = true;
				NkEditorDockSide mDefaultSide = NkEditorDockSide::NK_CENTER;
		};

	} // namespace editorkit
} // namespace nkentseu
