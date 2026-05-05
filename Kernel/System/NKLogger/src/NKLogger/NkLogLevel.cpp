// =============================================================================
// NKLogger/NkLogLevel.cpp
// Implémentation des fonctions utilitaires pour NkLogLevel.
//
// Design :
//  - Implémentations déterministes sans allocation dynamique
//  - Comparaisons case-insensitive optimisées pour le parsing config
//  - Tables de conversion statiques pour performance et simplicité
//  - Respect strict des spécifications de l'header public
//
// Auteur : TEUGUIA TADJUIDJE Rodolf / Rihen
// Date : 2024-2026
// License : Proprietary - Free to use and modify
// =============================================================================
#include "pch.h"
#include "NKLogger/NkLogLevel.h"


// -------------------------------------------------------------------------
// SECTION 1 : NAMESPACE ANONYME - UTILITAIRES INTERNES
// -------------------------------------------------------------------------
// Fonctions et constantes à usage interne uniquement.
// Non exposées dans l'API publique pour encapsulation.

namespace {


    // -------------------------------------------------------------------------
    // FONCTION : NkAsciiToLower
    // DESCRIPTION : Conversion ASCII d'un caractère en minuscule
    // NOTE : Implémentation minimale sans dépendance à <cctype> pour portabilité
    // -------------------------------------------------------------------------
    inline char NkAsciiToLower(char character) {
        if (character >= 'A' && character <= 'Z') {
            return static_cast<char>(character + ('a' - 'A'));
        }
        return character;
    }


    // -------------------------------------------------------------------------
    // FONCTION : NkEqualsIgnoreCase
    // DESCRIPTION : Comparaison de chaînes ASCII insensible à la casse
    // PARAMS : lhs - première chaîne (peut être nullptr)
    //          rhs - seconde chaîne (peut être nullptr)
    // RETURN : true si les chaînes sont égales (case-insensitive), false sinon
    // NOTE : Gestion sécurisée des pointeurs nullptr en entrée
    // -------------------------------------------------------------------------
    inline bool NkEqualsIgnoreCase(const char* lhs, const char* rhs) {
        if (lhs == nullptr || rhs == nullptr) {
            return false;
        }

        while (*lhs != '\0' && *rhs != '\0') {
            if (NkAsciiToLower(*lhs) != NkAsciiToLower(*rhs)) {
                return false;
            }
            ++lhs;
            ++rhs;
        }

        return (*lhs == '\0' && *rhs == '\0');
    }


} // namespace anonymous


// -------------------------------------------------------------------------
// SECTION 2 : NAMESPACE PRINCIPAL - IMPLÉMENTATIONS PUBLIQUES
// -------------------------------------------------------------------------
namespace nkentseu {

    // -------------------------------------------------------------------------
    // FONCTION : NkLogLevelToString
    // DESCRIPTION : Conversion NkLogLevel → chaîne lisible complète
    // -------------------------------------------------------------------------
    const char* NkLogLevelToString(NkLogLevel level) {
        switch (level) {
            case NkLogLevel::NK_TRACE:
                return "trace";

            case NkLogLevel::NK_DEBUG:
                return "debug";

            case NkLogLevel::NK_INFO:
                return "info";

            case NkLogLevel::NK_WARN:
                return "warning";

            case NkLogLevel::NK_ERROR:
                return "error";

            case NkLogLevel::NK_CRITICAL:
                return "critical";

            case NkLogLevel::NK_FATAL:
                return "fatal";

            case NkLogLevel::NK_OFF:
                return "off";

            default:
                return "unknown";
        }
    }


    // -------------------------------------------------------------------------
    // FONCTION : NkLogLevelToShortString
    // DESCRIPTION : Conversion NkLogLevel → chaîne courte 3 caractères
    // -------------------------------------------------------------------------
    const char* NkLogLevelToShortString(NkLogLevel level) {
        switch (level) {
            case NkLogLevel::NK_TRACE:
                return "TRC";

            case NkLogLevel::NK_DEBUG:
                return "DBG";

            case NkLogLevel::NK_INFO:
                return "INF";

            case NkLogLevel::NK_WARN:
                return "WRN";

            case NkLogLevel::NK_ERROR:
                return "ERR";

            case NkLogLevel::NK_CRITICAL:
                return "CRT";

            case NkLogLevel::NK_FATAL:
                return "FTL";

            case NkLogLevel::NK_OFF:
                return "OFF";

            default:
                return "UNK";
        }
    }


    // -------------------------------------------------------------------------
    // FONCTION : NkStringToLogLevel
    // DESCRIPTION : Conversion chaîne → NkLogLevel (case-insensitive)
    // -------------------------------------------------------------------------
    NkLogLevel NkStringToLogLevel(const char* str) {
        if (str == nullptr) {
            return NkLogLevel::NK_INFO;
        }

        if (NkEqualsIgnoreCase(str, "trace")) {
            return NkLogLevel::NK_TRACE;
        }

        if (NkEqualsIgnoreCase(str, "debug")) {
            return NkLogLevel::NK_DEBUG;
        }

        if (NkEqualsIgnoreCase(str, "info")) {
            return NkLogLevel::NK_INFO;
        }

        if (NkEqualsIgnoreCase(str, "warn") || NkEqualsIgnoreCase(str, "warning")) {
            return NkLogLevel::NK_WARN;
        }

        if (NkEqualsIgnoreCase(str, "error")) {
            return NkLogLevel::NK_ERROR;
        }

        if (NkEqualsIgnoreCase(str, "critical")) {
            return NkLogLevel::NK_CRITICAL;
        }

        if (NkEqualsIgnoreCase(str, "fatal")) {
            return NkLogLevel::NK_FATAL;
        }

        if (NkEqualsIgnoreCase(str, "off")) {
            return NkLogLevel::NK_OFF;
        }

        // Fallback sécurisé : niveau par défaut pour entrée non reconnue
        return NkLogLevel::NK_INFO;
    }


    // -------------------------------------------------------------------------
    // FONCTION : NkShortStringToLogLevel
    // DESCRIPTION : Conversion chaîne courte (3 lettres) → NkLogLevel
    // -------------------------------------------------------------------------
    NkLogLevel NkShortStringToLogLevel(const char* str) {
        if (str == nullptr) {
            return NkLogLevel::NK_INFO;
        }

        // Comparaison exacte case-sensitive pour format compact optimisé
        if (std::strcmp(str, "TRC") == 0) {
            return NkLogLevel::NK_TRACE;
        }

        if (std::strcmp(str, "DBG") == 0) {
            return NkLogLevel::NK_DEBUG;
        }

        if (std::strcmp(str, "INF") == 0) {
            return NkLogLevel::NK_INFO;
        }

        if (std::strcmp(str, "WRN") == 0) {
            return NkLogLevel::NK_WARN;
        }

        if (std::strcmp(str, "ERR") == 0) {
            return NkLogLevel::NK_ERROR;
        }

        if (std::strcmp(str, "CRT") == 0) {
            return NkLogLevel::NK_CRITICAL;
        }

        if (std::strcmp(str, "FTL") == 0) {
            return NkLogLevel::NK_FATAL;
        }

        if (std::strcmp(str, "OFF") == 0) {
            return NkLogLevel::NK_OFF;
        }

        // Fallback sécurisé : niveau par défaut pour entrée non reconnue
        return NkLogLevel::NK_INFO;
    }


    // -------------------------------------------------------------------------
    // FONCTION : NkLogLevelToANSIColor
    // DESCRIPTION : Obtention du code couleur ANSI pour terminaux Unix
    // -------------------------------------------------------------------------
    const char* NkLogLevelToANSIColor(NkLogLevel level) {
        switch (level) {
            case NkLogLevel::NK_TRACE:
                return "\033[37m";  // Blanc : messages de trace discrets

            case NkLogLevel::NK_DEBUG:
                return "\033[36m";  // Cyan : informations de débogage

            case NkLogLevel::NK_INFO:
                return "\033[32m";  // Vert : messages informatifs normaux

            case NkLogLevel::NK_WARN:
                return "\033[33m";  // Jaune : avertissements visibles

            case NkLogLevel::NK_ERROR:
                return "\033[31m";  // Rouge : erreurs nécessitant attention

            case NkLogLevel::NK_CRITICAL:
                return "\033[35m";  // Magenta : situations critiques

            case NkLogLevel::NK_FATAL:
                return "\033[41m\033[37m";  // Fond rouge + texte blanc : fatal

            default:
                return "\033[0m";  // Reset : valeur par défaut sécurisée
        }
    }


    // -------------------------------------------------------------------------
    // FONCTION : NkLogLevelToWindowsColor
    // DESCRIPTION : Obtention du code couleur pour console Windows native
    // -------------------------------------------------------------------------
    uint16 NkLogLevelToWindowsColor(NkLogLevel level) {
        switch (level) {
            case NkLogLevel::NK_TRACE:
                return 0x07;  // Gris clair sur noir (FOREGROUND_INTENSITY | R|G|B)

            case NkLogLevel::NK_DEBUG:
                return 0x0B;  // Cyan clair sur noir (FOREGROUND_INTENSITY | G|B)

            case NkLogLevel::NK_INFO:
                return 0x0A;  // Vert clair sur noir (FOREGROUND_INTENSITY | G)

            case NkLogLevel::NK_WARN:
                return 0x0E;  // Jaune sur noir (FOREGROUND_INTENSITY | R|G)

            case NkLogLevel::NK_ERROR:
                return 0x0C;  // Rouge clair sur noir (FOREGROUND_INTENSITY | R)

            case NkLogLevel::NK_CRITICAL:
                return 0x0D;  // Magenta clair sur noir (FOREGROUND_INTENSITY | R|B)

            case NkLogLevel::NK_FATAL:
                return 0x4F;  // Blanc sur fond rouge (BACKGROUND_RED | FOREGROUND_R|G|B|INTENSITY)

            default:
                return 0x07;  // Gris clair sur noir : valeur par défaut sécurisée
        }
    }
} // namespace nkentseu


// =============================================================================
// NOTES D'IMPLÉMENTATION ET OPTIMISATIONS
// =============================================================================
/*
    1. GESTION DES POINTERS NULL :
       - Toutes les fonctions acceptant const char* gèrent nullptr en entrée
       - Comportement fail-safe : retour d'une valeur par défaut raisonnable
       - Évite les crashes dans les configurations utilisateur erronées

    2. PERFORMANCE DES COMPARAISONS :
       - NkEqualsIgnoreCase : implémentation manuelle pour éviter <cctype>
       - Boucle early-exit : retour immédiat dès différence détectée
       - Pas d'allocation mémoire : toutes les chaînes sont des littéraux statiques

    3. MAINTENABILITÉ DES SWITCH :
       - Ordre cohérent avec l'enum NkLogLevel pour lecture facile
       - Cas 'default' explicite pour gestion des valeurs futures/inattendues
       - Commentaires inline pour association visuelle niveau ↔ couleur/texte

    4. COMPATIBILITÉ MULTIPLATEFORME :
       - Codes ANSI : séquences standard supportées par la majorité des terminaux
       - Codes Windows : attributs FOREGROUND_/BACKGROUND_ de l'API Console
       - Fallback vers reset/default pour les niveaux non colorés ou inconnus

    5. EXTENSIBILITÉ FUTURES :
       - Pour ajouter un niveau : mettre à jour l'enum + les 5 switch + les tests
       - Pour personnaliser les couleurs : envisager un système de thème via callback
       - Pour l'internationalisation : garder les retours en anglais, traduire en UI
*/


// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
// ============================================================