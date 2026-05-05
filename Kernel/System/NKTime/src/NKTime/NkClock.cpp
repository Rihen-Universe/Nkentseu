// =============================================================================
// NKTime/NkClock.cpp
// Implémentation de la classe NkClock.
//
// Design :
//  - NkClock::NkTime::From : construction avec clamp pour éviter INF/NaN
//  - Reset() : préserve timeScale et fixedDelta configurés par l'utilisateur
//  - Tick() : gère pause en forçant delta=0 tout en incrémentant frameCount
//  - Toutes les méthodes sont noexcept : pas d'exceptions, pas d'allocation
//
// Thread-safety :
//  - NkClock n'est PAS thread-safe : à utiliser depuis un seul thread
//  - Les snapshots NkTime retournés sont valides jusqu'au prochain Tick()/Reset()
//
// Auteur : TEUGUIA TADJUIDJE Rodolf Séderis
// Date : 2024-2026
// License : Apache-2.0
// =============================================================================

// -------------------------------------------------------------------------
// SECTION 1 : INCLUSIONS (ordre strict requis)
// -------------------------------------------------------------------------
// 1. Precompiled header en premier (obligatoire pour la compilation MSVC/Clang)
// 2. Header correspondant au fichier .cpp
// 3. Headers du projet NKEntseu
// 4. Headers système si nécessaires

#include "pch.h"
#include "NKTime/NkClock.h"

// -------------------------------------------------------------------------
// SECTION 2 : NAMESPACE PRINCIPAL
// -------------------------------------------------------------------------
// Implémentation des méthodes de NkClock dans le namespace nkentseu.

namespace nkentseu {

    // =============================================================================
    //  NkClock::NkTime::From — Fabrique de snapshot
    // =============================================================================
    // Construit un snapshot NkTime depuis des valeurs brutes mesurées.
    // Inclut un clamp sur le calcul des FPS pour éviter les divisions par zéro
    // ou les valeurs infinies lors de la première frame ou après un spike.

    NkClock::NkTime NkClock::NkTime::From(
        const NkElapsedTime& delta,
        const NkElapsedTime& total,
        uint64 frame,
        float32 fixedDt,
        float32 scale
    ) noexcept
    {
        // Construction locale avec initialisation explicite
        NkTime t;

        // Conversion des durées NkElapsedTime (float64) vers float32 pour le snapshot
        // Perte de précision acceptable pour les besoins d'une game loop
        t.delta = static_cast<float32>(delta.seconds);
        t.total = static_cast<float32>(total.seconds);

        // Copie directe du compteur de frames (uint64 → uint64)
        t.frameCount = frame;

        // Calcul des FPS avec protection contre division par zéro ou valeurs trop petites
        // Seuil 1.0e-6 secondes = 1 microseconde : en dessous, considérer FPS = 0
        // Évite les INF/NaN à la première frame ou après un spike de performance
        if (delta.seconds > 1.0e-6) {
            t.fps = static_cast<float32>(1.0 / delta.seconds);
        } else {
            t.fps = 0.f;
        }

        // Propagation des paramètres de configuration
        t.fixedDelta = fixedDt;
        t.timeScale = scale;

        // Retour par valeur : optimisé par RVO/NRVO du compilateur
        return t;
    }

    // =============================================================================
    //  NkClock — Constructeur
    // =============================================================================
    // Initialisation par défaut avec liste de membres explicite.
    // Aucun appel système, aucune allocation : noexcept garanti.

    NkClock::NkClock() noexcept
        : mFrameChrono()
        , mTotalChrono()
        , mTime()
        , mPaused(false)
    {
        // Initialisation via liste de membres : efficace et clair
        // Les chronos sont construits avec leur état par défaut (zéro)
        // mTime est initialisé avec les valeurs par défaut de sa définition
        // mPaused est explicitement mis à false pour clarté
    }

    // =============================================================================
    //  Cycle de vie : Reset
    // =============================================================================
    // Remet à zéro l'état temporel du clock tout en préservant la configuration
    // utilisateur (timeScale, fixedDelta). Utile pour les transitions de niveau.

    void NkClock::Reset() noexcept {
        // Sauvegarde des paramètres configurés par l'utilisateur
        // Ces valeurs doivent survivre au reset pour cohérence
        const float32 savedFixedDelta = mTime.fixedDelta;
        const float32 savedTimeScale = mTime.timeScale;

        // Reconstruction des chronos avec état par défaut (zéro)
        // Affectation directe : NkChrono est triviallement copiable
        mFrameChrono = NkChrono{};
        mTotalChrono = NkChrono{};

        // Reconstruction du snapshot avec valeurs par défaut
        mTime = NkTime{};

        // Restauration des paramètres utilisateur
        // Ces affectations sont sûres : NkTime est un POD avec champs publics
        mTime.fixedDelta = savedFixedDelta;
        mTime.timeScale = savedTimeScale;

        // Note : mPaused n'est pas modifié par Reset()
        // Si le clock était en pause, il le reste après le reset
    }

    // =============================================================================
    //  Gestion des frames : Tick
    // =============================================================================
    // Méthode principale : avance d'une frame, met à jour le snapshot, retourne
    // une référence vers l'état courant. Gère le mode pause en forçant delta=0.

    const NkClock::NkTime& NkClock::Tick() noexcept {
        // Mesure du delta de frame : Reset() retourne la durée écoulée puis remet à zéro
        // NkElapsedTime : 4 unités précalculées (ns, us, ms, s) pour accès rapide
        const NkElapsedTime rawDelta = mFrameChrono.Reset();

        // Mesure du temps total : Elapsed() retourne la durée depuis la construction
        // ou le dernier Reset(), sans remettre à zéro
        const NkElapsedTime total = mTotalChrono.Elapsed();

        // Construction du nouveau snapshot via la fabrique statique
        // Gestion du mode pause : si pause active, forcer delta à zéro (NkElapsedTime{})
        // Mais total continue de s'accumuler et frameCount continue d'incrémenter
        mTime = NkTime::From(
            mPaused ? NkElapsedTime{} : rawDelta,  // delta nul en pause, sinon mesure réelle
            total,                                  // temps total toujours mis à jour
            mTime.frameCount + 1,                   // incrémentation du compteur de frames
            mTime.fixedDelta,                       // conservation de la config fixedDelta
            mTime.timeScale                         // conservation de la config timeScale
        );

        // Retour par référence constante : évite la copie du snapshot
        // La référence est valide jusqu'au prochain appel à Tick() ou Reset()
        return mTime;
    }

    // =============================================================================
    //  Paramètres de contrôle — Setters simples
    // =============================================================================
    // Méthodes de configuration : affectation directe, noexcept, inline-friendly.

    void NkClock::SetTimeScale(float32 scale) noexcept {
        // Affectation directe du facteur de time scale
        // Affecte les méthodes Scaled() et FixedScaled() du snapshot
        mTime.timeScale = scale;
    }

    void NkClock::SetFixedDelta(float32 seconds) noexcept {
        // Affectation directe du delta fixe pour la logique déterministe
        // Utilisé par FixedScaled() pour les pas de physique constants
        mTime.fixedDelta = seconds;
    }

    void NkClock::Pause() noexcept {
        // Activation du flag de pause
        // Au prochain Tick(), delta et fps seront forcés à 0
        mPaused = true;
    }

    void NkClock::Resume() noexcept {
        // Désactivation du flag de pause
        // Au prochain Tick(), le calcul normal de delta/fps reprend
        mPaused = false;
    }

} // namespace nkentseu

// =============================================================================
// NOTES D'IMPLÉMENTATION
// =============================================================================
/*
    Gestion du temps en mode pause :
    -------------------------------
    - mPaused affecte uniquement le delta de frame, pas le temps total.
    - Cela permet de "geler" la logique métier tout en continuant à mesurer
      le temps réel écoulé (utile pour les timeouts réseau, les animations UI).
    - frameCount continue d'incrémenter même en pause : utile pour le debugging
      et les statistiques de frames "tentées" vs "exécutées".

    Précision des conversions float64 → float32 :
    --------------------------------------------
    - NkElapsedTime utilise float64 pour la précision des mesures brutes.
    - NkClock::NkTime utilise float32 pour compatibilité avec les moteurs
      de jeu et les APIs graphiques (OpenGL, DirectX, Vulkan).
    - La perte de précision (24 bits de mantisse vs 53) est acceptable pour
      les durées de frame typiques (1ms à 100ms = 1e-3 à 1e-1 secondes).

    Calcul des FPS avec clamp :
    --------------------------
    - Seuil de 1.0e-6 secondes (1 microseconde) pour éviter la division par zéro.
    - En pratique, une frame ne peut pas être plus rapide que ~100ns (limites CPU).
    - Le clamp évite les valeurs INF/NaN qui pourraient corrompre les affichages
      ou les calculs ultérieurs (moyennes glissantes, lissage, etc.).

    Retour par référence dans Tick() :
    ---------------------------------
    - Évite la copie du snapshot NkTime (6 champs = 24 bytes) à chaque frame.
    - La référence est valide jusqu'au prochain Tick() ou Reset() : documentation
      importante pour les utilisateurs de l'API.
    - Pattern courant dans les moteurs de jeu : "frame state" mutable interne,
      lecture externe en référence constante.

    Thread-safety :
    --------------
    - NkClock n'est PAS thread-safe : conçu pour un usage single-thread (game loop).
    - Si un accès multi-thread est nécessaire, synchronisation externe requise.
    - Alternative : utiliser un NkClock par thread pour les systèmes parallèles.

    Performance :
    ------------
    - Tick() : 2 appels chrono + 1 construction NkTime + quelques affectations.
    - Aucune allocation dynamique, aucun appel système (sauf via NkChrono interne).
    - Conçu pour être appelé 60-144+ fois par seconde sans overhead mesurable.
*/

// ============================================================
// Copyright © 2024-2026 TEUGUIA TADJUIDJE Rodolf Séderis.
// Apache-2.0 License - Free to use and modify
// ============================================================