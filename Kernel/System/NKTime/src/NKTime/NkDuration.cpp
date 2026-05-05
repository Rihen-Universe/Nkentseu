// =============================================================================
// NKTime/NkDuration.cpp
// Implémentation de NkDuration::ToString / ToStringPrecise.
//
// Design :
//  - Ce fichier contient UNIQUEMENT les méthodes non-constexpr/non-inline
//  - ToString/ToStringPrecise utilisent snprintf (allocation NkString)
//  - Aucune dépendance <cmath> : toutes les opérations sont arithmétiques simples
//  - Sélection automatique de l'unité la plus lisible pour ToString()
//
// Auteur : TEUGUIA TADJUIDJE Rodolf Séderis
// Date : 2024-2026
// License : Apache-2.0
// =============================================================================

// -------------------------------------------------------------------------
// SECTION 1 : INCLUSIONS (ordre strict requis)
// -------------------------------------------------------------------------
// 1. Precompiled header en premier (obligatoire pour MSVC/Clang)
// 2. Header correspondant au fichier .cpp
// 3. Headers système nécessaires

#include "pch.h"
#include "NKTime/NkDuration.h"

#include <cstdio>

// -------------------------------------------------------------------------
// SECTION 2 : NAMESPACE ET USING LOCAL
// -------------------------------------------------------------------------
// Alias local pour accéder aux constantes de conversion.
// Limité à ce fichier .cpp pour éviter la pollution des espaces de noms.

namespace nkentseu {

    // Alias local pour raccourcir l'accès aux constantes (scope fichier uniquement)
    using namespace time;

    // =============================================================================
    //  Formatage : ToString (unité adaptative)
    // =============================================================================
    // Sélectionne automatiquement l'unité la plus lisible selon la magnitude :
    //  - >= 1 jour    : format "X.XXd"
    //  - >= 1 heure   : format "X.XXh"
    //  - >= 1 minute  : format "X.XXmin"
    //  - >= 1 seconde : format "X.XXXs"
    //  - >= 1 ms      : format "X.XXXms"
    //  - >= 1 us      : format "X.XXXus"
    //  - sinon        : format "Xns" (entier)
    //
    // Le signe négatif est préservé dans le calcul mais l'unité affiche la valeur
    // absolue pour la lisibilité (ex: "-1.50ms" pour une durée négative).

    NkString NkDuration::ToString() const {
        // Buffer local pour le formatage (taille généreuse pour sécurité)
        char buf[128];

        // Calcul de la valeur absolue pour la sélection d'unité
        // Le signe est géré séparément dans les calculs de division flottante
        const int64 absNs = mNanoseconds < 0 ? -mNanoseconds : mNanoseconds;

        // Sélection de l'unité adaptative par ordre décroissant de magnitude
        if (absNs == 0) {
            // Cas spécial : durée nulle affichée simplement
            ::snprintf(buf, sizeof(buf), "0s");
        }
        else if (absNs >= NS_PER_DAY) {
            // Durée >= 1 jour : affichage en jours avec 2 décimales
            ::snprintf(
                buf,
                sizeof(buf),
                "%.2fd",
                static_cast<float64>(mNanoseconds) / static_cast<float64>(NS_PER_DAY)
            );
        }
        else if (absNs >= NS_PER_HOUR) {
            // Durée >= 1 heure : affichage en heures avec 2 décimales
            ::snprintf(
                buf,
                sizeof(buf),
                "%.2fh",
                static_cast<float64>(mNanoseconds) / static_cast<float64>(NS_PER_HOUR)
            );
        }
        else if (absNs >= NS_PER_MINUTE) {
            // Durée >= 1 minute : affichage en minutes avec 2 décimales
            ::snprintf(
                buf,
                sizeof(buf),
                "%.2fmin",
                static_cast<float64>(mNanoseconds) / static_cast<float64>(NS_PER_MINUTE)
            );
        }
        else if (absNs >= NS_PER_SECOND) {
            // Durée >= 1 seconde : affichage en secondes avec 3 décimales
            ::snprintf(
                buf,
                sizeof(buf),
                "%.3fs",
                static_cast<float64>(mNanoseconds) / static_cast<float64>(NS_PER_SECOND)
            );
        }
        else if (absNs >= NS_PER_MILLISECOND) {
            // Durée >= 1 milliseconde : affichage en ms avec 3 décimales
            ::snprintf(
                buf,
                sizeof(buf),
                "%.3fms",
                static_cast<float64>(mNanoseconds) / static_cast<float64>(NS_PER_MILLISECOND)
            );
        }
        else if (absNs >= NS_PER_MICROSECOND) {
            // Durée >= 1 microseconde : affichage en us avec 3 décimales
            ::snprintf(
                buf,
                sizeof(buf),
                "%.3fus",
                static_cast<float64>(mNanoseconds) / static_cast<float64>(NS_PER_MICROSECOND)
            );
        }
        else {
            // Durée < 1 microseconde : affichage en nanosecondes (entier)
            // Pas de décimales car la précision native est déjà en ns
            ::snprintf(
                buf,
                sizeof(buf),
                "%lldns",
                static_cast<long long>(mNanoseconds)
            );
        }

        // Construction de la chaîne NKEntseu depuis le buffer C
        return NkString(buf);
    }

    // =============================================================================
    //  Formatage : ToStringPrecise (nanosecondes exactes)
    // =============================================================================
    // Formatage toujours en nanosecondes, quelle que soit la magnitude.
    // Utile pour :
    //  - Logging debug nécessitant une précision exacte
    //  - Comparaisons visuelles de valeurs brutes
    //  - Export de données pour analyse post-mortem
    //
    // Format : "X ns" où X est la valeur int64 signée en nanosecondes.

    NkString NkDuration::ToStringPrecise() const {
        // Buffer local pour le formatage (taille suffisante pour int64 + suffixe)
        // Format : "-9223372036854775808 ns" = 20 + 4 = 24 caractères max
        char buf[64];

        // Formatage direct de la valeur interne en nanosecondes
        // %lld pour int64 sur toutes les plateformes supportées
        ::snprintf(
            buf,
            sizeof(buf),
            "%lld ns",
            static_cast<long long>(mNanoseconds)
        );

        // Construction de la chaîne NKEntseu depuis le buffer C
        return NkString(buf);
    }

} // namespace nkentseu

// =============================================================================
// NOTES D'IMPLÉMENTATION
// =============================================================================
/*
    Pourquoi seulement ces deux méthodes dans le .cpp ?
    ---------------------------------------------------
    - Toutes les autres méthodes de NkDuration sont constexpr et/ou inline.
    - Elles sont donc définies dans le header pour permettre l'inlining
      et l'évaluation à la compilation par le compilateur.
    - ToString() et ToStringPrecise() utilisent snprintf() qui :
      * Nécessite <cstdio> (header système)
      * Alloue dynamiquement via NkString (non-constexpr)
      * Ne peut pas être évaluée à la compilation

    Choix de précision dans ToString() :
    ------------------------------------
    - Unités grandes (jours/heures/minutes) : 2 décimales (%.2f)
      * Suffisant pour la lisibilité humaine
      * Évite l'affichage de valeurs trop longues
    - Unités petites (secondes/ms/us) : 3 décimales (%.3f)
      * Meilleure précision pour les durées courtes
      * Important pour le profiling et le debugging
    - Nanosecondes : entier uniquement (%lld)
      * Pas de décimales possibles car c'est l'unité de base
      * Affichage exact sans perte d'information

    Gestion des signes :
    -------------------
    - La valeur absolue (absNs) est utilisée uniquement pour sélectionner
      l'unité appropriée dans la cascade if/else.
    - Le calcul de division utilise mNanoseconds (signé) pour préserver
      le signe dans le résultat affiché.
    - Exemple : -1500ms s'affiche "-1.50s" et non "1.50s".

    Performance :
    ------------
    - Buffer sur la pile (pas d'allocation dynamique pour le formatage).
    - snprintf avec taille fixe : protection contre les buffer overflows.
    - Cascade if/else ordonnée par fréquence d'usage probable :
      les durées courtes (ms/us/ns) sont testées en dernier car moins
      fréquentes dans les APIs de timeout/sleep typiques.

    Portabilité :
    ------------
    - %lld pour int64 : standard C99, supporté par tous les compilateurs cibles.
    - static_cast<long long> : garantit la correspondance de type pour printf.
    - Aucune dépendance à <cmath> : évite les variations de comportement
      des fonctions mathématiques entre plateformes.
*/

// ============================================================
// Copyright © 2024-2026 TEUGUIA TADJUIDJE Rodolf Séderis.
// Apache-2.0 License - Free to use and modify
// ============================================================