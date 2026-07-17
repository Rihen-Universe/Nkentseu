#pragma once
/**
 * @File    NkPAConfig.h
 * @Brief   Configuration NKPA persistée dans un fichier texte ÉDITABLE.
 *
 *  Format simple `clef = valeur` (une par ligne, '#' = commentaire) → l'utilisateur
 *  peut l'éditer à la main OU via l'interface. Contient pour l'instant le backend
 *  graphique choisi ; l'appli le relit au démarrage (un changement nécessite un
 *  redémarrage — déclenché par l'UI ou manuel).
 *
 *  Fichier par défaut : "nkpa.cfg" (dans le répertoire de travail).
 */

#include "NKCore/NkTypes.h"
#include "NKContainers/String/NkString.h"
#include "NKCanvas/Core/NkGraphicsApi.h" // NkGraphicsApi (enum partagé) + NkGraphicsApiName

namespace nkentseu {
	namespace nkpa {

		struct NkPAConfig {
				NkGraphicsApi backend = NkGraphicsApi::NK_GFX_API_OPENGL;

				// Chemin par défaut du fichier de config.
				static const char *DefaultPath() {
					return "nkpa.cfg";
				}

				// Nom canonique d'un backend (pour le fichier ET l'affichage UI).
				static const char *ApiName(NkGraphicsApi api);

				// Parse un nom de backend ("opengl"/"vulkan"/"dx11"/"dx12"/"software").
				// Retourne true + remplit `out` si reconnu.
				static bool ParseApi(const NkString &name, NkGraphicsApi &out);

				// Charge la config depuis `path` (défauts si absent/illisible).
				static NkPAConfig Load(const NkString &path = DefaultPath());

				// Écrit la config dans `path`. Retourne false si échec d'écriture.
				bool Save(const NkString &path = DefaultPath()) const;
		};

	} // namespace nkpa
} // namespace nkentseu
