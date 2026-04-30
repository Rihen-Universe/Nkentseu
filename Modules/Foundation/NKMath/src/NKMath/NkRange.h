//
// NkRange.h
// =============================================================================
// Description :
//   Définition de la structure template NkRangeT<T> représentant un intervalle
//   fermé [min, max] pour des valeurs génériques sans allocation dynamique.
//
// Fonctionnalités principales :
//   - Gestion automatique de l'ordre min/max lors de la construction
//   - Opérations géométriques : intersection, union, découpage
//   - Requêtes : containment, overlap, clamping, longueur, centre
//   - Mutations : translation (Shift), expansion (Expand)
//
// Conception :
//   - Zéro allocation heap : compatible temps-réel et systèmes embarqués
//   - Méthodes inline pour performances optimales
//   - Split() utilise des out-parameters pour éviter std::vector
//
// Auteur   : Rihen
// Copyright: (c) 2024 Rihen. Tous droits réservés.
// =============================================================================

#pragma once

#ifndef __NKENTSEU_RANGE_H__
#define __NKENTSEU_RANGE_H__

    // =====================================================================
    // Inclusions des dépendances
    // =====================================================================
    #include "NKMath/NkLegacySystem.h"              // Types fondamentaux : float32, int32, etc.
    #include "NKMath/NkFunctions.h"                 // Fonctions utilitaires : NkMin, NkMax, etc.
    #include "NKContainers/String/NkFormat.h"        // NkString et NkFormatProps pour ToString/NkToString

    // =====================================================================
    // Namespace principal du projet
    // =====================================================================
    namespace nkentseu {

        // =================================================================
        // Sous-namespace mathématique : types et opérations géométriques
        // =================================================================
        namespace math {

            // =================================================================
            // Structure template : NkRangeT<T>
            // =================================================================
            // Représente un intervalle fermé [min, max] pour tout type T
            // supportant les opérations de comparaison et arithmétiques de base.
            //
            // Invariant : mMin <= mMax est toujours maintenu par les mutateurs.
            // =================================================================
            template <typename T>
            struct NkRangeT {

                // -----------------------------------------------------------------
                // Section : Membres publics
                // -----------------------------------------------------------------
                public:

                    // -----------------------------------------------------------------
                    // Sous-section : Constructeurs
                    // -----------------------------------------------------------------
                    // Constructeur par défaut : initialise à l'intervalle nul [0, 0]
                    constexpr NkRangeT() noexcept
                        : mMin(T(0))
                        , mMax(T(0))
                    {
                    }

                    // Constructeur paramétré : crée [a, b] en triant automatiquement
                    // Garantit que mMin <= mMax quel que soit l'ordre des arguments
                    constexpr NkRangeT(T lowerBound, T upperBound) noexcept
                        : mMin(lowerBound <= upperBound ? lowerBound : upperBound)
                        , mMax(lowerBound <= upperBound ? upperBound : lowerBound)
                    {
                    }

                    // Constructeur de copie : généré par défaut (optimisation compilateur)
                    NkRangeT(const NkRangeT&) noexcept = default;

                    // Opérateur d'affectation : généré par défaut (optimisation compilateur)
                    NkRangeT& operator=(const NkRangeT&) noexcept = default;

                    // -----------------------------------------------------------------
                    // Sous-section : Accesseurs en lecture/écriture
                    // -----------------------------------------------------------------
                    // Obtention de la borne inférieure (lecture seule)
                    NK_FORCE_INLINE T GetMin() const noexcept
                    {
                        return mMin;
                    }

                    // Obtention de la borne supérieure (lecture seule)
                    NK_FORCE_INLINE T GetMax() const noexcept
                    {
                        return mMax;
                    }

                    // Définition de la borne inférieure avec réajustement automatique
                    // Si la nouvelle valeur dépasse mMax, les bornes sont échangées
                    void SetMin(T newValue) noexcept
                    {
                        mMin = newValue;
                        if (mMin > mMax) {
                            T temporary = mMax;
                            mMax = mMin;
                            mMin = temporary;
                        }
                    }

                    // Définition de la borne supérieure avec réajustement automatique
                    // Si la nouvelle valeur est inférieure à mMin, les bornes sont échangées
                    void SetMax(T newValue) noexcept
                    {
                        mMax = newValue;
                        if (mMax < mMin) {
                            T temporary = mMin;
                            mMin = mMax;
                            mMax = temporary;
                        }
                    }

                    // -----------------------------------------------------------------
                    // Sous-section : Requêtes géométriques (const)
                    // -----------------------------------------------------------------
                    // Longueur de l'intervalle : différence entre max et min
                    NK_FORCE_INLINE T Length() const noexcept
                    {
                        return mMax - mMin;
                    }

                    // Alias sémantique pour Length()
                    NK_FORCE_INLINE T Len() const noexcept
                    {
                        return mMax - mMin;
                    }

                    // Centre de l'intervalle : point médian entre min et max
                    NK_FORCE_INLINE T Center() const noexcept
                    {
                        return (mMin + mMax) / T(2);
                    }

                    // Test d'intervalle vide : vrai si min == max (longueur nulle)
                    NK_FORCE_INLINE bool IsEmpty() const noexcept
                    {
                        return mMin == mMax;
                    }

                    // Test d'appartenance d'une valeur à l'intervalle [min, max]
                    NK_FORCE_INLINE bool Contains(T value) const noexcept
                    {
                        return (value >= mMin) && (value <= mMax);
                    }

                    // Test d'inclusion : cet intervalle contient-il entièrement 'other' ?
                    NK_FORCE_INLINE bool Contains(const NkRangeT& other) const noexcept
                    {
                        return (mMin <= other.mMin) && (mMax >= other.mMax);
                    }

                    // Test de chevauchement : les intervalles ont-ils une intersection ?
                    NK_FORCE_INLINE bool Overlaps(const NkRangeT& other) const noexcept
                    {
                        return (mMin <= other.mMax) && (mMax >= other.mMin);
                    }

                    // Clamp : projette une valeur dans l'intervalle [min, max]
                    NK_FORCE_INLINE T Clamp(T value) const noexcept
                    {
                        if (value < mMin) {
                            return mMin;
                        }
                        if (value > mMax) {
                            return mMax;
                        }
                        return value;
                    }

                    // -----------------------------------------------------------------
                    // Sous-section : Mutations (modifient l'instance)
                    // -----------------------------------------------------------------
                    // Translation : décale l'intervalle entier de 'delta'
                    void Shift(T delta) noexcept
                    {
                        mMin += delta;
                        mMax += delta;
                    }

                    // Expansion : agrandit l'intervalle de 'margin' de chaque côté
                    void Expand(T margin) noexcept
                    {
                        mMin -= margin;
                        mMax += margin;
                    }

                    // -----------------------------------------------------------------
                    // Sous-section : Opérations statiques (algorithmes)
                    // -----------------------------------------------------------------
                    // Intersection : retourne l'intervalle commun à a et b
                    // Si pas d'intersection, retourne un intervalle invalide (min > max potentiel)
                    static NkRangeT Intersect(
                        const NkRangeT& first,
                        const NkRangeT& second
                    ) noexcept {
                        return {
                            NkMax(first.mMin, second.mMin),
                            NkMin(first.mMax, second.mMax)
                        };
                    }

                    // Union : retourne le plus petit intervalle contenant a et b
                    static NkRangeT Union(
                        const NkRangeT& first,
                        const NkRangeT& second
                    ) noexcept {
                        return {
                            NkMin(first.mMin, second.mMin),
                            NkMax(first.mMax, second.mMax)
                        };
                    }

                    // Alias sémantique pour Union() : "Hull" comme enveloppe convexe 1D
                    static NkRangeT Hull(
                        const NkRangeT& first,
                        const NkRangeT& second
                    ) noexcept {
                        return Union(first, second);
                    }

                    // Split : découpe l'intervalle r en deux sous-intervalles à la position 'cutPoint'
                    // Si cutPoint n'est pas dans r, les deux outputs reçoivent une copie de r
                    // Utilisation : subdivision spatiale, partitionnement de données
                    static void Split(
                        const NkRangeT& source,
                        T cutPoint,
                        NkRangeT& outputLeft,
                        NkRangeT& outputRight
                    ) noexcept {
                        if (source.Contains(cutPoint)) {
                            outputLeft = { source.mMin, cutPoint };
                            outputRight = { cutPoint, source.mMax };
                        } else {
                            outputLeft = source;
                            outputRight = source;
                        }
                    }

                    // -----------------------------------------------------------------
                    // Sous-section : Opérateurs de comparaison
                    // -----------------------------------------------------------------
                    // Égalité : comparaison stricte des deux bornes
                    bool operator==(const NkRangeT& other) const noexcept
                    {
                        return (mMin == other.mMin) && (mMax == other.mMax);
                    }

                    // Inégalité : déléguée à l'opérateur d'égalité (principe DRY)
                    bool operator!=(const NkRangeT& other) const noexcept
                    {
                        return !(*this == other);
                    }

                    // -----------------------------------------------------------------
                    // Sous-section : Sérialisation et E/S
                    // -----------------------------------------------------------------
                    // ToString : conversion en chaîne formatée "[min, max]"
                    NkString ToString() const
                    {
                        return NkFormat("[{0}, {1}]", mMin, mMax);
                    }

                    // Surcharge globale de ToString pour appel fonctionnel libre
                    friend NkString ToString(const NkRangeT& range)
                    {
                        return range.ToString();
                    }

                    // Opérateur de flux pour affichage dans std::ostream
                    friend std::ostream& operator<<(
                        std::ostream& outputStream,
                        const NkRangeT& range
                    ) {
                        return outputStream << range.ToString().CStr();
                    }

                // -----------------------------------------------------------------
                // Section : Membres privés (données)
                // -----------------------------------------------------------------
                private:

                    T mMin;  ///< Borne inférieure de l'intervalle (invariant : <= mMax)
                    T mMax;  ///< Borne supérieure de l'intervalle (invariant : >= mMin)

            }; // struct NkRangeT

            // =================================================================
            // Aliases de types : variantes pour types scalaires courants
            // =================================================================
            using NkRange = NkRangeT<float32>;           // Alias par défaut : float 32 bits
            using NkRangeFloat = NkRangeT<float32>;      // Float 32 bits explicite
            using NkRangeDouble = NkRangeT<float64>;     // Float 64 bits pour précision
            using NkRangeInt = NkRangeT<int32>;          // Entier signé 32 bits
            using NkRangeUInt = NkRangeT<uint32>;        // Entier non-signé 32 bits

        } // namespace math

        // =====================================================================
        // Spécialisation de NkToString pour intégration au système de formatage
        // =====================================================================
        // Permet d'utiliser NkRangeT<T> avec NkFormat() et les utilitaires d'E/S
        // du projet, avec support des options de formatage via NkFormatProps.
        // =====================================================================
        template <typename T>
        inline NkString NkToString(
            const math::NkRangeT<T>& range,
            const NkFormatProps& formatProperties = {}
        ) {
            return formatProperties.ApplyWidth(NkStringView(range.ToString()), false);
        }

    } // namespace nkentseu

#endif // __NKENTSEU_RANGE_H__

// =============================================================================
// EXEMPLES D'UTILISATION (documentation - ne pas compiler)
// =============================================================================
/*
    // ---------------------------------------------------------------------
    // Exemple 1 : Création et initialisation d'intervalles
    // ---------------------------------------------------------------------
    #include "NKMath/NkRange.h"
    using namespace nkentseu::math;

    // Intervalle float avec bornes dans l'ordre
    NkRange validRange(0.0f, 100.0f);  // [0, 100]

    // Intervalle avec bornes inversées : tri automatique
    NkRange autoSorted(150.0f, 50.0f);  // Devient [50, 150]

    // Intervalle entier pour indexation de tableau
    NkRangeInt arrayIndices(0, 255);  // [0, 255]

    // Intervalle nul (point unique)
    NkRange singlePoint(42.0f, 42.0f);  // [42, 42]

    // ---------------------------------------------------------------------
    // Exemple 2 : Requêtes géométriques de base
    // ---------------------------------------------------------------------
    NkRange viewport(0.0f, 1920.0f);

    // Test d'appartenance
    bool isOnScreen = viewport.Contains(500.0f);    // true
    bool isOffScreen = viewport.Contains(-10.0f);   // false

    // Test d'inclusion complète
    NkRange uiElement(100.0f, 300.0f);
    bool isFullyVisible = viewport.Contains(uiElement);  // true

    // Test de chevauchement partiel
    NkRange partiallyVisible(1800.0f, 2000.0f);
    bool overlaps = viewport.Overlaps(partiallyVisible);  // true

    // ---------------------------------------------------------------------
    // Exemple 3 : Clamping et normalisation de valeurs
    // ---------------------------------------------------------------------
    NkRange normalized(0.0f, 1.0f);

    // Clamp de valeurs hors bornes
    float32 clampedLow = normalized.Clamp(-0.5f);   // 0.0f
    float32 clampedHigh = normalized.Clamp(1.5f);   // 1.0f
    float32 clampedOk = normalized.Clamp(0.75f);    // 0.75f

    // Utilisation typique : normalisation d'entrée utilisateur
    float32 userInput = GetMousePositionX();  // Peut être hors [0, 1920]
    float32 normalizedInput = viewport.Clamp(userInput) / 1920.0f;

    // ---------------------------------------------------------------------
    // Exemple 4 : Opérations d'ensemble : Intersection et Union
    // ---------------------------------------------------------------------
    NkRange timeSegmentA(10.0f, 30.0f);
    NkRange timeSegmentB(20.0f, 40.0f);

    // Intersection : période où les deux segments se chevauchent
    NkRange overlap = NkRangeT<float>::Intersect(timeSegmentA, timeSegmentB);
    // Résultat : [20, 30]

    // Union : enveloppe couvrant les deux segments
    NkRange totalSpan = NkRangeT<float>::Union(timeSegmentA, timeSegmentB);
    // Résultat : [10, 40]

    // Vérification avant utilisation
    if (!overlap.IsEmpty()) {
        // Traitement de la période de chevauchement
    }

    // ---------------------------------------------------------------------
    // Exemple 5 : Découpage d'intervalle (Split) pour subdivision
    // ---------------------------------------------------------------------
    NkRange timeline(0.0f, 100.0f);
    NkRange leftPart, rightPart;

    // Découpage au point médian
    NkRangeT<float>::Split(timeline, 50.0f, leftPart, rightPart);
    // leftPart  : [0, 50]
    // rightPart : [50, 100]

    // Découpage hors de l'intervalle : les deux outputs sont identiques à l'original
    NkRangeT<float>::Split(timeline, 200.0f, leftPart, rightPart);
    // leftPart  : [0, 100]
    // rightPart : [0, 100]

    // Application : construction d'un arbre de partitionnement binaire (BSP 1D)
    void BuildPartitionTree(NkRange range, int depth)
    {
        if (depth <= 0 || range.Length() < 1.0f) {
            return;  // Condition d'arrêt
        }

        NkRange leftChild, rightChild;
        NkRangeT<float>::Split(range, range.Center(), leftChild, rightChild);

        BuildPartitionTree(leftChild, depth - 1);
        BuildPartitionTree(rightChild, depth - 1);
    }

    // ---------------------------------------------------------------------
    // Exemple 6 : Mutations : Shift et Expand
    // ---------------------------------------------------------------------
    NkRange visibleArea(100.0f, 500.0f);

    // Défilement (scroll) : translation de l'intervalle
    visibleArea.Shift(50.0f);  // Devient [150, 550]

    // Zone de tolérance (margin) : expansion pour anti-aliasing ou culling large
    visibleArea.Expand(10.0f);  // Devient [140, 560]

    // Combinaison : camera follow avec marge de prédiction
    void UpdateCameraRange(NkRange& cameraRange, float32 targetPosition)
    {
        constexpr float32 viewWidth = 800.0f;
        constexpr float32 margin = 50.0f;

        // Centrer sur la cible
        float32 newMin = targetPosition - viewWidth / 2.0f;
        float32 newMax = targetPosition + viewWidth / 2.0f;

        cameraRange = NkRange(newMin, newMax);
        cameraRange.Expand(margin);  // Marge pour éviter les chargements fréquents
    }

    // ---------------------------------------------------------------------
    // Exemple 7 : Intégration dans un système de culling 1D
    // ---------------------------------------------------------------------
    struct SoundSource {
        NkRange activeTime;  // Période de validité du son
        float32 volume;
    };

    bool IsSoundAudible(const SoundSource& source, NkRange currentTimeWindow)
    {
        // Le son est audible si sa période active chevauche la fenêtre d'écoute
        return source.activeTime.Overlaps(currentTimeWindow);
    }

    NkRange GetAudibleTimeRange(const SoundSource& source, NkRange listenWindow)
    {
        // Retourne la portion du son effectivement audible dans la fenêtre
        return NkRangeT<float>::Intersect(source.activeTime, listenWindow);
    }

    // ---------------------------------------------------------------------
    // Exemple 8 : Affichage et débogage
    // ---------------------------------------------------------------------
    NkRange debugRange(3.14f, 9.99f);

    // Via méthode membre
    NkString rangeStr = debugRange.ToString();  // "[3.14, 9.99]"

    // Via flux de sortie
    std::cout << "Plage de valeurs : " << debugRange << std::endl;

    // Via fonction globale avec formatage personnalisé
    NkString formatted = NkToString(
        debugRange,
        NkFormatProps().SetPrecision(3)
    );  // "[3.140, 9.990]"

    // ---------------------------------------------------------------------
    // Bonnes pratiques d'utilisation
    // ---------------------------------------------------------------------
    // ✓ Utiliser Contains() plutôt que des comparaisons manuelles (plus lisible)
    // ✓ Vérifier IsEmpty() avant d'utiliser un intervalle résultant d'Intersect()
    // ✓ Préférer les alias courts (NkRange) pour float32, explicites pour autres types
    // ✓ Utiliser Split() avec out-parameters pour éviter les allocations temporaires
    // ✗ Ne pas modifier mMin/mMax directement : passer par SetMin/SetMax pour l'invariant
    // ✗ Attention aux types : NkRangeInt et NkRangeFloat ne sont pas interchangeables
*/