// -----------------------------------------------------------------------------
// FICHIER: NKMath\NkAngle.cpp
// DESCRIPTION: Implémentation des fonctions non-inline de NkAngleT
// AUTEUR: Rihen
// DATE: 2026-04-26
// VERSION: 2.0.0
// LICENCE: Proprietary - Free to use and modify
// -----------------------------------------------------------------------------

// -------------------------------------------------------------------------
// PRECOMPILED HEADER (requis pour tous les fichiers .cpp du projet)
// -------------------------------------------------------------------------
#include "pch.h"

// -------------------------------------------------------------------------
// EN-TÊTES DU MODULE
// -------------------------------------------------------------------------
#include "NKMath/NkAngle.h"
#include "NKCore/NkString.h"
#include <ostream>

// -------------------------------------------------------------------------
// ESPACE DE NOMS PRINCIPAL
// -------------------------------------------------------------------------

namespace nkentseu {

    // ====================================================================
    // NAMESPACE : MATH (IMPLÉMENTATIONS NON-INLINE TEMPLATE)
    // ====================================================================

    namespace math {

        // ====================================================================
        // INSTANCIATIONS EXPLICITES POUR PRÉCISIONS SUPPORTÉES
        // ====================================================================
        // Note : Les méthodes template non-inline doivent être instanciées
        // explicitement pour chaque type de précision supporté.

        // -------------------------------------------------------------------------
        // MÉTHODES DE REPRÉSENTATION TEXTE - FLOAT32
        // -------------------------------------------------------------------------

        NKENTSEU_MATH_API
        NkString NkAngleT<float32>::ToString() const
        {
            return NkFormat("{0}_deg", mDeg);
        }

        NKENTSEU_MATH_API
        NkString ToString(const NkAngleT<float32>& a)
        {
            return a.ToString();
        }

        NKENTSEU_MATH_API
        std::ostream& operator<<(std::ostream& os, const NkAngleT<float32>& a)
        {
            return os << a.ToString().CStr();
        }

        // -------------------------------------------------------------------------
        // MÉTHODES DE REPRÉSENTATION TEXTE - FLOAT64
        // -------------------------------------------------------------------------

        NKENTSEU_MATH_API
        NkString NkAngleT<float64>::ToString() const
        {
            return NkFormat("{0}_deg", mDeg);
        }

        NKENTSEU_MATH_API
        NkString ToString(const NkAngleT<float64>& a)
        {
            return a.ToString();
        }

        NKENTSEU_MATH_API
        std::ostream& operator<<(std::ostream& os, const NkAngleT<float64>& a)
        {
            return os << a.ToString().CStr();
        }

    } // namespace math

    // ============================================================================
    // SPÉCIALISATION : NKTOSTRING<NKANGLET> (ESPACE DE NOMS GLOBAL)
    // ============================================================================

    // -------------------------------------------------------------------------
    // INSTANCIATION FLOAT32
    // -------------------------------------------------------------------------
    template<>
    NKENTSEU_MATH_API
    NkString NkToString(const math::NkAngleT<float32>& a, const NkFormatProps& props)
    {
        return NkApplyFormatProps(a.ToString(), props);
    }

    // -------------------------------------------------------------------------
    // INSTANCIATION FLOAT64
    // -------------------------------------------------------------------------
    template<>
    NKENTSEU_MATH_API
    NkString NkToString(const math::NkAngleT<float64>& a, const NkFormatProps& props)
    {
        return NkApplyFormatProps(a.ToString(), props);
    }

} // namespace nkentseu

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
// ============================================================