// =============================================================================
// NKRenderer/Core/NkRendererTypes.cpp — Implémentations non-template
// =============================================================================
/**
 * @file NkRendererTypes.cpp
 * @brief Implémentations non-template et définitions de symboles pour NkRendererTypes.
 * 
 * Ce fichier contient :
 * - Aucune implémentation de fonction pour l'instant (tout est inline/header-only)
 * - Réservé pour les futures extensions non-template (sérialisation, helpers)
 * 
 * 🔹 Pourquoi un .cpp quasi-vide ?
 *    • NkRendererTypes.h est conçu comme un header-only de types (POD, enums, templates)
 *    • Toutes les méthodes sont inline ou constexpr — pas de code à linker séparément
 *    • Le .cpp existe pour :
 *      - Faciliter l'ajout futur de logique non-template sans casser l'API
 *      - Permettre la compilation séparée si des méthodes lourdes sont ajoutées
 *      - Servir de point d'ancrage pour les unités de compilation dépendantes
 * 
 * 🔹 Bonnes pratiques pour l'extension future :
 *    • Garder les nouvelles méthodes non-template dans ce .cpp
 *    • Documenter toute fonction ajoutée avec des commentaires Doxygen
 *    • Préserver la compatibilité binaire : ne pas modifier le layout des structs publics
 * 
 * @note Si vous ajoutez une méthode comme NkColorF::ToSrgb() qui nécessite
 *       une table de lookup ou un calcul lourd, l'implémenter ici plutôt
 *       que dans le header pour éviter de rallonger les temps de compilation.
 */

#include "NkRendererTypes.h"

namespace nkentseu {
    namespace renderer {

        // =====================================================================
        // 🔧 Espace réservé pour implémentations futures
        // =====================================================================

        /**
         * @note Aucune implémentation nécessaire pour la v3 actuelle.
         *       Toutes les fonctions sont inline ou constexpr dans le header.
         * 
         * Exemples de ce qui pourrait être ajouté ici à l'avenir :
         * 
         * // ── Sérialisation binaire des types pour le streaming ─────────
         * bool NkSerializeAABB(const NkAABB& aabb, NkOutputStream& out) {
         *     return out.Write(aabb.min) && out.Write(aabb.max);
         * }
         * 
         * // ── Helpers de conversion de couleur ─────────────────────────
         * NkColorF NkColorF::ToSrgb() const {
         *     // Implémentation avec table de lookup ou calcul gamma
         *     // Déplacée ici pour éviter d'incluer <cmath> dans le header
         * }
         * 
         * // ── Validation runtime des descripteurs ──────────────────────
         * bool NkValidateLightDesc(const NkLightDesc& light) {
         *     if (light.type == NkLightType::NK_SPOT) {
         *         return light.innerAngle < light.outerAngle;
         *     }
         *     return true;
         * }
         */

    } // namespace renderer
} // namespace nkentseu