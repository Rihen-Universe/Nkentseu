// =============================================================================
// NkRihenMarque.h — le symbole Rihen, EMBARQUE
//
// A QUOI SERT CE FICHIER
//   Porter le logo Rihen dans TOUTE application NkCanvasApp, sans aucun fichier
//   a livrer.
//
// ⚠️ POURQUOI EMBARQUE ET NON CHARGE DEPUIS LE DISQUE
//   Le logo complet du depot vit a `Resources/Pong/Textures/logo.svg` : 27,7 Ko,
//   1920x562, le mot RIHEN et son symbole. Pong le charge par chemin relatif au
//   REPERTOIRE COURANT — ce qui marche quand on lance depuis le depot, et pas
//   quand on distribue. Le livrer avec chaque jeu demanderait une chaine de
//   copie d'assets sur SEPT plateformes : `dependfiles` ici, `androidassets`
//   la, un preload cote Web, un bundle iOS. GemCrush a explicitement refuse
//   d'ouvrir cette chaine — « AUCUN dossier assets : GemCrush ne charge aucun
//   fichier a l'execution [...] a l'identique sur les six plateformes sans
//   chaine de copie d'assets ».
//
//   Le SYMBOLE seul, lui, tient en 514 octets. Embarque, il donne le vrai logo
//   sur les sept plateformes, sans un seul fichier a copier.
//
// ⚠️ CE N'EST PAS UN LOGO INVENTE
//   C'est `Applications/Mou/assets/brand/rihen-mark.svg`, extrait du logo
//   officiel, aux couleurs exactes de la charte : petrole #0A555F, orange
//   #F79A28. On ne DESSINE pas une marque, on rasterise la sienne.
//
// ⚠️ ET LE MOT « RIHEN » N'EST PAS DANS L'IMAGE
//   Il est ecrit en TEXTE par le splash, avec la police de l'application. Un
//   mot rasterise a 390 px serait flou sur un ecran dense et illisible sur un
//   petit ; en texte, il suit le facteur d'echelle et reste net partout.
//
// POINT D'EXTENSION
//   Le jour ou l'on veut le logo COMPLET (mot compris), deux voies : embarquer
//   `logo.svg` ici de la meme facon — il faudra alors accepter 27,7 Ko dans
//   chaque binaire — ou ouvrir la chaine d'assets et le charger. Le second choix
//   se paie sur sept plateformes, pas sur une.
// =============================================================================
#pragma once

#include "NKCore/NkTypes.h"

namespace nkentseu {
	namespace renderer {

		/// Le symbole Rihen, en SVG, tel quel.
		///
		/// ⚠️ Chaine BRUTE (`R"NKSVG(...)NKSVG"`) : le SVG contient des
		/// guillemets doubles a chaque attribut. Les echapper un par un serait
		/// une occasion d'en oublier un — et un SVG casse ne se voit qu'a
		/// l'execution, sous la forme d'un logo absent.
		inline const char *NkRihenMarqueSVG() noexcept {
			return R"NKSVG(<svg xmlns="http://www.w3.org/2000/svg" viewBox="370 305 390 390" width="390" height="390">
  <path fill="#0A555F" d="M395.87,435.93c14.12-58.15,91.99-114.9,166.64-115.44,71.45-.52,153.06,56.41,166.67,115.55l-333.31-.1h0Z"/>
  <path fill="#F79A28" d="M737.45,457.79c2.78,12.71,4.24,25.92,4.24,39.48,0,100.5-80.43,181.97-179.64,181.97s-179.65-81.47-179.65-181.97c0-13.6,1.47-26.86,4.26-39.61l350.78.13h.01Z"/>
</svg>)NKSVG";
		}

		/// La taille de rasterisation.
		///
		/// 256 px : assez pour un ecran dense sans peser. Le symbole est carre,
		/// donc une seule dimension suffit — et la supposer carree ailleurs
		/// serait une hypothese de plus a tenir.
		inline int32 NkRihenMarqueTaille() noexcept {
			return 256;
		}

		/// L'identifiant de texture reserve au symbole.
		///
		/// ⚠️ IL DOIT ETRE DISTINCT DE CEUX DES POLICES. `NkCanvasGuiApp` pose
		/// `0x4E4B4654` et les deux suivants pour ses trois polices ; celui-ci
		/// est pris volontairement loin de cette plage. Deux ressources qui
		/// partagent un identifiant ne provoquent aucune erreur : la seconde
		/// ECRASE la premiere, et le symptome est un texte qui devient une
		/// image, ou l'inverse.
		inline uint32 NkRihenMarqueTexId() noexcept {
			return 0x4E4B524Du; // 'NKRM'
		}

	} // namespace renderer
} // namespace nkentseu
