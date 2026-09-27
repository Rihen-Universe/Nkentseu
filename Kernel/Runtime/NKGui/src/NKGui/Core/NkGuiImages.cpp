// -----------------------------------------------------------------------------
// @File    NkGuiImages.cpp
// @Brief   Le cache d'images du monteur : un nom, un numero de texture, une seule
//          fois -- et le compte de tout ce qui n'arrive pas.
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// ⚠️ AUCUN DECODAGE ICI, ET C'EST LE GRAPHE DE DEPENDANCES QUI LE VEUT : `NkImage.h`
//    tire `NKStream/NKIResource.h`, hors des chemins d'inclusion de NKGui. Le
//    chargeur vit dans `Integrations/NKGui/`. Voir l'en-tete de `NkGuiImages.h`.
// -----------------------------------------------------------------------------

#include "NKGui/Doc/NkGuiImages.h"

namespace nkentseu {
	namespace nkgui {

		namespace {

			struct Entree {
					NkString nom;
					uint32 texId = 0;
					NkVec2 taille{0.f, 0.f};
					/// ⚠️ UN ECHEC SE MEMORISE AUSSI, et c'est le point le plus utile de
					///    cette structure. Sans ce drapeau, une image absente serait
					///    RELUE A CHAQUE IMAGE DE RENDU : soixante ouvertures d'un
					///    fichier qui n'existe pas par seconde, sur un chemin d'erreur
					///    que personne ne regarde. Un cache doit se souvenir de ce qui
					///    ne marche pas autant que de ce qui marche.
					bool echouee = false;
			};

			struct Registre {
					NkVector<Entree> entrees;
					uint32 prochainId = kNkGuiImageTexId0;
					NkGuiChargeurImage fn = nullptr;
					void *user = nullptr;
					NkGuiImagesRapport rap;
			};

			Registre &R() noexcept {
				static Registre r;
				return r;
			}

			Entree *Trouver(Registre &r, const char *nom) noexcept {
				for (uint32 i = 0; i < (uint32)r.entrees.Size(); ++i) {
					if (r.entrees[i].nom.Compare(nom) == 0) {
						return &r.entrees[i];
					}
				}
				return nullptr;
			}

		} // namespace

		void NkGuiPoserChargeurImage(NkGuiChargeurImage fn, void *user) noexcept {
			Registre &r = R();
			// ⚠️ CHANGER DE CHARGEUR VIDE LE CACHE. Un nouveau chargeur veut dire, en
			//    pratique, un nouveau dorsal : les numeros deja distribues ne
			//    designent plus rien chez lui. Les garder ferait dire au cache « deja
			//    chargee » pour des textures que personne ne connait, et les images
			//    disparaitraient sans une seule erreur -- la perte silencieuse ne se
			//    plaint pas.
			if (r.fn != fn || r.user != user) {
				r.entrees.Clear();
				r.prochainId = kNkGuiImageTexId0;
			}
			r.fn = fn;
			r.user = user;
		}

		bool NkGuiChargeurImagePose() noexcept {
			return R().fn != nullptr;
		}

		bool NkGuiResoudreImage(const char *nom, uint32 &texIdOut, NkVec2 &tailleOut) noexcept {
			Registre &r = R();
			++r.rap.demandees;
			if (!nom || !*nom) {
				++r.rap.introuvables;
				return false;
			}
			if (Entree *e = Trouver(r, nom)) {
				if (e->echouee) {
					++r.rap.introuvables;
					return false;
				}
				texIdOut = e->texId;
				tailleOut = e->taille;
				++r.rap.servies;
				++r.rap.duCache;
				return true;
			}
			// ⚠️ SANS CHARGEUR, ON NE MEMORISE RIEN. C'est le seul echec qui n'est PAS
			//    celui de l'image : l'hote n'a pas encore installe sa fonction (il
			//    l'installe apres la creation du dorsal). Marquer l'entree « echouee »
			//    condamnerait definitivement une image parfaitement valide, pour une
			//    question d'ordre de demarrage -- et le defaut ne se verrait qu'au
			//    premier ecran, des mois plus tard.
			if (!r.fn) {
				++r.rap.sansChargeur;
				return false;
			}

			const uint32 id = r.prochainId;
			NkVec2 taille{0.f, 0.f};
			if (!r.fn(nom, id, taille, r.user) || taille.x <= 0.f || taille.y <= 0.f) {
				Entree e;
				e.nom = NkString(nom);
				e.echouee = true;
				r.entrees.PushBack(e);
				++r.rap.introuvables;
				// LE NOM, pas seulement le compte : « il manque une image » sans dire
				// LAQUELLE renvoie chercher dans trente documents.
				if (r.rap.premiereIntrouvable.Empty()) {
					r.rap.premiereIntrouvable = NkString(nom);
				}
				// ⚠️ LE NUMERO N'EST PAS CONSOMME. Le reprendre au prochain appel evite
				//    qu'une suite d'images manquantes ne creuse un trou dans la
				//    numerotation -- et un trou finirait par faire croire a une
				//    collision qui n'existe pas.
				return false;
			}
			++r.prochainId;

			Entree e;
			e.nom = NkString(nom);
			e.texId = id;
			e.taille = taille;
			r.entrees.PushBack(e);
			texIdOut = id;
			tailleOut = taille;
			++r.rap.servies;
			return true;
		}

		const NkGuiImagesRapport &NkGuiImagesReleve() noexcept {
			return R().rap;
		}

		void NkGuiImagesRemiseAZero() noexcept {
			R().rap = NkGuiImagesRapport();
		}

		void NkGuiImagesOublier() noexcept {
			Registre &r = R();
			r.entrees.Clear();
			r.prochainId = kNkGuiImageTexId0;
			r.rap = NkGuiImagesRapport();
		}

	} // namespace nkgui
} // namespace nkentseu

// =============================================================================
// Copyright (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================
