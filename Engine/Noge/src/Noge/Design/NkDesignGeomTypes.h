#pragma once
// =============================================================================
// Nkentseu/Design/NkDesignGeomTypes.h
// =============================================================================
// [AJOUT 2026-07-24 — shim minimal, décision documentée]
// Les specs du sous-système Design/Doc (`NkVectorPath.h`, `NkRasterCanvas.h`,
// `NkVectorDocument.h`) référencent trois types — `NkAABB2f`, `NkIRect`,
// `NkIVec2` — qui n'existaient NULLE PART dans le repo (pas même en
// déclaration avancée) : `grep -rn "NkAABB2f\|NkIRect\|NkIVec2" Kernel/
// Engine/` ne retournait que ces mêmes fichiers de specs jamais compilés.
// Sans ce fichier, `NkVectorPath.h` seul échoue déjà à la simple analyse
// syntaxique (identifiant `NkAABB2f` inconnu dans une signature de retour).
//
// Plutôt que d'inventer de nouveaux types géométriques, on aliase des types
// RÉELS et déjà compilés de NKMath qui couvrent exactement le même besoin :
//   - NkAABB2f -> NkRectangle   (AABB float32 corner+size, avec NkRectangle.cpp)
//   - NkIRect  -> NkRectI       (= NkRectT<int32>, alias déjà défini dans
//                                 NkRectangle.h)
//   - NkIVec2  -> NkVec2i       (= NkVectorT<int32,2>, alias déjà défini dans
//                                 NkVec.h)
// Aucune nouvelle logique géométrique : uniquement des noms manquants
// raccordés à des types existants. Kernel/Foundation/NKMath n'est PAS modifié
// (aliases posés côté Noge uniquement, pas de risque de ricochet sur les
// autres modules qui dépendent de NKMath).
// =============================================================================

#include "NKMath/NKMath.h"

namespace nkentseu {
	namespace math {

		using NkAABB2f = NkRectangle; ///< AABB 2D float32 (corner + size)
		using NkIRect = NkRectI;	  ///< Rectangle 2D entier (= NkRectT<int32>)
		using NkIVec2 = NkVec2i;	  ///< Vecteur 2D entier (= NkVectorT<int32,2>)

	} // namespace math
} // namespace nkentseu
