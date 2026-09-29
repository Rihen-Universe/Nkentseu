// =============================================================================
// NkUE5Ui.h — une petite couche de widgets immediats, habillee facon UE5
//
// POURQUOI UNE COUCHE PAR-DESSUS NKGui, et pas NKGui directement
//   NKGui fournit tout ce qui compte : la liste d'affichage (NkGuiDrawList),
//   les polices (NkGuiFont), le decoupage, les modes de melange, et le dorsal
//   NKCanvas qui soumet le tout. Ses WIDGETS, eux, portent leur propre
//   silhouette (curseur rond, libelle a droite de la valeur). L'editeur d'UE5
//   en a une autre : libelle a gauche, champ numerique sombre a droite qu'on
//   GLISSE, bandeaux de categories repliables, boutons d'outils carres. On
//   reprend donc les fondations de NKGui et on redessine la surface.
//
// LE PATRON : immediat, un identifiant par widget (chaine hachee), un seul
// widget "actif" a la fois (celui qu'on tient a la souris). Les coordonnees
// sont en pixels ECRAN ; `E(x)` convertit une mesure de maquette en pixels
// reels selon la densite de l'ecran.
//
// AUTEUR: Rihen
// LICENCE: Proprietary - All Rights Reserved (see LICENSE)
// =============================================================================
#pragma once

#include "NKGui/Core/NkGuiDrawList.h"
#include "NKGui/Core/NkGuiFont.h"
#include "NKGui/Core/NkGuiTypes.h"
#include "Physic2D/Editeur/NkUE5Theme.h"
#include "Physic2D/Physique/NkPhysMath.h"

namespace nkentseu {
	namespace physic2d {

		using nkgui::NkColor;
		using nkgui::NkGuiDrawList;
		using nkgui::NkGuiFont;
		using nkgui::NkRect;
		using nkgui::NkVec2;

		/// Etat de la souris pour UNE trame. `presse` et `relache` sont des
		/// FRONTS : vrais pendant exactement une trame.
		struct NkUE5Entree {
				float32 x = -1.0e6f;
				float32 y = -1.0e6f;
				bool bas[3] = {};	  ///< 0 gauche, 1 droit, 2 milieu
				bool presse[3] = {};
				bool relache[3] = {};
				float32 molette = 0.f;
				bool ctrl = false;
				bool shift = false;

				bool Dans(const NkRect &r) const noexcept {
					return x >= r.x && y >= r.y && x < r.x + r.w && y < r.y + r.h;
				}
				void FinDeTrame() noexcept {
					for (int32 i = 0; i < 3; ++i) {
						presse[i] = false;
						relache[i] = false;
					}
					molette = 0.f;
				}
		};

		enum class NkIcone : uint8 {
			NK_AUCUNE = 0,
			NK_SELECTION,
			NK_SAISIR,
			NK_COUPER,
			NK_EXPLOSION,
			NK_AIMANT,
			NK_OBSTACLE,
			NK_GOMME,
			NK_EPINGLE,
			NK_JOUER,
			NK_PAUSE,
			NK_PAS,
			NK_STOP,
			NK_OEIL,
			NK_OEIL_BARRE,
			NK_FLECHE_BAS,
			NK_FLECHE_DROITE,
			NK_GRILLE,
			NK_LOUPE,
			NK_RECADRER,
			NK_POUBELLE,
			NK_ENGRENAGE,
			NK_NIVEAU,
			NK_CUBE_PLUS,
			// Acteurs
			NK_A_BALLON,
			NK_A_BLOB,
			NK_A_GELEE,
			NK_A_CAISSE,
			NK_A_GOUTTE,
			NK_A_SABLE,
			NK_A_ATOMES,
			NK_A_TISSU,
			NK_A_CORDE,
			NK_A_PONT
		};

		/// Dessine une icone vectorielle centree en `c`, dans un carre de cote `t`.
		void NkDessinerIcone(NkGuiDrawList &dl, NkIcone ic, const NkVec2 &c, float32 t, const NkColor &col);

		class NkUE5Ui {
			public:
				/// Trois polices chargees a leur taille REELLE (en pixels) : le
				/// texte courant est dessine a l'echelle 1, donc net.
				void Debut(NkGuiDrawList *dl, NkGuiFont *police, float32 taillePolice, NkGuiFont *grande,
						   float32 tailleGrande, NkGuiFont *mono, float32 tailleMono, const NkUE5Entree *entree,
						   float32 echelle, float32 dt) noexcept;
				/// Dessine l'infobulle et libere l'actif si le bouton est relache.
				void Fin() noexcept;

				NkGuiDrawList &Dl() noexcept {
					return *mDl;
				}
				float32 E(float32 v) const noexcept {
					return v * mEchelle;
				}
				const NkUE5Entree &Entree() const noexcept {
					return *mEntree;
				}

				// --- Zone modale (menu deroulant ouvert) -------------------------
				/// Hors de ce rectangle, plus aucun widget ne reagit. w <= 0 : aucune.
				void DefinirModale(const NkRect &r) noexcept {
					mModale = r;
				}
				bool Bloque(const NkRect &r) const noexcept;

				// --- Texte ---------------------------------------------------------
				float32 LargeurTexte(const char *s, float32 px, bool mono = false) const noexcept;
				void Texte(float32 x, float32 haut, const char *s, const NkColor &c, float32 px, bool mono = false,
						   float32 maxW = -1.f) noexcept;
				/// Texte centre verticalement dans r, cale a gauche avec une marge.
				void TexteGauche(const NkRect &r, const char *s, const NkColor &c, float32 px, float32 marge) noexcept;
				void TexteCentre(const NkRect &r, const char *s, const NkColor &c, float32 px) noexcept;
				void TexteDroite(const NkRect &r, const char *s, const NkColor &c, float32 px, float32 marge) noexcept;

				// --- Widgets -------------------------------------------------------
				bool Survol(const NkRect &r) const noexcept;
				bool Clic(const NkRect &r) const noexcept; ///< front d'appui gauche dans r
				bool Bouton(const char *id, const NkRect &r, const char *libelle, bool actif = false,
							NkIcone ic = NkIcone::NK_AUCUNE, const NkColor *teinteIcone = nullptr) noexcept;
				/// Bouton carre d'outil. `bulle` : infobulle apres un temps de survol.
				bool BoutonOutil(const char *id, const NkRect &r, NkIcone ic, bool actif, const char *bulle,
								 const NkColor *teinte = nullptr) noexcept;
				bool Case(const char *id, const NkRect &r, bool &v) noexcept;
				/// Champ numerique UE5 : on le GLISSE horizontalement (Maj = fin).
				/// Un clic sans glisser pose la valeur a l'endroit clique.
				bool Glisseur(const char *id, const NkRect &r, float32 &v, float32 mn, float32 mx, const char *fmt) noexcept;
				bool GlisseurEntier(const char *id, const NkRect &r, int32 &v, int32 mn, int32 mx) noexcept;
				/// Bandeau de categorie repliable (fleche + titre).
				bool Categorie(const char *id, const NkRect &r, const char *titre, bool &ouvert) noexcept;
				bool Onglet(const NkRect &r, const char *libelle, bool actif, NkIcone ic = NkIcone::NK_AUCUNE) noexcept;
				/// Ligne selectionnable (Outliner, menus). `impaire` : fond alterne.
				bool Ligne(const char *id, const NkRect &r, bool selectionnee, bool impaire) noexcept;

				void InfoBulle(const char *texte) noexcept;

				uint32 Actif() const noexcept {
					return mActif;
				}
				bool SourisCapturee() const noexcept {
					return mActif != 0;
				}
				static uint32 Hacher(const char *s) noexcept;

			private:
				/// La police chargee la plus proche de `px`, et l'echelle residuelle.
				NkGuiFont *Police(float32 px, bool mono, float32 &echelle) const noexcept;

				NkGuiDrawList *mDl = nullptr;
				NkGuiFont *mPolice = nullptr;
				NkGuiFont *mGrande = nullptr;
				NkGuiFont *mMono = nullptr;
				float32 mTaillePolice = 13.f;
				float32 mTailleGrande = 17.f;
				float32 mTailleMono = 12.f;
				const NkUE5Entree *mEntree = nullptr;
				float32 mEchelle = 1.f;
				float32 mDt = 0.016f;
				NkRect mModale = {0.f, 0.f, 0.f, 0.f};

				uint32 mActif = 0;
				float32 mDepartX = 0.f;
				float32 mDepartValeur = 0.f;
				bool mAGlisse = false;

				uint32 mBulleId = 0;
				uint32 mBulleIdTrame = 0;
				float32 mBulleTemps = 0.f;
				char mBulleTexte[160] = {};
				bool mBulleDemandee = false;
				uint32 mDernierSurvolId = 0;
		};

	} // namespace physic2d
} // namespace nkentseu
