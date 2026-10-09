// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
#pragma once

// =============================================================================
// NkX11Icon.h
// L'icone de la fenetre au format _NET_WM_ICON (EWMH), pour XLib ET XCB.
//
// POURQUOI CE FICHIER EXISTE (08/10) : aucun des deux dorsaux X11 ne posait
// d'icone. `NkWindowConfig::iconPath` y etait ignore en silence -- Win32 prend
// l'icone embarquee dans l'exe, Cocoa et le Web lisent le fichier -- et NKCode
// Linux n'avait rien dans la barre des taches.
//
// ⚠️ LE FORMAT, ECRIT UNE SEULE FOIS POUR LES DEUX DORSAUX. _NET_WM_ICON est un
//    tableau de CARDINAUX 32 bits : pour chaque taille, `largeur`, `hauteur`,
//    puis largeur x hauteur pixels 0xAARRGGBB -- alpha en octet de poids fort,
//    NON premultiplie, lignes du HAUT vers le bas. Nos images sont en RGBA8 par
//    octets : la permutation se fait ICI et nulle part ailleurs. Deux copies de
//    cette boucle finiraient par ne plus dire la meme chose.
//
// ⚠️ CE QUE LES DEUX DORSAUX NE PARTAGENT PAS, et qui reste chez eux :
//    Xlib veut un tableau de `long` pour un format « 32 » (8 octets sous LP64),
//    XCB veut de vrais mots de 32 bits. Passer ce tableau-ci tel quel a
//    XChangeProperty lirait un mot sur deux.
//
// Temoin : Kernel/Runtime/NKWindow/tests/test_icone_x11.cpp (une image dont
// chaque pixel se calcule, relue par XGetWindowProperty).
// =============================================================================

#include "NKWindow/Core/NkWindowConfig.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKLogger/NkLog.h" // le refus nomme (NkX11RefuserIcone)

namespace nkentseu {

	/// Emballe `images` en cardinaux _NET_WM_ICON, dans l'ordre donne.
	/// @param maxCardinaux  plafond du tableau (la plus grande requete que le
	///                      serveur accepte). Une image qui le ferait depasser est
	///                      LAISSEE, les suivantes sont encore essayees : mieux
	///                      vaut une icone sans sa plus grande taille que pas
	///                      d'icone du tout.
	/// @return nombre d'images emballees (0 = rien a poser).
	inline uint32 NkX11EmballerIcones(const NkVector<NkWindowIconImage> &images, usize maxCardinaux,
									  NkVector<uint32> &out) {
		out.Clear();
		uint32 emballees = 0;
		for (usize i = 0; i < images.Size(); ++i) {
			const NkWindowIconImage &image = images[i];
			if (!image.IsValid())
				continue;
			const usize nbPixels = static_cast<usize>(image.width) * image.height;
			if (out.Size() + 2u + nbPixels > maxCardinaux)
				continue;
			out.Reserve(out.Size() + 2u + nbPixels);
			out.PushBack(image.width);
			out.PushBack(image.height);
			const uint8 *p = image.pixels.Data();
			for (usize k = 0; k < nbPixels; ++k, p += 4) {
				// RGBA par octets -> 0xAARRGGBB
				out.PushBack((static_cast<uint32>(p[3]) << 24) | (static_cast<uint32>(p[0]) << 16) |
							 (static_cast<uint32>(p[1]) << 8) | static_cast<uint32>(p[2]));
			}
			++emballees;
		}
		return emballees;
	}

	/// Le refus NOMME, une fois par processus : ce que le dorsal ne posera pas, et
	/// pourquoi. `NkWindowRefuserMethode` ne convient pas ici -- son message parle
	/// d'une methode et renvoie a la table des curseurs.
	/// @param chemin  `iconPath` s'il a ete donne SANS `iconImages` (sinon nul ou vide)
	inline void NkX11RefuserIcone(const char *dorsal, const char *chemin) {
		static bool dejaDit = false;
		if (dejaDit)
			return;
		dejaDit = true;
		if (chemin && *chemin)
			NkLog::Instance().Warnf("[NkWindow] REFUS : NkWindowConfig::iconPath (%s) n'agit pas sur le dorsal %s. "
									"X11 n'a pas d'icone par fichier et NKWindow ne decode pas d'image : "
									"donner les pixels dans NkWindowConfig::iconImages. La fenetre n'a PAS d'icone.",
									chemin, dorsal ? dorsal : "?");
		else
			NkLog::Instance().Warnf("[NkWindow] REFUS : NkWindowConfig::iconImages, dorsal %s : aucune image valide "
									"(RGBA8, width*height*4 octets) ne tient dans une requete du serveur X. "
									"La fenetre n'a PAS d'icone.",
									dorsal ? dorsal : "?");
	}

} // namespace nkentseu
