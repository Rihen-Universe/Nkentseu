//
// NkUnkenyMaillageFichier.h
// =============================================================================
// Description :
//   L'ASSET MAILLAGE 2D (2026-10-02, R31) : `.nkmesh2d`, un maillage reutilisable
//   d'une entite a l'autre, d'une scene a l'autre (« Enregistrer comme asset »
//   de la fenetre d'edition ; « Utiliser un asset » de la carte Maillage 2D).
//
// LA DECISION DU FORMAT (CONVENTIONS_FICHIERS.md, § 1 et § 2)
//   Le `.nkmesh` existe : c'est le StaticMesh 3D, consomme par NKRenderer (Noge,
//   NKCraft). Un maillage 2D d'Unkeny ne se depose pas au meme endroit (Unkeny
//   rend par NKCanvas, exclusif de NKRenderer) et ne produit pas la meme chose :
//   une texture PLANE deformee, des PARTIES avec chacune sa physique, des liens,
//   un ordre de dessin, et demain des poids d'os contraints au plan (R30). Une
//   « marque 2D » dans le `.nkmesh` ferait dire deux choses a une extension
//   selon son contenu -- ce que le corollaire du § 1 refuse. D'ou une nature a
//   lui : `NkAssetType::Mesh2D` (20), extension `.nkmesh2d`, declaree au POINT DE
//   PASSAGE UNIQUE de NKSerialization (NkAssetExtensionFor / FromExtension).
//
//   Le contenu est du JSON qui porte son "format", comme `.nkscene` et
//   `.nkprefab` :
//       { "format": "unkeny.maillage2d", "version": 1,
//         "maillage": { les champs de NkChampsMaillage2D, texture par son NOM } }
//   ecrit PAR LE MEME CODE qu'un maillage sous "jeu" d'une scene
//   (interne::EcrireComposant) : les deux ne peuvent pas diverger.
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================

#pragma once

#ifndef __NKENTSEU_UNKENY_NKUNKENYMAILLAGEFICHIER_H__
#define __NKENTSEU_UNKENY_NKUNKENYMAILLAGEFICHIER_H__

#include "NKContainers/String/NkString.h"
#include "Unkeny/Maillage/NkUnkenyMaillage.h"
#include "Unkeny/Scene/NkUnkenySauvegarde.h"

namespace nkentseu {
	namespace unkeny {

		/// Le "format" du fichier, sa version.
		constexpr const char *NK_MAILLAGE2D_FORMAT = "unkeny.maillage2d";
		constexpr int32 NK_MAILLAGE2D_VERSION = 1;

		/// Le maillage en JSON (sans son etat de jeu, ni sa `source`).
		bool NkMaillage2DVersJSON(const NkMaillage2D &m, NkString &json, const NkRessourcesScene &r);
		/// L'inverse. `scene` : celle qui resout les references (la texture par son nom).
		bool NkMaillage2DDepuisJSON(NkMaillage2D &m, const char *json, NkScene &scene, const NkRessourcesScene &r,
									NkString *erreur = nullptr);
		bool NkSauverMaillage2D(const NkMaillage2D &m, const char *chemin, const NkRessourcesScene &r);
		bool NkChargerMaillage2D(NkMaillage2D &m, const char *chemin, NkScene &scene, const NkRessourcesScene &r,
								 NkString *erreur = nullptr);

	} // namespace unkeny
} // namespace nkentseu

#endif // __NKENTSEU_UNKENY_NKUNKENYMAILLAGEFICHIER_H__
