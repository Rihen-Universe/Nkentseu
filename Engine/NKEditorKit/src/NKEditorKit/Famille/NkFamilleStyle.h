#pragma once
// -----------------------------------------------------------------------------
// @File    NkFamilleStyle.h
// @Brief   LE DESSIN DE LA FAMILLE « a la maniere d'Unreal 5 » : palette lue
//          dans le theme du kit, cotes, texte, bouton, onglets, recherche.
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// D'OU CA VIENT
//   UnkenyEditor (Applications/UnkenyEditor/src/Editeur/NkEditeurChrome.cpp) a
//   peint, le premier, la disposition d'UE5 que Rihen a validee le 01/10/2026
//   (document 02 : « fidelite a Unreal Engine 5 »). Son §0 dit que TOUTE la
//   famille (Nogee, NkAnimaEditor, PV3DE, NKCraft) doit lui ressembler, chacun
//   avec sa touche, et que le dessin commun vit ICI, jamais en copie.
//   Ces pieces sont donc celles d'UnkenyEditor, MEMES cotes, MEMES couleurs,
//   MEMES gestes -- rendues neutres : aucune ne connait une scene, un ECS ou un
//   moteur. UnkenyEditor garde les siennes tant qu'il n'a pas bascule (feuille
//   de route R32) ; Nogee est le premier a s'en servir.
//
// CE QUE CES PIECES N'ONT PAS LE DROIT DE FAIRE
//   - inclure NKCanvas ou NKRenderer : elles servent les deux (Unkeny rend par
//     NKCanvas, Noge par NKRenderer, et les deux ne se rencontrent pas dans une
//     unite de compilation) ;
//   - porter une couleur en dur qui serait un role du theme : la palette est LUE
//     dans NkTheme, c'est ce qui fera suivre un theme clair.
// -----------------------------------------------------------------------------

#include "NKEditorKit/NkTheme.h"
#include "NKEditorKit/NkThemeToGui.h"
#include "NKEditorKit/Components/NkComponentPaint.h"
#include "NKGui/Core/NkGuiContext.h"
#include "NKGui/Core/NkGuiDrawList.h"
#include "NKGui/Core/NkGuiFont.h"

namespace nkentseu {
	namespace editorkit {

		// ── LES COTES DE LA FAMILLE (celles d'UnkenyEditor, NkEditeurChrome.cpp) ──
		struct NkFamilleCotes {
				static constexpr float32 kMenus = 30.f;		 ///< la barre de titre, menus compris
				static constexpr float32 kOnglets = 26.f;	 ///< les onglets de scene, dessous
				static constexpr float32 kOutils = 34.f;	 ///< la barre d'outils
				static constexpr float32 kStatut = 22.f;	 ///< la barre d'etat
				static constexpr float32 kBarreVue = 28.f;	 ///< la barre de la vue
				static constexpr float32 kCloison = 4.f;	 ///< une cloison entre deux panneaux
				static constexpr float32 kOngletPanneau = 26.f; ///< l'onglet d'un panneau (« Outliner », « Détails »)
		};

		/// Les couleurs du chrome, LUES dans le theme du kit : aucune n'est en dur.
		/// (La meme regle que NkPaletteEditeur d'UnkenyEditor.)
		struct NkFamillePalette {
				nkgui::NkColor fond;		 ///< WindowBg   #141414
				nkgui::NkColor panneau;		 ///< PanelBg    #212121
				nkgui::NkColor entete;		 ///< PanelHeader #2B2B2B
				nkgui::NkColor bord;
				nkgui::NkColor champ;		 ///< InputBg
				nkgui::NkColor bouton;
				nkgui::NkColor boutonSurvol;
				nkgui::NkColor texte;
				nkgui::NkColor attenue;
				nkgui::NkColor accent;		 ///< BLEU : l'etat de l'interface
				nkgui::NkColor selection;	 ///< AMBRE : la selection dans la scene
				nkgui::NkColor surAccent;
				nkgui::NkColor danger;		 ///< la croix de fermeture au survol
				nkgui::NkColor vert;		 ///< le « + » d'Unreal (StatusOk)
				nkgui::NkColor axeX, axeY, axeZ;
		};

		NkFamillePalette NkFamillePaletteDe(const NkTheme &t) noexcept;

		/// Ce que recoit chaque dessin de la famille : le contexte NKGui de la
		/// trame, le theme (pour les composants du kit qui veulent des ROLES), sa
		/// palette deja resolue, et les deux polices (corps ~13 px, petite ~10 px).
		struct NkFamilleCtx {
				nkgui::NkGuiContext &ctx;
				const NkTheme &theme;
				const NkFamillePalette &pal;
				nkgui::NkGuiFont *police;
				nkgui::NkGuiFont *petite;
		};

		// ── LE TEXTE (les memes aides que NKCanvas/App/NkCanvasTexte.h, sans
		//    NKCanvas : le haut de la ligne, jamais la ligne de base) ─────────────
		float32 NkFamilleLargeur(const nkgui::NkGuiFont *f, const char *s) noexcept;
		float32 NkFamilleHauteurLigne(const nkgui::NkGuiFont *f, float32 secours) noexcept;
		void NkFamilleTexte(nkgui::NkGuiDrawList &dl, const nkgui::NkGuiFont *f, float32 x, float32 hautY, const char *s,
							const nkgui::NkColor &c, float32 largeurMax = -1.f) noexcept;
		void NkFamilleTexteCentre(nkgui::NkGuiDrawList &dl, const nkgui::NkGuiFont *f, float32 cx, float32 hautY,
								  const char *s, const nkgui::NkColor &c) noexcept;
		void NkFamilleTexteADroite(nkgui::NkGuiDrawList &dl, const nkgui::NkGuiFont *f, float32 droite, float32 hautY,
								   const char *s, const nkgui::NkColor &c) noexcept;
		void NkFamilleTexteDansBoite(nkgui::NkGuiDrawList &dl, const nkgui::NkGuiFont *f, const nkgui::NkRect &boite,
									 const char *s, const nkgui::NkColor &c) noexcept;
		/// Le GRAS d'Unreal (titres de carte) : le texte repasse a un demi-pixel.
		void NkFamilleTexteGras(nkgui::NkGuiDrawList &dl, const nkgui::NkGuiFont *f, float32 x, float32 hautY,
								const char *s, const nkgui::NkColor &c) noexcept;

		// ── LES PETITES AIDES ────────────────────────────────────────────────
		bool NkFamilleDans(const nkgui::NkRect &r, const nkgui::NkVec2 &p) noexcept;
		nkgui::NkColor NkFamilleMelange(const nkgui::NkColor &a, const nkgui::NkColor &b, float32 t) noexcept;
		/// La pointe « ▾ » d'un bouton deroulant, tracee (la police n'a pas le glyphe).
		void NkFamilleChevron(nkgui::NkGuiDrawList &dl, float32 cx, float32 cy, const nkgui::NkColor &c) noexcept;
		/// La coche « ✓ » d'une entree de menu, tracee.
		void NkFamilleCoche(nkgui::NkGuiDrawList &dl, float32 cx, float32 cy, const nkgui::NkColor &c) noexcept;
		/// La croix « ✕ », tracee (deux traits).
		void NkFamilleCroix(nkgui::NkGuiDrawList &dl, const nkgui::NkRect &r, const nkgui::NkColor &c,
							float32 marge = 4.5f, float32 epaisseur = 1.4f) noexcept;
		/// L'entree des composants du kit (arbre, navigateur) depuis celle de NKGui.
		NkComponentInput NkFamilleEntreeComposant(const nkgui::NkGuiContext &ctx) noexcept;

		// ── LES WIDGETS DU CHROME ────────────────────────────────────────────
		/// Un bouton plat : fond, bord, libelle centre. `enfonce` = accent plein.
		/// Rend vrai au clic. `dl` nul = la liste principale du contexte.
		bool NkFamilleBouton(NkFamilleCtx &c, const nkgui::NkRect &r, const char *texte, bool enfonce = false,
							 bool actif = true, nkgui::NkGuiDrawList *dl = nullptr, const nkgui::NkGuiFont *police = nullptr);
		/// Une bande d'onglets de panneau (« Contenu | Journal | Terminal ») : l'actif
		/// prend la couleur du panneau et un filet d'accent (UE5). Rend vrai s'il change.
		bool NkFamilleOnglets(NkFamilleCtx &c, const nkgui::NkRect &bande, const char *const *noms, int32 n,
							  int32 &actif);
		/// Le champ de recherche de la famille : fond de champ, bord d'accent au focus,
		/// loupe facultative, texte d'attente. Le focus se prend et se perd au clic ;
		/// Echap vide et rend la main.
		void NkFamilleRecherche(NkFamilleCtx &c, const nkgui::NkRect &r, char *tampon, int32 capacite, bool &focus,
								const char *attente, bool loupe, const nkgui::NkGuiFont *police);
		/// La loupe seule, au centre (cx, cy).
		void NkFamilleLoupe(nkgui::NkGuiDrawList &dl, float32 cx, float32 cy, const nkgui::NkColor &c) noexcept;

	} // namespace editorkit
} // namespace nkentseu
