#pragma once
/**
 * @File    NkGIFCodec.h
 * @Brief   Codec GIF production-ready — GIF87a/GIF89a complet.
 * @Author  TEUGUIA TADJUIDJE Rodolf Séderis
 * @License Proprietary - All Rights Reserved (see LICENSE)
 *
 * @Support
 *  Lecture  : GIF87a + GIF89a, LZW variable, GCT + LCT, interlacement,
 *              transparence (Graphic Control Extension), MULTI-FRAME avec
 *              delays + disposal methods 0-3 + loop count NETSCAPE2.0.
 *  Écriture : GIF89a, LZW variable (2-12 bits), quantification médiane coupure,
 *              palette 256 couleurs, transparence alpha, Graphic Control Extension.
 *
 * @Animation
 *  Pour decoder TOUTES les frames d'un GIF anime :
 *      NkGIFAnimation* anim = NkGIFCodec::DecodeAnimation(data, size);
 *      if (anim) {
 *          for (uint32 i = 0; i < anim->frameCount; ++i) {
 *              const NkImage& frame = anim->frames[i].image;   // RGBA32 composite
 *              uint32 delayMs = anim->frames[i].delayMs;       // duree avant la suivante
 *              ...
 *          }
 *          NkGIFCodec::FreeAnimation(anim);
 *      }
 *
 *  L'image d'une frame est une NkImage PAR VALEUR possedee par la frame : elle
 *  vit aussi longtemps que l'animation et disparait avec FreeAnimation. Pour la
 *  garder au-dela, la DEPLACER (traits::NkMove) ou la cloner (frame.image.Copy()).
 */
#include "NKImage/Core/NkImage.h"
#include <cstdio>
#include <cstddef>

namespace nkentseu {

	/// Une frame composite d'une animation GIF. L'image est deja composee
	/// (avec les frames precedentes selon les disposal methods) -- le caller
	/// peut l'afficher tel quel sans gerer le blending.
	///
	/// La frame POSSEDE son image (NkImage par valeur) : elle se libere avec la
	/// frame. NkGIFFrame est donc NON COPIABLE mais DEPLACABLE, exactement comme
	/// NkImage. Ne pas la copier : la deplacer (nkentseu::traits::NkMove).
	struct NkGIFFrame {
			NkImage image;		///< Frame RGBA32, taille canvas global (possedee)
			uint32 delayMs = 0; ///< Duree d'affichage avant frame suivante
			uint16 left = 0;	///< Position originale (info, deja composee)
			uint16 top = 0;
			uint8 disposal = 0; ///< 0=undef,1=keep,2=clear,3=restore (info)
	};

	/// Resultat de DecodeAnimation : liste des frames + metadonnees globales.
	/// Allouer/liberer EXCLUSIVEMENT via NkGIFCodec::FreeAnimation : le tableau
	/// `frames` est de la memoire brute dont les NkGIFFrame sont construites et
	/// detruites a la main (placement new / destructeur explicite).
	struct NkGIFAnimation {
			uint32 width = 0;			  ///< Largeur du canvas global (logical screen)
			uint32 height = 0;			  ///< Hauteur du canvas global
			uint32 frameCount = 0;		  ///< Nombre de frames effectivement decodees
			NkGIFFrame *frames = nullptr; ///< Array de frameCount entries (libere par FreeAnimation)
			uint16 loopCount = 0;		  ///< 0 = infini, sinon nb de boucles (NETSCAPE2.0)
	};

	class NKENTSEU_IMAGE_API NkGIFCodec {
		public:
			/// Décode la PREMIERE frame d'un GIF en RGBA32. Backward-compat.
			/// Pour les GIF animes, utiliser DecodeAnimation() qui retourne toutes
			/// les frames + leurs delays.
			/// Retourne l'image PAR VALEUR ; en cas d'echec elle est INVALIDE
			/// (IsValid()==false) — il n'y a plus de nullptr.
			static NkImage Decode(const uint8 *data, usize size) noexcept;

			/// Décode TOUTES les frames d'un GIF anime (multi-frame). Retourne nullptr
			/// si parsing rate ou data invalide. Le caller DOIT appeler FreeAnimation
			/// pour liberer la struct + toutes les frames (et leurs images).
			static NkGIFAnimation *DecodeAnimation(const uint8 *data, usize size) noexcept;

			/// Libere une NkGIFAnimation (detruit ses frames — donc leurs images — et sa struct).
			static void FreeAnimation(NkGIFAnimation *anim) noexcept;

			/// Encode en GIF89a avec quantification médiane coupure 256 couleurs.
			/// Gère la transparence (canal alpha → entrée palette dédiée).
			static bool Encode(const NkImage &img, uint8 *&out, usize &outSize) noexcept;

			/// Sauvegarde directement dans un fichier .gif
			static bool Save(const NkImage &img, const char *path) noexcept;
	};

} // namespace nkentseu
