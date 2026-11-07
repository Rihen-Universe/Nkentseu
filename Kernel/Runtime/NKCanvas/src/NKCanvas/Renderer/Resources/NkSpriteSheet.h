// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
#pragma once
// =============================================================================
// NkSpriteSheet.h — les rectangles d'une PLANCHE de dessins, nommés.
//
// Une planche (« sprite sheet ») est une image qui contient plusieurs dessins.
// NkSprite sait déjà n'afficher qu'une partie d'une texture (SetTextureRect) :
// ce fichier ne refait pas cela. Il ne fait que CALCULER les rectangles et les
// NOMMER, pour qu'on écrive
//     sprite.SetTextureRect(*planche.Find("Burger"));
// Deux façons d'obtenir les rectangles :
//
// 1. GRILLE RÉGULIÈRE (BuildGrid). L'image est découpée en `colonnes x lignes`
//    cases égales, après retrait des marges de bord et des écarts entre cases.
//    Pour la case (c, l), l'origine est
//        x = margeGauche + c * (largeurCase + ecartX),
//        y = margeHaut   + l * (hauteurCase + ecartY),
//    avec largeurCase = (largeurImage - margeGauche - margeDroite
//                        - (colonnes - 1) * ecartX) / colonnes (division entière).
//    Une planche réelle porte souvent, dans chaque case, autre chose que le
//    dessin : une case à cocher, une étiquette. La « zone utile » (fractions de
//    la case, entre 0 et 1) restreint le rectangle rendu au dessin seul :
//        rect = case.x + u.x * w, case.y + u.y * h, u.w * w, u.h * h.
//
// 2. DÉTECTION AUTOMATIQUE (BuildAuto), pour une planche sur FOND UNI sans
//    grille régulière. On projette l'image sur l'axe vertical : une ligne est
//    « occupée » si elle contient un pixel différent du fond (voir
//    NkImagePalette::Spans). Les lignes occupées consécutives forment des
//    BANDES ; dans chaque bande on recommence sur l'axe horizontal pour
//    séparer les dessins côte à côte, puis on resserre le rectangle sur la
//    hauteur réellement occupée. Les rectangles sont rendus ligne par ligne,
//    de gauche à droite (ordre de lecture). Limite : deux dessins qui se
//    chevauchent dans les deux projections ne se séparent pas.
//
// Les noms sont attribués dans l'ordre de lecture (SetNames), ou un par un
// (Rename). Un nom absent s'appelle « case_<indice> ».
// =============================================================================

#include "NKCore/NkTypes.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKMath/NkRectangle.h"
#include "NKMath/NkColor.h"
#include "NKImage/Core/NkImage.h"

namespace nkentseu {
	namespace renderer {

		/// Parametres d'une planche en grille reguliere.
		struct NkSheetGrid {
				int32 colonnes = 1;
				int32 lignes = 1;
				int32 margeGauche = 0, margeHaut = 0, margeDroite = 0, margeBas = 0; ///< pixels
				int32 ecartX = 0, ecartY = 0;										   ///< pixels entre deux cases
				// Zone utile de chaque case, en fractions de la case (0..1).
				float32 utileX = 0.f, utileY = 0.f, utileL = 1.f, utileH = 1.f;
		};

		struct NkSheetCell {
				char nom[48] = {0};
				math::NkRect2i rect;
		};

		class NkSpriteSheet {
			public:
				/// Decoupe en grille. Rend le nombre de rectangles (colonnes x lignes), 0 si invalide.
				int32 BuildGrid(int32 largeurImage, int32 hauteurImage, const NkSheetGrid &g);

				/// Detection sur fond uni (voir l'en-tete). `fond` : couleur du fond ;
				/// `tolerance` : ecart par canal tolere ; `ecartMin` : lignes vides pour
				/// separer deux dessins ; `tailleMin` : plus petite bande retenue.
				int32 BuildAuto(const NkImage &image, const math::NkColor &fond, int32 tolerance, int32 ecartMin = 3,
								int32 tailleMin = 8);

				/// Resserre chaque rectangle sur les pixels occupes (autres que le fond) qu'il
				/// contient : retire les marges vides d'une zone utile trop genereuse.
				/// Un rectangle sans aucun pixel occupe est laisse tel quel.
				void Tighten(const NkImage &image, const math::NkColor &fond, int32 tolerance);

				/// (07/11, 1.5.8) EXTRAIT la sous-image `indice` de `image` : copie du rectangle, FOND retire (les pixels proches de `fond` ET reliés au bord par un
				/// chemin de pixels de fond deviennent transparents ; les blancs INTERIEURS a l'icone restent opaques), puis recadrage serre sur le contenu (+1 pixel).
				/// Faux si l'indice est hors planche, le rectangle trop petit (< 2 pixels) ou la sous-image entierement du fond. Vient de FoodHunt (SpriteSheet::Extract).
				bool Extract(const NkImage &image, int32 indice, const math::NkColor &fond, int32 tolerance, NkImage &out) const;

				/// Noms dans l'ordre de lecture : `noms[0]` pour le premier rectangle, etc.
				void SetNames(const char *const *noms, int32 n);
				void Rename(int32 indice, const char *nom);

				int32 Count() const noexcept {
					return static_cast<int32>(mCases.Size());
				}
				const NkSheetCell *Get(int32 indice) const noexcept;
				/// Rectangle d'un nom, nullptr s'il n'existe pas.
				const math::NkRect2i *Find(const char *nom) const noexcept;

			private:
				void NommerParDefaut();
				NkVector<NkSheetCell> mCases;
		};

	} // namespace renderer
} // namespace nkentseu
