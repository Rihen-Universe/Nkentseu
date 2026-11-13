#pragma once
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// -----------------------------------------------------------------------------
// @File    Kernel/Runtime/NKRenderer/src/NKRenderer/Tools/PathTracer/NkMatieresPhoto.h
// @License Proprietary - All Rights Reserved (see LICENSE)
// -----------------------------------------------------------------------------
//
// =============================================================================
//  LES CARTES IMAGE CC0 (13/11/2026, rendu-realiste)
// =============================================================================
//  Des PHOTOS de matieres (Poly Haven, ambientCG, licence CC0, 2K), rangees hors de
//  git dans D:\Rihen\References\textures_cc0\<source>\<nom>\ : couleur_2k.jpg,
//  normale_2k.jpg (convention OpenGL : le vert monte vers le HAUT de l'image),
//  rugosite_2k.jpg, deplacement_2k.jpg et echelle.txt (les metres couverts par une
//  repetition). Rien de ces images n'entre dans git ni dans un kit.
//
//  Ce fichier CUIT, pour une matiere decrite (couleur, rugosite), trois cartes au
//  standard que lisent le temps reel (glTF : couleur sRGB, normale et ORM lineaires)
//  et le traceur (la carte d'objet : texel x couleur), dans un dossier de cache :
//    - COULEUR : texel x couleur / MEDIANE de la photo (en lineaire, borne a 1). La
//      mediane de la carte cuite est la couleur DECRITE (les matieres exactes restent
//      exactes) ; la photo n'apporte que la variation. Le materiau porte albedo 1.
//    - ORM : r = 1 (pas d'occlusion), g = rugosite x r / mediane de r (bornee a 1),
//      b = 0 (pas de metal). Le materiau porte rugosite 1 (`rog = orm.g x rugosite`).
//    LA MEDIANE, PAS LA MOYENNE (banc du 13/11) : la description dit la surface DOMINANTE
//    (« gres cerame beige clair », rugosite 0,35 : le carreau), pas la moyenne carreau +
//    joint ; avec la moyenne, le carrelage (joints rugueux, carreaux lisses) sortait a
//    0,26 au lieu de 0,50 apres l'ecretage a 1. La mediane ne bouge pas quand on ecrete
//    au-dessus d'elle.
//    - NORMALE : celle de la photo, VERT INVERSE. Le moteur lit v = 0 sur la PREMIERE
//      ligne de l'image (aucun retournement au chargement : NkTextureLibrary) et sa
//      bitangente B = cross(N, T) suit les v croissants, donc le BAS de l'image ; une
//      normale OpenGL a son vert vers le HAUT. Le banc le verifie contre le gradient
//      du deplacement (NkPhotoBanc), pas sur la foi des conventions.
//  Une cuisson deja faite (meme nom de fichier) n'est pas refaite.
// =============================================================================

#include "NKCore/NkTypes.h"

namespace nkentseu {
	namespace renderer {

		/// Les cartes cuites d'une matiere a photo.
		struct NkPhotoJeu {
				bool ok = false;
				char couleur[520] = {0}; ///< couleur cuite (PNG sRGB)
				char normale[520] = {0}; ///< normale (PNG lineaire, vert inverse : +y = v croissants)
				char orm[520] = {0};	 ///< occlusion / rugosite / metal (PNG lineaire)
				float32 echelle = 1.f;	 ///< metres couverts par une repetition (echelle.txt)
				float32 medLin[3] = {1.f, 1.f, 1.f}; ///< mediane lineaire de la photo (avant cuisson ; 1 si deja cuite)
				float32 medRug = 1.f;				 ///< mediane de la rugosite de la photo (idem)
		};

		/// Cuit (ou retrouve) les cartes de la photo `nom` (« source/nom », relatif a `racine`) pour une matiere
		/// de couleur lineaire `couleur` et de rugosite `rugosite`, a `taille` texels de cote, dans `cache`.
		/// `contraste` : la variation de la photo autour de sa mediane, en puissance (1 : telle quelle ; 0,5 :
		/// adoucie -- un mur fraichement peint est plus uni que la photo d'un enduit use).
		/// Faux si une image source manque ou si l'ecriture echoue (la matiere reste alors sans photo).
		bool NkPhotoPreparer(const char *racine, const char *nom, const float32 *couleur, float32 rugosite, float32 contraste,
							 const char *cache, int32 taille, NkPhotoJeu &out);

		/// LE BANC (sans carte graphique) : pour chaque photo de `noms` (separees par des virgules) -- la
		/// mediane de la couleur cuite = la couleur decrite, celle de la rugosite cuite = la rugosite decrite,
		/// et le SENS de la normale cuite : correlation de ses x et y avec la pente descendante du deplacement
		/// (-dh/du, -dh/dv). Un axe dont |correlation| < 0,1 est INDECIDABLE (deplacement sans relief : placage,
		/// cuir) ; un axe decidable negatif est un echec ; il faut au moins 10 photos au y decidable.
		/// NK_PHOTO_MUTATION=1 : le vert n'est plus inverse (contre-epreuve, le sens doit rougir). Rend le
		/// nombre d'echecs ; ecrit le compte rendu sur la sortie.
		int32 NkPhotoBanc(const char *racine, const char *noms, const char *cache);

	} // namespace renderer
} // namespace nkentseu
