#pragma once
// =============================================================================
// Nogee/Panels/ContentBrowserPanel.h
// =============================================================================
// PORTAGE 4/4 (2026-08-17) — le navigateur d'assets, ecrit sur NKGui/NKEditorKit
// au lieu de NKUI, vise sur la CIBLE (§9 « Content Browser ») et sur la
// REFERENCE DES CARTES ecrite par NK3DModeler dans sa ROADMAP (section
// « CARTES D'ASSETS DU NAVIGATEUR DE CONTENU », decision de Rihen du 17/08 :
// s'inspirer de NK3DModeler, en lisant la description, pas une capture).
//
// Meme regle de nom que les portages 2 et 3 : vocabulaire de la cible
// (« Content Browser »), pas celui du code (« AssetBrowser »). Le panneau NKUI
// `AssetBrowser` reste vivant et intact.
//
// -----------------------------------------------------------------------------
// CE QUE LA CIBLE §9 DEMANDE, ET OU ON EN EST
// -----------------------------------------------------------------------------
//   ✅ fil d'ariane cliquable ................ fait (segments -> NavigateTo)
//   ✅ barre de recherche ..................... fait (insensible a la casse —
//        convention du panneau d'ORIGINE, conservee)
//   ✅ slider de taille des miniatures ........ fait (vignette variable ;
//        LARGEUR DE CARTE FIXE pendant un rendu, le nombre de colonnes varie —
//        c'est la regle NK3DModeler, et elle est compatible avec le slider)
//   ✅ grille de cartes ....................... fait (grille enveloppante :
//        une carte qui depasserait la marge droite part a la ligne)
//   ✅ nom sous la carte, type en dessous ..... fait (pied 2 lignes, clippe)
//   ✅ selection = marque accent .............. fait (aplat deborde de 2 px —
//        etat ACTIVE de NK3DModeler ; l'etat CHOISIES n'existe pas ici, la
//        selection de Nogee est simple)
//   ✅ double-clic dossier = entrer ........... fait
//   ✅ vignettes paresseuses .................. fait (budget partage du modele)
//   ⛔ colonne gauche Sources/Favoris/Collections  NON FAIT : demande un arbre
//        de dossiers recursif + une notion de favoris absente du modele. Poste
//        §9, pas un oubli.
//   ⛔ rotation 3D au survol / lecture au survol   NON FAIT (pas de rendu 3D
//        dans une vignette NKGui aujourd'hui).
//   ✅ glisser-deposer vers le Viewport ........ fait (2026-08-17) : l'API de
//        charge utile NKGui existe (commit 442fe8c7). Le pied de carte est
//        SOURCE (type "asset", charge = chemin relatif ENTIER — on refuse de
//        declarer une charge qui ne tient pas dans DragPayloadMax plutot que
//        de livrer un chemin tronque) ; la cible est le ViewportPanel
//        (journal MESURE — le spawn n'est pas cable, le panneau le dit).
//   ⛔ clic-droit complet (Migrer, Reference Viewer, Size Map…)  NON FAIT :
//        §9bis entier (graphe de dependances, treemap) est un chantier, pas un
//        menu.
//   ⛔ boutons Importer / + Ajouter ........... NON FAIT (l'import est un poste
//        AssetManager, pas un bouton).
//
// -----------------------------------------------------------------------------
// LA REFERENCE NK3DMODELER, ET CE QUI EN EST REPRIS ICI
// -----------------------------------------------------------------------------
//   Repris tel quel : geometrie de carte (vignette carree + BANDE DE TYPE 3 px
//   + pied 34 px, deux lignes), ombre portee (+2,+3) noir alpha 90, espacement
//   14, grille enveloppante a largeur de carte fixe, selection ACTIVE en aplat
//   debordant de 2 px, nom EDITABLE EN PLACE dans le pied (valide a la fin,
//   jamais copie par frame), damier de fond « ce fond est vide ».
//   NON repris : la table de couleurs `NkAssetColor` de NK3DModeler — elle vit
//   dans `NkModelerUI.h`, une AUTRE application ; l'inclure creerait une
//   dependance inter-applications. Nogee a donc SA table locale, en UN point
//   (`AssetColor`/`AssetKindName` ci-dessous), meme principe de point de
//   passage unique. Les natures different aussi (Nogee n'a ni procedural ni
//   dataset ; NK3DModeler n'a ni Font).
// =============================================================================

#include "NKCore/NkTypes.h"
#include "NKEditorKit/NkEditorPanel.h"
#include "NKEditorKit/NkEditorContext.h"
#include "Nogee/Panels/Model/NkAssetBrowserModel.h" // modele PARTAGE avec AssetBrowser

namespace nkentseu {
	namespace noge {

		class ContentBrowserPanel final : public editorkit::NkEditorPanel, public NkAssetBrowserModel {
			public:
				ContentBrowserPanel() noexcept
					: editorkit::NkEditorPanel("Content Browser", editorkit::NkEditorDockSide::NK_BOTTOM) {
				}

				// Init(mgr, projectDir) est herite du modele — meme appel que le
				// panneau NKUI, aucune duplication.

				void OnUI(editorkit::NkEditorFrameContext &ec) override;

				// ── SONDE drag-drop (--dragdrop-test) : rect ECRAN reel du pied de
				// la PREMIERE carte dessinee + son chemin relatif (meme regle que
				// la sonde de l'Outliner : on mesure la geometrie, on ne la devine
				// pas).
				void EnableProbe(bool on) noexcept {
					mProbeEnabled = on;
				}
				bool ProbeCard(nkgui::NkRect *outRect, const char **outRelPath) const noexcept {
					if (!mProbeCardValid)
						return false;
					if (outRect)
						*outRect = mProbeCardRect;
					if (outRelPath)
						*outRelPath = mProbeCardPath;
					return true;
				}
				// Horodatage (ctx.time) de la derniere mesure : frais = frame
				// courante ; un ecart qui grandit = le panneau n'est PLUS rendu
				// (onglet cache) et le rect est PERIME.
				float32 ProbeCardTime() const noexcept {
					return mProbeCardTime;
				}
				// Portes d'ItemHoverable relevees au dessin du pied (diagnostic).
				nkgui::NkGuiId ProbeGateHoveredWin() const noexcept {
					return mProbeGateHoveredWin;
				}
				nkgui::NkGuiId ProbeGateCurWin() const noexcept {
					return mProbeGateCurWin;
				}
				bool ProbeGateClipContains() const noexcept {
					return mProbeGateClipContains;
				}
				// Zone visible du panneau (clip de la frame courante) : ou poser la
				// molette pour faire defiler quand aucune carte fichier n'est visible.
				bool ProbeArea(nkgui::NkRect *out) const noexcept {
					if (!mProbeAreaValid)
						return false;
					if (out)
						*out = mProbeArea;
					return true;
				}

			private:
				// Une carte : vignette + bande de type + pied (nom editable, type).
				// Renvoie true si l'entree a ete double-cliquee (navigation).
				bool RenderCard(nkgui::NkGuiContext &ctx, NkAssetBrowserEntry &entry, const nkgui::NkRect &card,
								float32 thumb) noexcept;

				void RenderBreadcrumb(nkgui::NkGuiContext &ctx) noexcept;

				// Point de passage UNIQUE couleur/nom de type pour Nogee (meme
				// principe que NkAssetColor/NkAssetKindName chez NK3DModeler —
				// table locale, cf. en-tete).
				static nkgui::NkColor AssetColor(NkAssetType t, bool isDirectory) noexcept;
				static const char *AssetKindName(NkAssetType t, bool isDirectory) noexcept;

				// Tampon du nom en cours d'edition (SelectableEditable edite en
				// place ; la validation recopie vers l'entree — jamais par frame).
				char mNameEdit[96] = {};

				// Sonde drag-drop (cf. bloc public).
				bool mProbeEnabled = false;
				bool mProbeCardValid = false;
				bool mProbeCardIsMesh = false; ///< la carte retenue est un MESH (palier A : prioritaire)
				nkgui::NkRect mProbeCardRect{0.f, 0.f, 0.f, 0.f};
				char mProbeCardPath[256] = {};
				float32 mProbeCardTime = -1.f;
				nkgui::NkGuiId mProbeGateHoveredWin = 0;
				nkgui::NkGuiId mProbeGateCurWin = 0;
				bool mProbeGateClipContains = false;
				nkgui::NkRect mProbeArea{0.f, 0.f, 0.f, 0.f};
				bool mProbeAreaValid = false;
		};

	} // namespace noge
} // namespace nkentseu
