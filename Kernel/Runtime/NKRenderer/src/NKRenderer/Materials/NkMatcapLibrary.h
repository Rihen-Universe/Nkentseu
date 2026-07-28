#pragma once
// =============================================================================
// NkMatcapLibrary.h — NKRenderer / bibliotheque de 30 MatCaps facon Blender
//
// Une « matcap » (material capture) est une image de BOULE eclairee. On la
// echantillonne par la NORMALE EN ESPACE VUE :
//
//     uv = normalVue.xy * 0.5 + 0.5        (le disque unite = la boule)
//
// L'eclairage est donc « peint » dans la texture : aucune lumiere de scene,
// aucune ombre, aucun IBL. C'est exactement ce que fait Blender en mode Solid
// avec Lighting = MatCap, et c'est pourquoi une matcap doit rester en dehors du
// bloom : elle represente un materiau, pas une source lumineuse.
//
// CE MODULE NE PARLE PAS AU GPU. Il produit uniquement des PIXELS RGBA8, ce qui
// le rend testable sans peripherique et reutilisable par l'editeur (vignettes du
// selecteur de matcap) autant que par le moteur de rendu.
//
//   NkMatcapLibrary::GenerateBall(id, size, dst)   -> une boule seule
//   NkMatcapLibrary::GenerateAtlas(dst)            -> les 30 dans un atlas
//   NkMatcapLibrary::TileTransform(id, off, scale) -> ou lire la boule id
//
// Cote shader, l'echantillonnage devient :
//     vec2 ball = clamp(vn.xy * 0.5 + 0.5, 0.0, 1.0);
//     vec3 rgb  = texture(tMatcapAtlas, uMatcapOff + ball * uMatcapScale).rgb;
//
// ATLAS : 6 colonnes x 5 lignes de tuiles de 128 px, avec 4 px de marge autour
// de chaque boule. La marge n'est pas cosmetique : sans elle, le filtrage
// bilineaire au bord d'une tuile irait chercher des texels de la tuile VOISINE
// et dessinerait un lisere de la mauvaise matcap sur la silhouette.
// L'atlas est genere SANS mipmaps (une matcap est deja lisse a l'ecran, et des
// mips melangeraient les tuiles entre elles).
// =============================================================================
#include "NKCore/NkTypes.h"

namespace nkentseu {
	namespace renderer {

		class NkMatcapLibrary {
			public:
				static const int32 kCount = 30;	   // nombre de matcaps
				static const uint32 kTile = 128;   // cote d'une tuile, en pixels
				static const uint32 kPad = 4;	   // marge anti-bavure, en pixels
				static const uint32 kCols = 6;	   // colonnes de l'atlas
				static const uint32 kRows = 5;	   // lignes de l'atlas (kCols*kRows >= kCount)
				static const uint32 kAtlasW = kCols * kTile;
				static const uint32 kAtlasH = kRows * kTile;

				// Nom stable de la matcap (affiche dans l'UI, utilisable en cle de config).
				// Renvoie "" si id hors bornes.
				static const char *Name(int32 id);

				// Ecrit une boule matcap RGBA8 de size x size dans dst (size*size*4 octets).
				// Le disque occupe tout le carre ; utile pour les vignettes d'interface.
				static void GenerateBall(int32 id, uint32 size, uint8 *dst);

				// Ecrit l'atlas complet RGBA8 : kAtlasW x kAtlasH x 4 octets dans dst.
				// Les tuiles au-dela de kCount sont laissees en fond neutre.
				static void GenerateAtlas(uint8 *dst);

				// Transformation d'echantillonnage de la matcap id dans l'atlas :
				//   uvAtlas = offset + ballUV01 * scale
				// ou ballUV01 = clamp(normalVue.xy * 0.5 + 0.5, 0, 1).
				// offset/scale pointent l'INTERIEUR de la tuile (marge exclue).
				static void TileTransform(int32 id, float32 *outOffsetXY, float32 *outScaleXY);

			private:
				NkMatcapLibrary() = delete;
		};

	} // namespace renderer
} // namespace nkentseu
