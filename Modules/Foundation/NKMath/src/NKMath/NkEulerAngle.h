//
// NkEulerAngle.h
// =============================================================================
// Description :
//   Définition de la structure NkEulerAngle représentant un triplet d'angles
//   Euler (pitch/yaw/roll) pour les orientations 3D dans l'espace.
//
// Convention de rotation appliquée :
//   1. Yaw   (axe Y) : rotation horizontale (lacet)
//   2. Pitch (axe X) : rotation verticale (tangage)
//   3. Roll  (axe Z) : rotation autour de l'axe de vision (roulis)
//
// Chaque composante angulaire utilise le type NkAngle qui gère automatiquement
// le wrapping dans l'intervalle (-180°, 180°].
//
// Auteur   : Rihen
// Copyright: (c) 2024 Rihen. Tous droits réservés.
// =============================================================================

#pragma once

#ifndef __NKENTSEU_EULERANGLE_H__
#define __NKENTSEU_EULERANGLE_H__

    // Inclusions des dépendances nécessaires
    #include "NKMath/NkLegacySystem.h"  // Définitions des types fondamentaux (float32, etc.)
    #include "NkAngle.h"                 // Type NkAngle pour la gestion des angles

    // =====================================================================
    // Namespace principal du projet
    // =====================================================================
    namespace nkentseu {

        // =================================================================
        // Sous-namespace mathématique : contient les types géométriques
        // =================================================================
        namespace math {

            // =================================================================
            // Structure : NkEulerAngle
            // =================================================================
            // Représente une orientation 3D via trois angles d'Euler.
            // Cette structure est légère (POD-like), copiable et optimisée
            // pour les calculs mathématiques en temps réel.
            // =================================================================
            struct NkEulerAngle {

                // -----------------------------------------------------------------
                // Membres de données : composantes angulaires
                // -----------------------------------------------------------------
                NkAngle pitch;  ///< Tangage : rotation autour de l'axe X (haut/bas)
                NkAngle yaw;    ///< Lacet   : rotation autour de l'axe Y (gauche/droite)
                NkAngle roll;   ///< Roulis  : rotation autour de l'axe Z (inclinaison)

                // -----------------------------------------------------------------
                // Section : Constructeurs
                // -----------------------------------------------------------------
                // Constructeur par défaut : initialise les angles à zéro (noexcept)
                constexpr NkEulerAngle() noexcept = default;

                // Constructeur paramétré : initialise avec des valeurs explicites
                NkEulerAngle(
                    const NkAngle& pitch,
                    const NkAngle& yaw,
                    const NkAngle& roll
                ) noexcept
                    : pitch(pitch)
                    , yaw(yaw)
                    , roll(roll)
                {
                }

                // Constructeur de copie : généré par défaut (optimisé par le compilateur)
                NkEulerAngle(const NkEulerAngle&) noexcept = default;

                // Opérateur d'affectation : généré par défaut (optimisé par le compilateur)
                NkEulerAngle& operator=(const NkEulerAngle&) noexcept = default;

                // -----------------------------------------------------------------
                // Section : Opérateurs de comparaison
                // -----------------------------------------------------------------
                // Égalité : compare chaque composante angulaire individuellement
                bool operator==(const NkEulerAngle& other) const noexcept
                {
                    return (pitch == other.pitch) && (yaw == other.yaw) && (roll == other.roll);
                }

                // Inégalité : déléguée à l'opérateur d'égalité (principe DRY)
                bool operator!=(const NkEulerAngle& other) const noexcept
                {
                    return !(*this == other);
                }

                // -----------------------------------------------------------------
                // Section : Opérateurs arithmétiques binaires
                // -----------------------------------------------------------------
                // Addition : retourne un nouveau NkEulerAngle avec les sommes composantes
                NkEulerAngle operator+(const NkEulerAngle& other) const noexcept
                {
                    return { pitch + other.pitch, yaw + other.yaw, roll + other.roll };
                }

                // Soustraction : retourne un nouveau NkEulerAngle avec les différences
                NkEulerAngle operator-(const NkEulerAngle& other) const noexcept
                {
                    return { pitch - other.pitch, yaw - other.yaw, roll - other.roll };
                }

                // -----------------------------------------------------------------
                // Section : Opérateurs arithmétiques d'affectation
                // -----------------------------------------------------------------
                // Addition avec assignment : modifie l'instance courante en place
                NkEulerAngle& operator+=(const NkEulerAngle& other) noexcept
                {
                    pitch += other.pitch;
                    yaw   += other.yaw;
                    roll  += other.roll;
                    return *this;
                }

                // Soustraction avec assignment : modifie l'instance courante en place
                NkEulerAngle& operator-=(const NkEulerAngle& other) noexcept
                {
                    pitch -= other.pitch;
                    yaw   -= other.yaw;
                    roll  -= other.roll;
                    return *this;
                }

                // -----------------------------------------------------------------
                // Section : Multiplication par scalaire
                // -----------------------------------------------------------------
                // Multiplication membre : scale chaque angle par un facteur float32
                NkEulerAngle operator*(float32 scalar) const noexcept
                {
                    return { pitch * scalar, yaw * scalar, roll * scalar };
                }

                // Multiplication avec assignment : scale l'instance en place
                NkEulerAngle& operator*=(float32 scalar) noexcept
                {
                    pitch *= scalar;
                    yaw   *= scalar;
                    roll  *= scalar;
                    return *this;
                }

                // Multiplication commutative : permet scalar * euler (friend function)
                friend NkEulerAngle operator*(float32 scalar, const NkEulerAngle& euler) noexcept
                {
                    return euler * scalar;
                }

                // -----------------------------------------------------------------
                // Section : Opérateurs d'incrémentation/décrémentation
                // -----------------------------------------------------------------
                // Pré-incrémentation : incrémente chaque angle de 1 degré (avant retour)
                NkEulerAngle& operator++() noexcept
                {
                    ++pitch;
                    ++yaw;
                    ++roll;
                    return *this;
                }

                // Pré-décrémentation : décrémente chaque angle de 1 degré (avant retour)
                NkEulerAngle& operator--() noexcept
                {
                    --pitch;
                    --yaw;
                    --roll;
                    return *this;
                }

                // Post-incrémentation : retourne la valeur avant incrémentation
                NkEulerAngle operator++(int) noexcept
                {
                    NkEulerAngle temporary(*this);
                    ++(*this);
                    return temporary;
                }

                // Post-décrémentation : retourne la valeur avant décrémentation
                NkEulerAngle operator--(int) noexcept
                {
                    NkEulerAngle temporary(*this);
                    --(*this);
                    return temporary;
                }

                // -----------------------------------------------------------------
                // Section : Méthodes d'E/S et de sérialisation
                // -----------------------------------------------------------------
                // ToString : convertit l'orientation en chaîne lisible (format degrés)
                NkString ToString() const
                {
                    return NkFormat("euler({0}, {1}, {2})", pitch.Deg(), yaw.Deg(), roll.Deg());
                }

                // Surcharge globale de ToString : permet l'appel fonctionnel libre
                friend NkString ToString(const NkEulerAngle& euler)
                {
                    return euler.ToString();
                }

                // Opérateur de flux : permet l'affichage direct dans std::ostream
                friend std::ostream& operator<<(std::ostream& outputStream, const NkEulerAngle& euler)
                {
                    return outputStream << euler.ToString().CStr();
                }

            }; // struct NkEulerAngle

        } // namespace math

        // =====================================================================
        // Fonction : NkToString (spécialisation pour NkEulerAngle)
        // =====================================================================
        // Permet l'intégration de NkEulerAngle dans le système de formatage
        // générique du projet via NkFormatProps pour la personnalisation.
        // =====================================================================
        inline NkString NkToString(
            const math::NkEulerAngle& euler,
            const NkFormatProps& formatProperties = {}
        ) {
            return NkApplyFormatProps(euler.ToString(), formatProperties);
        }

    } // namespace nkentseu

#endif // __NKENTSEU_EULERANGLE_H__

// =============================================================================
// EXEMPLES D'UTILISATION (à titre documentaire - ne pas compiler)
// =============================================================================
/*
    // ---------------------------------------------------------------------
    // Exemple 1 : Création et initialisation d'un NkEulerAngle
    // ---------------------------------------------------------------------
    #include "NKMath/NkEulerAngle.h"
    using namespace nkentseu::math;

    // Création avec valeurs explicites (en degrés via NkAngle::FromDeg)
    NkEulerAngle orientation1(
        NkAngle::FromDeg(30.0f),  // pitch : 30° vers le haut
        NkAngle::FromDeg(45.0f),  // yaw   : 45° vers la droite
        NkAngle::FromDeg(10.0f)   // roll  : 10° d'inclinaison
    );

    // Création avec constructeur par défaut (tous angles à 0°)
    NkEulerAngle orientation2;

    // ---------------------------------------------------------------------
    // Exemple 2 : Opérations arithmétiques
    // ---------------------------------------------------------------------
    // Addition de deux orientations
    NkEulerAngle result = orientation1 + orientation2;

    // Multiplication par un scalaire (double l'amplitude des rotations)
    NkEulerAngle scaled = orientation1 * 2.0f;

    // Modification en place avec opérateur composé
    orientation1 += orientation2;
    orientation1 *= 0.5f;  // Réduction de moitié

    // ---------------------------------------------------------------------
    // Exemple 3 : Comparaisons
    // ---------------------------------------------------------------------
    if (orientation1 == orientation2) {
        // Les deux orientations sont identiques composante par composante
    }

    if (orientation1 != orientation2) {
        // Les orientations diffèrent sur au moins un axe
    }

    // ---------------------------------------------------------------------
    // Exemple 4 : Incrémentation/Décrémentation
    // ---------------------------------------------------------------------
    // Incrémente chaque angle de 1° (pré-incrémentation)
    ++orientation1;

    // Décrémente chaque angle de 1° (post-décrémentation)
    NkEulerAngle previous = orientation1--;

    // ---------------------------------------------------------------------
    // Exemple 5 : Affichage et sérialisation
    // ---------------------------------------------------------------------
    // Via méthode membre
    NkString debugInfo = orientation1.ToString();
    // Résultat : "euler(31, 46, 11)"

    // Via flux de sortie standard
    std::cout << "Orientation actuelle : " << orientation1 << std::endl;

    // Via fonction globale ToString avec options de formatage
    NkString formatted = NkToString(orientation1, NkFormatProps().SetPrecision(2));

    // ---------------------------------------------------------------------
    // Exemple 6 : Intégration dans une boucle de simulation
    // ---------------------------------------------------------------------
    void UpdateCameraRotation(NkEulerAngle& cameraRotation, float deltaTime)
    {
        // Application d'une rotation progressive basée sur l'entrée utilisateur
        constexpr float rotationSpeed = 90.0f;  // degrés par seconde

        if (IsKeyDown(KEY_UP)) {
            cameraRotation.pitch += NkAngle::FromDeg(rotationSpeed * deltaTime);
        }
        if (IsKeyDown(KEY_LEFT)) {
            cameraRotation.yaw -= NkAngle::FromDeg(rotationSpeed * deltaTime);
        }

        // Normalisation automatique gérée par NkAngle (wrapping -180/180)
    }

    // ---------------------------------------------------------------------
    // Bonnes pratiques d'utilisation
    // ---------------------------------------------------------------------
    // ✓ Préférer NkAngle::FromDeg() / NkAngle::FromRad() pour la clarté
    // ✓ Utiliser les opérateurs composés (+=, *=) pour les performances
    // ✓ La structure est POD-friendly : compatible avec memcpy si nécessaire
    // ✓ noexcept sur toutes les opérations : sécurité en contexte temps-réel
    // ✗ Éviter les comparaisons flottantes directes sur les angles bruts
    // ✗ Ne pas modifier directement les membres sans passer par NkAngle
*/